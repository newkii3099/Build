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

	// AAA Cyber Dark Obsidian Input Field
	renderer->drawRect(pMin, pMax, ImColor(8, 16, 28, 220), true, 4.0f);
	// Glowing Electric Cyan Border
	renderer->drawRect(pMin, pMax, ImColor(0, 190, 245, 175), false, 4.0f, 1.2f);
	// Left accent marker
	renderer->drawRect(pMin + ImVec2(1.0f, 2.0f), ImVec2(pMin.x + 3.0f, pMax.y - 2.0f), ImColor(0, 235, 255, 230), true, 1.0f);

	Widget::draw(renderer);
}

void EditBox::touchPopEvent()
{
	pUI->keyboard()->show(this);
}

void EditBox::keyboardEvent(const std::string& input)
{
	m_input = input;
	m_label->setText(Encoding::cp2utf(input));
}