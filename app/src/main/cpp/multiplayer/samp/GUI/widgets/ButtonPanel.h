#pragma once

class ButtonPanel : public Layout
{
public:
    ButtonPanel();

    void performLayout() override;
    void draw(ImGuiRenderer* renderer) override;

    CButton* m_bH;
    OButton* m_bOpen;
    Button* m_bTab;
    Button* m_bSV;

private:
    Button* m_bAlt;
    Button* m_bY;
    Button* m_bN;
};
