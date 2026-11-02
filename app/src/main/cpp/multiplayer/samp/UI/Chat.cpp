#include "Chat.h"
#include "samp/GUI/gui.h"
#include "samp/main.h"
#include "samp/Multiplayer/Multiplayer.h"
#include "samp/Network/Network.h"
#include "samp/Settings/Settings.h"
#include "samp/Utils/JNIUtil.h"
#include "samp/UI/PlayerTags.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <ctime>
#include <cstdarg>
#include <cstdint>
#include <vector>
#include <string>

extern UI* pUI;
extern CGame* pGame;
extern CNetGame* pNetGame;
extern CSettings* pSettings;
extern CJavaWrapper* pJavaWrapper;

Chat* pChat = nullptr;
std::string Chat::sDevTagNick = "";

// ============================================================================
// Unicode & Thai/Lao Text Helpers
// ============================================================================

static uint32_t decodeUtf8(const char*& p, const char* end)
{
    if (p >= end) return 0;
    unsigned char c = (unsigned char)*p++;
    if (c <= 0x7F) return c;
    if ((c & 0xE0) == 0xC0) {
        if (p >= end) return 0;
        return ((c & 0x1F) << 6) | (*p++ & 0x3F);
    }
    if ((c & 0xF0) == 0xE0) {
        if (p + 1 >= end) return 0;
        uint32_t cp = ((c & 0x0F) << 12) | (((unsigned char)*p++ & 0x3F) << 6);
        return cp | ((unsigned char)*p++ & 0x3F);
    }
    if ((c & 0xF8) == 0xF0) {
        if (p + 2 >= end) return 0;
        uint32_t cp = ((c & 0x07) << 18) | (((unsigned char)*p++ & 0x3F) << 12);
        cp |= (((unsigned char)*p++ & 0x3F) << 6);
        return cp | ((unsigned char)*p++ & 0x3F);
    }
    return 0;
}

// Thai combining marks: vowels above/below, tone marks, thanthakhat, etc.
// Lao combining marks: vowels above/below, tone marks, etc.
static bool isCombiningMark(uint32_t cp)
{
    // Thai combining marks
    if (cp == 0x0E31 || (cp >= 0x0E34 && cp <= 0x0E3A) || (cp >= 0x0E47 && cp <= 0x0E4E))
        return true;
    // Lao combining marks
    if (cp == 0x0EB1 || (cp >= 0x0EB4 && cp <= 0x0EB9) || cp == 0x0EBB || cp == 0x0EBC || (cp >= 0x0EC8 && cp <= 0x0ECD))
        return true;
    return false;
}

// Thai & Lao leading vowels (เ, แ, โ, ใ, ไ, Lao: ເ, ແ, ໂ, ໃ, ไ)
static bool isLeadingVowel(uint32_t cp)
{
    return (cp >= 0x0E40 && cp <= 0x0E44) || (cp >= 0x0EC0 && cp <= 0x0EC4);
}

static bool isAsciiPunctuation(char c)
{
    return (c == ',' || c == '.' || c == '!' || c == '?' || c == ';' || c == ':' ||
            c == '-' || c == '/' || c == '(' || c == ')' || c == '[' || c == ']');
}

// Measure visual pixel width of a string (ignoring SA-MP inline color tags {RRGGBB})
static float measureStringVisualWidth(const std::string& str, float fontSize)
{
    if (str.empty()) return 0.0f;

    if (ImGui::GetCurrentContext() && ImGui::GetIO().Fonts && !ImGui::GetIO().Fonts->Fonts.empty())
    {
        ImFont* font = ImGui::GetIO().Fonts->Fonts[0];
        if (font)
        {
            float sz = (fontSize <= 0.0f) ? font->FontSize : fontSize;
            const char* start = str.c_str();
            const char* cur = start;
            const char* end = start + str.length();
            float totalW = 0.0f;
            const char* segStart = start;

            while (cur < end)
            {
                if (*cur == '{' && cur + 7 < end && cur[7] == '}')
                {
                    if (cur > segStart)
                    {
                        ImVec2 s = font->CalcTextSizeA(sz, FLT_MAX, 0.0f, segStart, cur);
                        totalW += s.x;
                    }
                    cur += 7;
                    segStart = cur + 1;
                }
                cur++;
            }
            if (cur > segStart)
            {
                ImVec2 s = font->CalcTextSizeA(sz, FLT_MAX, 0.0f, segStart, cur);
                totalW += s.x;
            }
            return totalW;
        }
    }

    // Fallback if font is not ready: estimate width
    float totalW = 0.0f;
    float charW = (fontSize > 0.0f ? fontSize : 14.0f) * 0.55f;
    const char* p = str.c_str();
    const char* end = p + str.length();
    while (p < end)
    {
        if (*p == '{' && p + 7 < end && p[7] == '}') {
            p += 8;
            continue;
        }
        uint32_t cp = decodeUtf8(p, end);
        if (!isCombiningMark(cp)) totalW += charW;
    }
    return totalW;
}

// ============================================================================
// Thai / Lao & Multi-language Word Wrapper
// ============================================================================

struct TextCluster
{
    std::string str;
    float width = 0.0f;
    bool isWhitespace = false;
    bool isNewline = false;
    bool isColorTag = false;
    bool isLeadingVowel = false;
    bool isPunctuation = false;
};

static std::vector<TextCluster> parseMessageClusters(const std::string& message, float fontSize)
{
    std::vector<TextCluster> clusters;
    const char* p = message.c_str();
    const char* end = p + message.length();

    while (p < end)
    {
        // 1. Color tag {RRGGBB}
        if (*p == '{' && p + 7 < end && p[7] == '}')
        {
            TextCluster cl;
            cl.str = std::string(p, 8);
            cl.width = 0.0f;
            cl.isColorTag = true;
            clusters.push_back(cl);
            p += 8;
            continue;
        }

        // 2. Newline
        if (*p == '\n')
        {
            TextCluster cl;
            cl.str = "\n";
            cl.width = 0.0f;
            cl.isNewline = true;
            clusters.push_back(cl);
            p++;
            continue;
        }

        // 3. Whitespace
        if (*p == ' ' || *p == '\t')
        {
            TextCluster cl;
            cl.str = std::string(1, *p);
            cl.width = measureStringVisualWidth(cl.str, fontSize);
            cl.isWhitespace = true;
            clusters.push_back(cl);
            p++;
            continue;
        }

        // 4. Unicode codepoint & cluster formation
        const char* clusterStart = p;
        uint32_t baseCp = decodeUtf8(p, end);

        // Consume any attached combining marks for Thai/Lao
        while (p < end)
        {
            const char* peekP = p;
            uint32_t nextCp = decodeUtf8(peekP, end);
            if (isCombiningMark(nextCp))
            {
                p = peekP;
            }
            else
            {
                break;
            }
        }

        TextCluster cl;
        cl.str = std::string(clusterStart, p - clusterStart);
        cl.width = measureStringVisualWidth(cl.str, fontSize);
        cl.isLeadingVowel = isLeadingVowel(baseCp);
        cl.isPunctuation = (cl.str.size() == 1 && isAsciiPunctuation(cl.str[0]));
        clusters.push_back(cl);
    }

    return clusters;
}

static std::vector<std::string> wrapChatMessage(const std::string& message, float firstLineWidth, float subsequentLineWidth, float fontSize)
{
    std::vector<std::string> lines;
    if (message.empty()) return lines;

    std::vector<TextCluster> clusters = parseMessageClusters(message, fontSize);
    if (clusters.empty()) return lines;

    std::string currentLine;
    std::string activeColorTag;
    float currentLineWidth = 0.0f;
    float maxW = firstLineWidth;

    int lastBreakIndex = -1;
    std::string lineAtBreak;
    float widthAtBreak = 0.0f;
    std::string colorTagAtBreak;

    for (size_t i = 0; i < clusters.size(); i++)
    {
        const auto& cl = clusters[i];

        if (cl.isColorTag)
        {
            activeColorTag = cl.str;
            currentLine += cl.str;
            continue;
        }

        if (cl.isNewline)
        {
            lines.push_back(currentLine);
            currentLine = activeColorTag;
            currentLineWidth = 0.0f;
            maxW = subsequentLineWidth;
            lastBreakIndex = -1;
            lineAtBreak.clear();
            continue;
        }

        if (currentLineWidth + cl.width > maxW)
        {
            if (lastBreakIndex != -1 && !lineAtBreak.empty())
            {
                // Break at previous natural break opportunity (space, punctuation, or before leading vowel)
                lines.push_back(lineAtBreak);
                currentLine = colorTagAtBreak;
                currentLineWidth = 0.0f;
                maxW = subsequentLineWidth;
                i = (size_t)lastBreakIndex; // Next iteration will advance to lastBreakIndex + 1
                lastBreakIndex = -1;
                lineAtBreak.clear();
                continue;
            }
            else if (!currentLine.empty())
            {
                // No break point found on this line (Thai/Lao sentence with no spaces)
                // Cleanly break between grapheme clusters without corrupting UTF-8 or splitting combining marks!
                lines.push_back(currentLine);
                currentLine = activeColorTag + cl.str;
                currentLineWidth = cl.width;
                maxW = subsequentLineWidth;
                lastBreakIndex = -1;
                lineAtBreak.clear();
                continue;
            }
        }

        currentLine += cl.str;
        currentLineWidth += cl.width;

        // Record break opportunity
        if (cl.isWhitespace)
        {
            lastBreakIndex = (int)i;
            lineAtBreak = currentLine;
            widthAtBreak = currentLineWidth;
            colorTagAtBreak = activeColorTag;
        }
        else if (cl.isLeadingVowel && currentLineWidth > 0.25f * maxW)
        {
            // Breaking right before a Thai/Lao leading vowel (เ, แ, โ, ใ, ไ)
            std::string beforeVowel = currentLine.substr(0, currentLine.size() - cl.str.size());
            if (!beforeVowel.empty())
            {
                lastBreakIndex = (int)i - 1;
                lineAtBreak = beforeVowel;
                widthAtBreak = currentLineWidth - cl.width;
                colorTagAtBreak = activeColorTag;
            }
        }
        else if (cl.isPunctuation)
        {
            lastBreakIndex = (int)i;
            lineAtBreak = currentLine;
            widthAtBreak = currentLineWidth;
            colorTagAtBreak = activeColorTag;
        }
    }

    if (!currentLine.empty())
    {
        lines.push_back(currentLine);
    }

    return lines;
}

// ============================================================================
// SMS Detection
// ============================================================================

static bool containsCaseInsensitive(const std::string& haystack, const std::string& needle)
{
    if (needle.empty()) return false;
    auto it = std::search(
        haystack.begin(), haystack.end(),
        needle.begin(), needle.end(),
        [](char ch1, char ch2) { return std::tolower((unsigned char)ch1) == std::tolower((unsigned char)ch2); }
    );
    return (it != haystack.end());
}

static bool isSmsMessage(const std::string& msg, const std::string& customPattern)
{
    if (!customPattern.empty() && containsCaseInsensitive(msg, customPattern))
        return true;

    // Default SA-MP SMS keywords (English & Thai)
    if (containsCaseInsensitive(msg, "sms") ||
        containsCaseInsensitive(msg, "[sms]") ||
        containsCaseInsensitive(msg, "(sms)") ||
        containsCaseInsensitive(msg, "ข้อความ") ||
        containsCaseInsensitive(msg, "ส่งถึง") ||
        containsCaseInsensitive(msg, "จาก:") ||
        containsCaseInsensitive(msg, "whisper") ||
        containsCaseInsensitive(msg, "pm from") ||
        containsCaseInsensitive(msg, "pm to"))
    {
        return true;
    }
    return false;
}

// ============================================================================
// Chat Implementation
// ============================================================================

Chat::Chat()
    : ListBox(), m_showingChat(true), m_activeTab(TAB_CHAT), m_unreadSms(false),
      m_smsPattern("sms"), m_currentAlpha(1.0f), m_lastActivityTime(0.0)
{
    pChat = this;
    m_lastActivityTime = ImGui::GetTime();
}

void Chat::markActivity()
{
    m_lastActivityTime = ImGui::GetTime();
}

void Chat::switchTab(int tab)
{
    if (m_activeTab == tab) return;
    m_activeTab = tab;
    if (m_activeTab == TAB_SMS)
    {
        m_unreadSms = false;
    }
    markActivity();
    rebuildVisibleItems();
}

void Chat::setSmsPattern(const std::string& pattern)
{
    m_smsPattern = pattern;
    for (auto& entry : m_history)
    {
        entry.isSms = isSmsMessage(entry.message, m_smsPattern);
    }
    rebuildVisibleItems();
    addInfoMessage("{38BDF8}[Chat]{FFFFFF} ตั้งค่าคำกรอง SMS เป็น: {38BDF8}%s{FFFFFF}", m_smsPattern.c_str());
}

void Chat::addHistoryEntry(const ChatEntry& entry)
{
    m_history.push_back(entry);
    if (m_history.size() > 200)
    {
        m_history.erase(m_history.begin());
    }

    markActivity();

    if (entry.isSms && m_activeTab != TAB_SMS)
    {
        m_unreadSms = true;
    }

    if (m_activeTab == TAB_CHAT || (m_activeTab == TAB_SMS && entry.isSms))
    {
        renderEntryToListBox(entry);
        while (this->itemsCount() > UISettings::chatMaxMessages())
        {
            this->removeItem(0);
        }
        this->setScrollY(1.0f);
    }
}

void Chat::renderEntryToListBox(const ChatEntry& entry)
{
    float fontSize = UISettings::fontSize() / 2.0f;
    float maxChatW = UISettings::chatSize().x - 10.0f;

    if (entry.isPlayerMessage)
    {
        float nickW = measureStringVisualWidth(entry.nick + ": ", fontSize);
        float firstLineW = maxChatW - nickW;
        if (firstLineW < 60.0f) firstLineW = 60.0f;

        std::vector<std::string> lines = wrapChatMessage(entry.message, firstLineW, maxChatW, fontSize);
        if (lines.empty()) return;

        PlayerMessageItem* firstItem = new PlayerMessageItem(lines[0], entry.nick, entry.nickColor);
        firstItem->setAlpha(m_currentAlpha);
        this->addItem(firstItem);

        for (size_t i = 1; i < lines.size(); i++)
        {
            MessageItem* contItem = new MessageItem(lines[i], entry.color);
            contItem->setAlpha(m_currentAlpha);
            this->addItem(contItem);
        }
    }
    else
    {
        std::vector<std::string> lines = wrapChatMessage(entry.message, maxChatW, maxChatW, fontSize);
        for (const auto& line : lines)
        {
            MessageItem* item = new MessageItem(line, entry.color);
            item->setAlpha(m_currentAlpha);
            this->addItem(item);
        }
    }
}

void Chat::rebuildVisibleItems()
{
    while (this->itemsCount() > 0)
    {
        this->removeItem(0);
    }

    std::vector<ChatEntry> visibleEntries;
    for (const auto& entry : m_history)
    {
        if (m_activeTab == TAB_SMS && !entry.isSms) continue;
        visibleEntries.push_back(entry);
    }

    int maxMsg = UISettings::chatMaxMessages();
    int startIdx = 0;
    if ((int)visibleEntries.size() > maxMsg)
    {
        startIdx = (int)visibleEntries.size() - maxMsg;
    }

    for (size_t i = startIdx; i < visibleEntries.size(); i++)
    {
        renderEntryToListBox(visibleEntries[i]);
    }

    while (this->itemsCount() > UISettings::chatMaxMessages())
    {
        this->removeItem(0);
    }

    this->setScrollY(1.0f);
}

void Chat::addMessage(const std::string& message, const ImColor& color)
{
    ChatEntry entry;
    entry.type = ChatEntry::TYPE_NORMAL;
    entry.message = message;
    entry.color = color;
    entry.isPlayerMessage = false;
    entry.isSms = isSmsMessage(message, m_smsPattern);
    entry.timestamp = time(nullptr);

    addHistoryEntry(entry);
}

void Chat::addChatMessage(const std::string& message, const std::string& nick, const ImColor& nick_color)
{
    ChatEntry entry;
    entry.type = ChatEntry::TYPE_PLAYER;
    entry.message = message;
    entry.nick = nick;
    entry.color = ImColor(1.0f, 1.0f, 1.0f);
    entry.nickColor = nick_color;
    entry.isPlayerMessage = true;
    entry.isSms = isSmsMessage(message, m_smsPattern);
    entry.timestamp = time(nullptr);

    addHistoryEntry(entry);
}

void Chat::addInfoMessage(const std::string& format, ...)
{
    char tmp_buf[512];
    va_list args;
    va_start(args, format);
    vsnprintf(tmp_buf, sizeof(tmp_buf), format.c_str(), args);
    va_end(args);

    addMessage(std::string(tmp_buf), ImColor(0x00, 0xc8, 0xc8));
}

void Chat::addDebugMessage(const std::string& format, ...)
{
    char tmp_buf[512];
    va_list args;
    va_start(args, format);
    vsnprintf(tmp_buf, sizeof(tmp_buf), format.c_str(), args);
    va_end(args);

    addMessage(std::string(tmp_buf), ImColor(0xbe, 0xbe, 0xbe));
}

void Chat::addClientMessage(const std::string& message, const ImColor& color)
{
    addMessage(message, color);
}

void Chat::toggleVisibility()
{
    m_showingChat = !m_showingChat;
    if (m_showingChat)
    {
        rebuildVisibleItems();
    }
}

void Chat::touchPopEvent()
{
    if (!m_showingChat) return;
    if (pUI && pUI->playertablist() && pUI->playertablist()->visible()) return;
    markActivity();
    pUI->keyboard()->show(this);
}

void Chat::touchEvent(const ImVec2& pos, TouchType type)
{
    markActivity();

    // Check touches on Toggle button (works both when expanded and when collapsed)
    if (pos.x >= m_toggleMin.x && pos.x <= m_toggleMax.x &&
        pos.y >= m_toggleMin.y && pos.y <= m_toggleMax.y)
    {
        if (type == TouchType::pop)
        {
            toggleVisibility();
        }
        return;
    }

    if (!m_showingChat) return;

    // Check touches on CHAT / SMS tabs
    if (pos.x >= m_tabChatMin.x && pos.x <= m_tabChatMax.x &&
        pos.y >= m_tabChatMin.y && pos.y <= m_tabChatMax.y)
    {
        if (type == TouchType::pop)
        {
            switchTab(TAB_CHAT);
        }
        return;
    }

    if (pos.x >= m_tabSmsMin.x && pos.x <= m_tabSmsMax.x &&
        pos.y >= m_tabSmsMin.y && pos.y <= m_tabSmsMax.y)
    {
        if (type == TouchType::pop)
        {
            switchTab(TAB_SMS);
        }
        return;
    }

    ListBox::touchEvent(pos, type);
}

void Chat::keyboardEvent(const std::string& input)
{
    markActivity();
    if (input.empty() || !pNetGame) return;

    if (input[0] == '/')
    {
        if (!commandClient(input))
            pNetGame->SendChatCommand(input.c_str());
    }
    else
        pNetGame->SendChatMessage(input.c_str());
}

bool Chat::commandClient(const std::string& command)
{
    if (command == "/dev" || command == "/editor" || command == "/devedit")
    {
        // หากเปิดอยู่แล้ว ให้คำสั่งปิดได้ทันที
        if (pUI && pUI->deveditor() && pUI->deveditor()->visible())
        {
            pUI->deveditor()->toggle();
            return true;
        }
        // หากยังไม่ได้เปิด ให้ส่งคำสั่งไปตรวจสิทธิ์แอดมินระดับ 129+ ที่เซิร์ฟเวอร์ก่อน
        return false;
    }

    if (command == "/chat")
    {
        if (pChat) pChat->toggleVisibility();
        return true;
    }

    if (command == "/sms" || command == "/sms ")
    {
        if (pChat) pChat->switchTab(TAB_SMS);
        return true;
    }

    if (command.rfind("/smspattern", 0) == 0 || command.rfind("/setsms", 0) == 0)
    {
        size_t sp = command.find(' ');
        if (sp != std::string::npos && sp + 1 < command.size())
        {
            std::string pat = command.substr(sp + 1);
            while (!pat.empty() && pat.front() == ' ') pat.erase(0, 1);
            while (!pat.empty() && pat.back() == ' ') pat.pop_back();
            if (!pat.empty())
            {
                if (pChat) pChat->setSmsPattern(pat);
                return true;
            }
        }
        if (pChat)
        {
            pChat->addInfoMessage("{38BDF8}[Chat]{FFFFFF} รูปแบบ: /smspattern <คำที่ต้องการกรอง> (ปัจจุบัน: %s)",
                                  pChat->smsPattern().c_str());
        }
        return true;
    }

    return false;
}

void Chat::draw(ImGuiRenderer* renderer)
{
    float scaleX = (UISettings::scaleFactor().x > 0.0f) ? UISettings::scaleFactor().x : 1.0f;
    float scaleY = (UISettings::scaleFactor().y > 0.0f) ? UISettings::scaleFactor().y : 1.0f;
    float fontSize = UISettings::fontSize() / 2.2f;

    if (!m_showingChat)
    {
        // When collapsed: draw a compact badge "💬 เปิด" so user can expand chat
        ImVec2 pos = absolutePosition();
        float btnH = 22.0f * scaleY;
        float btnW = 55.0f * scaleX;
        m_toggleMin = pos;
        m_toggleMax = ImVec2(pos.x + btnW, pos.y + btnH);

        ImColor bg(15, 23, 42, 180);
        renderer->drawRect(m_toggleMin, m_toggleMax, bg, true, 6.0f);
        std::string label = "💬 เปิด";
        ImVec2 labelSz = renderer->calculateTextSize(label, fontSize);
        ImVec2 txtPos(
            m_toggleMin.x + (btnW - labelSz.x) / 2.0f,
            m_toggleMin.y + (btnH - labelSz.y) / 2.0f
        );
        renderer->drawText(txtPos, ImColor(255, 255, 255, 255), label, false, fontSize);
        return;
    }

    // Always 1.0f alpha (no dimming/decay, per user request)
    m_currentAlpha = 1.0f;
    if (panel())
    {
        for (Widget* w : panel()->children())
        {
            if (ListBoxItem* item = static_cast<ListBoxItem*>(w))
            {
                item->setAlpha(1.0f);
            }
        }
    }

    // Draw messages
    ListBox::draw(renderer);

    // Clean Minimalist Transparent Tabs (CHAT / SMS) and Toggle button underneath the chat box
    ImVec2 pos = absolutePosition();
    ImVec2 sz = size();
    float tabH = 20.0f * scaleY;
    float tabW = 55.0f * scaleX;
    float tabY = pos.y + sz.y + 6.0f;

    m_tabChatMin = ImVec2(pos.x, tabY);
    m_tabChatMax = ImVec2(pos.x + tabW, tabY + tabH);

    m_tabSmsMin = ImVec2(pos.x + tabW + 6.0f, tabY);
    m_tabSmsMax = ImVec2(pos.x + tabW * 2.0f + 6.0f, tabY + tabH);

    m_toggleMin = ImVec2(pos.x + tabW * 2.0f + 12.0f, tabY);
    m_toggleMax = ImVec2(pos.x + tabW * 3.0f + 12.0f, tabY + tabH);

    // CHAT Tab
    bool isChatActive = (m_activeTab == TAB_CHAT);
    ImColor chatBg = isChatActive
        ? ImColor(255, 255, 255, 45)
        : ImColor(0, 0, 0, 30);
    ImColor chatTextCol = isChatActive
        ? ImColor(255, 255, 255, 255)
        : ImColor(170, 170, 170, 180);
    renderer->drawRect(m_tabChatMin, m_tabChatMax, chatBg, true, tabH / 2.0f);

    std::string chatLabel = "CHAT";
    ImVec2 chatLabelSz = renderer->calculateTextSize(chatLabel, fontSize);
    ImVec2 chatTextPos = ImVec2(
        m_tabChatMin.x + (tabW - chatLabelSz.x) / 2.0f,
        m_tabChatMin.y + (tabH - chatLabelSz.y) / 2.0f
    );
    renderer->drawText(chatTextPos, chatTextCol, chatLabel, false, fontSize);

    // SMS Tab
    bool isSmsActive = (m_activeTab == TAB_SMS);
    ImColor smsBg = isSmsActive
        ? ImColor(255, 255, 255, 45)
        : ImColor(0, 0, 0, 30);
    ImColor smsTextCol = isSmsActive
        ? ImColor(255, 255, 255, 255)
        : (m_unreadSms ? ImColor(251, 191, 36, 250) : ImColor(170, 170, 170, 180));
    renderer->drawRect(m_tabSmsMin, m_tabSmsMax, smsBg, true, tabH / 2.0f);

    std::string smsLabel = m_unreadSms ? "SMS •" : "SMS";
    ImVec2 smsLabelSz = renderer->calculateTextSize(smsLabel, fontSize);
    ImVec2 smsTextPos = ImVec2(
        m_tabSmsMin.x + (tabW - smsLabelSz.x) / 2.0f,
        m_tabSmsMin.y + (tabH - smsLabelSz.y) / 2.0f
    );
    renderer->drawText(smsTextPos, smsTextCol, smsLabel, false, fontSize);

    // Toggle Hide Button ("💬 ซ่อน")
    ImColor toggleBg(0, 0, 0, 30);
    ImColor toggleTextCol(200, 200, 200, 220);
    renderer->drawRect(m_toggleMin, m_toggleMax, toggleBg, true, tabH / 2.0f);

    std::string toggleLabel = "💬 ซ่อน";
    ImVec2 toggleLabelSz = renderer->calculateTextSize(toggleLabel, fontSize);
    ImVec2 toggleTextPos = ImVec2(
        m_toggleMin.x + (tabW - toggleLabelSz.x) / 2.0f,
        m_toggleMin.y + (tabH - toggleLabelSz.y) / 2.0f
    );
    renderer->drawText(toggleTextPos, toggleTextCol, toggleLabel, false, fontSize);
}

void Chat::activateEvent(bool active)
{
    markActivity();
    this->setScrollable(active);
}
