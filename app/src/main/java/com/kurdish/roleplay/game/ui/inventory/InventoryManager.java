package com.kurdish.roleplay.game.ui.inventory;

import android.app.Activity;
import android.app.Dialog;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.util.Log;
import android.view.DragEvent;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;
import android.widget.Toast;

import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.kurdish.roleplay.R;
import com.kurdish.roleplay.game.SAMP;

import java.util.ArrayList;
import java.util.List;

public class InventoryManager {

    private static final String TAG = "InventoryManager";
    public static final int TOTAL_BAG_SLOTS = 30;
    public static final int TOTAL_HOTBAR_SLOTS = 7;

    private final Activity activity;
    private FrameLayout rootLayout;
    private RecyclerView recyclerGrid;
    private InventoryAdapter adapter;
    private final List<InventoryItem> bagItems = new ArrayList<>();
    private final List<InventoryItem> hotbarItems = new ArrayList<>();

    private TextView tvCapacity;
    private final View[] hotbarSlotViews = new View[TOTAL_HOTBAR_SLOTS];

    public InventoryManager(Activity activity) {
        this.activity = activity;
        initData();
        initViews();
    }

    private void initData() {
        for (int i = 0; i < TOTAL_BAG_SLOTS; i++) {
            bagItems.add(new InventoryItem(i, 0, "", 0, 100, 0));
        }
        for (int i = 0; i < TOTAL_HOTBAR_SLOTS; i++) {
            hotbarItems.add(new InventoryItem(i, 0, "", 0, 100, 0));
        }
    }

    private void initViews() {
        try {
            LayoutInflater inflater = LayoutInflater.from(activity);
            rootLayout = (FrameLayout) inflater.inflate(R.layout.inventory_view, null);

            tvCapacity = rootLayout.findViewById(R.id.tv_inv_capacity);
            View btnClose = rootLayout.findViewById(R.id.btn_close_inv);
            if (btnClose != null) {
                btnClose.setOnClickListener(v -> hide());
            }

            // Click backdrop to dismiss
            rootLayout.setOnClickListener(v -> hide());
            View mainCard = rootLayout.findViewById(R.id.card_inventory_main);
            if (mainCard != null) {
                mainCard.setOnClickListener(v -> {}); // consume click
            }

            // 🔹 Setup Recycler Grid (5 columns, smooth vertical scroll)
            recyclerGrid = rootLayout.findViewById(R.id.recycler_inv_grid);
            if (recyclerGrid != null) {
                recyclerGrid.setLayoutManager(new GridLayoutManager(activity, 5));
                adapter = new InventoryAdapter(bagItems, this::onItemClick);
                recyclerGrid.setAdapter(adapter);
            }

            // 🔹 Setup 7 Hotbar Slots with Drag & Drop Listener
            setupHotbarSlots();

            // Add to activity view tree
            activity.addContentView(rootLayout, new ViewGroup.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT,
                    ViewGroup.LayoutParams.MATCH_PARENT
            ));

            rootLayout.setVisibility(View.GONE);
        } catch (Throwable t) {
            Log.e(TAG, "Error initializing InventoryManager views", t);
        }
    }

    private void setupHotbarSlots() {
        int[] slotIds = {
                R.id.hotbar_slot_0, R.id.hotbar_slot_1, R.id.hotbar_slot_2,
                R.id.hotbar_slot_3, R.id.hotbar_slot_4, R.id.hotbar_slot_5,
                R.id.hotbar_slot_6
        };

        for (int i = 0; i < TOTAL_HOTBAR_SLOTS; i++) {
            final int hotbarIdx = i;
            View slotView = rootLayout.findViewById(slotIds[i]);
            hotbarSlotViews[i] = slotView;

            if (slotView != null) {
                TextView tvNum = slotView.findViewById(R.id.tv_hotbar_num);
                if (tvNum != null) {
                    tvNum.setText(String.valueOf(i + 1));
                }

                // 🌟 Native Drag & Drop Target
                slotView.setOnDragListener((v, event) -> {
                    switch (event.getAction()) {
                        case DragEvent.ACTION_DRAG_STARTED:
                            return true;

                        case DragEvent.ACTION_DRAG_ENTERED:
                            v.setBackgroundResource(R.drawable.bg_hotbar_slot_active);
                            return true;

                        case DragEvent.ACTION_DRAG_EXITED:
                        case DragEvent.ACTION_DRAG_ENDED:
                            v.setBackgroundResource(R.drawable.bg_hotbar_slot);
                            return true;

                        case DragEvent.ACTION_DROP:
                            v.setBackgroundResource(R.drawable.bg_hotbar_slot);
                            handleDropOnHotbar(hotbarIdx, event);
                            return true;
                    }
                    return false;
                });

                // 🌟 Tap Hotbar Slot to Immediately Use Item!
                slotView.setOnClickListener(v -> {
                    InventoryItem hItem = hotbarItems.get(hotbarIdx);
                    if (hItem != null && !hItem.isEmpty()) {
                        sendServerCommand("/hotbar use " + (hotbarIdx + 1));
                        hide();
                    } else {
                        Toast.makeText(activity, "ช่องคีย์ลัด " + (hotbarIdx + 1) + " ยังว่างอยู่ (ลากไอเทมมาวาง)", Toast.LENGTH_SHORT).show();
                    }
                });
            }
        }
    }

    private void handleDropOnHotbar(int hotbarIdx, DragEvent event) {
        try {
            if (event.getClipData() != null && event.getClipData().getItemCount() > 0) {
                String slotStr = event.getClipData().getItemAt(0).getText().toString();
                int fromSlot = Integer.parseInt(slotStr);

                if (fromSlot >= 0 && fromSlot < bagItems.size()) {
                    InventoryItem bagItem = bagItems.get(fromSlot);
                    if (bagItem != null && !bagItem.isEmpty()) {
                        // Update local hotbar item
                        InventoryItem hItem = new InventoryItem(
                                hotbarIdx, bagItem.itemId, bagItem.name, bagItem.amount, bagItem.maxStack, bagItem.iconResId
                        );
                        hotbarItems.set(hotbarIdx, hItem);
                        updateHotbarSlotUI(hotbarIdx);

                        // Send assignment command to server
                        sendServerCommand("/hotbar set " + fromSlot + " " + (hotbarIdx + 1));

                        Toast.makeText(activity, "ใส่ " + bagItem.name + " ลงช่องคีย์ลัด " + (hotbarIdx + 1) + " สำเร็จ", Toast.LENGTH_SHORT).show();
                    }
                }
            }
        } catch (Throwable t) {
            Log.e(TAG, "Error handling drop on hotbar", t);
        }
    }

    private void updateHotbarSlotUI(int hotbarIdx) {
        if (hotbarIdx < 0 || hotbarIdx >= TOTAL_HOTBAR_SLOTS) return;
        View slotView = hotbarSlotViews[hotbarIdx];
        if (slotView == null) return;

        InventoryItem item = hotbarItems.get(hotbarIdx);
        ImageView ivIcon = slotView.findViewById(R.id.iv_hotbar_icon);
        TextView tvCount = slotView.findViewById(R.id.tv_hotbar_count);

        if (item != null && !item.isEmpty()) {
            if (ivIcon != null) {
                ivIcon.setImageResource(item.iconResId != 0 ? item.iconResId : R.drawable.ic_inv_bag);
                ivIcon.setVisibility(View.VISIBLE);
            }
            if (tvCount != null) {
                if (item.amount > 1) {
                    tvCount.setText("x" + item.amount);
                    tvCount.setVisibility(View.VISIBLE);
                } else {
                    tvCount.setVisibility(View.GONE);
                }
            }
        } else {
            if (ivIcon != null) {
                ivIcon.setImageDrawable(null);
                ivIcon.setVisibility(View.INVISIBLE);
            }
            if (tvCount != null) {
                tvCount.setVisibility(View.GONE);
            }
        }
    }

    public void updateAllHotbarUI() {
        for (int i = 0; i < TOTAL_HOTBAR_SLOTS; i++) {
            updateHotbarSlotUI(i);
        }
    }

    private void onItemClick(InventoryItem item, int position) {
        if (item == null || item.isEmpty()) return;
        showItemActionDialog(item, position);
    }

    private void showItemActionDialog(InventoryItem item, int slotPosition) {
        try {
            Dialog dialog = new Dialog(activity);
            dialog.requestWindowFeature(Window.FEATURE_NO_TITLE);
            dialog.setContentView(R.layout.dialog_item_action);

            if (dialog.getWindow() != null) {
                dialog.getWindow().setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
            }

            ImageView ivIcon = dialog.findViewById(R.id.dialog_act_icon);
            TextView tvName = dialog.findViewById(R.id.dialog_act_name);
            TextView tvAmount = dialog.findViewById(R.id.dialog_act_amount);

            if (ivIcon != null) {
                ivIcon.setImageResource(item.iconResId != 0 ? item.iconResId : R.drawable.ic_inv_bag);
            }
            if (tvName != null) {
                tvName.setText(item.name);
            }
            if (tvAmount != null) {
                tvAmount.setText("จำนวน: " + item.amount + " / " + item.maxStack + " ชิ้น");
            }

            // 1. Use Item
            View btnUse = dialog.findViewById(R.id.btn_action_use);
            if (btnUse != null) {
                btnUse.setOnClickListener(v -> {
                    dialog.dismiss();
                    sendServerCommand("/inv use " + slotPosition);
                    hide();
                });
            }

            // 2. Put on Hotbar
            View btnHotbar = dialog.findViewById(R.id.btn_action_hotbar);
            if (btnHotbar != null) {
                btnHotbar.setOnClickListener(v -> {
                    dialog.dismiss();
                    showHotbarPickerDialog(item, slotPosition);
                });
            }

            // 3. Give
            View btnGive = dialog.findViewById(R.id.btn_action_give);
            if (btnGive != null) {
                btnGive.setOnClickListener(v -> {
                    dialog.dismiss();
                    sendServerCommand("/inv give " + slotPosition);
                });
            }

            // 4. Drop
            View btnDrop = dialog.findViewById(R.id.btn_action_drop);
            if (btnDrop != null) {
                btnDrop.setOnClickListener(v -> {
                    dialog.dismiss();
                    sendServerCommand("/inv drop " + slotPosition);
                });
            }

            // Cancel
            View btnCancel = dialog.findViewById(R.id.btn_action_cancel);
            if (btnCancel != null) {
                btnCancel.setOnClickListener(v -> dialog.dismiss());
            }

            dialog.show();
        } catch (Throwable t) {
            Log.e(TAG, "Error showing item action dialog", t);
        }
    }

    private void showHotbarPickerDialog(InventoryItem item, int slotPosition) {
        String[] options = new String[TOTAL_HOTBAR_SLOTS];
        for (int i = 0; i < TOTAL_HOTBAR_SLOTS; i++) {
            InventoryItem h = hotbarItems.get(i);
            options[i] = "ช่องคีย์ลัด " + (i + 1) + (h != null && !h.isEmpty() ? " [" + h.name + "]" : " [ว่าง]");
        }

        android.app.AlertDialog.Builder builder = new android.app.AlertDialog.Builder(activity);
        builder.setTitle("เลือกช่องคีย์ลัด (1-7)");
        builder.setItems(options, (dialog, which) -> {
            InventoryItem hItem = new InventoryItem(which, item.itemId, item.name, item.amount, item.maxStack, item.iconResId);
            hotbarItems.set(which, hItem);
            updateHotbarSlotUI(which);
            sendServerCommand("/hotbar set " + slotPosition + " " + (which + 1));
            Toast.makeText(activity, "ตั้งค่าช่องคีย์ลัด " + (which + 1) + " เรียบร้อยแล้ว", Toast.LENGTH_SHORT).show();
        });
        builder.setNegativeButton("ยกเลิก", null);
        builder.show();
    }

    public void setBagSlot(int slot, int itemId, String name, int amount, int maxStack) {
        if (slot < 0 || slot >= TOTAL_BAG_SLOTS) return;
        int iconRes = getIconForItem(itemId);
        bagItems.set(slot, new InventoryItem(slot, itemId, name, amount, maxStack, iconRes));
    }

    public void setHotbarSlot(int hotbarSlot, int itemId, String name, int amount) {
        if (hotbarSlot < 0 || hotbarSlot >= TOTAL_HOTBAR_SLOTS) return;
        int iconRes = getIconForItem(itemId);
        hotbarItems.set(hotbarSlot, new InventoryItem(hotbarSlot, itemId, name, amount, 100, iconRes));
    }

    public void updateCapacityBadge() {
        int used = 0;
        for (InventoryItem it : bagItems) {
            if (it != null && !it.isEmpty()) used++;
        }
        if (tvCapacity != null) {
            tvCapacity.setText(used + " / " + TOTAL_BAG_SLOTS + " ช่อง");
        }
    }

    public int getIconForItem(int itemId) {
        // Weapons 1-46
        if (itemId >= 1 && itemId <= 46) {
            int resId = activity.getResources().getIdentifier("weapon_" + itemId, "drawable", activity.getPackageName());
            if (resId != 0) return resId;
        }
        // Food / Drink
        if (itemId >= 50 && itemId <= 70) {
            return R.drawable.eat;
        }
        return R.drawable.ic_inv_bag;
    }

    public void show() {
        activity.runOnUiThread(() -> {
            if (rootLayout != null) {
                updateCapacityBadge();
                updateAllHotbarUI();
                if (adapter != null) adapter.notifyDataSetChanged();
                rootLayout.setVisibility(View.VISIBLE);
            }
        });
    }

    public void hide() {
        activity.runOnUiThread(() -> {
            if (rootLayout != null) {
                rootLayout.setVisibility(View.GONE);
            }
        });
    }

    public void toggle() {
        if (isOpen()) {
            hide();
        } else {
            show();
        }
    }

    public boolean isOpen() {
        return rootLayout != null && rootLayout.getVisibility() == View.VISIBLE;
    }

    private void sendServerCommand(String command) {
        try {
            SAMP samp = SAMP.getInstance();
            if (samp != null) {
                samp.sendCommand(command);
            }
        } catch (Throwable t) {
            Log.e(TAG, "Error sending server command: " + command, t);
        }
    }
}
