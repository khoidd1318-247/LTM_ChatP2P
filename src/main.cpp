#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "Backend/P2PNode.h"
#include "Frontend/Win32GUI.h"
#include "Backend/ChatManager.h"
#include <memory>
#include <string>
#include <chrono>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    P2PNode node;
    Win32GUI gui;
    
    if (!gui.init(hInstance, nCmdShow)) {
        return 0;
    }

    std::unique_ptr<ChatManager> chatManager;
    auto lastTypingTime = std::chrono::steady_clock::now();

    auto setupChatManager = [&](const std::string& name) {
        if (!chatManager) {
            chatManager = std::make_unique<ChatManager>(&node, &gui, name);
            node.setOnMessageReceived([&](const std::string& data) {
                chatManager->onMessageReceived(data);
            });
            node.setOnPeerConnected([&]() {
                chatManager->onPeerConnected();
            });
            node.setOnPeerDisconnected([&]() {
                chatManager->onPeerDisconnected();
            });
        }
    };

    gui.setOnListen([&](const std::string& name, int port) {
        setupChatManager(name);
        if (node.startListening(port)) {
            gui.printSystemMessage("Đang lắng nghe kết nối trên cổng " + std::to_string(port) + "...");
        } else {
            gui.printSystemMessage("Không thể lắng nghe trên cổng " + std::to_string(port));
        }
    });

    gui.setOnConnect([&](const std::string& name, const std::string& ip, int port) {
        setupChatManager(name);
        if (node.connectToPeer(ip, port)) {
            gui.printSystemMessage("Đang kết nối tới " + ip + ":" + std::to_string(port) + "...");
        } else {
            gui.printSystemMessage("Kết nối thất bại!");
        }
    });

    gui.setOnSendMessage([&](const std::string& text) {
        if (!chatManager) return;
        
        if (text[0] == '/') {
            if (text == "/ping") chatManager->sendPing();
            else if (text.find("/file ") == 0) chatManager->startFileTransfer(text.substr(6));
            else if (text.find("/revoke ") == 0) chatManager->revokeMessage(text.substr(8));
            else if (text.find("/edit ") == 0) {
                size_t space = text.find(' ', 6);
                if (space != std::string::npos) {
                    chatManager->editMessage(text.substr(6, space - 6), text.substr(space + 1));
                }
            }
            else if (text.find("/autoreply ") == 0) chatManager->setAutoReply(text.substr(11));
            else if (text == "/busy") chatManager->setStatus(UserStatus::BUSY);
            else if (text == "/online") chatManager->setStatus(UserStatus::ONLINE);
            else if (text == "/theme") chatManager->toggleTheme();
            else if (text == "/help") {
                gui.printSystemMessage("Các lệnh hỗ trợ: /ping, /file <đường_dẫn>, /revoke <id_tin_nhắn>, /edit <id_tin_nhắn> <nội_dung>, /autoreply <nội_dung>, /busy (bận), /online (sẵn sàng), /theme (giao diện)");
            }
        } else {
            chatManager->sendTextMessage(text);
        }
    });

    gui.setOnTyping([&](bool isTyping) {
        if (chatManager) {
            chatManager->sendTypingIndicator(isTyping);
        }
    });

    gui.printSystemMessage("Chào mừng bạn đến với Ứng dụng Chat P2P. Hãy thiết lập Tên/Cổng và ấn Lắng nghe, hoặc nhập IP đối tác rồi ấn Kết nối.");
    
    gui.runMessageLoop();

    return 0;
}
