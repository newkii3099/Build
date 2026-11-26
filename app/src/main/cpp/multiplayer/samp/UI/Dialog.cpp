#include <algorithm>
#include "samp/main.h"
#include "samp/GUI/gui.h"
#include "samp/Multiplayer/Multiplayer.h"
#include "samp/Network/Network.h"

extern CNetGame* pNetGame;
extern UI* pUI;

extern int Button2Show;

static void drawModernDialogButton(ImGuiRenderer* renderer, const ImVec2& pos, const ImVec2& sz, bool isFocused)
{
	ImVec2 maxPos = pos + sz;
	float radius = 6.0f;
	ImColor bgCol = isFocused ? ImColor(65, 68, 78, 240) : ImColor(40, 42, 49, 228);
	ImColor borderCol = isFocused ? ImColor(245, 166, 35, 220) : ImColor(255, 255, 255, 28);
	renderer->drawRect(pos, maxPos, bgCol, true, radius);
	renderer->drawRect(pos, maxPos, borderCol, false, radius, 1.0f);
}

/* Dialog */

Dialog::Dialog()
{
	m_title = new DialogTitle();
	this->addChild(m_title);

	m_content = new Content();
	this->addChild(m_content);

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

		m_content->setActive(dialog_style, message);

		this->performLayout();
		this->setVisible(true);

		m_dialogID = dialog_id;
	}
}

void Dialog::hide()
{
	this->setVisible(false);
	m_dialogID = -1;
	if (pUI && pUI->keyboard() && pUI->keyboard()->visible()) {
		pUI->keyboard()->hide();
	}
}

void Dialog::confirm()
{
	if (m_dialogID < 0) return;
	if (pNetGame) {
		pNetGame->SendDialogResponse(m_dialogID, 1, m_content->listItem(), m_content->inputString().c_str());
	}
	hide();
}

void Dialog::drawSelectionHighlight(ImGuiRenderer* renderer, const ImVec2& pMin, const ImVec2& pMax)
{
	ImVec2 padMin(pMin.x + 2.0f, pMin.y + 1.0f);
	ImVec2 padMax(pMax.x - 2.0f, pMax.y - 1.0f);
	// Amber / Gold selected row bar matching reference UI (#F5A623)
	renderer->drawRect(padMin, padMax, ImColor(245, 166, 35, 255), true, 4.0f);
}

void Dialog::handleListRowTap(Widget* panelParent, bool isSelected, bool selectable)
{
	if (!selectable || !isSelected) return;
	static Widget* s_lastPanel = nullptr;
	static int s_lastRow = -1;
	static uint32_t s_lastTick = 0;

	ListBox::Panel* p = dynamic_cast<ListBox::Panel*>(panelParent);
	int curIdx = p ? p->activeItemIndex() : -1;
	if (curIdx < 0) return;

	uint32_t now = GetTickCount();
	bool isDoubleTap = (s_lastPanel == panelParent && s_lastRow == curIdx && (now - s_lastTick) <= 450);
	s_lastPanel = panelParent;
	s_lastRow = curIdx;
	s_lastTick = now;

	if (isDoubleTap && pUI && pUI->dialog()) {
		s_lastRow = -1;
		pUI->dialog()->confirm();
	}
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
	m_title->performLayout();

	m_content->setMinSize(ImVec2(
		this->minSize().x - (UISettings::padding() + UISettings::padding()),
		this->minSize().y - (UISettings::dialogTitleHeight() + UISettings::dialogButtonPanelSize().y)
	));
	m_content->setMaxSize(ImVec2(
		this->maxSize().x - (UISettings::padding() + UISettings::padding()),
		this->maxSize().y - (UISettings::dialogTitleHeight() + UISettings::dialogButtonPanelSize().y + UISettings::padding() + UISettings::padding())
	));
	m_content->performLayout();

	float minNeededW = std::max(m_title->minRequiredWidth(), UISettings::dialogButtonPanelSize().x);
	float contentW = std::max(m_content->width(), minNeededW);
	contentW = std::min(contentW, this->maxSize().x - (UISettings::padding() + UISettings::padding()));

	ImVec2 dialog_size = ImVec2(
		contentW + (UISettings::padding() + UISettings::padding()),
		UISettings::dialogTitleHeight() + m_content->height() + UISettings::dialogButtonPanelSize().y + UISettings::padding() + UISettings::padding()
	);

	this->setSize(dialog_size);

	m_title->setSize(ImVec2(dialog_size.x, UISettings::dialogTitleHeight()));
	m_title->performLayout();

	m_buttonPanel->setFixedSize(ImVec2(
		std::min(dialog_size.x - UISettings::padding(), UISettings::dialogButtonPanelSize().x),
		UISettings::dialogButtonPanelSize().y));
	m_buttonPanel->performLayout();

	m_title->setPosition(ImVec2(0.0f, 0.0f));
	m_content->setPosition(ImVec2(
		(dialog_size.x - m_content->width()) * 0.5f,
		m_title->position().y + m_title->height() + UISettings::padding() * 0.6f));
	m_buttonPanel->setPosition(ImVec2(
		(dialog_size.x - m_buttonPanel->width()) * 0.5f,
		(dialog_size.y - m_buttonPanel->height())));

	this->setPosition(ImVec2(
		(parent()->width() - width()) * 0.5f,
		(parent()->height() - height()) * 0.5f));
}

void Dialog::draw(ImGuiRenderer* renderer)
{
	ImVec2 pos = absolutePosition();
	ImVec2 sz = size();
	ImVec2 maxPos = pos + sz;
	float rounding = 10.0f;

	// Dark translucent glassmorphic panel matching reference dialog
	renderer->drawRect(pos, maxPos, ImColor(14, 15, 19, 228), true, rounding);
	renderer->drawRect(pos, maxPos, ImColor(255, 255, 255, 28), false, rounding, 1.0f);

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
	m_label = new Label("Title", ImColor(242, 245, 250, 255), false, UISettings::fontSize() * 0.54f);
	this->addChild(m_label);
}

void DialogTitle::setTitle(const std::string& caption)
{
	m_label->setText(caption);
}

void DialogTitle::performLayout()
{
	m_label->performLayout();
	m_label->setPosition(ImVec2(
		std::max(UISettings::padding(), (width() - m_label->width()) * 0.5f),
		(height() - m_label->height()) * 0.5f));
}

void DialogTitle::draw(ImGuiRenderer* renderer)
{
	ImVec2 pos = absolutePosition();
	ImVec2 sz = size();
	ImVec2 maxPos = pos + sz;

	renderer->drawLine(
		ImVec2(pos.x + 12.0f, maxPos.y - 1.0f),
		ImVec2(maxPos.x - 12.0f, maxPos.y - 1.0f),
		ImColor(255, 255, 255, 22), 1.0f);

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
	float padY = UISettings::padding() * 0.45f;
	float btnH = std::max(18.0f, height() - padY * 2.0f);
	float gap = UISettings::padding() * 0.8f;

	if (!m_button2->visible() || m_button2->caption().empty()) {
		float btnW = std::min(width() * 0.58f, width() - UISettings::padding());
		m_button1->setFixedSize(ImVec2(btnW, btnH));
		m_button1->performLayout();
		m_button1->setPosition(ImVec2((width() - btnW) * 0.5f, (height() - btnH) * 0.5f));
	} else {
		float btnW = (width() - gap - UISettings::padding()) * 0.5f;
		float totalW = btnW * 2.0f + gap;
		float startX = (width() - totalW) * 0.5f;
		float posY = (height() - btnH) * 0.5f;

		m_button1->setFixedSize(ImVec2(btnW, btnH));
		m_button1->performLayout();
		m_button1->setPosition(ImVec2(startX, posY));

		m_button2->setFixedSize(ImVec2(btnW, btnH));
		m_button2->performLayout();
		m_button2->setPosition(ImVec2(startX + btnW + gap, posY));
	}
}

/* DialogButtonPanel::DialogButton1 */

DialogButtonPanel::DialogButton1::DialogButton1()
	: Button("", UISettings::fontSize() * 0.50f)
{

}

void DialogButtonPanel::DialogButton1::draw(ImGuiRenderer* renderer)
{
	drawModernDialogButton(renderer, absolutePosition(), size(), focused());
	Widget::draw(renderer);
}

void DialogButtonPanel::DialogButton1::touchPopEvent()
{
	Dialog* dialog = dynamic_cast<Dialog*>(this->parent()->parent());
	if (dialog) dialog->confirm();
}

/* DialogButtonPanel::DialogButton2 */

DialogButtonPanel::DialogButton2::DialogButton2()
	: Button("", UISettings::fontSize() * 0.50f)
{

}

void DialogButtonPanel::DialogButton2::draw(ImGuiRenderer* renderer)
{
	if (caption().empty()) return;
	drawModernDialogButton(renderer, absolutePosition(), size(), focused());
	Widget::draw(renderer);
}

void DialogButtonPanel::DialogButton2::touchPopEvent()
{
	Dialog* dialog = dynamic_cast<Dialog*>(this->parent()->parent());
	if (!dialog) return;

	if (pNetGame) {
		pNetGame->SendDialogResponse(dialog->dialogID(), 0, dialog->content()->listItem(), dialog->content()->inputString().c_str());
	}

	dialog->hide();
}
