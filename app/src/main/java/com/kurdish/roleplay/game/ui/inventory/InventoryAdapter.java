package com.kurdish.roleplay.game.ui.inventory;

import android.content.ClipData;
import android.content.ClipDescription;
import android.os.Build;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;

import com.kurdish.roleplay.R;

import java.util.List;

public class InventoryAdapter extends RecyclerView.Adapter<InventoryAdapter.ViewHolder> {

    public interface OnItemClickListener {
        void onItemClick(InventoryItem item, int position);
    }

    private final List<InventoryItem> items;
    private final OnItemClickListener clickListener;

    public InventoryAdapter(List<InventoryItem> items, OnItemClickListener clickListener) {
        this.items = items;
        this.clickListener = clickListener;
    }

    @NonNull
    @Override
    public ViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
        View view = LayoutInflater.from(parent.getContext())
                .inflate(R.layout.item_inv_slot, parent, false);
        return new ViewHolder(view);
    }

    @Override
    public void onBindViewHolder(@NonNull ViewHolder holder, int position) {
        InventoryItem item = items.get(position);

        if (item != null && !item.isEmpty()) {
            holder.tvName.setText(item.name);
            holder.tvName.setVisibility(View.VISIBLE);

            if (item.amount > 1) {
                holder.tvCount.setText("x" + item.amount);
                holder.tvCount.setVisibility(View.VISIBLE);
            } else {
                holder.tvCount.setVisibility(View.GONE);
            }

            if (item.iconResId != 0) {
                holder.ivIcon.setImageResource(item.iconResId);
                holder.ivIcon.setVisibility(View.VISIBLE);
            } else {
                holder.ivIcon.setImageResource(R.drawable.ic_inv_bag);
                holder.ivIcon.setVisibility(View.VISIBLE);
            }

            // 🌟 Native Android Drag & Drop on Long Press!
            holder.itemView.setOnLongClickListener(v -> {
                ClipData.Item clipItem = new ClipData.Item(String.valueOf(position));
                ClipData dragData = new ClipData(
                        "INVENTORY_SLOT",
                        new String[]{ClipDescription.MIMETYPE_TEXT_PLAIN},
                        clipItem
                );

                View.DragShadowBuilder shadow = new View.DragShadowBuilder(holder.itemView);

                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
                    v.startDragAndDrop(dragData, shadow, item, 0);
                } else {
                    v.startDrag(dragData, shadow, item, 0);
                }
                return true;
            });

            holder.itemView.setOnClickListener(v -> {
                if (clickListener != null) {
                    clickListener.onItemClick(item, position);
                }
            });

        } else {
            // Empty slot
            holder.tvName.setText("");
            holder.tvName.setVisibility(View.GONE);
            holder.tvCount.setVisibility(View.GONE);
            holder.ivIcon.setImageDrawable(null);
            holder.ivIcon.setVisibility(View.INVISIBLE);
            holder.itemView.setOnLongClickListener(null);
            holder.itemView.setOnClickListener(null);
        }
    }

    @Override
    public int getItemCount() {
        return items.size();
    }

    public static class ViewHolder extends RecyclerView.ViewHolder {
        public final TextView tvName;
        public final TextView tvCount;
        public final ImageView ivIcon;

        public ViewHolder(@NonNull View itemView) {
            super(itemView);
            tvName = itemView.findViewById(R.id.tv_item_name);
            tvCount = itemView.findViewById(R.id.tv_item_count);
            ivIcon = itemView.findViewById(R.id.iv_item_icon);
        }
    }
}
