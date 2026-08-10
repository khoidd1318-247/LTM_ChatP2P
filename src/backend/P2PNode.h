#pragma once
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <thread>
#include <functional>
#include <atomic>
#include <vector>

#pragma comment(lib, "ws2_32.lib")

class P2PNode {
private:
    SOCKET listenSocket;
    SOCKET peerSocket;
    std::thread listenThread;
    std::thread receiveThread;
    std::atomic<bool> isRunning;
    
    std::function<void(const std::string&)> onMessageReceived;
    std::function<void()> onPeerConnected;
    std::function<void()> onPeerDisconnected;

    void listenLoop(int port);
    void receiveLoop();

public:
    P2PNode();
    ~P2PNode();

    bool startListening(int port);
    bool connectToPeer(const std::string& ip, int port);
    bool sendData(const std::string& data);
    void stop();

    bool isConnected() const { return peerSocket != INVALID_SOCKET; }

    void setOnMessageReceived(std::function<void(const std::string&)> cb) { onMessageReceived = cb; }
    void setOnPeerConnected(std::function<void()> cb) { onPeerConnected = cb; }
    void setOnPeerDisconnected(std::function<void()> cb) { onPeerDisconnected = cb; }
};
