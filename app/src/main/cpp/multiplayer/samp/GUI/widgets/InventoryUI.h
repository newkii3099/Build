#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include "samp/GUI/widget.h"
#include "gta-reversed/game_sa/RenderWare/RenderWare.h"

struct InvSlotItem
{
	int slot = -1;
	int itemId = 0;
	int modelId = 0;
	int amount = 0;
	int maxStack = 100;
	float weight = 0.0f;
	int category = 0; // 1=Food/Drink, 2=Material, 3=Weapon/Equip, 4=Other
	bool favorite = false;
	CVector vecRot{};
	float zoom = 1.0f;
	std::string nameUtf8;
	RwTexture* texture = nullptr;
	bool isSnapshot = false;
	uint8_t snapAttempts = 0;
};

struct InvHotbarSlot
{
	int hotbarIdx = 0;
	int itemId = 0;
	int modelId = 0;
	int amount = 0;
	std::string nameUtf8;
	RwTexture* texture = nullptr;
	bool isSnapshot = false;
	uint8_t snapAttempts = 0;
};

class InventoryUI : public Widget
{
public:
	InventoryUI();
	virtual ~InventoryUI();

	virtual void performLayout() override;
	virtual void draw(ImGuiRenderer* renderer) override;
	virtual void touchEvent(const ImVec2& pos, TouchType type) override;

	// Gameplay external hotbar (rendered & touched outside the bag window)
	void drawHudHotbar(ImGuiRenderer* renderer);
	bool handleHudTouch(const ImVec2& pos, TouchType type, int pointerId = 0);

	// Server sync protocol handlers ($MS|UI|... and $MS|INV|...)
	void SetServerUIVersion(int version);
	int GetEffectiveUIVersion() const;
	bool IsMode20() const { return GetEffectiveUIVersion() == 2; }

	void BeginSync(float usedWeight, float maxWeight, int usedSlots, int maxSlots);
	void AddSyncSlot(int slot, int itemId, int modelId, int amount, int maxStack,
		float weight, int category, bool favorite,
		float rotX, float rotY, float rotZ, float zoom, const std::string& nameUtf8);
	void SetSyncHotbar(int hslot, int itemId, int modelId, int amount, const std::string& nameUtf8);
	void ClearLiveHotbar();
	void FinishSyncAndShow();
	void CloseFromServer();

	void Show();
	void Hide(bool notifyServer = true);
	void ProcessSnapshots();

private:
	struct HitRect
	{
		ImVec2 min;
		ImVec2 max;
		bool contains(const ImVec2& p) const {
			return p.x >= min.x && p.x <= max.x && p.y >= min.y && p.y <= max.y;
		}
	};

	bool canShowHudHotbar() const;
	std::vector<const InvSlotItem*> getFilteredItems() const;
	bool isItemInHotbar(int itemId) const;
	void drawHeader(ImGuiRenderer* renderer, const ImVec2& panelMin, const ImVec2& panelMax);
	void drawCategoryIcon(ImGuiRenderer* renderer, int tabIndex, const ImVec2& center, float radius, const ImColor& col);
	void drawGrid(ImGuiRenderer* renderer, const ImVec2& gridMin, const ImVec2& gridMax);
	void drawDropdown(ImGuiRenderer* renderer);
	void closeDropdown();
	const InvSlotItem* findItemBySlot(int slot) const;
	int findHotbarIndexForItem(int itemId) const;
	void assignHotbarLocal(int hotbarIdx, const InvSlotItem* item);
	void clearHotbarForItemLocal(int itemId);
	void drawStar(ImGuiRenderer* renderer, const ImVec2& center, float r, const ImColor& col);
	RwTexture* getOrCreateModelSnapshot(int modelId, const CVector& rot, float zoom);
	RwTexture* getCustomItemTexture(int itemId);
	void clearSnapshotCache();

private:
	int m_serverUIVersion = 0; // 0 = unknown/use settings, 1 = Classic, 2 = Mod 2.0
	float m_usedWeight = 0.0f;
	float m_maxWeight = 80.0f;
	int m_usedSlots = 0;
	int m_maxSlots = 30;
	bool m_syncInProgress = false;

	std::vector<InvSlotItem> m_pendingItems;
	std::vector<InvSlotItem> m_items;
	InvHotbarSlot m_pendingHotbar[7];
	InvHotbarSlot m_hotbar[7];

	int m_activeCategory = 0; // 0=All, 1=Food/Potion, 2=Materials, 3=Equip/Weapon, 4=Other
	float m_scrollY = 0.0f;
	float m_maxScrollY = 0.0f;
	float m_scrollVel = 0.0f;

	// Layout rects computed during layout/draw for crisp hit testing
	ImVec2 m_panelMin = ImVec2(0, 0);
	ImVec2 m_panelMax = ImVec2(0, 0);
	ImVec2 m_gridMin = ImVec2(0, 0);
	ImVec2 m_gridMax = ImVec2(0, 0);
	HitRect m_closeBtnRect{};
	HitRect m_tabRects[5]{};
	HitRect m_scrollbarRect{};

	// ImGui item action dropdown (anchored beside tapped item card)
	bool m_dropdownOpen = false;
	bool m_dropdownHotbarExpanded = false;
	int m_dropdownSlot = -1;
	HitRect m_dropdownAnchorRect{};
	HitRect m_dropdownRect{};
	HitRect m_dropActionRects[4]{}; // 0=Use, 1=Hotbar toggle, 2=Drop all, 3=More server options
	HitRect m_dropHotbarRects[8]{}; // 0..6=slots 1..7, 7=clear hotbar

	// External HUD hotbar (outside bag)
	bool m_hudHotbarOpen = false;
	float m_hotbarAnim = 0.0f;
	HitRect m_hudArrowRect{};
	HitRect m_hudHotbarDockRect{};
	HitRect m_hudHotbarSlotRects[7]{};
	int m_hudTouchPointerId = -1;
	ImVec2 m_hudTouchStart = ImVec2(0, 0);
	float m_hudDragDist = 0.0f;

	struct CardHit
	{
		HitRect rect;
		int slot = -1;
	};
	std::vector<CardHit> m_cardHits;

	// Touch tracking inside bag
	bool m_touchDown = false;
	bool m_draggingGrid = false;
	bool m_draggingScrollbar = false;
	bool m_hasDragged = false;
	bool m_touchStartedInDropdown = false;
	ImVec2 m_touchStartPos = ImVec2(0, 0);
	ImVec2 m_lastTouchPos = ImVec2(0, 0);
	float m_totalDragDist = 0.0f;

	// Cached 3D snapshots keyed by modelId & custom TXD icons keyed by itemId
	std::unordered_map<int, RwTexture*> m_snapshotCache;
	std::unordered_map<int, RwTexture*> m_txdCache;
};
