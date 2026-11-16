package com.kurdish.roleplay.launcher.util;

import android.app.Activity;
import android.content.Context;
import android.content.SharedPreferences;

public class Config {

    public static Activity currentContext;

    public static final String PREF_NAME = "game_update_config";
    public static final String KEY_DATA_URL = "custom_data_url";
    public static final String KEY_MANIFEST_URL = "custom_manifest_url";
    public static final String KEY_GPU_TYPE = "gpu_type";

    // Default download source (User-configurable Web Hosting or GitHub Release raw)
    public static final String DEFAULT_DATA_URL = "https://raw.githubusercontent.com/newkii3099/Build/main/data/";
    public static final String DEFAULT_MANIFEST_URL = "https://raw.githubusercontent.com/newkii3099/Build/main/files.json";

    public static String getDataUrl(Context context) {
        if (context == null) return DEFAULT_DATA_URL;
        SharedPreferences prefs = context.getSharedPreferences(PREF_NAME, Context.MODE_PRIVATE);
        return prefs.getString(KEY_DATA_URL, DEFAULT_DATA_URL);
    }

    public static void setDataUrl(Context context, String url) {
        if (context == null || url == null) return;
        SharedPreferences prefs = context.getSharedPreferences(PREF_NAME, Context.MODE_PRIVATE);
        prefs.edit().putString(KEY_DATA_URL, url.trim()).apply();
    }

    public static String getManifestUrl(Context context) {
        if (context == null) return DEFAULT_MANIFEST_URL;
        SharedPreferences prefs = context.getSharedPreferences(PREF_NAME, Context.MODE_PRIVATE);
        return prefs.getString(KEY_MANIFEST_URL, DEFAULT_MANIFEST_URL);
    }

    public static void setManifestUrl(Context context, String url) {
        if (context == null || url == null) return;
        SharedPreferences prefs = context.getSharedPreferences(PREF_NAME, Context.MODE_PRIVATE);
        prefs.edit().putString(KEY_MANIFEST_URL, url.trim()).apply();
    }

    public static int getGpuType(Context context) {
        if (context == null) return 2; // Default ETC/Mali
        SharedPreferences prefs = context.getSharedPreferences(PREF_NAME, Context.MODE_PRIVATE);
        return prefs.getInt(KEY_GPU_TYPE, 2);
    }

    public static void setGpuType(Context context, int gpuType) {
        if (context == null || gpuType <= 0) return;
        SharedPreferences prefs = context.getSharedPreferences(PREF_NAME, Context.MODE_PRIVATE);
        prefs.edit().putInt(KEY_GPU_TYPE, gpuType).apply();
    }
}
