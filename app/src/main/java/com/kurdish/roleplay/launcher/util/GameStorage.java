package com.kurdish.roleplay.launcher.util;

import android.content.Context;

import java.io.File;

public final class GameStorage {
    private static final String PACKAGE_DIR = "com.thundersamp.online";
    private static final String SAMP_DIR = "SAMP";

    private GameStorage() {
    }

    public static File getGameBaseDirectory(Context context) {
        // 1. Check if external files dir (Android/data/<applicationId>/files) has game data
        if (context != null) {
            File extFiles = context.getExternalFilesDir(null);
            if (extFiles != null && extFiles.exists()) {
                File texdb = new File(extFiles, "texdb");
                if (texdb.exists()) {
                    return extFiles;
                }
            }
        }

        // 2. Direct check for com.thundersamp.online in Android/data
        File thundersampFiles = new File("/storage/emulated/0/Android/data/com.thundersamp.online/files");
        if (thundersampFiles.exists()) {
            return thundersampFiles;
        }

        File thundersampDir = new File("/storage/emulated/0/Android/data/com.thundersamp.online");
        if (thundersampDir.exists()) {
            return thundersampDir;
        }

        // 3. Check Android/media
        File mediaRoot = new File("/storage/emulated/0/Android/media");
        File gameDir = new File(mediaRoot, PACKAGE_DIR);
        if (gameDir.exists()) {
            return gameDir;
        }

        // 4. Fallback to context files or media directory
        if (context != null) {
            File fallback = context.getExternalFilesDir(null);
            if (fallback != null) {
                return fallback;
            }
        }

        if (!gameDir.exists()) {
            gameDir.mkdirs();
        }
        return gameDir;
    }

    public static String getGameBasePath(Context context) {
        return withTrailingSlash(getGameBaseDirectory(context).getAbsolutePath());
    }

    public static File getSampDirectory(Context context) {
        File sampDir = new File(getGameBaseDirectory(context), SAMP_DIR);
        if (!sampDir.exists()) {
            sampDir.mkdirs();
        }
        return sampDir;
    }

    public static File getSettingsFile(Context context) {
        return new File(getSampDirectory(context), "settings.ini");
    }

    public static File getDownloadDirectory(Context context) {
        File downloadDir = new File(getGameBaseDirectory(context), "download");
        if (!downloadDir.exists()) {
            downloadDir.mkdirs();
        }
        return downloadDir;
    }

    private static String withTrailingSlash(String path) {
        return path.endsWith("/") ? path : path + "/";
    }
}
