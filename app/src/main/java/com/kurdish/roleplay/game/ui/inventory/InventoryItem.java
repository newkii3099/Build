package com.kurdish.roleplay.game.ui.inventory;

public class InventoryItem {
    public int slot;
    public int itemId;
    public String name;
    public int amount;
    public int maxStack;
    public int iconResId;

    public InventoryItem(int slot, int itemId, String name, int amount, int maxStack, int iconResId) {
        this.slot = slot;
        this.itemId = itemId;
        this.name = name != null ? name : "";
        this.amount = amount;
        this.maxStack = maxStack > 0 ? maxStack : 100;
        this.iconResId = iconResId;
    }

    public boolean isEmpty() {
        return itemId <= 0 || amount <= 0;
    }
}
