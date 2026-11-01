#include "samp/main.h"
#include "samp/GUI/gui.h"
#include "button.h"

extern bool OpenButton;

//============== Default Button =================//
Button::Button(const std::string& caption, float font_size)
{
    m_callback = nullptr;

    m_label = new Label(caption, ImColor(15, 23, 42, 235), false, font_size);
    this->addChild(m_label);

    m_color = UISettings::buttonColor();
    m_colorFocused = UISettings::buttonFocusedColor();
}

void Button::performLayout()
{
    float padding = UISettings::padding();

    m_label->performLayout();
    this->setSize(m_label->size() + ImVec2(padding * 2, padding));

    m_label->setPosition((size() - m_label->size()) / 2);
}

void Button::draw(ImGuiRenderer* renderer)
{
    // Modern bright white-cyan translucent glass pill
    ImColor bgCol = focused() ? m_colorFocused : m_color;
    ImColor borderCol = focused() ? ImColor(255, 255, 255, 240) : ImColor(56, 189, 248, 190);

    renderer->drawRect(
            absolutePosition(),
            absolutePosition() + size(),
            bgCol,
            true,
            20.0f
    );

    renderer->drawRect(
            absolutePosition(),
            absolutePosition() + size(),
            borderCol,
            false,
            20.0f,
            1.2f
    );

    if (m_label)
    {
        m_label->setColor(focused() ? ImColor(255, 255, 255, 255) : ImColor(15, 23, 42, 235));
    }

    Widget::draw(renderer);
}

void Button::touchPopEvent()
{
    if (m_callback) m_callback();
}


//============== Custom Button =================//
CButton::CButton(const std::string& caption, float font_size)
{
    m_callback = nullptr;

    m_label = new Label(caption, ImColor(15, 23, 42, 235), false, font_size);
    this->addChild(m_label);

    m_color = UISettings::buttonColor();
    m_colorFocused = UISettings::buttonFocusedColor();
}

void CButton::performLayout()
{
    float padding = UISettings::padding();

    m_label->performLayout();
    this->setSize(m_label->size() + ImVec2(padding * 2, padding));

    m_label->setPosition((size() - m_label->size()) / 2);
}

void CButton::draw(ImGuiRenderer* renderer)
{
    if (!OpenButton) return;

    // Modern bright white-cyan translucent glass pill
    ImColor bgCol = focused() ? m_colorFocused : m_color;
    ImColor borderCol = focused() ? ImColor(255, 255, 255, 240) : ImColor(56, 189, 248, 190);

    renderer->drawRect(
            absolutePosition(),
            absolutePosition() + size(),
            bgCol,
            true,
            20.0f
    );

    renderer->drawRect(
            absolutePosition(),
            absolutePosition() + size(),
            borderCol,
            false,
            20.0f,
            1.2f
    );

    if (m_label)
    {
        m_label->setColor(focused() ? ImColor(255, 255, 255, 255) : ImColor(15, 23, 42, 235));
    }

    Widget::draw(renderer);
}

void CButton::touchPopEvent()
{
    if (!OpenButton) return;
    if (m_callback) m_callback();
}


//============== OButton =================//
OButton::OButton(const std::string& caption, float font_size)
{
    m_callback = nullptr;

    m_label = new Label(caption, ImColor(15, 23, 42, 235), false, font_size);
    this->addChild(m_label);

    m_color = UISettings::buttonColor();
    m_colorFocused = UISettings::buttonFocusedColor();
}

void OButton::performLayout()
{
    float padding = UISettings::padding();

    m_label->performLayout();
    this->setSize(m_label->size() + ImVec2(padding * 2, padding));

    m_label->setPosition((size() - m_label->size()) / 2);
}

void OButton::draw(ImGuiRenderer* renderer)
{
    if (OpenButton)
    {
        this->setPosition(ImVec2(-150.0f, -150.0f));
        return;
    }

    // Modern bright white-cyan translucent glass pill
    ImColor bgCol = focused() ? m_colorFocused : m_color;
    ImColor borderCol = focused() ? ImColor(255, 255, 255, 240) : ImColor(56, 189, 248, 190);

    renderer->drawRect(
            absolutePosition(),
            absolutePosition() + size(),
            bgCol,
            true,
            20.0f
    );

    renderer->drawRect(
            absolutePosition(),
            absolutePosition() + size(),
            borderCol,
            false,
            20.0f,
            1.2f
    );

    if (m_label)
    {
        m_label->setColor(focused() ? ImColor(255, 255, 255, 255) : ImColor(15, 23, 42, 235));
    }

    Widget::draw(renderer);

    this->setPosition(ImVec2(15.0f, 15.0f));
}

void OButton::touchPopEvent()
{
    if (m_callback) m_callback();
}
