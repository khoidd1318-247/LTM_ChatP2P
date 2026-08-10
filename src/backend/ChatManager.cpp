#include "ChatManager.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <thread>

ChatManager::ChatManager(P2PNode* node, IGUI* guiInterface, const std::string& name)
    : networkNode(node), ui(guiInterface), myName(name), myStatus(UserStatus::ONLINE), 
      isDarkTheme(false)
{
}

std::string ChatManager::generateId() {
    static int counter = 0;
    return std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + "_" + std::to_string(++counter);
}

void ChatManager::sendPacket(PacketType type, const std::string& id, const std::string& content) {
    Packet p{type, id, myName, content};
    networkNode->sendData(p.serialize());
}

std::string ChatManager::toHex(const std::vector<char>& bytes) {
    std::ostringstream oss;
    for (unsigned char b : bytes) {
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)b;
    }
    return oss.str();
}

std::vector<char> ChatManager::fromHex(const std::string& hex) {
    std::vector<char> bytes;
    for (size_t i = 0; i < hex.length(); i += 2) {
        std::string byteString = hex.substr(i, 2);
        char byte = (char)strtol(byteString.c_str(), NULL, 16);
        bytes.push_back(byte);
    }
    return bytes;
}

void ChatManager::onPeerConnected() {
    ui->addOrUpdateMessage("sys_connect", "", "Đã kết nối thành công với đối tác!", true);
    ui->updateStatus("Đã kết nối.");
    sendPacket(PacketType::STATUS_UPDATE, "0", "ONLINE");
}

void ChatManager::onPeerDisconnected() {
    ui->addOrUpdateMessage("sys_disconnect", "", "Đối tác đã ngắt kết nối.", true);
    ui->updateStatus("Chưa kết nối.");
}

void ChatManager::onMessageReceived(const std::string& rawData) {
    Packet p = Packet::deserialize(rawData);
    if (p.type == PacketType::UNKNOWN) return;

    switch (p.type) {
        case PacketType::TEXT: {
            ui->addOrUpdateMessage(p.id, p.sender, p.content, false);
            sendReceipt(p.id, false); // Gửi phản hồi DELIVERED
            
            if (myStatus == UserStatus::BUSY && !autoReplyMessage.empty()) {
                sendTextMessage("[Tự động phản hồi]: " + autoReplyMessage);
            }
            break;
        }
        case PacketType::TYPING: {
            if (p.content == "1") {
                ui->updateStatus(p.sender + " đang gõ...");
            } else {
                ui->updateStatus("Đã kết nối.");
            }
            break;
        }
        case PacketType::REVOKE: {
            ui->revokeMessage(p.id);
            ui->addOrUpdateMessage("sys_rev_" + p.id, "", p.sender + " đã thu hồi một tin nhắn.", true);
            break;
        }
        case PacketType::EDIT: {
            ui->addOrUpdateMessage(p.id, p.sender, p.content, false);
            break;
        }
        case PacketType::PING: {
            sendPacket(PacketType::PONG, p.id, "");
            break;
        }
        case PacketType::PONG: {
            auto now = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - pingSendTime);
            ui->addOrUpdateMessage("sys_ping_" + p.id, "", "Phản hồi Ping: " + std::to_string(duration.count()) + " ms", true);
            break;
        }
        case PacketType::STATUS_UPDATE: {
            std::string st = p.content;
            if (st == "ONLINE") st = "SẴN SÀNG";
            else if (st == "BUSY") st = "BẬN";
            else if (st == "AWAY") st = "ĐI VẮNG";
            
            ui->addOrUpdateMessage("sys_status_" + p.id, "", p.sender + " đã đổi trạng thái sang: " + st, true);
            break;
        }
        case PacketType::DELIVERED: {
            ui->addOrUpdateMessage("sys_deliv_" + p.id, "", "Tin nhắn đã gửi thành công.", true);
            break;
        }
        case PacketType::READ_RECEIPT: {
            ui->addOrUpdateMessage("sys_read_" + p.id, "", "Tin nhắn đối tác đã xem.", true);
            break;
        }
        case PacketType::FILE_REQ: {
            // Định dạng: content = filename|filesize
            size_t sep = p.content.find('|');
            if (sep == std::string::npos) break;
            std::string filename = p.content.substr(0, sep);
            size_t filesize = std::stoull(p.content.substr(sep + 1));
            
            IncomingFile infile;
            infile.filename = "received_" + filename;
            infile.size = filesize;
            infile.receivedBytes = 0;
            infile.fileStream.open(infile.filename, std::ios::binary);

            if (infile.fileStream.is_open()) {
                incomingFiles[p.id] = std::move(infile);
                ui->addOrUpdateMessage("sys_file_req_" + p.id, "", "Đang nhận file từ " + p.sender + ": " + filename + " (" + std::to_string(filesize) + " bytes)...", true);
            } else {
                ui->addOrUpdateMessage("sys_file_err_" + p.id, "", "Lỗi: Không thể ghi file nhận: " + filename, true);
            }
            break;
        }
        case PacketType::FILE_CHUNK: {
            auto it = incomingFiles.find(p.id);
            if (it == incomingFiles.end()) break;
            
            if (p.content == "END") {
                it->second.fileStream.close();
                ui->updateFileProgress(it->second.filename, it->second.size, it->second.size);
                ui->addOrUpdateMessage("sys_file_end_" + p.id, "", "Nhận file thành công! Lưu tại: " + it->second.filename, true);
                incomingFiles.erase(it);
            } else {
                std::vector<char> bytes = fromHex(p.content);
                it->second.fileStream.write(bytes.data(), bytes.size());
                it->second.receivedBytes += bytes.size();
                ui->updateFileProgress(it->second.filename, it->second.receivedBytes, it->second.size);
            }
            break;
        }
        default:
            break;
    }
}

void ChatManager::sendTextMessage(const std::string& content) {
    std::string id = generateId();
    sentMessages[id] = content;
    sendPacket(PacketType::TEXT, id, content);
    ui->addOrUpdateMessage(id, myName, content, false);
}

void ChatManager::sendPing() {
    pingSendTime = std::chrono::high_resolution_clock::now();
    sendPacket(PacketType::PING, generateId(), "");
    ui->addOrUpdateMessage("sys_ping_sent", "", "Đang gửi tín hiệu Ping đo độ trễ...", true);
}

void ChatManager::startFileTransfer(const std::string& path) {
    std::thread([this, path]() {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
            ui->addOrUpdateMessage("sys_file_err", "", "Lỗi: Không thể mở file " + path, true);
            return;
        }

        size_t lastSlash = path.find_last_of("/\\");
        std::string filename = (lastSlash == std::string::npos) ? path : path.substr(lastSlash + 1);

        file.seekg(0, std::ios::end);
        size_t filesize = file.tellg();
        file.seekg(0, std::ios::beg);

        std::string fileId = generateId();
        ui->addOrUpdateMessage("sys_file_send_" + fileId, "", "Đang gửi file: " + filename + " (" + std::to_string(filesize) + " bytes)...", true);

        sendPacket(PacketType::FILE_REQ, fileId, filename + "|" + std::to_string(filesize));

        const size_t chunkSize = 1024;
        std::vector<char> buffer(chunkSize);
        size_t bytesSent = 0;

        while (file) {
            file.read(buffer.data(), chunkSize);
            std::streamsize count = file.gcount();
            if (count <= 0) break;

            std::vector<char> actualData(buffer.begin(), buffer.begin() + count);
            std::string hexData = toHex(actualData);
            sendPacket(PacketType::FILE_CHUNK, fileId, hexData);
            
            bytesSent += count;
            ui->updateFileProgress(filename, bytesSent, filesize);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        sendPacket(PacketType::FILE_CHUNK, fileId, "END");
        ui->updateFileProgress(filename, filesize, filesize);
        ui->addOrUpdateMessage("sys_file_done_" + fileId, "", "Đã gửi file thành công: " + filename, true);
    }).detach();
}

void ChatManager::revokeMessage(const std::string& msgId) {
    sendPacket(PacketType::REVOKE, msgId, "");
    ui->revokeMessage(msgId);
    ui->addOrUpdateMessage("sys_rev_me_" + msgId, "", "Bạn đã thu hồi một tin nhắn.", true);
}

void ChatManager::editMessage(const std::string& msgId, const std::string& newContent) {
    sendPacket(PacketType::EDIT, msgId, newContent);
    ui->addOrUpdateMessage(msgId, myName, newContent, false);
}

void ChatManager::sendTypingIndicator(bool isTyping) {
    sendPacket(PacketType::TYPING, "0", isTyping ? "1" : "0");
}

void ChatManager::sendReceipt(const std::string& msgId, bool isRead) {
    sendPacket(isRead ? PacketType::READ_RECEIPT : PacketType::DELIVERED, msgId, "");
}

void ChatManager::setAutoReply(const std::string& msg) {
    autoReplyMessage = msg;
    ui->addOrUpdateMessage("sys_auto_reply", "", "Đã cài tin nhắn tự động: " + msg, true);
}

void ChatManager::setStatus(UserStatus status) {
    myStatus = status;
    std::string stStr = (status == UserStatus::ONLINE) ? "ONLINE" : (status == UserStatus::BUSY ? "BUSY" : "AWAY");
    sendPacket(PacketType::STATUS_UPDATE, "0", stStr);
    
    std::string stStrVi = (status == UserStatus::ONLINE) ? "SẴN SÀNG" : (status == UserStatus::BUSY ? "BẬN" : "ĐI VẮNG");
    ui->addOrUpdateMessage("sys_my_status", "", "Trạng thái của bạn hiện là: " + stStrVi, true);
}

void ChatManager::toggleTheme() {
    isDarkTheme = !isDarkTheme;
    ui->setTheme(isDarkTheme);
    ui->addOrUpdateMessage("sys_theme", "", std::string("Đã đổi giao diện sang ") + (isDarkTheme ? "Tối" : "Sáng") + ".", true);
}

void ChatManager::blockUser(const std::string& ip) {
    blockedIPs.push_back(ip);
    ui->addOrUpdateMessage("sys_block_" + ip, "", "Đã chặn địa chỉ IP: " + ip, true);
}
