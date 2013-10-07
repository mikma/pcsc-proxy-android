package se.m7n.android.pcsc_proxy;

import android.app.Service;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothSocket;
import android.content.ComponentName;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.ServiceConnection;
import android.content.pm.PackageManager;
import android.os.Binder;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.IBinder;
import android.os.Message;
import android.preference.PreferenceManager;
import android.util.Log;

import android.net.Credentials;
import android.net.LocalServerSocket;
import android.net.LocalSocket;

import java.util.UUID;
import java.util.concurrent.locks.Condition;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;

import java.io.*;
import java.security.*;
import java.util.*;

import se.m7n.android.env.Setenv;
import org.openintents.smartcard.PCSCDaemon;

public class PCSCProxyService extends Service {
    public static final String TAG = "PCSCProxyService";
    public static final String ACCESS_PCSC = "org.openintents.smartcard.permission.ACCESS_PCSC";
    public static final int TIMEOUT = 2000;
    public static final UUID BCSC_UUID = UUID.fromString("1ad34c3e-7872-4142-981d-98b280416ce8");

    private ServerThread mThread;
    private BluetoothAdapter mBluetoothAdapter;
    private String mAddress;
    private String mSocketName;
    private Handler mHandler;
    private HandlerThread mHandlerThread;

    @Override
    public void onCreate() {
        super.onCreate();

        Log.d(TAG, "onCreate");
        mHandlerThread = new HandlerThread("PCSCProxyService handler");
        mHandlerThread.start();
        mHandler = new Handler(mHandlerThread.getLooper(),
                               new HandlerCallback());

        Setenv.setenv("test", "value", 1);
        mBluetoothAdapter = BluetoothAdapter.getDefaultAdapter();

        SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(getApplicationContext());
        mAddress = prefs.getString(PCSCPreferenceActivity.BT_ADDR_PREF, null);
    }

    @Override
    public void onDestroy() {
        Log.d(TAG, "onDestroy");
        if (isStarted())
            stop();
        stopHandler();

        super.onDestroy();
    }

    @Override
    public IBinder onBind(Intent intent) {
        // Return the interface
        Log.d(TAG, "onBind");
        return mBinder;
    }


    @Override
    public boolean onUnbind(Intent intent) {
        Log.d(TAG, "onUnbind");
        return true;
    }

    private void stopHandler() {
        if (mHandler == null)
            return;
        mHandler.getLooper().quit();
        try {
            mHandlerThread.join(TIMEOUT);
        } catch (InterruptedException e) {
        }
        mHandlerThread = null;
        mHandler = null;
    }

    private class HandlerCallback implements Handler.Callback {
        public boolean handleMessage(Message msg) {
            return false;
        }
    }

    private interface OnTerminatedListener {
        void onTerminated();
    }

    private interface CheckPermission {
        boolean isAllowed(Credentials creds);
    }

    private static class ReadWriteThread extends Thread {
        private OnTerminatedListener listener;
        private InputStream mInput;
        private OutputStream mOutput;
        private volatile boolean mShouldExit = false;

        private ReadWriteThread(String name,
                              InputStream input, OutputStream output) {
            super(name);
            mInput = input;
            mOutput = output;
        }
        private void setTerminatedListener(OnTerminatedListener listener) {
            this.listener = listener;
        }
        private void setShouldExit() {
            mShouldExit = true;
        }

        private void cancelAndJoin(boolean closeInput) {
            setShouldExit();

            if (closeInput) {
                try {
                    Log.d(TAG, "Pre close input: " + toString());
                    mInput.close();
                    Log.d(TAG, "Post close input: " + toString());
                } catch(IOException e) {
                    Log.d(TAG, "Close input", e);
                }
            }

            try {
                Log.d(TAG, "Pre join: " + toString());
                join(TIMEOUT);
                Log.d(TAG, "Post join: " + toString());
            } catch (InterruptedException e) {
                Log.d(TAG, "Channel join: " + toString());
            }
        }

        private void onExit() {
            Log.d(TAG, "Channel exiting: " + this);
            try {
                mInput.close();
            } catch(IOException e) {
                Log.d(TAG, "Close input", e);
            }
            mInput = null;

            try {
                mOutput.close();
            } catch(IOException e) {
                Log.d(TAG, "Close output", e);
            }
            mOutput = null;
        }

        public void run() {
            byte[] buf = new byte[1024];
            int len;

            try {
                while((len = mInput.read(buf, 0, buf.length)) >= 0) {
                    if (mShouldExit) {
                        onExit();
                        return;
                    }
                    if (interrupted()) {
                        Log.d(TAG, "Interrupted: " + this);
                        break;
                    }

                    if (len > 0) {
                        mOutput.write(buf, 0, len);
                    }
                }
            } catch(IOException e) {
                if (mShouldExit) {
                    onExit();
                    return;
                }
                Log.d(TAG, "Connection", e);
            }

            Log.d(TAG, "Channel finished: " + toString());

            onExit();
            if (listener != null)
                listener.onTerminated();
        }
    }

    private static class ConnectionThread extends Thread {
        private Lock lock = new ReentrantLock();
        private Condition stop = lock.newCondition();
        private LocalSocket socket;
        private BluetoothSocket btSock;
        private OnTerminatedListener listener;

        private ConnectionThread(LocalSocket socket_, BluetoothSocket btSock_) {
            super("Connection");
            socket = socket_;
            btSock = btSock_;
        }
        private void setTerminatedListener(OnTerminatedListener listener) {
            this.listener = listener;
        }
        private void cancel() {
            Log.d(TAG, "ConnectionThread cancel");
            try {
                lock.lock();
                stop.signalAll();
            } finally {
                lock.unlock();
            }
        }
        private void cancelAndJoin() {
            cancel();
            try {
                Log.d(TAG, "ConnectionThread Pre join: " + toString());
                join(TIMEOUT);
                Log.d(TAG, "ConnectionThread Post join: " + toString());
            } catch (InterruptedException e) {
                Log.d(TAG, "ConnectionThread join: " + toString(), e);
            }
        }

        OnTerminatedListener rwListener = new OnTerminatedListener() {
                public void onTerminated() {
                    if (listener != null)
                        listener.onTerminated();
                    else
                        cancelAndJoin();
                }
            };

                public void run() {
                    try {
                        ReadWriteThread thread1;
                        ReadWriteThread thread2;

                        thread1 =
                            new ReadWriteThread("BTtoUN",
                                              btSock.getInputStream(),
                                              socket.getOutputStream());
                        thread2 =
                            new ReadWriteThread("UNtoBT",
                                              socket.getInputStream(),
                                              btSock.getOutputStream());
                        thread1.setTerminatedListener(rwListener);
                        thread2.setTerminatedListener(rwListener);
                        thread1.start();
                        thread2.start();

                        Log.i(TAG, "ConnectionThread await");
                        try {
                            lock.lock();
                            stop.await();
                        } catch (InterruptedException e) {
                            Log.d(TAG, "ConnectionThread await", e);
                        } finally {
                            lock.unlock();
                        }
                        Log.i(TAG, "ConnectionThread signalled");

                        thread1.setShouldExit();
                        thread2.setShouldExit();
                        try {
                            socket.shutdownInput();
                        } catch(IOException e) {
                            Log.d(TAG, "socket close failed", e);
                        }
                        try {
                            btSock.close();
                        } catch(IOException e) {
                            Log.d(TAG, "BT socket close failed", e);
                        }
                        Log.i(TAG, "LocalSocket input shutdown");

                        thread1.cancelAndJoin(false);
                        thread2.cancelAndJoin(false);
                    } catch(IOException e) {
                        throw new RuntimeException(e);
                    }
                    Log.i(TAG, "ConnectionThread end");
                    if (listener != null)
                        listener.onTerminated();
                }
    }

    private void stop() {
        if (!isStarted())
            return;

        Log.d(TAG, "Stopping thread");
        mSocketName = null;
        mThread.cancelAndJoin();
        mThread = null;
        Log.d(TAG, "Thread stopped");
    }

    private boolean isStarted() {
        return mThread != null && mThread.isRunning();
    }

    private boolean start() {
        if (isStarted()) {
            Log.w(TAG, "Already started");
            return false;
        }
        if (mAddress == null) {
            Log.w(TAG, "Address not configured");
            return false;
        }
        CheckPermission check = new CheckPermission() {
                public boolean isAllowed(Credentials creds) {
                    return checkPermission(ACCESS_PCSC,
                                           creds.getPid(), creds.getUid())
                        == PackageManager.PERMISSION_GRANTED;
                }
            };
        mSocketName = null;
        mThread = new ServerThread(mBluetoothAdapter, mAddress, check);
        mThread.setTerminatedListener(new OnTerminatedListener(){
                public void onTerminated() {
                    mHandler.post(new Runnable() {
                            public void run() {
                                Log.d(TAG, "onTerminated trying to cancel");
                                mThread.cancelAndJoin();
                            }
                        });
                }
            });
        try {
            mSocketName = mThread.startServer();
            return true;
        } catch(IOException e) {
            Log.e(TAG, "Failed to start", e);
            mThread = null;
            return false;
        }
    }

    private static class ServerThread extends Thread {
        private Lock lock = new ReentrantLock();
        private Condition stop = lock.newCondition();

        private String mSocketName;
        private LocalServerSocket mServer;
        private BluetoothAdapter mBluetoothAdapter;
        private String mBtAddress;
        private BluetoothSocket mBtSock;
        private boolean mRunning;
        private boolean mStopping;
        private ConnectionThread mConnectionThread;
        private CheckPermission mCheckPermission;
        private OnTerminatedListener listener;

        public ServerThread(BluetoothAdapter adapter, String btAddress,
                            CheckPermission checkPermission) {
            super("ServerThread");
            mBluetoothAdapter = adapter;
            mSocketName = toString()+"@"+hashCode();
            mRunning = false;
            mStopping = false;
            mBtAddress = btAddress;
            mCheckPermission = checkPermission;
        }
        private void setTerminatedListener(OnTerminatedListener listener) {
            this.listener = listener;
        }
        public boolean isRunning() {
            return mRunning && !mStopping;
        }
        private void onExit() {
            Log.d(TAG, "ServerThread onExit");
            if (mServer != null) {
                try {
                    mServer.close();
                } catch (IOException e) {
                    // Ignore
                    Log.d(TAG, "ServerThread LocalServerSocket close failed", e);
                }
                mServer = null;
            }
            if (mBtSock != null) {
                try {
                    mBtSock.close();
                } catch (IOException e) {
                    // Ignore
                    Log.d(TAG, "ServerThread BluetoothSocket close failed", e);
                }
                mBtSock = null;
            }
        }
        public String startServer() throws IOException {
            mServer = new LocalServerSocket(mSocketName);

            BluetoothDevice device =
                mBluetoothAdapter.getRemoteDevice(mBtAddress);
            BluetoothSocket btSock = null;

            try {
                btSock =
                    device.createRfcommSocketToServiceRecord(BCSC_UUID);

                // Device discovery is a heavyweight procedure
                // on the Bluetooth adapter and will
                // significantly slow a device connection.
                mBluetoothAdapter.cancelDiscovery();

                Log.i(TAG, "BT socket try connect");
                btSock.connect();
                mBtSock = btSock;
                Log.i(TAG, "BT socket connected");
                start();
                return mSocketName;
            } catch(IOException e) {
                if (btSock != null)
                    btSock.close();
                throw e;
            }
        }
        private void cancel() {
            Log.d(TAG, "ServerThread cancel");
            try {
                lock.lock();
                stop.signalAll();
            } finally {
                lock.unlock();
            }
        }
        public void cancelAndJoin() {
            if (!isRunning()) {
                Log.w(TAG, "ServerThread not running");
                return;
            }
            try {
                Log.d(TAG, "ServerThread.cancelAndJoin");
                mStopping = true;
                //cancelConnection();
                cancel();
                LocalSocket mSock = new LocalSocket();
                mSock.connect(mServer.getLocalSocketAddress());
                mSock.close();
                // mThread.interrupt();
                // Log.i(TAG, "Interrupt thread");
                Log.d(TAG, "ServerThread.join");
                join(TIMEOUT);
                Log.d(TAG, "ServerThread joined");
            } catch(IOException e) {
                Log.w(TAG, "Thread join failed", e);
            } catch(InterruptedException e) {
                Log.w(TAG, "Thread join failed", e);
            } finally {
                mRunning = false;
            }
        }
            public void run() {
                    try {
                        mRunning = true;
                        Log.i(TAG, "Waiting for connection on: " + mSocketName);
                        LocalSocket socket;

                        while (!mStopping
                               && ((socket = mServer.accept()) != null)) {

                            if (mStopping) {
                                Log.i(TAG, "Stopped");
                                if (socket != null)
                                    socket.close();
                                break;
                            }

                            Log.i(TAG, "Accepted: " + socket);

                            Credentials creds = socket.getPeerCredentials();

                            if (mCheckPermission.isAllowed(creds)) {
                                Log.i(TAG, "Permission granted");
                                if (handleConnection(socket))
                                    continue;
                            } else {
                                Log.i(TAG, "Permission not granted");
                            }

                            socket.close();
                        }
                    } catch(IOException e) {
                        throw new RuntimeException(e);
                    }
                    Log.i(TAG, "Thread end");
                    onExit();
                }
        private boolean handleConnection(final LocalSocket socket) {
            if (!startConnection(socket))
                return false;

            Log.i(TAG, "ServerThread await");
            try {
                lock.lock();
                stop.await();
            } catch (InterruptedException e) {
                Log.d(TAG, "ServerThread await", e);
            } finally {
                lock.unlock();
            }
            Log.i(TAG, "ServerThread signalled");

            cancelAndJoinConnection();
            return true;
        }
        private OnTerminatedListener mListener = new OnTerminatedListener() {
                public void onTerminated() {
                    if (listener != null)
                        listener.onTerminated();
                    else
                        cancelAndJoin();
                }
            };
        private boolean startConnection(final LocalSocket socket) {
            if (mConnectionThread != null)
                return false;

            mConnectionThread = new ConnectionThread(socket, mBtSock);
            mConnectionThread.setTerminatedListener(mListener);
            mConnectionThread.start();
            return true;
        }
        private void cancelAndJoinConnection() {
            if (mConnectionThread != null) {
                mConnectionThread.cancelAndJoin();
                mConnectionThread = null;
            }
        }
        private void cancelConnection() {
            if (mConnectionThread != null) {
                Log.d(TAG, "ServerThread.cancelConnection enter");
                mConnectionThread.cancel();
                Log.d(TAG, "ServerThread.cancelConnection exit");
            }
        }
    }

    private final PCSCDaemon.Stub mBinder = new PCSCProxyBinder();

    final class PCSCProxyBinder extends PCSCDaemon.Stub {
        public boolean start() {
            Log.d(TAG, "start " + String.format("pid:%d uid:%d", getCallingPid(), getCallingUid()));
            if (PCSCProxyService.this.isStarted()) {
                Log.e(TAG, "Already started");
                return false;
            }
            return PCSCProxyService.this.start();
        }
        public void stop() {
            if (PCSCProxyService.this.isStarted()) {
                PCSCProxyService.this.stop();
            } else {
                Log.e(TAG, "Not started");
            }
        }
        public String getLocalSocketAddress() {
            return mSocketName;
        }
    }
}
