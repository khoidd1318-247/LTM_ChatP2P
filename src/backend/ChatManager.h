#pragma once
#include "P2PNode.h"
#include "Packet.h"
#include "../Frontend/IGUI.h"
#include <string>
#include <map>
#include <vector>
#include <chrono>
#include <fstream>

enum class UserStatus { ONLINE, BUSY, AWAY };

struct IncomingFile {
    std::string filename;
    size_t size;
    size_t receivedBytes;
    std::ofstream fileStream;
};

class ChatManager {
private:
    P2PNode* networkNode;
    IGUI* ui;
    std::string myName;
    UserStatus myStatus;
    
    std::string autoReplyMessage;
    std::vector<std::string> blockedIPs;
    
    std::map<std::string, std::string> sentMessages; 
    std::map<std::string, IncomingFile> incomingFiles;

    bool isDarkTheme;
    
    std::chrono::high_resolution_clock::time_point pingSendTime;

public:
    ChatManager(P2PNode* node, IGUI* guiInterface, const std::string& name);
    
    void onMessageReceived(const std::string& rawData);
    void onPeerConnected();
    void onPeerDisconnected();

    // Core messaging
    void sendTextMessage(const std::string& content);
    
    // Features
    void sendPing();                            
    void startFileTransfer(const std::string& path); 
    void revokeMessage(const std::string& msgId);    
    void editMessage(const std::string& msgId, const std::string& newContent); 
    void sendTypingIndicator(bool isTyping);         
    void sendReceipt(const std::string& msgId, bool isRead); 
    
    void setAutoReply(const std::string& msg);       
    void setStatus(UserStatus status);               
    void blockUser(const std::string& ip);           
    void toggleTheme();           

private:
    std::string generateId();
    void sendPacket(PacketType type, const std::string& id, const std::string& content);
    std::string toHex(const std::vector<char>& bytes);
    std::vector<char> fromHex(const std::string& hex);
};
