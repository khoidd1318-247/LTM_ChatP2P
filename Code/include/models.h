#pragma once
#include <string>
#include <vector>

using namespace std;

struct ReactionItem {
  string emoji;
  string user_id;
};

struct ChatMessage {
  string id;
  string sender_name;
  string sender_id;
  string role; 
  string timestamp;
  string reply_to_name;
  string reply_to_text;
  string content;
  vector<ReactionItem> reactions;
  bool is_delivered;
  bool is_system;
  bool is_self;
  bool is_deleted = false; 
  bool is_edited = false;  
  bool is_read = false;    
  bool has_sent_read_receipt = false; 
};

enum AppState { IDLE, WAITING_FOR_PEER, CONNECTED, DISCONNECTED_NOTICE };