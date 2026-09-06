#include "globals.h"

using namespace std;

mutex chat_mutex;
vector<ChatMessage> chatHistoryList;
int messageCounter = 0;

char username[128] = "";
string local_user_id = "";
string local_role = "Host";

string peer_username = "";
string peer_user_id = "";
string peer_role = "";

bool remote_is_typing = false;
chrono::steady_clock::time_point last_remote_typing_time;
chrono::steady_clock::time_point last_local_typing_sent;

string replyTargetId = "";
string replyTargetName = "";
string replyTargetText = "";
bool showInfoPanel = false;
string statusErrorMessage = "";
string editingMessageId = "";
int unreadMessageCount = 0;
size_t unreadTrackingIndex = 0;
bool scrollToBottomRequested = false;

char targetIP[128] = "127.0.0.1";
char portBuf[16] = "8080";
char joinPortBuf[16] = "8080";
char messageBuf[1024] = "";
char editMessageBuf[1024] = "";
char searchBuf[128] = "";

AppState currentState = IDLE;

asio::io_context *io_context_ptr = nullptr;
tcp::socket *peer_socket = nullptr;
tcp::acceptor *peer_acceptor = nullptr;
thread *network_thread = nullptr;
asio::streambuf read_buffer;