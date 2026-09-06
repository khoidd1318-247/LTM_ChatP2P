#pragma once
#include <string>

using namespace std;

void add_system_log(const string &msg);
void add_chat_log(const string &msg_id, const string &sender_name,
                  const string &sender_id, const string &role,
                  const string &timestamp, const string &reply_name,
                  const string &reply_text, const string &content,
                  bool is_self);
void send_raw_line(const string &line);
void send_handshake();
void send_typing_status(bool is_typing);
void send_pending_read_receipts_nolock();
void send_pending_read_receipts();
void stop_network();
void async_read_loop();
void start_hosting(int port);
void start_connecting(string ip, int port);