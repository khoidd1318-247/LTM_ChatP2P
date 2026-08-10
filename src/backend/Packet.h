#pragma once
#include <string>
#include <vector>
#include <sstream>

enum class PacketType {
    TEXT,
    FILE_REQ,
    FILE_CHUNK,
    REVOKE,
    EDIT,
    TYPING,
    DELIVERED,
    READ_RECEIPT,
    PING,
    PONG,
    STATUS_UPDATE,
    UNKNOWN
};

struct Packet {
    PacketType type;
    std::string id;
    std::string sender;
    std::string content;

    std::string serialize() const {
        std::string t = std::to_string(static_cast<int>(type));
        return t + "|" + id + "|" + sender + "|" + content;
    }

    static Packet deserialize(const std::string& data) {
        Packet p;
        p.type = PacketType::UNKNOWN;
        
        size_t pos1 = data.find('|');
        if (pos1 == std::string::npos) return p;
        
        size_t pos2 = data.find('|', pos1 + 1);
        if (pos2 == std::string::npos) return p;
        
        size_t pos3 = data.find('|', pos2 + 1);
        if (pos3 == std::string::npos) return p;

        int typeInt = std::stoi(data.substr(0, pos1));
        p.type = static_cast<PacketType>(typeInt);
        p.id = data.substr(pos1 + 1, pos2 - pos1 - 1);
        p.sender = data.substr(pos2 + 1, pos3 - pos2 - 1);
        p.content = data.substr(pos3 + 1);

        return p;
    }
};
