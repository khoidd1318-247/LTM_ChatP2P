#include "Win32GUI.h"
#include <stdexcept>
#include <vector>
#include <chrono>
#include <commdlg.h>
#include <map>
#include <algorithm>

// Enable Windows Modern Controls Visual Styles (Common Controls V6)
#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

static std::map<HWND, bool> buttonHoverMap;
static WNDPROC g_oldButtonProc = NULL;

LRESULT CALLBACK ButtonSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_MOUSEMOVE: {
            if (!buttonHoverMap[hwnd]) {
                buttonHoverMap[hwnd] = true;
                TRACKMOUSEEVENT tme;
                tme.cbSize = sizeof(tme);
                tme.dwFlags = TME_LEAVE;
                tme.hwndTrack = hwnd;
                TrackMouseEvent(&tme);
                InvalidateRect(hwnd, NULL, FALSE);
            }
            break;
        }
        case WM_MOUSELEAVE: {
            buttonHoverMap[hwnd] = false;
            InvalidateRect(hwnd, NULL, FALSE);
            break;
        }
    }
    return CallWindowProc(g_oldButtonProc, hwnd, uMsg, wParam, lParam);
}

Win32GUI::Win32GUI() 
    : hwndLogin(NULL), hwndMain(NULL), isDarkMode(false), 
      hFont(NULL), hBoldFont(NULL), hLargeFont(NULL),
      fileProgress(0.0), fileProgressLabel("Không có truyền tải"), 
      connectionStatus("Chưa kết nối.") 
{
    uiMutex = CreateMutex(NULL, FALSE, NULL);
    
    // Modern Indigo/Slate Color Palette
    bgCol = RGB(248, 250, 252);        // Slate 50 background
    textCol = RGB(15, 23, 42);         // Slate 900 dark text
    panelCol = RGB(255, 255, 255);     // Pure White panels
    navBarCol = RGB(30, 41, 59);       // Slate 800 navigation bar
    progressBgCol = RGB(226, 232, 240); // Slate 200 progress bg
    progressFillCol = RGB(99, 102, 241); // Indigo progress fill
    editCol = RGB(255, 255, 255);       // Edit bg

    bgBrush = CreateSolidBrush(bgCol);
    panelBrush = CreateSolidBrush(panelCol);
    navBarBrush = CreateSolidBrush(navBarCol);
    progressBgBrush = CreateSolidBrush(progressBgCol);
    progressFillBrush = CreateSolidBrush(progressFillCol);
    editBrush = CreateSolidBrush(editCol);
}

Win32GUI::~Win32GUI() {
    CloseHandle(uiMutex);
    if (bgBrush) DeleteObject(bgBrush);
    if (panelBrush) DeleteObject(panelBrush);
    if (navBarBrush) DeleteObject(navBarBrush);
    if (progressBgBrush) DeleteObject(progressBgBrush);
    if (progressFillBrush) DeleteObject(progressFillBrush);
    if (editBrush) DeleteObject(editBrush);
    if (hFont) DeleteObject(hFont);
    if (hBoldFont) DeleteObject(hBoldFont);
    if (hLargeFont) DeleteObject(hLargeFont);
}

bool Win32GUI::init(HINSTANCE hInstance, int nCmdShow) {
    // 1. Register Window Classes
    WNDCLASS wcSetup = { };
    wcSetup.lpfnWndProc   = Win32GUI::SetupWindowProc;
    wcSetup.hInstance     = hInstance;
    wcSetup.lpszClassName = "SetupWindowClass";
    wcSetup.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wcSetup.hbrBackground = NULL;
    RegisterClass(&wcSetup);

    WNDCLASS wcChat = { };
    wcChat.lpfnWndProc   = Win32GUI::ChatWindowProc;
    wcChat.hInstance     = hInstance;
    wcChat.lpszClassName = "ChatWindowClass";
    wcChat.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wcChat.hbrBackground = NULL;
    RegisterClass(&wcChat);

    // 2. Create Fonts
    hFont = CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                       OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
    
    hBoldFont = CreateFont(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                       OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");

    hLargeFont = CreateFont(18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                       OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");

    // Load Rich Edit library
    LoadLibrary("Riched20.dll");

    // ==========================================
    // CỬA SỔ THIẾT LẬP KẾT NỐI (hwndLogin)
    // ==========================================
    hwndLogin = CreateWindowEx(
        0, "SetupWindowClass", "Thiết lập mạng P2P", 
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 360, 400,
        NULL, NULL, hInstance, this
    );

    if (hwndLogin == NULL) return false;

    hwndLoginTitle = CreateWindow("STATIC", "Mạng P2P Chat", WS_VISIBLE | WS_CHILD | SS_CENTER, 20, 25, 300, 30, hwndLogin, NULL, hInstance, NULL);
    hwndLoginNameLabel = CreateWindow("STATIC", "Tên hiển thị của bạn:", WS_VISIBLE | WS_CHILD, 30, 75, 280, 20, hwndLogin, NULL, hInstance, NULL);
    hwndLoginNameEdit = CreateWindow("EDIT", "Người dùng A", WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL, 30, 95, 280, 25, hwndLogin, (HMENU)IDC_NAMEEDIT, hInstance, NULL);

    hwndLoginPortLabel = CreateWindow("STATIC", "Cổng lắng nghe (Port):", WS_VISIBLE | WS_CHILD, 30, 130, 280, 20, hwndLogin, NULL, hInstance, NULL);
    hwndLoginPortEdit = CreateWindow("EDIT", "8080", WS_VISIBLE | WS_CHILD | ES_NUMBER, 30, 150, 280, 25, hwndLogin, (HMENU)IDC_PORTEDIT, hInstance, NULL);

    hwndLoginListenBtn = CreateWindow("BUTTON", "LẮNG NGHE KẾT NỐI", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 30, 185, 280, 35, hwndLogin, (HMENU)IDC_LISTENBTN, hInstance, NULL);

    hwndLoginIpLabel = CreateWindow("STATIC", "Nhập IP đối tác để kết nối:", WS_VISIBLE | WS_CHILD, 30, 235, 280, 20, hwndLogin, NULL, hInstance, NULL);
    hwndLoginIpEdit = CreateWindow("EDIT", "127.0.0.1", WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL, 30, 255, 280, 25, hwndLogin, (HMENU)IDC_IPEDIT, hInstance, NULL);

    hwndLoginConnectBtn = CreateWindow("BUTTON", "KẾT NỐI ĐỐI TÁC", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 30, 290, 280, 35, hwndLogin, (HMENU)IDC_CONNECTBTN, hInstance, NULL);

    hwndLoginStatus = CreateWindow("STATIC", "Chờ thiết lập...", WS_VISIBLE | WS_CHILD | SS_CENTER, 30, 335, 280, 20, hwndLogin, NULL, hInstance, NULL);

    // Apply margins (padding) to setup edits
    SendMessage(hwndLoginNameEdit, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELONG(8, 8));
    SendMessage(hwndLoginPortEdit, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELONG(8, 8));
    SendMessage(hwndLoginIpEdit, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELONG(8, 8));

    // Apply fonts to Login Window
    EnumChildWindows(hwndLogin, [](HWND hwnd, LPARAM lParam) -> BOOL {
        HFONT hF = (HFONT)lParam;
        SendMessage(hwnd, WM_SETFONT, (WPARAM)hF, TRUE);
        return TRUE;
    }, (LPARAM)hFont);
    SendMessage(hwndLoginTitle, WM_SETFONT, (WPARAM)hLargeFont, TRUE);

    // ==========================================
    // KHUNG CHAT CHÍNH (hwndMain)
    // ==========================================
    hwndMain = CreateWindowEx(
        0, "ChatWindowClass", "Ứng dụng Chat P2P", 
        WS_OVERLAPPEDWINDOW, 
        CW_USEDEFAULT, CW_USEDEFAULT, 980, 580,
        NULL, NULL, hInstance, this
    );

    if (hwndMain == NULL) return false;

    // Col 1: Navbar buttons
    hwndChatNavBtn = CreateWindow("BUTTON", "💬", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 12, 80, 40, 40, hwndMain, NULL, hInstance, NULL);
    hwndContactNavBtn = CreateWindow("BUTTON", "👥", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 12, 130, 40, 40, hwndMain, NULL, hInstance, NULL);
    hwndThemeBtn = CreateWindow("BUTTON", "⚙️", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 12, 490, 40, 40, hwndMain, (HMENU)IDC_THEMEBTN, hInstance, NULL);

    // Col 2: Chat List controls
    hwndSearchEdit = CreateWindow("EDIT", "Tìm kiếm...", WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL, 76, 45, 176, 25, hwndMain, NULL, hInstance, NULL);
    hwndChatListBox = CreateWindow("LISTBOX", "", WS_VISIBLE | WS_CHILD | LBS_NOTIFY | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS | WS_VSCROLL, 76, 80, 176, 455, hwndMain, (HMENU)IDC_CHATLISTBOX, hInstance, NULL);
    SendMessage(hwndChatListBox, LB_SETITEMHEIGHT, 0, 48);

    SendMessage(hwndSearchEdit, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELONG(8, 8));

    // Mock chat names
    SendMessage(hwndChatListBox, LB_ADDSTRING, 0, (LPARAM)"Trần Khánh Hiệp");
    SendMessage(hwndChatListBox, LB_ADDSTRING, 0, (LPARAM)"Việt Trường");
    SendMessage(hwndChatListBox, LB_ADDSTRING, 0, (LPARAM)"Quốc Bảo");
    SendMessage(hwndChatListBox, LB_ADDSTRING, 0, (LPARAM)"Phan Văn Lại");
    SendMessage(hwndChatListBox, LB_ADDSTRING, 0, (LPARAM)"Hoàng Quân");
    SendMessage(hwndChatListBox, LB_ADDSTRING, 0, (LPARAM)"Trò chuyện P2P (Chờ kết nối)");
    SendMessage(hwndChatListBox, LB_SETCURSEL, 5, 0);

    // Col 3: Chat Pane controls
    hwndChatBox = CreateWindow("RichEdit20A", "", WS_VISIBLE | WS_CHILD | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY, 
                                275, 70, 470, 350, hwndMain, (HMENU)IDC_CHATBOX, hInstance, NULL);
    SendMessage(hwndChatBox, EM_SETBKGNDCOLOR, 0, (LPARAM)bgCol);

    hwndFileBtn = CreateWindow("BUTTON", "Gửi File", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 275, 430, 90, 25, hwndMain, (HMENU)IDC_FILEBTN, hInstance, NULL);
    hwndPingBtn = CreateWindow("BUTTON", "Đo Ping", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 370, 430, 70, 25, hwndMain, (HMENU)IDC_PINGBTN, hInstance, NULL);

    hwndInputBox = CreateWindow("EDIT", "", WS_VISIBLE | WS_CHILD | ES_MULTILINE | ES_AUTOVSCROLL, 275, 465, 360, 60, hwndMain, (HMENU)IDC_INPUTBOX, hInstance, NULL);
    SendMessage(hwndInputBox, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELONG(8, 8));

    hwndSendBtn = CreateWindow("BUTTON", "Gửi", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 645, 465, 100, 60, hwndMain, (HMENU)IDC_SENDBTN, hInstance, NULL);

    // Apply fonts to Main Window
    EnumChildWindows(hwndMain, [](HWND hwnd, LPARAM lParam) -> BOOL {
        HFONT hF = (HFONT)lParam;
        SendMessage(hwnd, WM_SETFONT, (WPARAM)hF, TRUE);
        return TRUE;
    }, (LPARAM)hFont);

    // Subclass buttons to support hover animations
    g_oldButtonProc = (WNDPROC)SetWindowLongPtr(hwndLoginListenBtn, GWLP_WNDPROC, (LONG_PTR)ButtonSubclassProc);
    SetWindowLongPtr(hwndLoginConnectBtn, GWLP_WNDPROC, (LONG_PTR)ButtonSubclassProc);
    SetWindowLongPtr(hwndChatNavBtn, GWLP_WNDPROC, (LONG_PTR)ButtonSubclassProc);
    SetWindowLongPtr(hwndContactNavBtn, GWLP_WNDPROC, (LONG_PTR)ButtonSubclassProc);
    SetWindowLongPtr(hwndThemeBtn, GWLP_WNDPROC, (LONG_PTR)ButtonSubclassProc);
    SetWindowLongPtr(hwndFileBtn, GWLP_WNDPROC, (LONG_PTR)ButtonSubclassProc);
    SetWindowLongPtr(hwndPingBtn, GWLP_WNDPROC, (LONG_PTR)ButtonSubclassProc);
    SetWindowLongPtr(hwndSendBtn, GWLP_WNDPROC, (LONG_PTR)ButtonSubclassProc);

    // Show setup window initially
    ShowWindow(hwndLogin, nCmdShow);
    UpdateWindow(hwndLogin);

    return true;
}

void Win32GUI::runMessageLoop() {
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

LRESULT CALLBACK Win32GUI::SetupWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    Win32GUI* pThis = NULL;
    if (uMsg == WM_NCCREATE) {
        CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
        pThis = (Win32GUI*)pCreate->lpCreateParams;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
    } else {
        pThis = (Win32GUI*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    }
    if (pThis) return pThis->handleSetupMessage(hwnd, uMsg, wParam, lParam);
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK Win32GUI::ChatWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    Win32GUI* pThis = NULL;
    if (uMsg == WM_NCCREATE) {
        CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
        pThis = (Win32GUI*)pCreate->lpCreateParams;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
    } else {
        pThis = (Win32GUI*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    }
    if (pThis) return pThis->handleChatMessage(hwnd, uMsg, wParam, lParam);
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

LRESULT Win32GUI::handleSetupMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_ERASEBKGND:
            return 1;
        case WM_CTLCOLORDLG: {
            return (LRESULT)panelBrush;
        }
        case WM_CTLCOLORSTATIC: {
            HDC hdc = (HDC)wParam;
            HWND hwndCtrl = (HWND)lParam;
            if (hwndCtrl == hwndLoginTitle) {
                SetTextColor(hdc, progressFillCol);
            } else {
                SetTextColor(hdc, textCol);
            }
            SetBkColor(hdc, panelCol);
            return (LRESULT)panelBrush;
        }
        case WM_CTLCOLOREDIT: {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, textCol);
            SetBkColor(hdc, editCol);
            return (LRESULT)editBrush;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);
            
            // Fill background
            FillRect(hdc, &rc, panelBrush);

            // Draw border outlines around flat edits
            COLORREF borderCol = isDarkMode ? RGB(71, 85, 105) : RGB(203, 213, 225);
            HPEN hPen = CreatePen(PS_SOLID, 1, borderCol);
            HPEN oldPen = (HPEN)SelectObject(hdc, hPen);
            HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));

            // Name Edit Border
            RoundRect(hdc, 30 - 1, 95 - 1, 30 + 280 + 1, 95 + 25 + 1, 6, 6);
            // Port Edit Border
            RoundRect(hdc, 30 - 1, 150 - 1, 30 + 280 + 1, 150 + 25 + 1, 6, 6);
            // IP Edit Border
            RoundRect(hdc, 30 - 1, 255 - 1, 30 + 280 + 1, 255 + 25 + 1, 6, 6);

            SelectObject(hdc, oldBrush);
            SelectObject(hdc, oldPen);
            DeleteObject(hPen);

            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT lpDrawItem = (LPDRAWITEMSTRUCT)lParam;
            if (lpDrawItem->CtlType == ODT_BUTTON) {
                int btnId = GetDlgCtrlID(lpDrawItem->hwndItem);
                bool isPrimary = (btnId == IDC_LISTENBTN || btnId == IDC_CONNECTBTN || btnId == IDC_SENDBTN);

                COLORREF btnBg, btnText;
                bool isHovered = buttonHoverMap[lpDrawItem->hwndItem];
                bool isPressed = (lpDrawItem->itemState & ODS_SELECTED) != 0;
                bool isDisabled = (lpDrawItem->itemState & ODS_DISABLED) != 0;

                if (isPrimary) {
                    if (isDisabled) {
                        btnBg = isDarkMode ? RGB(71, 85, 105) : RGB(203, 213, 225);
                        btnText = isDarkMode ? RGB(148, 163, 184) : RGB(100, 116, 139);
                    } else if (isPressed) {
                        btnBg = isDarkMode ? RGB(79, 70, 229) : RGB(67, 56, 202);
                        btnText = RGB(255, 255, 255);
                    } else if (isHovered) {
                        btnBg = isDarkMode ? RGB(129, 140, 248) : RGB(79, 70, 229);
                        btnText = RGB(255, 255, 255);
                    } else {
                        btnBg = RGB(99, 102, 241);
                        btnText = RGB(255, 255, 255);
                    }
                } else {
                    if (isDisabled) {
                        btnBg = isDarkMode ? RGB(30, 41, 59) : RGB(241, 245, 249);
                        btnText = isDarkMode ? RGB(71, 85, 105) : RGB(203, 213, 225);
                    } else if (isPressed) {
                        btnBg = isDarkMode ? RGB(30, 41, 59) : RGB(203, 213, 225);
                        btnText = isDarkMode ? RGB(241, 245, 249) : RGB(51, 65, 85);
                    } else if (isHovered) {
                        btnBg = isDarkMode ? RGB(71, 85, 105) : RGB(226, 232, 240);
                        btnText = isDarkMode ? RGB(248, 250, 252) : RGB(15, 23, 42);
                    } else {
                        btnBg = isDarkMode ? RGB(51, 65, 85) : RGB(241, 245, 249);
                        btnText = isDarkMode ? RGB(226, 232, 240) : RGB(51, 65, 85);
                    }
                }

                HDC btnHdc = lpDrawItem->hDC;
                RECT btnRc = lpDrawItem->rcItem;

                HBRUSH hBrush = CreateSolidBrush(btnBg);
                HPEN hPen = CreatePen(PS_SOLID, 1, btnBg);
                HBRUSH oldB = (HBRUSH)SelectObject(btnHdc, hBrush);
                HPEN oldP = (HPEN)SelectObject(btnHdc, hPen);

                RoundRect(btnHdc, btnRc.left, btnRc.top, btnRc.right, btnRc.bottom, 8, 8);

                SelectObject(btnHdc, oldB);
                SelectObject(btnHdc, oldP);
                DeleteObject(hBrush);
                DeleteObject(hPen);

                char bTxt[256];
                GetWindowText(lpDrawItem->hwndItem, bTxt, sizeof(bTxt));
                SetTextColor(btnHdc, btnText);
                SetBkMode(btnHdc, TRANSPARENT);
                
                HFONT hBtnFont = (HFONT)SendMessage(lpDrawItem->hwndItem, WM_GETFONT, 0, 0);
                HFONT oldF = NULL;
                if (hBtnFont) oldF = (HFONT)SelectObject(btnHdc, hBtnFont);
                DrawText(btnHdc, bTxt, -1, &btnRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                if (oldF) SelectObject(btnHdc, oldF);
            }
            return TRUE;
        }
        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            switch (wmId) {
                case IDC_LISTENBTN: {
                    char nameBuf[256];
                    char portBuf[32];
                    GetWindowText(hwndLoginNameEdit, nameBuf, sizeof(nameBuf));
                    GetWindowText(hwndLoginPortEdit, portBuf, sizeof(portBuf));
                    myName = std::string(nameBuf);
                    if (onListen) onListen(myName, std::stoi(portBuf));
                    break;
                }
                case IDC_CONNECTBTN: {
                    char nameBuf[256];
                    char ipBuf[256];
                    char portBuf[32];
                    GetWindowText(hwndLoginNameEdit, nameBuf, sizeof(nameBuf));
                    GetWindowText(hwndLoginIpEdit, ipBuf, sizeof(ipBuf));
                    GetWindowText(hwndLoginPortEdit, portBuf, sizeof(portBuf));
                    myName = std::string(nameBuf);
                    if (onConnect) onConnect(myName, std::string(ipBuf), std::stoi(portBuf));
                    break;
                }
            }
            return 0;
        }
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

LRESULT Win32GUI::handleChatMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_ERASEBKGND:
            return 1;
        
        case WM_CTLCOLOREDIT: {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, textCol);
            SetBkColor(hdc, editCol);
            return (LRESULT)editBrush;
        }

        case WM_SIZE: {
            int w = LOWORD(lParam);
            int h = HIWORD(lParam);

            // Col 1 buttons
            MoveWindow(hwndChatNavBtn, 12, 80, 40, 40, TRUE);
            MoveWindow(hwndContactNavBtn, 12, 130, 40, 40, TRUE);
            MoveWindow(hwndThemeBtn, 12, h - 55, 40, 40, TRUE);

            // Col 2
            MoveWindow(hwndSearchEdit, 76, 45, 176, 25, TRUE);
            MoveWindow(hwndChatListBox, 76, 80, 176, h - 95, TRUE);

            // Col 3 Pane Layout
            int col3X = 264;
            int col3W = w - col3X - 220;
            if (col3W < 200) col3W = 200;

            // Chat Area
            MoveWindow(hwndChatBox, col3X + 11, 70, col3W - 22, h - 195, TRUE);
            
            // Rich Edit formatting rect
            RECT chatRect;
            chatRect.left = 12;
            chatRect.top = 12;
            chatRect.right = (col3W - 22) - 12;
            chatRect.bottom = (h - 195) - 12;
            SendMessage(hwndChatBox, EM_SETRECT, 0, (LPARAM)&chatRect);

            // File & Ping
            MoveWindow(hwndFileBtn, col3X + 11, h - 115, 90, 25, TRUE);
            MoveWindow(hwndPingBtn, col3X + 106, h - 115, 70, 25, TRUE);

            // Input / Send Box
            MoveWindow(hwndInputBox, col3X + 11, h - 80, col3W - 137, 60, TRUE);
            MoveWindow(hwndSendBtn, col3X + col3W - 116, h - 80, 105, 60, TRUE);

            InvalidateRect(hwnd, NULL, TRUE);
            return 0;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            RECT rc;
            GetClientRect(hwnd, &rc);
            int w = rc.right - rc.left;
            int h = rc.bottom - rc.top;
            int col3X = 264;

            // Offscreen Buffer
            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP memBitmap = CreateCompatibleBitmap(hdc, w, h);
            HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);

            // 1. Draw Panels Backgrounds
            // Col 1 (Navbar): Slate Navy/Slate 900
            RECT col1Rc = { 0, 0, 64, h };
            HBRUSH hCol1Brush = CreateSolidBrush(isDarkMode ? RGB(15, 23, 42) : RGB(30, 41, 59));
            FillRect(memDC, &col1Rc, hCol1Brush);
            DeleteObject(hCol1Brush);

            // Col 2 (Sidebar/Chat List): panelCol
            RECT col2Rc = { 64, 0, 264, h };
            HBRUSH hCol2Brush = CreateSolidBrush(panelCol);
            FillRect(memDC, &col2Rc, hCol2Brush);
            DeleteObject(hCol2Brush);

            // Col 4 (Info Sidebar): panelCol
            int col4X = w - 220;
            RECT col4Rc = { col4X, 0, w, h };
            HBRUSH hCol4Brush = CreateSolidBrush(panelCol);
            FillRect(memDC, &col4Rc, hCol4Brush);
            DeleteObject(hCol4Brush);

            // Col 3 (Chat Area): bgCol (or Slate 50 in light mode)
            RECT col3Rc = { 264, 0, col4X, h };
            HBRUSH hCol3Brush = CreateSolidBrush(isDarkMode ? bgCol : RGB(248, 250, 252));
            FillRect(memDC, &col3Rc, hCol3Brush);
            DeleteObject(hCol3Brush);

            // Header pane in Col 3
            RECT headerRc = { 264, 0, col4X, 60 };
            HBRUSH hHeaderBrush = CreateSolidBrush(panelCol);
            FillRect(memDC, &headerRc, hHeaderBrush);
            DeleteObject(hHeaderBrush);

            // 2. Draw Dividers (Modern subtle single-pixel lines)
            COLORREF dividerCol = isDarkMode ? RGB(51, 65, 85) : RGB(226, 232, 240);
            HPEN hDivPen = CreatePen(PS_SOLID, 1, dividerCol);
            HPEN oldPen = (HPEN)SelectObject(memDC, hDivPen);

            MoveToEx(memDC, 64, 0, NULL); LineTo(memDC, 64, h);
            MoveToEx(memDC, 264, 0, NULL); LineTo(memDC, 264, h);
            MoveToEx(memDC, 264, 60, NULL); LineTo(memDC, col4X, 60);
            MoveToEx(memDC, col4X, 0, NULL); LineTo(memDC, col4X, h);

            // Draw outlines around edit controls
            COLORREF borderCol = isDarkMode ? RGB(71, 85, 105) : RGB(203, 213, 225);
            HPEN hEditPen = CreatePen(PS_SOLID, 1, borderCol);
            SelectObject(memDC, hEditPen);
            HBRUSH oldBrush = (HBRUSH)SelectObject(memDC, GetStockObject(NULL_BRUSH));

            // Search Edit Box Border
            RoundRect(memDC, 76 - 1, 45 - 1, 76 + 176 + 1, 45 + 25 + 1, 6, 6);

            // Chat Input Box Border
            int col3W = col4X - 264;
            RoundRect(memDC, 264 + 11 - 1, h - 80 - 1, 264 + 11 + col3W - 137 + 1, h - 80 + 60 + 1, 8, 8);

            SelectObject(memDC, oldBrush);
            SelectObject(memDC, oldPen);
            DeleteObject(hDivPen);
            DeleteObject(hEditPen);

            // 3. Draw Avatar (Col 1)
            int avSize = 40;
            int avX = (64 - avSize) / 2;
            int avY = 15;
            COLORREF avBgCol = RGB(99, 102, 241);
            HBRUSH hAvBrush = CreateSolidBrush(avBgCol);
            HPEN hAvPen = CreatePen(PS_SOLID, 1, avBgCol);
            HBRUSH oldAvB = (HBRUSH)SelectObject(memDC, hAvBrush);
            HPEN oldAvP = (HPEN)SelectObject(memDC, hAvPen);
            Ellipse(memDC, avX, avY, avX + avSize, avY + avSize);
            SelectObject(memDC, oldAvB);
            SelectObject(memDC, oldAvP);
            DeleteObject(hAvBrush);
            DeleteObject(hAvPen);

            std::string initials = "ME";
            if (!myName.empty()) {
                initials = "";
                initials += myName[0];
                size_t lastSpace = myName.find_last_of(' ');
                if (lastSpace != std::string::npos && lastSpace + 1 < myName.length()) {
                    initials += myName[lastSpace + 1];
                }
            }
            SetTextColor(memDC, RGB(255, 255, 255));
            SetBkMode(memDC, TRANSPARENT);
            HFONT oldFont = (HFONT)SelectObject(memDC, hBoldFont);
            RECT avRc = { avX, avY, avX + avSize, avY + avSize };
            DrawText(memDC, initials.c_str(), -1, &avRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            // 4. Draw Col 2 Title
            SetTextColor(memDC, textCol);
            RECT listTitleRc = { 76, 15, 240, 40 };
            DrawText(memDC, "Tin nhắn", -1, &listTitleRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            // 5. Draw Col 3 Header Title
            std::string headerName = peerName.empty() ? "Trò chuyện P2P" : peerName;
            RECT headerTextRc = { 280, 0, col4X - 100, 60 };
            DrawText(memDC, headerName.c_str(), -1, &headerTextRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            // Status LED in header
            int dotSize = 8;
            int dotX = col4X - 30;
            int dotY = (60 - dotSize) / 2;
            COLORREF dotCol = RGB(156, 163, 175); // offline / unknown
            if (connectionStatus == "Đã kết nối.") {
                dotCol = RGB(34, 197, 94); // online green
            } else if (connectionStatus.find("lắng nghe") != std::string::npos) {
                dotCol = RGB(245, 158, 11); // listening orange
            }
            HBRUSH hDotBrush = CreateSolidBrush(dotCol);
            HPEN hDotPen = CreatePen(PS_SOLID, 1, dotCol);
            HBRUSH oldDotB = (HBRUSH)SelectObject(memDC, hDotBrush);
            HPEN oldDotP = (HPEN)SelectObject(memDC, hDotPen);
            Ellipse(memDC, dotX, dotY, dotX + dotSize, dotY + dotSize);
            SelectObject(memDC, oldDotB);
            SelectObject(memDC, oldDotP);
            DeleteObject(hDotBrush);
            DeleteObject(hDotPen);

            // 6. Draw Status label text
            SetTextColor(memDC, isDarkMode ? RGB(180, 180, 180) : RGB(100, 116, 139));
            SelectObject(memDC, hFont);
            RECT statusRc = { col3X + 185, h - 113, col4X - 15, h - 95 };
            DrawText(memDC, connectionStatus.c_str(), -1, &statusRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

            // 7. Draw Col 4 Content (Info Sidebar)
            // Title
            SetTextColor(memDC, textCol);
            SelectObject(memDC, hBoldFont);
            RECT infoTitleRc = { col4X + 16, 15, w - 16, 35 };
            DrawText(memDC, "Thông tin hội thoại", -1, &infoTitleRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            // Detail
            SelectObject(memDC, hFont);
            RECT infoDetailRc = { col4X + 16, 45, w - 16, 95 };
            std::string details = "Kênh trò chuyện P2P\nTrực tiếp ngang hàng\n(Không máy chủ)";
            DrawText(memDC, details.c_str(), -1, &infoDetailRc, DT_LEFT | DT_TOP);

            // Divider
            HPEN hSepPen = CreatePen(PS_SOLID, 1, dividerCol);
            SelectObject(memDC, hSepPen);
            MoveToEx(memDC, col4X + 16, 105, NULL); LineTo(memDC, w - 16, 105);

            // Progress Section Title
            SelectObject(memDC, hBoldFont);
            RECT storageTitleRc = { col4X + 16, 120, w - 16, 140 };
            DrawText(memDC, "Tiến độ truyền tải", -1, &storageTitleRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            // Progress Bar Bg
            int prgX = col4X + 16;
            int prgY = 145;
            int prgW = w - col4X - 32;
            int prgH = 10;
            RECT prgBgRc = { prgX, prgY, prgX + prgW, prgY + prgH };
            HBRUSH hPrgBgBrush = CreateSolidBrush(isDarkMode ? RGB(71, 85, 105) : RGB(226, 232, 240));
            FillRect(memDC, &prgBgRc, hPrgBgBrush);
            DeleteObject(hPrgBgBrush);

            // Progress Bar Fill
            int fillW = (int)(fileProgress * prgW);
            if (fillW > 0) {
                RECT prgFillRc = { prgX, prgY, prgX + fillW, prgY + prgH };
                HBRUSH hPrgFillBrush = CreateSolidBrush(RGB(99, 102, 241));
                FillRect(memDC, &prgFillRc, hPrgFillBrush);
                DeleteObject(hPrgFillBrush);
            }

            // Progress Label Text
            SelectObject(memDC, hFont);
            RECT prgLabelRc = { col4X + 16, 160, w - 16, 180 };
            DrawText(memDC, fileProgressLabel.c_str(), -1, &prgLabelRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

            // Divider
            MoveToEx(memDC, col4X + 16, 195, NULL); LineTo(memDC, w - 16, 195);

            // Shared Files List Title
            SelectObject(memDC, hBoldFont);
            RECT filesTitleRc = { col4X + 16, 210, w - 16, 230 };
            DrawText(memDC, "Danh sách File nhận/gửi", -1, &filesTitleRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            // List files
            SelectObject(memDC, hFont);
            int yOffset = 235;
            if (sharedFiles.empty()) {
                RECT emptyRc = { col4X + 16, yOffset, w - 16, yOffset + 100 };
                DrawText(memDC, "[Thư mục trống]", -1, &emptyRc, DT_LEFT | DT_TOP);
            } else {
                int index = 1;
                for (const auto& file : sharedFiles) {
                    if (yOffset + 20 > h - 10) break;
                    RECT fileRc = { col4X + 16, yOffset, w - 16, yOffset + 20 };
                    std::string fileText = std::to_string(index++) + ". " + file;
                    DrawText(memDC, fileText.c_str(), -1, &fileRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                    yOffset += 20;
                }
            }

            SelectObject(memDC, oldPen);
            DeleteObject(hSepPen);

            // Draw to screen
            BitBlt(hdc, 0, 0, w, h, memDC, 0, 0, SRCCOPY);

            // Cleanup
            SelectObject(memDC, oldBitmap);
            DeleteObject(memBitmap);
            DeleteDC(memDC);

            SelectObject(memDC, oldFont);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT lpDrawItem = (LPDRAWITEMSTRUCT)lParam;
            if (lpDrawItem->CtlType == ODT_LISTBOX) {
                HDC hdc = lpDrawItem->hDC;
                RECT rc = lpDrawItem->rcItem;
                HWND hwndList = lpDrawItem->hwndItem;

                if (lpDrawItem->itemID == -1) return TRUE;

                bool isSel = (lpDrawItem->itemState & ODS_SELECTED) != 0;
                
                COLORREF bgC, textC, subTextC;
                if (isSel) {
                    bgC = isDarkMode ? RGB(79, 70, 229) : RGB(99, 102, 241);
                    textC = RGB(255, 255, 255);
                    subTextC = RGB(224, 231, 255);
                } else {
                    bgC = isDarkMode ? RGB(30, 41, 59) : RGB(255, 255, 255);
                    textC = isDarkMode ? RGB(248, 250, 252) : RGB(15, 23, 42);
                    subTextC = isDarkMode ? RGB(148, 163, 184) : RGB(100, 116, 139);
                }

                HBRUSH hBr = CreateSolidBrush(bgC);
                FillRect(hdc, &rc, hBr);
                DeleteObject(hBr);

                char itemText[512];
                SendMessage(hwndList, LB_GETTEXT, lpDrawItem->itemID, (LPARAM)itemText);
                std::string textStr(itemText);

                std::string name = textStr;
                std::string lastMsg = "";
                size_t colon = textStr.find(": ");
                if (colon != std::string::npos) {
                    name = textStr.substr(0, colon);
                    lastMsg = textStr.substr(colon + 2);
                }

                int avS = 32;
                int avX = rc.left + 10;
                int avY = rc.top + (rc.bottom - rc.top - avS) / 2;
                
                COLORREF avBgCol = isSel ? (isDarkMode ? RGB(99, 102, 241) : RGB(79, 70, 229)) : RGB(99, 102, 241);
                HBRUSH hAvB = CreateSolidBrush(avBgCol);
                HPEN hAvP = CreatePen(PS_SOLID, 1, avBgCol);
                HBRUSH oldB = (HBRUSH)SelectObject(hdc, hAvB);
                HPEN oldP = (HPEN)SelectObject(hdc, hAvP);
                Ellipse(hdc, avX, avY, avX + avS, avY + avS);
                SelectObject(hdc, oldB);
                SelectObject(hdc, oldP);
                DeleteObject(hAvB);
                DeleteObject(hAvP);

                std::string initials = "";
                if (!name.empty()) {
                    initials += name[0];
                    size_t lastSpace = name.find_last_of(' ');
                    if (lastSpace != std::string::npos && lastSpace + 1 < name.length()) {
                        initials += name[lastSpace + 1];
                    }
                }
                SetTextColor(hdc, RGB(255, 255, 255));
                SetBkMode(hdc, TRANSPARENT);
                RECT avRc = { avX, avY, avX + avS, avY + avS };
                HFONT oldF = (HFONT)SelectObject(hdc, hBoldFont);
                DrawText(hdc, initials.c_str(), -1, &avRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                SelectObject(hdc, oldF);

                RECT textRc = { rc.left + 50, rc.top + 6, rc.right - 10, rc.bottom - 6 };
                SetTextColor(hdc, textC);
                HFONT hB = CreateFont(15, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                   OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
                oldF = (HFONT)SelectObject(hdc, hB);
                RECT nameRc = textRc;
                nameRc.bottom = nameRc.top + 16;
                DrawText(hdc, name.c_str(), -1, &nameRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                SelectObject(hdc, oldF);
                DeleteObject(hB);

                SetTextColor(hdc, subTextC);
                HFONT hR = CreateFont(13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                   OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
                oldF = (HFONT)SelectObject(hdc, hR);
                RECT msgRc = textRc;
                msgRc.top = msgRc.bottom - 16;
                std::string previewText = lastMsg.empty() ? "Sẵn sàng" : lastMsg;
                DrawText(hdc, previewText.c_str(), -1, &msgRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                SelectObject(hdc, oldF);
                DeleteObject(hR);

                // Online indicator
                if (name.find("P2P") != std::string::npos || name.find("trò chuyện") != std::string::npos || (!peerName.empty() && name.find(peerName) != std::string::npos)) {
                    int dotS = 8;
                    int dotX = avX + avS - dotS;
                    int dotY = avY + avS - dotS;
                    COLORREF dotC = (connectionStatus == "Đã kết nối.") ? RGB(34, 197, 94) : RGB(156, 163, 175);
                    HBRUSH hDotBrush = CreateSolidBrush(dotC);
                    HPEN hDotPen = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
                    oldB = (HBRUSH)SelectObject(hdc, hDotBrush);
                    oldP = (HPEN)SelectObject(hdc, hDotPen);
                    Ellipse(hdc, dotX, dotY, dotX + dotS, dotY + dotS);
                    SelectObject(hdc, oldB);
                    SelectObject(hdc, oldP);
                    DeleteObject(hDotBrush);
                    DeleteObject(hDotPen);
                }
            } else if (lpDrawItem->CtlType == ODT_BUTTON) {
                int btnId = GetDlgCtrlID(lpDrawItem->hwndItem);
                bool isPrimary = (btnId == IDC_LISTENBTN || btnId == IDC_CONNECTBTN || btnId == IDC_SENDBTN);

                COLORREF btnBg, btnText;
                bool isHovered = buttonHoverMap[lpDrawItem->hwndItem];
                bool isPressed = (lpDrawItem->itemState & ODS_SELECTED) != 0;
                bool isDisabled = (lpDrawItem->itemState & ODS_DISABLED) != 0;

                if (isPrimary) {
                    if (isDisabled) {
                        btnBg = isDarkMode ? RGB(71, 85, 105) : RGB(203, 213, 225);
                        btnText = isDarkMode ? RGB(148, 163, 184) : RGB(100, 116, 139);
                    } else if (isPressed) {
                        btnBg = isDarkMode ? RGB(79, 70, 229) : RGB(67, 56, 202);
                        btnText = RGB(255, 255, 255);
                    } else if (isHovered) {
                        btnBg = isDarkMode ? RGB(129, 140, 248) : RGB(79, 70, 229);
                        btnText = RGB(255, 255, 255);
                    } else {
                        btnBg = RGB(99, 102, 241);
                        btnText = RGB(255, 255, 255);
                    }
                } else {
                    if (isDisabled) {
                        btnBg = isDarkMode ? RGB(30, 41, 59) : RGB(241, 245, 249);
                        btnText = isDarkMode ? RGB(71, 85, 105) : RGB(203, 213, 225);
                    } else if (isPressed) {
                        btnBg = isDarkMode ? RGB(30, 41, 59) : RGB(203, 213, 225);
                        btnText = isDarkMode ? RGB(241, 245, 249) : RGB(51, 65, 85);
                    } else if (isHovered) {
                        btnBg = isDarkMode ? RGB(71, 85, 105) : RGB(226, 232, 240);
                        btnText = isDarkMode ? RGB(248, 250, 252) : RGB(15, 23, 42);
                    } else {
                        btnBg = isDarkMode ? RGB(51, 65, 85) : RGB(241, 245, 249);
                        btnText = isDarkMode ? RGB(226, 232, 240) : RGB(51, 65, 85);
                    }
                }

                HDC btnHdc = lpDrawItem->hDC;
                RECT btnRc = lpDrawItem->rcItem;

                HBRUSH hBrush = CreateSolidBrush(btnBg);
                HPEN hPen = CreatePen(PS_SOLID, 1, btnBg);
                HBRUSH oldB = (HBRUSH)SelectObject(btnHdc, hBrush);
                HPEN oldP = (HPEN)SelectObject(btnHdc, hPen);

                RoundRect(btnHdc, btnRc.left, btnRc.top, btnRc.right, btnRc.bottom, 8, 8);

                SelectObject(btnHdc, oldB);
                SelectObject(btnHdc, oldP);
                DeleteObject(hBrush);
                DeleteObject(hPen);

                char bTxt[256];
                GetWindowText(lpDrawItem->hwndItem, bTxt, sizeof(bTxt));
                SetTextColor(btnHdc, btnText);
                SetBkMode(btnHdc, TRANSPARENT);
                
                HFONT hBtnFont = (HFONT)SendMessage(lpDrawItem->hwndItem, WM_GETFONT, 0, 0);
                HFONT oldF = NULL;
                if (hBtnFont) oldF = (HFONT)SelectObject(btnHdc, hBtnFont);
                DrawText(btnHdc, bTxt, -1, &btnRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                if (oldF) SelectObject(btnHdc, oldF);
            }
            return TRUE;
        }

        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            int wmEvent = HIWORD(wParam);

            if (wmEvent == EN_CHANGE && wmId == IDC_INPUTBOX) {
                if (onTyping) onTyping(true);
            }

            switch (wmId) {
                case IDC_SENDBTN: {
                    char buf[2048];
                    GetWindowText(hwndInputBox, buf, sizeof(buf));
                    std::string text(buf);
                    if (!text.empty() && onSendMessage) {
                        onSendMessage(text);
                        SetWindowText(hwndInputBox, "");
                        if (onTyping) onTyping(false);
                    }
                    break;
                }
                case IDC_THEMEBTN: {
                    if (onSendMessage) onSendMessage("/theme");
                    break;
                }
                case IDC_PINGBTN: {
                    if (onSendMessage) onSendMessage("/ping");
                    break;
                }
                case IDC_FILEBTN: {
                    OPENFILENAME ofn;
                    char szFile[260] = { 0 };
                    ZeroMemory(&ofn, sizeof(ofn));
                    ofn.lStructSize = sizeof(ofn);
                    ofn.hwndOwner = hwnd;
                    ofn.lpstrFile = szFile;
                    ofn.nMaxFile = sizeof(szFile);
                    ofn.lpstrFilter = "All Files\0*.*\0";
                    ofn.nFilterIndex = 1;
                    ofn.lpstrFileTitle = NULL;
                    ofn.nMaxFileTitle = 0;
                    ofn.lpstrInitialDir = NULL;
                    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

                    if (GetOpenFileName(&ofn) == TRUE) {
                        if (onSendMessage) onSendMessage("/file " + std::string(ofn.lpstrFile));
                    }
                    break;
                }
            }
            return 0;
        }
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void Win32GUI::addOrUpdateMessage(const std::string& msgId, const std::string& sender, const std::string& msg, bool isSystem) {
    if (msgId == "sys_connect") {
        ShowWindow(hwndLogin, SW_HIDE);
        ShowWindow(hwndMain, SW_SHOW);
    } else if (msgId == "sys_disconnect") {
        ShowWindow(hwndMain, SW_HIDE);
        ShowWindow(hwndLogin, SW_SHOW);
    }

    WaitForSingleObject(uiMutex, INFINITE);
    bool found = false;
    for (auto& m : chatMessages) {
        if (m.id == msgId) {
            m.content = msg;
            found = true;
            break;
        }
    }
    if (!found) {
        chatMessages.push_back({msgId, sender, msg, isSystem});
    }

    // Capture peer name dynamically
    if (!sender.empty() && sender != myName) {
        if (peerName != sender) {
            peerName = sender;
            // Rename active conversation card
            SendMessage(hwndChatListBox, LB_DELETESTRING, 5, 0);
            SendMessage(hwndChatListBox, LB_INSERTSTRING, 5, (LPARAM)(peerName + ": Đang kết nối").c_str());
            SendMessage(hwndChatListBox, LB_SETCURSEL, 5, 0);
        }
    }
    
    // Update left preview card text
    if (!isSystem && !sender.empty()) {
        SendMessage(hwndChatListBox, LB_DELETESTRING, 5, 0);
        SendMessage(hwndChatListBox, LB_INSERTSTRING, 5, (LPARAM)(sender + ": " + (msg.length() > 15 ? msg.substr(0,12) + "..." : msg)).c_str());
        SendMessage(hwndChatListBox, LB_SETCURSEL, 5, 0);
    }

    ReleaseMutex(uiMutex);
    refreshChatBox();
    InvalidateRect(hwndMain, NULL, TRUE);
}

void Win32GUI::revokeMessage(const std::string& msgId) {
    WaitForSingleObject(uiMutex, INFINITE);
    for (auto& m : chatMessages) {
        if (m.id == msgId) {
            m.content = "[Tin nhắn đã thu hồi]";
            break;
        }
    }
    ReleaseMutex(uiMutex);
    refreshChatBox();
}

void Win32GUI::refreshChatBox() {
    WaitForSingleObject(uiMutex, INFINITE);
    
    // Clear RichEdit
    SetWindowText(hwndChatBox, "");

    for (const auto& m : chatMessages) {
        if (m.isSystem) {
            COLORREF sysCol = isDarkMode ? RGB(148, 163, 184) : RGB(100, 116, 139);
            std::string content = "— " + m.content + " —\n";
            appendRichText(hwndChatBox, content, sysCol, false, true, PFA_CENTER);
        } else {
            bool isMe = (m.sender == myName);
            COLORREF senderCol = isMe ? RGB(99, 102, 241) : (isDarkMode ? RGB(248, 250, 252) : RGB(15, 23, 42));
            COLORREF textColLocal = isDarkMode ? RGB(226, 232, 240) : RGB(51, 65, 85);
            WORD align = isMe ? PFA_RIGHT : PFA_LEFT;

            // Sender name (small, bold)
            std::string nameHeader = (isMe ? "Bạn" : m.sender) + "\n";
            COLORREF nameCol = isMe ? RGB(99, 102, 241) : (isDarkMode ? RGB(148, 163, 184) : RGB(100, 116, 139));
            appendRichText(hwndChatBox, nameHeader, nameCol, true, false, align);

            // Content body
            std::string content = m.content + "\n\n";
            appendRichText(hwndChatBox, content, textColLocal, false, false, align);
        }
    }

    // Scroll to bottom
    int len = GetWindowTextLength(hwndChatBox);
    SendMessage(hwndChatBox, EM_SETSEL, len, len);
    SendMessage(hwndChatBox, EM_SCROLLCARET, 0, 0);
    
    ReleaseMutex(uiMutex);
}

void Win32GUI::updateStatus(const std::string& status) {
    WaitForSingleObject(uiMutex, INFINITE);
    connectionStatus = status;
    ReleaseMutex(uiMutex);
    InvalidateRect(hwndMain, NULL, TRUE);
    SetWindowText(hwndLoginStatus, status.c_str());
}

void Win32GUI::setTheme(bool dark) {
    WaitForSingleObject(uiMutex, INFINITE);
    isDarkMode = dark;
    if (isDarkMode) {
        bgCol = RGB(15, 23, 42);          // Slate 900
        textCol = RGB(248, 250, 252);     // Slate 50
        panelCol = RGB(30, 41, 59);       // Slate 800
        navBarCol = RGB(15, 23, 42);      // Slate 950
        progressBgCol = RGB(71, 85, 105); // Slate 600
        progressFillCol = RGB(99, 102, 241); // Indigo
        editCol = RGB(51, 65, 85);        // Slate 700
    } else {
        bgCol = RGB(248, 250, 252);       // Slate 50
        textCol = RGB(15, 23, 42);        // Slate 900
        panelCol = RGB(255, 255, 255);    // White
        navBarCol = RGB(30, 41, 59);      // Slate 800
        progressBgCol = RGB(226, 232, 240); // Slate 200
        progressFillCol = RGB(99, 102, 241); // Indigo
        editCol = RGB(255, 255, 255);       // White
    }
    
    if (bgBrush) DeleteObject(bgBrush);
    if (panelBrush) DeleteObject(panelBrush);
    if (navBarBrush) DeleteObject(navBarBrush);
    if (progressBgBrush) DeleteObject(progressBgBrush);
    if (progressFillBrush) DeleteObject(progressFillBrush);
    if (editBrush) DeleteObject(editBrush);
    
    bgBrush = CreateSolidBrush(bgCol);
    panelBrush = CreateSolidBrush(panelCol);
    navBarBrush = CreateSolidBrush(navBarCol);
    progressBgBrush = CreateSolidBrush(progressBgCol);
    progressFillBrush = CreateSolidBrush(progressFillCol);
    editBrush = CreateSolidBrush(editCol);

    // Apply colors to RichEdit background
    if (hwndChatBox) {
        SendMessage(hwndChatBox, EM_SETBKGNDCOLOR, 0, (LPARAM)bgCol);
    }
    
    InvalidateRect(hwndLogin, NULL, TRUE);
    InvalidateRect(hwndMain, NULL, TRUE);
    if (hwndChatListBox) InvalidateRect(hwndChatListBox, NULL, TRUE);
    if (hwndInputBox) InvalidateRect(hwndInputBox, NULL, TRUE);
    
    ReleaseMutex(uiMutex);
}

void Win32GUI::printSystemMessage(const std::string& msg) {
    addOrUpdateMessage("sys_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()), "", msg, true);
    SetWindowText(hwndLoginStatus, msg.c_str());
}

void Win32GUI::updateFileProgress(const std::string& filename, size_t processed, size_t total) {
    WaitForSingleObject(uiMutex, INFINITE);
    if (total == 0) {
        fileProgress = 0.0;
        fileProgressLabel = "Không có truyền tải";
    } else {
        fileProgress = (double)processed / total;
        int percentage = (int)(fileProgress * 100.0);
        
        size_t lastSlash = filename.find_last_of("/\\");
        std::string nameOnly = (lastSlash == std::string::npos) ? filename : filename.substr(lastSlash + 1);

        if (processed == total) {
            fileProgressLabel = "Hoàn tất: " + nameOnly;
            fileProgress = 0.0;
            if (std::find(sharedFiles.begin(), sharedFiles.end(), nameOnly) == sharedFiles.end()) {
                sharedFiles.push_back(nameOnly);
            }
        } else {
            fileProgressLabel = "Truyền: " + nameOnly + " (" + std::to_string(percentage) + "%)";
        }
    }
    ReleaseMutex(uiMutex);
    InvalidateRect(hwndMain, NULL, TRUE);
}

void Win32GUI::appendRichText(HWND hwndRich, const std::string& text, COLORREF color, bool bold, bool italic, WORD alignment) {
    int len = GetWindowTextLength(hwndRich);
    SendMessage(hwndRich, EM_SETSEL, len, len);

    PARAFORMAT pf;
    ZeroMemory(&pf, sizeof(pf));
    pf.cbSize = sizeof(pf);
    pf.dwMask = PFM_ALIGNMENT;
    pf.wAlignment = alignment;
    SendMessage(hwndRich, EM_SETPARAFORMAT, 0, (LPARAM)&pf);

    CHARFORMAT cf;
    ZeroMemory(&cf, sizeof(cf));
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_COLOR | CFM_FACE | CFM_SIZE | CFM_BOLD | CFM_ITALIC;
    cf.dwEffects = 0;
    if (bold) cf.dwEffects |= CFE_BOLD;
    if (italic) cf.dwEffects |= CFE_ITALIC;
    cf.crTextColor = color;
    cf.yHeight = 220; // 11pt
    strcpy_s(cf.szFaceName, "Segoe UI");
    SendMessage(hwndRich, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);

    SendMessage(hwndRich, EM_REPLACESEL, FALSE, (LPARAM)text.c_str());
}
