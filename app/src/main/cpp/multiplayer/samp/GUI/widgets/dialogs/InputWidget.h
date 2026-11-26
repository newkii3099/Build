#pragma once

class InputWidget : public Widget
{
public:
	InputWidget();
	virtual void performLayout() override;

	void setMessage(const std::string& message);
	void setPassword(bool isPassword);
	const std::string& inputString() const;

private:
	MsgBoxWidget* m_msgbox;
	EditBox* m_editbox;
};