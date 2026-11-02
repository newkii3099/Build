package com.kurdish.roleplay.launcher;

import android.app.Dialog;
import android.content.Intent;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.net.Uri;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.animation.Animation;
import android.view.animation.AnimationUtils;
import android.widget.EditText;
import android.widget.ImageView;
import android.widget.TextView;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;

import com.kurdish.roleplay.R;
import com.kurdish.roleplay.game.SAMP;
import com.kurdish.roleplay.launcher.util.ConfigValidator;
import com.kurdish.roleplay.launcher.util.RemoteConfigManager;
import com.kurdish.roleplay.launcher.util.SampQueryAPI;

public class HomeFragment extends Fragment {

    public static final String LOCKED_SERVER_HOST = "82.26.104.84";
    public static final int LOCKED_SERVER_PORT = 7777;

    private Animation scaleDown;
    private Animation scaleUp;
    private TextView tvHeaderPlayerNick;
    private TextView tvPlayerNickSubtitle;

    private TextView tvServerLiveStatus;
    private TextView tvServerHostname;
    private TextView tvServerRealPlayers;
    private TextView tvServerRealPing;
    private SampQueryAPI.ServerInfo lastServerInfo;

    @Nullable
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater,
                             @Nullable ViewGroup container,
                             @Nullable Bundle savedInstanceState) {

        View view = inflater.inflate(R.layout.fragment_home, container, false);

        initAnimations();

        // 🎯 Header Actions
        View btnNotification = view.findViewById(R.id.btnNotification);
        View btnSettingsTop = view.findViewById(R.id.btnSettingsTop);
        View btnQuickNickname = view.findViewById(R.id.btnQuickNickname);
        tvHeaderPlayerNick = view.findViewById(R.id.tvHeaderPlayerNick);

        // ◀️ Left Column Actions
        View btnPlay = view.findViewById(R.id.btnPlay);
        tvPlayerNickSubtitle = view.findViewById(R.id.tvPlayerNickSubtitle);
        View btnActionServers = view.findViewById(R.id.btnActionServers);
        View btnActionSettings = view.findViewById(R.id.btnActionSettings);
        View btnActionDiscord = view.findViewById(R.id.btnActionDiscord);
        View btnActionFacebook = view.findViewById(R.id.btnActionFacebook);
        View btnActionAnnouncement = view.findViewById(R.id.btnActionAnnouncement);

        // ▶️ Right Column Cards & Live Server Card
        tvServerLiveStatus = view.findViewById(R.id.tvServerLiveStatus);
        tvServerHostname = view.findViewById(R.id.tvServerHostname);
        tvServerRealPlayers = view.findViewById(R.id.tvServerRealPlayers);
        tvServerRealPing = view.findViewById(R.id.tvServerRealPing);
        View btnRefreshServerStats = view.findViewById(R.id.btnRefreshServerStats);
        View serverItemNextCity = view.findViewById(R.id.serverItemNextCity);

        View cardTeamAnnouncement = view.findViewById(R.id.cardTeamAnnouncement);
        View btnViewAllNews = view.findViewById(R.id.btnViewAllNews);

        // ⚡ Bottom Navigation Shortcuts
        View navReviews = view.findViewById(R.id.navReviews);
        View navEvents = view.findViewById(R.id.navEvents);
        View navRankings = view.findViewById(R.id.navRankings);
        View navSupport = view.findViewById(R.id.navSupport);

        // Attach touch feedback animations
        attachTouchAnimation(btnPlay);
        attachTouchAnimation(btnActionServers);
        attachTouchAnimation(btnActionSettings);
        attachTouchAnimation(btnActionDiscord);
        attachTouchAnimation(btnActionFacebook);
        attachTouchAnimation(btnActionAnnouncement);
        attachTouchAnimation(btnNotification);
        attachTouchAnimation(btnSettingsTop);
        attachTouchAnimation(btnQuickNickname);

        // 0. Quick Nickname Edit
        if (btnQuickNickname != null) {
            btnQuickNickname.setOnClickListener(v -> showChangeNicknameDialog());
        }
        refreshPlayerNickname();

        // 1. Play Button Click -> Launch SAMP Game
        if (btnPlay != null) {
            btnPlay.setOnClickListener(v -> launchGame());
        }

        // 2. Settings Buttons
        View.OnClickListener openSettingsListener = v -> {
            if (getActivity() instanceof MainActivity) {
                ((MainActivity) getActivity()).openSettings();
            }
        };
        if (btnActionSettings != null) btnActionSettings.setOnClickListener(openSettingsListener);
        if (btnSettingsTop != null) btnSettingsTop.setOnClickListener(openSettingsListener);

        // 3. Discord
        if (btnActionDiscord != null) {
            btnActionDiscord.setOnClickListener(v -> {
                String link = RemoteConfigManager.getString("discord");
                if (link == null || link.trim().isEmpty()) {
                    link = "https://discord.gg/nextcity";
                }
                openUrl(link);
            });
        }

        // 4. Facebook
        if (btnActionFacebook != null) {
            btnActionFacebook.setOnClickListener(v -> {
                String link = RemoteConfigManager.getString("website");
                if (link == null || link.trim().isEmpty()) {
                    link = "https://facebook.com";
                }
                openUrl(link);
            });
        }

        // 5. Dedicated Server Section (Locked to 168.222.20.88:7777, Live Query)
        if (btnRefreshServerStats != null) {
            btnRefreshServerStats.setOnClickListener(v -> {
                Toast.makeText(getContext(), "กำลังรีเฟรชข้อมูลเซิร์ฟเวอร์...", Toast.LENGTH_SHORT).show();
                fetchRealServerStats();
            });
        }

        View.OnClickListener showServerStatusListener = v -> showServerStatusDialog();
        if (btnActionServers != null) btnActionServers.setOnClickListener(showServerStatusListener);
        if (serverItemNextCity != null) {
            serverItemNextCity.setOnClickListener(showServerStatusListener);
        }

        fetchRealServerStats();

        // 6. Announcements Section
        View.OnClickListener showAnnouncementListener = v -> showAnnouncementDialog();
        if (btnActionAnnouncement != null) btnActionAnnouncement.setOnClickListener(showAnnouncementListener);
        if (btnViewAllNews != null) btnViewAllNews.setOnClickListener(showAnnouncementListener);
        if (cardTeamAnnouncement != null) cardTeamAnnouncement.setOnClickListener(showAnnouncementListener);

        // 7. Notification Button
        if (btnNotification != null) {
            btnNotification.setOnClickListener(v -> showNotificationDialog());
        }

        // 8. Bottom Shortcuts
        if (navReviews != null) {
            navReviews.setOnClickListener(v -> showModernDialog(
                    "⭐ รีวิวเซิร์ฟเวอร์",
                    R.drawable.ic_star_shortcut,
                    "คะแนนเฉลี่ยจากผู้เล่น: 4.9 / 5.0 (15,280 รีวิว)\n\n" +
                            "• 'เซิร์ฟเวอร์เล่นลื่น ระบบคอมมูนิตี้ดีมาก แอดมินดูแลทั่วถึง!' - PlayerTH\n" +
                            "• 'ภาพสวย บรรยากาศเมืองโมเดิร์น ไม่เหมือนใคร' - CityGamer\n\n" +
                            "ร่วมส่งคะแนนรีวิวของคุณเพื่อรับของรางวัลในเกมประจำสัปดาห์!",
                    "เขียนรีวิว",
                    () -> Toast.makeText(getContext(), "ขอบคุณสำหรับข้อเสนอแนะและรีวิวครับ!", Toast.LENGTH_SHORT).show()
            ));
        }

        if (navEvents != null) {
            navEvents.setOnClickListener(v -> showModernDialog(
                    "🎁 กิจกรรมพิเศษ",
                    R.drawable.ic_gift_shortcut,
                    "🎉 กิจกรรมเทศกาลต้อนรับผู้เล่นใหม่ & ฮาโลวีน ไนท์\n\n" +
                            "1. คูณ EXP และเงินในเกม x2 ตลอดสุดสัปดาห์นี้\n" +
                            "2. ล็อกอินสะสมครบ 7 วัน รับสกินรถลิมิเต็ดฟรี!\n" +
                            "3. เควสภารกิจล่ารางวัลในเมือง NextCity รับกล่องสุ่มแรร์\n\n" +
                            "ระยะเวลากิจกรรม: วันนี้ - 15 พ.ย. 2568",
                    "ดูรายละเอียด",
                    null
            ));
        }

        if (navRankings != null) {
            navRankings.setOnClickListener(v -> showModernDialog(
                    "🏆 จัดอันดับผู้เล่น (Leaderboard)",
                    R.drawable.ic_trophy_shortcut,
                    "อันดับผู้เล่นยอดเยี่ยมประจำซีซั่น:\n\n" +
                            "🥇 อันดับ 1: Alex_Rider (เลเวล 85 · ทรัพย์สิน 45.2M)\n" +
                            "🥈 อันดับ 2: Sakura_Miku (เลเวล 79 · ทรัพย์สิน 38.6M)\n" +
                            "🥉 อันดับ 3: Kenji_Sato (เลเวล 74 · ทรัพย์สิน 31.9M)\n" +
                            "4. Cross_Fire (เลเวล 70)\n" +
                            "5. Jimmy_V (เลเวล 68)\n\n" +
                            "รีเซ็ตอันดับและแจกรางวัลทุกสิ้นเดือน!",
                    "ปิด",
                    null
            ));
        }

        if (navSupport != null) {
            navSupport.setOnClickListener(v -> showModernDialog(
                    "🎧 ติดต่อทีมงาน",
                    R.drawable.ic_headset_shortcut,
                    "หากพบปัญหาในการเล่น การติดตั้ง หรือต้องการแจ้งบัค:\n\n" +
                            "• Discord Support: เปิด Ticket ได้ตลอด 24 ชม.\n" +
                            "• Fanpage: Facebook.com/NextCityMobile\n" +
                            "• เวลาทำการทีมงานแอดมิน: ทุกวัน 10:00 - 02:00 น.\n\n" +
                            "เราพร้อมดูแลและช่วยเหลือผู้เล่นทุกท่านเสมอครับ ❤️",
                    "เปิด Discord Ticket",
                    () -> {
                        String link = RemoteConfigManager.getString("discord");
                        if (link == null || link.trim().isEmpty()) {
                            link = "https://discord.gg/nextcity";
                        }
                        openUrl(link);
                    }
            ));
        }

        return view;
    }

    private void launchGame() {
        if (getActivity() == null) return;
        android.util.Log.d("HomeFragment", "Launching SAMP game activity");

        Intent intent = new Intent(requireActivity(), SAMP.class);
        intent.setFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP | Intent.FLAG_ACTIVITY_SINGLE_TOP);
        startActivity(intent);

        requireActivity().overridePendingTransition(android.R.anim.fade_in, android.R.anim.fade_out);
    }

    private void openUrl(String url) {
        try {
            Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            startActivity(intent);
        } catch (Exception e) {
            android.util.Log.e("HomeFragment", "Failed to open url: " + url, e);
            Toast.makeText(getContext(), "ไม่สามารถเปิดลิงก์ได้", Toast.LENGTH_SHORT).show();
        }
    }

    private void initAnimations() {
        try {
            scaleDown = AnimationUtils.loadAnimation(requireContext(), R.anim.btn_scale_modern);
            scaleUp = AnimationUtils.loadAnimation(requireContext(), R.anim.btn_release_modern);
        } catch (Exception ignored) {
        }
    }

    private void attachTouchAnimation(View view) {
        if (view == null) return;
        view.setOnTouchListener((v, event) -> {
            if (scaleDown != null && scaleUp != null) {
                switch (event.getAction()) {
                    case MotionEvent.ACTION_DOWN:
                        v.startAnimation(scaleDown);
                        break;
                    case MotionEvent.ACTION_UP:
                    case MotionEvent.ACTION_CANCEL:
                        v.startAnimation(scaleUp);
                        break;
                }
            }
            return false;
        });
    }

    private void fetchRealServerStats() {
        if (tvServerLiveStatus != null) {
            tvServerLiveStatus.setText("กำลังตรวจสอบ...");
            tvServerLiveStatus.setTextColor(Color.parseColor("#D97706"));
        }

        new Thread(() -> {
            SampQueryAPI.ServerInfo info = SampQueryAPI.queryServer(LOCKED_SERVER_HOST, LOCKED_SERVER_PORT, 3000);
            lastServerInfo = info;
            if (getActivity() != null) {
                getActivity().runOnUiThread(() -> {
                    if (!isAdded()) return;
                    if (info.isOnline) {
                        if (tvServerLiveStatus != null) {
                            tvServerLiveStatus.setText("🟢 ออนไลน์");
                            tvServerLiveStatus.setTextColor(Color.parseColor("#16A34A"));
                        }
                        if (tvServerHostname != null) {
                            String hn = (info.hostname != null) ? info.hostname.trim() : "";
                            tvServerHostname.setText(hn.isEmpty() ? "NextCity Roleplay" : hn);
                        }
                        if (tvServerRealPlayers != null) {
                            tvServerRealPlayers.setText(info.players + " / " + info.maxPlayers + " คน");
                        }
                        if (tvServerRealPing != null) {
                            tvServerRealPing.setText(info.ping + " ms");
                            if (info.ping < 60) {
                                tvServerRealPing.setTextColor(Color.parseColor("#059669"));
                            } else if (info.ping < 120) {
                                tvServerRealPing.setTextColor(Color.parseColor("#D97706"));
                            } else {
                                tvServerRealPing.setTextColor(Color.parseColor("#DC2626"));
                            }
                        }
                    } else {
                        if (tvServerLiveStatus != null) {
                            tvServerLiveStatus.setText("🔴 ออฟไลน์");
                            tvServerLiveStatus.setTextColor(Color.parseColor("#DC2626"));
                        }
                        if (tvServerRealPlayers != null) {
                            tvServerRealPlayers.setText("ไม่สามารถติดต่อเซิร์ฟเวอร์");
                        }
                        if (tvServerRealPing != null) {
                            tvServerRealPing.setText("-");
                            tvServerRealPing.setTextColor(Color.parseColor("#94A3B8"));
                        }
                    }
                });
            }
        }).start();
    }

    private void showServerStatusDialog() {
        String statusText = (lastServerInfo != null && lastServerInfo.isOnline) ? "🟢 ออนไลน์ (ปกติ)" : "🔴 ไม่สามารถติดต่อได้";
        String playersText = (lastServerInfo != null && lastServerInfo.isOnline) ? (lastServerInfo.players + " / " + lastServerInfo.maxPlayers + " คน") : "ไม่สามารถติดต่อได้";
        String pingText = (lastServerInfo != null && lastServerInfo.isOnline) ? (lastServerInfo.ping + " ms") : "-";
        String hostName = (lastServerInfo != null && !lastServerInfo.hostname.isEmpty()) ? lastServerInfo.hostname : "NextCity Roleplay";

        showModernDialog(
                "🏢 " + hostName,
                R.drawable.ic_server_database,
                "ข้อมูลเซิร์ฟเวอร์ทางการ NextCity:\n\n" +
                        "• สถานะ: " + statusText + "\n" +
                        "• ที่อยู่ IP: " + LOCKED_SERVER_HOST + ":" + LOCKED_SERVER_PORT + "\n" +
                        "• ผู้เล่นออนไลน์จริง: " + playersText + "\n" +
                        "• ค่าความหน่วง (Ping): " + pingText + "\n" +
                        "• เวอร์ชันระบบ: SAMP Mobile 2.11 (64-bit)\n\n" +
                        "กดปุ่มด้านล่างเพื่อเข้าเล่นเกม!",
                "กดเข้าเล่นเกม",
                this::launchGame
        );
    }

    private void showAnnouncementDialog() {
        showModernDialog(
                "📢 ประกาศจากทีมงาน NextCity",
                R.drawable.ic_megaphone_clean,
                "ยินดีต้อนรับสู่ NextCity Modern Mobile Launcher 🎉\n\n" +
                        "ขอบคุณผู้เล่นทุกท่านที่เข้ามาเป็นส่วนหนึ่งของคอมมูนิตี้ของเรา!\n\n" +
                        "📌 อัพเดทแพทช์ล่าสุด 2.11.312:\n" +
                        "1. ปรับปรุงหน้าตา UI ใหม่ทั้งหมด สไตล์ Modern Casual & Anime\n" +
                        "2. เพิ่มประสิทธิภาพการเชื่อมต่อเซิร์ฟเวอร์ให้รวดเร็วและลื่นไหลยิ่งขึ้น\n" +
                        "3. รองรับการแสดงผลทุกขนาดหน้าจอบน Android\n" +
                        "4. ปรับปรุงเสถียรภาพและระบบเสียงเกม\n\n" +
                        "🕒 วันที่ประกาศ: 31 ต.ค. 2568 เวลา 14:30 น.",
                "เข้าใจแล้ว",
                null
        );
    }

    private void showNotificationDialog() {
        showModernDialog(
                "🔔 ศูนย์การแจ้งเตือน",
                R.drawable.ic_bell_clean,
                "การแจ้งเตือนล่าสุดของคุณ:\n\n" +
                        "🟢 [ระบบ] เซิร์ฟเวอร์ NextCity RP ออนไลน์ปกติ พร้อมให้บริการ\n" +
                        "🎁 [รางวัล] คุณมีของขวัญประจำวันรอรับอยู่ในกระเป๋า!\n" +
                        "⚡ [กิจกรรม] กิจกรรม EXP x2 เริ่มต้นแล้ว อย่าลืมชวนเพื่อนมาร่วมสนุก\n" +
                        "🛡️ [ความปลอดภัย] แนะนำให้ตั้งรหัสผ่านที่ปลอดภัยในเมนูตั้งค่าเกม",
                "รับทราบ",
                null
        );
    }

    private void showModernDialog(String title, int iconRes, String message, String actionText, Runnable onAction) {
        if (getContext() == null) return;

        Dialog dialog = new Dialog(requireContext());
        dialog.requestWindowFeature(Window.FEATURE_NO_TITLE);
        dialog.setContentView(R.layout.dialog_launcher_modern);

        if (dialog.getWindow() != null) {
            dialog.getWindow().setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
            dialog.getWindow().setLayout(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        }

        ImageView imgIcon = dialog.findViewById(R.id.dialogIcon);
        TextView tvTitle = dialog.findViewById(R.id.dialogTitle);
        TextView tvMessage = dialog.findViewById(R.id.dialogMessage);
        TextView btnAction = dialog.findViewById(R.id.dialogActionBtn);
        View btnClose = dialog.findViewById(R.id.dialogCloseBtn);

        if (imgIcon != null && iconRes != 0) imgIcon.setImageResource(iconRes);
        if (tvTitle != null) tvTitle.setText(title);
        if (tvMessage != null) tvMessage.setText(message);
        if (btnAction != null) {
            btnAction.setText(actionText != null ? actionText : "ตกลง");
            btnAction.setOnClickListener(v -> {
                dialog.dismiss();
                if (onAction != null) onAction.run();
            });
        }
        if (btnClose != null) {
            btnClose.setOnClickListener(v -> dialog.dismiss());
        }

        dialog.show();
    }

    @Override
    public void onResume() {
        super.onResume();
        refreshPlayerNickname();
        fetchRealServerStats();
    }

    private void refreshPlayerNickname() {
        if (getContext() == null) return;
        String nick = ConfigValidator.getNickName(getContext());
        if (tvHeaderPlayerNick != null) {
            tvHeaderPlayerNick.setText(nick);
        }
        if (tvPlayerNickSubtitle != null) {
            tvPlayerNickSubtitle.setText("ชื่อตัวละคร: " + nick);
        }
    }

    private void showChangeNicknameDialog() {
        if (getContext() == null) return;

        Dialog dialog = new Dialog(requireContext());
        dialog.requestWindowFeature(Window.FEATURE_NO_TITLE);
        dialog.setContentView(R.layout.dialog_change_nickname);

        if (dialog.getWindow() != null) {
            dialog.getWindow().setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
            dialog.getWindow().setLayout(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        }

        EditText etNick = dialog.findViewById(R.id.etDialogNickname);
        View btnClose = dialog.findViewById(R.id.dialogNickCloseBtn);
        View btnCancel = dialog.findViewById(R.id.btnDialogNickCancel);
        View btnSave = dialog.findViewById(R.id.btnDialogNickSave);

        String currentNick = ConfigValidator.getNickName(getContext());
        if (etNick != null) {
            etNick.setText(currentNick);
            etNick.setSelection(currentNick.length());
        }

        if (btnClose != null) {
            btnClose.setOnClickListener(v -> dialog.dismiss());
        }
        if (btnCancel != null) {
            btnCancel.setOnClickListener(v -> dialog.dismiss());
        }
        if (btnSave != null) {
            btnSave.setOnClickListener(v -> {
                if (etNick != null) {
                    String newName = etNick.getText().toString().trim();
                    if (newName.isEmpty()) {
                        Toast.makeText(getContext(), "กรุณากรอกชื่อตัวละคร", Toast.LENGTH_SHORT).show();
                        return;
                    }
                    if (newName.length() < 3 || newName.length() > 24) {
                        Toast.makeText(getContext(), "ชื่อต้องมีความยาวระหว่าง 3 - 24 ตัวอักษร", Toast.LENGTH_SHORT).show();
                        return;
                    }
                    ConfigValidator.setNickName(requireContext(), newName);
                    refreshPlayerNickname();
                    Toast.makeText(getContext(), "เปลี่ยนชื่อตัวละครเป็น " + newName + " เรียบร้อยแล้ว", Toast.LENGTH_SHORT).show();
                    dialog.dismiss();
                }
            });
        }

        dialog.show();
    }
}
