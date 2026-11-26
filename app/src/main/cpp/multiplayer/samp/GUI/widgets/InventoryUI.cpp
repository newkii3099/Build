#include "samp/main.h"
#include "samp/GUI/gui.h"
#include "samp/GUI/widgets/InventoryUI.h"
#include "samp/Network/Network.h"
#include "samp/Settings/Settings.h"
#include "samp/Multiplayer/Multiplayer.h"
#include "gta-reversed/game_sa/Models/ModelInfo.h"
#include "gta-reversed/game_sa/VisibilityPlugins.h"
#include "gta-reversed/game_sa/Camera.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

extern CNetGame* pNetGame;
extern CGame* pGame;
extern CSettings* pSettings;
extern CSnapShotHelper* pSnapShotHelper;
extern UI* pUI;
extern uintptr_t g_libGTASA;

InventoryUI::InventoryUI()
{
	setVisible(false);
	for (int i = 0; i < 7; i++) {
		m_hotbar[i].hotbarIdx = i;
		m_pendingHotbar[i].hotbarIdx = i;
	}
}

InventoryUI::~InventoryUI()
{
	clearSnapshotCache();
}

void InventoryUI::clearSnapshotCache()
{
	for (auto& kv : m_snapshotCache) {
		if (kv.second) {
			RwTextureDestroy(kv.second);
		}
	}
	m_snapshotCache.clear();
}

void InventoryUI::SetServerUIVersion(int version)
{
	m_serverUIVersion = (version == 1 || version == 2) ? version : 0;
	if (m_serverUIVersion == 1 && visible()) {
		Hide(false);
	}
}

int InventoryUI::GetEffectiveUIVersion() const
{
	if (m_serverUIVersion == 1 || m_serverUIVersion == 2) {
		return m_serverUIVersion;
	}
	if (pSettings) {
		return (pSettings->Get().iUIVersion == 1) ? 1 : 2;
	}
	return 2;
}

void InventoryUI::BeginSync(float usedWeight, float maxWeight, int usedSlots, int maxSlots)
{
	m_pendingItems.clear();
	m_usedWeight = usedWeight;
	m_maxWeight = (maxWeight > 0.0f) ? maxWeight : 80.0f;
	m_usedSlots = usedSlots;
	m_maxSlots = (maxSlots > 0) ? maxSlots : 30;
	for (int i = 0; i < 7; i++) {
		m_pendingHotbar[i] = InvHotbarSlot();
		m_pendingHotbar[i].hotbarIdx = i;
	}
}

void InventoryUI::AddSyncSlot(int slot, int itemId, int modelId, int amount, int maxStack,
	float weight, int category, bool favorite,
	float rotX, float rotY, float rotZ, float zoom, const std::string& nameUtf8)
{
	if (slot < 0 || itemId <= 0 || amount <= 0) return;

	InvSlotItem item;
	item.slot = slot;
	item.itemId = itemId;
	item.modelId = modelId;
	item.amount = amount;
	item.maxStack = (maxStack > 0) ? maxStack : 100;
	item.weight = weight;
	item.category = (category >= 1 && category <= 4) ? category : 4;
	item.favorite = favorite;
	item.vecRot = { rotX, rotY, rotZ };
	item.zoom = (zoom > 0.05f) ? zoom : 1.0f;
	item.nameUtf8 = nameUtf8;

	auto it = m_snapshotCache.find(modelId);
	if (it != m_snapshotCache.end() && it->second) {
		item.texture = it->second;
		item.isSnapshot = true;
	}

	m_pendingItems.push_back(std::move(item));
}

void InventoryUI::SetSyncHotbar(int hslot, int itemId, int modelId, int amount, const std::string& nameUtf8)
{
	if (hslot < 0 || hslot >= 7) return;
	InvHotbarSlot& hb = m_pendingHotbar[hslot];
	hb.hotbarIdx = hslot;
	hb.itemId = itemId;
	hb.modelId = modelId;
	hb.amount = amount;
	hb.nameUtf8 = nameUtf8;

	if (modelId > 0) {
		auto it = m_snapshotCache.find(modelId);
		if (it != m_snapshotCache.end() && it->second) {
			hb.texture = it->second;
			hb.isSnapshot = true;
		}
	}
}

void InventoryUI::FinishSyncAndShow()
{
	if (!IsMode20()) return;

	m_items = std::move(m_pendingItems);
	for (int i = 0; i < 7; i++) {
		m_hotbar[i] = m_pendingHotbar[i];
	}
	Show();
}

void InventoryUI::CloseFromServer()
{
	Hide(false);
}

void InventoryUI::Show()
{
	if (!IsMode20()) return;
	performLayout();
	setVisible(true);
	if (pGame) {
		pGame->DisplayHUD(false);
		if (pGame->FindPlayerPed()) {
			pGame->FindPlayerPed()->TogglePlayerControllable(false);
		}
	}
}

void InventoryUI::Hide(bool notifyServer)
{
	bool wasVisible = visible();
	setVisible(false);
	m_touchDown = false;
	m_draggingGrid = false;

	if (wasVisible && pGame) {
		pGame->DisplayHUD(true);
		if (pGame->FindPlayerPed()) {
			pGame->FindPlayerPed()->TogglePlayerControllable(true);
		}
	}

	if (notifyServer && wasVisible && pNetGame && pNetGame->GetGameState() == GAMESTATE_CONNECTED) {
		pNetGame->SendChatCommand("/_invclose");
	}
}

void InventoryUI::performLayout()
{
	ImVec2 disp = pUI ? pUI->displaySize() : ImVec2(1920.0f, 1080.0f);
	setPosition(ImVec2(0.0f, 0.0f));
	setFixedSize(disp);

	float panelW = disp.x * 0.78f;
	float panelH = disp.y * (m_showHotbar ? 0.76f : 0.67f);
	float left = (disp.x - panelW) * 0.5f;
	float top = disp.y * 0.09f;

	m_panelMin = ImVec2(left, top);
	m_panelMax = ImVec2(left + panelW, top + panelH);
}

RwTexture* InventoryUI::getOrCreateModelSnapshot(int modelId, const CVector& rot, float zoom)
{
	if (modelId <= 0) return nullptr;

	auto it = m_snapshotCache.find(modelId);
	if (it != m_snapshotCache.end()) {
		return it->second;
	}

	if (!pSnapShotHelper) return nullptr;

	uint16_t safeModel = (uint16_t)modelId;
	RwTexture* snap = nullptr;
	uint32_t bgTransparent = 0x00000000;
	CVector useRot = rot;

	if (IsValidPedModel(safeModel)) {
		snap = pSnapShotHelper->CreatePedSnapShot(safeModel, bgTransparent, &useRot, zoom);
	} else if (safeModel >= 400 && safeModel <= 611) {
		snap = pSnapShotHelper->CreateVehicleSnapShot(safeModel, bgTransparent, &useRot, zoom, 1, 1);
	} else {
		if (safeModel >= CModelInfo::NUM_MODEL_INFOS || !CModelInfo::GetModelInfo(safeModel)) {
			safeModel = 18631;
		}
		snap = pSnapShotHelper->CreateObjectSnapShot(safeModel, bgTransparent, &useRot, zoom);
	}

	if (snap) {
		m_snapshotCache[modelId] = snap;
	}

	CCamera* pTheCamera = reinterpret_cast<CCamera*>(g_libGTASA + 0x9F86F8);
	if (pTheCamera && pTheCamera->m_pRwCamera) {
		CVisibilityPlugins::SetRenderWareCamera(pTheCamera->m_pRwCamera);
	}

	return snap;
}

void InventoryUI::ProcessSnapshots()
{
	if (!visible() || !pSnapShotHelper) return;

	int created = 0;
	for (auto& item : m_items) {
		if (item.texture || item.snapAttempts >= 2 || item.modelId <= 0) continue;
		item.snapAttempts++;
		RwTexture* tex = getOrCreateModelSnapshot(item.modelId, item.vecRot, item.zoom);
		if (tex) {
			item.texture = tex;
			item.isSnapshot = true;
		}
		if (++created >= 4) return;
	}

	for (int i = 0; i < 7; i++) {
		InvHotbarSlot& hb = m_hotbar[i];
		if (hb.itemId <= 0 || hb.texture || hb.snapAttempts >= 2 || hb.modelId <= 0) continue;
		hb.snapAttempts++;
		CVector defaultRot = { 0.0f, 0.0f, 0.0f };
		RwTexture* tex = getOrCreateModelSnapshot(hb.modelId, defaultRot, 1.0f);
		if (tex) {
			hb.texture = tex;
			hb.isSnapshot = true;
		}
		if (++created >= 4) return;
	}
}

bool InventoryUI::isItemInHotbar(int itemId) const
{
	if (itemId <= 0) return false;
	for (int i = 0; i < 7; i++) {
		if (m_hotbar[i].itemId == itemId) return true;
	}
	return false;
}

std::vector<const InvSlotItem*> InventoryUI::getFilteredItems() const
{
	std::vector<const InvSlotItem*> out;
	out.reserve(m_items.size());
	for (const auto& item : m_items) {
		if (m_activeCategory == 0 || item.category == m_activeCategory) {
			out.push_back(&item);
		}
	}
	return out;
}

void InventoryUI::drawStar(ImGuiRenderer* renderer, const ImVec2& center, float r, const ImColor& col)
{
	ImVec2 outer[5];
	ImVec2 inner[5];
	const float pi = 3.14159265f;
	for (int i = 0; i < 5; i++) {
		float aOut = -pi * 0.5f + i * (2.0f * pi / 5.0f);
		float aIn = aOut + (pi / 5.0f);
		outer[i] = ImVec2(center.x + std::cos(aOut) * r, center.y + std::sin(aOut) * r);
		inner[i] = ImVec2(center.x + std::cos(aIn) * (r * 0.44f), center.y + std::sin(aIn) * (r * 0.44f));
	}
	for (int i = 0; i < 5; i++) {
		int prev = (i + 4) % 5;
		renderer->drawTriangle(center, outer[i], inner[i], col, true);
		renderer->drawTriangle(center, inner[prev], outer[i], col, true);
	}
}

void InventoryUI::drawCategoryIcon(ImGuiRenderer* renderer, int tabIndex, const ImVec2& center, float r, const ImColor& col)
{
	switch (tabIndex) {
		case 0: { // 4-square grid (All)
			float g = r * 0.16f;
			float s = r * 0.48f;
			renderer->drawRect(ImVec2(center.x - g - s, center.y - g - s), ImVec2(center.x - g, center.y - g), col, false, 1.5f, 1.6f);
			renderer->drawRect(ImVec2(center.x + g, center.y - g - s), ImVec2(center.x + g + s, center.y - g), col, false, 1.5f, 1.6f);
			renderer->drawRect(ImVec2(center.x - g - s, center.y + g), ImVec2(center.x - g, center.y + g + s), col, false, 1.5f, 1.6f);
			renderer->drawRect(ImVec2(center.x + g, center.y + g), ImVec2(center.x + g + s, center.y + g + s), col, false, 1.5f, 1.6f);
			break;
		}
		case 1: { // Potion / Food flask
			renderer->drawRect(ImVec2(center.x - r * 0.22f, center.y - r * 0.72f), ImVec2(center.x + r * 0.22f, center.y - r * 0.36f), col, false, 1.0f, 1.6f);
			renderer->drawTriangle(
				ImVec2(center.x, center.y - r * 0.42f),
				ImVec2(center.x - r * 0.58f, center.y + r * 0.62f),
				ImVec2(center.x + r * 0.58f, center.y + r * 0.62f),
				col, false, 1.6f);
			break;
		}
		case 2: { // Crystal / Materials
			ImVec2 pts[4] = {
				ImVec2(center.x, center.y - r * 0.78f),
				ImVec2(center.x + r * 0.56f, center.y),
				ImVec2(center.x, center.y + r * 0.78f),
				ImVec2(center.x - r * 0.56f, center.y)
			};
			renderer->drawLine(pts[0], pts[1], col, 1.7f);
			renderer->drawLine(pts[1], pts[2], col, 1.7f);
			renderer->drawLine(pts[2], pts[3], col, 1.7f);
			renderer->drawLine(pts[3], pts[0], col, 1.7f);
			renderer->drawLine(pts[0], pts[2], col, 1.1f);
			break;
		}
		case 3: { // Shield / Equipment
			ImVec2 pts[5] = {
				ImVec2(center.x - r * 0.54f, center.y - r * 0.62f),
				ImVec2(center.x + r * 0.54f, center.y - r * 0.62f),
				ImVec2(center.x + r * 0.54f, center.y + r * 0.08f),
				ImVec2(center.x, center.y + r * 0.74f),
				ImVec2(center.x - r * 0.54f, center.y + r * 0.08f)
			};
			for (int i = 0; i < 5; i++) {
				renderer->drawLine(pts[i], pts[(i + 1) % 5], col, 1.7f);
			}
			break;
		}
		case 4: { // Misc / Bag tag circle
			renderer->drawRect(ImVec2(center.x - r * 0.55f, center.y - r * 0.55f), ImVec2(center.x + r * 0.55f, center.y + r * 0.55f), col, false, r * 0.55f, 1.6f);
			renderer->drawRect(ImVec2(center.x - r * 0.18f, center.y - r * 0.18f), ImVec2(center.x + r * 0.18f, center.y + r * 0.18f), col, true, r * 0.18f);
			break;
		}
	}
}

void InventoryUI::drawHeader(ImGuiRenderer* renderer, const ImVec2& panelMin, const ImVec2& panelMax)
{
	float scaleY = UISettings::scaleFactor().y;
	float headerH = 28.0f * scaleY;
	float padX = 12.0f * UISettings::scaleFactor().x;
	float titleFontSz = UISettings::fontSize() * 0.58f;
	float weightFontSz = UISettings::fontSize() * 0.50f;

	// Subtle darker header strip
	ImVec2 hdrMax(panelMax.x, panelMin.y + headerH);
	renderer->drawRect(panelMin, hdrMax, ImColor(10, 12, 16, 195), true, 10.0f);
	renderer->drawLine(ImVec2(panelMin.x + 8.0f, hdrMax.y), ImVec2(panelMax.x - 8.0f, hdrMax.y), ImColor(255, 255, 255, 28), 1.0f);

	// Bag icon on top-left
	float iconSz = headerH * 0.44f;
	float iconX = panelMin.x + padX;
	float iconY = panelMin.y + (headerH - iconSz) * 0.5f + 1.0f;
	renderer->drawRect(ImVec2(iconX, iconY + iconSz * 0.25f), ImVec2(iconX + iconSz, iconY + iconSz), ImColor(240, 244, 250, 240), true, 2.5f);
	renderer->drawRect(ImVec2(iconX + iconSz * 0.25f, iconY), ImVec2(iconX + iconSz * 0.75f, iconY + iconSz * 0.35f), ImColor(240, 244, 250, 240), false, 2.0f, 1.6f);

	// Title: กระเป๋าส่วนตัว
	std::string titleStr = "กระเป๋าส่วนตัว";
	ImVec2 titleSz = renderer->calculateTextSize(titleStr, titleFontSz);
	float titleX = iconX + iconSz + 8.0f * UISettings::scaleFactor().x;
	float titleY = panelMin.y + (headerH - titleSz.y) * 0.5f;
	renderer->drawText(ImVec2(titleX, titleY), ImColor(245, 247, 250, 255), titleStr, false, titleFontSz);

	// Weight: [ 42.20 / 80.00 Kg. ]
	char weightBuf[64];
	snprintf(weightBuf, sizeof(weightBuf), "[ %.2f / %.2f Kg. ]", m_usedWeight, m_maxWeight);
	ImVec2 weightSz = renderer->calculateTextSize(weightBuf, weightFontSz);
	float weightX = titleX + titleSz.x + 10.0f * UISettings::scaleFactor().x;
	float weightY = panelMin.y + (headerH - weightSz.y) * 0.5f;
	renderer->drawText(ImVec2(weightX, weightY), ImColor(215, 220, 228, 225), weightBuf, false, weightFontSz);

	// Right side: [5 Category Icons] + [X Close]
	float btnSize = headerH * 0.78f;
	float gap = 6.0f * UISettings::scaleFactor().x;
	float rightCursor = panelMax.x - padX;

	// Close button X
	m_closeBtnRect.min = ImVec2(rightCursor - btnSize, panelMin.y + (headerH - btnSize) * 0.5f);
	m_closeBtnRect.max = ImVec2(rightCursor, m_closeBtnRect.min.y + btnSize);
	ImVec2 cCenter = (m_closeBtnRect.min + m_closeBtnRect.max) * 0.5f;
	float xr = btnSize * 0.28f;
	renderer->drawLine(ImVec2(cCenter.x - xr, cCenter.y - xr), ImVec2(cCenter.x + xr, cCenter.y + xr), ImColor(225, 230, 238, 240), 2.0f);
	renderer->drawLine(ImVec2(cCenter.x + xr, cCenter.y - xr), ImVec2(cCenter.x - xr, cCenter.y + xr), ImColor(225, 230, 238, 240), 2.0f);
	rightCursor = m_closeBtnRect.min.x - gap * 1.6f;

	// 5 Category Tabs (from right to left: 4..0)
	for (int t = 4; t >= 0; t--) {
		m_tabRects[t].min = ImVec2(rightCursor - btnSize, panelMin.y + (headerH - btnSize) * 0.5f);
		m_tabRects[t].max = ImVec2(rightCursor, m_tabRects[t].min.y + btnSize);

		bool active = (m_activeCategory == t);
		if (active) {
			renderer->drawRect(m_tabRects[t].min, m_tabRects[t].max, ImColor(255, 255, 255, 28), true, 5.0f);
			renderer->drawLine(
				ImVec2(m_tabRects[t].min.x + 4.0f, m_tabRects[t].max.y),
				ImVec2(m_tabRects[t].max.x - 4.0f, m_tabRects[t].max.y),
				ImColor(245, 166, 35, 255), 2.0f);
		}
		ImColor iconCol = active ? ImColor(255, 255, 255, 255) : ImColor(175, 182, 195, 200);
		drawCategoryIcon(renderer, t, (m_tabRects[t].min + m_tabRects[t].max) * 0.5f, btnSize * 0.34f, iconCol);
		rightCursor = m_tabRects[t].min.x - gap;
	}
}

void InventoryUI::drawGrid(ImGuiRenderer* renderer, const ImVec2& gridMin, const ImVec2& gridMax)
{
	m_cardHits.clear();
	std::vector<const InvSlotItem*> items = getFilteredItems();

	float scaleX = UISettings::scaleFactor().x;
	float scaleY = UISettings::scaleFactor().y;
	float pad = 8.0f * scaleX;
	float gap = 5.5f * scaleX;
	float availW = (gridMax.x - gridMin.x) - pad * 2.0f;

	const int cols = 8;
	float cardW = (availW - gap * (cols - 1)) / (float)cols;
	float cardH = cardW * 1.02f;
	float footerH = cardH * 0.26f;

	int totalItems = (int)items.size();
	int rows = (totalItems + cols - 1) / cols;
	float contentH = pad + rows * (cardH + gap);
	float viewH = gridMax.y - gridMin.y;
	m_maxScrollY = std::max(0.0f, contentH - viewH);
	if (m_scrollY < 0.0f) m_scrollY = 0.0f;
	if (m_scrollY > m_maxScrollY) m_scrollY = m_maxScrollY;

	renderer->pushClipRect(gridMin, gridMax, true);

	if (items.empty()) {
		float emptyFontSz = UISettings::fontSize() * 0.52f;
		std::string emptyMsg = "ไม่มีไอเทมในหมวดหมู่นี้";
		ImVec2 esz = renderer->calculateTextSize(emptyMsg, emptyFontSz);
		renderer->drawText(
			ImVec2(gridMin.x + ((gridMax.x - gridMin.x) - esz.x) * 0.5f, gridMin.y + (viewH - esz.y) * 0.5f),
			ImColor(170, 178, 190, 200), emptyMsg, false, emptyFontSz);
		renderer->popClipRect();
		return;
	}

	float countFontSz = std::max(10.0f, cardH * 0.16f);
	float nameFontSz = std::max(10.0f, cardH * 0.17f);

	for (int idx = 0; idx < totalItems; idx++) {
		const InvSlotItem* item = items[idx];
		int r = idx / cols;
		int c = idx % cols;

		float x0 = gridMin.x + pad + c * (cardW + gap);
		float y0 = gridMin.y + pad + r * (cardH + gap) - m_scrollY;
		float x1 = x0 + cardW;
		float y1 = y0 + cardH;

		if (y1 < gridMin.y || y0 > gridMax.y) continue;

		// Card body
		renderer->drawRect(ImVec2(x0, y0), ImVec2(x1, y1), ImColor(28, 31, 38, 224), true, 5.0f);
		renderer->drawRect(ImVec2(x0, y0), ImVec2(x1, y1), ImColor(255, 255, 255, 22), false, 5.0f, 1.0f);

		// Bottom dark footer strip for item name
		float footY = y1 - footerH;
		renderer->drawRect(ImVec2(x0, footY), ImVec2(x1, y1), ImColor(13, 15, 20, 240), true, 4.0f);

		// Top-left star badge (favorite or in hotbar)
		if (item->favorite || isItemInHotbar(item->itemId)) {
			drawStar(renderer, ImVec2(x0 + cardW * 0.13f, y0 + cardH * 0.13f), cardW * 0.075f, ImColor(245, 166, 35, 255));
		}

		// Top-right quantity badge
		char qtyBuf[16];
		snprintf(qtyBuf, sizeof(qtyBuf), "%d", item->amount);
		ImVec2 qsz = renderer->calculateTextSize(qtyBuf, countFontSz);
		renderer->drawText(
			ImVec2(x1 - qsz.x - 5.0f * scaleX, y0 + 3.0f * scaleY),
			ImColor(235, 238, 245, 235), qtyBuf, false, countFontSz);

		// Center 3D snapshot texture (RenderWare snapshot uses flipped V: (0,1)->(1,0))
		float imgPadX = cardW * 0.20f;
		float imgTop = y0 + cardH * 0.14f;
		float imgBot = footY - 2.0f * scaleY;
		float imgSide = std::min(cardW - imgPadX * 2.0f, imgBot - imgTop);
		ImVec2 imgMin(x0 + (cardW - imgSide) * 0.5f, imgTop + ((imgBot - imgTop) - imgSide) * 0.5f);
		ImVec2 imgMax(imgMin.x + imgSide, imgMin.y + imgSide);

		if (item->texture && item->texture->raster) {
			ImVec2 uv0 = item->isSnapshot ? ImVec2(0.0f, 1.0f) : ImVec2(0.0f, 0.0f);
			ImVec2 uv1 = item->isSnapshot ? ImVec2(1.0f, 0.0f) : ImVec2(1.0f, 1.0f);
			renderer->drawImage(imgMin, imgMax, (ImTextureID)item->texture->raster, uv0, uv1);
		} else {
			renderer->drawRect(imgMin, imgMax, ImColor(255, 255, 255, 16), true, 4.0f);
		}

		// Bottom item name (clipped cleanly inside card footer)
		ImVec2 nameSz = renderer->calculateTextSize(item->nameUtf8, nameFontSz);
		float nameX = (nameSz.x <= cardW - 6.0f)
			? (x0 + (cardW - nameSz.x) * 0.5f)
			: (x0 + 3.0f);
		float nameY = footY + (footerH - nameSz.y) * 0.5f;
		renderer->pushClipRect(ImVec2(x0 + 2.0f, footY), ImVec2(x1 - 2.0f, y1), true);
		renderer->drawText(ImVec2(nameX, nameY), ImColor(242, 245, 250, 255), item->nameUtf8, false, nameFontSz);
		renderer->popClipRect();

		CardHit hit;
		hit.rect.min = ImVec2(std::max(x0, gridMin.x), std::max(y0, gridMin.y));
		hit.rect.max = ImVec2(std::min(x1, gridMax.x), std::min(y1, gridMax.y));
		hit.slot = item->slot;
		m_cardHits.push_back(hit);
	}

	renderer->popClipRect();
}

void InventoryUI::drawHotbarDock(ImGuiRenderer* renderer, const ImVec2& panelMin, const ImVec2& panelMax)
{
	float scaleX = UISettings::scaleFactor().x;
	float scaleY = UISettings::scaleFactor().y;
	float toggleW = 105.0f * scaleX;
	float toggleH = 16.0f * scaleY;
	float centerX = (panelMin.x + panelMax.x) * 0.5f;

	if (m_showHotbar) {
		float dockH = 44.0f * scaleY;
		ImVec2 dockMin(panelMin.x + 8.0f * scaleX, panelMax.y - dockH - 4.0f * scaleY);
		ImVec2 dockMax(panelMax.x - 8.0f * scaleX, panelMax.y - 4.0f * scaleY);
		renderer->drawRect(dockMin, dockMax, ImColor(10, 12, 16, 225), true, 6.0f);
		renderer->drawRect(dockMin, dockMax, ImColor(245, 166, 35, 95), false, 6.0f, 1.0f);

		float slotSize = dockH - 6.0f * scaleY;
		float gap = 6.0f * scaleX;
		float totalW = 7.0f * slotSize + 6.0f * gap;
		float startX = centerX - totalW * 0.5f;
		float slotY = dockMin.y + 3.0f * scaleY;
		float smallSz = std::max(9.0f, slotSize * 0.24f);

		for (int i = 0; i < 7; i++) {
			float sx0 = startX + i * (slotSize + gap);
			m_hotbarSlotRects[i].min = ImVec2(sx0, slotY);
			m_hotbarSlotRects[i].max = ImVec2(sx0 + slotSize, slotY + slotSize);

			renderer->drawRect(m_hotbarSlotRects[i].min, m_hotbarSlotRects[i].max, ImColor(28, 31, 38, 235), true, 5.0f);
			renderer->drawRect(m_hotbarSlotRects[i].min, m_hotbarSlotRects[i].max, ImColor(255, 255, 255, 28), false, 5.0f, 1.0f);

			char numBuf[4];
			snprintf(numBuf, sizeof(numBuf), "%d", i + 1);
			renderer->drawText(ImVec2(sx0 + 3.0f * scaleX, slotY + 2.0f * scaleY), ImColor(245, 166, 35, 235), numBuf, false, smallSz);

			const InvHotbarSlot& hb = m_hotbar[i];
			if (hb.itemId > 0) {
				if (hb.texture && hb.texture->raster) {
					float pad = slotSize * 0.16f;
					ImVec2 uv0 = hb.isSnapshot ? ImVec2(0.0f, 1.0f) : ImVec2(0.0f, 0.0f);
					ImVec2 uv1 = hb.isSnapshot ? ImVec2(1.0f, 0.0f) : ImVec2(1.0f, 1.0f);
					renderer->drawImage(
						ImVec2(sx0 + pad, slotY + pad),
						ImVec2(sx0 + slotSize - pad, slotY + slotSize - pad),
						(ImTextureID)hb.texture->raster, uv0, uv1);
				}
				char cntBuf[16];
				snprintf(cntBuf, sizeof(cntBuf), "x%d", hb.amount);
				ImVec2 csz = renderer->calculateTextSize(cntBuf, smallSz);
				renderer->drawText(
					ImVec2(sx0 + slotSize - csz.x - 2.0f * scaleX, slotY + slotSize - csz.y - 1.0f * scaleY),
					ImColor(240, 244, 250, 240), cntBuf, false, smallSz);
			}
		}
	}

	// Toggle pill docked just under panel bottom edge
	m_hotbarToggleRect.min = ImVec2(centerX - toggleW * 0.5f, panelMax.y + 2.0f * scaleY);
	m_hotbarToggleRect.max = ImVec2(centerX + toggleW * 0.5f, panelMax.y + 2.0f * scaleY + toggleH);
	renderer->drawRect(m_hotbarToggleRect.min, m_hotbarToggleRect.max, ImColor(16, 18, 24, 225), true, toggleH * 0.5f);
	renderer->drawRect(m_hotbarToggleRect.min, m_hotbarToggleRect.max, ImColor(255, 255, 255, 38), false, toggleH * 0.5f, 1.0f);

	std::string label = m_showHotbar ? "ซ่อนคีย์ลัด 1-7 ▼" : "คีย์ลัด 1-7 ▲";
	float lblSz = UISettings::fontSize() * 0.42f;
	ImVec2 tsz = renderer->calculateTextSize(label, lblSz);
	renderer->drawText(
		ImVec2(centerX - tsz.x * 0.5f, m_hotbarToggleRect.min.y + (toggleH - tsz.y) * 0.5f),
		ImColor(245, 166, 35, 255), label, false, lblSz);
}

void InventoryUI::draw(ImGuiRenderer* renderer)
{
	if (!visible() || !IsMode20()) return;

	performLayout();

	float scaleY = UISettings::scaleFactor().y;
	float headerH = 28.0f * scaleY;
	float dockSpace = m_showHotbar ? (50.0f * scaleY) : (4.0f * scaleY);

	// 1. Main glassmorphic translucent container
	float rounding = 10.0f;
	renderer->drawRect(m_panelMin, m_panelMax, ImColor(15, 17, 22, 212), true, rounding);
	renderer->drawRect(m_panelMin, m_panelMax, ImColor(255, 255, 255, 26), false, rounding, 1.1f);

	// 2. Header bar
	drawHeader(renderer, m_panelMin, m_panelMax);

	// 3. Item Grid
	m_gridMin = ImVec2(m_panelMin.x + 2.0f, m_panelMin.y + headerH + 2.0f);
	m_gridMax = ImVec2(m_panelMax.x - 2.0f, m_panelMax.y - dockSpace);
	drawGrid(renderer, m_gridMin, m_gridMax);

	// 4. Collapsible Hotbar 1-7 + toggle pill
	drawHotbarDock(renderer, m_panelMin, m_panelMax);
}

void InventoryUI::touchEvent(const ImVec2& pos, TouchType type)
{
	if (!visible() || !IsMode20()) return;

	if (type == TouchType::push) {
		m_touchDown = true;
		m_touchStartPos = pos;
		m_lastTouchPos = pos;
		m_totalDragDist = 0.0f;
		m_draggingGrid = (pos.x >= m_gridMin.x && pos.x <= m_gridMax.x && pos.y >= m_gridMin.y && pos.y <= m_gridMax.y);
		return;
	}

	if (type == TouchType::move) {
		if (!m_touchDown) return;
		float dx = pos.x - m_lastTouchPos.x;
		float dy = pos.y - m_lastTouchPos.y;
		m_totalDragDist += std::sqrt(dx * dx + dy * dy);
		if (m_draggingGrid) {
			m_scrollY -= dy;
			if (m_scrollY < 0.0f) m_scrollY = 0.0f;
			if (m_scrollY > m_maxScrollY) m_scrollY = m_maxScrollY;
		}
		m_lastTouchPos = pos;
		return;
	}

	if (type == TouchType::pop) {
		if (!m_touchDown) return;
		m_touchDown = false;
		bool wasTap = (m_totalDragDist < 18.0f * UISettings::scaleFactor().x);
		m_draggingGrid = false;
		if (!wasTap) return;

		// 1. Hotbar toggle pill
		if (m_hotbarToggleRect.contains(pos)) {
			m_showHotbar = !m_showHotbar;
			performLayout();
			return;
		}

		// 2. Tap outside panel (and outside toggle pill) closes inventory
		if (pos.x < m_panelMin.x || pos.x > m_panelMax.x || pos.y < m_panelMin.y || pos.y > m_panelMax.y) {
			Hide(true);
			return;
		}

		// 3. Close [X] button
		if (m_closeBtnRect.contains(pos)) {
			Hide(true);
			return;
		}

		// 4. Category tabs
		for (int t = 0; t < 5; t++) {
			if (m_tabRects[t].contains(pos)) {
				m_activeCategory = t;
				m_scrollY = 0.0f;
				return;
			}
		}

		// 5. Hotbar slot tap (if expanded)
		if (m_showHotbar) {
			for (int h = 0; h < 7; h++) {
				if (m_hotbarSlotRects[h].contains(pos)) {
					if (m_hotbar[h].itemId > 0 && pNetGame) {
						char cmd[32];
						snprintf(cmd, sizeof(cmd), "/hotbar use %d", h + 1);
						Hide(false);
						pNetGame->SendChatCommand(cmd);
					}
					return;
				}
			}
		}

		// 6. Grid card tap -> open server item action dialog
		for (const auto& card : m_cardHits) {
			if (card.rect.contains(pos)) {
				if (card.slot >= 0 && pNetGame) {
					char cmd[32];
					snprintf(cmd, sizeof(cmd), "/_invact %d", card.slot);
					pNetGame->SendChatCommand(cmd);
				}
				return;
			}
		}
	}
}
