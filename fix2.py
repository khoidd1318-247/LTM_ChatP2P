with open("src/frontend/main.cpp", "r", encoding="utf-8", errors="ignore") as f:
    lines = f.readlines()

for i in range(len(lines)):
    if 'add_log(u8"[System] K' in lines[i] or 'K\ufffdt n\ufffdi th\ufffdt b' in lines[i]:
        lines[i] = '                        add_log(u8"[System] Kết nối thất bại: Tên bị trùng! Vui lòng đổi tên khác.");\n'
    elif 'string reject_msg = u8"REJECT' in lines[i]:
        lines[i] = '                        string reject_msg = u8"REJECT|Tên bị trùng!\\n";\n'
    elif 'đã tham gia cuộc trò chuyện' in lines[i] or '\ufffd th' in lines[i] or 'da tham gia' in lines[i] or 'tham gia cu' in lines[i]:
        if 'add_log' in lines[i] and 'System' in lines[i]:
            lines[i] = '                        add_log(string(u8"[System] ") + peer_name + u8" đã tham gia cuộc trò chuyện.");\n'
    elif 'Bị từ chối' in lines[i] or 'B\ufffd t\ufffd ch' in lines[i]:
        lines[i] = '                    add_log(string(u8"[System] Bị từ chối: ") + reason);\n'
    elif '[Đối tác]' in lines[i] or 'tAc' in lines[i] or 'i tA' in lines[i]:
        lines[i] = '                        add_log(string(u8"[Đối tác] ") + peer_name + ": " + msg_content);\n'
    elif 'Tham gia thất bại' in lines[i] or 'Tham gia tht bi' in lines[i] or 'Tham gia th\ufffdt b\ufffdi' in lines[i] or 'Tham gia' in lines[i] and 'System' in lines[i] and 'IP/Port' in lines[i]:
        lines[i] = '                add_log(string(u8"[System] Tham gia thất bại: Phòng không khả dụng hoặc sai IP/Port! (") + error.message() + ")");\n'
    elif '[Tôi]' in lines[i] or 'TA\'i' in lines[i] or 'T\ufffdi' in lines[i] or 'TA' in lines[i] and 'display_name' in lines[i]:
        lines[i] = '                            add_log(string(u8"[Tôi] ") + display_name + ": " + messageBuf);\n'

with open("src/frontend/main.cpp", "w", encoding="utf-8") as f:
    f.writelines(lines)
