#include "samp/main.h"
#include "samp/GUI/gui.h"
#include "samp/GUI/widgets/InventoryUI.h"
#include "samp/Network/Network.h"
#include "samp/Settings/Settings.h"
#include "samp/Multiplayer/Multiplayer.h"
#include "samp/Utils/CUtil.h"
#include "gta-reversed/game_sa/Models/ModelInfo.h"
#include "gta-reversed/game_sa/VisibilityPlugins.h"
#include "gta-reversed/game_sa/Camera.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>

extern std::atomic<bool> g_bJavaDialogVisible;

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
	for (auto& kv : m_txdCache) {
		if (kv.second) {
			RwTextureDestroy(kv.second);
		}
	}
	m_txdCache.clear();
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
	m_syncInProgress = true;
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
	item.vecRot.x = rotX;
	item.vecRot.y = rotY;
	item.vecRot.z = rotZ;
	item.zoom = (zoom > 0.05f) ? zoom : 1.0f;
	item.nameUtf8 = nameUtf8;

	auto txdIt = m_txdCache.find(itemId);
	if (txdIt != m_txdCache.end() && txdIt->second) {
		item.texture = txdIt->second;
		item.isSnapshot = false;
	} else {
		auto it = m_snapshotCache.find(modelId);
		if (it != m_snapshotCache.end() && it->second) {
			item.texture = it->second;
			item.isSnapshot = true;
		}
	}

	// Deduplicate by server slot to keep ordering stable if packets repeat
	for (auto& existing : m_pendingItems) {
		if (existing.slot == slot) {
			existing = std::move(item);
			return;
		}
	}
	m_pendingItems.push_back(std::move(item));
}

void InventoryUI::ClearLiveHotbar()
{
	for (int i = 0; i < 7; i++) {
		m_hotbar[i] = InvHotbarSlot();
		m_hotbar[i].hotbarIdx = i;
	}
}

void InventoryUI::SetSyncHotbar(int hslot, int itemId, int modelId, int amount, const std::string& nameUtf8)
{
	if (hslot < 0 || hslot >= 7) return;

	InvHotbarSlot slotData;
	slotData.hotbarIdx = hslot;
	slotData.itemId = itemId;
	slotData.modelId = modelId;
	slotData.amount = amount;
	slotData.nameUtf8 = nameUtf8;

	if (itemId > 0) {
		auto txdIt = m_txdCache.find(itemId);
		if (txdIt != m_txdCache.end() && txdIt->second) {
			slotData.texture = txdIt->second;
			slotData.isSnapshot = false;
		} else if (modelId > 0) {
			auto it = m_snapshotCache.find(modelId);
			if (it != m_snapshotCache.end() && it->second) {
				slotData.texture = it->second;
				slotData.isSnapshot = true;
			}
		}
	}

	m_pendingHotbar[hslot] = slotData;
	if (!m_syncInProgress) {
		m_hotbar[hslot] = std::move(slotData);
	}
}

void InventoryUI::FinishSyncAndShow()
{
	if (!IsMode20()) return;

	m_syncInProgress = false;
	std::sort(m_pendingItems.begin(), m_pendingItems.end(),
		[](const InvSlotItem& a, const InvSlotItem& b) {
			return a.slot < b.slot;
		});

	m_items = std::move(m_pendingItems);
	for (int i = 0; i < 7; i++) {
		m_hotbar[i] = m_pendingHotbar[i];
	}

	if (m_dropdownOpen && !findItemBySlot(m_dropdownSlot)) {
		closeDropdown();
	}

	// Only call Show() if not already visible; avoids flicker & scroll reset on server resyncs
	if (!visible()) {
		Show();
	}
}

void InventoryUI::CloseFromServer()
{
	Hide(false);
}

void InventoryUI::Show()
{
	if (!IsMode20()) return;
	m_scrollVel = 0.0f;
	closeDropdown();
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
	closeDropdown();
	m_touchDown = false;
	m_draggingGrid = false;
	m_draggingScrollbar = false;
	m_hasDragged = false;
	m_touchStartedInDropdown = false;
	m_scrollVel = 0.0f;

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
	ImVec2 disp = (parent() && parent()->width() > 0.0f) ? parent()->size() : ImGui::GetIO().DisplaySize;
	if (disp.x <= 0.0f || disp.y <= 0.0f) disp = ImVec2(1920.0f, 1080.0f);
	setPosition(ImVec2(0.0f, 0.0f));
	setFixedSize(disp);

	// Expanded vertical & horizontal bag area now that hotbar sits outside
	float panelW = disp.x * 0.80f;
	float panelH = disp.y * 0.82f;
	float left = (disp.x - panelW) * 0.5f;
	float top = (disp.y - panelH) * 0.5f;

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

RwTexture* InventoryUI::getCustomItemTexture(int itemId)
{
	if (itemId < 61 || itemId > 100) return nullptr;

	auto it = m_txdCache.find(itemId);
	if (it != m_txdCache.end()) {
		return it->second;
	}

	static const char* kItemTexNames[40] = {
		"cabbage", "carrot", "tomato", "potato", "corn", "broccoli", "pumpkin", "cucumber",
		"eggplant", "chili", "garlic", "onion", "lettuce", "cauliflower", "radish", "watermelon",
		"strawberry", "pineapple", "mango", "banana", "orange", "grape", "coconut", "durian",
		"apple", "papaya", "dragon_fruit", "avocado", "lychee", "jackfruit", "stone", "coal",
		"copper_ore", "iron_ore", "gold_ore", "silver_ore", "diamond", "emerald", "ruby", "quartz"
	};

	const char* texName = kItemTexNames[itemId - 61];
	RwTexture* tex = CUtil::LoadTextureFromDB("gta", texName);
	if (!tex) tex = CUtil::LoadTextureFromDB("gta3", texName);
	if (!tex) tex = CUtil::GetTexture(texName);
	m_txdCache[itemId] = tex;
	return tex;
}

void InventoryUI::ProcessSnapshots()
{
	bool needBag = visible();
	bool needHudHotbar = !visible() && IsMode20() && (m_hudHotbarOpen || m_hotbarAnim > 0.01f);
	if (!needBag && !needHudHotbar) return;

	if (needBag) {
		for (auto& item : m_items) {
			if (!item.texture && item.itemId >= 61 && item.itemId <= 100) {
				RwTexture* tex = getCustomItemTexture(item.itemId);
				if (tex) {
					item.texture = tex;
					item.isSnapshot = false;
				}
			}
		}
	}

	for (int i = 0; i < 7; i++) {
		InvHotbarSlot& hb = m_hotbar[i];
		if (!hb.texture && hb.itemId >= 61 && hb.itemId <= 100) {
			RwTexture* tex = getCustomItemTexture(hb.itemId);
			if (tex) {
				hb.texture = tex;
				hb.isSnapshot = false;
			}
		}
	}

	if (!pSnapShotHelper) return;

	int created = 0;
	if (needBag) {
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
	}

	for (int i = 0; i < 7; i++) {
		InvHotbarSlot& hb = m_hotbar[i];
		if (hb.itemId <= 0 || hb.texture || hb.snapAttempts >= 2 || hb.modelId <= 0) continue;
		hb.snapAttempts++;
		CVector defaultRot{};
		defaultRot.x = defaultRot.y = defaultRot.z = 0.0f;
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
		case 0: {
			float g = r * 0.16f;
			float s = r * 0.48f;
			renderer->drawRect(ImVec2(center.x - g - s, center.y - g - s), ImVec2(center.x - g, center.y - g), col, false, 1.5f, 1.6f);
			renderer->drawRect(ImVec2(center.x + g, center.y - g - s), ImVec2(center.x + g + s, center.y - g), col, false, 1.5f, 1.6f);
			renderer->drawRect(ImVec2(center.x - g - s, center.y + g), ImVec2(center.x - g, center.y + g + s), col, false, 1.5f, 1.6f);
			renderer->drawRect(ImVec2(center.x + g, center.y + g), ImVec2(center.x + g + s, center.y + g + s), col, false, 1.5f, 1.6f);
			break;
		}
		case 1: {
			renderer->drawRect(ImVec2(center.x - r * 0.22f, center.y - r * 0.72f), ImVec2(center.x + r * 0.22f, center.y - r * 0.36f), col, false, 1.0f, 1.6f);
			renderer->drawTriangle(
				ImVec2(center.x, center.y - r * 0.42f),
				ImVec2(center.x - r * 0.58f, center.y + r * 0.62f),
				ImVec2(center.x + r * 0.58f, center.y + r * 0.62f),
				col, false, 1.6f);
			break;
		}
		case 2: {
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
		case 3: {
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
		case 4: {
			renderer->drawRect(ImVec2(center.x - r * 0.55f, center.y - r * 0.55f), ImVec2(center.x + r * 0.55f, center.y + r * 0.55f), col, false, r * 0.55f, 1.6f);
			renderer->drawRect(ImVec2(center.x - r * 0.18f, center.y - r * 0.18f), ImVec2(center.x + r * 0.18f, center.y + r * 0.18f), col, true, r * 0.18f);
			break;
		}
	}
}

void InventoryUI::drawHeader(ImGuiRenderer* renderer, const ImVec2& panelMin, const ImVec2& panelMax)
{
	float u = UISettings::scaleFactor().y;
	float headerH = 26.0f * u;
	float padX = 10.0f * u;
	float titleFontSz = UISettings::fontSize() * 0.56f;
	float weightFontSz = UISettings::fontSize() * 0.48f;

	ImVec2 hdrMax(panelMax.x, panelMin.y + headerH);
	renderer->drawRect(panelMin, hdrMax, ImColor(10, 12, 16, 165), true, 10.0f);
	renderer->drawLine(ImVec2(panelMin.x + 8.0f, hdrMax.y), ImVec2(panelMax.x - 8.0f, hdrMax.y), ImColor(255, 255, 255, 32), 1.0f);

	// Bag icon on top-left
	float iconSz = headerH * 0.44f;
	float iconX = panelMin.x + padX;
	float iconY = panelMin.y + (headerH - iconSz) * 0.5f + 1.0f;
	renderer->drawRect(ImVec2(iconX, iconY + iconSz * 0.25f), ImVec2(iconX + iconSz, iconY + iconSz), ImColor(240, 244, 250, 240), true, 2.5f);
	renderer->drawRect(ImVec2(iconX + iconSz * 0.25f, iconY), ImVec2(iconX + iconSz * 0.75f, iconY + iconSz * 0.35f), ImColor(240, 244, 250, 240), false, 2.0f, 1.6f);

	// Title: กระเป๋าส่วนตัว
	std::string titleStr = "กระเป๋าส่วนตัว";
	ImVec2 titleSz = renderer->calculateTextSize(titleStr, titleFontSz);
	float titleX = iconX + iconSz + 7.0f * u;
	float titleY = panelMin.y + (headerH - titleSz.y) * 0.5f;
	renderer->drawText(ImVec2(titleX, titleY), ImColor(245, 247, 250, 255), titleStr, false, titleFontSz);

	// Weight: [ 42.20 / 80.00 Kg. ]
	char weightBuf[64];
	snprintf(weightBuf, sizeof(weightBuf), "[ %.2f / %.2f Kg. ]", m_usedWeight, m_maxWeight);
	ImVec2 weightSz = renderer->calculateTextSize(weightBuf, weightFontSz);
	float weightX = titleX + titleSz.x + 9.0f * u;
	float weightY = panelMin.y + (headerH - weightSz.y) * 0.5f;
	renderer->drawText(ImVec2(weightX, weightY), ImColor(215, 220, 228, 225), weightBuf, false, weightFontSz);

	// Right side: [5 Category Icons] + [X Close]
	float btnSize = headerH * 0.76f;
	float gap = 5.0f * u;
	float rightCursor = panelMax.x - padX;

	// Close button X (expanded hit target for reliable thumb taps)
	ImVec2 closeVisMin(rightCursor - btnSize, panelMin.y + (headerH - btnSize) * 0.5f);
	ImVec2 closeVisMax(rightCursor, closeVisMin.y + btnSize);
	float closePad = 5.0f * u;
	m_closeBtnRect.min = ImVec2(closeVisMin.x - closePad, closeVisMin.y - closePad);
	m_closeBtnRect.max = ImVec2(closeVisMax.x + closePad, closeVisMax.y + closePad);
	ImVec2 cCenter = (closeVisMin + closeVisMax) * 0.5f;
	float xr = btnSize * 0.28f;
	renderer->drawRect(closeVisMin, closeVisMax, ImColor(255, 255, 255, 18), true, 5.0f);
	renderer->drawLine(ImVec2(cCenter.x - xr, cCenter.y - xr), ImVec2(cCenter.x + xr, cCenter.y + xr), ImColor(235, 238, 245, 245), 2.1f);
	renderer->drawLine(ImVec2(cCenter.x + xr, cCenter.y - xr), ImVec2(cCenter.x - xr, cCenter.y + xr), ImColor(235, 238, 245, 245), 2.1f);
	rightCursor = closeVisMin.x - gap * 1.8f;

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

	float u = UISettings::scaleFactor().y;
	float pad = 7.0f * u;
	float gap = 4.5f * u;
	float scrollReserve = 10.0f * u;
	float availW = (gridMax.x - gridMin.x) - pad - scrollReserve;

	// 10 columns -> original compact card size
	const int cols = 10;
	float cardW = (availW - gap * (cols - 1)) / (float)cols;
	float cardH = cardW * 1.00f;
	float footerH = cardH * 0.27f;

	int totalItems = (int)items.size();
	int rows = (totalItems + cols - 1) / cols;
	float contentH = pad + rows * (cardH + gap) + pad;
	float viewH = gridMax.y - gridMin.y;
	m_maxScrollY = std::max(0.0f, contentH - viewH);

	// Smooth scroll momentum when finger released
	float dt = std::min(ImGui::GetIO().DeltaTime, 0.05f);
	if (!m_touchDown && std::abs(m_scrollVel) > 5.0f && m_maxScrollY > 0.0f) {
		m_scrollY += m_scrollVel * dt;
		m_scrollVel *= std::exp(-8.5f * dt);
		if (m_scrollY <= 0.0f) {
			m_scrollY = 0.0f;
			m_scrollVel = 0.0f;
		} else if (m_scrollY >= m_maxScrollY) {
			m_scrollY = m_maxScrollY;
			m_scrollVel = 0.0f;
		}
	}

	if (m_scrollY < 0.0f) m_scrollY = 0.0f;
	if (m_scrollY > m_maxScrollY) m_scrollY = m_maxScrollY;

	// Scrollbar hit area (right edge)
	float sbW = 3.5f * u;
	float sbX = gridMax.x - sbW - 3.5f * u;
	float sbTop = gridMin.y + 4.0f * u;
	float sbBot = gridMax.y - 4.0f * u;
	m_scrollbarRect.min = ImVec2(sbX - 10.0f * u, sbTop);
	m_scrollbarRect.max = ImVec2(gridMax.x, sbBot);

	renderer->pushClipRect(gridMin, gridMax, true);

	if (items.empty()) {
		float emptyFontSz = UISettings::fontSize() * 0.50f;
		std::string emptyMsg = "ไม่มีไอเทมในหมวดหมู่นี้";
		ImVec2 esz = renderer->calculateTextSize(emptyMsg, emptyFontSz);
		renderer->drawText(
			ImVec2(gridMin.x + ((gridMax.x - gridMin.x) - esz.x) * 0.5f, gridMin.y + (viewH - esz.y) * 0.5f),
			ImColor(170, 178, 190, 200), emptyMsg, false, emptyFontSz);
		renderer->popClipRect();
		return;
	}

	float countFontSz = std::max(10.0f, cardH * 0.16f);
	float nameFontSz = std::max(10.0f, cardH * 0.165f);

	for (int idx = 0; idx < totalItems; idx++) {
		const InvSlotItem* item = items[idx];
		int r = idx / cols;
		int c = idx % cols;

		float x0 = gridMin.x + pad + c * (cardW + gap);
		float y0 = gridMin.y + pad + r * (cardH + gap) - m_scrollY;
		float x1 = x0 + cardW;
		float y1 = y0 + cardH;

		if (y1 < gridMin.y || y0 > gridMax.y) continue;

		// Card body (semi-transparent dark glass, gold highlight when dropdown active)
		bool isDropdownTarget = (m_dropdownOpen && m_dropdownSlot == item->slot);
		renderer->drawRect(ImVec2(x0, y0), ImVec2(x1, y1),
			isDropdownTarget ? ImColor(36, 38, 46, 200) : ImColor(24, 26, 32, 162), true, 5.0f);
		renderer->drawRect(ImVec2(x0, y0), ImVec2(x1, y1),
			isDropdownTarget ? ImColor(245, 166, 35, 235) : ImColor(255, 255, 255, 24),
			false, 5.0f, isDropdownTarget ? 1.8f : 1.0f);

		// Bottom dark footer strip for item name
		float footY = y1 - footerH;
		renderer->drawRect(ImVec2(x0, footY), ImVec2(x1, y1), ImColor(12, 14, 18, 188), true, 4.0f);

		// Top-left star badge (favorite or in hotbar)
		if (item->favorite || isItemInHotbar(item->itemId)) {
			drawStar(renderer, ImVec2(x0 + cardW * 0.13f, y0 + cardH * 0.13f), cardW * 0.075f, ImColor(245, 166, 35, 255));
		}

		// Top-right quantity badge
		char qtyBuf[16];
		snprintf(qtyBuf, sizeof(qtyBuf), "%d", item->amount);
		ImVec2 qsz = renderer->calculateTextSize(qtyBuf, countFontSz);
		renderer->drawText(
			ImVec2(x1 - qsz.x - 4.0f * u, y0 + 2.5f * u),
			ImColor(235, 238, 245, 235), qtyBuf, false, countFontSz);

		// Center icon (TXD uses normal V, RenderWare snapshot uses flipped V)
		float imgPadX = cardW * 0.19f;
		float imgTop = y0 + cardH * 0.14f;
		float imgBot = footY - 1.5f * u;
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

	// Visible right-side vertical scrollbar whenever content exceeds grid height
	if (m_maxScrollY > 0.5f && sbBot > sbTop + 10.0f) {
		float trackH = sbBot - sbTop;
		renderer->drawRect(ImVec2(sbX, sbTop), ImVec2(sbX + sbW, sbBot), ImColor(255, 255, 255, 24), true, sbW * 0.5f);

		float thumbRatio = std::clamp(viewH / contentH, 0.14f, 1.0f);
		float thumbH = std::max(22.0f * u, trackH * thumbRatio);
		float scrollFrac = (m_maxScrollY > 0.0f) ? (m_scrollY / m_maxScrollY) : 0.0f;
		float thumbY = sbTop + (trackH - thumbH) * scrollFrac;
		ImColor thumbCol = m_draggingScrollbar ? ImColor(245, 166, 35, 240) : ImColor(245, 166, 35, 175);
		renderer->drawRect(ImVec2(sbX, thumbY), ImVec2(sbX + sbW, thumbY + thumbH), thumbCol, true, sbW * 0.5f);
	}

	renderer->popClipRect();
}

void InventoryUI::closeDropdown()
{
	m_dropdownOpen = false;
	m_dropdownHotbarExpanded = false;
	m_dropdownSlot = -1;
	m_dropdownRect = HitRect{};
	for (int i = 0; i < 4; i++) m_dropActionRects[i] = HitRect{};
	for (int i = 0; i < 8; i++) m_dropHotbarRects[i] = HitRect{};
}

const InvSlotItem* InventoryUI::findItemBySlot(int slot) const
{
	if (slot < 0) return nullptr;
	for (const auto& item : m_items) {
		if (item.slot == slot) return &item;
	}
	return nullptr;
}

int InventoryUI::findHotbarIndexForItem(int itemId) const
{
	if (itemId <= 0) return -1;
	for (int i = 0; i < 7; i++) {
		if (m_hotbar[i].itemId == itemId) return i;
	}
	return -1;
}

void InventoryUI::assignHotbarLocal(int hotbarIdx, const InvSlotItem* item)
{
	if (hotbarIdx < 0 || hotbarIdx >= 7 || !item || item->itemId <= 0) return;
	clearHotbarForItemLocal(item->itemId);

	InvHotbarSlot hb;
	hb.hotbarIdx = hotbarIdx;
	hb.itemId = item->itemId;
	hb.modelId = item->modelId;
	hb.amount = item->amount;
	hb.nameUtf8 = item->nameUtf8;
	hb.texture = item->texture;
	hb.isSnapshot = item->isSnapshot;
	m_hotbar[hotbarIdx] = std::move(hb);
}

void InventoryUI::clearHotbarForItemLocal(int itemId)
{
	if (itemId <= 0) return;
	for (int i = 0; i < 7; i++) {
		if (m_hotbar[i].itemId == itemId) {
			m_hotbar[i] = InvHotbarSlot();
			m_hotbar[i].hotbarIdx = i;
		}
	}
}

void InventoryUI::drawDropdown(ImGuiRenderer* renderer)
{
	if (!m_dropdownOpen || m_dropdownSlot < 0) return;

	const InvSlotItem* item = findItemBySlot(m_dropdownSlot);
	if (!item) {
		closeDropdown();
		return;
	}

	float u = UISettings::scaleFactor().y;
	float dropW = 132.0f * u;
	float pad = 5.5f * u;
	float hdrH = 20.0f * u;
	float rowH = 21.5f * u;
	float gap = 3.0f * u;
	int boundHb = findHotbarIndexForItem(item->itemId);

	float hbSectionH = 0.0f;
	float hbNumH = 19.5f * u;
	float clearBtnH = 18.5f * u;
	if (m_dropdownHotbarExpanded) {
		hbSectionH = hbNumH + gap;
		if (boundHb >= 0) {
			hbSectionH += clearBtnH + gap;
		}
	}

	float dropH = pad * 2.0f + hdrH + gap + 4.0f * rowH + 3.0f * gap + hbSectionH;

	// Smart anchor beside tapped item card (prefer right, flip left if near right edge)
	float x = m_dropdownAnchorRect.max.x + 5.0f * u;
	if (x + dropW > m_panelMax.x - 6.0f * u) {
		x = m_dropdownAnchorRect.min.x - dropW - 5.0f * u;
	}
	x = std::clamp(x, m_panelMin.x + 6.0f * u, m_panelMax.x - dropW - 6.0f * u);

	float y = m_dropdownAnchorRect.min.y;
	y = std::clamp(y, m_gridMin.y + 4.0f * u, std::max(m_gridMin.y + 4.0f * u, m_panelMax.y - dropH - 4.0f * u));

	m_dropdownRect.min = ImVec2(x, y);
	m_dropdownRect.max = ImVec2(x + dropW, y + dropH);

	// Soft drop shadow + dark glass container matching bag theme
	renderer->drawRect(ImVec2(x + 2.0f * u, y + 3.0f * u), ImVec2(x + dropW + 2.0f * u, y + dropH + 3.0f * u),
		ImColor(0, 0, 0, 115), true, 8.0f * u);
	renderer->drawRect(m_dropdownRect.min, m_dropdownRect.max, ImColor(16, 19, 26, 244), true, 7.0f * u);
	renderer->drawRect(m_dropdownRect.min, m_dropdownRect.max, ImColor(245, 166, 35, 175), false, 7.0f * u, 1.3f);

	float cx0 = x + pad;
	float cx1 = x + dropW - pad;
	float curY = y + pad;

	// Header: Item name + xAmount
	float hdrFontSz = std::max(11.0f, hdrH * 0.62f);
	char hdrBuf[96];
	snprintf(hdrBuf, sizeof(hdrBuf), "%s (x%d)", item->nameUtf8.c_str(), item->amount);
	renderer->pushClipRect(ImVec2(cx0, curY), ImVec2(cx1, curY + hdrH), true);
	ImVec2 hsz = renderer->calculateTextSize(hdrBuf, hdrFontSz);
	renderer->drawText(ImVec2(cx0 + 2.0f * u, curY + (hdrH - hsz.y) * 0.5f),
		ImColor(245, 196, 81, 255), hdrBuf, false, hdrFontSz);
	renderer->popClipRect();
	curY += hdrH;
	renderer->drawLine(ImVec2(cx0, curY), ImVec2(cx1, curY), ImColor(255, 255, 255, 34), 1.0f);
	curY += gap;

	float btnFontSz = std::max(11.0f, rowH * 0.54f);
	auto drawActionRow = [&](int idx, const char* label, const ImColor& bg, const ImColor& border, const ImColor& textCol) {
		m_dropActionRects[idx].min = ImVec2(cx0, curY);
		m_dropActionRects[idx].max = ImVec2(cx1, curY + rowH);
		renderer->drawRect(m_dropActionRects[idx].min, m_dropActionRects[idx].max, bg, true, 5.0f * u);
		renderer->drawRect(m_dropActionRects[idx].min, m_dropActionRects[idx].max, border, false, 5.0f * u, 1.0f);
		ImVec2 tsz = renderer->calculateTextSize(label, btnFontSz);
		renderer->drawText(
			ImVec2(cx0 + (cx1 - cx0 - tsz.x) * 0.5f, curY + (rowH - tsz.y) * 0.5f),
			textCol, label, false, btnFontSz);
		curY += rowH + gap;
	};

	// Action 0: Use
	drawActionRow(0, "ใช้งาน", ImColor(34, 92, 58, 215), ImColor(74, 222, 128, 165), ImColor(240, 253, 244, 255));

	// Action 1: Hotbar toggle
	char hbLabel[64];
	if (boundHb >= 0) {
		snprintf(hbLabel, sizeof(hbLabel), "ใส่คีย์ลัด [ช่อง %d] %s", boundHb + 1, m_dropdownHotbarExpanded ? "▾" : "▸");
	} else {
		snprintf(hbLabel, sizeof(hbLabel), "ใส่คีย์ลัด (1-7) %s", m_dropdownHotbarExpanded ? "▾" : "▸");
	}
	drawActionRow(1, hbLabel,
		m_dropdownHotbarExpanded ? ImColor(36, 56, 86, 230) : ImColor(28, 34, 46, 215),
		ImColor(96, 165, 250, 155), ImColor(224, 242, 254, 255));

	// Optional inline 1..7 hotbar strip
	for (int i = 0; i < 8; i++) m_dropHotbarRects[i] = HitRect{};
	if (m_dropdownHotbarExpanded) {
		float hbGap = 2.5f * u;
		float hbW = ((cx1 - cx0) - hbGap * 6.0f) / 7.0f;
		float hbFontSz = std::max(10.0f, hbNumH * 0.56f);
		for (int h = 0; h < 7; h++) {
			float hx0 = cx0 + h * (hbW + hbGap);
			m_dropHotbarRects[h].min = ImVec2(hx0, curY);
			m_dropHotbarRects[h].max = ImVec2(hx0 + hbW, curY + hbNumH);
			bool isBound = (boundHb == h);
			bool hasOther = (!isBound && m_hotbar[h].itemId > 0);
			ImColor hBg = isBound ? ImColor(245, 166, 35, 210) : (hasOther ? ImColor(38, 42, 54, 215) : ImColor(24, 28, 36, 220));
			ImColor hBd = isBound ? ImColor(253, 224, 71, 240) : ImColor(255, 255, 255, 48);
			ImColor hTx = isBound ? ImColor(18, 18, 22, 255) : ImColor(245, 247, 250, 245);
			renderer->drawRect(m_dropHotbarRects[h].min, m_dropHotbarRects[h].max, hBg, true, 4.0f * u);
			renderer->drawRect(m_dropHotbarRects[h].min, m_dropHotbarRects[h].max, hBd, false, 4.0f * u, 1.0f);
			char nbuf[4];
			snprintf(nbuf, sizeof(nbuf), "%d", h + 1);
			ImVec2 nsz = renderer->calculateTextSize(nbuf, hbFontSz);
			renderer->drawText(ImVec2(hx0 + (hbW - nsz.x) * 0.5f, curY + (hbNumH - nsz.y) * 0.5f), hTx, nbuf, false, hbFontSz);
		}
		curY += hbNumH + gap;

		if (boundHb >= 0) {
			m_dropHotbarRects[7].min = ImVec2(cx0, curY);
			m_dropHotbarRects[7].max = ImVec2(cx1, curY + clearBtnH);
			renderer->drawRect(m_dropHotbarRects[7].min, m_dropHotbarRects[7].max, ImColor(82, 34, 38, 215), true, 4.0f * u);
			renderer->drawRect(m_dropHotbarRects[7].min, m_dropHotbarRects[7].max, ImColor(248, 113, 113, 150), false, 4.0f * u, 1.0f);
			float clrFontSz = std::max(10.0f, clearBtnH * 0.56f);
			const char* clrTxt = "ถอดออกจากคีย์ลัด";
			ImVec2 csz = renderer->calculateTextSize(clrTxt, clrFontSz);
			renderer->drawText(ImVec2(cx0 + (cx1 - cx0 - csz.x) * 0.5f, curY + (clearBtnH - csz.y) * 0.5f),
				ImColor(254, 226, 226, 255), clrTxt, false, clrFontSz);
			curY += clearBtnH + gap;
		}
	}

	// Action 2: Drop all
	drawActionRow(2, "ทิ้งทั้งหมด", ImColor(88, 36, 36, 210), ImColor(248, 113, 113, 150), ImColor(254, 226, 226, 255));

	// Action 3: More server dialog options (Give / Sell / Amount / Details)
	drawActionRow(3, "เพิ่มเติม (มอบ/ขาย/ดู)", ImColor(30, 34, 44, 220), ImColor(255, 255, 255, 55), ImColor(232, 236, 245, 245));
}

void InventoryUI::draw(ImGuiRenderer* renderer)
{
	if (!visible() || !IsMode20()) return;

	performLayout();

	float u = UISettings::scaleFactor().y;
	float headerH = 26.0f * u;

	// 1. Main semi-transparent glassmorphic container (no hotbar inside bag)
	float rounding = 10.0f;
	renderer->drawRect(m_panelMin, m_panelMax, ImColor(14, 16, 20, 148), true, rounding);
	renderer->drawRect(m_panelMin, m_panelMax, ImColor(255, 255, 255, 34), false, rounding, 1.1f);

	// 2. Header bar
	drawHeader(renderer, m_panelMin, m_panelMax);

	// 3. Full-height scrollable Item Grid
	m_gridMin = ImVec2(m_panelMin.x + 2.0f, m_panelMin.y + headerH + 2.0f);
	m_gridMax = ImVec2(m_panelMax.x - 2.0f, m_panelMax.y - 3.0f * u);
	drawGrid(renderer, m_gridMin, m_gridMax);

	// 4. Floating item action dropdown (drawn above grid)
	drawDropdown(renderer);
}

bool InventoryUI::canShowHudHotbar() const
{
	if (!IsMode20() || visible()) return false;
	if (g_bJavaDialogVisible.load()) return false;
	if (!pNetGame || pNetGame->GetGameState() != GAMESTATE_CONNECTED) return false;
	if (!pUI) return false;
	if (pUI->splashscreen() && pUI->splashscreen()->visible()) return false;
	if (pUI->spawn() && pUI->spawn()->visible()) return false;
	if (pUI->dialog() && pUI->dialog()->visible()) return false;
	if (pUI->keyboard() && pUI->keyboard()->visible()) return false;
	if (pUI->playertablist() && pUI->playertablist()->visible()) return false;
	return true;
}

void InventoryUI::drawHudHotbar(ImGuiRenderer* renderer)
{
	if (!canShowHudHotbar()) {
		m_hudTouchDown = false;
		return;
	}

	ImVec2 disp = (parent() && parent()->width() > 0.0f) ? parent()->size() : ImGui::GetIO().DisplaySize;
	if (disp.x <= 0.0f || disp.y <= 0.0f) return;

	float u = UISettings::scaleFactor().y;
	float dt = std::min(ImGui::GetIO().DeltaTime, 0.05f);
	float target = m_hudHotbarOpen ? 1.0f : 0.0f;
	if (m_hotbarAnim < target) {
		m_hotbarAnim = std::min(target, m_hotbarAnim + dt * 7.5f);
	} else if (m_hotbarAnim > target) {
		m_hotbarAnim = std::max(target, m_hotbarAnim - dt * 7.5f);
	}

	// 1. Far bottom-right arrow toggle button (chevron only, no overlap with hotbar)
	float arrowBtnSz = 22.0f * u;
	float marginR = 10.0f * u;
	float marginB = 8.0f * u;
	m_hudArrowRect.max = ImVec2(disp.x - marginR, disp.y - marginB);
	m_hudArrowRect.min = ImVec2(m_hudArrowRect.max.x - arrowBtnSz, m_hudArrowRect.max.y - arrowBtnSz);

	ImColor arrowBg = m_hudHotbarOpen ? ImColor(24, 28, 36, 175) : ImColor(14, 16, 20, 150);
	ImColor arrowBorder = m_hudHotbarOpen ? ImColor(245, 166, 35, 165) : ImColor(255, 255, 255, 48);
	renderer->drawRect(m_hudArrowRect.min, m_hudArrowRect.max, arrowBg, true, 6.0f * u);
	renderer->drawRect(m_hudArrowRect.min, m_hudArrowRect.max, arrowBorder, false, 6.0f * u, 1.1f);

	ImVec2 ac = (m_hudArrowRect.min + m_hudArrowRect.max) * 0.5f;
	float aw = arrowBtnSz * 0.27f;
	float ah = arrowBtnSz * 0.16f;
	ImColor chevronCol = m_hudHotbarOpen ? ImColor(245, 166, 35, 245) : ImColor(245, 247, 250, 235);
	float stroke = std::max(2.0f, 1.4f * u);
	if (!m_hudHotbarOpen) {
		// Up arrow chevron (^) -> press to slide hotbar up
		renderer->drawLine(ImVec2(ac.x - aw, ac.y + ah), ImVec2(ac.x, ac.y - ah), chevronCol, stroke);
		renderer->drawLine(ImVec2(ac.x, ac.y - ah), ImVec2(ac.x + aw, ac.y + ah), chevronCol, stroke);
	} else {
		// Down arrow chevron (v) -> press to slide hotbar down/close
		renderer->drawLine(ImVec2(ac.x - aw, ac.y - ah), ImVec2(ac.x, ac.y + ah), chevronCol, stroke);
		renderer->drawLine(ImVec2(ac.x, ac.y + ah), ImVec2(ac.x + aw, ac.y - ah), chevronCol, stroke);
	}

	// 2. Slide-up external Hotbar 1-7 dock (centered bottom, always left of the far-right arrow)
	if (m_hotbarAnim <= 0.01f) {
		m_hudHotbarDockRect = HitRect{};
		return;
	}

	float ease = 1.0f - (1.0f - m_hotbarAnim) * (1.0f - m_hotbarAnim);
	float slotSize = 25.0f * u;
	float gap = 4.0f * u;
	float dockPad = 4.5f * u;
	float dockW = 7.0f * slotSize + 6.0f * gap + dockPad * 2.0f;
	float dockH = slotSize + dockPad * 2.0f;

	float dockX = (disp.x - dockW) * 0.5f;
	float maxAllowedX = m_hudArrowRect.min.x - dockW - 8.0f * u;
	if (dockX > maxAllowedX) dockX = std::max(8.0f * u, maxAllowedX);

	float hiddenY = disp.y + 4.0f * u;
	float shownY = disp.y - dockH - marginB;
	float dockY = hiddenY + (shownY - hiddenY) * ease;

	m_hudHotbarDockRect.min = ImVec2(dockX, dockY);
	m_hudHotbarDockRect.max = ImVec2(dockX + dockW, dockY + dockH);

	renderer->pushClipRect(ImVec2(0.0f, 0.0f), disp, true);
	renderer->drawRect(m_hudHotbarDockRect.min, m_hudHotbarDockRect.max, ImColor(12, 14, 18, 165), true, 7.0f * u);
	renderer->drawRect(m_hudHotbarDockRect.min, m_hudHotbarDockRect.max, ImColor(255, 255, 255, 36), false, 7.0f * u, 1.0f);

	float startX = dockX + dockPad;
	float slotY = dockY + dockPad;
	float numFontSz = std::max(9.0f, slotSize * 0.25f);
	float qtyFontSz = std::max(9.0f, slotSize * 0.24f);

	for (int i = 0; i < 7; i++) {
		float sx0 = startX + i * (slotSize + gap);
		m_hudHotbarSlotRects[i].min = ImVec2(sx0, slotY);
		m_hudHotbarSlotRects[i].max = ImVec2(sx0 + slotSize, slotY + slotSize);

		const InvHotbarSlot& hb = m_hotbar[i];
		bool hasItem = (hb.itemId > 0);

		renderer->drawRect(m_hudHotbarSlotRects[i].min, m_hudHotbarSlotRects[i].max,
			ImColor(24, 26, 32, hasItem ? 180 : 145), true, 5.0f);
		renderer->drawRect(m_hudHotbarSlotRects[i].min, m_hudHotbarSlotRects[i].max,
			hasItem ? ImColor(245, 166, 35, 95) : ImColor(255, 255, 255, 24), false, 5.0f, 1.0f);

		char numBuf[4];
		snprintf(numBuf, sizeof(numBuf), "%d", i + 1);
		renderer->drawText(ImVec2(sx0 + 2.5f * u, slotY + 1.5f * u), ImColor(245, 166, 35, 235), numBuf, false, numFontSz);

		if (hasItem) {
			if (hb.texture && hb.texture->raster) {
				float pad = slotSize * 0.15f;
				ImVec2 uv0 = hb.isSnapshot ? ImVec2(0.0f, 1.0f) : ImVec2(0.0f, 0.0f);
				ImVec2 uv1 = hb.isSnapshot ? ImVec2(1.0f, 0.0f) : ImVec2(1.0f, 1.0f);
				renderer->drawImage(
					ImVec2(sx0 + pad, slotY + pad),
					ImVec2(sx0 + slotSize - pad, slotY + slotSize - pad),
					(ImTextureID)hb.texture->raster, uv0, uv1);
			}
			char cntBuf[16];
			snprintf(cntBuf, sizeof(cntBuf), "x%d", hb.amount);
			ImVec2 csz = renderer->calculateTextSize(cntBuf, qtyFontSz);
			renderer->drawText(
				ImVec2(sx0 + slotSize - csz.x - 2.0f * u, slotY + slotSize - csz.y - 1.0f * u),
				ImColor(240, 244, 250, 240), cntBuf, false, qtyFontSz);
		}
	}
	renderer->popClipRect();
}

bool InventoryUI::handleHudTouch(const ImVec2& pos, TouchType type)
{
	if (!canShowHudHotbar()) {
		m_hudTouchDown = false;
		return false;
	}

	float u = UISettings::scaleFactor().y;

	if (type == TouchType::push) {
		bool hitArrow = m_hudArrowRect.contains(pos);
		bool hitDock = (m_hudHotbarOpen && m_hotbarAnim > 0.25f && m_hudHotbarDockRect.contains(pos));
		if (hitArrow || hitDock) {
			m_hudTouchDown = true;
			m_hudTouchStart = pos;
			m_hudDragDist = 0.0f;
			return true;
		}
		return false;
	}

	if (!m_hudTouchDown) return false;

	if (type == TouchType::move) {
		float dx = pos.x - m_hudTouchStart.x;
		float dy = pos.y - m_hudTouchStart.y;
		m_hudDragDist = std::max(m_hudDragDist, std::sqrt(dx * dx + dy * dy));
		return true;
	}

	if (type == TouchType::pop) {
		m_hudTouchDown = false;
		if (m_hudDragDist > 16.0f * u) {
			return true;
		}

		if (m_hudArrowRect.contains(pos)) {
			m_hudHotbarOpen = !m_hudHotbarOpen;
			return true;
		}

		if (m_hudHotbarOpen && m_hotbarAnim > 0.25f) {
			for (int h = 0; h < 7; h++) {
				if (m_hudHotbarSlotRects[h].contains(pos)) {
					if (m_hotbar[h].itemId > 0 && pNetGame) {
						char cmd[32];
						snprintf(cmd, sizeof(cmd), "/hotbar use %d", h + 1);
						pNetGame->SendChatCommand(cmd);
					}
					return true;
				}
			}
		}
		return true;
	}

	return false;
}

void InventoryUI::touchEvent(const ImVec2& pos, TouchType type)
{
	if (!visible() || !IsMode20()) return;

	float u = UISettings::scaleFactor().y;

	if (type == TouchType::push) {
		m_touchDown = true;
		m_touchStartPos = pos;
		m_lastTouchPos = pos;
		m_totalDragDist = 0.0f;
		m_hasDragged = false;
		m_scrollVel = 0.0f;
		m_touchStartedInDropdown = (m_dropdownOpen && m_dropdownRect.contains(pos));

		if (m_touchStartedInDropdown) {
			m_draggingScrollbar = false;
			m_draggingGrid = false;
		} else {
			m_draggingScrollbar = (m_maxScrollY > 0.5f && m_scrollbarRect.contains(pos));
			if (m_draggingScrollbar) {
				float sbH = std::max(1.0f, m_scrollbarRect.max.y - m_scrollbarRect.min.y);
				float frac = std::clamp((pos.y - m_scrollbarRect.min.y) / sbH, 0.0f, 1.0f);
				m_scrollY = frac * m_maxScrollY;
				m_draggingGrid = false;
			} else {
				m_draggingGrid = (pos.x >= m_panelMin.x && pos.x <= m_panelMax.x &&
					pos.y >= m_gridMin.y && pos.y <= m_panelMax.y);
			}
		}
		return;
	}

	if (type == TouchType::move) {
		if (!m_touchDown) return;
		float dx = pos.x - m_lastTouchPos.x;
		float dy = pos.y - m_lastTouchPos.y;
		m_totalDragDist += std::sqrt(dx * dx + dy * dy);

		float netDx = pos.x - m_touchStartPos.x;
		float netDy = pos.y - m_touchStartPos.y;
		float netDist = std::sqrt(netDx * netDx + netDy * netDy);

		// Latch scroll mode once finger moves past deadzone so release never triggers an item tap
		if (!m_hasDragged && (m_totalDragDist >= 10.0f * u || netDist >= 10.0f * u)) {
			m_hasDragged = true;
			if (!m_touchStartedInDropdown && m_dropdownOpen) {
				closeDropdown();
			}
		}

		if (m_draggingScrollbar && m_maxScrollY > 0.5f) {
			float sbH = std::max(1.0f, m_scrollbarRect.max.y - m_scrollbarRect.min.y);
			float frac = std::clamp((pos.y - m_scrollbarRect.min.y) / sbH, 0.0f, 1.0f);
			m_scrollY = frac * m_maxScrollY;
		} else if (m_draggingGrid && m_hasDragged) {
			m_scrollY -= dy;
			if (m_scrollY < 0.0f) m_scrollY = 0.0f;
			if (m_scrollY > m_maxScrollY) m_scrollY = m_maxScrollY;

			float dt = std::max(ImGui::GetIO().DeltaTime, 0.008f);
			if (std::abs(dy) > 0.1f) {
				m_scrollVel = (-dy / dt) * 0.50f;
			}
		}
		m_lastTouchPos = pos;
		return;
	}

	if (type == TouchType::pop) {
		if (!m_touchDown) return;
		m_touchDown = false;
		bool wasTap = !m_draggingScrollbar && !m_hasDragged && (m_totalDragDist < 14.0f * u);
		bool startedInDropdown = m_touchStartedInDropdown;
		m_draggingGrid = false;
		m_draggingScrollbar = false;
		m_hasDragged = false;
		m_touchStartedInDropdown = false;
		if (!wasTap) return;

		// 1. Handle open dropdown taps first
		if (m_dropdownOpen) {
			if (startedInDropdown && m_dropdownRect.contains(pos)) {
				const InvSlotItem* item = findItemBySlot(m_dropdownSlot);
				if (!item) {
					closeDropdown();
					return;
				}

				if (m_dropActionRects[0].contains(pos)) {
					// Use item directly
					int slot = item->slot;
					closeDropdown();
					if (pNetGame) {
						char cmd[32];
						snprintf(cmd, sizeof(cmd), "/inv use %d", slot);
						pNetGame->SendChatCommand(cmd);
					}
					return;
				}
				if (m_dropActionRects[1].contains(pos)) {
					// Toggle inline hotbar 1-7 picker
					m_dropdownHotbarExpanded = !m_dropdownHotbarExpanded;
					return;
				}
				if (m_dropdownHotbarExpanded) {
					for (int h = 0; h < 7; h++) {
						if (m_dropHotbarRects[h].contains(pos)) {
							int slot = item->slot;
							assignHotbarLocal(h, item);
							closeDropdown();
							if (pNetGame) {
								char cmd[36];
								snprintf(cmd, sizeof(cmd), "/hotbar set %d %d", slot, h + 1);
								pNetGame->SendChatCommand(cmd);
							}
							return;
						}
					}
					if (m_dropHotbarRects[7].contains(pos)) {
						int boundHb = findHotbarIndexForItem(item->itemId);
						clearHotbarForItemLocal(item->itemId);
						closeDropdown();
						if (boundHb >= 0 && pNetGame) {
							char cmd[32];
							snprintf(cmd, sizeof(cmd), "/hotbar clear %d", boundHb + 1);
							pNetGame->SendChatCommand(cmd);
						}
						return;
					}
				}
				if (m_dropActionRects[2].contains(pos)) {
					// Drop entire stack
					int slot = item->slot;
					closeDropdown();
					if (pNetGame) {
						char cmd[32];
						snprintf(cmd, sizeof(cmd), "/inv drop %d", slot);
						pNetGame->SendChatCommand(cmd);
					}
					return;
				}
				if (m_dropActionRects[3].contains(pos)) {
					// Open full server action dialog (Give / Sell / Drop amount / Details)
					int slot = item->slot;
					closeDropdown();
					if (pNetGame) {
						char cmd[32];
						snprintf(cmd, sizeof(cmd), "/_invact %d", slot);
						pNetGame->SendChatCommand(cmd);
					}
					return;
				}
				return;
			}

			// Tap outside dropdown closes only the dropdown (never closes the bag)
			closeDropdown();
			return;
		}

		// 2. Close [X] button (only intentional close button closes bag; avoids accidental misses near edge)
		if (m_closeBtnRect.contains(pos)) {
			Hide(true);
			return;
		}

		// 3. Category tabs
		for (int t = 0; t < 5; t++) {
			if (m_tabRects[t].contains(pos)) {
				m_activeCategory = t;
				m_scrollY = 0.0f;
				m_scrollVel = 0.0f;
				closeDropdown();
				return;
			}
		}

		// 4. Grid card tap -> open ImGui dropdown right beside tapped item
		for (const auto& card : m_cardHits) {
			if (card.rect.contains(pos)) {
				if (card.slot >= 0) {
					m_dropdownSlot = card.slot;
					m_dropdownAnchorRect = card.rect;
					m_dropdownHotbarExpanded = false;
					m_dropdownOpen = true;
				}
				return;
			}
		}
	}
}
