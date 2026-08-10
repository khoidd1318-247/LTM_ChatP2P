#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <richedit.h>
#include <string>
#include <functional>
#include <vector>
#include "../Backend/ChatManager.h"

#define IDC_CHATBOX 101
#define IDC_INPUTBOX 102
#define IDC_SENDBTN 103
#define IDC_LISTENBTN 104
#define IDC_CONNECTBTN 105
#define IDC_PORTEDIT 106
#define IDC_IPEDIT 107
#define IDC_NAMEEDIT 108
#define IDC_STATUSLABEL 109
#define IDC_THEMEBTN 111
#define IDC_PINGBTN 113
#define IDC_FILEBTN 114
#define IDC_CHATLISTBOX 115

struct ChatMessage {
    std::string id;
    std::string sender;
    std::string content;
    bool isSystem;
};

class Win32GUI : public IGUI {
private:
    // Window Handles
    HWND hwndLogin;           // Connection Setup Window
    HWND hwndMain;            // Main Chat Window
    
    // Login Window Controls
    HWND hwndLoginTitle;
    HWND hwndLoginNameLabel;
    HWND hwndLoginPortLabel;
    HWND hwndLoginIpLabel;
    HWND hwndLoginNameEdit;
    HWND hwndLoginPortEdit;
    HWND hwndLoginIpEdit;
    HWND hwndLoginListenBtn;
    HWND hwndLoginConnectBtn;
    HWND hwndLoginStatus;

    // Col 1 Controls (Navbar)
    HWND hwndChatNavBtn;
    HWND hwndContactNavBtn;
    HWND hwndThemeBtn;
    
    // Col 2 Controls (Chat List)
    HWND hwndSearchEdit;
    HWND hwndChatListBox;
    
    // Col 3 Controls (Chat Pane)
    HWND hwndChatBox;
    HWND hwndInputBox;
    HWND hwndSendBtn;
    HWND hwndPingBtn;
    HWND hwndFileBtn;

    HFONT hFont;
    HFONT hBoldFont;
    HFONT hLargeFont;

    std::function<void(const std::string&)> onSendMessage;
    std::function<void(const std::string&, int)> onListen;
    std::function<void(const std::string&, const std::string&, int)> onConnect;
    std::function<void(bool)> onTyping;

    // Mutex for thread-safe UI updates
    HANDLE uiMutex;

    // Vector to store history for dynamic Edit/Revoke redraws
    std::vector<ChatMessage> chatMessages;

    // State properties for modern GDI rendering
    std::string myName;
    std::string peerName;
    double fileProgress;
    std::string fileProgressLabel;
    std::vector<std::string> sharedFiles;
    std::string connectionStatus;

    // Theme Customization properties
    bool isDarkMode;
    HBRUSH bgBrush;           // Chat Area Bg
    HBRUSH panelBrush;        // Top Header/List Bg
    HBRUSH navBarBrush;       // Col 1 Navy Bg
    HBRUSH progressBgBrush;   // Progress Bar Gray Bg
    HBRUSH progressFillBrush; // Progress Bar Blue Bg
    HBRUSH editBrush;         // Custom border/background for edit boxes
    
    COLORREF bgCol;
    COLORREF textCol;
    COLORREF panelCol;
    COLORREF navBarCol;
    COLORREF progressBgCol;
    COLORREF progressFillCol;
    COLORREF editCol;

public:
    Win32GUI();
    ~Win32GUI();

    bool init(HINSTANCE hInstance, int nCmdShow);
    void runMessageLoop();

    void setOnSendMessage(std::function<void(const std::string&)> cb) { onSendMessage = cb; }
    void setOnListen(std::function<void(const std::string&, int)> cb) { onListen = cb; }
    void setOnConnect(std::function<void(const std::string&, const std::string&, int)> cb) { onConnect = cb; }
    void setOnTyping(std::function<void(bool)> cb) { onTyping = cb; }

    // IGUI implementation
    void addOrUpdateMessage(const std::string& msgId, const std::string& sender, const std::string& msg, bool isSystem = false) override;
    void revokeMessage(const std::string& msgId) override;
    void updateStatus(const std::string& status) override;
    void setTheme(bool dark) override;
    void printSystemMessage(const std::string& msg) override;
    void updateFileProgress(const std::string& filename, size_t processed, size_t total) override;

    // Window Procedures
    static LRESULT CALLBACK SetupWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK ChatWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    LRESULT handleSetupMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT handleChatMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

private:
    void refreshChatBox();
    void appendRichText(HWND hwndRich, const std::string& text, COLORREF color, bool bold, bool italic, WORD alignment);
};
