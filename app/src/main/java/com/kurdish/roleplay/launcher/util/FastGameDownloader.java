package com.kurdish.roleplay.launcher.util;

import android.content.Context;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;

import com.google.firebase.remoteconfig.FirebaseRemoteConfig;
import com.kurdish.roleplay.launcher.FilesData;

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.BufferedReader;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicLong;

public final class FastGameDownloader {

    private static final String TAG = "FastGameDownloader";
    private static final int BUFFER_SIZE = 65536; // 64 KB buffer for maximum throughput
    private static final int CONNECT_TIMEOUT_MS = 15000;
    private static final int READ_TIMEOUT_MS = 30000;
    private static final int MAX_FILE_RETRIES = 3;

    // 8 parallel threads to maximize 4G/5G/WiFi network pipe saturation
    private static final int THREAD_POOL_SIZE = Math.max(4, Math.min(8, Runtime.getRuntime().availableProcessors() * 2));

    private static final Handler sMainHandler = new Handler(Looper.getMainLooper());

    private FastGameDownloader() {}

    public interface DownloadCallback {
        void onProgress(long currentBytes, long totalBytes, int currentFile, int totalFiles, String currentFileName, String speedText);
        void onFileCompleted(String fileName, int currentFile, int totalFiles);
        void onComplete();
        void onError(String errorReason);
    }

    /**
     * Reads manifest from remote URL or falls back to bundled assets/files.json.
     */
    public static List<FilesData> fetchManifest(Context context, String remoteUrl) {
        String jsonContent = null;

        if (remoteUrl != null && (remoteUrl.startsWith("http://") || remoteUrl.startsWith("https://"))) {
            try {
                jsonContent = downloadString(remoteUrl);
            } catch (Exception e) {
                Log.w(TAG, "Failed to download remote manifest from " + remoteUrl + ", using fallback", e);
            }
        }

        if (jsonContent == null || jsonContent.trim().isEmpty()) {
            jsonContent = loadAssetString(context, "files.json");
        }

        if (jsonContent == null || jsonContent.trim().isEmpty()) {
            Log.e(TAG, "No manifest available (remote failed and assets/files.json missing)");
            return Collections.emptyList();
        }

        return parseManifestJson(jsonContent);
    }

    public static List<FilesData> parseManifestJson(String jsonStr) {
        List<FilesData> list = new ArrayList<>();
        try {
            JSONObject obj = new JSONObject(jsonStr);
            JSONArray filesArray = obj.optJSONArray("files");
            if (filesArray == null) {
                return list;
            }

            for (int i = 0; i < filesArray.length(); i++) {
                JSONObject item = filesArray.getJSONObject(i);
                String name = item.getString("name");
                long size = item.getLong("size");
                String path = item.getString("path");
                String url = item.optString("url", path);

                // Ignore mutable or runtime-generated files
                if (isMutableFile(name)) {
                    continue;
                }

                list.add(new FilesData(name, size, path, url));
            }
        } catch (Exception e) {
            Log.e(TAG, "Failed to parse manifest JSON", e);
        }
        return list;
    }

    /**
     * Verifies game files against manifest:
     * - Returns list of files that do not exist or have mismatched size.
     */
    public static List<FilesData> verifyFiles(Context context, List<FilesData> manifest, int gpuType) {
        List<FilesData> missingOrInvalid = new ArrayList<>();
        String basePath = GameStorage.getGameBasePath(context);

        for (FilesData data : manifest) {
            String path = data.getPath();

            // GPU filter for texture files
            if (!path.contains("player") && !path.contains("menu") && !path.contains("samp")) {
                if (path.contains(".dxt.") && gpuType != 1) continue;
                if (path.contains(".etc.") && gpuType != 2) continue;
                if (path.contains(".pvr.") && gpuType != 3) continue;
            }

            File localFile = new File(basePath, path);
            if (!localFile.exists() || localFile.length() != data.getSize()) {
                missingOrInvalid.add(data);
            }
        }

        return missingOrInvalid;
    }

    /**
     * Quick check if all critical files exist on storage.
     */
    public static boolean areCriticalFilesPresent(Context context) {
        String basePath = GameStorage.getGameBasePath(context);
        String[] criticalFiles = {
                "SAMP/main.scm",
                "SAMP/script.img",
                "SAMP/peds.ide",
                "texdb/gta3.img",
                "anim/anim.img"
        };

        for (String rel : criticalFiles) {
            File f = new File(basePath, rel);
            if (!f.exists() || f.length() == 0) {
                return false;
            }
        }
        return true;
    }

    /**
     * Downloads files directly in parallel WITHOUT zips, at maximum possible speed.
     */
    public static void downloadFilesParallel(
            final Context context,
            final String baseUrl,
            final List<FilesData> filesToDownload,
            final DownloadCallback callback) {

        if (filesToDownload == null || filesToDownload.isEmpty()) {
            if (callback != null) {
                sMainHandler.post(callback::onComplete);
            }
            return;
        }

        final String basePath = GameStorage.getGameBasePath(context);
        final int totalFiles = filesToDownload.size();

        long totalSize = 0;
        for (FilesData f : filesToDownload) {
            totalSize += f.getSize();
        }
        final long totalBytesAll = totalSize;

        final AtomicLong downloadedBytes = new AtomicLong(0);
        final AtomicInteger completedFiles = new AtomicInteger(0);
        final AtomicInteger failedFiles = new AtomicInteger(0);
        final AtomicBoolean isCancelled = new AtomicBoolean(false);

        final ExecutorService executor = Executors.newFixedThreadPool(THREAD_POOL_SIZE);
        final CountDownLatch latch = new CountDownLatch(totalFiles);

        // Progress monitor thread (updates speed & UI every 100ms)
        final Thread monitorThread = new Thread(() -> {
            long lastTime = System.currentTimeMillis();
            long lastBytes = 0;
            String lastSpeedStr = "0 KB/s";

            while (latch.getCount() > 0 && !isCancelled.get()) {
                try {
                    Thread.sleep(100);
                } catch (InterruptedException ignored) {
                    break;
                }

                long now = System.currentTimeMillis();
                long currentBytes = downloadedBytes.get();
                long elapsed = now - lastTime;

                if (elapsed >= 500) {
                    long bytesDiff = currentBytes - lastBytes;
                    double bytesPerSec = (bytesDiff * 1000.0) / elapsed;
                    if (bytesPerSec >= 1048576.0) {
                        lastSpeedStr = String.format("%.2f MB/s", bytesPerSec / 1048576.0);
                    } else {
                        lastSpeedStr = String.format("%.0f KB/s", bytesPerSec / 1024.0);
                    }
                    lastBytes = currentBytes;
                    lastTime = now;
                }

                final String speedText = lastSpeedStr;
                final int doneCount = completedFiles.get();

                if (callback != null) {
                    sMainHandler.post(() -> callback.onProgress(
                            currentBytes,
                            totalBytesAll,
                            doneCount,
                            totalFiles,
                            "กำลังดาวน์โหลดไฟล์...",
                            speedText
                    ));
                }
            }
        });
        monitorThread.setDaemon(true);
        monitorThread.start();

        // Worker tasks
        for (final FilesData fileData : filesToDownload) {
            executor.submit(() -> {
                boolean success = false;
                for (int attempt = 1; attempt <= MAX_FILE_RETRIES && !isCancelled.get(); attempt++) {
                    try {
                        downloadSingleFile(baseUrl, fileData, basePath, downloadedBytes);
                        success = true;
                        break;
                    } catch (Exception e) {
                        Log.w(TAG, "Download attempt " + attempt + " failed for " + fileData.getPath() + ": " + e.getMessage());
                        try {
                            Thread.sleep(300L * attempt);
                        } catch (InterruptedException ignored) {}
                    }
                }

                if (success) {
                    int c = completedFiles.incrementAndGet();
                    if (callback != null) {
                        sMainHandler.post(() -> callback.onFileCompleted(fileData.getName(), c, totalFiles));
                    }
                } else {
                    failedFiles.incrementAndGet();
                    Log.e(TAG, "Failed permanently to download file: " + fileData.getPath());
                }

                latch.countDown();
            });
        }

        // Completion waiter thread
        new Thread(() -> {
            try {
                latch.await();
                executor.shutdown();
                executor.awaitTermination(10, TimeUnit.SECONDS);
            } catch (InterruptedException e) {
                Log.e(TAG, "Download latch interrupted", e);
            }

            if (callback != null) {
                sMainHandler.post(() -> {
                    if (failedFiles.get() == 0) {
                        callback.onComplete();
                    } else {
                        callback.onError("ดาวน์โหลดไม่สมบูรณ์ " + failedFiles.get() + " ไฟล์");
                    }
                });
            }
        }).start();
    }

    private static void downloadSingleFile(
            String baseUrl,
            FilesData fileData,
            String basePath,
            AtomicLong downloadedBytesCounter) throws IOException {

        String fullUrl;
        if (fileData.getUrl() != null && (fileData.getUrl().startsWith("http://") || fileData.getUrl().startsWith("https://"))) {
            fullUrl = fileData.getUrl();
        } else {
            String base = baseUrl.endsWith("/") ? baseUrl : baseUrl + "/";
            fullUrl = base + fileData.getPath();
        }

        File targetFile = new File(basePath, fileData.getPath());
        File parentDir = targetFile.getParentFile();
        if (parentDir != null && !parentDir.exists()) {
            parentDir.mkdirs();
        }

        File tempFile = new File(basePath, fileData.getPath() + ".tmp");
        if (tempFile.exists()) {
            tempFile.delete();
        }

        HttpURLConnection connection = null;
        InputStream in = null;
        BufferedOutputStream out = null;

        try {
            URL url = new URL(fullUrl);
            connection = (HttpURLConnection) url.openConnection();
            connection.setConnectTimeout(CONNECT_TIMEOUT_MS);
            connection.setReadTimeout(READ_TIMEOUT_MS);
            connection.setRequestProperty("Connection", "Keep-Alive");
            connection.setRequestProperty("User-Agent", "MAYES-FastDownloader/2.11");
            connection.connect();

            int code = connection.getResponseCode();
            if (code != HttpURLConnection.HTTP_OK && code != HttpURLConnection.HTTP_PARTIAL) {
                throw new IOException("HTTP code " + code + " for URL: " + fullUrl);
            }

            in = new BufferedInputStream(connection.getInputStream(), BUFFER_SIZE);
            out = new BufferedOutputStream(new FileOutputStream(tempFile), BUFFER_SIZE);

            byte[] buffer = new byte[BUFFER_SIZE];
            int read;
            long fileBytes = 0;

            while ((read = in.read(buffer)) != -1) {
                out.write(buffer, 0, read);
                fileBytes += read;
                downloadedBytesCounter.addAndGet(read);
            }

            out.flush();

            // Verify file size if not empty
            if (fileData.getSize() > 0 && tempFile.length() != fileData.getSize()) {
                throw new IOException("Downloaded file size mismatch: expected " + fileData.getSize() + " but got " + tempFile.length());
            }

            // Atomic rename
            if (targetFile.exists()) {
                targetFile.delete();
            }
            if (!tempFile.renameTo(targetFile)) {
                // If rename fails, try copy
                copyFileDirect(tempFile, targetFile);
                tempFile.delete();
            }

        } finally {
            if (out != null) {
                try { out.close(); } catch (Exception ignored) {}
            }
            if (in != null) {
                try { in.close(); } catch (Exception ignored) {}
            }
            if (connection != null) {
                connection.disconnect();
            }
        }
    }

    private static void copyFileDirect(File src, File dst) throws IOException {
        try (InputStream in = new BufferedInputStream(new java.io.FileInputStream(src));
             java.io.OutputStream out = new BufferedOutputStream(new FileOutputStream(dst))) {
            byte[] buf = new byte[BUFFER_SIZE];
            int len;
            while ((len = in.read(buf)) > 0) {
                out.write(buf, 0, len);
            }
            out.flush();
        }
    }

    public static String downloadString(String urlStr) throws IOException {
        URL url = new URL(urlStr);
        HttpURLConnection conn = (HttpURLConnection) url.openConnection();
        conn.setConnectTimeout(CONNECT_TIMEOUT_MS);
        conn.setReadTimeout(READ_TIMEOUT_MS);
        conn.connect();

        try (BufferedReader reader = new BufferedReader(new InputStreamReader(conn.getInputStream()))) {
            StringBuilder sb = new StringBuilder();
            String line;
            while ((line = reader.readLine()) != null) {
                sb.append(line).append('\n');
            }
            return sb.toString();
        } finally {
            conn.disconnect();
        }
    }

    private static String loadAssetString(Context context, String assetPath) {
        if (context == null) return null;
        try (InputStream is = context.getAssets().open(assetPath);
             BufferedReader reader = new BufferedReader(new InputStreamReader(is))) {
            StringBuilder sb = new StringBuilder();
            String line;
            while ((line = reader.readLine()) != null) {
                sb.append(line).append('\n');
            }
            return sb.toString();
        } catch (Exception e) {
            Log.w(TAG, "Asset not found: " + assetPath);
            return null;
        }
    }

    private static boolean isMutableFile(String name) {
        if (name == null) return false;
        String lower = name.toLowerCase();
        return lower.endsWith(".log")
                || "gtasatelem.set".equalsIgnoreCase(lower)
                || "gta_sa.set".equalsIgnoreCase(lower)
                || "settings.ini".equalsIgnoreCase(lower)
                || "GTASAMP10.b".equalsIgnoreCase(lower)
                || "GTASAsf10.b".equalsIgnoreCase(lower)
                || ".htaccess".equalsIgnoreCase(lower);
    }
}
