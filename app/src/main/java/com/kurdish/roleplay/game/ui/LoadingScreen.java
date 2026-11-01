package com.kurdish.roleplay.game.ui;

import android.app.Activity;
import android.os.Handler;
import android.os.Looper;
import android.view.View;
import android.view.animation.Animation;
import android.view.animation.AnimationUtils;
import android.view.ViewGroup;
import android.widget.ProgressBar;
import android.widget.TextView;
import android.widget.ImageView;

import com.kurdish.roleplay.R;

public class LoadingScreen {

    private Activity activity;
    private View mainLayout;
    private ProgressBar progressBar;
    private TextView progressPercent;
    private TextView loadingText;
    private View dot1, dot2, dot3;

    private Handler handler;
    private Runnable progressRunnable;
    private int progress = 0;
    private static final int TOTAL_DURATION = 4000;
    private static final int UPDATE_INTERVAL = 40;


    public LoadingScreen(Activity activity) {
        this.activity = activity;

        try {
            mainLayout = activity.getLayoutInflater()
                    .inflate(R.layout.loadingscreen, null);
            activity.addContentView(mainLayout,
                    new ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));

            initializeViews();
            setupAnimations();
            startProgressAnimation();
        } catch (Throwable t) {
            android.util.Log.e("LoadingScreen", "Error initializing LoadingScreen", t);
        }
    }

    private void initializeViews() {
        if (mainLayout == null) return;
        progressBar = mainLayout.findViewById(R.id.progressBarr);
        progressPercent = mainLayout.findViewById(R.id.progressPercent);
        loadingText = mainLayout.findViewById(R.id.loadingText);
        dot1 = mainLayout.findViewById(R.id.dot1);
        dot2 = mainLayout.findViewById(R.id.dot2);
        dot3 = mainLayout.findViewById(R.id.dot3);

        handler = new Handler(Looper.getMainLooper());
    }

    private void setupAnimations() {
        try {
            Animation dotAnimation = AnimationUtils.loadAnimation(activity, R.anim.loading_dots);
            if (dot1 != null) dot1.startAnimation(dotAnimation);

            Animation dotAnimation2 = AnimationUtils.loadAnimation(activity, R.anim.loading_dots);
            dotAnimation2.setStartOffset(200);
            if (dot2 != null) dot2.startAnimation(dotAnimation2);

            Animation dotAnimation3 = AnimationUtils.loadAnimation(activity, R.anim.loading_dots);
            dotAnimation3.setStartOffset(400);
            if (dot3 != null) dot3.startAnimation(dotAnimation3);

            ImageView loadingIcon = mainLayout.findViewById(R.id.loadingIcon);
            if (loadingIcon != null) {
                Animation rotateAnimation =
                        AnimationUtils.loadAnimation(activity, R.anim.rotate_animation);
                loadingIcon.startAnimation(rotateAnimation);
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    private void startProgressAnimation() {
        progressRunnable = new Runnable() {
            @Override
            public void run() {
                if (progress < 100) {
                    progress++;
                    updateProgress(progress);
                    updateLoadingText(progress);
                    handler.postDelayed(this, UPDATE_INTERVAL);
                } else {
                    onLoadingComplete();
                }
            }
        };

        handler.postDelayed(progressRunnable, 500);
    }

    private void updateProgress(int progress) {
        progressBar.setProgress(progress);
        progressPercent.setText(progress + "%");
    }

    private void updateLoadingText(int progress) {
        String[] loadingMessages = {
                "กำลังโหลดทรัพยากรตัวเกม...",
                "กำลังเตรียมข้อมูลระบบ...",
                "กำลังเชื่อมต่อเข้าสู่เซิร์ฟเวอร์...",
                "เกือบเสร็จสมบูรณ์แล้ว...",
                "ยินดีต้อนรับสู่ NextCity!"
        };

        int messageIndex = progress / 20;
        if (messageIndex < loadingMessages.length) {
            loadingText.setText(loadingMessages[messageIndex]);
        }
    }

    private void onLoadingComplete() {
        if (mainLayout == null) return;
        try {
            Animation fadeOut = AnimationUtils.loadAnimation(activity, R.anim.fade_out);
            fadeOut.setAnimationListener(new Animation.AnimationListener() {
                @Override
                public void onAnimationStart(Animation animation) { }

                @Override
                public void onAnimationEnd(Animation animation) {
                    hide();
                }

                @Override
                public void onAnimationRepeat(Animation animation) { }
            });
            mainLayout.startAnimation(fadeOut);
        } catch (Exception e) {
            hide();
        }
    }

    public void hide() {
        if (handler != null && progressRunnable != null) {
            handler.removeCallbacks(progressRunnable);
        }

        if (dot1 != null) dot1.clearAnimation();
        if (dot2 != null) dot2.clearAnimation();
        if (dot3 != null) dot3.clearAnimation();

        if (mainLayout != null) {
            mainLayout.setVisibility(View.GONE);
        }
    }

    public void show() {
        if (mainLayout != null) {
            mainLayout.setVisibility(View.VISIBLE);
        }
        progress = 0;
        if (progressBar != null) progressBar.setProgress(0);
        startProgressAnimation();
    }

    public void destroy() {
        hide();
        handler = null;
        progressRunnable = null;
    }
}
