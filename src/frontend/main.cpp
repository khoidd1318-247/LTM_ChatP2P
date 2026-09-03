#define ASIO_STANDALONE
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <asio.hpp>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include "sticker_addon.h"

using namespace std;
using asio::ip::tcp;
namespace fs = std::filesystem;

// --- DATA STRUCTURES & PROTOCOL ---
struct ReactionItem {
    string emoji;
    string user_id;
};

struct ChatMessage {
    string id;
    string sender_name;
    string sender_id;
    string role; // "Host" or "Peer"
    string timestamp;
    string reply_to_name;
    string reply_to_text;
    string content;
    vector<ReactionItem> reactions;
    bool is_delivered;
    bool is_system;
    bool is_self;
    bool is_deleted = false; // Tinh nang Delete Message (soft-delete)
    bool is_edited = false;  // Tinh nang Edit Message
    bool is_read = false;    // Tinh nang Message Status (Issue 1): da xem chua
    bool has_sent_read_receipt = false; // chong gui [READ] trung lap cho 1 message
};

enum AppState { IDLE, WAITING_FOR_PEER, CONNECTED, DISCONNECTED_NOTICE };

// --- GLOBAL VARIABLES & STATE ---
mutex chat_mutex;
vector<ChatMessage> chatHistoryList;
int messageCounter = 0;

// Local & Peer Identification
char username[128] = "";
string local_user_id = "";
string local_role = "Host";

string peer_username = "";
string peer_user_id = "";
string peer_role = "";

// Typing & State
bool remote_is_typing = false;
chrono::steady_clock::time_point last_remote_typing_time;
chrono::steady_clock::time_point last_local_typing_sent;

// UI State
string replyTargetId = "";
string replyTargetName = "";
string replyTargetText = "";
bool showInfoPanel = false;
string statusErrorMessage = "";

// Edit Message State
string editingMessageId = "";

// Unread Messages State
int unreadMessageCount = 0;
size_t unreadTrackingIndex = 0;
bool scrollToBottomRequested = false;

// Inputs
char targetIP[128] = "127.0.0.1";
char portBuf[16] = "8080";
char joinPortBuf[16] = "8080";
char messageBuf[1024] = "";
char editMessageBuf[1024] = "";
char searchBuf[128] = "";

AppState currentState = IDLE;

// Asio Network Objects
asio::io_context* io_context_ptr = nullptr;
tcp::socket* peer_socket = nullptr;
tcp::acceptor* peer_acceptor = nullptr;
thread* network_thread = nullptr;
asio::streambuf read_buffer;


// --- AVATAR HELPERS ---
ImVec4 get_avatar_color(const string& str) {
    unsigned int hash = 5381;
    for (char c : str) {
        hash = ((hash << 5) + hash) + static_cast<unsigned char>(c);
    }
    float r = 0.35f + ((hash & 0xFF) % 150) / 300.0f;
    float g = 0.35f + (((hash >> 8) & 0xFF) % 150) / 300.0f;
    float b = 0.35f + (((hash >> 16) & 0xFF) % 150) / 300.0f;
    return ImVec4(r, g, b, 1.0f);
}

void render_avatar(const string& name, const string& id, float size = 32.0f) {
    string initial = "?";
    if (!name.empty() && name != "Anonymous") {
        initial = (char)toupper(name[0]);
    }
    else if (!id.empty()) {
        initial = (char)toupper(id[1]);
    }
    ImVec4 col = get_avatar_color(name + id);
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 center = ImVec2(p.x + size * 0.5f, p.y + size * 0.5f);
    draw_list->AddCircleFilled(center, size * 0.5f, ImGui::ColorConvertFloat4ToU32(col));

    ImVec2 text_sz = ImGui::CalcTextSize(initial.c_str());
    ImVec2 text_pos = ImVec2(center.x - text_sz.x * 0.5f, center.y - text_sz.y * 0.5f);
    draw_list->AddText(text_pos, IM_COL32(255, 255, 255, 255), initial.c_str());
    ImGui::Dummy(ImVec2(size, size));
}

// --- DEBOUNCE CHAR CALLBACK (PREVENTS UNKEY / GLFW DUPLICATE CHARACTER
// INJECTION) ---
static unsigned int g_LastChar = 0;
static chrono::steady_clock::time_point g_LastCharTime;

void CustomDebouncedCharCallback(GLFWwindow* window, unsigned int c) {
    auto now = chrono::steady_clock::now();
    auto elapsedMs =
        chrono::duration_cast<chrono::milliseconds>(now - g_LastCharTime).count();
    if (c == g_LastChar && elapsedMs < 35)
        return;
    g_LastChar = c;
    g_LastCharTime = now;
    ImGui_ImplGlfw_CharCallback(window, c);
}

// Function Prototypes
void async_read_loop();
void stop_network();
void start_hosting(int port);
void start_connecting(string ip, int port);
void send_raw_line(const string& line);

// Input Filter Callbacks
static int PortInputFilter(ImGuiInputTextCallbackData* data) {
    return (data->EventChar >= '0' && data->EventChar <= '9') ? 0 : 1;
}

static int IPInputFilter(ImGuiInputTextCallbackData* data) {
    return ((data->EventChar >= '0' && data->EventChar <= '9') ||
        (data->EventChar >= 'a' && data->EventChar <= 'z') ||
        (data->EventChar >= 'A' && data->EventChar <= 'Z') ||
        data->EventChar == '.')
        ? 0
        : 1;
}

// Helper: Formatted HH:MM:SS Time
string get_current_time_str() {
    auto now = chrono::system_clock::now();
    auto in_time_t = chrono::system_clock::to_time_t(now);
    tm buf;
#if defined(_WIN32)
    localtime_s(&buf, &in_time_t);
#else
    localtime_r(&in_time_t, &buf);
#endif
    ostringstream ss;
    ss << setfill('0') << setw(2) << buf.tm_hour << ":" << setw(2) << buf.tm_min
        << ":" << setw(2) << buf.tm_sec;
    return ss.str();
}

string generate_random_id() { return "#" + to_string(1000 + (rand() % 9000)); }

string generate_msg_id() {
    messageCounter++;
    return local_user_id + "_" + to_string(messageCounter);
}

bool isValidIPv4(const string& ip) {
    if (ip == "localhost" || ip == "127.0.0.1")
        return true;
    stringstream ss(ip);
    string segment;
    vector<string> segments;
    while (getline(ss, segment, '.'))
        segments.push_back(segment);
    if (segments.size() != 4)
        return false;
    for (const string& seg : segments) {
        if (seg.empty() || seg.size() > 3)
            return false;
        for (char c : seg)
            if (!isdigit(static_cast<unsigned char>(c)))
                return false;
        int num = stoi(seg);
        if (num < 0 || num > 255)
            return false;
    }
    return true;
}

bool isValidPort(const string& portStr, int& outPort) {
    if (portStr.empty())
        return false;
    for (char c : portStr)
        if (!isdigit(static_cast<unsigned char>(c)))
            return false;
    try {
        int p = stoi(portStr);
        if (p >= 1 && p <= 65535) {
            outPort = p;
            return true;
        }
    }
    catch (...) {
    }
    return false;
}

void add_system_log(const string& msg) {
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

void add_chat_log(const string& msg_id, const string& sender_name,
    const string& sender_id, const string& role,
    const string& timestamp, const string& reply_name,
    const string& reply_text, const string& content,
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

void send_raw_line(const string& line) {
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

// Tinh nang Message Status (Issue 1): gui [READ] cho cac tin nhan cua peer
// ma minh chua xac nhan da xem. Ham nay KHONG tu lock chat_mutex, gia dinh
// noi goi da giu lock san (dung khi da o trong 1 lock_guard khac).
void send_pending_read_receipts_nolock() {
    for (auto& msg : chatHistoryList) {
        if (!msg.is_self && !msg.is_system && !msg.has_sent_read_receipt) {
            send_raw_line("[READ]|" + msg.id);
            msg.has_sent_read_receipt = true;
        }
    }
}

// Ban tu lock chat_mutex, dung khi goi tu ngoai pham vi da lock san.
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
        [](const asio::error_code& error, size_t bytes) {
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
                    }
                    else if (line.rfind("[MSG]|", 0) == 0) {
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
                    }
                    else if (line.rfind("[TYPING]|", 0) == 0) {
                        stringstream ss(line);
                        string tag, flag;
                        getline(ss, tag, '|');
                        getline(ss, flag, '|');
                        remote_is_typing = (flag == "1");
                        last_remote_typing_time = chrono::steady_clock::now();
                    }
                    else if (line.rfind("[REACTION]|", 0) == 0) {
                        stringstream ss(line);
                        string tag, target_msg_id, emoji, reactor_id;
                        getline(ss, tag, '|');
                        getline(ss, target_msg_id, '|');
                        getline(ss, emoji, '|');
                        getline(ss, reactor_id, '|');
                        if (reactor_id.empty())
                            reactor_id = peer_user_id;

                        lock_guard<mutex> lock(chat_mutex);
                        for (auto& msg : chatHistoryList) {
                            if (msg.id == target_msg_id) {
                                auto it = find_if(msg.reactions.begin(), msg.reactions.end(),
                                    [&](const ReactionItem& item) {
                                        return item.emoji == emoji &&
                                            item.user_id == reactor_id;
                                    });
                                if (it != msg.reactions.end())
                                    msg.reactions.erase(it);
                                else
                                    msg.reactions.push_back({ emoji, reactor_id });
                                break;
                            }
                        }
                    }
                    else if (line.rfind("[ACK]|", 0) == 0) {
                        stringstream ss(line);
                        string tag, ack_msg_id;
                        getline(ss, tag, '|');
                        getline(ss, ack_msg_id, '|');
                        lock_guard<mutex> lock(chat_mutex);
                        for (auto& msg : chatHistoryList) {
                            if (msg.id == ack_msg_id) {
                                msg.is_delivered = true;
                                break;
                            }
                        }
                    }
                    else if (line.rfind("[READ]|", 0) == 0) {
                        // --- XU LY NHAN GOI [READ] TU PEER (Message Status - Issue 1) ---
                        stringstream ss(line);
                        string tag, read_msg_id;
                        getline(ss, tag, '|');
                        getline(ss, read_msg_id, '|');
                        lock_guard<mutex> lock(chat_mutex);
                        for (auto& msg : chatHistoryList) {
                            if (msg.id == read_msg_id) {
                                msg.is_read = true;
                                break;
                            }
                        }
                    }
                    else if (line.rfind("[EDIT]|", 0) == 0) {
                        // --- XỬ LÝ NHẬN GÓI [EDIT] TỪ PEER ---
                        stringstream ss(line);
                        string tag, edit_msg_id, new_content;
                        getline(ss, tag, '|');
                        getline(ss, edit_msg_id, '|');
                        // Lấy toàn bộ chuỗi còn lại (bao gồm cả ký tự '|') một cách an toàn
                        getline(ss, new_content);

                        lock_guard<mutex> lock(chat_mutex);
                        for (auto& msg : chatHistoryList) {
                            if (msg.id == edit_msg_id) {
                                msg.content = new_content;
                                msg.is_edited = true;
                                break;
                            }
                        }
                    }
                    else if (line.rfind("[DELETE]|", 0) == 0) {
                        // --- XỬ LÝ NHẬN GÓI [DELETE] TỪ PEER ---
                        stringstream ss(line);
                        string tag, del_msg_id;
                        getline(ss, tag, '|');
                        getline(ss, del_msg_id); // hoặc getline(ss, del_msg_id, '|') cũng được

                        lock_guard<mutex> lock(chat_mutex);
                        for (auto& msg : chatHistoryList) {
                            if (msg.id == del_msg_id) {
                                msg.is_deleted = true;
                                break;
                            }
                        }
                    }
                    else if (line.rfind("[LEAVE]|", 0) == 0) {
                        stringstream ss(line);
                        string tag, remote_id, remote_name;
                        getline(ss, tag, '|');
                        getline(ss, remote_id, '|');
                        getline(ss, remote_name, '|');
                        add_system_log(remote_name + " (" + remote_id +
                            ") left the chat session");
                    }
                    else {
                        string remote_name =
                            peer_username.empty() ? "Peer" : peer_username;
                        add_chat_log("", remote_name, peer_user_id, peer_role,
                            get_current_time_str(), "", "", line, false);
                    }
                }
                async_read_loop();
            }
            else {
                if (local_role == "Host") {
                    add_system_log("Peer disconnected. Host remains open waiting for "
                        "new connections");
                    if (peer_socket) {
                        asio::error_code ec;
                        peer_socket->close(ec);
                    }
                    currentState = WAITING_FOR_PEER;
                    remote_is_typing = false;
                    if (peer_acceptor && peer_acceptor->is_open()) {
                        peer_acceptor->async_accept(
                            *peer_socket, [](const asio::error_code& accept_ec) {
                                if (!accept_ec) {
                                    add_system_log("Connected with a new Peer");
                                    currentState = CONNECTED;
                                    send_handshake();
                                    async_read_loop();
                                }
                            });
                    }
                }
                else {
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
        peer_acceptor =
            new tcp::acceptor(*io_context_ptr, tcp::endpoint(tcp::v4(), port));
        peer_socket = new tcp::socket(*io_context_ptr);
        currentState = WAITING_FOR_PEER;
        add_system_log("Listening on Port " + to_string(port) +
            ". Waiting for peer to connect...");

        peer_acceptor->async_accept(
            *peer_socket, [](const asio::error_code& error) {
                if (!error) {
                    add_system_log("Connected successfully! Chat session started.");
                    currentState = CONNECTED;
                    send_handshake();
                    async_read_loop();
                }
                else {
                    add_system_log("Host error: " + error.message());
                    currentState = IDLE;
                }
            });
        network_thread = new thread([]() {
            try {
                io_context_ptr->run();
            }
            catch (...) {
            }
            });
    }
    catch (exception& e) {
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
            endpoint, [ip, port](const asio::error_code& error) {
                if (!error) {
                    add_system_log("Connected successfully to Host!");
                    currentState = CONNECTED;
                    send_handshake();
                    async_read_loop();
                }
                else {
                    if (error == asio::error::connection_refused)
                        statusErrorMessage =
                        "Room unavailable — Host is not ready or Port is incorrect";
                    else if (error == asio::error::timed_out)
                        statusErrorMessage =
                        "Connection timed out — Please check target IP address";
                    else
                        statusErrorMessage = "Join failed: " + error.message();
                    add_system_log("Connection error: " + statusErrorMessage);
                    currentState = IDLE;
                }
            });
        network_thread = new thread([]() {
            try {
                io_context_ptr->run();
            }
            catch (...) {
            }
            });
    }
    catch (exception& e) {
        statusErrorMessage =
            string("Cannot connect to specified IP/Port: ") + e.what();
        stop_network();
    }
}

void apply_modern_theme() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 8.0f;
    style.ChildRounding = 8.0f;
    style.FrameRounding = 6.0f;
    style.PopupRounding = 8.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding = 6.0f;
    style.WindowPadding = ImVec2(14, 14);
    style.FramePadding = ImVec2(10, 8);
    style.ItemSpacing = ImVec2(10, 10);
    style.ButtonTextAlign = ImVec2(0.5f, 0.5f);

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text] = ImVec4(0.95f, 0.96f, 0.98f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.55f, 0.60f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.11f, 0.13f, 0.17f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.15f, 0.17f, 0.22f, 1.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.15f, 0.17f, 0.22f, 0.98f);
    colors[ImGuiCol_Border] = ImVec4(0.22f, 0.26f, 0.32f, 0.50f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.18f, 0.21f, 0.27f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.24f, 0.28f, 0.35f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.30f, 0.33f, 0.42f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.11f, 0.13f, 0.17f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.15f, 0.17f, 0.22f, 1.00f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.38f, 0.52f, 0.98f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.32f, 0.44f, 0.92f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.40f, 0.52f, 0.98f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.26f, 0.36f, 0.82f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.22f, 0.26f, 0.33f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.28f, 0.33f, 0.42f, 1.00f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.34f, 0.40f, 0.50f, 1.00f);
    colors[ImGuiCol_Separator] = ImVec4(0.22f, 0.26f, 0.32f, 0.80f);
}

// --- MODULAR REUSABLE CHAT BUBBLE COMPONENT ---
void render_chat_bubble(size_t idx, ChatMessage& msg) {
    ImGui::Spacing();
    string editedMark = (msg.is_edited && !msg.is_deleted) ? " (edited)" : "";
    string headerText =
        msg.is_self
        ? ("You " + msg.sender_id + " • " + msg.timestamp + editedMark)
        : (msg.sender_name + " " + msg.sender_id + " • " + msg.role + " • " +
            msg.timestamp + editedMark);

    // Tinh nang Message Status (Issue 1): [Da gui] / [Da nhan] / [Da xem].
    // Chi ap dung cho tin nhan cua chinh minh (is_self == true); tin nhan
    // cua peer khong hien thi trang thai nay. Uu tien hien thi trang thai cao
    // nhat: is_read > is_delivered > mac dinh Da gui.
    string statusLabel = "";
    ImVec4 statusColor = ImVec4(0.65f, 0.68f, 0.72f, 1.00f); // mau xam (Da gui)
    if (msg.is_self) {
        if (msg.is_read) {
            statusLabel = "[Đã xem]";
            statusColor = ImVec4(0.40f, 0.80f, 0.45f, 1.00f); // mau xanh la
        }
        else if (msg.is_delivered) {
            statusLabel = "[Đã nhận]";
            statusColor = ImVec4(0.40f, 0.65f, 1.00f, 1.00f); // mau xanh lam
        }
        else {
            statusLabel = "[Đã gửi]";
            statusColor = ImVec4(0.65f, 0.68f, 0.72f, 1.00f); // mau xam
        }
    }

    // Noi dung hien thi: tin nhan da xoa thi chi hien dong chu thong bao,
    // khong dung noi dung that de tinh kich thuoc bubble.
    string displayContent =
        msg.is_deleted ? "This message was deleted." : msg.content;
    bool showReplyPreview = !msg.is_deleted && !msg.reply_to_text.empty();

    float maxBubbleWidth =
        max(340.0f, min(540.0f, ImGui::GetWindowWidth() * 0.75f));
    ImVec2 textSize = ImGui::CalcTextSize(displayContent.c_str(), NULL, false,
        maxBubbleWidth - 24.0f);
    float headerWidth = ImGui::CalcTextSize(headerText.c_str()).x;
    if (!statusLabel.empty())
        headerWidth += ImGui::CalcTextSize(statusLabel.c_str()).x + 6.0f;
    float replyWidth =
        !showReplyPreview
        ? 0.0f
        : ImGui::CalcTextSize(
            ("Replying to " + msg.reply_to_name + ": " + msg.reply_to_text)
            .c_str(),
            NULL, false, maxBubbleWidth - 24.0f)
        .x;
    float bubbleWidth =
        max(340.0f, min(maxBubbleWidth,
            max({ headerWidth, textSize.x, replyWidth }) + 30.0f));

    if (msg.is_self) {
        float posX = max(10.0f, ImGui::GetWindowWidth() - bubbleWidth - 25.0f);
        ImGui::SetCursorPosX(posX);
        ImGui::PushStyleColor(
            ImGuiCol_ChildBg,
            ImVec4(0.25f, 0.38f, 0.85f, 0.95f)); // Accent Blue Bubble
    }
    else {
        ImGui::PushStyleColor(ImGuiCol_ChildBg,
            ImVec4(0.18f, 0.21f, 0.27f, 0.95f)); // Slate Bubble
    }

    ImGui::BeginChild(msg.id.c_str(), ImVec2(bubbleWidth, 0),
        ImGuiChildFlags_AutoResizeY |
        ImGuiChildFlags_AlwaysUseWindowPadding,
        ImGuiWindowFlags_NoScrollbar);
    {
        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows)) {
            ImVec2 p_min = ImGui::GetWindowPos();
            ImVec2 p_max = ImVec2(p_min.x + ImGui::GetWindowSize().x, p_min.y + ImGui::GetWindowSize().y);
            ImGui::GetWindowDrawList()->AddRectFilled(p_min, p_max, IM_COL32(255, 255, 255, 20), 8.0f);
        }

        ImGui::TextColored(msg.is_self ? ImVec4(0.85f, 0.90f, 1.00f, 1.00f)
            : ImVec4(0.45f, 0.75f, 1.00f, 1.00f),
            "%s", headerText.c_str());
        if (!statusLabel.empty()) {
            ImGui::SameLine(0.0f, 6.0f);
            ImGui::TextColored(statusColor, "%s", statusLabel.c_str());
        }

        if (msg.is_deleted) {
            // Tin nhan da bi xoa: chi hien dong chu thong bao, an het cac hanh dong
            ImGui::TextColored(ImVec4(0.55f, 0.60f, 0.68f, 1.00f),
                "This message was deleted.");
        }
        else if (editingMessageId == msg.id) {
            // Dang chinh sua tin nhan nay: hien o nhap lieu thay cho noi dung tinh
            if (showReplyPreview) {
                ImGui::TextColored(ImVec4(0.70f, 0.80f, 1.00f, 0.90f),
                    "Replying to %s: \"%s\"", msg.reply_to_name.c_str(),
                    msg.reply_to_text.c_str());
                ImGui::Separator();
            }

            ImGui::PushItemWidth(bubbleWidth - 24.0f);
            bool editEnterPressed =
                ImGui::InputText(("##edit_" + msg.id).c_str(), editMessageBuf,
                    IM_ARRAYSIZE(editMessageBuf),
                    ImGuiInputTextFlags_EnterReturnsTrue);
            ImGui::PopItemWidth();

            ImGui::PushID((int)idx);
            if (ImGui::SmallButton("Save") || editEnterPressed) {
                if (strlen(editMessageBuf) > 0) {
                    msg.content = string(editMessageBuf);
                    msg.is_edited = true;
                    // --- GỬI GÓI [EDIT] QUA P2P ---
                    send_raw_line("[EDIT]|" + msg.id + "|" + msg.content);
                }
                editingMessageId = "";
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Cancel")) {
                editingMessageId = "";
            }
            ImGui::PopID();
        }
        else {
            if (showReplyPreview) {
                ImGui::TextColored(ImVec4(0.70f, 0.80f, 1.00f, 0.90f),
                    "Replying to %s: \"%s\"", msg.reply_to_name.c_str(),
                    msg.reply_to_text.c_str());
                ImGui::Separator();
            }

            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0, 0, 0, 0));

            ImGui::InputTextMultiline(("##msg_" + msg.id).c_str(),
                (char*)displayContent.c_str(),
                displayContent.size() + 1,
                ImVec2(maxBubbleWidth - 24.0f, textSize.y + ImGui::GetStyle().FramePadding.y * 2),
                ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_NoHorizontalScroll | 1 << 24); // 1 << 24 is ImGuiInputTextFlags_WordWrap

            ImGui::PopStyleColor(3);

            // Reaction Badges
            map<string, pair<int, bool>> reactionCounts;
            for (const auto& rItem : msg.reactions) {
                auto& entry = reactionCounts[rItem.emoji];
                entry.first++;
                if (rItem.user_id == local_user_id)
                    entry.second = true;
            }

            if (!reactionCounts.empty()) {
                ImGui::Spacing();
                ImGui::PushID(("reactions_" + msg.id).c_str());
                int rIdx = 0;
                for (auto& kv : reactionCounts) {
                    string emoji = kv.first;
                    int count = kv.second.first;
                    bool myReaction = kv.second.second;
                    if (rIdx > 0)
                        ImGui::SameLine();

                    string badgeText =
                        emoji + " " + to_string(count) + (myReaction ? " ✓" : "");

                    if (myReaction) {
                        ImGui::PushStyleColor(ImGuiCol_Button,
                            ImVec4(0.15f, 0.45f, 0.95f, 1.00f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                            ImVec4(0.25f, 0.55f, 1.00f, 1.00f));
                        ImGui::PushStyleColor(ImGuiCol_Text,
                            ImVec4(1.00f, 1.00f, 1.00f, 1.00f));
                    }
                    else {
                        ImGui::PushStyleColor(ImGuiCol_Button,
                            ImVec4(0.18f, 0.22f, 0.30f, 0.85f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                            ImVec4(0.26f, 0.32f, 0.42f, 1.00f));
                        ImGui::PushStyleColor(ImGuiCol_Text,
                            ImVec4(0.85f, 0.90f, 0.95f, 1.00f));
                    }

                    if (ImGui::SmallButton(badgeText.c_str())) {
                        send_raw_line("[REACTION]|" + msg.id + "|" + emoji + "|" +
                            local_user_id);
                        auto it = find_if(msg.reactions.begin(), msg.reactions.end(),
                            [&](const ReactionItem& item) {
                                return item.emoji == emoji &&
                                    item.user_id == local_user_id;
                            });
                        if (it != msg.reactions.end())
                            msg.reactions.erase(it);
                        else
                            msg.reactions.push_back({ emoji, local_user_id });
                    }
                    ImGui::PopStyleColor(3);
                    rIdx++;
                }
                ImGui::PopID();
            }

            // Reaction Bar Buttons
            ImGui::Spacing();
            ImGui::PushID((int)idx);
            if (msg.is_self) {
                // Chi tin nhan cua chinh minh moi co Edit / Delete
                if (ImGui::SmallButton("Copy")) {
                    ImGui::SetClipboardText(msg.content.c_str());
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Edit")) {
                    editingMessageId = msg.id;
                    strcpy_s(editMessageBuf, sizeof(editMessageBuf), msg.content.c_str());
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Delete")) {
                    msg.is_deleted = true;
                    // --- GỬI GÓI [DELETE] QUA P2P ---
                    send_raw_line("[DELETE]|" + msg.id);
                }
                ImGui::SameLine();
            }
            else {
                if (ImGui::SmallButton("Reply")) {
                    replyTargetId = msg.id;
                    replyTargetName = msg.sender_name;
                    replyTargetText = msg.content.substr(0, 35);
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Forward")) {
                    string forwardContent =
                        "[Forwarded from " + msg.sender_name + "]: " + msg.content;
                    snprintf(messageBuf, sizeof(messageBuf), "%s", forwardContent.c_str());
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Copy")) {
                    ImGui::SetClipboardText(msg.content.c_str());
                }
                ImGui::SameLine();
            }

            const char* quickIcons[] = { "❤️", "⭐", "✨", "✔", "✖" };
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
            for (int k = 0; k < 5; k++) {
                if (k > 0)
                    ImGui::SameLine();
                string emoji = quickIcons[k];
                bool isMyReacted =
                    any_of(msg.reactions.begin(), msg.reactions.end(),
                        [&](const ReactionItem& r) {
                            return r.emoji == emoji && r.user_id == local_user_id;
                        });

                if (isMyReacted) {
                    ImGui::PushStyleColor(ImGuiCol_Button,
                        ImVec4(0.20f, 0.50f, 1.00f, 1.00f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                        ImVec4(0.30f, 0.60f, 1.00f, 1.00f));
                }
                else {
                    ImGui::PushStyleColor(ImGuiCol_Button,
                        ImVec4(0.20f, 0.26f, 0.40f, 0.60f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                        ImVec4(0.30f, 0.40f, 0.60f, 1.00f));
                }

                if (ImGui::Button(quickIcons[k], ImVec2(32, 26))) {
                    send_raw_line("[REACTION]|" + msg.id + "|" + emoji + "|" +
                        local_user_id);
                    auto it = find_if(msg.reactions.begin(), msg.reactions.end(),
                        [&](const ReactionItem& item) {
                            return item.emoji == emoji &&
                                item.user_id == local_user_id;
                        });
                    if (it != msg.reactions.end())
                        msg.reactions.erase(it);
                    else
                        msg.reactions.push_back({ emoji, local_user_id });
                }
                ImGui::PopStyleColor(2);
            }
            ImGui::PopStyleVar();
            ImGui::PopID();
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

int main() {
    srand(static_cast<unsigned int>(time(nullptr)));
    local_user_id = generate_random_id();

    if (!glfwInit())
        return -1;
    GLFWwindow* window = glfwCreateWindow(760, 860, "P2P Chat", NULL, NULL);
    if (!window) {
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    io.IniFilename = nullptr;

    apply_modern_theme();

    ImFontConfig font_config;
    font_config.OversampleH = 2;
    font_config.OversampleV = 2;
    // We load the Segoe UI Emoji font directly as the main font since it contains both
    // standard Latin/Vietnamese characters and all emoji glyphs. This bypasses the MergeMode / 16-bit wchar overflow issues.
    // Load the default base font first (Segoe UI or Arial)
    if (fs::exists("C:\\Windows\\Fonts\\segoeui.ttf")) {
        io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 17.0f, &font_config, io.Fonts->GetGlyphRangesVietnamese());
    }
    else if (fs::exists("C:\\Windows\\Fonts\\arial.ttf")) {
        io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\arial.ttf", 17.0f, &font_config, io.Fonts->GetGlyphRangesVietnamese());
    }
    else {
        io.Fonts->AddFontDefault();
    }

    // Merge the Windows Emoji Font (seguiemj.ttf) to support emojis (😆, 🥰, 😮, 😡, 👍, 🔥, etc.)
    // Works with standard 16-bit ImWchar without modifying any external files.
    if (fs::exists("C:\\Windows\\Fonts\\seguiemj.ttf")) {
        ImFontConfig emoji_config;
        emoji_config.MergeMode = true;
        emoji_config.OversampleH = 1;
        emoji_config.OversampleV = 1;

        static const ImWchar emoji_ranges[] = {
            0x2000, 0x3300,   // Symbols & Punctuation (❤️, etc.)
            0x2600, 0x27FF,   // Miscellaneous Symbols & Dingbats
            0xE000, 0xFFFD,   // Private Use Area & 16-bit truncated Emoji glyphs
            0 };
        io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\seguiemj.ttf", 17.0f, &emoji_config, emoji_ranges);
    }

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    glfwSetCharCallback(window, CustomDebouncedCharCallback);
    ImGui_ImplOpenGL3_Init("#version 130");

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::Begin("MainWindow", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize);

        if (remote_is_typing) {
            auto elapsed = chrono::duration_cast<chrono::seconds>(
                chrono::steady_clock::now() - last_remote_typing_time)
                .count();
            if (elapsed > 3)
                remote_is_typing = false;
        }

        // --- IDLE STATE ---
        if (currentState == IDLE) {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.48f, 0.60f, 1.00f, 1.00f), "P2P Chat");
            ImGui::TextColored(ImVec4(0.55f, 0.60f, 0.68f, 1.00f),
                "Direct Peer-to-Peer Messaging");
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (!statusErrorMessage.empty()) {
                ImGui::PushStyleColor(ImGuiCol_ChildBg,
                    ImVec4(0.35f, 0.12f, 0.15f, 1.00f));
                ImGui::BeginChild("ErrorBanner", ImVec2(0, 60), true);
                ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "System Alert");
                ImGui::TextWrapped("%s", statusErrorMessage.c_str());
                ImGui::EndChild();
                ImGui::PopStyleColor();
                ImGui::Spacing();
            }

            ImGui::Text("Display Name");
            ImGui::InputText("##username", username, IM_ARRAYSIZE(username));
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.40f, 0.80f, 0.50f, 1.00f), "%s",
                local_user_id.c_str());
            ImGui::TextColored(ImVec4(0.55f, 0.60f, 0.68f, 1.00f),
                "Unique session identifier for this peer");
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            int parsedPort = 8080;
            bool validPort = isValidPort(portBuf, parsedPort);
            bool validIP = isValidIPv4(targetIP);

            ImGui::TextColored(ImVec4(0.45f, 0.75f, 1.0f, 1.0f),
                "Option 1: Host Room");
            ImGui::Text("Port (Digits 0-9 only)");
            ImGui::InputText("##hostport", portBuf, IM_ARRAYSIZE(portBuf),
                ImGuiInputTextFlags_CallbackCharFilter, PortInputFilter);

            if (!validPort)
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                    "Invalid Port! Must be between 1 and 65535");
            else
                ImGui::TextColored(ImVec4(0.4f, 0.85f, 0.5f, 1.0f), "Valid Port (%d)",
                    parsedPort);

            ImGui::BeginDisabled(!validPort);
            if (ImGui::Button("Host Room", ImVec2(200, 40)))
                start_hosting(parsedPort);
            ImGui::EndDisabled();

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::TextColored(ImVec4(0.45f, 0.75f, 1.0f, 1.0f),
                "Option 2: Join Room");
            ImGui::Text("Peer IP Address (Digits and '.' only)");
            ImGui::InputText("##targetip", targetIP, IM_ARRAYSIZE(targetIP),
                ImGuiInputTextFlags_CallbackCharFilter, IPInputFilter);

            ImGui::Text("Target Port (Digits 0-9 only)");
            ImGui::InputText("##joinport", joinPortBuf, IM_ARRAYSIZE(joinPortBuf),
                ImGuiInputTextFlags_CallbackCharFilter, PortInputFilter);

            int parsedJoinPort = 8080;
            bool validJoinPort = isValidPort(joinPortBuf, parsedJoinPort);

            if (!validIP)
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                    "Invalid IP format (Standard format: x.x.x.x)");
            else if (!validJoinPort)
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                    "Invalid Port! Must be between 1 and 65535");
            else
                ImGui::TextColored(ImVec4(0.4f, 0.85f, 0.5f, 1.0f), "Valid IP & Port format");

            ImGui::BeginDisabled(!validJoinPort || !validIP);
            if (ImGui::Button("Join Room", ImVec2(200, 40)))
                start_connecting(targetIP, parsedJoinPort);
            ImGui::EndDisabled();
        }
        // --- CONNECTING / CONNECTED / DISCONNECTED ---
        else {
            string my_name = (strlen(username) > 0) ? string(username) : "Anonymous";

            ImGui::BeginChild("HeaderBar", ImVec2(0, 56), true);
            {
                render_avatar(my_name, local_user_id, 32.0f);
                ImGui::SameLine();

                if (currentState == CONNECTED)
                    ImGui::TextColored(ImVec4(0.4f, 0.85f, 0.5f, 1.0f), "Connected");
                else if (currentState == WAITING_FOR_PEER)
                    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f),
                        "Waiting for Peer...");
                else
                    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Disconnected");

                ImGui::SameLine();
                ImGui::Text(" | You: %s %s • %s", my_name.c_str(),
                    local_user_id.c_str(), local_role.c_str());

                if (currentState == CONNECTED && !peer_username.empty()) {
                    ImGui::SameLine();
                    ImGui::Text(" | Peer:");
                    ImGui::SameLine();
                    render_avatar(peer_username, peer_user_id, 24.0f);
                    ImGui::SameLine();
                    ImGui::Text("%s %s", peer_username.c_str(), peer_user_id.c_str());
                }

                ImGui::SameLine(ImGui::GetWindowWidth() - 240);
                if (ImGui::Button(showInfoPanel ? "Hide Info" : "Room Info",
                    ImVec2(85, 30)))
                    showInfoPanel = !showInfoPanel;

                ImGui::SameLine();
                if (ImGui::Button("Leave Room", ImVec2(120, 30))) {
                    send_raw_line("[LEAVE]|" + local_user_id + "|" + my_name);
                    stop_network();
                }
            }
            ImGui::EndChild();

            if (showInfoPanel) {
                ImGui::PushStyleColor(ImGuiCol_ChildBg,
                    ImVec4(0.14f, 0.16f, 0.21f, 1.00f));
                ImGui::BeginChild("InfoPanel", ImVec2(0, 75), true);
                {
                    ImGui::TextColored(ImVec4(0.48f, 0.60f, 1.00f, 1.00f),
                        "Room Details & Connection Status");
                    ImGui::Text("Protocol: Direct TCP P2P Socket  |  Port: %s", portBuf);
                    if (local_role == "Peer")
                        ImGui::Text("Host Target IP: %s  |  Peer ID: %s", targetIP,
                            peer_user_id.empty() ? "Connecting..."
                            : peer_user_id.c_str());
                    else
                        ImGui::Text("Hosting Port: %s  |  Connected Peer: %s", portBuf,
                            peer_username.empty()
                            ? "None"
                            : (peer_username + " " + peer_user_id).c_str());
                }
                ImGui::EndChild();
                ImGui::PopStyleColor();
                ImGui::Spacing();
            }

            ImGui::PushItemWidth(220);
            ImGui::InputTextWithHint("##search", "Search messages...", searchBuf,
                IM_ARRAYSIZE(searchBuf));
            ImGui::PopItemWidth();
            ImGui::SameLine();
            if (ImGui::Button("Clear History")) {
                lock_guard<mutex> lock(chat_mutex);
                chatHistoryList.clear();
                unreadMessageCount = 0;
                unreadTrackingIndex = 0;
                editingMessageId = "";
            }

            float bottomPadding = (currentState == CONNECTED) ? 145.0f : 60.0f;
            ImGui::BeginChild("ChatRegion", ImVec2(0, -bottomPadding), true);
            {
                lock_guard<mutex> lock(chat_mutex);

                // Tinh nang Unread Messages: kiem tra vi tri cuon TRUOC khi tin nhan
                // moi cua khung hinh nay duoc ve, de biet nguoi dung co dang o day
                // khung chat hay khong.
                bool wasAtBottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY();
                if (chatHistoryList.size() > unreadTrackingIndex) {
                    for (size_t i = unreadTrackingIndex; i < chatHistoryList.size();
                        i++) {
                        const ChatMessage& incomingMsg = chatHistoryList[i];
                        // Chi tinh la "chua doc" neu la tin nhan cua peer (khong phai
                        // tin cua minh, khong phai log he thong) va nguoi dung dang
                        // khong o day khung chat.
                        if (!incomingMsg.is_self && !incomingMsg.is_system && !wasAtBottom)
                            unreadMessageCount++;
                    }
                    unreadTrackingIndex = chatHistoryList.size();
                }
                if (wasAtBottom) {
                    unreadMessageCount = 0;
                    // Tinh nang Message Status (Issue 1): dang o cuoi khung chat nen
                    // coi nhu da xem het tin nhan cua peer hien co, gui [READ].
                    send_pending_read_receipts_nolock();
                }

                string filterStr = searchBuf;
                transform(filterStr.begin(), filterStr.end(), filterStr.begin(),
                    ::tolower);

                for (size_t idx = 0; idx < chatHistoryList.size(); idx++) {
                    auto& msg = chatHistoryList[idx];

                    if (!filterStr.empty()) {
                        string contentLower = msg.content;
                        transform(contentLower.begin(), contentLower.end(),
                            contentLower.begin(), ::tolower);
                        if (contentLower.find(filterStr) == string::npos)
                            continue;
                    }

                    if (msg.is_system) {
                        ImGui::Spacing();
                        ImGui::SetCursorPosX((ImGui::GetWindowWidth() -
                            ImGui::CalcTextSize(msg.content.c_str()).x) *
                            0.5f);
                        ImGui::TextColored(ImVec4(0.55f, 0.60f, 0.68f, 1.00f), "%s • %s",
                            msg.timestamp.c_str(), msg.content.c_str());
                        ImGui::Spacing();
                    }
                    else {
                        render_chat_bubble(idx, msg);
                    }
                }
            }
            if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() ||
                scrollToBottomRequested) {
                ImGui::SetScrollHereY(1.0f);
                scrollToBottomRequested = false;
                unreadMessageCount = 0;
                // Tinh nang Message Status (Issue 1): vua cuon xuong cuoi / bam nut
                // xem tin nhan moi -> gui [READ] cho cac tin nhan phu hop.
                send_pending_read_receipts();
            }
            ImGui::EndChild();

            if (unreadMessageCount > 0) {
                string unreadLabel = to_string(unreadMessageCount) + " new message" +
                    (unreadMessageCount > 1 ? "s" : "") + " ↓";
                float labelWidth = ImGui::CalcTextSize(unreadLabel.c_str()).x + 30.0f;
                ImGui::SetCursorPosX((ImGui::GetWindowWidth() - labelWidth) * 0.5f);
                if (ImGui::Button(unreadLabel.c_str())) {
                    scrollToBottomRequested = true;
                }
            }

            if (currentState == CONNECTED) {
                if (remote_is_typing) {
                    string typingName = peer_username.empty() ? "Peer" : peer_username;
                    ImGui::TextColored(ImVec4(0.40f, 0.80f, 0.50f, 1.00f),
                        "%s is typing...", typingName.c_str());
                }

                if (!replyTargetId.empty()) {
                    ImGui::TextColored(ImVec4(0.45f, 0.75f, 1.00f, 1.00f),
                        "Replying to %s: \"%s\"", replyTargetName.c_str(),
                        replyTargetText.c_str());
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Cancel")) {
                        replyTargetId = "";
                        replyTargetName = "";
                        replyTargetText = "";
                    }
                }

                ImGui::Text("Quick Replies:");
                ImGui::SameLine();
                const char* quickReplies[] = { "Hello!",       "I'm on my way!",
                                              "Sounds good!", "Call you later",
                                              "Got it!",      "Thanks!" };
                for (int i = 0; i < 6; i++) {
                    if (i > 0)
                        ImGui::SameLine();
                    if (ImGui::SmallButton(quickReplies[i]))
                        strcpy_s(messageBuf, sizeof(messageBuf), quickReplies[i]);
                }

                ImGui::Text("Quick Emotes:");
                ImGui::SameLine();
                const char* emojis[] = { "❤️", "⭐", "✨", "✔", "✖" };
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
                for (int i = 0; i < 5; i++) {
                    if (i > 0)
                        ImGui::SameLine();
                    if (ImGui::Button(emojis[i], ImVec2(32, 26))) {
                        if (strlen(messageBuf) + strlen(emojis[i]) < sizeof(messageBuf)) {
                            strcat_s(messageBuf, sizeof(messageBuf), emojis[i]);
                        }
                    }
                }
                ImGui::PopStyleVar();
                // --- BỘ CHỌN EMOJI POPUP (CHUẨN 16-BIT UNICODE) ---
                if (ImGui::Button("Emoji", ImVec2(54, 0))) {
                    ImGui::OpenPopup("EmojiPickerPopup");
                }

                if (ImGui::BeginPopup("EmojiPickerPopup")) {
                    ImGui::TextColored(ImVec4(0.45f, 0.75f, 1.00f, 1.0f), "Choose Emoji");
                    ImGui::Separator();
                    const char* palette[] = {
                        "❤️", "⭐", "✨", "⚡", "☀️", "☁️", "☔", "☕",
                        "✌️", "✋", "☝️", "✍️", "✉️", "✈️", "⌛", "⏰",
                        "⚓", "⚙️", "⚠️", "⛔", "❌", "❓", "❗", "🎵",
                        "⚽", "⚾", "⛄", "⛅", "✔", "✖", "✳", "❇" };
                    int totalEmojis = IM_ARRAYSIZE(palette);
                    int cols = 8;
                    for (int eIdx = 0; eIdx < totalEmojis; eIdx++) {
                        if (eIdx % cols != 0)
                            ImGui::SameLine();
                        ImGui::PushID(eIdx);
                        if (ImGui::Button(palette[eIdx], ImVec2(32, 28))) {
                            if (strlen(messageBuf) + strlen(palette[eIdx]) < sizeof(messageBuf)) {
                                strcat_s(messageBuf, sizeof(messageBuf), palette[eIdx]);
                            }
                        }
                        ImGui::PopID();
                    }
                    ImGui::EndPopup();
                }
                
                // THÊM STICKER PICKER
                

                StickerAddon::DrawStickerPicker(
                    messageBuf,
                    IM_ARRAYSIZE(messageBuf));
                ImGui::SameLine();
                ImGui::PushItemWidth(-70);
                bool isInputChanged =
                    ImGui::InputText("##InputBox", messageBuf, IM_ARRAYSIZE(messageBuf),
                        ImGuiInputTextFlags_EnterReturnsTrue);

                if (ImGui::IsItemActive()) {
                    auto now = chrono::steady_clock::now();
                    if (chrono::duration_cast<chrono::milliseconds>(
                        now - last_local_typing_sent)
                        .count() > 800) {
                        send_typing_status(strlen(messageBuf) > 0);
                        last_local_typing_sent = now;
                    }
                }
                ImGui::PopItemWidth();
                ImGui::SameLine();

                if (ImGui::Button("Send", ImVec2(60, 0)) || isInputChanged) {
                    if (strlen(messageBuf) > 0) {
                        string display_name =
                            (strlen(username) > 0) ? string(username) : "Anonymous";
                        string ts = get_current_time_str();
                        string new_msg_id = generate_msg_id();
                        string safe_name = display_name;
                        replace(safe_name.begin(), safe_name.end(), '|', '/');
                        string safe_r_name = replyTargetName;
                        replace(safe_r_name.begin(), safe_r_name.end(), '|', '/');
                        string safe_r_text = replyTargetText;
                        replace(safe_r_text.begin(), safe_r_text.end(), '|', '/');
                        string packet = "[MSG]|" + new_msg_id + "|" + local_user_id + "|" +
                            safe_name + "|" + local_role + "|" + ts + "|" +
                            safe_r_name + "|" + safe_r_text + "|" +
                            string(messageBuf);
                        send_raw_line(packet);
                        add_chat_log(new_msg_id, display_name, local_user_id, local_role,
                            ts, replyTargetName, replyTargetText,
                            string(messageBuf), true);

                        messageBuf[0] = '\0';
                        replyTargetId = "";
                        replyTargetName = "";
                        replyTargetText = "";
                        send_typing_status(false);
                        ImGui::SetKeyboardFocusHere(-1);
                    }
                }
            }
            else if (currentState == DISCONNECTED_NOTICE) {
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                    "Chat session ended or peer disconnected.");
                if (ImGui::Button("Return to Main Screen", ImVec2(200, 36)))
                    stop_network();
            }
        }
        ImGui::End();

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.11f, 0.13f, 0.17f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    stop_network();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
