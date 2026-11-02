#include "DevEditor.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <iomanip>

DevEditor::DevEditor()
{
    memset(m_exportStatus, 0, sizeof(m_exportStatus));

    // ตัวอย่าง TextDraw ตั้งต้นให้ผู้ใช้เห็นทันทีที่เปิด
    DevTextDrawElement box;
    box.id = m_nextTdId++;
    box.type = 1; // Box
    snprintf(box.text, sizeof(box.text), "_");
    box.x = 180.0f;
    box.y = 120.0f;
    box.letterSizeX = 0.0f;
    box.letterSizeY = 0.0f;
    box.textSizeX = 460.0f;
    box.textSizeY = 220.0f;
    box.letterColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    box.boxColor = ImVec4(0.08f, 0.08f, 0.14f, 0.85f);
    box.backgroundColor = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
    box.font = 1;
    box.alignment = 1;
    box.useBox = true;
    box.proportional = true;
    box.shadow = 0;
    box.outline = 0;
    box.selectable = false;
    box.modelId = 0;
    m_textdraws.push_back(box);

    DevTextDrawElement title;
    title.id = m_nextTdId++;
    title.type = 0; // Text
    snprintf(title.text, sizeof(title.text), "SA-MP MOBILE DEV STUDIO");
    title.x = 200.0f;
    title.y = 135.0f;
    title.letterSizeX = 0.35f;
    title.letterSizeY = 1.4f;
    title.textSizeX = 440.0f;
    title.textSizeY = 20.0f;
    title.letterColor = ImVec4(0.0f, 1.0f, 0.5f, 1.0f); // Neon Green
    title.boxColor = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    title.backgroundColor = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
    title.font = 2; // Slim
    title.alignment = 1;
    title.useBox = false;
    title.proportional = true;
    title.shadow = 1;
    title.outline = 1;
    title.selectable = false;
    title.modelId = 0;
    m_textdraws.push_back(title);

    m_iSelectedTextDraw = 1; // เลือก Title เป็นค่าเริ่มต้น

    // ตัวอย่างโมเดลบ้าน Custom ID -1000 ที่เราเพิ่งแปลง
    DevMapObject obj;
    obj.id = m_nextObjId++;
    obj.modelId = -1000;
    obj.x = 2485.0f;
    obj.y = -1665.0f;
    obj.z = 13.5f;
    obj.rx = 0.0f;
    obj.ry = 0.0f;
    obj.rz = 0.0f;
    m_mapObjects.push_back(obj);
    m_iSelectedMapObject = 0;
}

void DevEditor::renderImGui()
{
    // เปิดเฉพาะเมื่อได้รับสิทธิ์ (Admin 129+) และเปิดผ่านคำสั่งเท่านั้น (ไม่มีปุ่มลอยบนหน้าจอ)
    if (!m_bVisible || !m_bAdminVerified) return;

    ImGuiIO& io = ImGui::GetIO();

    // แสดง Canvas Preview ทับหน้าจอเกมถ้าเปิดอยู่
    if (m_bShowCanvasPreview) {
        renderCanvasPreview();
    }

    // หน้าต่างหลักของ DevStudio
    ImVec2 mainSize = ImVec2(io.DisplaySize.x * 0.72f, io.DisplaySize.y * 0.85f);
    ImGui::SetNextWindowSize(mainSize, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.05f, io.DisplaySize.y * 0.08f), ImGuiCond_FirstUseEver);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.07f, 0.09f, 0.15f, 0.94f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.75f, 1.0f, 0.6f));
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(0.05f, 0.35f, 0.6f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);

    std::string title = "🛠️ SA-MP Mobile Developer Suite (ยศแอดมิน: " + std::to_string(m_iAdminLevel) + "+)";
    if (ImGui::Begin(title.c_str(), &m_bVisible)) {
        if (ImGui::BeginTabBar("DevTabs")) {
            if (ImGui::BeginTabItem("📐 TextDraw Studio (GUI)")) {
                renderTextDrawStudio();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("🏠 3D Map Studio")) {
                renderMapStudio();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("💾 Export Pawn & Files")) {
                renderSettingsAndExport();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(3);
}

void DevEditor::renderCanvasPreview()
{
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* drawList = ImGui::GetForegroundDrawList();

    // แปลงพิกัด SA-MP (640x480) เป็นพิกัดหน้าจอจริง
    float scaleX = io.DisplaySize.x / 640.0f;
    float scaleY = io.DisplaySize.y / 480.0f;

    for (size_t i = 0; i < m_textdraws.size(); i++) {
        const auto& td = m_textdraws[i];
        float scrX = td.x * scaleX;
        float scrY = td.y * scaleY;

        if (td.useBox) {
            float boxW = td.textSizeX * scaleX;
            float boxH = td.textSizeY * scaleY;
            ImU32 boxCol = ImGui::ColorConvertFloat4ToU32(td.boxColor);
            drawList->AddRectFilled(ImVec2(scrX, scrY), ImVec2(boxW, boxH), boxCol, 4.0f);
        }

        if (strlen(td.text) > 0 && strcmp(td.text, "_") != 0) {
            ImU32 txtCol = ImGui::ColorConvertFloat4ToU32(td.letterColor);
            drawList->AddText(ImVec2(scrX, scrY), txtCol, td.text);
        }

        // กรอบไฮไลต์ Element ที่กำลังเลือกอยู่
        if ((int)i == m_iSelectedTextDraw) {
            drawList->AddRect(ImVec2(scrX - 3, scrY - 3), ImVec2(scrX + 100, scrY + 25), IM_COL32(0, 255, 128, 255), 3.0f, 0, 2.0f);
        }
    }
}

void DevEditor::renderTextDrawStudio()
{
    ImGui::Checkbox("👁️ แสดง Live Preview บนหน้าจอเกม", &m_bShowCanvasPreview);
    ImGui::SameLine();
    if (ImGui::Button("➕ เพิ่ม Element ใหม่")) {
        DevTextDrawElement elem;
        elem.id = m_nextTdId++;
        elem.type = 0;
        snprintf(elem.text, sizeof(elem.text), "Text %d", elem.id);
        elem.x = 320.0f;
        elem.y = 240.0f;
        elem.letterSizeX = 0.35f;
        elem.letterSizeY = 1.4f;
        elem.textSizeX = 400.0f;
        elem.textSizeY = 20.0f;
        elem.letterColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        elem.boxColor = ImVec4(0.1f, 0.1f, 0.1f, 0.7f);
        elem.backgroundColor = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
        elem.font = 1;
        elem.alignment = 1;
        elem.useBox = false;
        elem.proportional = true;
        elem.shadow = 1;
        elem.outline = 1;
        elem.selectable = false;
        elem.modelId = 0;
        m_textdraws.push_back(elem);
        m_iSelectedTextDraw = (int)m_textdraws.size() - 1;
    }

    ImGui::Separator();

    // รายการ Elements (Layers)
    ImGui::BeginChild("TdList", ImVec2(180, 0), true);
    ImGui::Text("📑 รายการ Layers:");
    for (size_t i = 0; i < m_textdraws.size(); i++) {
        char label[64];
        snprintf(label, sizeof(label), "%s##%d", m_textdraws[i].text, (int)i);
        if (ImGui::Selectable(label, (int)i == m_iSelectedTextDraw)) {
            m_iSelectedTextDraw = (int)i;
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // ตัวปรับแต่ง Element ที่เลือก
    ImGui::BeginChild("TdProperties", ImVec2(0, 0), true);
    if (m_iSelectedTextDraw >= 0 && m_iSelectedTextDraw < (int)m_textdraws.size()) {
        auto& cur = m_textdraws[m_iSelectedTextDraw];
        ImGui::Text("✏️ แก้ไข Element: ID %d", cur.id);
        ImGui::InputText("ข้อความ (Text)", cur.text, sizeof(cur.text));

        ImGui::SliderFloat("พิกัด X (0-640)", &cur.x, 0.0f, 640.0f, "%.1f");
        ImGui::SliderFloat("พิกัด Y (0-480)", &cur.y, 0.0f, 480.0f, "%.1f");

        ImGui::SliderFloat("Letter Size X", &cur.letterSizeX, 0.0f, 2.0f, "%.2f");
        ImGui::SliderFloat("Letter Size Y", &cur.letterSizeY, 0.0f, 5.0f, "%.2f");

        ImGui::Checkbox("ใช้กล่องพื้นหลัง (UseBox)", &cur.useBox);
        if (cur.useBox) {
            ImGui::SliderFloat("ความกว้างกล่อง (TextSize X)", &cur.textSizeX, 0.0f, 640.0f, "%.1f");
            ImGui::SliderFloat("ความสูงกล่อง (TextSize Y)", &cur.textSizeY, 0.0f, 480.0f, "%.1f");
            ImGui::ColorEdit4("สีกรอบกล่อง (BoxColor)", (float*)&cur.boxColor);
        }

        ImGui::ColorEdit4("สีตัวหนังสือ (LetterColor)", (float*)&cur.letterColor);
        ImGui::SliderInt("แบบฟอนต์ (Font 0-3)", &cur.font, 0, 3);
        ImGui::SliderInt("เงา (Shadow)", &cur.shadow, 0, 5);
        ImGui::SliderInt("เส้นขอบ (Outline)", &cur.outline, 0, 5);
        ImGui::Checkbox("Proportional", &cur.proportional);
        ImGui::Checkbox("Selectable (กดได้)", &cur.selectable);

        if (ImGui::Button("🗑️ ลบ Element นี้")) {
            m_textdraws.erase(m_textdraws.begin() + m_iSelectedTextDraw);
            m_iSelectedTextDraw = -1;
        }
    } else {
        ImGui::TextDisabled("กรุณาเลือกหรือสร้าง Element จากรายการทางซ้าย");
    }
    ImGui::EndChild();
}

void DevEditor::renderMapStudio()
{
    ImGui::Text("🏠 จัดการโมเดล 3D และวัตถุในแมพ");
    ImGui::Separator();

    ImGui::InputInt("Model ID", &m_iInputModelId);
    ImGui::SameLine();
    if (ImGui::Button("➕ เสกวัตถุ")) {
        DevMapObject obj;
        obj.id = m_nextObjId++;
        obj.modelId = m_iInputModelId;
        obj.x = 2485.0f;
        obj.y = -1665.0f;
        obj.z = 13.5f;
        obj.rx = 0.0f;
        obj.ry = 0.0f;
        obj.rz = 0.0f;
        m_mapObjects.push_back(obj);
        m_iSelectedMapObject = (int)m_mapObjects.size() - 1;
    }

    // ตัวเลือกหมวดหมู่ด่วน
    ImGui::Text("หมวดหมู่โมเดลยอดนิยม:");
    if (ImGui::Button("บ้าน Custom (-1000)")) m_iInputModelId = -1000;
    ImGui::SameLine();
    if (ImGui::Button("อาคารหลัก (18631)")) m_iInputModelId = 18631;
    ImGui::SameLine();
    if (ImGui::Button("ถนน (19377)")) m_iInputModelId = 19377;
    ImGui::SameLine();
    if (ImGui::Button("ประตูกำแพง (980)")) m_iInputModelId = 980;

    ImGui::Separator();

    // Step Speed
    ImGui::Text("ระยะความเร็วในการขยับ (Step Size): %.2f เมตร", m_fCurrentStep);
    if (ImGui::Button("0.05m (ละเอียด)")) m_fCurrentStep = 0.05f;
    ImGui::SameLine();
    if (ImGui::Button("0.5m (ปกติ)")) m_fCurrentStep = 0.5f;
    ImGui::SameLine();
    if (ImGui::Button("2.0m (เร็ว)")) m_fCurrentStep = 2.0f;
    ImGui::SameLine();
    if (ImGui::Button("10.0m (เร็วมาก)")) m_fCurrentStep = 10.0f;

    ImGui::Separator();

    if (m_iSelectedMapObject >= 0 && m_iSelectedMapObject < (int)m_mapObjects.size()) {
        auto& cur = m_mapObjects[m_iSelectedMapObject];
        ImGui::Text("🎯 วัตถุที่กำลังแก้ไข: ID %d (Model: %d)", cur.id, cur.modelId);

        // แถวปุ่มปรับแกน X
        ImGui::Text("แกน X: %.3f", cur.x);
        ImGui::SameLine();
        if (ImGui::Button("X -##btn")) cur.x -= m_fCurrentStep;
        ImGui::SameLine();
        if (ImGui::Button("X +##btn")) cur.x += m_fCurrentStep;

        // แถวปุ่มปรับแกน Y
        ImGui::Text("แกน Y: %.3f", cur.y);
        ImGui::SameLine();
        if (ImGui::Button("Y -##btn")) cur.y -= m_fCurrentStep;
        ImGui::SameLine();
        if (ImGui::Button("Y +##btn")) cur.y += m_fCurrentStep;

        // แถวปุ่มปรับแกน Z
        ImGui::Text("แกน Z: %.3f", cur.z);
        ImGui::SameLine();
        if (ImGui::Button("Z -##btn")) cur.z -= m_fCurrentStep;
        ImGui::SameLine();
        if (ImGui::Button("Z +##btn")) cur.z += m_fCurrentStep;

        // การหมุนองศา
        ImGui::SliderFloat("หมุน RX", &cur.rx, 0.0f, 360.0f);
        ImGui::SliderFloat("หมุน RY", &cur.ry, 0.0f, 360.0f);
        ImGui::SliderFloat("หมุน RZ", &cur.rz, 0.0f, 360.0f);

        if (ImGui::Button("🗑️ ลบวัตถุนี้")) {
            m_mapObjects.erase(m_mapObjects.begin() + m_iSelectedMapObject);
            m_iSelectedMapObject = -1;
        }
    }
}

void DevEditor::renderSettingsAndExport()
{
    ImGui::Text("💾 ระบบ Export ไฟล์ Pawn ลงเครื่องมือถือ");
    ImGui::Separator();

    if (ImGui::Button("📥 Export TextDraw เป็นโค้ด Pawn (.pwn)", ImVec2(280, 40))) {
        if (exportTextDrawsToPawn("/sdcard/Download/exported_textdraw.pwn")) {
            snprintf(m_exportStatus, sizeof(m_exportStatus), "Export TextDraw สำเร็จ! บันทึกที่: /sdcard/Download/exported_textdraw.pwn");
        } else {
            snprintf(m_exportStatus, sizeof(m_exportStatus), "เกิดข้อผิดพลาดในการบันทึกไฟล์ TextDraw");
        }
    }

    ImGui::Spacing();

    if (ImGui::Button("📥 Export แมพ 3D เป็นโค้ด Pawn (.pwn)", ImVec2(280, 40))) {
        if (exportMapToPawn("/sdcard/Download/exported_map.pwn")) {
            snprintf(m_exportStatus, sizeof(m_exportStatus), "Export แมพสำเร็จ! บันทึกที่: /sdcard/Download/exported_map.pwn");
        } else {
            snprintf(m_exportStatus, sizeof(m_exportStatus), "เกิดข้อผิดพลาดในการบันทึกไฟล์แมพ");
        }
    }

    if (strlen(m_exportStatus) > 0) {
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.5f, 1.0f), "%s", m_exportStatus);
    }
}

bool DevEditor::exportTextDrawsToPawn(const std::string& filepath)
{
    std::ofstream out(filepath);
    if (!out.is_open()) return false;

    out << "// =========================================================\n";
    out << "// TextDraw GUI Exported by SA-MP Mobile DevStudio\n";
    out << "// =========================================================\n\n";

    for (size_t i = 0; i < m_textdraws.size(); i++) {
        out << "new Text:TD_Gui_" << i << ";\n";
    }

    out << "\npublic OnGameModeInit()\n{\n";

    for (size_t i = 0; i < m_textdraws.size(); i++) {
        const auto& td = m_textdraws[i];
        out << "    // Element " << i << ": " << td.text << "\n";
        out << "    TD_Gui_" << i << " = TextDrawCreate(" << std::fixed << std::setprecision(1) << td.x << ", " << td.y << ", \"" << td.text << "\");\n";
        out << "    TextDrawLetterSize(TD_Gui_" << i << ", " << std::setprecision(2) << td.letterSizeX << ", " << td.letterSizeY << ");\n";
        out << "    TextDrawFont(TD_Gui_" << i << ", " << td.font << ");\n";
        out << "    TextDrawSetShadow(TD_Gui_" << i << ", " << td.shadow << ");\n";
        out << "    TextDrawSetOutline(TD_Gui_" << i << ", " << td.outline << ");\n";

        // แปลง ImVec4 สีเป็น Hex RGBA
        uint32_t col = ((uint32_t)(td.letterColor.x * 255) << 24) |
                       ((uint32_t)(td.letterColor.y * 255) << 16) |
                       ((uint32_t)(td.letterColor.z * 255) << 8)  |
                       ((uint32_t)(td.letterColor.w * 255));
        out << "    TextDrawColor(TD_Gui_" << i << ", 0x" << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << col << std::dec << ");\n";

        if (td.useBox) {
            out << "    TextDrawUseBox(TD_Gui_" << i << ", 1);\n";
            out << "    TextDrawTextSize(TD_Gui_" << i << ", " << std::fixed << std::setprecision(1) << td.textSizeX << ", " << td.textSizeY << ");\n";
            uint32_t boxCol = ((uint32_t)(td.boxColor.x * 255) << 24) |
                              ((uint32_t)(td.boxColor.y * 255) << 16) |
                              ((uint32_t)(td.boxColor.z * 255) << 8)  |
                              ((uint32_t)(td.boxColor.w * 255));
            out << "    TextDrawBoxColor(TD_Gui_" << i << ", 0x" << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << boxCol << std::dec << ");\n";
        }

        if (td.proportional) {
            out << "    TextDrawSetProportional(TD_Gui_" << i << ", 1);\n";
        }
        if (td.selectable) {
            out << "    TextDrawSetSelectable(TD_Gui_" << i << ", 1);\n";
        }
        out << "\n";
    }

    out << "    return 1;\n}\n";
    return true;
}

bool DevEditor::exportMapToPawn(const std::string& filepath)
{
    std::ofstream out(filepath);
    if (!out.is_open()) return false;

    out << "// =========================================================\n";
    out << "// 3D Map Objects Exported by SA-MP Mobile DevStudio\n";
    out << "// =========================================================\n\n";
    out << "public OnGameModeInit()\n{\n";

    for (size_t i = 0; i < m_mapObjects.size(); i++) {
        const auto& obj = m_mapObjects[i];
        out << "    CreateDynamicObject(" << obj.modelId << ", " 
            << std::fixed << std::setprecision(3) 
            << obj.x << ", " << obj.y << ", " << obj.z << ", "
            << obj.rx << ", " << obj.ry << ", " << obj.rz << ", -1, -1, -1, 300.0);\n";
    }

    out << "    return 1;\n}\n";
    return true;
}
