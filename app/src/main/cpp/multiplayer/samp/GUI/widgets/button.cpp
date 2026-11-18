#include "samp/main.h"
#include "samp/GUI/gui.h"
#include "button.h"

extern bool OpenButton;

//============== Default Button =================//
Button::Button(const std::string& caption, float font_size)
{
    m_callback = nullptr;

    m_label = new Label(caption, ImColor(255, 255, 255, 255), false, font_size);
    this->addChild(m_label);

    m_color = UISettings::buttonColor();
    m_colorFocused = UISettings::buttonFocusedColor();
}

void Button::performLayout()
{
    float padX = UISettings::padding() * 0.8f;
    float padY = UISettings::padding() * 0.45f;

    m_label->performLayout();
    this->setSize(m_label->size() + ImVec2(padX * 2.0f, padY * 2.0f));

    m_label->setPosition((size() - m_label->size()) / 2.0f);
}

void Button::draw(ImGuiRenderer* renderer)
{
    // Static translucent dark background or pressed state (color change only)
    ImColor bgCol = focused() ? m_colorFocused : m_color;
    ImColor borderCol = focused() ? ImColor(255, 255, 255, 220) : ImColor(255, 255, 255, 140);

    float radius = size().y * 0.45f;
    if (radius > 16.0f) radius = 16.0f;

    renderer->drawRect(
            absolutePosition(),
            absolutePosition() + size(),
            bgCol,
            true,
            radius
    );

    // Thin solid border
    renderer->drawRect(
            absolutePosition(),
            absolutePosition() + size(),
            borderCol,
            false,
            radius,
            1.0f
    );

    if (m_label)
    {
        m_label->setColor(ImColor(255, 255, 255, 255));
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

    m_label = new Label(caption, ImColor(255, 255, 255, 255), false, font_size);
    this->addChild(m_label);

    m_color = UISettings::buttonColor();
    m_colorFocused = UISettings::buttonFocusedColor();
}

void CButton::performLayout()
{
    float padX = UISettings::padding() * 0.8f;
    float padY = UISettings::padding() * 0.45f;

    m_label->performLayout();
    this->setSize(m_label->size() + ImVec2(padX * 2.0f, padY * 2.0f));

    m_label->setPosition((size() - m_label->size()) / 2.0f);
}

void CButton::draw(ImGuiRenderer* renderer)
{
    if (!OpenButton) return;

    // Static translucent dark background or pressed state (color change only)
    ImColor bgCol = focused() ? m_colorFocused : m_color;
    ImColor borderCol = focused() ? ImColor(255, 255, 255, 220) : ImColor(255, 255, 255, 140);

    float radius = size().y * 0.45f;
    if (radius > 16.0f) radius = 16.0f;

    renderer->drawRect(
            absolutePosition(),
            absolutePosition() + size(),
            bgCol,
            true,
            radius
    );

    // Thin solid border
    renderer->drawRect(
            absolutePosition(),
            absolutePosition() + size(),
            borderCol,
            false,
            radius,
            1.0f
    );

    if (m_label)
    {
        m_label->setColor(ImColor(255, 255, 255, 255));
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

    m_label = new Label(caption, ImColor(255, 255, 255, 255), false, font_size);
    this->addChild(m_label);

    m_color = UISettings::buttonColor();
    m_colorFocused = UISettings::buttonFocusedColor();
}

void OButton::performLayout()
{
    float padX = UISettings::padding() * 0.8f;
    float padY = UISettings::padding() * 0.45f;

    m_label->performLayout();
    this->setSize(m_label->size() + ImVec2(padX * 2.0f, padY * 2.0f));

    m_label->setPosition((size() - m_label->size()) / 2.0f);
}

void OButton::draw(ImGuiRenderer* renderer)
{
    if (OpenButton)
    {
        this->setPosition(ImVec2(-150.0f, -150.0f));
        return;
    }

    // Static translucent dark background or pressed state (color change only)
    ImColor bgCol = focused() ? m_colorFocused : m_color;
    ImColor borderCol = focused() ? ImColor(255, 255, 255, 220) : ImColor(255, 255, 255, 140);

    float radius = size().y * 0.45f;
    if (radius > 16.0f) radius = 16.0f;

    renderer->drawRect(
            absolutePosition(),
            absolutePosition() + size(),
            bgCol,
            true,
            radius
    );

    // Thin solid border
    renderer->drawRect(
            absolutePosition(),
            absolutePosition() + size(),
            borderCol,
            false,
            radius,
            1.0f
    );

    if (m_label)
    {
        m_label->setColor(ImColor(255, 255, 255, 255));
    }

    Widget::draw(renderer);

    this->setPosition(ImVec2(15.0f, 15.0f));
}

void OButton::touchPopEvent()
{
    if (m_callback) m_callback();
}
