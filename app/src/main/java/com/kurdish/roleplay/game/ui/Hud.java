package com.kurdish.roleplay.game.ui;

import android.app.Activity;
import android.net.Uri;
import android.os.Handler;
import android.text.TextUtils;
import android.util.Log;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.constraintlayout.widget.ConstraintLayout;

import com.android.volley.Request;
import com.android.volley.RequestQueue;
import com.android.volley.toolbox.StringRequest;
import com.android.volley.toolbox.Volley;
import com.kurdish.roleplay.R;
import com.kurdish.roleplay.launcher.util.RemoteConfigManager;
import com.kurdish.roleplay.launcher.util.SeekArc;
import com.kurdish.roleplay.launcher.util.Util;

import java.text.DecimalFormat;
import java.text.DecimalFormatSymbols;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;
import java.util.Formatter;

import org.json.JSONArray;
import org.json.JSONException;


public class Hud {

    public Activity activity;
    public ConstraintLayout hud_layout;
    public native void WeaponChanged();

    public ImageView hud_camera_butt;

    public TextView hud_money, hud_ammo, bank_money, time;
    public ImageView hud_weapon;
    public SeekArc hud_health, hud_armour, hud_eat, hud_drink;

    private final Handler handler = new Handler();
    private final RequestQueue requestQueue;
    private final Runnable onlineRefresh = this::fetchOnline;
    private boolean paused;
    private int visibilityBeforePause = View.GONE;

    public TextView textPlayers, PlayerId;

    private native void clickCameraMode();
    private native void clickKeyboard(boolean isJavaShowing);
    private native void clickInventoryBag();


    public Hud(Activity aactivity) {
        activity = aactivity;
        requestQueue = Volley.newRequestQueue(aactivity.getApplicationContext());
        hud_layout = aactivity.findViewById(R.id.gamehud);

        if (hud_layout == null) {
            Log.e("Hud", "Hud layout is null! Check setContentView.");
            return;
        }

        hud_camera_butt = hud_layout.findViewById(R.id.hud_camera);
        textPlayers = hud_layout.findViewById(R.id.textPlayers);
        hud_health = hud_layout.findViewById(R.id.hud_progress_hp);
        hud_armour = hud_layout.findViewById(R.id.hud_progress_shp);
        hud_eat = hud_layout.findViewById(R.id.hud_progress_eat);
        hud_drink = hud_layout.findViewById(R.id.hud_progress_drink);
        hud_money = hud_layout.findViewById(R.id.money_text);
        bank_money = hud_layout.findViewById(R.id.bank_money_text);
        hud_weapon = hud_layout.findViewById(R.id.hud_weapon);
        PlayerId = hud_layout.findViewById(R.id.yourid);
        hud_ammo = hud_layout.findViewById(R.id.weapon_ammo_text);
        time = hud_layout.findViewById(R.id.realTime);

        View hudCameraTarget = hud_layout.findViewById(R.id.hud_camera_container);
        if (hudCameraTarget == null) hudCameraTarget = hud_camera_butt;
        if (hudCameraTarget != null) {
            hudCameraTarget.setOnClickListener(v -> clickCameraMode());
        }
        if (hud_weapon != null) {
            hud_weapon.setOnClickListener(v -> WeaponChanged());
        }

        View hudKeyboardBtn = hud_layout.findViewById(R.id.hud_keyboard_container);
        if (hudKeyboardBtn == null) hudKeyboardBtn = hud_layout.findViewById(R.id.hud_keyboard_btn);
        if (hudKeyboardBtn != null) {
            hudKeyboardBtn.setOnClickListener(v -> {
                com.kurdish.roleplay.game.SAMP samp = com.kurdish.roleplay.game.SAMP.getInstance();
                boolean javaShowing = samp != null && samp.isKeyboardShowing();
                clickKeyboard(javaShowing);
            });
        }

        View hudInventoryBtn = hud_layout.findViewById(R.id.hud_inventory_container);
        if (hudInventoryBtn == null) hudInventoryBtn = hud_layout.findViewById(R.id.hud_inventory_btn);
        if (hudInventoryBtn != null) {
            hudInventoryBtn.setOnClickListener(v -> clickInventoryBag());
        }

        // Permanently hide Java launcher HUD overlay (money, bars, weapon, bag/camera buttons)
        Util.HideLayout(hud_layout, false);
        hud_layout.setVisibility(View.GONE);
    }

    private void startUpdatingTime() {
        // HUD permanently disabled
    }

    private void fetchOnline() {
        // HUD permanently disabled
    }

    private boolean isHttpUrl(String value) {
        if (TextUtils.isEmpty(value)) {
            return false;
        }

        Uri uri = Uri.parse(value.trim());
        String scheme = uri.getScheme();
        return ("http".equalsIgnoreCase(scheme) || "https".equalsIgnoreCase(scheme))
                && !TextUtils.isEmpty(uri.getHost());
    }

    private void scheduleOnlineRefresh(long delayMillis) {
        handler.removeCallbacks(onlineRefresh);
        handler.postDelayed(onlineRefresh, delayMillis);
    }

    private void updateTime() {
        if (time != null) {
            String currentTime = new SimpleDateFormat("HH:mm", Locale.getDefault())
                    .format(new Date());
            time.setText(currentTime);
        }
    }

    public void UpdateHudInfo(int health, int armour, int weaponid, int ammo, int ammoinclip,
                              int money, int eat, int drink, int bankMoney, int playerid)
    {
        // HUD permanently disabled
    }

    public void ShowHud() {
        // HUD permanently disabled
        if (hud_layout != null && hud_layout.getVisibility() != View.GONE) {
            hud_layout.setVisibility(View.GONE);
        }
    }

    public void HideHud() {
        if (hud_layout != null && hud_layout.getVisibility() != View.GONE) {
            Util.HideLayout(hud_layout, false);
            hud_layout.setVisibility(View.GONE);
        }
    }

    public void setPaused(boolean pause) {
        paused = pause;
        visibilityBeforePause = View.GONE;
        if (hud_layout != null) {
            hud_layout.clearFocus();
            hud_layout.setVisibility(View.GONE);
        }
    }
}
