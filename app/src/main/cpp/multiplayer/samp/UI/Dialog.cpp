#include "samp/main.h"
#include "samp/GUI/gui.h"
#include "samp/Multiplayer/Multiplayer.h"
#include "samp/Network/Network.h"

extern CNetGame* pNetGame;

extern void MyLog(const char* fmt, ...);

extern int Button2Show;

/* Dialog */

Dialog::Dialog()
{
	/* title */
	m_title = new DialogTitle();
	this->addChild(m_title);

	m_content = new Content();
	this->addChild(m_content);

	/* button panel */
	m_buttonPanel = new DialogButtonPanel();
	this->addChild(m_buttonPanel);

	m_dialogID = -1;
}

void Dialog::show(int16_t dialog_id, DialogStyle dialog_style, const std::string& title, const std::string& message, const std::string& button1, const std::string& button2)
{
	if (dialog_id >= 0) {
		this->setTitleCaption(title);
		this->setButton1Caption(button1);
		this->setButton2Caption(button2);
		/*if (button2.length() >= 1)
		{
			Button2Show = 1;
		}
		else
		{
			Button2Show = 2;
		}
		if (Button2Show == 2) {
			this->removeChild(button2);
		}
		if (Button2Show == 1) {
			this->addChild(button2);
		}*/
		//MyLog("button2 is %i", button2.length());

		m_content->setActive(dialog_style, message);

		this->performLayout();
		this->setVisible(true);

		m_dialogID = dialog_id;
		//MyLog("Dialog ID %d", dialog_id);
	}
}

void Dialog::hide()
{
	this->setVisible(false);
}

void Dialog::setTitleCaption(const std::string& caption)
{
	m_title->setTitle(caption);
}

void Dialog::setButton1Caption(const std::string& caption)
{
	m_buttonPanel->setButton1Caption(caption);
}

void Dialog::setButton2Caption(const std::string& caption)
{
	m_buttonPanel->setButton2Caption(caption);
}

void Dialog::performLayout()
{
	/* ��������� ������ �������� */
	m_content->setMinSize(ImVec2(
		this->minSize().x - (UISettings::padding() + UISettings::padding()),
		this->minSize().y - (UISettings::dialogTitleHeight() + UISettings::dialogButtonPanelSize().y)
	));
	m_content->setMaxSize(ImVec2(
		this->maxSize().x - (UISettings::padding() + UISettings::padding()),
		this->maxSize().y - (UISettings::dialogTitleHeight() + UISettings::dialogButtonPanelSize().y + UISettings::padding() + UISettings::padding())
	));
	m_content->performLayout();

	/* ������������ ������� ������� */
	ImVec2 dialog_size = ImVec2(
		m_content->width() + (UISettings::padding() + UISettings::padding()),
		UISettings::dialogTitleHeight() + m_content->height() + UISettings::dialogButtonPanelSize().y + UISettings::padding() + UISettings::padding()
	);

	this->setSize(dialog_size);

	/* ��������� ��������� */
	m_title->setSize(ImVec2(dialog_size.x, UISettings::dialogTitleHeight()));
	m_title->performLayout();

	/* ��������� ������ ������ */
	m_buttonPanel->setFixedSize(UISettings::dialogButtonPanelSize());
	m_buttonPanel->performLayout();

	/* ��������� ��� �������� */
	m_title->setPosition(ImVec2(0.0f, 0.0f));
	m_content->setPosition(ImVec2(
		UISettings::padding(),
		m_title->position().y + m_title->height() + UISettings::padding()));
	m_buttonPanel->setPosition(ImVec2(
		(dialog_size.x - m_buttonPanel->width()) / 2,
		(dialog_size.y - m_buttonPanel->height())));

	/* ������������� ������ �� ������ */
	this->setPosition(ImVec2(
		(parent()->width() - width()) / 2,
		(parent()->height() - height()) / 2));
}

void Dialog::draw(ImGuiRenderer* renderer)
{
	ImVec2 pos = absolutePosition();
	ImVec2 sz = size();
	ImVec2 maxPos = pos + sz;
	float rounding = 8.0f;

	// 1. AAA Outer Neon Reshade Bloom Glow
	renderer->drawRect(pos - ImVec2(2.0f, 2.0f), maxPos + ImVec2(2.0f, 2.0f), ImColor(0, 190, 255, 40), false, rounding + 2.0f, 3.0f);
	renderer->drawRect(pos - ImVec2(1.0f, 1.0f), maxPos + ImVec2(1.0f, 1.0f), ImColor(0, 220, 255, 80), false, rounding + 1.0f, 1.5f);

	// 2. High-Tech Glassmorphic Dark Base Background
	renderer->drawRect(pos, maxPos, ImColor(7, 13, 22, 240), true, rounding);

	// Cybernetic Depth Gradient
	renderer->drawRectFilledMulticolor(
		pos, maxPos,
		ImColor(10, 32, 60, 170), // Top-left: Electric deep sapphire
		ImColor(15, 45, 80, 170), // Top-right: Cyan-tinted deep blue
		ImColor(5, 10, 18, 245),  // Bottom-right: Dark obsidian
		ImColor(5, 10, 18, 245)   // Bottom-left: Dark obsidian
	);

	// 3. "ลายสลับเส้น แบบสายฟ้า" (Interleaved electric lightning scanlines & cybernetic lines)
	renderer->pushClipRect(pos, maxPos, true);

	// Angled 45-degree cyber scanlines with alternating electric blue pulses
	float step = 14.0f;
	int lineIdx = 0;
	for (float d = -sz.y; d < sz.x + sz.y; d += step, lineIdx++)
	{
		ImVec2 p1(pos.x + d, pos.y);
		ImVec2 p2(pos.x + d - sz.y, pos.y + sz.y);

		if (lineIdx % 4 == 0)
		{
			// Accent bright lightning trace line
			renderer->drawLine(p1, p2, ImColor(0, 215, 255, 26), 1.0f);
		}
		else if (lineIdx % 2 == 0)
		{
			// Subtle alternating scanline
			renderer->drawLine(p1, p2, ImColor(0, 160, 240, 12), 1.0f);
		}
	}

	// Fine horizontal holographic raster lines
	for (float y = pos.y; y < maxPos.y; y += 4.0f)
	{
		renderer->drawLine(ImVec2(pos.x, y), ImVec2(maxPos.x, y), ImColor(0, 180, 255, 7), 1.0f);
	}

	renderer->popClipRect();

	// 4. Sharp Neon Cyan Primary Border
	renderer->drawRect(pos, maxPos, ImColor(0, 225, 255, 190), false, rounding, 1.2f);

	// 5. Tactical AAA Sci-Fi Corner Brackets
	float bracketLen = 14.0f;
	float bThick = 2.0f;
	ImColor bracketCol(0, 245, 255, 255); // Intense electric cyan

	// Top-Left bracket
	renderer->drawLine(pos, ImVec2(pos.x + bracketLen, pos.y), bracketCol, bThick);
	renderer->drawLine(pos, ImVec2(pos.x, pos.y + bracketLen), bracketCol, bThick);

	// Top-Right bracket
	renderer->drawLine(ImVec2(maxPos.x, pos.y), ImVec2(maxPos.x - bracketLen, pos.y), bracketCol, bThick);
	renderer->drawLine(ImVec2(maxPos.x, pos.y), ImVec2(maxPos.x, pos.y + bracketLen), bracketCol, bThick);

	// Bottom-Left bracket
	renderer->drawLine(ImVec2(pos.x, maxPos.y), ImVec2(pos.x + bracketLen, maxPos.y), bracketCol, bThick);
	renderer->drawLine(ImVec2(pos.x, maxPos.y), ImVec2(pos.x, maxPos.y - bracketLen), bracketCol, bThick);

	// Bottom-Right bracket
	renderer->drawLine(maxPos, ImVec2(maxPos.x - bracketLen, maxPos.y), bracketCol, bThick);
	renderer->drawLine(maxPos, ImVec2(maxPos.x, maxPos.y - bracketLen), bracketCol, bThick);

	Widget::draw(renderer);
}

void Dialog::touchEvent(const ImVec2& pos, TouchType type)
{
	if (m_buttonPanel->contains(pos))
	{
		m_buttonPanel->touchEvent(pos, type);
		return;
	}

	Widget::touchEvent(pos, type);
}

/* DialogTitle */
DialogTitle::DialogTitle()
{
	m_label = new Label("Title", ImColor(230, 245, 255, 255), false, UISettings::fontSize() / 2);
	this->addChild(m_label);
}

void DialogTitle::setTitle(const std::string& caption)
{
	m_label->setText(caption);
}

void DialogTitle::performLayout()
{
	m_label->performLayout();
	m_label->setPosition(ImVec2(UISettings::padding() + 10.0f, (height() - m_label->height()) / 2));
}

void DialogTitle::draw(ImGuiRenderer* renderer)
{
	ImVec2 pos = absolutePosition();
	ImVec2 sz = size();
	ImVec2 maxPos = pos + sz;

	// 1. High-Tech Header Background: Electric Blue Reshade Gradient
	renderer->drawRectFilledMulticolor(
		pos, maxPos,
		ImColor(0, 110, 220, 225), // Left: Vibrant Electric Blue
		ImColor(0, 45, 100, 190),  // Right: Deep Holographic Navy
		ImColor(0, 30, 75, 190),
		ImColor(0, 80, 180, 225)
	);

	// 2. Neon cyan glowing top edge highlight
	renderer->drawLine(pos, ImVec2(maxPos.x, pos.y), ImColor(120, 240, 255, 230), 1.5f);

	// 3. Cyberpunk / Sci-Fi Vertical Energy Tag on Left of Title
	float barW = 4.0f;
	float barH = sz.y * 0.55f;
	float barY = pos.y + (sz.y - barH) / 2.0f;
	float barX = pos.x + 8.0f;
	renderer->drawRect(ImVec2(barX, barY), ImVec2(barX + barW, barY + barH), ImColor(0, 255, 255, 255), true, 2.0f);
	renderer->drawRect(ImVec2(barX - 1.0f, barY - 1.0f), ImVec2(barX + barW + 1.0f, barY + barH + 1.0f), ImColor(0, 220, 255, 120), false, 2.0f, 1.0f);

	// 4. "สายฟ้า / Lightning Wave Pulse" Separator Underneath Title
	float lineY = maxPos.y - 1.0f;
	float pulseMidX = pos.x + sz.x * 0.35f;
	float pW = 18.0f;
	float pH = 5.0f;

	// Left segment
	renderer->drawLine(ImVec2(pos.x, lineY), ImVec2(pulseMidX - pW, lineY), ImColor(0, 200, 255, 200), 1.5f);

	// Lightning zig-zag bolt / pulse peak
	ImVec2 pt0(pulseMidX - pW, lineY);
	ImVec2 pt1(pulseMidX - pW * 0.4f, lineY - pH);
	ImVec2 pt2(pulseMidX + pW * 0.1f, lineY + pH * 0.7f);
	ImVec2 pt3(pulseMidX + pW * 0.5f, lineY - pH * 0.6f);
	ImVec2 pt4(pulseMidX + pW, lineY);

	renderer->drawLine(pt0, pt1, ImColor(140, 245, 255, 255), 2.0f);
	renderer->drawLine(pt1, pt2, ImColor(0, 220, 255, 255), 2.0f);
	renderer->drawLine(pt2, pt3, ImColor(140, 245, 255, 255), 2.0f);
	renderer->drawLine(pt3, pt4, ImColor(0, 200, 255, 255), 2.0f);

	// Right segment
	renderer->drawLine(pt4, ImVec2(maxPos.x, lineY), ImColor(0, 200, 255, 200), 1.5f);

	// Bloom glow along separator line
	renderer->drawLine(ImVec2(pos.x, lineY), ImVec2(maxPos.x, lineY), ImColor(0, 200, 255, 50), 3.0f);

	Widget::draw(renderer);
}

/* DialogButtonPanel */
DialogButtonPanel::DialogButtonPanel()
	: Layout(Orientation::HORIZONTAL)
{
	m_button1 = new DialogButton1();
	this->addChild(m_button1);

	m_button2 = new DialogButton2();
	this->addChild(m_button2);
}

void DialogButtonPanel::setButton1Caption(const std::string& caption)
{
	m_button1->setCaption(caption);
}

void DialogButtonPanel::setButton2Caption(const std::string& caption)
{
	m_button2->setCaption(caption);
	m_button2->setVisible(!caption.empty());
}

void DialogButtonPanel::performLayout()
{
	Layout::performLayout();
}

/* DialogButtonPanel::DialogButton1 */

DialogButtonPanel::DialogButton1::DialogButton1()
	: Button("", UISettings::fontSize() / 2)
{

}

void DialogButtonPanel::DialogButton1::draw(ImGuiRenderer* renderer)
{
	ImVec2 pos = absolutePosition();
	ImVec2 sz = size();
	ImVec2 maxPos = pos + sz;
	float radius = sz.y * 0.35f;
	if (radius > 12.0f) radius = 12.0f;

	if (focused())
	{
		renderer->drawRect(pos - ImVec2(1.0f, 1.0f), maxPos + ImVec2(1.0f, 1.0f), ImColor(0, 240, 255, 140), false, radius + 1.0f, 1.5f);
		renderer->drawRectFilledMulticolor(pos, maxPos,
			ImColor(0, 200, 255, 255), ImColor(0, 160, 245, 255),
			ImColor(0, 120, 215, 255), ImColor(0, 160, 245, 255));
		renderer->drawRect(pos, maxPos, ImColor(200, 250, 255, 255), false, radius, 1.5f);
	}
	else
	{
		renderer->drawRectFilledMulticolor(pos, maxPos,
			ImColor(0, 140, 235, 225), ImColor(0, 100, 205, 225),
			ImColor(0, 70, 165, 225), ImColor(0, 110, 205, 225));
		renderer->drawRect(pos, maxPos, ImColor(0, 225, 255, 220), false, radius, 1.2f);
	}

	// Tech corner accent (little angled diagonal tick in top-left)
	renderer->drawLine(pos + ImVec2(3.0f, 6.0f), pos + ImVec2(6.0f, 3.0f), ImColor(255, 255, 255, 200), 1.0f);

	Widget::draw(renderer);
}

void DialogButtonPanel::DialogButton1::touchPopEvent()
{
	Dialog* dialog = dynamic_cast<Dialog*>(this->parent()->parent());

	if (pNetGame) {
		pNetGame->SendDialogResponse(dialog->dialogID(), 1, dialog->content()->listItem(), dialog->content()->inputString().c_str());
	}

	dialog->hide();
}

/* DialogButtonPanel::DialogButton2 */

DialogButtonPanel::DialogButton2::DialogButton2()
	: Button("", UISettings::fontSize() / 2)
{

}

void DialogButtonPanel::DialogButton2::draw(ImGuiRenderer* renderer)
{
	if (caption().empty()) return;

	ImVec2 pos = absolutePosition();
	ImVec2 sz = size();
	ImVec2 maxPos = pos + sz;
	float radius = sz.y * 0.35f;
	if (radius > 12.0f) radius = 12.0f;

	if (focused())
	{
		renderer->drawRectFilledMulticolor(pos, maxPos,
			ImColor(20, 48, 78, 230), ImColor(16, 38, 65, 230),
			ImColor(10, 26, 48, 230), ImColor(16, 38, 65, 230));
		renderer->drawRect(pos, maxPos, ImColor(0, 210, 255, 230), false, radius, 1.2f);
	}
	else
	{
		renderer->drawRectFilledMulticolor(pos, maxPos,
			ImColor(12, 22, 35, 200), ImColor(9, 18, 30, 200),
			ImColor(6, 12, 20, 220), ImColor(8, 16, 26, 200));
		renderer->drawRect(pos, maxPos, ImColor(0, 175, 225, 160), false, radius, 1.0f);
	}

	Widget::draw(renderer);
}

void DialogButtonPanel::DialogButton2::touchPopEvent()
{
	Dialog* dialog = dynamic_cast<Dialog*>(this->parent()->parent());

	if (pNetGame) {
		pNetGame->SendDialogResponse(dialog->dialogID(), 0, dialog->content()->listItem(), dialog->content()->inputString().c_str());
	}

	dialog->hide();
}