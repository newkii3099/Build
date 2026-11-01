package com.kurdish.roleplay.launcher.util;

import android.content.Context;
import android.content.res.AssetManager;

import org.ini4j.Wini;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

public class ConfigValidator {
    public static void validateConfigFiles(Context context) {
        File file = GameStorage.getSettingsFile(context);
        if (!file.exists() || file.length() == 0) {
            if (file.getParentFile() != null) {
                file.getParentFile().mkdirs();
            }
            boolean copied = copyAsset(context.getAssets(), "settings.ini", file.toString());
            if (!copied) {
                try {
                    FileOutputStream fos = new FileOutputStream(file);
                    StringBuilder sb = new StringBuilder();
                    sb.append("[client]\n");
                    sb.append("name=NextCity_Player\n");
                    sb.append("password=\n");
                    sb.append("version=0.3.7\n\n");
                    sb.append("[gui]\n");
                    sb.append("androidkeyboard=false\n");
                    sb.append("VoiceChatEnable=true\n");
                    sb.append("fps=0\n");
                    sb.append("ChatMaxMessages=6\n");
                    sb.append("FPSLimit=120\n");
                    fos.write(sb.toString().getBytes());
                    fos.flush();
                    fos.close();
                } catch (IOException e) {
                    e.printStackTrace();
                }
            }
        }
    }

    public static String getNickName(Context context) {
        if (context == null) return "NextCity_Player";
        try {
            File file = GameStorage.getSettingsFile(context);
            if (!file.exists()) {
                validateConfigFiles(context);
            }
            if (file.exists()) {
                Wini wini = new Wini(file);
                String name = wini.get("client", "name");
                if (name != null && !name.trim().isEmpty()) {
                    return name.trim();
                }
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
        String saved = new SharedPreferenceCore().getString(context, "NICKNAME");
        if (saved != null && !saved.trim().isEmpty()) {
            return saved.trim();
        }
        return "NextCity_Player";
    }

    public static void setNickName(Context context, String nickname) {
        if (context == null || nickname == null || nickname.trim().isEmpty()) return;
        String trimmed = nickname.trim();
        new SharedPreferenceCore().setString(context, "NICKNAME", trimmed);
        try {
            File file = GameStorage.getSettingsFile(context);
            if (!file.exists()) {
                validateConfigFiles(context);
            }
            Wini wini = new Wini(file);
            wini.put("client", "name", trimmed);
            wini.store();
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    static boolean copyAsset(AssetManager assetManager, String str, String str2) {
        try {
            InputStream open = assetManager.open(str);
            new File(str2).createNewFile();
            FileOutputStream fileOutputStream = new FileOutputStream(str2);
            copyFile(open, fileOutputStream);
            open.close();
            fileOutputStream.flush();
            fileOutputStream.close();
            return true;
        } catch (Exception e) {
            e.printStackTrace();
            return false;
        }
    }

    static void copyFile(InputStream inputStream, OutputStream outputStream) throws IOException {
        byte[] bArr = new byte[1024];
        while (true) {
            int read = inputStream.read(bArr);
            if (read != -1) {
                outputStream.write(bArr, 0, read);
            } else {
                return;
            }
        }
    }
}
