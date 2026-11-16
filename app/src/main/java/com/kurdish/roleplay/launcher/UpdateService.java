package com.kurdish.roleplay.launcher;

import android.app.Service;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.IBinder;
import android.os.Looper;
import android.os.Message;
import android.os.Messenger;
import android.os.RemoteException;
import android.text.TextUtils;
import android.util.Log;
import android.net.Uri;

import com.android.volley.Response;
import com.android.volley.VolleyError;
import com.android.volley.toolbox.StringRequest;
import com.android.volley.toolbox.Volley;
import com.downloader.Error;
import com.downloader.OnDownloadListener;
import com.downloader.OnProgressListener;
import com.downloader.PRDownloader;
import com.downloader.PRDownloaderConfig;
import com.downloader.Progress;
import com.joom.paranoid.Obfuscate;
import com.google.firebase.remoteconfig.FirebaseRemoteConfig;
import com.kurdish.roleplay.launcher.util.Config;
import com.kurdish.roleplay.launcher.util.FastGameDownloader;
import com.kurdish.roleplay.launcher.util.GameStorage;
import com.kurdish.roleplay.launcher.util.Util;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.BufferedReader;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.URL;
import java.util.ArrayList;
import java.util.List;
@Obfuscate
public class UpdateService extends Service {

    Messenger mMessenger;
    Messenger mActivityMessenger;
    IncomingHandler mInHandler;

    public UpdateActivity.GameStatus mGameStatus = UpdateActivity.GameStatus.Unknown;
    public UpdateActivity.UpdateStatus mUpdateStatus = UpdateActivity.UpdateStatus.Undefined;

    public boolean mDownloadingStatus = false;

    public long mUpdateGameDataSize = 0;
    public long mUpdateGameDataSizeUpdated = 0;
    public String mUpdateGameURL = "";
    public int mUpdateVersion;

    public ArrayList<String> mUpdateFiles;
    public ArrayList<String> mUpdateFilesName;
    public ArrayList<Long> mUpdateFilesSize;
    public ArrayList<FilesData> mFilesDataList = new ArrayList<>();

    public int mGpuType = 0;

    @Override
    public void onCreate() {
        super.onCreate();

        FirebaseRemoteConfig.getInstance().fetchAndActivate();

        mUpdateFiles = new ArrayList<>();
        mUpdateFilesName = new ArrayList<>();
        mUpdateFilesSize = new ArrayList<>();

        HandlerThread handlerThread = new HandlerThread("ServiceStartArguments", 10);
        handlerThread.start();
        PRDownloader.initialize(getApplicationContext(), PRDownloaderConfig.newBuilder().setDatabaseEnabled(true).setReadTimeout(30000).setConnectTimeout(30000).build());
        mInHandler = new IncomingHandler(handlerThread.getLooper());
        mMessenger = new Messenger(mInHandler);
    }

    @Override
    public IBinder onBind(Intent intent) {
        return mMessenger.getBinder();
    }

    class IncomingHandler extends Handler {
        public IncomingHandler(Looper looper) {
            super(looper);
        }

        @Override
        public void handleMessage(Message msg) {
            mActivityMessenger = msg.replyTo;
            Message obtain = null;
            Messenger messenger = null;
            if (msg.what == 0) {
                mGpuType = msg.getData().getInt("gputype");
                if(mGpuType == 0)
                {
                    Log.e("Dev Hama", "GPU not found");
                    return;
                }
                startUpdating();
            } else if (msg.what == 1) {
                startGameUpdateChecking();
            } else if(msg.what == 2) {
                updateGame();
            } else if (msg.what == 4) {
                obtain = Message.obtain(mInHandler, 4);
                obtain.getData().putString("status", mUpdateStatus.name());
                UpdateService updateService = UpdateService.this;
                obtain.replyTo = updateService.mMessenger;
                messenger = updateService.mActivityMessenger;
                try {
                    messenger.send(obtain);
                } catch (RemoteException e5) {
                    e5.printStackTrace();
                }
            } else if (msg.what == 5) {
                obtain = Message.obtain(mInHandler, 5);
                obtain.getData().putString("status", mGameStatus.name());
                obtain.replyTo = mMessenger;
                try {
                    mActivityMessenger.send(obtain);
                } catch (RemoteException e5) {
                    e5.printStackTrace();
                }

            } else if (msg.what == 7) {
                mGpuType = msg.getData().getInt("gputype");
                if(mGpuType == 0)
                {
                    Log.e("Dev Hama", "GPU not found");
                    return;
                }
                startUpdating();
            }
            else if (msg.what == 8) { // 🔥 REINSTALL CACHE
                mGpuType = msg.getData().getInt("gputype");
                if (mGpuType == 0) {
                    Log.e("Dev Hama", "GPU not found");
                    return;
                }

                startUpdating();
            }

        }
    }

    void startUpdating() {
        setUpdateStatus(UpdateActivity.UpdateStatus.CheckUpdate);

        new Thread(new Runnable() {
            @Override
            public void run() {
                try {
                    String urlClient = FirebaseRemoteConfig.getInstance().getString("launcher_client");
                    String manifestUrl = null;

                    if (isHttpUrl(urlClient)) {
                        try {
                            String response = FastGameDownloader.downloadString(urlClient);
                            if (response != null && response.contains("client_config")) {
                                JSONObject jSONObject = new JSONObject(response).getJSONObject("client_config");
                                mUpdateVersion = jSONObject.optInt("version_code", 0);
                                mUpdateGameURL = jSONObject.optString("url_launcher", "");
                                manifestUrl = jSONObject.optString("url_cache_files", null);
                            }
                        } catch (Exception e) {
                            Log.w("UpdateService", "Error reading launcher_client config: " + e.getMessage());
                        }
                    }

                    if (manifestUrl == null || !isHttpUrl(manifestUrl)) {
                        manifestUrl = Config.getManifestUrl(UpdateService.this);
                    }

                    if (mGpuType == 0) {
                        mGpuType = Config.getGpuType(UpdateService.this);
                    }

                    List<FilesData> manifest = FastGameDownloader.fetchManifest(UpdateService.this, manifestUrl);
                    if (manifest == null || manifest.isEmpty()) {
                        manifest = FastGameDownloader.fetchManifest(UpdateService.this, null);
                    }

                    List<FilesData> invalidFiles = FastGameDownloader.verifyFiles(UpdateService.this, manifest, mGpuType);

                    mFilesDataList = new ArrayList<>(invalidFiles);
                    mUpdateFiles = new ArrayList<>();
                    mUpdateFilesName = new ArrayList<>();
                    mUpdateFilesSize = new ArrayList<>();
                    mUpdateGameDataSize = 0;

                    for (FilesData fd : invalidFiles) {
                        mUpdateFiles.add(fd.getPath());
                        mUpdateFilesName.add(fd.getName());
                        mUpdateFilesSize.add(fd.getSize());
                        mUpdateGameDataSize += fd.getSize();
                    }

                    Log.d("UpdateService", "Manifest total: " + (manifest != null ? manifest.size() : 0) + ", invalid: " + invalidFiles.size() + " (" + mUpdateGameDataSize + " bytes)");

                    if (!isGameUpdateExists()) {
                        if (mUpdateFiles.isEmpty()) {
                            mGameStatus = UpdateActivity.GameStatus.Updated;
                        } else {
                            mGameStatus = UpdateActivity.GameStatus.UpdateRequired;
                        }
                    } else {
                        mGameStatus = UpdateActivity.GameStatus.GameUpdateRequired;
                    }

                    setUpdateStatus(UpdateActivity.UpdateStatus.Undefined);

                } catch (Exception e) {
                    Log.e("UpdateService", "startUpdating failed", e);
                    mGameStatus = UpdateActivity.GameStatus.Updated;
                    setUpdateStatus(UpdateActivity.UpdateStatus.Undefined);
                }
            }
        }).start();
    }

    public void updateGame() {
        if (isGameUpdateExists()) {
            Log.d("UpdateService", "updateGame exists");
            //setUpdateStatus(UpdateActivity.UpdateStatus.DownloadGame);
            downloadGame();
            return;
        }
        Log.d("UpdateService", "updateGame done");
        //setUpdateStatus(UpdateActivity.UpdateStatus.Undefined);
        File file = new File(GameStorage.getDownloadDirectory(this), "update.apk");
        Message obtain = Message.obtain(mInHandler, 2);
        obtain.getData().putBoolean("status", true);
        obtain.getData().putString("apkPath", file.getAbsolutePath());
        obtain.replyTo = mMessenger;
        if (mActivityMessenger != null) {
            try {
                mActivityMessenger.send(obtain);
            } catch (RemoteException e5) {
                e5.printStackTrace();
            }
        }
        //setUpdateStatus(UpdateActivity.UpdateStatus.Undefined);
    }


    public void startGameUpdateChecking()
    {
        if (!mUpdateFiles.isEmpty()) {
            setUpdateStatus(UpdateActivity.UpdateStatus.DownloadGameData);
            startDataUpdating();
            return;
        }

        Log.d("UpdateService", "updateGameData()");
        Message obtain = Message.obtain(this.mInHandler, 1);
        obtain.getData().putBoolean("status", true);
        obtain.replyTo = mMessenger;
        if (mActivityMessenger != null) {
            try {
                mActivityMessenger.send(obtain);
            } catch (RemoteException e5) {
                e5.printStackTrace();
            }
        }

    }

    public void setUpdateStatus(UpdateActivity.UpdateStatus updateStatus) {
        if (updateStatus.name().length() != 0 && mUpdateStatus != updateStatus) {
            mUpdateStatus = updateStatus;
            Message obtain = Message.obtain(mInHandler, 4);
            obtain.getData().putString("status", mUpdateStatus.name());
            obtain.replyTo = mMessenger;
            Messenger messenger = mActivityMessenger;
            if (messenger != null) {
                try {
                    messenger.send(obtain);
                } catch (RemoteException e5) {
                    e5.printStackTrace();
                }
            }
        }
    }


    public void startDataUpdating() {
        String dataBase = FirebaseRemoteConfig.getInstance().getString("launcher_data");
        if (!isHttpUrl(dataBase)) {
            dataBase = Config.getDataUrl(this);
        }

        final List<FilesData> toDownload;
        if (mFilesDataList != null && !mFilesDataList.isEmpty()) {
            toDownload = new ArrayList<>(mFilesDataList);
        } else {
            toDownload = new ArrayList<>();
            for (int i = 0; i < mUpdateFiles.size(); i++) {
                String path = mUpdateFiles.get(i);
                String name = (i < mUpdateFilesName.size()) ? mUpdateFilesName.get(i) : new File(path).getName();
                long size = (i < mUpdateFilesSize.size()) ? mUpdateFilesSize.get(i) : 0;
                toDownload.add(new FilesData(name, size, path, path));
            }
        }

        if (toDownload.isEmpty()) {
            Log.d("UpdateService", "No files to download, update finished");
            notifyDataUpdateCompleted();
            return;
        }

        mDownloadingStatus = true;
        final String finalDataBase = dataBase;

        FastGameDownloader.downloadFilesParallel(
                UpdateService.this,
                finalDataBase,
                toDownload,
                new FastGameDownloader.DownloadCallback() {
                    @Override
                    public void onProgress(long currentBytes, long totalBytes, int currentFile, int totalFiles, String currentFileName, String speedText) {
                        Message obtain = Message.obtain(mInHandler, 4);
                        obtain.getData().putString("status", UpdateActivity.UpdateStatus.DownloadGameData.name());
                        obtain.getData().putBoolean("withProgress", true);
                        obtain.getData().putLong("current", currentBytes);
                        obtain.getData().putLong("total", totalBytes);
                        obtain.getData().putString("filename", currentFileName);
                        obtain.getData().putString("speed", speedText);
                        obtain.getData().putLong("totalfiles", totalFiles);
                        obtain.getData().putLong("currentfile", currentFile);
                        obtain.replyTo = mMessenger;
                        if (mActivityMessenger != null) {
                            try {
                                mActivityMessenger.send(obtain);
                            } catch (RemoteException e) {
                                e.printStackTrace();
                            }
                        }
                    }

                    @Override
                    public void onFileCompleted(String fileName, int currentFile, int totalFiles) {
                        Log.d("UpdateService", "File completed (" + currentFile + "/" + totalFiles + "): " + fileName);
                    }

                    @Override
                    public void onComplete() {
                        mDownloadingStatus = false;
                        mUpdateFiles.clear();
                        mUpdateFilesName.clear();
                        mUpdateFilesSize.clear();
                        mFilesDataList.clear();
                        mUpdateGameDataSize = 0;
                        Log.d("UpdateService", "All files downloaded successfully");
                        notifyDataUpdateCompleted();
                    }

                    @Override
                    public void onError(String errorReason) {
                        mDownloadingStatus = false;
                        Log.e("UpdateService", "Download error: " + errorReason);
                        notifyDataUpdateCompleted();
                    }
                }
        );
    }

    private void notifyDataUpdateCompleted() {
        setUpdateStatus(UpdateActivity.UpdateStatus.Undefined);
        Message obtain = Message.obtain(this.mInHandler, 1);
        obtain.getData().putBoolean("status", true);
        obtain.replyTo = mMessenger;
        if (mActivityMessenger != null) {
            try {
                mActivityMessenger.send(obtain);
            } catch (RemoteException e5) {
                e5.printStackTrace();
            }
        }
    }

    public void downloadGame()
    {
        Log.d("UpdateService", "downloadGame");
        if (!isHttpUrl(mUpdateGameURL)) {
            handleUnavailableUpdateConfig("url_launcher");
            return;
        }

        mDownloadingStatus = true;

        File file = new File(GameStorage.getDownloadDirectory(this), "update.apk");
        if (file.exists()) {
            file.delete();
        }

        Ref.LongRef longRef = new Ref.LongRef();
        longRef.element = System.currentTimeMillis();

        mDownloadingStatus = true;
        PRDownloader.download(mUpdateGameURL, GameStorage.getDownloadDirectory(UpdateService.this).getAbsolutePath(), "update.apk").build().setOnStartOrResumeListener(null).setOnPauseListener(null).setOnCancelListener(null).setOnProgressListener(new OnProgressListener() {
            @Override
            public void onProgress(Progress progress) {
                mDownloadingStatus = true;
                if(System.currentTimeMillis() - longRef.element > 100) {
                    longRef.element = System.currentTimeMillis();
                    Message obtain = Message.obtain(mInHandler, 4);
                    obtain.getData().putString("status", UpdateActivity.UpdateStatus.DownloadGame.name());
                    obtain.getData().putBoolean("withProgress", true);
                    obtain.getData().putLong("current", progress.currentBytes);
                    obtain.getData().putLong("total", progress.totalBytes);
                    obtain.getData().putString("filename", "update.apk");
                    obtain.getData().putLong("totalfiles", 1);
                    obtain.getData().putLong("currentfile", 1);
                    if (mActivityMessenger != null) {
                        try {
                            mActivityMessenger.send(obtain);
                        } catch (RemoteException e) {
                            e.printStackTrace();
                        }
                    }
                }

            }
        }).start(new OnDownloadListener() {
            @Override
            public void onDownloadComplete() {
                Message obtain = Message.obtain(UpdateService.this.mInHandler, 2);
                obtain.getData().putBoolean("status", true);
                obtain.getData().putString("apkPath", file.getAbsolutePath());
                obtain.replyTo = mMessenger;
                if (mActivityMessenger != null) {
                    try {
                        mActivityMessenger.send(obtain);
                    } catch (RemoteException e5) {
                        e5.printStackTrace();
                    }
                }
                setUpdateStatus(UpdateActivity.UpdateStatus.Undefined);
                mDownloadingStatus = false;
                Log.d("Dev Hama", "completed");
            }

            @Override
            public void onError(Error error) {
                mDownloadingStatus = false;
                Log.d("Dev Hama", "error downloadgame");
                handleUnavailableUpdateConfig("url_launcher download failed");
            }
        });

        do {
            try {
                Thread.sleep(30);
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
        } while (mDownloadingStatus);

        mDownloadingStatus = false;
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

    private void handleUnavailableUpdateConfig(String key) {
        Log.w("UpdateService", "Remote Config URL is unavailable: " + key);
        mDownloadingStatus = false;
        mGameStatus = UpdateActivity.GameStatus.Unknown;
        setUpdateStatus(UpdateActivity.UpdateStatus.Undefined);

        Message message = Message.obtain(mInHandler, 5);
        message.getData().putString("status", mGameStatus.name());
        message.replyTo = mMessenger;
        if (mActivityMessenger != null) {
            try {
                mActivityMessenger.send(message);
            } catch (RemoteException exception) {
                Log.w("UpdateService", "Unable to report unavailable update config", exception);
            }
        }
    }

    public boolean isGameUpdateExists() {
        if (mUpdateVersion <= 0) {
            return false;
        }
        try {
            PackageInfo packageInfo = getPackageManager().getPackageInfo(getPackageName(), 0);
            if (packageInfo != null) {
                Log.d("UpdateService", "isGameUpdateExists -> currentVersion " + packageInfo.versionCode + " | mUpdateVersion " + this.mUpdateVersion);
                return this.mUpdateVersion > packageInfo.versionCode;
            }
        } catch (PackageManager.NameNotFoundException e) {
            e.printStackTrace();
        }
        return false;
    }

    private void sendLoadingScreen(boolean unpacking, String filename, long current, long total) {
        new Thread(new Runnable() {
            public void run() {
                Message obtain = Message.obtain(UpdateService.this.mInHandler, 4);
                obtain.getData().putString("status", UpdateActivity.UpdateStatus.CheckUpdate.name());
                obtain.getData().putBoolean("withProgress", true);
                obtain.getData().putString("filename", filename);
                obtain.getData().putBoolean("unpacking", unpacking);
                obtain.getData().putLong("current", current);
                obtain.getData().putLong("total", total);
                obtain.replyTo = mMessenger;
                if (mActivityMessenger != null) {
                    try {
                        mActivityMessenger.send(obtain);
                    } catch (RemoteException e5) {
                        e5.printStackTrace();
                    }
                }
            }
        }).start();
    }


}
