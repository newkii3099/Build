#include "samp/GUI/gui.h"
#include "samp/main.h"

SplashScreen::SplashScreen()
{
	// Load logo from samp database (mylogo or defaultlogo)
	m_logo = new Image("samp", "mylogo");
	if (!m_logo || !m_logo->getTexture()) {
		if (m_logo) delete m_logo;
		m_logo = new Image("samp", "defaultlogo");
	}
	if (!m_logo || !m_logo->getTexture()) {
		if (m_logo) delete m_logo;
		m_logo = nullptr;
	}

	if (m_logo) {
		this->addChild(m_logo);
	}

	m_progressBar = new ProgressBar(ImColor(0.0f, 0.0f, 0.0f),
		ImColor(190, 0, 0), ImColor(255, 100, 0),
		ImColor(255, 100, 0), ImColor(190, 0, 0));
	if (m_progressBar) {
		this->addChild(m_progressBar);
	}
}

void SplashScreen::performLayout()
{
	if (m_logo) {
		m_logo->setFixedSize(UISettings::splashscreenLogoSize());
		m_logo->setPosition(UISettings::splashscreenLogoPos());
	}

	if (m_progressBar) {
		m_progressBar->setFixedSize(UISettings::splashscreenPBarSize());
		m_progressBar->setPosition(UISettings::splashscreenPBarPos());
	}
}

void SplashScreen::draw(ImGuiRenderer* renderer)
{
	if (!renderer) return;
	// background
	renderer->drawRect(absolutePosition(), absolutePosition() + size(), ImColor(0.0f, 0.0f, 0.0f), true);
	// logo
	if (m_logo) {
		m_logo->draw(renderer);
	}
	// progress bar
	if (m_progressBar) {
		m_progressBar->draw(renderer);
	}
}