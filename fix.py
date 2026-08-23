import re
with open("src/frontend/main.cpp", "r", encoding="utf-8", errors="ignore") as f:
    content = f.read()

# Replace corrupted strings
content = re.sub(r'u8"\[[^\]]+\] "\) \+ peer_name \+ u8" [^"]+"\.', 'u8"[System] ") + peer_name + u8" đã tham gia cuộc trò chuyện."', content)
content = re.sub(r'u8"\[[^\]]+\] "\) \+ peer_name \+ ": "', 'u8"[Đối tác] ") + peer_name + ": "', content)
content = re.sub(r'u8"\[[^\]]+\] "\) \+ display_name', 'u8"[Tôi] ") + display_name', content)
content = re.sub(r'u8"\[System\] Tham gia[^"]+"', 'u8"[System] Tham gia thất bại: Phòng không khả dụng hoặc sai IP/Port! ("', content)
content = re.sub(r'u8"\[System\] B[^"]+"', 'u8"[System] Bị từ chối: "', content)

with open("src/frontend/main.cpp", "w", encoding="utf-8") as f:
    f.write(content)
