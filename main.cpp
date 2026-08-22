#define ASIO_STANDALONE
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <asio.hpp>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using namespace std;
using asio::ip::tcp;
using asio::ip::udp;

namespace fs = std::filesystem;

// CONFIGURATION

static const int DEFAULT_CHAT_PORT = 8080;
static const int DISCOVERY_PORT = 9090;
static const char* RECENT_PEERS_FILE = "recent_peers.txt";

// DATA STRUCTURES 


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
    bool is_delivered = false;
    bool is_system = false;
    bool is_self = false;
    bool is_deleted = false;
    bool is_edited = false;
};

struct PeerInfo {
    string id;
    string name;
    string ip;
    int port = DEFAULT_CHAT_PORT;
    string role;
    string last_seen;
};

struct RecentPeer {
    string id;
    string name;
    string ip;
    int port = DEFAULT_CHAT_PORT;
    string role;
    string last_connected;
};

enum AppState { IDLE, WAITING_FOR_PEER, CONNECTED, DISCONNECTED_NOTICE };

// GLOBAL CHAT STATE


mutex chat_mutex;
vector<ChatMessage> chatHistoryList;

int messageCounter = 0;

// LOCAL / REMOTE USER


char username[128] = "";

string local_user_id = "";
string local_role = "Idle";

string peer_username = "";
string peer_user_id = "";
string peer_role = "";
string peer_ip = "";

int local_tcp_port = DEFAULT_CHAT_PORT;
int peer_tcp_port = DEFAULT_CHAT_PORT;

// TYPING & STATE

bool remote_is_typing = false;

chrono::steady_clock::time_point last_remote_typing_time;
chrono::steady_clock::time_point last_local_typing_sent;

//  UI STATE

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

// INPUT BUFFERS

char targetIP[128] = "127.0.0.1";
char portBuf[16] = "8080";
char joinPortBuf[16] = "8080";
char messageBuf[1024] = "";
char editMessageBuf[1024] = "";
char searchBuf[128] = "";

// NEW:
// Search riêng cho Contact / Peer Discovery
char peerSearchBuf[128] = "";

// APPLICATION STATE

AppState currentState = IDLE;

// TCP ASIO OBJECTS

asio::io_context* io_context_ptr = nullptr;
tcp::socket* peer_socket = nullptr;
tcp::acceptor* peer_acceptor = nullptr;
thread* network_thread = nullptr;
asio::streambuf read_buffer;
mutex socket_write_mutex;

// PEER DISCOVERY STATE

mutex peer_mutex;
vector<PeerInfo> discoveredPeers;
vector<RecentPeer> recentPeers;
atomic<bool> discoveryRunning(false);
thread discoveryThread;
mutex discoveryConfigMutex;
string discoveryName = "Anonymous";
string discoveryRole = "Idle";

int discoveryTcpPort = DEFAULT_CHAT_PORT;

string discoveryStatus = "Discovery service starting...";
string discoveryError = "";

atomic<bool> discoveryScanRequested(false);

// HELPER FUNCTIONS

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

    ss << setfill('0')
        << setw(2) << buf.tm_hour
        << ":"
        << setw(2) << buf.tm_min
        << ":"
        << setw(2) << buf.tm_sec;

    return ss.str();
}

string generate_random_id() {
    return "#" + to_string(1000 + (rand() % 9000));
}

string generate_msg_id() {
    ++messageCounter;

    return local_user_id + "_" + to_string(messageCounter);
}

void safe_copy(char* destination,
    size_t destinationSize,
    const string& source) {
#if defined(_MSC_VER)
    strncpy_s(destination,
        destinationSize,
        source.c_str(),
        _TRUNCATE);
#else
    if (destinationSize == 0)
        return;

    strncpy(destination,
        source.c_str(),
        destinationSize - 1);

    destination[destinationSize - 1] = '\0';
#endif
}

string sanitize_field(string value) {
    replace(value.begin(), value.end(), '|', '/');
    replace(value.begin(), value.end(), '\n', ' ');
    replace(value.begin(), value.end(), '\r', ' ');
    return value;
}

// VALIDATE IP / PORT


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
        for (char c : seg) {
            if (!isdigit(static_cast<unsigned char>(c)))
                return false;
        }
        try {
            int num = stoi(seg);
            if (num < 0 || num > 255)
                return false;
        }
        catch (...) {
            return false;
        }
    }

    return true;
}

bool isValidPort(const string& portStr, int& outPort) {
    if (portStr.empty())
        return false;

    for (char c : portStr) {
        if (!isdigit(static_cast<unsigned char>(c)))
            return false;
    }

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

// AVATAR

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

void render_avatar(const string& name,
    const string& id,
    float size = 32.0f) {
    string initial = "?";

    if (!name.empty() && name != "Anonymous") {
        initial = string(1,
            static_cast<char>(
                toupper(
                    static_cast<unsigned char>(
                        name[0]))));
    }
    else if (!id.empty()) {
        initial = string(1,
            static_cast<char>(
                toupper(
                    static_cast<unsigned char>(
                        id[0]))));
    }

    ImVec4 col = get_avatar_color(name + id);

    ImVec2 p = ImGui::GetCursorScreenPos();

    ImDrawList* draw_list =
        ImGui::GetWindowDrawList();

    ImVec2 center(
        p.x + size * 0.5f,
        p.y + size * 0.5f
    );

    draw_list->AddCircleFilled(
        center,
        size * 0.5f,
        ImGui::ColorConvertFloat4ToU32(col)
    );

    ImVec2 text_sz =
        ImGui::CalcTextSize(initial.c_str());

    ImVec2 text_pos(
        center.x - text_sz.x * 0.5f,
        center.y - text_sz.y * 0.5f
    );

    draw_list->AddText(
        text_pos,
        IM_COL32(255, 255, 255, 255),
        initial.c_str()
    );

    ImGui::Dummy(ImVec2(size, size));
}

// INPUT FILTERS

static int PortInputFilter(
    ImGuiInputTextCallbackData* data) {

    return (data->EventChar >= '0' &&
        data->EventChar <= '9')
        ? 0
        : 1;
}

static int IPInputFilter(
    ImGuiInputTextCallbackData* data) {

    if ((data->EventChar >= '0' &&
        data->EventChar <= '9') ||
        data->EventChar == '.') {
        return 0;
    }

    return 1;
}

// DEBOUNCE CHAR CALLBACK

static unsigned int g_LastChar = 0;

static chrono::steady_clock::time_point
g_LastCharTime;

void CustomDebouncedCharCallback(
    GLFWwindow* window,
    unsigned int c) {

    auto now = chrono::steady_clock::now();

    auto elapsedMs =
        chrono::duration_cast<chrono::milliseconds>(
            now - g_LastCharTime
        ).count();

    if (c == g_LastChar && elapsedMs < 35)
        return;

    g_LastChar = c;
    g_LastCharTime = now;

    ImGui_ImplGlfw_CharCallback(window, c);
}

// CHAT LOG FUNCTIONS

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

void add_chat_log(
    const string& msg_id,
    const string& sender_name,
    const string& sender_id,
    const string& role,
    const string& timestamp,
    const string& reply_name,
    const string& reply_text,
    const string& content,
    bool is_self) {

    lock_guard<mutex> lock(chat_mutex);

    ChatMessage cm;

    cm.id = msg_id.empty()
        ? generate_msg_id()
        : msg_id;

    cm.sender_name = sender_name;
    cm.sender_id = sender_id;
    cm.role = role;

    cm.timestamp =
        timestamp.empty()
        ? get_current_time_str()
        : timestamp;

    cm.reply_to_name = reply_name;
    cm.reply_to_text = reply_text;

    cm.content = content;

    cm.is_system = false;
    cm.is_self = is_self;
    cm.is_delivered = !is_self;

    chatHistoryList.push_back(cm);
}

// NETWORK SEND

void send_raw_line(const string& line) {
    lock_guard<mutex> writeLock(socket_write_mutex);

    if (!peer_socket ||
        !peer_socket->is_open()) {
        return;
    }

    asio::error_code ec;

    string msg =
        line + "\n";

    asio::write(
        *peer_socket,
        asio::buffer(msg),
        ec
    );
}

// DISCOVERY CONFIG

void update_discovery_config() {
    lock_guard<mutex> lock(
        discoveryConfigMutex
    );

    discoveryName =
        (strlen(username) > 0)
        ? string(username)
        : "Anonymous";

    discoveryRole =
        local_role;

    discoveryTcpPort =
        local_tcp_port;
}

// RECENT PEER STORAGE

void save_recent_peers() {
    lock_guard<mutex> lock(peer_mutex);

    ofstream file(RECENT_PEERS_FILE);

    if (!file.is_open())
        return;

    for (const auto& peer : recentPeers) {
        file
            << sanitize_field(peer.id)
            << "|"
            << sanitize_field(peer.name)
            << "|"
            << sanitize_field(peer.ip)
            << "|"
            << peer.port
            << "|"
            << sanitize_field(peer.role)
            << "|"
            << sanitize_field(peer.last_connected)
            << "\n";
    }
}

void load_recent_peers() {
    lock_guard<mutex> lock(peer_mutex);

    recentPeers.clear();

    ifstream file(RECENT_PEERS_FILE);

    if (!file.is_open())
        return;

    string line;

    while (getline(file, line)) {
        if (line.empty())
            continue;

        stringstream ss(line);

        RecentPeer peer;

        string portString;

        getline(ss, peer.id, '|');
        getline(ss, peer.name, '|');
        getline(ss, peer.ip, '|');
        getline(ss, portString, '|');
        getline(ss, peer.role, '|');
        getline(ss, peer.last_connected);

        try {
            peer.port = stoi(portString);
        }
        catch (...) {
            peer.port = DEFAULT_CHAT_PORT;
        }

        if (!peer.id.empty() &&
            !peer.ip.empty()) {
            recentPeers.push_back(peer);
        }
    }
}

void remember_peer(
    const string& id,
    const string& name,
    const string& ip,
    int port,
    const string& role) {

    if (id.empty() || ip.empty())
        return;

    lock_guard<mutex> lock(peer_mutex);

    auto it = find_if(
        recentPeers.begin(),
        recentPeers.end(),
        [&](const RecentPeer& peer) {
            return peer.id == id ||
                (peer.ip == ip &&
                    peer.port == port);
        }
    );

    RecentPeer newPeer;

    newPeer.id = id;
    newPeer.name =
        name.empty()
        ? "Anonymous"
        : name;
    newPeer.ip = ip;
    newPeer.port = port;
    newPeer.role = role;
    newPeer.last_connected =
        get_current_time_str();

    if (it != recentPeers.end()) {
        *it = newPeer;
    }
    else {
        recentPeers.insert(
            recentPeers.begin(),
            newPeer
        );
    }

    if (recentPeers.size() > 20)
        recentPeers.resize(20);
}

// SAVE RECENT PEER WITHOUT DEADLOCK

void remember_peer_and_save(
    const string& id,
    const string& name,
    const string& ip,
    int port,
    const string& role) {

        {
            lock_guard<mutex> lock(peer_mutex);

            auto it = find_if(
                recentPeers.begin(),
                recentPeers.end(),
                [&](const RecentPeer& peer) {
                    return peer.id == id ||
                        (peer.ip == ip &&
                            peer.port == port);
                }
            );

            RecentPeer newPeer;

            newPeer.id = id;
            newPeer.name =
                name.empty()
                ? "Anonymous"
                : name;
            newPeer.ip = ip;
            newPeer.port = port;
            newPeer.role = role;
            newPeer.last_connected =
                get_current_time_str();

            if (it != recentPeers.end()) {
                *it = newPeer;
            }
            else {
                recentPeers.insert(
                    recentPeers.begin(),
                    newPeer
                );
            }

            if (recentPeers.size() > 20)
                recentPeers.resize(20);

            ofstream file(RECENT_PEERS_FILE);

            if (file.is_open()) {
                for (const auto& peer : recentPeers) {
                    file
                        << sanitize_field(peer.id)
                        << "|"
                        << sanitize_field(peer.name)
                        << "|"
                        << sanitize_field(peer.ip)
                        << "|"
                        << peer.port
                        << "|"
                        << sanitize_field(peer.role)
                        << "|"
                        << sanitize_field(
                            peer.last_connected)
                        << "\n";
                }
            }
        }
}

// DISCOVERY PACKET

string build_discovery_packet() {
    lock_guard<mutex> lock(
        discoveryConfigMutex
    );

    string name =
        sanitize_field(discoveryName);

    string role =
        sanitize_field(discoveryRole);

    return "[DISCOVER]|" +
        local_user_id +
        "|" +
        name +
        "|" +
        role +
        "|" +
        to_string(discoveryTcpPort);
}

string build_discovery_reply() {
    lock_guard<mutex> lock(
        discoveryConfigMutex
    );

    string name =
        sanitize_field(discoveryName);

    string role =
        sanitize_field(discoveryRole);

    return "[DISCOVER_REPLY]|" +
        local_user_id +
        "|" +
        name +
        "|" +
        role +
        "|" +
        to_string(discoveryTcpPort);
}

// UPDATE DISCOVERED PEER

void update_discovered_peer(
    const string& id,
    const string& name,
    const string& ip,
    int port,
    const string& role) {

    if (id.empty() ||
        id == local_user_id ||
        ip.empty()) {
        return;
    }

    lock_guard<mutex> lock(peer_mutex);

    auto it = find_if(
        discoveredPeers.begin(),
        discoveredPeers.end(),
        [&](const PeerInfo& peer) {
            return peer.id == id;
        }
    );

    PeerInfo peer;

    peer.id = id;

    peer.name =
        name.empty()
        ? "Anonymous"
        : name;

    peer.ip = ip;

    peer.port =
        (port >= 1 && port <= 65535)
        ? port
        : DEFAULT_CHAT_PORT;

    peer.role =
        role.empty()
        ? "Idle"
        : role;

    peer.last_seen =
        get_current_time_str();

    if (it != discoveredPeers.end()) {
        *it = peer;
    }
    else {
        discoveredPeers.push_back(peer);
    }

    if (discoveredPeers.size() > 50)
        discoveredPeers.erase(
            discoveredPeers.begin()
        );
}

// FORCE DISCOVERY SCAN

void request_peer_discovery() {
    discoveryScanRequested = true;

    lock_guard<mutex> lock(peer_mutex);

    discoveryStatus =
        "Searching for peers...";
}

// DISCOVERY SERVICE

void discovery_loop() {
    try {
        asio::io_context discoveryContext;

        udp::socket socket(discoveryContext);

        socket.open(udp::v4());

        socket.set_option(
            asio::socket_base::reuse_address(true)
        );

        socket.set_option(
            asio::socket_base::broadcast(true)
        );

        socket.bind(
            udp::endpoint(
                udp::v4(),
                DISCOVERY_PORT
            )
        );

        socket.non_blocking(true);

        udp::endpoint broadcastEndpoint(
            asio::ip::address_v4::broadcast(),
            DISCOVERY_PORT
        );

        discoveryStatus =
            "Discovery active";

        auto lastBroadcast =
            chrono::steady_clock::now() -
            chrono::seconds(5);

        while (discoveryRunning) {
            auto now =
                chrono::steady_clock::now();

            bool forceScan =
                discoveryScanRequested.exchange(
                    false
                );

            if (forceScan ||
                chrono::duration_cast<
                chrono::milliseconds>(
                    now - lastBroadcast
                ).count() >= 2000) {

                string packet =
                    build_discovery_packet();

                asio::error_code sendError;

                socket.send_to(
                    asio::buffer(packet),
                    broadcastEndpoint,
                    0,
                    sendError
                );

                if (sendError) {
                    discoveryError =
                        "UDP discovery send error: " +
                        sendError.message();
                }

                lastBroadcast = now;
            }

            char data[2048];

            udp::endpoint sender;

            asio::error_code receiveError;

            size_t length =
                socket.receive_from(
                    asio::buffer(data),
                    sender,
                    0,
                    receiveError
                );

            if (!receiveError &&
                length > 0) {

                string packet(
                    data,
                    length
                );

                stringstream ss(packet);

                string tag;
                string remoteId;
                string remoteName;
                string remoteRole;
                string remotePortString;

                getline(ss, tag, '|');
                getline(ss, remoteId, '|');
                getline(ss, remoteName, '|');
                getline(ss, remoteRole, '|');
                getline(ss, remotePortString, '|');

                int remotePort =
                    DEFAULT_CHAT_PORT;

                try {
                    remotePort =
                        stoi(remotePortString);
                }
                catch (...) {
                }

                string remoteIp =
                    sender.address().to_string();

                if (remoteId != local_user_id &&
                    !remoteId.empty()) {

                    update_discovered_peer(
                        remoteId,
                        remoteName,
                        remoteIp,
                        remotePort,
                        remoteRole
                    );

                    // If someone is searching,
                    // answer their discovery packet.
                    if (tag == "[DISCOVER]") {

                        string reply =
                            build_discovery_reply();

                        asio::error_code replyError;

                        socket.send_to(
                            asio::buffer(reply),
                            sender,
                            0,
                            replyError
                        );
                    }
                }
            }
            else if (receiveError !=
                asio::error::would_block &&
                receiveError !=
                asio::error::try_again) {

                discoveryError =
                    "UDP discovery receive error: " +
                    receiveError.message();
            }

            this_thread::sleep_for(
                chrono::milliseconds(50)
            );
        }

        asio::error_code closeError;

        socket.close(closeError);

        discoveryStatus =
            "Discovery stopped";
    }
    catch (const exception& e) {
        discoveryError =
            string("Discovery service error: ") +
            e.what();

        discoveryStatus =
            "Discovery unavailable";
    }
}

void start_discovery_service() {
    if (discoveryRunning)
        return;

    discoveryRunning = true;

    discoveryThread =
        thread(discovery_loop);
}

void stop_discovery_service() {
    discoveryRunning = false;

    discoveryScanRequested = false;

    if (discoveryThread.joinable())
        discoveryThread.join();
}

// HANDSHAKE

void send_handshake() {
    string display_name =
        (strlen(username) > 0)
        ? string(username)
        : "Anonymous";

    display_name =
        sanitize_field(display_name);

    send_raw_line(
        "[HANDSHAKE]|" +
        local_user_id +
        "|" +
        display_name +
        "|" +
        local_role +
        "|" +
        to_string(local_tcp_port)
    );
}

// TYPING

void send_typing_status(
    bool is_typing) {

    if (currentState == CONNECTED) {
        send_raw_line(
            "[TYPING]|" +
            string(
                is_typing
                ? "1"
                : "0"
            )
        );
    }
}

// NETWORK FUNCTION PROTOTYPES

void async_read_loop();

void stop_network();

void start_hosting(int port);

void start_connecting(
    string ip,
    int port);

// TCP READ LOOP

void async_read_loop() {
    if (!peer_socket ||
        !peer_socket->is_open()) {
        return;
    }

    asio::async_read_until(
        *peer_socket,
        read_buffer,
        '\n',
        [](const asio::error_code& error,
            size_t bytes) {

                if (!error) {

                    istream is(&read_buffer);

                    string line;

                    getline(is, line);

                    if (!line.empty()) {


                        // HANDSHAKE


                        if (line.rfind(
                            "[HANDSHAKE]|",
                            0) == 0) {

                            stringstream ss(line);

                            string tag;
                            string remote_id;
                            string remote_name;
                            string remote_role;
                            string remote_port_string;

                            getline(
                                ss,
                                tag,
                                '|'
                            );

                            getline(
                                ss,
                                remote_id,
                                '|'
                            );

                            getline(
                                ss,
                                remote_name,
                                '|'
                            );

                            getline(
                                ss,
                                remote_role,
                                '|'
                            );

                            getline(
                                ss,
                                remote_port_string
                            );

                            int remotePort =
                                DEFAULT_CHAT_PORT;

                            try {
                                remotePort =
                                    stoi(
                                        remote_port_string
                                    );
                            }
                            catch (...) {
                            }

                            peer_user_id =
                                remote_id;

                            peer_username =
                                remote_name;

                            peer_role =
                                remote_role;

                            peer_tcp_port =
                                remotePort;

                            string connectionIP =
                                peer_ip;

                            if (connectionIP.empty() &&
                                peer_socket) {

                                asio::error_code epError;

                                auto endpoint =
                                    peer_socket->remote_endpoint(
                                        epError
                                    );

                                if (!epError) {
                                    connectionIP =
                                        endpoint.address()
                                        .to_string();

                                    peer_ip =
                                        connectionIP;
                                }
                            }

                            add_system_log(
                                remote_name +
                                " (" +
                                remote_id +
                                " - " +
                                remote_role +
                                ") joined the chat session"
                            );

                            if (!connectionIP.empty()) {

                                remember_peer_and_save(
                                    remote_id,
                                    remote_name,
                                    connectionIP,
                                    remotePort,
                                    remote_role
                                );
                            }
                        }


                        // MESSAGE


                        else if (line.rfind(
                            "[MSG]|",
                            0) == 0) {

                            stringstream ss(line);

                            string tag;
                            string msg_id;
                            string remote_id;
                            string remote_name;
                            string remote_role;
                            string ts;
                            string r_name;
                            string r_text;
                            string content;

                            getline(ss, tag, '|');
                            getline(ss, msg_id, '|');
                            getline(ss, remote_id, '|');
                            getline(ss, remote_name, '|');
                            getline(ss, remote_role, '|');
                            getline(ss, ts, '|');
                            getline(ss, r_name, '|');
                            getline(ss, r_text, '|');

                            getline(
                                ss,
                                content
                            );

                            add_chat_log(
                                msg_id,
                                remote_name,
                                remote_id,
                                remote_role,
                                ts,
                                r_name,
                                r_text,
                                content,
                                false
                            );

                            send_raw_line(
                                "[ACK]|" +
                                msg_id
                            );

                            scrollToBottomRequested =
                                true;
                        }

                        // TYPING

                        else if (line.rfind(
                            "[TYPING]|",
                            0) == 0) {

                            stringstream ss(line);

                            string tag;
                            string flag;

                            getline(
                                ss,
                                tag,
                                '|'
                            );

                            getline(
                                ss,
                                flag,
                                '|'
                            );

                            remote_is_typing =
                                (flag == "1");

                            last_remote_typing_time =
                                chrono::steady_clock::now();
                        }


                        // REACTION


                        else if (line.rfind(
                            "[REACTION]|",
                            0) == 0) {

                            stringstream ss(line);

                            string tag;
                            string target_msg_id;
                            string emoji;
                            string reactor_id;

                            getline(
                                ss,
                                tag,
                                '|'
                            );

                            getline(
                                ss,
                                target_msg_id,
                                '|'
                            );

                            getline(
                                ss,
                                emoji,
                                '|'
                            );

                            getline(
                                ss,
                                reactor_id,
                                '|'
                            );

                            if (reactor_id.empty())
                                reactor_id =
                                peer_user_id;

                            lock_guard<mutex> lock(
                                chat_mutex
                            );

                            for (auto& msg :
                                chatHistoryList) {

                                if (msg.id ==
                                    target_msg_id) {

                                    auto it =
                                        find_if(
                                            msg.reactions.begin(),
                                            msg.reactions.end(),
                                            [&](const ReactionItem& item) {
                                                return
                                                    item.emoji ==
                                                    emoji &&
                                                    item.user_id ==
                                                    reactor_id;
                                            }
                                        );

                                    if (it !=
                                        msg.reactions.end()) {

                                        msg.reactions.erase(
                                            it
                                        );
                                    }
                                    else {

                                        msg.reactions.push_back(
                                            {
                                                emoji,
                                                reactor_id
                                            }
                                        );
                                    }

                                    break;
                                }
                            }
                        }


                        // ACK


                        else if (line.rfind(
                            "[ACK]|",
                            0) == 0) {

                            stringstream ss(line);

                            string tag;
                            string ack_msg_id;

                            getline(
                                ss,
                                tag,
                                '|'
                            );

                            getline(
                                ss,
                                ack_msg_id,
                                '|'
                            );

                            lock_guard<mutex> lock(
                                chat_mutex
                            );

                            for (auto& msg :
                                chatHistoryList) {

                                if (msg.id ==
                                    ack_msg_id) {

                                    msg.is_delivered =
                                        true;

                                    break;
                                }
                            }
                        }


                        // EDIT


                        else if (line.rfind(
                            "[EDIT]|",
                            0) == 0) {

                            stringstream ss(line);

                            string tag;
                            string edit_msg_id;
                            string new_content;

                            getline(
                                ss,
                                tag,
                                '|'
                            );

                            getline(
                                ss,
                                edit_msg_id,
                                '|'
                            );

                            getline(
                                ss,
                                new_content
                            );

                            lock_guard<mutex> lock(
                                chat_mutex
                            );

                            for (auto& msg :
                                chatHistoryList) {

                                if (msg.id ==
                                    edit_msg_id) {

                                    msg.content =
                                        new_content;

                                    msg.is_edited =
                                        true;

                                    break;
                                }
                            }
                        }


                        // DELETE


                        else if (line.rfind(
                            "[DELETE]|",
                            0) == 0) {

                            stringstream ss(line);

                            string tag;
                            string del_msg_id;

                            getline(
                                ss,
                                tag,
                                '|'
                            );

                            getline(
                                ss,
                                del_msg_id
                            );

                            lock_guard<mutex> lock(
                                chat_mutex
                            );

                            for (auto& msg :
                                chatHistoryList) {

                                if (msg.id ==
                                    del_msg_id) {

                                    msg.is_deleted =
                                        true;

                                    break;
                                }
                            }
                        }


                        // LEAVE


                        else if (line.rfind(
                            "[LEAVE]|",
                            0) == 0) {

                            stringstream ss(line);

                            string tag;
                            string remote_id;
                            string remote_name;

                            getline(
                                ss,
                                tag,
                                '|'
                            );

                            getline(
                                ss,
                                remote_id,
                                '|'
                            );

                            getline(
                                ss,
                                remote_name,
                                '|'
                            );

                            add_system_log(
                                remote_name +
                                " (" +
                                remote_id +
                                ") left the chat session"
                            );
                        }


                        // UNKNOWN


                        else {

                            string remote_name =
                                peer_username.empty()
                                ? "Peer"
                                : peer_username;

                            add_chat_log(
                                "",
                                remote_name,
                                peer_user_id,
                                peer_role,
                                get_current_time_str(),
                                "",
                                "",
                                line,
                                false
                            );
                        }
                    }

                    async_read_loop();
                }


                // CONNECTION ERROR


                else {

                    if (local_role == "Host") {

                        add_system_log(
                            "Peer disconnected. Host remains open waiting for a new connection."
                        );

                        if (peer_socket) {

                            asio::error_code ec;

                            peer_socket->close(ec);
                        }

                        peer_username = "";
                        peer_user_id = "";
                        peer_role = "";
                        peer_ip = "";

                        currentState =
                            WAITING_FOR_PEER;

                        remote_is_typing =
                            false;

                        if (peer_acceptor &&
                            peer_acceptor->is_open() &&
                            peer_socket) {

                            peer_acceptor->async_accept(
                                *peer_socket,
                                [](const asio::error_code& accept_ec) {

                                    if (!accept_ec) {

                                        asio::error_code endpointError;

                                        if (peer_socket) {

                                            auto ep =
                                                peer_socket->remote_endpoint(
                                                    endpointError
                                                );

                                            if (!endpointError) {
                                                peer_ip =
                                                    ep.address()
                                                    .to_string();
                                            }
                                        }

                                        add_system_log(
                                            "Connected with a new Peer"
                                        );

                                        currentState =
                                            CONNECTED;

                                        send_handshake();

                                        async_read_loop();
                                    }
                                }
                            );
                        }
                    }
                    else {

                        add_system_log(
                            "Connection lost to Host"
                        );

                        currentState =
                            DISCONNECTED_NOTICE;

                        remote_is_typing =
                            false;
                    }
                }
        }
    );
}


// STOP NETWORK


void stop_network() {

    if (io_context_ptr)
        io_context_ptr->stop();

    if (peer_socket) {

        asio::error_code ec;

        peer_socket->close(ec);
    }

    if (peer_acceptor) {

        asio::error_code ec;

        peer_acceptor->close(ec);
    }

    if (network_thread &&
        network_thread->joinable()) {

        network_thread->join();

        delete network_thread;

        network_thread = nullptr;
    }

    if (peer_socket) {

        delete peer_socket;

        peer_socket = nullptr;
    }

    if (peer_acceptor) {

        delete peer_acceptor;

        peer_acceptor = nullptr;
    }

    if (io_context_ptr) {

        delete io_context_ptr;

        io_context_ptr = nullptr;
    }

    currentState =
        IDLE;

    local_role =
        "Idle";

    local_tcp_port =
        DEFAULT_CHAT_PORT;

    peer_username = "";
    peer_user_id = "";
    peer_role = "";
    peer_ip = "";

    remote_is_typing =
        false;

    replyTargetId = "";
    replyTargetName = "";
    replyTargetText = "";

    update_discovery_config();
}

// START HOST


void start_hosting(int port) {

    stop_network();

    {
        lock_guard<mutex> lock(
            chat_mutex
        );

        chatHistoryList.clear();

        unreadMessageCount = 0;

        unreadTrackingIndex = 0;

        editingMessageId = "";
    }

    statusErrorMessage = "";

    local_role =
        "Host";

    local_tcp_port =
        port;

    peer_username = "";
    peer_user_id = "";
    peer_role = "";
    peer_ip = "";

    update_discovery_config();

    try {

        io_context_ptr =
            new asio::io_context();

        peer_acceptor =
            new tcp::acceptor(
                *io_context_ptr,
                tcp::endpoint(
                    tcp::v4(),
                    port
                )
            );

        peer_socket =
            new tcp::socket(
                *io_context_ptr
            );

        currentState =
            WAITING_FOR_PEER;

        add_system_log(
            "Listening on Port " +
            to_string(port) +
            ". Waiting for peer to connect..."
        );

        peer_acceptor->async_accept(
            *peer_socket,
            [](const asio::error_code& error) {

                if (!error) {

                    asio::error_code endpointError;

                    if (peer_socket) {

                        auto endpoint =
                            peer_socket->remote_endpoint(
                                endpointError
                            );

                        if (!endpointError) {

                            peer_ip =
                                endpoint.address()
                                .to_string();
                        }
                    }

                    add_system_log(
                        "Connected successfully! Chat session started."
                    );

                    currentState =
                        CONNECTED;

                    send_handshake();

                    async_read_loop();
                }
                else {

                    if (currentState !=
                        IDLE) {

                        add_system_log(
                            "Host error: " +
                            error.message()
                        );

                        currentState =
                            IDLE;
                    }
                }
            }
        );

        network_thread =
            new thread([]() {

            try {

                if (io_context_ptr)
                    io_context_ptr->run();
            }
            catch (...) {
            }
                });
    }
    catch (exception& e) {

        statusErrorMessage =
            string(
                "Host creation error: "
            ) +
            e.what();

        stop_network();
    }
}

// START CONNECTING

void start_connecting(
    string ip,
    int port) {

    stop_network();

    {
        lock_guard<mutex> lock(
            chat_mutex
        );

        chatHistoryList.clear();

        unreadMessageCount = 0;

        unreadTrackingIndex = 0;

        editingMessageId = "";
    }

    statusErrorMessage = "";

    local_role =
        "Peer";

    local_tcp_port =
        port;

    peer_ip =
        ip;

    if (ip == "localhost")
        ip = "127.0.0.1";

    update_discovery_config();

    try {

        io_context_ptr =
            new asio::io_context();

        peer_socket =
            new tcp::socket(
                *io_context_ptr
            );

        currentState =
            WAITING_FOR_PEER;

        add_system_log(
            "Connecting to " +
            ip +
            ":" +
            to_string(port) +
            "..."
        );

        tcp::endpoint endpoint(
            asio::ip::make_address(ip),
            port
        );

        peer_socket->async_connect(
            endpoint,
            [ip, port](
                const asio::error_code& error) {

                    if (!error) {

                        add_system_log(
                            "Connected successfully to Host!"
                        );

                        currentState =
                            CONNECTED;

                        peer_ip =
                            ip;

                        peer_tcp_port =
                            port;

                        send_handshake();

                        async_read_loop();
                    }
                    else {

                        if (error ==
                            asio::error::connection_refused) {

                            statusErrorMessage =
                                "Room unavailable — Host is not ready or Port is incorrect";
                        }
                        else if (
                            error ==
                            asio::error::timed_out) {

                            statusErrorMessage =
                                "Connection timed out — Please check target IP address";
                        }
                        else {

                            statusErrorMessage =
                                "Join failed: " +
                                error.message();
                        }

                        add_system_log(
                            "Connection error: " +
                            statusErrorMessage
                        );

                        currentState =
                            IDLE;

                        local_role =
                            "Idle";

                        update_discovery_config();
                    }
            }
        );

        network_thread =
            new thread([]() {

            try {

                if (io_context_ptr)
                    io_context_ptr->run();
            }
            catch (...) {
            }
                });
    }
    catch (exception& e) {

        statusErrorMessage =
            string(
                "Cannot connect to specified IP/Port: "
            ) +
            e.what();

        stop_network();
    }
}

// MODERN THEME


void apply_modern_theme() {

    ImGuiStyle& style =
        ImGui::GetStyle();

    style.WindowRounding = 8.0f;
    style.ChildRounding = 8.0f;
    style.FrameRounding = 6.0f;
    style.PopupRounding = 8.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding = 6.0f;

    style.WindowPadding =
        ImVec2(14, 14);

    style.FramePadding =
        ImVec2(10, 8);

    style.ItemSpacing =
        ImVec2(10, 10);

    style.ButtonTextAlign =
        ImVec2(0.5f, 0.5f);

    ImVec4* colors =
        style.Colors;

    colors[ImGuiCol_Text] =
        ImVec4(0.95f, 0.96f, 0.98f, 1.00f);

    colors[ImGuiCol_TextDisabled] =
        ImVec4(0.50f, 0.55f, 0.60f, 1.00f);

    colors[ImGuiCol_WindowBg] =
        ImVec4(0.11f, 0.13f, 0.17f, 1.00f);

    colors[ImGuiCol_ChildBg] =
        ImVec4(0.15f, 0.17f, 0.22f, 1.00f);

    colors[ImGuiCol_PopupBg] =
        ImVec4(0.15f, 0.17f, 0.22f, 0.98f);

    colors[ImGuiCol_Border] =
        ImVec4(0.22f, 0.26f, 0.32f, 0.50f);

    colors[ImGuiCol_FrameBg] =
        ImVec4(0.18f, 0.21f, 0.27f, 1.00f);

    colors[ImGuiCol_FrameBgHovered] =
        ImVec4(0.24f, 0.28f, 0.35f, 1.00f);

    colors[ImGuiCol_FrameBgActive] =
        ImVec4(0.30f, 0.33f, 0.42f, 1.00f);

    colors[ImGuiCol_TitleBg] =
        ImVec4(0.11f, 0.13f, 0.17f, 1.00f);

    colors[ImGuiCol_TitleBgActive] =
        ImVec4(0.15f, 0.17f, 0.22f, 1.00f);

    colors[ImGuiCol_CheckMark] =
        ImVec4(0.38f, 0.52f, 0.98f, 1.00f);

    colors[ImGuiCol_Button] =
        ImVec4(0.32f, 0.44f, 0.92f, 1.00f);

    colors[ImGuiCol_ButtonHovered] =
        ImVec4(0.40f, 0.52f, 0.98f, 1.00f);

    colors[ImGuiCol_ButtonActive] =
        ImVec4(0.26f, 0.36f, 0.82f, 1.00f);

    colors[ImGuiCol_Header] =
        ImVec4(0.22f, 0.26f, 0.33f, 1.00f);

    colors[ImGuiCol_HeaderHovered] =
        ImVec4(0.28f, 0.33f, 0.42f, 1.00f);

    colors[ImGuiCol_HeaderActive] =
        ImVec4(0.34f, 0.40f, 0.50f, 1.00f);

    colors[ImGuiCol_Separator] =
        ImVec4(0.22f, 0.26f, 0.32f, 0.80f);
}

// ============================================================
// CHAT BUBBLE
// ============================================================

void render_chat_bubble(
    size_t idx,
    ChatMessage& msg) {

    ImGui::Spacing();

    string statusMark =
        msg.is_self
        ? (msg.is_delivered
            ? " ✓✓"
            : " ✓")
        : "";

    string editedMark =
        (msg.is_edited &&
            !msg.is_deleted)
        ? " (edited)"
        : "";

    string headerText =
        msg.is_self
        ? (
            "You " +
            msg.sender_id +
            " • " +
            msg.timestamp +
            editedMark +
            statusMark
            )
        : (
            msg.sender_name +
            " " +
            msg.sender_id +
            " • " +
            msg.role +
            " • " +
            msg.timestamp +
            editedMark
            );

    string displayContent =
        msg.is_deleted
        ? "This message was deleted."
        : msg.content;

    bool showReplyPreview =
        !msg.is_deleted &&
        !msg.reply_to_text.empty();

    float maxBubbleWidth =
        max(
            340.0f,
            min(
                540.0f,
                ImGui::GetWindowWidth() *
                0.75f
            )
        );

    ImVec2 textSize =
        ImGui::CalcTextSize(
            displayContent.c_str(),
            NULL,
            false,
            maxBubbleWidth - 24.0f
        );

    float headerWidth =
        ImGui::CalcTextSize(
            headerText.c_str()
        ).x;

    float replyWidth =
        !showReplyPreview
        ? 0.0f
        : ImGui::CalcTextSize(
            (
                "Replying to " +
                msg.reply_to_name +
                ": " +
                msg.reply_to_text
                ).c_str(),
            NULL,
            false,
            maxBubbleWidth - 24.0f
        ).x;

    float bubbleWidth =
        max(
            340.0f,
            min(
                maxBubbleWidth,
                max(
                    {
                        headerWidth,
                        textSize.x,
                        replyWidth
                    }
                ) + 30.0f
            )
        );

    if (msg.is_self) {

        float posX =
            max(
                10.0f,
                ImGui::GetWindowWidth() -
                bubbleWidth -
                25.0f
            );

        ImGui::SetCursorPosX(posX);

        ImGui::PushStyleColor(
            ImGuiCol_ChildBg,
            ImVec4(
                0.25f,
                0.38f,
                0.85f,
                0.95f
            )
        );
    }
    else {

        ImGui::PushStyleColor(
            ImGuiCol_ChildBg,
            ImVec4(
                0.18f,
                0.21f,
                0.27f,
                0.95f
            )
        );
    }

    ImGui::BeginChild(
        msg.id.c_str(),
        ImVec2(
            bubbleWidth,
            0
        ),
        ImGuiChildFlags_AutoResizeY |
        ImGuiChildFlags_AlwaysUseWindowPadding,
        ImGuiWindowFlags_NoScrollbar
    );

    ImGui::TextColored(
        msg.is_self
        ? ImVec4(
            0.85f,
            0.90f,
            1.00f,
            1.00f
        )
        : ImVec4(
            0.45f,
            0.75f,
            1.00f,
            1.00f
        ),
        "%s",
        headerText.c_str()
    );

    if (msg.is_deleted) {

        ImGui::TextColored(
            ImVec4(
                0.55f,
                0.60f,
                0.68f,
                1.00f
            ),
            "This message was deleted."
        );
    }
    else if (editingMessageId ==
        msg.id) {

        if (showReplyPreview) {

            ImGui::TextColored(
                ImVec4(
                    0.70f,
                    0.80f,
                    1.00f,
                    0.90f
                ),
                "Replying to %s: \"%s\"",
                msg.reply_to_name.c_str(),
                msg.reply_to_text.c_str()
            );

            ImGui::Separator();
        }

        ImGui::PushItemWidth(
            bubbleWidth - 24.0f
        );

        bool editEnterPressed =
            ImGui::InputText(
                (
                    "##edit_" +
                    msg.id
                    ).c_str(),
                editMessageBuf,
                IM_ARRAYSIZE(editMessageBuf),
                ImGuiInputTextFlags_EnterReturnsTrue
            );

        ImGui::PopItemWidth();

        ImGui::PushID(
            static_cast<int>(idx)
        );

        if (ImGui::SmallButton("Save") ||
            editEnterPressed) {

            if (strlen(editMessageBuf) > 0) {

                msg.content =
                    string(editMessageBuf);

                msg.is_edited =
                    true;

                send_raw_line(
                    "[EDIT]|" +
                    msg.id +
                    "|" +
                    sanitize_field(
                        msg.content
                    )
                );
            }

            editingMessageId =
                "";
        }

        ImGui::SameLine();

        if (ImGui::SmallButton("Cancel")) {
            editingMessageId =
                "";
        }

        ImGui::PopID();
    }
    else {

        if (showReplyPreview) {

            ImGui::TextColored(
                ImVec4(
                    0.70f,
                    0.80f,
                    1.00f,
                    0.90f
                ),
                "Replying to %s: \"%s\"",
                msg.reply_to_name.c_str(),
                msg.reply_to_text.c_str()
            );

            ImGui::Separator();
        }

        ImGui::TextWrapped(
            "%s",
            displayContent.c_str()
        );


        // REACTION COUNTS


        map<string, pair<int, bool>>
            reactionCounts;

        for (const auto& rItem :
            msg.reactions) {

            auto& entry =
                reactionCounts[
                    rItem.emoji
                ];

            entry.first++;

            if (rItem.user_id ==
                local_user_id) {

                entry.second =
                    true;
            }
        }

        if (!reactionCounts.empty()) {

            ImGui::Spacing();

            ImGui::PushID(
                (
                    "reactions_" +
                    msg.id
                    ).c_str()
            );

            int rIdx = 0;

            for (auto& kv :
                reactionCounts) {

                string emoji =
                    kv.first;

                int count =
                    kv.second.first;

                bool myReaction =
                    kv.second.second;

                if (rIdx > 0)
                    ImGui::SameLine();

                string badgeText =
                    emoji +
                    " " +
                    to_string(count) +
                    (
                        myReaction
                        ? " ✓"
                        : ""
                        );

                if (myReaction) {

                    ImGui::PushStyleColor(
                        ImGuiCol_Button,
                        ImVec4(
                            0.15f,
                            0.45f,
                            0.95f,
                            1.00f
                        )
                    );

                    ImGui::PushStyleColor(
                        ImGuiCol_ButtonHovered,
                        ImVec4(
                            0.25f,
                            0.55f,
                            1.00f,
                            1.00f
                        )
                    );

                    ImGui::PushStyleColor(
                        ImGuiCol_Text,
                        ImVec4(
                            1.00f,
                            1.00f,
                            1.00f,
                            1.00f
                        )
                    );
                }
                else {

                    ImGui::PushStyleColor(
                        ImGuiCol_Button,
                        ImVec4(
                            0.18f,
                            0.22f,
                            0.30f,
                            0.85f
                        )
                    );

                    ImGui::PushStyleColor(
                        ImGuiCol_ButtonHovered,
                        ImVec4(
                            0.26f,
                            0.32f,
                            0.42f,
                            1.00f
                        )
                    );

                    ImGui::PushStyleColor(
                        ImGuiCol_Text,
                        ImVec4(
                            0.85f,
                            0.90f,
                            0.95f,
                            1.00f
                        )
                    );
                }

                if (ImGui::SmallButton(
                    badgeText.c_str())) {

                    send_raw_line(
                        "[REACTION]|" +
                        msg.id +
                        "|" +
                        emoji +
                        "|" +
                        local_user_id
                    );

                    auto it =
                        find_if(
                            msg.reactions.begin(),
                            msg.reactions.end(),
                            [&](const ReactionItem& item) {
                                return
                                    item.emoji ==
                                    emoji &&
                                    item.user_id ==
                                    local_user_id;
                            }
                        );

                    if (it !=
                        msg.reactions.end()) {

                        msg.reactions.erase(
                            it
                        );
                    }
                    else {

                        msg.reactions.push_back(
                            {
                                emoji,
                                local_user_id
                            }
                        );
                    }
                }

                ImGui::PopStyleColor(3);

                rIdx++;
            }

            ImGui::PopID();
        }


        // ACTION BUTTONS


        ImGui::Spacing();

        ImGui::PushID(
            static_cast<int>(idx)
        );

        if (msg.is_self) {

            if (ImGui::SmallButton("Copy")) {
                ImGui::SetClipboardText(
                    msg.content.c_str()
                );
            }

            ImGui::SameLine();

            if (ImGui::SmallButton("Edit")) {

                editingMessageId =
                    msg.id;

                safe_copy(
                    editMessageBuf,
                    sizeof(editMessageBuf),
                    msg.content
                );
            }

            ImGui::SameLine();

            if (ImGui::SmallButton("Delete")) {

                msg.is_deleted =
                    true;

                send_raw_line(
                    "[DELETE]|" +
                    msg.id
                );
            }

            ImGui::SameLine();
        }
        else {

            if (ImGui::SmallButton("Reply")) {

                replyTargetId =
                    msg.id;

                replyTargetName =
                    msg.sender_name;

                replyTargetText =
                    msg.content.substr(
                        0,
                        35
                    );
            }

            ImGui::SameLine();

            if (ImGui::SmallButton("Forward")) {

                string forwardContent =
                    "[Forwarded from " +
                    msg.sender_name +
                    "]: " +
                    msg.content;

                safe_copy(
                    messageBuf,
                    sizeof(messageBuf),
                    forwardContent
                );
            }

            ImGui::SameLine();

            if (ImGui::SmallButton("Copy")) {

                ImGui::SetClipboardText(
                    msg.content.c_str()
                );
            }

            ImGui::SameLine();
        }


        // QUICK REACTIONS


        const char* quickIcons[] = {
            "❤️",
            "⭐",
            "✨",
            "✔",
            "✖"
        };

        ImGui::PushStyleVar(
            ImGuiStyleVar_FramePadding,
            ImVec2(2, 2)
        );

        for (int k = 0; k < 5; k++) {

            if (k > 0)
                ImGui::SameLine();

            string emoji =
                quickIcons[k];

            bool isMyReacted =
                any_of(
                    msg.reactions.begin(),
                    msg.reactions.end(),
                    [&](const ReactionItem& r) {
                        return
                            r.emoji == emoji &&
                            r.user_id ==
                            local_user_id;
                    }
                );

            if (isMyReacted) {

                ImGui::PushStyleColor(
                    ImGuiCol_Button,
                    ImVec4(
                        0.20f,
                        0.50f,
                        1.00f,
                        1.00f
                    )
                );

                ImGui::PushStyleColor(
                    ImGuiCol_ButtonHovered,
                    ImVec4(
                        0.30f,
                        0.60f,
                        1.00f,
                        1.00f
                    )
                );
            }
            else {

                ImGui::PushStyleColor(
                    ImGuiCol_Button,
                    ImVec4(
                        0.20f,
                        0.26f,
                        0.40f,
                        0.60f
                    )
                );

                ImGui::PushStyleColor(
                    ImGuiCol_ButtonHovered,
                    ImVec4(
                        0.30f,
                        0.40f,
                        0.60f,
                        1.00f
                    )
                );
            }

            if (ImGui::Button(
                quickIcons[k],
                ImVec2(32, 26))) {

                send_raw_line(
                    "[REACTION]|" +
                    msg.id +
                    "|" +
                    emoji +
                    "|" +
                    local_user_id
                );

                auto it =
                    find_if(
                        msg.reactions.begin(),
                        msg.reactions.end(),
                        [&](const ReactionItem& item) {
                            return
                                item.emoji ==
                                emoji &&
                                item.user_id ==
                                local_user_id;
                        }
                    );

                if (it !=
                    msg.reactions.end()) {

                    msg.reactions.erase(
                        it
                    );
                }
                else {

                    msg.reactions.push_back(
                        {
                            emoji,
                            local_user_id
                        }
                    );
                }
            }

            ImGui::PopStyleColor(2);
        }

        ImGui::PopStyleVar();

        ImGui::PopID();
    }

    ImGui::EndChild();

    ImGui::PopStyleColor();
}


// RENDER DISCOVERED PEER


bool peer_matches_search(
    const PeerInfo& peer,
    const string& filter) {

    if (filter.empty())
        return true;

    string text =
        peer.name +
        " " +
        peer.id +
        " " +
        peer.ip +
        " " +
        peer.role;

    transform(
        text.begin(),
        text.end(),
        text.begin(),
        [](unsigned char c) {
            return static_cast<char>(
                tolower(c)
                );
        }
    );

    return text.find(filter) !=
        string::npos;
}


// RENDER RECENT PEER


bool recent_peer_matches_search(
    const RecentPeer& peer,
    const string& filter) {

    if (filter.empty())
        return true;

    string text =
        peer.name +
        " " +
        peer.id +
        " " +
        peer.ip +
        " " +
        peer.role;

    transform(
        text.begin(),
        text.end(),
        text.begin(),
        [](unsigned char c) {
            return static_cast<char>(
                tolower(c)
                );
        }
    );

    return text.find(filter) !=
        string::npos;
}


// MAIN


int main() {

    srand(
        static_cast<unsigned int>(
            time(nullptr)
            )
    );

    local_user_id =
        generate_random_id();

    load_recent_peers();


    // GLFW


    if (!glfwInit())
        return -1;

    GLFWwindow* window =
        glfwCreateWindow(
            900,
            860,
            "P2P Chat - Peer Discovery",
            NULL,
            NULL
        );

    if (!window) {

        glfwTerminate();

        return -1;
    }

    glfwMakeContextCurrent(
        window
    );

    glfwSwapInterval(1);


    // IMGUI


    IMGUI_CHECKVERSION();

    ImGui::CreateContext();

    ImGuiIO& io =
        ImGui::GetIO();

    io.IniFilename = nullptr;

    apply_modern_theme();


    // FONTS


    ImFontConfig font_config;

    font_config.OversampleH = 2;
    font_config.OversampleV = 2;

    if (fs::exists(
        "C:\\Windows\\Fonts\\segoeui.ttf")) {

        io.Fonts->AddFontFromFileTTF(
            "C:\\Windows\\Fonts\\segoeui.ttf",
            17.0f,
            &font_config,
            io.Fonts->GetGlyphRangesVietnamese()
        );
    }
    else if (
        fs::exists(
            "C:\\Windows\\Fonts\\arial.ttf")) {

        io.Fonts->AddFontFromFileTTF(
            "C:\\Windows\\Fonts\\arial.ttf",
            17.0f,
            &font_config,
            io.Fonts->GetGlyphRangesVietnamese()
        );
    }
    else {

        io.Fonts->AddFontDefault();
    }

    if (fs::exists(
        "C:\\Windows\\Fonts\\seguiemj.ttf")) {

        ImFontConfig emoji_config;

        emoji_config.MergeMode = true;

        emoji_config.OversampleH = 1;
        emoji_config.OversampleV = 1;

        static const ImWchar
            emoji_ranges[] = {
                0x2000,
                0x3300,

                0x2600,
                0x27FF,

                0xE000,
                0xFFFD,

                0
        };

        io.Fonts->AddFontFromFileTTF(
            "C:\\Windows\\Fonts\\seguiemj.ttf",
            17.0f,
            &emoji_config,
            emoji_ranges
        );
    }


    // BACKENDS


    ImGui_ImplGlfw_InitForOpenGL(
        window,
        true
    );

    glfwSetCharCallback(
        window,
        CustomDebouncedCharCallback
    );

    ImGui_ImplOpenGL3_Init(
        "#version 130"
    );


    // START UDP PEER DISCOVERY


    update_discovery_config();

    start_discovery_service();


    // MAIN LOOP


    while (!glfwWindowShouldClose(window)) {

        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();

        ImGui_ImplGlfw_NewFrame();

        ImGui::NewFrame();


        // REMOTE TYPING TIMEOUT


        if (remote_is_typing) {

            auto elapsed =
                chrono::duration_cast<
                chrono::seconds>(
                    chrono::steady_clock::now() -
                    last_remote_typing_time
                ).count();

            if (elapsed > 3)
                remote_is_typing =
                false;
        }


        // MAIN WINDOW


        ImGuiViewport* viewport =
            ImGui::GetMainViewport();

        ImGui::SetNextWindowPos(
            viewport->WorkPos
        );

        ImGui::SetNextWindowSize(
            viewport->WorkSize
        );

        ImGui::Begin(
            "MainWindow",
            nullptr,
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize
        );

        // IDLE / CONNECTION MANAGER


        if (currentState == IDLE) {

            ImGui::Spacing();

            ImGui::TextColored(
                ImVec4(
                    0.48f,
                    0.60f,
                    1.00f,
                    1.00f
                ),
                "P2P Chat"
            );

            ImGui::TextColored(
                ImVec4(
                    0.55f,
                    0.60f,
                    0.68f,
                    1.00f
                ),
                "Peer Discovery & Direct Peer-to-Peer Messaging"
            );

            ImGui::Spacing();


            // ERROR


            if (!statusErrorMessage.empty()) {

                ImGui::PushStyleColor(
                    ImGuiCol_ChildBg,
                    ImVec4(
                        0.35f,
                        0.12f,
                        0.15f,
                        1.00f
                    )
                );

                ImGui::BeginChild(
                    "ErrorBanner",
                    ImVec2(0, 60),
                    true
                );

                ImGui::TextColored(
                    ImVec4(
                        1.0f,
                        0.45f,
                        0.45f,
                        1.0f
                    ),
                    "System Alert"
                );

                ImGui::TextWrapped(
                    "%s",
                    statusErrorMessage.c_str()
                );

                ImGui::EndChild();

                ImGui::PopStyleColor();

                ImGui::Spacing();
            }


            // USER PROFILE


            ImGui::BeginChild(
                "ProfileSection",
                ImVec2(0, 105),
                true
            );

            ImGui::TextColored(
                ImVec4(
                    0.45f,
                    0.75f,
                    1.0f,
                    1.0f
                ),
                "Your Profile"
            );

            ImGui::Spacing();

            ImGui::Text(
                "Display Name"
            );

            ImGui::SameLine();

            ImGui::InputText(
                "##username",
                username,
                IM_ARRAYSIZE(username)
            );

            ImGui::SameLine();

            ImGui::TextColored(
                ImVec4(
                    0.40f,
                    0.80f,
                    0.50f,
                    1.00f
                ),
                "%s",
                local_user_id.c_str()
            );

            ImGui::TextColored(
                ImVec4(
                    0.55f,
                    0.60f,
                    0.68f,
                    1.00f
                ),
                "Your unique peer identifier"
            );

            update_discovery_config();

            ImGui::EndChild();

            ImGui::Spacing();


            // TWO COLUMN LAYOUT


            float availableWidth =
                ImGui::GetContentRegionAvail().x;

            float leftWidth =
                availableWidth * 0.58f;

            float rightWidth =
                availableWidth - leftWidth - 12.0f;


            // LEFT: CONTACT / DISCOVERY


            ImGui::BeginChild(
                "ContactManager",
                ImVec2(leftWidth, 0),
                true
            );

            ImGui::TextColored(
                ImVec4(
                    0.45f,
                    0.75f,
                    1.00f,
                    1.00f
                ),
                "Contacts / Peer List"
            );

            ImGui::TextColored(
                ImVec4(
                    0.55f,
                    0.60f,
                    0.68f,
                    1.00f
                ),
                "Find friends on the same LAN"
            );

            ImGui::Spacing();


            // PEER SEARCH - SEPARATE FROM CHAT SEARCH


            ImGui::InputTextWithHint(
                "##peerSearch",
                "Search contacts / peers...",
                peerSearchBuf,
                IM_ARRAYSIZE(peerSearchBuf)
            );

            ImGui::SameLine();

            if (ImGui::Button(
                "Find Friends",
                ImVec2(115, 0))) {

                request_peer_discovery();
            }

            ImGui::Spacing();

            ImGui::TextColored(
                ImVec4(
                    0.40f,
                    0.85f,
                    0.50f,
                    1.00f
                ),
                "Discovery: %s",
                discoveryStatus.c_str()
            );

            if (!discoveryError.empty()) {

                ImGui::TextColored(
                    ImVec4(
                        1.0f,
                        0.45f,
                        0.45f,
                        1.0f
                    ),
                    "%s",
                    discoveryError.c_str()
                );
            }

            ImGui::Spacing();

            ImGui::Separator();

            ImGui::Text(
                "Discovered Peers"
            );

            ImGui::Spacing();

            string peerFilter =
                peerSearchBuf;

            transform(
                peerFilter.begin(),
                peerFilter.end(),
                peerFilter.begin(),
                [](unsigned char c) {
                    return static_cast<char>(
                        tolower(c)
                        );
                }
            );

            ImGui::BeginChild(
                "DiscoveredPeerList",
                ImVec2(0, 260),
                true
            );

            {
                lock_guard<mutex> lock(
                    peer_mutex
                );

                bool foundAny =
                    false;

                for (const auto& peer :
                    discoveredPeers) {

                    if (!peer_matches_search(
                        peer,
                        peerFilter)) {
                        continue;
                    }

                    foundAny = true;

                    ImGui::PushID(
                        (
                            "discover_" +
                            peer.id
                            ).c_str()
                    );

                    render_avatar(
                        peer.name,
                        peer.id,
                        34.0f
                    );

                    ImGui::SameLine();

                    ImGui::BeginGroup();

                    ImGui::Text(
                        "%s %s",
                        peer.name.c_str(),
                        peer.id.c_str()
                    );

                    ImGui::TextColored(
                        ImVec4(
                            0.55f,
                            0.60f,
                            0.68f,
                            1.00f
                        ),
                        "%s:%d • %s • Seen %s",
                        peer.ip.c_str(),
                        peer.port,
                        peer.role.c_str(),
                        peer.last_seen.c_str()
                    );

                    ImGui::EndGroup();

                    ImGui::SameLine(
                        ImGui::GetContentRegionAvail().x -
                        105
                    );

                    bool canConnect =
                        peer.role == "Host";

                    if (!canConnect)
                        ImGui::BeginDisabled();

                    if (ImGui::Button(
                        "Connect",
                        ImVec2(90, 30))) {

                        safe_copy(
                            targetIP,
                            sizeof(targetIP),
                            peer.ip
                        );

                        safe_copy(
                            joinPortBuf,
                            sizeof(joinPortBuf),
                            to_string(
                                peer.port
                            )
                        );

                        start_connecting(
                            peer.ip,
                            peer.port
                        );
                    }

                    if (!canConnect)
                        ImGui::EndDisabled();

                    ImGui::Separator();

                    ImGui::PopID();
                }

                if (!foundAny) {

                    ImGui::TextColored(
                        ImVec4(
                            0.55f,
                            0.60f,
                            0.68f,
                            1.00f
                        ),
                        "No peers found."
                    );

                    ImGui::TextWrapped(
                        "Click \"Find Friends\" to search for P2P Chat users on your local network."
                    );
                }
            }

            ImGui::EndChild();

            ImGui::Spacing();


            // RECENT PEERS


            ImGui::Text(
                "Recent Peers"
            );

            ImGui::TextColored(
                ImVec4(
                    0.55f,
                    0.60f,
                    0.68f,
                    1.00f
                ),
                "Reconnect with one click"
            );

            ImGui::BeginChild(
                "RecentPeerList",
                ImVec2(0, 190),
                true
            );

            {
                lock_guard<mutex> lock(
                    peer_mutex
                );

                bool foundRecent =
                    false;

                for (size_t i = 0;
                    i < recentPeers.size();
                    ++i) {

                    const auto& peer =
                        recentPeers[i];

                    if (!recent_peer_matches_search(
                        peer,
                        peerFilter)) {
                        continue;
                    }

                    foundRecent = true;

                    ImGui::PushID(
                        (
                            "recent_" +
                            to_string(i)
                            ).c_str()
                    );

                    render_avatar(
                        peer.name,
                        peer.id,
                        30.0f
                    );

                    ImGui::SameLine();

                    ImGui::BeginGroup();

                    ImGui::Text(
                        "%s %s",
                        peer.name.c_str(),
                        peer.id.c_str()
                    );

                    ImGui::TextColored(
                        ImVec4(
                            0.55f,
                            0.60f,
                            0.68f,
                            1.00f
                        ),
                        "%s:%d • Last: %s",
                        peer.ip.c_str(),
                        peer.port,
                        peer.last_connected.c_str()
                    );

                    ImGui::EndGroup();

                    ImGui::SameLine(
                        ImGui::GetContentRegionAvail().x -
                        105
                    );

                    if (ImGui::Button(
                        "Reconnect",
                        ImVec2(90, 30))) {

                        safe_copy(
                            targetIP,
                            sizeof(targetIP),
                            peer.ip
                        );

                        safe_copy(
                            joinPortBuf,
                            sizeof(joinPortBuf),
                            to_string(
                                peer.port
                            )
                        );

                        start_connecting(
                            peer.ip,
                            peer.port
                        );
                    }

                    ImGui::Separator();

                    ImGui::PopID();
                }

                if (!foundRecent) {

                    ImGui::TextColored(
                        ImVec4(
                            0.55f,
                            0.60f,
                            0.68f,
                            1.00f
                        ),
                        "No recent peers yet."
                    );
                }
            }

            ImGui::EndChild();

            ImGui::EndChild();

            ImGui::SameLine();

            // RIGHT: CONNECTION METHODS


            ImGui::BeginChild(
                "ConnectionManager",
                ImVec2(rightWidth, 0),
                true
            );

            ImGui::TextColored(
                ImVec4(
                    0.45f,
                    0.75f,
                    1.00f,
                    1.00f
                ),
                "Connection"
            );

            ImGui::Spacing();


            // HOST


            ImGui::Text(
                "Host Room"
            );

            ImGui::TextColored(
                ImVec4(
                    0.55f,
                    0.60f,
                    0.68f,
                    1.00f
                ),
                "Create a room for another peer"
            );

            ImGui::Spacing();

            int parsedPort =
                DEFAULT_CHAT_PORT;

            bool validPort =
                isValidPort(
                    portBuf,
                    parsedPort
                );

            ImGui::Text(
                "TCP Port"
            );

            ImGui::InputText(
                "##hostport",
                portBuf,
                IM_ARRAYSIZE(portBuf),
                ImGuiInputTextFlags_CallbackCharFilter,
                PortInputFilter
            );

            if (!validPort) {

                ImGui::TextColored(
                    ImVec4(
                        1.0f,
                        0.4f,
                        0.4f,
                        1.0f
                    ),
                    "Port must be 1-65535"
                );
            }

            ImGui::BeginDisabled(
                !validPort
            );

            if (ImGui::Button(
                "Host Room",
                ImVec2(-1, 40))) {

                start_hosting(
                    parsedPort
                );
            }

            ImGui::EndDisabled();

            ImGui::Spacing();

            ImGui::Separator();

            ImGui::Spacing();


            // MANUAL JOIN


            ImGui::Text(
                "Manual Join"
            );

            ImGui::TextColored(
                ImVec4(
                    0.55f,
                    0.60f,
                    0.68f,
                    1.00f
                ),
                "Fallback if discovery is unavailable"
            );

            ImGui::Spacing();

            bool validIP =
                isValidIPv4(
                    targetIP
                );

            ImGui::Text(
                "Host IP"
            );

            ImGui::InputText(
                "##targetip",
                targetIP,
                IM_ARRAYSIZE(targetIP),
                ImGuiInputTextFlags_CallbackCharFilter,
                IPInputFilter
            );

            ImGui::Text(
                "Host Port"
            );

            ImGui::InputText(
                "##joinport",
                joinPortBuf,
                IM_ARRAYSIZE(joinPortBuf),
                ImGuiInputTextFlags_CallbackCharFilter,
                PortInputFilter
            );

            int parsedJoinPort =
                DEFAULT_CHAT_PORT;

            bool validJoinPort =
                isValidPort(
                    joinPortBuf,
                    parsedJoinPort
                );

            if (!validIP) {

                ImGui::TextColored(
                    ImVec4(
                        1.0f,
                        0.4f,
                        0.4f,
                        1.0f
                    ),
                    "Invalid IP"
                );
            }

            if (!validJoinPort) {

                ImGui::TextColored(
                    ImVec4(
                        1.0f,
                        0.4f,
                        0.4f,
                        1.0f
                    ),
                    "Invalid Port"
                );
            }

            ImGui::BeginDisabled(
                !validIP ||
                !validJoinPort
            );

            if (ImGui::Button(
                "Join Room",
                ImVec2(-1, 40))) {

                start_connecting(
                    targetIP,
                    parsedJoinPort
                );
            }

            ImGui::EndDisabled();

            ImGui::Spacing();

            ImGui::Separator();

            ImGui::Spacing();


            // DISCOVERY INFORMATION


            ImGui::TextColored(
                ImVec4(
                    0.45f,
                    0.75f,
                    1.00f,
                    1.00f
                ),
                "Peer Discovery"
            );

            ImGui::TextWrapped(
                "UDP discovery port: %d",
                DISCOVERY_PORT
            );

            ImGui::TextWrapped(
                "Discovery works automatically on the same LAN/Wi-Fi."
            );

            ImGui::Spacing();

            if (ImGui::Button(
                "Scan Again",
                ImVec2(-1, 36))) {

                request_peer_discovery();
            }

            ImGui::EndChild();
        }


        // CONNECTED / WAITING


        else {

            string my_name =
                (strlen(username) > 0)
                ? string(username)
                : "Anonymous";


            // HEADER


            ImGui::BeginChild(
                "HeaderBar",
                ImVec2(0, 56),
                true
            );

            render_avatar(
                my_name,
                local_user_id,
                32.0f
            );

            ImGui::SameLine();

            if (currentState ==
                CONNECTED) {

                ImGui::TextColored(
                    ImVec4(
                        0.4f,
                        0.85f,
                        0.5f,
                        1.0f
                    ),
                    "Connected"
                );
            }
            else if (
                currentState ==
                WAITING_FOR_PEER) {

                ImGui::TextColored(
                    ImVec4(
                        1.0f,
                        0.8f,
                        0.3f,
                        1.0f
                    ),
                    "Waiting for Peer..."
                );
            }
            else {

                ImGui::TextColored(
                    ImVec4(
                        1.0f,
                        0.4f,
                        0.4f,
                        1.0f
                    ),
                    "Disconnected"
                );
            }

            ImGui::SameLine();

            ImGui::Text(
                " | You: %s %s • %s",
                my_name.c_str(),
                local_user_id.c_str(),
                local_role.c_str()
            );

            if (currentState ==
                CONNECTED &&
                !peer_username.empty()) {

                ImGui::SameLine();

                ImGui::Text(
                    " | Peer:"
                );

                ImGui::SameLine();

                render_avatar(
                    peer_username,
                    peer_user_id,
                    24.0f
                );

                ImGui::SameLine();

                ImGui::Text(
                    "%s %s",
                    peer_username.c_str(),
                    peer_user_id.c_str()
                );
            }

            ImGui::SameLine(
                ImGui::GetWindowWidth() -
                240
            );

            if (ImGui::Button(
                showInfoPanel
                ? "Hide Info"
                : "Room Info",
                ImVec2(85, 30))) {

                showInfoPanel =
                    !showInfoPanel;
            }

            ImGui::SameLine();

            if (ImGui::Button(
                "Leave Room",
                ImVec2(120, 30))) {

                send_raw_line(
                    "[LEAVE]|" +
                    local_user_id +
                    "|" +
                    my_name
                );

                stop_network();
            }

            ImGui::EndChild();


            // INFO PANEL


            if (showInfoPanel) {

                ImGui::PushStyleColor(
                    ImGuiCol_ChildBg,
                    ImVec4(
                        0.14f,
                        0.16f,
                        0.21f,
                        1.00f
                    )
                );

                ImGui::BeginChild(
                    "InfoPanel",
                    ImVec2(0, 75),
                    true
                );

                ImGui::TextColored(
                    ImVec4(
                        0.48f,
                        0.60f,
                        1.00f,
                        1.00f
                    ),
                    "Room Details & Connection Status"
                );

                ImGui::Text(
                    "Protocol: TCP P2P | Discovery: UDP %d",
                    DISCOVERY_PORT
                );

                if (local_role ==
                    "Peer") {

                    ImGui::Text(
                        "Host IP: %s | Peer ID: %s",
                        peer_ip.c_str(),
                        peer_user_id.empty()
                        ? "Connecting..."
                        : peer_user_id.c_str()
                    );
                }
                else {

                    ImGui::Text(
                        "Hosting Port: %d | Connected Peer: %s",
                        local_tcp_port,
                        peer_username.empty()
                        ? "None"
                        : (
                            peer_username +
                            " " +
                            peer_user_id
                            ).c_str()
                    );
                }

                ImGui::EndChild();

                ImGui::PopStyleColor();

                ImGui::Spacing();
            }


            // CHAT SEARCH
            //
            // IMPORTANT:
            // Đây là search riêng cho lịch sử chat.
            // Không dùng chung với Peer Search.
            //

            ImGui::TextColored(
                ImVec4(
                    0.45f,
                    0.75f,
                    1.00f,
                    1.00f
                ),
                "Chat History Search"
            );

            ImGui::SameLine();

            ImGui::PushItemWidth(250);

            ImGui::InputTextWithHint(
                "##search",
                "Search messages...",
                searchBuf,
                IM_ARRAYSIZE(searchBuf)
            );

            ImGui::PopItemWidth();

            ImGui::SameLine();

            if (ImGui::Button(
                "Clear History")) {

                lock_guard<mutex> lock(
                    chat_mutex
                );

                chatHistoryList.clear();

                unreadMessageCount = 0;

                unreadTrackingIndex = 0;

                editingMessageId = "";
            }


            // CHAT REGION


            float bottomPadding =
                currentState ==
                CONNECTED
                ? 175.0f
                : 60.0f;

            ImGui::BeginChild(
                "ChatRegion",
                ImVec2(
                    0,
                    -bottomPadding
                ),
                true
            );

            {
                lock_guard<mutex> lock(
                    chat_mutex
                );

                bool wasAtBottom =
                    ImGui::GetScrollY() >=
                    ImGui::GetScrollMaxY() - 5.0f;

                if (chatHistoryList.size() >
                    unreadTrackingIndex) {

                    for (
                        size_t i =
                        unreadTrackingIndex;
                        i <
                        chatHistoryList.size();
                        i++) {

                        const ChatMessage
                            & incomingMsg =
                            chatHistoryList[i];

                        if (!incomingMsg.is_self &&
                            !incomingMsg.is_system &&
                            !wasAtBottom) {

                            unreadMessageCount++;
                        }
                    }

                    unreadTrackingIndex =
                        chatHistoryList.size();
                }

                if (wasAtBottom)
                    unreadMessageCount = 0;

                string filterStr =
                    searchBuf;

                transform(
                    filterStr.begin(),
                    filterStr.end(),
                    filterStr.begin(),
                    [](unsigned char c) {
                        return static_cast<char>(
                            tolower(c)
                            );
                    }
                );

                for (
                    size_t idx = 0;
                    idx <
                    chatHistoryList.size();
                    idx++) {

                    auto& msg =
                        chatHistoryList[idx];

                    if (!filterStr.empty()) {

                        string contentLower =
                            msg.content;

                        transform(
                            contentLower.begin(),
                            contentLower.end(),
                            contentLower.begin(),
                            [](unsigned char c) {
                                return static_cast<char>(
                                    tolower(c)
                                    );
                            }
                        );

                        if (contentLower.find(
                            filterStr
                        ) ==
                            string::npos) {

                            continue;
                        }
                    }

                    if (msg.is_system) {

                        ImGui::Spacing();

                        float textWidth =
                            ImGui::CalcTextSize(
                                msg.content.c_str()
                            ).x;

                        ImGui::SetCursorPosX(
                            max(
                                0.0f,
                                (
                                    ImGui::GetWindowWidth() -
                                    textWidth
                                    ) *
                                0.5f
                            )
                        );

                        ImGui::TextColored(
                            ImVec4(
                                0.55f,
                                0.60f,
                                0.68f,
                                1.00f
                            ),
                            "%s • %s",
                            msg.timestamp.c_str(),
                            msg.content.c_str()
                        );

                        ImGui::Spacing();
                    }
                    else {

                        render_chat_bubble(
                            idx,
                            msg
                        );
                    }
                }
            }

            if (ImGui::GetScrollY() >=
                ImGui::GetScrollMaxY() ||
                scrollToBottomRequested) {

                ImGui::SetScrollHereY(
                    1.0f
                );

                scrollToBottomRequested =
                    false;

                unreadMessageCount =
                    0;
            }

            ImGui::EndChild();


            // UNREAD


            if (unreadMessageCount > 0) {

                string unreadLabel =
                    to_string(
                        unreadMessageCount
                    ) +
                    " new message" +
                    (
                        unreadMessageCount > 1
                        ? "s"
                        : ""
                        ) +
                    " ↓";

                float labelWidth =
                    ImGui::CalcTextSize(
                        unreadLabel.c_str()
                    ).x +
                    30.0f;

                ImGui::SetCursorPosX(
                    (
                        ImGui::GetWindowWidth() -
                        labelWidth
                        ) *
                    0.5f
                );

                if (ImGui::Button(
                    unreadLabel.c_str())) {

                    scrollToBottomRequested =
                        true;
                }
            }


            // INPUT AREA


            if (currentState ==
                CONNECTED) {

                if (remote_is_typing) {

                    string typingName =
                        peer_username.empty()
                        ? "Peer"
                        : peer_username;

                    ImGui::TextColored(
                        ImVec4(
                            0.40f,
                            0.80f,
                            0.50f,
                            1.00f
                        ),
                        "%s is typing...",
                        typingName.c_str()
                    );
                }


                // REPLY


                if (!replyTargetId.empty()) {

                    ImGui::TextColored(
                        ImVec4(
                            0.45f,
                            0.75f,
                            1.00f,
                            1.00f
                        ),
                        "Replying to %s: \"%s\"",
                        replyTargetName.c_str(),
                        replyTargetText.c_str()
                    );

                    ImGui::SameLine();

                    if (ImGui::SmallButton(
                        "Cancel")) {

                        replyTargetId = "";
                        replyTargetName = "";
                        replyTargetText = "";
                    }
                }

                // QUICK REPLIES


                ImGui::Text(
                    "Quick Replies:"
                );

                ImGui::SameLine();

                const char* quickReplies[] = {
                    "Hello!",
                    "I'm on my way!",
                    "Sounds good!",
                    "Call you later",
                    "Got it!",
                    "Thanks!"
                };

                for (int i = 0; i < 6; i++) {

                    if (i > 0)
                        ImGui::SameLine();

                    if (ImGui::SmallButton(
                        quickReplies[i])) {

                        safe_copy(
                            messageBuf,
                            sizeof(messageBuf),
                            quickReplies[i]
                        );
                    }
                }


                // QUICK EMOTES


                ImGui::Text(
                    "Quick Emotes:"
                );

                ImGui::SameLine();

                const char* emojis[] = {
                    "❤️",
                    "⭐",
                    "✨",
                    "✔",
                    "✖"
                };

                ImGui::PushStyleVar(
                    ImGuiStyleVar_FramePadding,
                    ImVec2(2, 2)
                );

                for (int i = 0; i < 5; i++) {

                    if (i > 0)
                        ImGui::SameLine();

                    if (ImGui::Button(
                        emojis[i],
                        ImVec2(32, 26))) {

                        if (
                            strlen(messageBuf) +
                            strlen(emojis[i]) <
                            sizeof(messageBuf)) {

                            strcat_s(
                                messageBuf,
                                sizeof(messageBuf),
                                emojis[i]
                            );
                        }
                    }
                }

                ImGui::PopStyleVar();

                // EMOJI PICKER


                ImGui::SameLine();

                if (ImGui::Button(
                    "Emoji",
                    ImVec2(54, 0))) {

                    ImGui::OpenPopup(
                        "EmojiPickerPopup"
                    );
                }

                if (ImGui::BeginPopup(
                    "EmojiPickerPopup")) {

                    ImGui::TextColored(
                        ImVec4(
                            0.45f,
                            0.75f,
                            1.00f,
                            1.0f
                        ),
                        "Choose Emoji"
                    );

                    ImGui::Separator();

                    const char* palette[] = {
                        "❤️", "⭐", "✨", "⚡",
                        "☀️", "☁️", "☔", "☕",
                        "✌️", "✋", "☝️", "✍️",
                        "✉️", "✈️", "⌛", "⏰",
                        "⚓", "⚙️", "⚠️", "⛔",
                        "❌", "❓", "❗", "🎵",
                        "⚽", "⚾", "⛄", "⛅",
                        "✔", "✖", "✳", "❇"
                    };

                    int totalEmojis =
                        IM_ARRAYSIZE(
                            palette
                        );

                    int cols = 8;

                    for (
                        int eIdx = 0;
                        eIdx < totalEmojis;
                        eIdx++) {

                        if (eIdx % cols != 0)
                            ImGui::SameLine();

                        ImGui::PushID(
                            eIdx
                        );

                        if (ImGui::Button(
                            palette[eIdx],
                            ImVec2(32, 28))) {

                            if (
                                strlen(messageBuf) +
                                strlen(palette[eIdx]) <
                                sizeof(messageBuf)) {

                                strcat_s(
                                    messageBuf,
                                    sizeof(messageBuf),
                                    palette[eIdx]
                                );
                            }
                        }

                        ImGui::PopID();
                    }

                    ImGui::EndPopup();
                }


                // MESSAGE INPUT


                ImGui::SameLine();

                ImGui::PushItemWidth(
                    -70
                );

                bool isInputChanged =
                    ImGui::InputText(
                        "##InputBox",
                        messageBuf,
                        IM_ARRAYSIZE(messageBuf),
                        ImGuiInputTextFlags_EnterReturnsTrue
                    );

                if (ImGui::IsItemActive()) {

                    auto now =
                        chrono::steady_clock::now();

                    if (
                        chrono::duration_cast<
                        chrono::milliseconds>(
                            now -
                            last_local_typing_sent
                        ).count() >
                        800) {

                        send_typing_status(
                            strlen(messageBuf) > 0
                        );

                        last_local_typing_sent =
                            now;
                    }
                }

                ImGui::PopItemWidth();

                ImGui::SameLine();

                if (
                    ImGui::Button(
                        "Send",
                        ImVec2(60, 0)
                    ) ||
                    isInputChanged) {

                    if (strlen(messageBuf) > 0) {

                        string display_name =
                            (strlen(username) > 0)
                            ? string(username)
                            : "Anonymous";

                        string ts =
                            get_current_time_str();

                        string new_msg_id =
                            generate_msg_id();

                        string safe_name =
                            sanitize_field(
                                display_name
                            );

                        string safe_r_name =
                            sanitize_field(
                                replyTargetName
                            );

                        string safe_r_text =
                            sanitize_field(
                                replyTargetText
                            );

                        string content =
                            string(messageBuf);

                        string packet =
                            "[MSG]|" +
                            new_msg_id +
                            "|" +
                            local_user_id +
                            "|" +
                            safe_name +
                            "|" +
                            local_role +
                            "|" +
                            ts +
                            "|" +
                            safe_r_name +
                            "|" +
                            safe_r_text +
                            "|" +
                            content;

                        send_raw_line(
                            packet
                        );

                        add_chat_log(
                            new_msg_id,
                            display_name,
                            local_user_id,
                            local_role,
                            ts,
                            replyTargetName,
                            replyTargetText,
                            content,
                            true
                        );

                        messageBuf[0] =
                            '\0';

                        replyTargetId =
                            "";

                        replyTargetName =
                            "";

                        replyTargetText =
                            "";

                        send_typing_status(
                            false
                        );

                        scrollToBottomRequested =
                            true;
                    }
                }
            }
            else if (
                currentState ==
                DISCONNECTED_NOTICE) {

                ImGui::TextColored(
                    ImVec4(
                        1.0f,
                        0.4f,
                        0.4f,
                        1.0f
                    ),
                    "Chat session ended or peer disconnected."
                );

                if (ImGui::Button(
                    "Return to Main Screen",
                    ImVec2(200, 36))) {

                    stop_network();
                }
            }
        }

        ImGui::End();

        // RENDER


        ImGui::Render();

        int display_w;
        int display_h;

        glfwGetFramebufferSize(
            window,
            &display_w,
            &display_h
        );

        glViewport(
            0,
            0,
            display_w,
            display_h
        );

        glClearColor(
            0.11f,
            0.13f,
            0.17f,
            1.0f
        );

        glClear(
            GL_COLOR_BUFFER_BIT
        );

        ImGui_ImplOpenGL3_RenderDrawData(
            ImGui::GetDrawData()
        );

        glfwSwapBuffers(
            window
        );
    }


    // SHUTDOWN


    stop_network();

    stop_discovery_service();

    ImGui_ImplOpenGL3_Shutdown();

    ImGui_ImplGlfw_Shutdown();

    ImGui::DestroyContext();

    glfwDestroyWindow(
        window
    );

    glfwTerminate();

    return 0;
}