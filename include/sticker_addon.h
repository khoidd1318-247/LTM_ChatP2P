#pragma once
#include "imgui.h"
#include <cstring>
#include <cstddef>

namespace StickerAddon
{
    // ============================================================
    // ZALO-LIKE STICKER PICKER
    // ------------------------------------------------------------
    // Không dùng emoji font của Windows.
    // Sticker được vẽ trực tiếp bằng ImGui DrawList nên chắc chắn
    // nhìn thấy trên mọi máy có ImGui.
    // ============================================================

    inline void AppendSticker(char* buffer, std::size_t capacity, const char* text)
    {
        if (!buffer || !text || capacity == 0)
            return;

        const std::size_t current = std::strlen(buffer);
        const std::size_t add = std::strlen(text);

        if (current + add + 1 < capacity)
        {
            if (current > 0)
                std::strcat(buffer, " ");

            std::strcat(buffer, text);
        }
    }

    struct StickerItem
    {
        const char* sendText;
        const char* name;
        int type;
    };

    // 0 smile, 1 laugh, 2 love, 3 heart, 4 cry, 5 angry,
    // 6 cool, 7 hug, 8 wow, 9 thumbs, 10 clap, 11 pray,
    // 12 fire, 13 gift, 14 party, 15 sad, 16 wink, 17 kiss,
    // 18 shock, 19 sleepy, 20 happy, 21 ok, 22 strong, 23 thanks
    inline const StickerItem& GetSticker(int index)
    {
        static const StickerItem items[] =
        {
            {":)",       "Yêu quá",     2},
            {"XD",       "Mê luôn",     1},
            {":*",       "Hôn nè",     17},
            {"<3",       "Thương bạn",  3},

            {"=D",        "Cười xỉu",   1},
            {"haha",      "Haha",       1},
            {":D",        "Vui quá",    0},
            {":P",        "Trêu nè",    16},

            {"T_T",       "Năn nỉ đó",  15},
            {":'(",       "Buồn quá",   4},
            {":(",        "Huhu",       4},
            {">:(",       "Giận rồi",   5},

            {"B-)",       "Ngầu chưa",  6},
            {"(hug)",     "Ôm nè",      7},
            {"WOW!",      "Tuyệt vời",  8},
            {":-O",       "Ngạc nhiên", 18},

            {"OK",        "Ok luôn",    21},
            {"CLAP",      "Hay quá",    10},
            {"THANKS",    "Cảm ơn",     23},
            {"FIGHT!",    "Cố lên",     22},

            {"YAY!",      "Chúc mừng",  14},
            {"GIFT",      "Tặng bạn",   13},
            {"FIRE!",     "Đỉnh quá",   12},
            {"100%",      "Chuẩn luôn",  9}
        };

        return items[index % (int)(sizeof(items) / sizeof(items[0]))];
    }

    inline void DrawHeart(ImDrawList* draw, ImVec2 c, float s)
    {
        // Trái tim đơn giản bằng hai hình tròn + tam giác.
        draw->AddCircleFilled(ImVec2(c.x - s * 0.23f, c.y - s * 0.12f),
            s * 0.28f, IM_COL32(245, 75, 105, 255));
        draw->AddCircleFilled(ImVec2(c.x + s * 0.23f, c.y - s * 0.12f),
            s * 0.28f, IM_COL32(245, 75, 105, 255));
        draw->AddTriangleFilled(
            ImVec2(c.x - s * 0.52f, c.y - s * 0.02f),
            ImVec2(c.x + s * 0.52f, c.y - s * 0.02f),
            ImVec2(c.x, c.y + s * 0.62f),
            IM_COL32(245, 75, 105, 255));
    }

    inline void DrawFace(ImDrawList* draw, ImVec2 c, float r, int type)
    {
        const ImU32 face = IM_COL32(255, 218, 95, 255);
        const ImU32 outline = IM_COL32(80, 70, 55, 255);
        const ImU32 black = IM_COL32(45, 45, 50, 255);
        const ImU32 white = IM_COL32(255, 255, 255, 255);

        if (type == 2)
        {
            DrawHeart(draw, c, r * 1.55f);
            return;
        }

        if (type == 3)
        {
            DrawHeart(draw, c, r * 1.35f);
            draw->AddCircleFilled(
                ImVec2(c.x, c.y - r * 0.85f),
                r * 0.28f,
                IM_COL32(255, 255, 255, 220));
            return;
        }

        // Mặt tròn.
        draw->AddCircleFilled(c, r, face);
        draw->AddCircle(c, r, outline, 0, 2.0f);

        // Kiểu "cool".
        if (type == 6)
        {
            draw->AddRectFilled(
                ImVec2(c.x - r * 0.62f, c.y - r * 0.28f),
                ImVec2(c.x + r * 0.62f, c.y + r * 0.02f),
                black,
                4.0f);

            draw->AddLine(
                ImVec2(c.x - r * 0.58f, c.y - r * 0.22f),
                ImVec2(c.x - r * 0.08f, c.y - r * 0.22f),
                white, 2.0f);

            draw->AddLine(
                ImVec2(c.x + r * 0.08f, c.y - r * 0.22f),
                ImVec2(c.x + r * 0.58f, c.y - r * 0.22f),
                white, 2.0f);

            draw->AddBezierCubic(
                ImVec2(c.x - r * 0.35f, c.y + r * 0.28f),
                ImVec2(c.x - r * 0.05f, c.y + r * 0.55f),
                ImVec2(c.x + r * 0.05f, c.y + r * 0.55f),
                ImVec2(c.x + r * 0.35f, c.y + r * 0.28f),
                black, 2.5f);
            return;
        }

        // Mắt.
        if (type == 5)
        {
            draw->AddLine(ImVec2(c.x - r * 0.55f, c.y - r * 0.25f),
                ImVec2(c.x - r * 0.12f, c.y - r * 0.38f), black, 3.0f);
            draw->AddLine(ImVec2(c.x + r * 0.12f, c.y - r * 0.38f),
                ImVec2(c.x + r * 0.55f, c.y - r * 0.25f), black, 3.0f);
        }
        else if (type == 16)
        {
            draw->AddLine(ImVec2(c.x - r * 0.55f, c.y - r * 0.25f),
                ImVec2(c.x - r * 0.12f, c.y - r * 0.25f), black, 3.0f);
            draw->AddCircleFilled(ImVec2(c.x + r * 0.32f, c.y - r * 0.25f),
                r * 0.09f, black);
        }
        else
        {
            draw->AddCircleFilled(ImVec2(c.x - r * 0.34f, c.y - r * 0.25f),
                r * 0.09f, black);
            draw->AddCircleFilled(ImVec2(c.x + r * 0.34f, c.y - r * 0.25f),
                r * 0.09f, black);
        }

        // Miệng / biểu cảm.
        if (type == 4 || type == 15)
        {
            draw->AddCircleFilled(ImVec2(c.x, c.y + r * 0.32f),
                r * 0.12f, black);
            draw->AddLine(ImVec2(c.x - r * 0.48f, c.y + r * 0.08f),
                ImVec2(c.x - r * 0.60f, c.y + r * 0.35f),
                IM_COL32(70, 150, 255, 255), 2.0f);
            draw->AddLine(ImVec2(c.x + r * 0.48f, c.y + r * 0.08f),
                ImVec2(c.x + r * 0.60f, c.y + r * 0.35f),
                IM_COL32(70, 150, 255, 255), 2.0f);
        }
        else if (type == 8 || type == 18)
        {
            draw->AddCircleFilled(ImVec2(c.x, c.y + r * 0.25f),
                r * 0.18f, black);
        }
        else if (type == 1 || type == 0 || type == 20)
        {
            draw->AddBezierCubic(
                ImVec2(c.x - r * 0.35f, c.y + r * 0.20f),
                ImVec2(c.x - r * 0.10f, c.y + r * 0.52f),
                ImVec2(c.x + r * 0.10f, c.y + r * 0.52f),
                ImVec2(c.x + r * 0.35f, c.y + r * 0.20f),
                black, 2.5f);
        }
        else if (type == 6)
        {
            // Đã xử lý ở trên.
        }
        else
        {
            draw->AddLine(ImVec2(c.x - r * 0.28f, c.y + r * 0.32f),
                ImVec2(c.x + r * 0.28f, c.y + r * 0.32f),
                black, 2.5f);
        }

        // Một số hiệu ứng.
        if (type == 2 || type == 17)
        {
            DrawHeart(draw, ImVec2(c.x + r * 0.72f, c.y - r * 0.72f), r * 0.48f);
        }
        if (type == 12)
        {
            draw->AddTriangleFilled(
                ImVec2(c.x, c.y - r * 1.25f),
                ImVec2(c.x - r * 0.38f, c.y - r * 0.55f),
                ImVec2(c.x + r * 0.38f, c.y - r * 0.55f),
                IM_COL32(255, 110, 25, 255));
        }
    }

    inline void DrawSpecialSticker(ImDrawList* draw, ImVec2 c, float r, int type)
    {
        const ImU32 black = IM_COL32(45, 45, 50, 255);
        const ImU32 white = IM_COL32(255, 255, 255, 255);

        if (type == 3)
        {
            DrawHeart(draw, c, r * 1.25f);
            return;
        }

        if (type == 7)
        {
            // Hai khuôn mặt đang ôm nhau.
            DrawFace(draw, ImVec2(c.x - r * 0.42f, c.y), r * 0.72f, 0);
            DrawFace(draw, ImVec2(c.x + r * 0.42f, c.y), r * 0.72f, 2);
            return;
        }

        if (type == 9)
        {
            draw->AddCircleFilled(c, r, IM_COL32(255, 218, 95, 255));
            draw->AddCircle(c, r, black, 0, 2.0f);
            draw->AddLine(ImVec2(c.x - r * 0.35f, c.y - r * 0.20f),
                ImVec2(c.x - r * 0.35f, c.y + r * 0.08f), black, 3.0f);
            draw->AddLine(ImVec2(c.x + r * 0.35f, c.y - r * 0.20f),
                ImVec2(c.x + r * 0.35f, c.y + r * 0.08f), black, 3.0f);
            draw->AddLine(ImVec2(c.x - r * 0.38f, c.y + r * 0.34f),
                ImVec2(c.x + r * 0.38f, c.y + r * 0.34f), black, 3.0f);
            return;
        }

        if (type == 10)
        {
            // Hai bàn tay vỗ.
            draw->AddRectFilled(
                ImVec2(c.x - r * 0.65f, c.y - r * 0.30f),
                ImVec2(c.x - r * 0.05f, c.y + r * 0.65f),
                IM_COL32(255, 205, 145, 255), 8.0f);
            draw->AddRectFilled(
                ImVec2(c.x + r * 0.05f, c.y - r * 0.30f),
                ImVec2(c.x + r * 0.65f, c.y + r * 0.65f),
                IM_COL32(255, 205, 145, 255), 8.0f);
            draw->AddLine(ImVec2(c.x - r * 0.9f, c.y - r * 0.55f),
                ImVec2(c.x - r * 0.75f, c.y - r * 0.80f), white, 3.0f);
            draw->AddLine(ImVec2(c.x + r * 0.9f, c.y - r * 0.55f),
                ImVec2(c.x + r * 0.75f, c.y - r * 0.80f), white, 3.0f);
            return;
        }

        if (type == 11)
        {
            draw->AddCircleFilled(ImVec2(c.x - r * 0.25f, c.y),
                r * 0.48f, IM_COL32(255, 205, 145, 255));
            draw->AddCircleFilled(ImVec2(c.x + r * 0.25f, c.y),
                r * 0.48f, IM_COL32(255, 205, 145, 255));
            draw->AddLine(ImVec2(c.x - r * 0.10f, c.y - r * 0.42f),
                ImVec2(c.x - r * 0.10f, c.y + r * 0.42f), black, 2.0f);
            return;
        }

        if (type == 13)
        {
            draw->AddRectFilled(
                ImVec2(c.x - r * 0.60f, c.y - r * 0.48f),
                ImVec2(c.x + r * 0.60f, c.y + r * 0.55f),
                IM_COL32(240, 95, 125, 255), 6.0f);
            draw->AddLine(ImVec2(c.x, c.y - r * 0.48f),
                ImVec2(c.x, c.y + r * 0.55f), white, 3.0f);
            draw->AddLine(ImVec2(c.x - r * 0.60f, c.y - r * 0.05f),
                ImVec2(c.x + r * 0.60f, c.y - r * 0.05f), white, 3.0f);
            return;
        }

        if (type == 14)
        {
            draw->AddCircleFilled(c, r * 0.68f, IM_COL32(255, 215, 70, 255));
            draw->AddLine(ImVec2(c.x - r * 1.0f, c.y),
                ImVec2(c.x - r * 0.55f, c.y), IM_COL32(255, 190, 40, 255), 4.0f);
            draw->AddLine(ImVec2(c.x + r * 0.55f, c.y),
                ImVec2(c.x + r * 1.0f, c.y), IM_COL32(255, 190, 40, 255), 4.0f);
            draw->AddLine(ImVec2(c.x, c.y - r * 1.0f),
                ImVec2(c.x, c.y - r * 0.55f), IM_COL32(255, 190, 40, 255), 4.0f);
            draw->AddLine(ImVec2(c.x, c.y + r * 0.55f),
                ImVec2(c.x, c.y + r * 1.0f), IM_COL32(255, 190, 40, 255), 4.0f);
            return;
        }

        if (type == 12)
        {
            // Ngọn lửa.
            draw->AddTriangleFilled(
                ImVec2(c.x, c.y - r * 1.0f),
                ImVec2(c.x - r * 0.75f, c.y + r * 0.75f),
                ImVec2(c.x + r * 0.75f, c.y + r * 0.75f),
                IM_COL32(255, 105, 35, 255));
            draw->AddTriangleFilled(
                ImVec2(c.x, c.y - r * 0.45f),
                ImVec2(c.x - r * 0.35f, c.y + r * 0.55f),
                ImVec2(c.x + r * 0.35f, c.y + r * 0.55f),
                IM_COL32(255, 225, 80, 255));
            return;
        }

        // Mặc định dùng mặt.
        DrawFace(draw, c, r, type);
    }

    inline void DrawStickerButton(
        const StickerItem& sticker,
        int index,
        char* messageBuffer,
        std::size_t capacity)
    {
        ImGui::PushID(index);

        const ImVec2 cellSize(72.0f, 78.0f);
        const ImVec2 start = ImGui::GetCursorScreenPos();

        // Button trong suốt để không phụ thuộc font emoji.
        ImGui::InvisibleButton("##sticker", cellSize);

        const bool hovered = ImGui::IsItemHovered();
        const bool clicked = ImGui::IsItemClicked();

        ImDrawList* draw = ImGui::GetWindowDrawList();

        ImVec2 boxMin(start.x + 2.0f, start.y + 2.0f);
        ImVec2 boxMax(start.x + cellSize.x - 2.0f, start.y + 58.0f);

        if (hovered)
        {
            draw->AddRectFilled(
                boxMin, boxMax,
                IM_COL32(105, 130, 235, 255),
                9.0f);
        }
        else
        {
            draw->AddRectFilled(
                boxMin, boxMax,
                IM_COL32(78, 106, 220, 255),
                9.0f);
        }

        ImVec2 center(
            (boxMin.x + boxMax.x) * 0.5f,
            (boxMin.y + boxMax.y) * 0.5f - 1.0f);

        DrawSpecialSticker(draw, center, 20.0f, sticker.type);

        ImVec2 textPos(
            start.x + 2.0f,
            start.y + 61.0f);

        draw->AddText(
            textPos,
            IM_COL32(235, 235, 240, 255),
            sticker.name);

        if (clicked)
            AppendSticker(messageBuffer, capacity, sticker.sendText);

        ImGui::PopID();
    }

    inline void DrawStickerPicker(char* messageBuffer, std::size_t capacity)
    {
        if (ImGui::Button("Sticker", ImVec2(70.0f, 0.0f)))
            ImGui::OpenPopup("StickerPickerPopup");

        if (!ImGui::BeginPopup("StickerPickerPopup"))
            return;

        ImGui::TextUnformatted("Sticker");
        ImGui::Separator();

        const int columns = 4;
        const int total = 24;

        for (int i = 0; i < total; ++i)
        {
            if (i % columns != 0)
                ImGui::SameLine();

            DrawStickerButton(
                GetSticker(i),
                i,
                messageBuffer,
                capacity);
        }

        ImGui::Separator();

        if (ImGui::Button("Xóa nội dung", ImVec2(110.0f, 0.0f)))
        {
            if (messageBuffer && capacity > 0)
                messageBuffer[0] = '\0';
        }

        ImGui::SameLine();

        if (ImGui::Button("Đóng", ImVec2(80.0f, 0.0f)))
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }
}
