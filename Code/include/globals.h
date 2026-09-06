#pragma once
#ifndef ASIO_STANDALONE
#define ASIO_STANDALONE
#endif

#include "models.h"
#include <mutex>
#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include <asio.hpp>

using namespace std;
using asio::ip::tcp;

extern mutex chat_mutex;
extern vector<ChatMessage> chatHistoryList;
extern int messageCounter;

extern char username[128];
extern string local_user_id;
extern string local_role;

extern string peer_username;
extern string peer_user_id;
extern string peer_role;

extern bool remote_is_typing;
extern chrono::steady_clock::time_point last_remote_typing_time;
extern chrono::steady_clock::time_point last_local_typing_sent;

extern string replyTargetId;
extern string replyTargetName;
extern string replyTargetText;
extern bool showInfoPanel;
extern string statusErrorMessage;
extern string editingMessageId;
extern int unreadMessageCount;
extern size_t unreadTrackingIndex;
extern bool scrollToBottomRequested;

extern char targetIP[128];
extern char portBuf[16];
extern char joinPortBuf[16];
extern char messageBuf[1024];
extern char editMessageBuf[1024];
extern char searchBuf[128];

extern AppState currentState;

extern asio::io_context *io_context_ptr;
extern tcp::socket *peer_socket;
extern tcp::acceptor *peer_acceptor;
extern thread *network_thread;
extern asio::streambuf read_buffer;