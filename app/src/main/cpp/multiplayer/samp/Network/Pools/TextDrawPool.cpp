#include "samp/main.h"
#include "samp/Multiplayer/Multiplayer.h"
#include "samp/Network/Network.h"
#include "samp/Network/Pools/TextDrawPool.h"
#include "samp/Multiplayer/Util.h"
#include "samp/GUI/gui.h"
#include <algorithm>

extern CNetGame* pNetGame;
extern CGame* pGame;
extern UI* pUI;

CTextDrawPool::CTextDrawPool()
{
	for (int i = 0; i < MAX_TEXT_DRAWS; i++) {
		m_pTextDraw[i] = nullptr;
		m_bSlotState[i] = 0;
	}

	ResetTextDrawTextures();

	m_bSelectState = false;
	m_dwHoverColor = 0;
	m_wClickedTextDrawID = 0xFFFF;
}

CTextDrawPool::~CTextDrawPool()
{
	for (int i = 0; i < MAX_TEXT_DRAWS; i++) {
		Delete(i);
	}
}

void CTextDrawPool::New(uint16_t wTextDrawID, TEXT_DRAW_TRANSMIT* pTextDrawTransmit, const char* szText)
{
	if (wTextDrawID >= MAX_TEXT_DRAWS) return;

	if (m_pTextDraw[wTextDrawID]) {
		Delete(wTextDrawID);
	}

	CTextDraw* pTextDraw = new CTextDraw(pTextDrawTransmit, szText);
	if (pTextDraw == nullptr) return;

	m_pTextDraw[wTextDrawID] = pTextDraw;
	m_bSlotState[wTextDrawID] = 1;
}

void CTextDrawPool::Delete(uint16_t wTextDrawID)
{
	if (wTextDrawID >= MAX_TEXT_DRAWS) return;

	if (m_pTextDraw[wTextDrawID]) {
		delete m_pTextDraw[wTextDrawID];
		m_pTextDraw[wTextDrawID] = nullptr;
		m_bSlotState[wTextDrawID] = 0;
	}
}

void CTextDrawPool::Draw()
{
	for (int i = 0; i < MAX_TEXT_DRAWS; i++) {
		if (m_bSlotState[i] && m_pTextDraw[i]) {
			m_pTextDraw[i]->Draw();
		}
	}
}

void CTextDrawPool::SetSelectState(bool bState, uint32_t dwColor)
{
	if (bState) {
		m_bSelectState = true;
		m_dwHoverColor = dwColor;
		m_wClickedTextDrawID = 0xFFFF;
		if (pGame) {
			pGame->DisplayHUD(false);
			if (pGame->FindPlayerPed()) pGame->FindPlayerPed()->TogglePlayerControllable(false);
		}
	}
	else {
		m_bSelectState = false;
		m_dwHoverColor = 0;
		m_wClickedTextDrawID = 0xFFFF;
		if (pGame) {
			pGame->DisplayHUD(true);
			if (pGame->FindPlayerPed()) pGame->FindPlayerPed()->TogglePlayerControllable(true);
		}

		SendClick(0xFFFF);

		for (int i = 0; i < MAX_TEXT_DRAWS; i++) {
			if (m_bSlotState[i] && m_pTextDraw[i]) {
				CTextDraw* pTextDraw = m_pTextDraw[i];
				pTextDraw->m_bHovered = false;
				pTextDraw->m_dwHoverColor = 0;
			}
		}
	}
}

void CTextDrawPool::SendClick(uint16_t wTextDrawID)
{
	if (!pNetGame || !pNetGame->GetRakClient()) return;

	RakNet::BitStream bsClick;
	bsClick.Write(wTextDrawID);
	pNetGame->GetRakClient()->RPC(&RPC_ClickTextDraw, &bsClick, HIGH_PRIORITY, RELIABLE_ORDERED, 0, false, UNASSIGNED_NETWORK_ID, nullptr);
}

bool CTextDrawPool::onTouchEvent(int type, int num, int x, int y)
{
	if (!m_bSelectState) return true;

	m_wClickedTextDrawID = 0xFFFF;

	for (int i = MAX_TEXT_DRAWS - 1; i >= 0; i--) {
		if (!m_bSlotState[i] || !m_pTextDraw[i]) continue;

		CTextDraw* pTextDraw = m_pTextDraw[i];
		pTextDraw->m_bHovered = false;
		pTextDraw->m_dwHoverColor = 0;

		if (pTextDraw->m_TextDrawData.byteSelectable && pTextDraw->m_TextDrawData.bHasRectArea) {
			CRect* rect = &pTextDraw->m_rectArea;
			float left = std::min(rect->left, rect->right);
			float right = std::max(rect->left, rect->right);
			float top = std::min(rect->top, rect->bottom);
			float bottom = std::max(rect->top, rect->bottom);

			if (x >= left && x <= right && y >= top && y <= bottom) {
				switch (type) {
					case 2: // push
					case 3: // move
						m_wClickedTextDrawID = 0xFFFF;
						pTextDraw->m_bHovered = true;
						pTextDraw->m_dwHoverColor = m_dwHoverColor;
						break;

					case 1: // pop
						m_wClickedTextDrawID = (uint16_t)i;
						if (pUI) pUI->PushToBufferedQueueTextDrawPressed((uint16_t)i);
						else SendClick((uint16_t)i);
						pTextDraw->m_bHovered = false;
						pTextDraw->m_dwHoverColor = 0;
						break;
				}
				return false;
			}
		}
	}

	return false; // Consume touch event while in SelectTextDraw mode
}

void CTextDrawPool::SnapshotProcess()
{
	for (int i = 0; i < MAX_TEXT_DRAWS; i++) {
		if (m_bSlotState[i] && m_pTextDraw[i]) {
			m_pTextDraw[i]->SnapshotProcess();
		}
	}
}
