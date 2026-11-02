package com.kurdish.roleplay.game.ui;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.graphics.Color;
import android.util.DisplayMetrics;
import android.view.Gravity;
import android.view.View;
import android.webkit.JavascriptInterface;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.FrameLayout;

import com.kurdish.roleplay.game.SAMP;

import org.json.JSONObject;

import java.nio.charset.StandardCharsets;
import java.util.ArrayList;

/** Transparent HTML chat overlay. Native code calls SAMP.addChatMessage(...). */
public class ChatWebView {

    private final Activity activity;
    private final WebView web;
    private boolean ready = false;
    private final ArrayList<String> pending = new ArrayList<>();

    public class ChatJsInterface {
        @JavascriptInterface
        public void openChatInput() {
            activity.runOnUiThread(() -> {
                if (activity instanceof SAMP) {
                    ((SAMP) activity).showKeyboard();
                }
            });
        }

        @JavascriptInterface
        public void onToggleChat(boolean collapsed) {
            activity.runOnUiThread(() -> {
                DisplayMetrics dm = activity.getResources().getDisplayMetrics();
                FrameLayout.LayoutParams lp = (FrameLayout.LayoutParams) web.getLayoutParams();
                if (lp != null) {
                    if (collapsed) {
                        lp.width = (int) (dm.density * 68);
                        lp.height = (int) (dm.density * 36);
                    } else {
                        lp.width = (int) (dm.widthPixels * 0.46f);
                        lp.height = (int) (dm.heightPixels * 0.40f);
                    }
                    web.setLayoutParams(lp);
                }
            });
        }
    }

    @SuppressLint("SetJavaScriptEnabled")
    public ChatWebView(Activity a) {
        activity = a;
        web = new WebView(a);
        web.setBackgroundColor(Color.TRANSPARENT);
        web.setLayerType(View.LAYER_TYPE_HARDWARE, null);
        web.setFocusable(false);
        web.setFocusableInTouchMode(false);
        web.setHorizontalScrollBarEnabled(false);
        web.setVerticalScrollBarEnabled(false);

        WebSettings s = web.getSettings();
        s.setJavaScriptEnabled(true);
        s.setAllowFileAccess(true);          // file:///android_asset only
        s.setAllowFileAccessFromFileURLs(true);
        s.setAllowUniversalAccessFromFileURLs(true);
        s.setAllowContentAccess(false);
        s.setDomStorageEnabled(false);
        s.setCacheMode(WebSettings.LOAD_NO_CACHE);

        web.addJavascriptInterface(new ChatJsInterface(), "Android");

        web.setWebViewClient(new WebViewClient() {
            @Override public void onPageFinished(WebView v, String url) {
                ready = true;
                for (String js : pending) v.evaluateJavascript(js, null);
                pending.clear();
            }
        });
        web.loadUrl("file:///android_asset/chat/index.html");

        DisplayMetrics dm = a.getResources().getDisplayMetrics();
        FrameLayout.LayoutParams lp = new FrameLayout.LayoutParams(
                (int) (dm.widthPixels * 0.46f), (int) (dm.heightPixels * 0.40f),
                Gravity.TOP | Gravity.START);
        lp.leftMargin = (int) (dm.widthPixels * 0.16f);   // clear of the radar
        lp.topMargin = (int) (dm.heightPixels * 0.02f);
        a.addContentView(web, lp);
    }

    /** nick may be empty (server/system line). Colors are 0xRRGGBB. */
    public void addMessage(byte[] nick, byte[] text, int nickRgb, int textRgb) {
        String n = new String(nick, StandardCharsets.UTF_8);
        String t = new String(text, StandardCharsets.UTF_8);
        try {
            JSONObject o = new JSONObject();
            if (!n.isEmpty()) o.put("name", n);
            o.put("text", t);
            o.put("color", String.format("#%06X", nickRgb & 0xFFFFFF));
            o.put("textColor", String.format("#%06X", textRgb & 0xFFFFFF));
            o.put("system", n.isEmpty());
            String json = o.toString().replace("\u2028", "\\u2028").replace("\u2029", "\\u2029");
            final String js = "addMsg(" + json + ")";
            activity.runOnUiThread(() -> {
                if (ready) web.evaluateJavascript(js, null); else pending.add(js);
            });
        } catch (Exception ignored) { }
    }

    public void setVisible(boolean v) {
        activity.runOnUiThread(() -> web.setVisibility(v ? View.VISIBLE : View.GONE));
    }
}
