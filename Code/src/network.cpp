#include "network.h"
#include "globals.h"
#include "utils.h"
#include <iostream>
#include <algorithm>
#include <mutex>

using namespace std;

void add_system_log(const string &msg) {
  lock_guard<mutex> lock(chat_mutex);
  ChatMessage cm;
  cm.id = generate_msg_id();
  cm.sender_name = "System";
  cm.timestamp = get_current_time_str();
  cm.content = msg;
  cm.is_system = true;
  cm.is_delivered = true;
  chatHistoryList.push_back(cm);
}

void add_chat_log(const string &msg_id, const string &sender_name,
                  const string &sender_id, const string &role,
                  const string &timestamp, const string &reply_name,
                  const string &reply_text, const string &content,
                  bool is_self) {
  lock_guard<mutex> lock(chat_mutex);
  ChatMessage cm;
  cm.id = msg_id.empty() ? generate_msg_id() : msg_id;
  cm.sender_name = sender_name;
  cm.sender_id = sender_id;
  cm.role = role;
  cm.timestamp = timestamp.empty() ? get_current_time_str() : timestamp;
  cm.reply_to_name = reply_name;
  cm.reply_to_text = reply_text;
  cm.content = content;
  cm.is_system = false;
  cm.is_self = is_self;
  cm.is_delivered = !is_self;
  chatHistoryList.push_back(cm);
}

void send_raw_line(const string &line) {
  if (peer_socket && peer_socket->is_open()) {
    asio::error_code ec;
    string msg_with_newline = line + "\n";
    asio::write(*peer_socket, asio::buffer(msg_with_newline), ec);
  }
}

void send_handshake() {
  string display_name = (strlen(username) > 0) ? string(username) : "Anonymous";
  string safe_name = display_name;
  replace(safe_name.begin(), safe_name.end(), '|', '/');
  send_raw_line("[HANDSHAKE]|" + local_user_id + "|" + safe_name + "|" +
                local_role);
}

void send_typing_status(bool is_typing) {
  if (currentState == CONNECTED) {
    send_raw_line("[TYPING]|" + string(is_typing ? "1" : "0"));
  }
}

void send_pending_read_receipts_nolock() {
  for (auto &msg : chatHistoryList) {
    if (!msg.is_self && !msg.is_system && !msg.has_sent_read_receipt) {
      send_raw_line("[READ]|" + msg.id);
      msg.has_sent_read_receipt = true;
    }
  }
}

void send_pending_read_receipts() {
  lock_guard<mutex> lock(chat_mutex);
  send_pending_read_receipts_nolock();
}

void stop_network() {
  if (io_context_ptr)
    io_context_ptr->stop();
  if (peer_socket) {
    asio::error_code ec;
    peer_socket->close(ec);
    delete peer_socket;
    peer_socket = nullptr;
  }
  if (peer_acceptor) {
    asio::error_code ec;
    peer_acceptor->close(ec);
    delete peer_acceptor;
    peer_acceptor = nullptr;
  }
  if (network_thread && network_thread->joinable()) {
    network_thread->join();
    delete network_thread;
    network_thread = nullptr;
  }
  if (io_context_ptr) {
    delete io_context_ptr;
    io_context_ptr = nullptr;
  }
  currentState = IDLE;
  remote_is_typing = false;
  replyTargetId = "";
}

void async_read_loop() {
  if (!peer_socket || !peer_socket->is_open())
    return;

  asio::async_read_until(
      *peer_socket, read_buffer, '\n',
      [](const asio::error_code &error, size_t bytes) {
        if (!error) {
          istream is(&read_buffer);
          string line;
          getline(is, line);
          if (!line.empty()) {
            if (line.rfind("[HANDSHAKE]|", 0) == 0) {
              stringstream ss(line);
              string tag, remote_id, remote_name, remote_role;
              getline(ss, tag, '|');
              getline(ss, remote_id, '|');
              getline(ss, remote_name, '|');
              getline(ss, remote_role, '|');
              peer_user_id = remote_id;
              peer_username = remote_name;
              peer_role = remote_role;
              add_system_log(remote_name + " (" + remote_id + " - " +
                             remote_role + ") joined the chat session");
            } else if (line.rfind("[MSG]|", 0) == 0) {
              stringstream ss(line);
              string tag, msg_id, remote_id, remote_name, remote_role, ts,
                  r_name, r_text, content;
              getline(ss, tag, '|');
              getline(ss, msg_id, '|');
              getline(ss, remote_id, '|');
              getline(ss, remote_name, '|');
              getline(ss, remote_role, '|');
              getline(ss, ts, '|');
              getline(ss, r_name, '|');
              getline(ss, r_text, '|');
              getline(ss, content);
              add_chat_log(msg_id, remote_name, remote_id, remote_role, ts,
                           r_name, r_text, content, false);
              send_raw_line("[ACK]|" + msg_id);
            } else if (line.rfind("[TYPING]|", 0) == 0) {
              stringstream ss(line);
              string tag, flag;
              getline(ss, tag, '|');
              getline(ss, flag, '|');
              remote_is_typing = (flag == "1");
              last_remote_typing_time = chrono::steady_clock::now();
            } else if (line.rfind("[REACTION]|", 0) == 0) {
              stringstream ss(line);
              string tag, target_msg_id, emoji, reactor_id;
              getline(ss, tag, '|');
              getline(ss, target_msg_id, '|');
              getline(ss, emoji, '|');
              getline(ss, reactor_id, '|');
              if (reactor_id.empty())
                reactor_id = peer_user_id;

              lock_guard<mutex> lock(chat_mutex);
              for (auto &msg : chatHistoryList) {
                if (msg.id == target_msg_id) {
                  auto it = find_if(msg.reactions.begin(), msg.reactions.end(),
                                    [&](const ReactionItem &item) {
                                      return item.emoji == emoji &&
                                             item.user_id == reactor_id;
                                    });
                  if (it != msg.reactions.end())
                    msg.reactions.erase(it);
                  else
                    msg.reactions.push_back({emoji, reactor_id});
                  break;
                }
              }
            } else if (line.rfind("[ACK]|", 0) == 0) {
              stringstream ss(line);
              string tag, ack_msg_id;
              getline(ss, tag, '|');
              getline(ss, ack_msg_id, '|');
              lock_guard<mutex> lock(chat_mutex);
              for (auto &msg : chatHistoryList) {
                if (msg.id == ack_msg_id) {
                  msg.is_delivered = true;
                  break;
                }
              }
            } else if (line.rfind("[READ]|", 0) == 0) {
              stringstream ss(line);
              string tag, read_msg_id;
              getline(ss, tag, '|');
              getline(ss, read_msg_id, '|');
              lock_guard<mutex> lock(chat_mutex);
              for (auto &msg : chatHistoryList) {
                if (msg.id == read_msg_id) {
                  msg.is_read = true;
                  break;
                }
              }
            } else if (line.rfind("[EDIT]|", 0) == 0) {
              stringstream ss(line);
              string tag, edit_msg_id, new_content;
              getline(ss, tag, '|');
              getline(ss, edit_msg_id, '|');
              getline(ss, new_content); 
              lock_guard<mutex> lock(chat_mutex);
              for (auto &msg : chatHistoryList) {
                if (msg.id == edit_msg_id) {
                  msg.content = new_content;
                  msg.is_edited = true;
                  break;
                }
              }
            } else if (line.rfind("[DELETE]|", 0) == 0) {
              stringstream ss(line);
              string tag, del_msg_id;
              getline(ss, tag, '|');
              getline(ss, del_msg_id); 
              lock_guard<mutex> lock(chat_mutex);
              for (auto &msg : chatHistoryList) {
                if (msg.id == del_msg_id) {
                  msg.is_deleted = true;
                  break;
                }
              }
            } else if (line.rfind("[LEAVE]|", 0) == 0) {
              stringstream ss(line);
              string tag, remote_id, remote_name;
              getline(ss, tag, '|');
              getline(ss, remote_id, '|');
              getline(ss, remote_name, '|');
              add_system_log(remote_name + " (" + remote_id +
                             ") left the chat session");
            } else {
              string remote_name = peer_username.empty() ? "Peer" : peer_username;
              add_chat_log("", remote_name, peer_user_id, peer_role,
                           get_current_time_str(), "", "", line, false);
            }
          }
          async_read_loop();
        } else {
          if (local_role == "Host") {
            add_system_log("Peer disconnected. Host remains open waiting for new connections");
            if (peer_socket) {
              asio::error_code ec;
              peer_socket->close(ec);
            }
            currentState = WAITING_FOR_PEER;
            remote_is_typing = false;
            if (peer_acceptor && peer_acceptor->is_open()) {
              peer_acceptor->async_accept(
                  *peer_socket, [](const asio::error_code &accept_ec) {
                    if (!accept_ec) {
                      add_system_log("Connected with a new Peer");
                      currentState = CONNECTED;
                      send_handshake();
                      async_read_loop();
                    }
                  });
            }
          } else {
            add_system_log("Connection lost to Host");
            currentState = DISCONNECTED_NOTICE;
            remote_is_typing = false;
          }
        }
      });
}

void start_hosting(int port) {
  stop_network();
  chatHistoryList.clear();
  unreadMessageCount = 0;
  unreadTrackingIndex = 0;
  editingMessageId = "";
  statusErrorMessage = "";
  local_role = "Host";

  try {
    io_context_ptr = new asio::io_context();
    peer_acceptor = new tcp::acceptor(*io_context_ptr, tcp::endpoint(tcp::v4(), port));
    peer_socket = new tcp::socket(*io_context_ptr);
    currentState = WAITING_FOR_PEER;
    add_system_log("Listening on Port " + to_string(port) + ". Waiting for peer to connect...");

    peer_acceptor->async_accept(
        *peer_socket, [](const asio::error_code &error) {
          if (!error) {
            add_system_log("Connected successfully! Chat session started.");
            currentState = CONNECTED;
            send_handshake();
            async_read_loop();
          } else {
            add_system_log("Host error: " + error.message());
            currentState = IDLE;
          }
        });
    network_thread = new thread([]() {
      try { io_context_ptr->run(); } catch (...) {}
    });
  } catch (exception &e) {
    statusErrorMessage = string("Host creation error: ") + e.what();
    stop_network();
  }
}

void start_connecting(string ip, int port) {
  stop_network();
  chatHistoryList.clear();
  unreadMessageCount = 0;
  unreadTrackingIndex = 0;
  editingMessageId = "";
  statusErrorMessage = "";
  local_role = "Peer";

  if (ip == "localhost") {
      ip = "127.0.0.1";
  }

  try {
    io_context_ptr = new asio::io_context();
    peer_socket = new tcp::socket(*io_context_ptr);
    currentState = WAITING_FOR_PEER;
    add_system_log("Connecting to " + ip + ":" + to_string(port) + "...");

    tcp::endpoint endpoint(asio::ip::make_address(ip), port);
    peer_socket->async_connect(
        endpoint, [ip, port](const asio::error_code &error) {
          if (!error) {
            add_system_log("Connected successfully to Host!");
            currentState = CONNECTED;
            send_handshake();
            async_read_loop();
          } else {
            if (error == asio::error::connection_refused)
              statusErrorMessage = "Room unavailable — Host is not ready or Port is incorrect";
            else if (error == asio::error::timed_out)
              statusErrorMessage = "Connection timed out — Please check target IP address";
            else
              statusErrorMessage = "Join failed: " + error.message();
            add_system_log("Connection error: " + statusErrorMessage);
            currentState = IDLE;
          }
        });
    network_thread = new thread([]() {
      try { io_context_ptr->run(); } catch (...) {}
    });
  } catch (exception &e) {
    statusErrorMessage = string("Cannot connect to specified IP/Port: ") + e.what();
    stop_network();
  }
}