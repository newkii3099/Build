#pragma once

class EditBox : public Widget
{
public:
	EditBox();

	virtual void performLayout() override;
	virtual void touchPopEvent() override;
	virtual void draw(ImGuiRenderer* renderer) override;
	virtual void keyboardEvent(const std::string& input) override;

	void setPassword(bool isPassword) { m_isPassword = isPassword; }
	const std::string& text() const { return m_input; }

private:
	Label* m_label;
	std::string m_input;
	bool m_isPassword = false;
};