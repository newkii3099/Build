#pragma once

#include <list>
#include <vector>
#include <string>
#include <chrono>
#include <ctime>
#include "samp/GUI/gui.h"

struct ChatEntry
{
    enum Type {
        TYPE_NORMAL,
        TYPE_PLAYER,
        TYPE_INFO,
        TYPE_DEBUG
    };

    Type type = TYPE_NORMAL;
    std::string message;
    std::string nick;
    ImColor color = ImColor(1.0f, 1.0f, 1.0f);
    ImColor nickColor = ImColor(1.0f, 1.0f, 1.0f);
    bool isPlayerMessage = false;
    bool isSms = false;
    time_t timestamp = 0;
};

class Chat : public ListBox
{
public:
    enum Tab {
        TAB_CHAT = 0,
        TAB_SMS = 1
    };

    Chat();

    void addChatMessage(const std::string& message, const std::string& nick, const ImColor& nick_color);
    void addInfoMessage(const std::string& format, ...);
    void addDebugMessage(const std::string& format, ...);
    void addClientMessage(const std::string& message, const ImColor& color);

    virtual void draw(ImGuiRenderer* renderer) override;
    virtual void touchEvent(const ImVec2& pos, TouchType type) override;
    virtual void activateEvent(bool active) override;
    virtual void touchPopEvent() override;
    virtual void keyboardEvent(const std::string& input) override;

    [[maybe_unused]] static bool commandClient(const std::string& command);

    static std::string sDevTagNick;

    void toggleVisibility();
    bool isVisible() const { return m_showingChat; }

    void switchTab(int tab);
    int activeTab() const { return m_activeTab; }

    void setSmsPattern(const std::string& pattern);
    const std::string& smsPattern() const { return m_smsPattern; }

    void markActivity();

private:
    void addMessage(const std::string& message, const ImColor& color = ImColor(1.0f, 1.0f, 1.0f));
    void addHistoryEntry(const ChatEntry& entry);
    void rebuildVisibleItems();
    void renderEntryToListBox(const ChatEntry& entry);

    bool m_showingChat = true;

    // Tabs & filtering
    int m_activeTab = TAB_CHAT;
    bool m_unreadSms = false;
    std::string m_smsPattern = "sms";
    std::vector<ChatEntry> m_history;

    // Auto-fade state
    float m_currentAlpha = 1.0f;
    double m_lastActivityTime = 0.0;

    // Tab & Toggle button touch rectangles
    ImVec2 m_tabChatMin = ImVec2(0.0f, 0.0f);
    ImVec2 m_tabChatMax = ImVec2(0.0f, 0.0f);
    ImVec2 m_tabSmsMin = ImVec2(0.0f, 0.0f);
    ImVec2 m_tabSmsMax = ImVec2(0.0f, 0.0f);
    ImVec2 m_toggleMin = ImVec2(0.0f, 0.0f);
    ImVec2 m_toggleMax = ImVec2(0.0f, 0.0f);

    class MessageItem : public ListBoxItem
    {
    public:
        MessageItem(const std::string& message, const ImColor& color)
            : m_baseColor(color)
        {
            m_text = new Label(message, color, true, UISettings::fontSize() / 2);
            this->addChild(m_text);
        }

        void performLayout() override
        {
            m_text->performLayout();
            m_text->setPosition(ImVec2(0.0f, (height() - m_text->height()) / 2));
        }

        void setAlpha(float alpha) override
        {
            ImColor c = m_baseColor;
            c.Value.w *= alpha;
            m_text->setColor(c);
        }

    private:
        ImColor m_baseColor;
        Label* m_text;
    };

    class PlayerMessageItem : public ListBoxItem
    {
    public:
        PlayerMessageItem(const std::string& message, const std::string& nick, const ImColor& nick_color)
            : m_baseNickColor(nick_color), m_baseTextColor(ImColor(1.0f, 1.0f, 1.0f))
        {
            m_nick = new Label(nick, nick_color, true, UISettings::fontSize() / 2);
            m_text = new Label(": " + message, ImColor(1.0f, 1.0f, 1.0f), true, UISettings::fontSize() / 2);
            this->addChild(m_nick);
            this->addChild(m_text);
        }

        void performLayout() override
        {
            m_nick->performLayout();
            m_nick->setPosition(ImVec2(0.0f, (height() - m_nick->height()) / 2));

            m_text->performLayout();
            m_text->setPosition(ImVec2(m_nick->position().x + m_nick->width(), (height() - m_text->height()) / 2));
        }

        void setAlpha(float alpha) override
        {
            ImColor nc = m_baseNickColor;
            nc.Value.w *= alpha;
            m_nick->setColor(nc);

            ImColor tc = m_baseTextColor;
            tc.Value.w *= alpha;
            m_text->setColor(tc);
        }

    private:
        ImColor m_baseNickColor;
        ImColor m_baseTextColor;
        Label* m_nick;
        Label* m_text;
    };
};
