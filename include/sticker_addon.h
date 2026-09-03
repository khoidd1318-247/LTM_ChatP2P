#pragma once

#include "imgui.h"
#include <cstring>
#include <cstddef>
#include <cmath>

namespace StickerAddon
{
    // ============================================================
    // THÊM MÃ STICKER VÀO Ô NHẬP TIN NHẮN
    // ============================================================
    inline void AppendSticker(
        char* messageBuffer,
        std::size_t capacity,
        const char* sticker)
    {
        if (!messageBuffer || !sticker || capacity == 0)
            return;

        std::size_t current =
            std::strlen(messageBuffer);

        std::size_t add =
            std::strlen(sticker);

        std::size_t space =
            (current > 0) ? 1 : 0;

        if (current + space + add + 1 >= capacity)
            return;

        if (current > 0)
        {
            messageBuffer[current] = ' ';
            current++;
            messageBuffer[current] = '\0';
        }

        std::memcpy(
            messageBuffer + current,
            sticker,
            add + 1);
    }


    // ============================================================
    // VẼ MẶT STICKER
    // ============================================================
    inline void DrawFace(
        ImDrawList* draw,
        ImVec2 center,
        float radius,
        int type)
    {
        // Mặt
        draw->AddCircleFilled(
            center,
            radius,
            IM_COL32(255, 218, 80, 255));

        // Viền
        draw->AddCircle(
            center,
            radius,
            IM_COL32(90, 80, 50, 255),
            32,
            2.0f);


        // --------------------------------------------------------
        // Mắt
        // --------------------------------------------------------
        if (type == 5)
        {
            // Nhắm mắt
            draw->AddLine(
                ImVec2(
                    center.x - radius * 0.50f,
                    center.y - radius * 0.20f),
                ImVec2(
                    center.x - radius * 0.15f,
                    center.y - radius * 0.05f),
                IM_COL32(50, 50, 50, 255),
                2.5f);

            draw->AddLine(
                ImVec2(
                    center.x + radius * 0.15f,
                    center.y - radius * 0.05f),
                ImVec2(
                    center.x + radius * 0.50f,
                    center.y - radius * 0.20f),
                IM_COL32(50, 50, 50, 255),
                2.5f);
        }
        else
        {
            draw->AddCircleFilled(
                ImVec2(
                    center.x - radius * 0.32f,
                    center.y - radius * 0.20f),
                radius * 0.09f,
                IM_COL32(45, 45, 45, 255));

            draw->AddCircleFilled(
                ImVec2(
                    center.x + radius * 0.32f,
                    center.y - radius * 0.20f),
                radius * 0.09f,
                IM_COL32(45, 45, 45, 255));
        }


        // ========================================================
        // TYPE 0 - CƯỜI
        // ========================================================
        if (type == 0)
        {
            draw->PathClear();

            draw->PathArcTo(
                ImVec2(
                    center.x,
                    center.y),
                radius * 0.42f,
                0.25f,
                2.90f,
                20);

            draw->PathStroke(
                IM_COL32(50, 50, 50, 255),
                false,
                3.0f);
        }


        // ========================================================
        // TYPE 1 - CƯỜI TO
        // ========================================================
        else if (type == 1)
        {
            draw->AddEllipseFilled(
                ImVec2(
                    center.x,
                    center.y + radius * 0.20f),
                ImVec2(
                    radius * 0.40f,
                    radius * 0.28f),
                IM_COL32(50, 50, 50, 255));
        }


        // ========================================================
        // TYPE 2 - BUỒN
        // ========================================================
        else if (type == 2)
        {
            draw->PathClear();

            draw->PathArcTo(
                ImVec2(
                    center.x,
                    center.y + radius * 0.55f),
                radius * 0.36f,
                3.35f,
                6.05f,
                20);

            draw->PathStroke(
                IM_COL32(50, 50, 50, 255),
                false,
                3.0f);

            // Nước mắt
            draw->AddCircleFilled(
                ImVec2(
                    center.x + radius * 0.48f,
                    center.y + radius * 0.20f),
                radius * 0.10f,
                IM_COL32(80, 170, 255, 255));
        }


        // ========================================================
        // TYPE 3 - KHÓC
        // ========================================================
        else if (type == 3)
        {
            draw->AddCircleFilled(
                ImVec2(
                    center.x - radius * 0.32f,
                    center.y + radius * 0.20f),
                radius * 0.11f,
                IM_COL32(80, 170, 255, 255));

            draw->AddCircleFilled(
                ImVec2(
                    center.x + radius * 0.32f,
                    center.y + radius * 0.20f),
                radius * 0.11f,
                IM_COL32(80, 170, 255, 255));

            draw->PathClear();

            draw->PathArcTo(
                ImVec2(
                    center.x,
                    center.y + radius * 0.45f),
                radius * 0.32f,
                3.30f,
                6.10f,
                20);

            draw->PathStroke(
                IM_COL32(50, 50, 50, 255),
                false,
                3.0f);
        }


        // ========================================================
        // TYPE 4 - TỨC GIẬN
        // ========================================================
        else if (type == 4)
        {
            draw->AddLine(
                ImVec2(
                    center.x - radius * 0.55f,
                    center.y - radius * 0.45f),
                ImVec2(
                    center.x - radius * 0.10f,
                    center.y - radius * 0.28f),
                IM_COL32(50, 50, 50, 255),
                3.0f);

            draw->AddLine(
                ImVec2(
                    center.x + radius * 0.10f,
                    center.y - radius * 0.28f),
                ImVec2(
                    center.x + radius * 0.55f,
                    center.y - radius * 0.45f),
                IM_COL32(50, 50, 50, 255),
                3.0f);

            draw->AddLine(
                ImVec2(
                    center.x - radius * 0.30f,
                    center.y + radius * 0.35f),
                ImVec2(
                    center.x + radius * 0.30f,
                    center.y + radius * 0.35f),
                IM_COL32(50, 50, 50, 255),
                3.0f);
        }


        // ========================================================
        // TYPE 5 - NHÁY MẮT
        // ========================================================
        else if (type == 5)
        {
            draw->PathClear();

            draw->PathArcTo(
                ImVec2(
                    center.x,
                    center.y + radius * 0.05f),
                radius * 0.35f,
                0.25f,
                2.90f,
                20);

            draw->PathStroke(
                IM_COL32(50, 50, 50, 255),
                false,
                3.0f);
        }


        // ========================================================
        // TYPE 6 - NGẠC NHIÊN
        // ========================================================
        else if (type == 6)
        {
            draw->AddCircleFilled(
                ImVec2(
                    center.x,
                    center.y + radius * 0.25f),
                radius * 0.18f,
                IM_COL32(50, 50, 50, 255));
        }


        // ========================================================
        // TYPE 7 - CƯỜI NHẸ
        // ========================================================
        else if (type == 7)
        {
            draw->AddLine(
                ImVec2(
                    center.x - radius * 0.28f,
                    center.y + radius * 0.20f),
                ImVec2(
                    center.x + radius * 0.28f,
                    center.y + radius * 0.20f),
                IM_COL32(50, 50, 50, 255),
                3.0f);
        }
    }


    // ============================================================
    // VẼ TRÁI TIM
    // ============================================================
    inline void DrawHeart(
        ImDrawList* draw,
        ImVec2 center,
        float size)
    {
        draw->AddCircleFilled(
            ImVec2(
                center.x - size * 0.30f,
                center.y - size * 0.10f),
            size * 0.32f,
            IM_COL32(255, 80, 110, 255));

        draw->AddCircleFilled(
            ImVec2(
                center.x + size * 0.30f,
                center.y - size * 0.10f),
            size * 0.32f,
            IM_COL32(255, 80, 110, 255));

        ImVec2 points[3];

        points[0] = ImVec2(
            center.x - size * 0.62f,
            center.y - size * 0.05f);

        points[1] = ImVec2(
            center.x + size * 0.62f,
            center.y - size * 0.05f);

        points[2] = ImVec2(
            center.x,
            center.y + size * 0.75f);

        draw->AddTriangleFilled(
            points[0],
            points[1],
            points[2],
            IM_COL32(255, 80, 110, 255));
    }


    // ============================================================
    // VẼ NGÔI SAO
    // ============================================================
    inline void DrawStar(
        ImDrawList* draw,
        ImVec2 center,
        float radius)
    {
        ImVec2 points[10];

        for (int i = 0; i < 10; ++i)
        {
            float angle =
                -3.14159265358979323846f * 0.5f +
                (float)i * 3.14159265358979323846f / 5.0f;
            float r =
                (i % 2 == 0)
                ? radius
                : radius * 0.45f;

            points[i] = ImVec2(
                center.x + std::cos(angle) * r,
                center.y + std::sin(angle) * r);
        }

        draw->AddConvexPolyFilled(
            points,
            10,
            IM_COL32(255, 215, 60, 255));
    }


    // ============================================================
    // VẼ THUMBS UP
    // ============================================================
    inline void DrawThumb(
        ImDrawList* draw,
        ImVec2 center,
        float size)
    {
        draw->AddRectFilled(
            ImVec2(
                center.x - size * 0.20f,
                center.y - size * 0.10f),
            ImVec2(
                center.x + size * 0.55f,
                center.y + size * 0.35f),
            IM_COL32(80, 190, 100, 255),
            5.0f);

        draw->AddRectFilled(
            ImVec2(
                center.x - size * 0.50f,
                center.y - size * 0.10f),
            ImVec2(
                center.x - size * 0.20f,
                center.y + size * 0.45f),
            IM_COL32(80, 190, 100, 255),
            4.0f);

        draw->AddRectFilled(
            ImVec2(
                center.x - size * 0.05f,
                center.y - size * 0.65f),
            ImVec2(
                center.x + size * 0.22f,
                center.y - size * 0.05f),
            IM_COL32(80, 190, 100, 255),
            4.0f);
    }


    // ============================================================
    // VẼ NGÔI SAO + TIM + MẶT...
    // ============================================================
    inline void DrawStickerGraphic(
        int type,
        ImVec2 pos,
        ImVec2 size)
    {
        ImDrawList* draw =
            ImGui::GetWindowDrawList();

        ImVec2 center(
            pos.x + size.x * 0.5f,
            pos.y + size.y * 0.5f);

        float radius =
            (size.x < size.y
                ? size.x
                : size.y) * 0.34f;


        if (type < 8)
        {
            DrawFace(
                draw,
                center,
                radius,
                type);
        }
        else if (type == 8)
        {
            DrawHeart(
                draw,
                center,
                radius * 1.55f);
        }
        else if (type == 9)
        {
            DrawStar(
                draw,
                center,
                radius * 1.25f);
        }
        else if (type == 10)
        {
            DrawThumb(
                draw,
                center,
                radius * 1.3f);
        }
        else if (type == 11)
        {
            // Xanh + check
            draw->AddCircleFilled(
                center,
                radius * 1.05f,
                IM_COL32(80, 190, 120, 255));

            draw->AddLine(
                ImVec2(
                    center.x - radius * 0.50f,
                    center.y),
                ImVec2(
                    center.x - radius * 0.10f,
                    center.y + radius * 0.40f),
                IM_COL32(255, 255, 255, 255),
                4.0f);

            draw->AddLine(
                ImVec2(
                    center.x - radius * 0.10f,
                    center.y + radius * 0.40f),
                ImVec2(
                    center.x + radius * 0.60f,
                    center.y - radius * 0.45f),
                IM_COL32(255, 255, 255, 255),
                4.0f);
        }
        else if (type == 12)
        {
            // X đỏ
            draw->AddCircleFilled(
                center,
                radius * 1.05f,
                IM_COL32(230, 80, 80, 255));

            draw->AddLine(
                ImVec2(
                    center.x - radius * 0.45f,
                    center.y - radius * 0.45f),
                ImVec2(
                    center.x + radius * 0.45f,
                    center.y + radius * 0.45f),
                IM_COL32(255, 255, 255, 255),
                4.0f);

            draw->AddLine(
                ImVec2(
                    center.x + radius * 0.45f,
                    center.y - radius * 0.45f),
                ImVec2(
                    center.x - radius * 0.45f,
                    center.y + radius * 0.45f),
                IM_COL32(255, 255, 255, 255),
                4.0f);
        }
        else if (type == 13)
        {
            // Bong bóng chat
            draw->AddRectFilled(
                ImVec2(
                    center.x - radius * 0.75f,
                    center.y - radius * 0.55f),
                ImVec2(
                    center.x + radius * 0.75f,
                    center.y + radius * 0.40f),
                IM_COL32(90, 160, 240, 255),
                8.0f);

            draw->AddTriangleFilled(
                ImVec2(
                    center.x - radius * 0.45f,
                    center.y + radius * 0.35f),
                ImVec2(
                    center.x - radius * 0.15f,
                    center.y + radius * 0.80f),
                ImVec2(
                    center.x + radius * 0.05f,
                    center.y + radius * 0.35f),
                IM_COL32(90, 160, 240, 255));
        }
        else if (type == 14)
        {
            // Tia chớp
            ImVec2 p[6];

            p[0] = ImVec2(
                center.x,
                center.y - radius);

            p[1] = ImVec2(
                center.x - radius * 0.35f,
                center.y);

            p[2] = ImVec2(
                center.x - radius * 0.05f,
                center.y);

            p[3] = ImVec2(
                center.x - radius * 0.30f,
                center.y + radius);

            p[4] = ImVec2(
                center.x + radius * 0.50f,
                center.y - radius * 0.10f);

            p[5] = ImVec2(
                center.x + radius * 0.10f,
                center.y - radius * 0.10f);

            draw->AddConvexPolyFilled(
                p,
                6,
                IM_COL32(255, 210, 50, 255));
        }
        else if (type == 15)
        {
            // Mặt xanh
            draw->AddCircleFilled(
                center,
                radius,
                IM_COL32(90, 190, 220, 255));

            draw->AddCircleFilled(
                ImVec2(
                    center.x - radius * 0.30f,
                    center.y - radius * 0.15f),
                radius * 0.08f,
                IM_COL32(40, 60, 70, 255));

            draw->AddCircleFilled(
                ImVec2(
                    center.x + radius * 0.30f,
                    center.y - radius * 0.15f),
                radius * 0.08f,
                IM_COL32(40, 60, 70, 255));

            draw->AddCircleFilled(
                ImVec2(
                    center.x,
                    center.y + radius * 0.30f),
                radius * 0.12f,
                IM_COL32(40, 60, 70, 255));
        }
        else if (type == 16)
        {
            // Mặt tím
            draw->AddCircleFilled(
                center,
                radius,
                IM_COL32(170, 110, 220, 255));

            draw->AddCircleFilled(
                ImVec2(
                    center.x - radius * 0.30f,
                    center.y - radius * 0.15f),
                radius * 0.09f,
                IM_COL32(50, 40, 60, 255));

            draw->AddCircleFilled(
                ImVec2(
                    center.x + radius * 0.30f,
                    center.y - radius * 0.15f),
                radius * 0.09f,
                IM_COL32(50, 40, 60, 255));

            draw->PathClear();

            draw->PathArcTo(
                ImVec2(
                    center.x,
                    center.y + radius * 0.20f),
                radius * 0.35f,
                0.20f,
                2.90f,
                20);

            draw->PathStroke(
                IM_COL32(50, 40, 60, 255),
                false,
                3.0f);
        }
        else if (type == 17)
        {
            // Tim nhỏ
            DrawHeart(
                draw,
                center,
                radius * 1.0f);
        }
        else if (type == 18)
        {
            // Sao nhỏ
            DrawStar(
                draw,
                center,
                radius * 0.90f);
        }
        else if (type == 19)
        {
            // Check lớn
            draw->AddCircleFilled(
                center,
                radius,
                IM_COL32(80, 180, 120, 255));

            draw->AddLine(
                ImVec2(
                    center.x - radius * 0.45f,
                    center.y),
                ImVec2(
                    center.x - radius * 0.10f,
                    center.y + radius * 0.35f),
                IM_COL32(255, 255, 255, 255),
                4.0f);

            draw->AddLine(
                ImVec2(
                    center.x - radius * 0.10f,
                    center.y + radius * 0.35f),
                ImVec2(
                    center.x + radius * 0.55f,
                    center.y - radius * 0.40f),
                IM_COL32(255, 255, 255, 255),
                4.0f);
        }
    }


    // ============================================================
    // NÚT STICKER
    // ============================================================
    inline void DrawStickerButton(
        int index,
        char* messageBuffer,
        std::size_t capacity)
    {
        ImVec2 size(
            62.0f,
            62.0f);

        ImVec2 pos =
            ImGui::GetCursorScreenPos();

        ImGui::PushID(index);

        // Nút invisible để bắt click
        bool clicked =
            ImGui::InvisibleButton(
                "##StickerButton",
                size);

        bool hovered =
            ImGui::IsItemHovered();

        ImGui::PopID();


        ImDrawList* draw =
            ImGui::GetWindowDrawList();


        // --------------------------------------------------------
        // Nền
        // --------------------------------------------------------
        ImU32 bg =
            hovered
            ? IM_COL32(90, 120, 235, 255)
            : IM_COL32(70, 100, 215, 255);

        draw->AddRectFilled(
            pos,
            ImVec2(
                pos.x + size.x,
                pos.y + size.y),
            bg,
            9.0f);


        // --------------------------------------------------------
        // Sticker
        // --------------------------------------------------------
        DrawStickerGraphic(
            index,
            pos,
            size);


        // --------------------------------------------------------
        // Click
        // --------------------------------------------------------
        if (clicked)
        {
            const char* codes[] =
            {
                "[STICKER_SMILE]",
                "[STICKER_LAUGH]",
                "[STICKER_SAD]",
                "[STICKER_CRY]",
                "[STICKER_ANGRY]",
                "[STICKER_WINK]",
                "[STICKER_SURPRISE]",
                "[STICKER_HAPPY]",
                "[STICKER_HEART]",
                "[STICKER_STAR]",
                "[STICKER_THUMB]",
                "[STICKER_OK]",
                "[STICKER_NO]",
                "[STICKER_CHAT]",
                "[STICKER_LIGHTNING]",
                "[STICKER_BLUE]",
                "[STICKER_PURPLE]",
                "[STICKER_HEART_SMALL]",
                "[STICKER_STAR_SMALL]",
                "[STICKER_CHECK]"
            };

            AppendSticker(
                messageBuffer,
                capacity,
                codes[index]);

            ImGui::CloseCurrentPopup();
        }
    }


    // ============================================================
    // STICKER PICKER
    // ============================================================
    inline void DrawStickerPicker(
        char* messageBuffer,
        std::size_t capacity)
    {
        // --------------------------------------------------------
        // Nút Sticker
        // --------------------------------------------------------
        if (ImGui::Button(
            "Sticker",
            ImVec2(70, 0)))
        {
            ImGui::OpenPopup(
                "StickerPickerPopup");
        }


        // --------------------------------------------------------
        // Kích thước popup
        // --------------------------------------------------------
        ImGui::SetNextWindowSize(
            ImVec2(350.0f, 340.0f),
            ImGuiCond_Appearing);


        if (!ImGui::BeginPopup(
            "StickerPickerPopup"))
        {
            return;
        }


        // --------------------------------------------------------
        // Tiêu đề
        // --------------------------------------------------------
        ImGui::TextColored(
            ImVec4(
                0.45f,
                0.75f,
                1.0f,
                1.0f),
            "Choose Sticker");


        ImGui::Separator();


        // --------------------------------------------------------
        // 4 sticker mỗi hàng
        // --------------------------------------------------------
        const int total = 20;
        const int columns = 4;


        for (int i = 0; i < total; ++i)
        {
            if (i % columns != 0)
                ImGui::SameLine();


            DrawStickerButton(
                i,
                messageBuffer,
                capacity);
        }


        ImGui::Separator();


        // --------------------------------------------------------
        // Nút Clear
        // --------------------------------------------------------
        if (ImGui::Button(
            "Clear",
            ImVec2(80, 0)))
        {
            if (messageBuffer &&
                capacity > 0)
            {
                messageBuffer[0] = '\0';
            }
        }


        ImGui::SameLine();


        // --------------------------------------------------------
        // Nút Close
        // --------------------------------------------------------
        if (ImGui::Button(
            "Close",
            ImVec2(80, 0)))
        {
            ImGui::CloseCurrentPopup();
        }


        ImGui::EndPopup();
    }
}
