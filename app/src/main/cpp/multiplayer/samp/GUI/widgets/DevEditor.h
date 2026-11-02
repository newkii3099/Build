#pragma once

#include <vector>
#include <string>
#include "imgui.h"

// โครงสร้างข้อมูล TextDraw Element
struct DevTextDrawElement {
    int id;
    int type; // 0 = Text, 1 = Box, 2 = Model Preview
    char text[128];
    float x; // พิกัด SA-MP (0.0 - 640.0)
    float y; // พิกัด SA-MP (0.0 - 480.0)
    float letterSizeX;
    float letterSizeY;
    float textSizeX;
    float textSizeY;
    ImVec4 letterColor;
    ImVec4 boxColor;
    ImVec4 backgroundColor;
    int font;
    int alignment; // 1 = Left, 2 = Center, 3 = Right
    bool useBox;
    bool proportional;
    int shadow;
    int outline;
    bool selectable;
    int modelId;
};

// โครงสร้างข้อมูล 3D Map Object
struct DevMapObject {
    int id;
    int modelId;
    float x;
    float y;
    float z;
    float rx;
    float ry;
    float rz;
};

class DevEditor {
public:
    DevEditor();
    ~DevEditor() = default;

    void renderImGui();
    void toggle() {
        if (!m_bAdminVerified) {
            m_bVisible = false;
            return;
        }
        m_bVisible = !m_bVisible;
    }
    bool visible() const { return m_bVisible; }
    void setVisible(bool visible) { 
        if (!m_bAdminVerified && visible) return;
        m_bVisible = visible; 
    }
    void setAdminVerified(bool v) { m_bAdminVerified = v; }
    bool isAdminVerified() const { return m_bAdminVerified; }

    // Export functions
    bool exportTextDrawsToPawn(const std::string& filepath);
    bool exportMapToPawn(const std::string& filepath);

private:
    void renderTextDrawStudio();
    void renderMapStudio();
    void renderSettingsAndExport();
    void renderCanvasPreview();

private:
    bool m_bVisible = false;
    bool m_bAdminVerified = false; // ต้องได้รับการยืนยันสิทธิ์แอดมิน 129+ จากเซิร์ฟเวอร์ก่อน
    int m_iAdminLevel = 129;

    // TextDraw Studio State
    std::vector<DevTextDrawElement> m_textdraws;
    int m_iSelectedTextDraw = -1;
    bool m_bShowCanvasPreview = true;
    int m_nextTdId = 1;

    // Map Studio State
    std::vector<DevMapObject> m_mapObjects;
    int m_iSelectedMapObject = -1;
    float m_fCurrentStep = 0.5f; // Step การเลื่อน 0.05, 0.5, 2.0, 10.0
    int m_iInputModelId = -1000; // ค่าเริ่มต้น เช่น house.dff ID -1000
    int m_nextObjId = 1;

    // Export Status
    char m_exportStatus[256];
};
