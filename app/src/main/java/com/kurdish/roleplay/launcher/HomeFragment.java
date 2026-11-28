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
import com.kurdish.roleplay.launcher.util.Config;
import com.kurdish.roleplay.launcher.util.ConfigValidator;
import com.kurdish.roleplay.launcher.util.FastGameDownloader;
import com.kurdish.roleplay.launcher.util.RemoteConfigManager;
import com.kurdish.roleplay.launcher.util.SampQueryAPI;

import java.util.List;

public class HomeFragment extends Fragment {

    public static final String LOCKED_SERVER_HOST = "157.254.192.108";
    public static final int LOCKED_SERVER_PORT = 7777;

    private Animation scaleDown;
    private Animation scaleUp;
    private TextView tvHeaderPlayerNick;
    private TextView tvPlayerNickSubtitle;

    private TextView tvServerLiveStatus;
    private TextView tvBottomServerStatus;
    private TextView tvServerHostname;
    private TextView tvServerIp;
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
        View btnSettingsTop = view.findViewById(R.id.btnSettingsTop);
        View btnQuickNickname = view.findViewById(R.id.btnQuickNickname);
        tvHeaderPlayerNick = view.findViewById(R.id.tvHeaderPlayerNick);

        // ◀️ Server Showcase & Live Stats
        tvServerLiveStatus = view.findViewById(R.id.tvServerLiveStatus);
        tvBottomServerStatus = view.findViewById(R.id.tvBottomServerStatus);
        tvServerHostname = view.findViewById(R.id.tvServerHostname);
        tvServerIp = view.findViewById(R.id.tvServerIp);
        tvServerRealPlayers = view.findViewById(R.id.tvServerRealPlayers);
        tvServerRealPing = view.findViewById(R.id.tvServerRealPing);
        View btnRefreshServerStats = view.findViewById(R.id.btnRefreshServerStats);
        View serverItemCryzen = view.findViewById(R.id.serverItemCryzen);

        if (tvServerIp != null) {
            tvServerIp.setText("IP: " + LOCKED_SERVER_HOST + ":" + LOCKED_SERVER_PORT);
        }

        // ▶️ Right Column Actions & Play CTA
        View btnProfileNick = view.findViewById(R.id.btnProfileNick);
        tvPlayerNickSubtitle = view.findViewById(R.id.tvPlayerNickSubtitle);
        View btnPlay = view.findViewById(R.id.btnPlay);
        View btnActionServers = view.findViewById(R.id.btnActionServers);
        View btnActionSettings = view.findViewById(R.id.btnActionSettings);
        View btnActionDiscord = view.findViewById(R.id.btnActionDiscord);
        View btnActionFacebook = view.findViewById(R.id.btnActionFacebook);

        // ⚡ Bottom Bar Support Action
        View navSupport = view.findViewById(R.id.navSupport);

        // Attach touch feedback animations
        attachTouchAnimation(btnPlay);
        attachTouchAnimation(btnProfileNick);
        attachTouchAnimation(btnActionServers);
        attachTouchAnimation(btnActionSettings);
        attachTouchAnimation(btnActionDiscord);
        attachTouchAnimation(btnActionFacebook);
        attachTouchAnimation(btnSettingsTop);
        attachTouchAnimation(btnQuickNickname);

        // 0. Quick Nickname Edit
        View.OnClickListener nickClickListener = v -> showChangeNicknameDialog();
        if (btnQuickNickname != null) btnQuickNickname.setOnClickListener(nickClickListener);
        if (btnProfileNick != null) btnProfileNick.setOnClickListener(nickClickListener);
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
            btnActionDiscord.setOnClickListener(v -> openDiscordLink());
        }

        // 4. Facebook / Website
        if (btnActionFacebook != null) {
            btnActionFacebook.setOnClickListener(v -> {
                String link = RemoteConfigManager.getString("website");
                if (link == null || link.trim().isEmpty()) {
                    link = "https://facebook.com";
                }
                openUrl(link);
            });
        }

        // 5. Live Server Stats Query
        if (btnRefreshServerStats != null) {
            btnRefreshServerStats.setOnClickListener(v -> {
                Toast.makeText(getContext(), "กำลังรีเฟรชข้อมูลเซิร์ฟเวอร์...", Toast.LENGTH_SHORT).show();
                fetchRealServerStats();
            });
        }

        View.OnClickListener showServerStatusListener = v -> showServerStatusDialog();
        if (btnActionServers != null) btnActionServers.setOnClickListener(showServerStatusListener);
        if (serverItemCryzen != null) serverItemCryzen.setOnClickListener(showServerStatusListener);

        fetchRealServerStats();

        // 6. Support Contact
        if (navSupport != null) {
            navSupport.setOnClickListener(v -> showModernDialog(
                    "ติดต่อทีมงาน CRYZEN CITY 2.0",
                    R.drawable.ic_headset_shortcut,
                    "หากพบปัญหาในการเข้าเล่น การติดตั้ง หรือต้องการแจ้งบั๊ก:\n\n" +
                            "• สามารถเปิด Ticket สอบถามทีมงานได้ทาง Discord ทางการ\n" +
                            "• เวอร์ชันตัวเกม: 2.0.1-cryzen (64-bit)\n" +
                            "• แพ็กเกจระบบ: com.newlab.cryzen",
                    "เปิด Discord",
                    this::openDiscordLink
            ));
        }

        return view;
    }

    private void openDiscordLink() {
        String link = RemoteConfigManager.getString("discord");
        if (link == null || link.trim().isEmpty()) {
            Toast.makeText(getContext(), "ยังไม่ได้ตั้งค่าลิงก์ Discord ของเซิร์ฟเวอร์", Toast.LENGTH_SHORT).show();
            return;
        }
        openUrl(link);
    }

    private void launchGame() {
        if (getActivity() == null) return;
        android.util.Log.d("HomeFragment", "Checking game files before launch...");

        try {
            ConfigValidator.setServerConfig(requireContext(), LOCKED_SERVER_HOST, LOCKED_SERVER_PORT);
        } catch (Exception e) {
            android.util.Log.e("HomeFragment", "Failed to update server config", e);
        }

        // Fast-path: if critical game files are already present, launch SAMP directly!
        if (FastGameDownloader.areCriticalFilesPresent(requireContext())) {
            android.util.Log.i("HomeFragment", "Critical game files present, launching SAMP immediately");
            Intent intent = new Intent(requireActivity(), SAMP.class);
            intent.setFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP | Intent.FLAG_ACTIVITY_SINGLE_TOP);
            startActivity(intent);
            requireActivity().overridePendingTransition(android.R.anim.fade_in, android.R.anim.fade_out);
            return;
        }

        // Run fast file verification on background thread
        new Thread(() -> {
            try {
                if (getContext() == null || getActivity() == null) return;
                int gpuType = Config.getGpuType(requireContext());
                List<FilesData> manifest = FastGameDownloader.fetchManifest(requireContext(), Config.getManifestUrl(requireContext()));
                List<FilesData> missingOrInvalid = FastGameDownloader.verifyFiles(requireContext(), manifest, gpuType);

                if (getActivity() == null) return;

                getActivity().runOnUiThread(() -> {
                    if (missingOrInvalid != null && !missingOrInvalid.isEmpty()) {
                        android.util.Log.w("HomeFragment", "Missing or invalid files found: " + missingOrInvalid.size() + ", launching UpdateActivity");
                        Toast.makeText(getContext(), "ตรวจพบไฟล์เกมไม่ครบ (" + missingOrInvalid.size() + " ไฟล์) กำลังดาวน์โหลดอัตโนมัติ...", Toast.LENGTH_SHORT).show();

                        Intent intent = new Intent(requireActivity(), UpdateActivity.class);
                        intent.putExtra("mode", UpdateActivity.UpdateMode.GameDataUpdate.name());
                        intent.putExtra("autoLaunchAfter", true);
                        startActivity(intent);
                        requireActivity().overridePendingTransition(android.R.anim.fade_in, android.R.anim.fade_out);
                    } else {
                        android.util.Log.i("HomeFragment", "All game files verified, launching SAMP");
                        Intent intent = new Intent(requireActivity(), SAMP.class);
                        intent.setFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP | Intent.FLAG_ACTIVITY_SINGLE_TOP);
                        startActivity(intent);
                        requireActivity().overridePendingTransition(android.R.anim.fade_in, android.R.anim.fade_out);
                    }
                });
            } catch (Exception e) {
                android.util.Log.e("HomeFragment", "Error checking files, fallback to launch", e);
                if (getActivity() != null) {
                    getActivity().runOnUiThread(() -> {
                        Intent intent = new Intent(requireActivity(), SAMP.class);
                        intent.setFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP | Intent.FLAG_ACTIVITY_SINGLE_TOP);
                        startActivity(intent);
                        requireActivity().overridePendingTransition(android.R.anim.fade_in, android.R.anim.fade_out);
                    });
                }
            }
        }).start();
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
            tvServerLiveStatus.setTextColor(Color.parseColor("#FBBF24"));
        }
        if (tvBottomServerStatus != null) {
            tvBottomServerStatus.setText("เซิร์ฟเวอร์ : กำลังตรวจสอบ...");
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
                            tvServerLiveStatus.setTextColor(Color.parseColor("#4ADE80"));
                        }
                        if (tvBottomServerStatus != null) {
                            tvBottomServerStatus.setText("เซิร์ฟเวอร์ : ออนไลน์ (" + info.players + "/" + info.maxPlayers + ")");
                        }
                        if (tvServerHostname != null) {
                            String hn = (info.hostname != null) ? info.hostname.trim() : "";
                            tvServerHostname.setText(hn.isEmpty() ? "CRYZEN CITY 2.0" : hn);
                        }
                        if (tvServerRealPlayers != null) {
                            tvServerRealPlayers.setText(info.players + " / " + info.maxPlayers + " คน");
                        }
                        if (tvServerRealPing != null) {
                            tvServerRealPing.setText(info.ping + " ms");
                            if (info.ping < 60) {
                                tvServerRealPing.setTextColor(Color.parseColor("#4ADE80"));
                            } else if (info.ping < 120) {
                                tvServerRealPing.setTextColor(Color.parseColor("#FBBF24"));
                            } else {
                                tvServerRealPing.setTextColor(Color.parseColor("#FB7185"));
                            }
                        }
                    } else {
                        if (tvServerLiveStatus != null) {
                            tvServerLiveStatus.setText("🔴 ออฟไลน์");
                            tvServerLiveStatus.setTextColor(Color.parseColor("#FB7185"));
                        }
                        if (tvBottomServerStatus != null) {
                            tvBottomServerStatus.setText("เซิร์ฟเวอร์ : ไม่สามารถติดต่อได้");
                        }
                        if (tvServerRealPlayers != null) {
                            tvServerRealPlayers.setText("-- / --");
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
        String hostName = (lastServerInfo != null && !lastServerInfo.hostname.isEmpty()) ? lastServerInfo.hostname : "CRYZEN CITY 2.0";

        showModernDialog(
                hostName,
                R.drawable.ic_server_database,
                "ข้อมูลเซิร์ฟเวอร์ CRYZEN CITY 2.0:\n\n" +
                        "• สถานะ: " + statusText + "\n" +
                        "• ที่อยู่ IP: " + LOCKED_SERVER_HOST + ":" + LOCKED_SERVER_PORT + "\n" +
                        "• ผู้เล่นออนไลน์จริง: " + playersText + "\n" +
                        "• ค่าความหน่วง (Ping): " + pingText + "\n" +
                        "• เวอร์ชัน Launcher: 2.0.1-cryzen (UI 2.0)\n\n" +
                        "กดปุ่มด้านล่างเพื่อเข้าเล่นเกม!",
                "กดเข้าเล่นเกม",
                this::launchGame
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
