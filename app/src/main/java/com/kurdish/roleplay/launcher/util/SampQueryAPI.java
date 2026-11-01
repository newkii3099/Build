package com.kurdish.roleplay.launcher.util;

import android.util.Log;

import java.io.UnsupportedEncodingException;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.StringTokenizer;

public final class SampQueryAPI {

    private static final String TAG = "SampQueryAPI";

    public static class ServerInfo {
        public boolean isOnline = false;
        public int players = 0;
        public int maxPlayers = 0;
        public String hostname = "";
        public String gamemode = "";
        public String language = "";
        public int ping = -1;
    }

    public static ServerInfo queryServer(String host, int port, int timeoutMs) {
        ServerInfo info = new ServerInfo();
        DatagramSocket socket = null;
        try {
            InetAddress address = InetAddress.getByName(host);
            socket = new DatagramSocket();
            socket.setSoTimeout(timeoutMs > 0 ? timeoutMs : 2500);

            // Construct 11-byte SAMP query packet: 'S','A','M','P' + 4 bytes IP + 2 bytes port + opcode
            byte[] sendBuffer = new byte[11];
            sendBuffer[0] = 'S';
            sendBuffer[1] = 'A';
            sendBuffer[2] = 'M';
            sendBuffer[3] = 'P';

            String ipStr = address.getHostAddress();
            StringTokenizer tokenizer = new StringTokenizer(ipStr, ".");
            sendBuffer[4] = (byte) Integer.parseInt(tokenizer.nextToken());
            sendBuffer[5] = (byte) Integer.parseInt(tokenizer.nextToken());
            sendBuffer[6] = (byte) Integer.parseInt(tokenizer.nextToken());
            sendBuffer[7] = (byte) Integer.parseInt(tokenizer.nextToken());
            sendBuffer[8] = (byte) (port & 0xFF);
            sendBuffer[9] = (byte) ((port >> 8) & 0xFF);
            sendBuffer[10] = 'i'; // Info query

            DatagramPacket sendPacket = new DatagramPacket(sendBuffer, sendBuffer.length, address, port);

            long startTime = System.currentTimeMillis();
            socket.send(sendPacket);

            byte[] recvBuffer = new byte[2048];
            DatagramPacket recvPacket = new DatagramPacket(recvBuffer, recvBuffer.length);
            socket.receive(recvPacket);
            long endTime = System.currentTimeMillis();

            info.ping = (int) (endTime - startTime);

            int len = recvPacket.getLength();
            if (len >= 11) {
                ByteBuffer buffer = ByteBuffer.wrap(recvBuffer, 0, len);
                buffer.order(ByteOrder.LITTLE_ENDIAN);
                buffer.position(11); // Skip 11 header bytes

                if (buffer.remaining() >= 1) {
                    boolean password = buffer.get() != 0;
                }
                if (buffer.remaining() >= 4) {
                    info.players = buffer.getShort() & 0xFFFF;
                    info.maxPlayers = buffer.getShort() & 0xFFFF;
                }

                if (buffer.remaining() >= 4) {
                    int hostLen = buffer.getInt();
                    if (hostLen > 0 && buffer.remaining() >= hostLen) {
                        byte[] strBytes = new byte[hostLen];
                        buffer.get(strBytes);
                        info.hostname = decodeString(strBytes);
                    }
                }

                if (buffer.remaining() >= 4) {
                    int modeLen = buffer.getInt();
                    if (modeLen > 0 && buffer.remaining() >= modeLen) {
                        byte[] strBytes = new byte[modeLen];
                        buffer.get(strBytes);
                        info.gamemode = decodeString(strBytes);
                    }
                }

                if (buffer.remaining() >= 4) {
                    int langLen = buffer.getInt();
                    if (langLen > 0 && buffer.remaining() >= langLen) {
                        byte[] strBytes = new byte[langLen];
                        buffer.get(strBytes);
                        info.language = decodeString(strBytes);
                    }
                }

                info.isOnline = true;
            }
        } catch (Exception e) {
            Log.w(TAG, "Query server " + host + ":" + port + " failed: " + e.getMessage());
            info.isOnline = false;
        } finally {
            if (socket != null && !socket.isClosed()) {
                socket.close();
            }
        }
        return info;
    }

    private static String decodeString(byte[] bytes) {
        if (bytes == null || bytes.length == 0) return "";
        try {
            String utf8 = new String(bytes, "UTF-8");
            if (!utf8.contains("\uFFFD")) {
                return utf8;
            }
        } catch (UnsupportedEncodingException ignored) { }

        try {
            return new String(bytes, "TIS-620"); // Thai
        } catch (UnsupportedEncodingException ignored) { }

        try {
            return new String(bytes, "windows-1251");
        } catch (UnsupportedEncodingException ignored) { }

        return new String(bytes);
    }
}
