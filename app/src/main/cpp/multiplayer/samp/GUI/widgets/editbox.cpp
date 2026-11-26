#include "samp/GUI/gui.h"

extern UI* pUI;

EditBox::EditBox()
{
	m_label = new Label(" ", ImColor(1.0f, 1.0f, 1.0f), false, UISettings::fontSize() / 2);//...
	this->addChild(m_label);
}

void EditBox::performLayout()
{
	m_label->performLayout();

	m_label->setPosition(ImVec2(
		UISettings::padding(),

		(height() - m_label->height()) / 2
	));
}

void EditBox::draw(ImGuiRenderer* renderer)
{
	ImVec2 pMin = absolutePosition() + ImVec2(UISettings::outlineSize(), UISettings::outlineSize());
	ImVec2 pMax = (absolutePosition() + size()) - ImVec2(UISettings::outlineSize(), UISettings::outlineSize());

	// Clean dark translucent input field matching modern dialog theme
	renderer->drawRect(pMin, pMax, ImColor(10, 11, 15, 232), true, 5.0f);
	ImColor borderCol = focused() ? ImColor(245, 166, 35, 220) : ImColor(255, 255, 255, 40);
	renderer->drawRect(pMin, pMax, borderCol, false, 5.0f, 1.1f);

	Widget::draw(renderer);
}

void EditBox::touchPopEvent()
{
	pUI->keyboard()->show(this);
}

void EditBox::keyboardEvent(const std::string& input)
{
	m_input = input;
	if (m_isPassword) {
		std::string utf = Encoding::cp2utf(input);
		size_t chars = 0;
		for (unsigned char c : utf) {
			if ((c & 0xC0) != 0x80) chars++;
		}
		m_label->setText(std::string(chars, '*'));
	} else {
		m_label->setText(Encoding::cp2utf(input));
	}
}