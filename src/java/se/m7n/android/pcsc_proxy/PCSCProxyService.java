package se.m7n.android.pcsc_proxy;

import android.app.Service;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothSocket;
import android.content.ComponentName;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.pm.PackageManager;
import android.os.Binder;
import android.os.IBinder;
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
    public static final int TIMEOUT = 500;
    public static final UUID BCSC_UUID = UUID.fromString("1ad34c3e-7872-4142-981d-98b280416ce8");

    private final Lock pkcs11Lock = new ReentrantLock();
    private final Condition pkcs11Bound = pkcs11Lock.newCondition();

    private String mSocketName;
    private Thread mThread;
    private ConnectionThread mConnectionThread;
    private LocalServerSocket mServer;
    private boolean mStopping = false;
    private BluetoothAdapter mBluetoothAdapter;
    private boolean mIsBound = false;

    @Override
    public void onCreate() {
        super.onCreate();

        mSocketName = toString();

        Log.d(TAG, "onCreate: " + mSocketName);
        Setenv.setenv("test", "value", 1);
        mBluetoothAdapter = BluetoothAdapter.getDefaultAdapter();
    }

    @Override
    public void onDestroy() {
        super.onDestroy();

        Log.d(TAG, "onDestroy");
        if (isStarted())
            stop();
    }

    @Override
    public IBinder onBind(Intent intent) {
        // Return the interface
        Log.d(TAG, "onBind");

        if (mIsBound) {
            Log.w(TAG, "onBind: Already bound");
            return null;
        }

        mIsBound = true;
        return mBinder;
    }


    @Override
    public boolean onUnbind(Intent intent) {
        Log.d(TAG, "onUnbind");
        mIsBound = false;
        return true;
    }

    private static class ChannelThread extends Thread {
        private Lock mLock;
        private Condition mCond;
        private InputStream mInput;
        private OutputStream mOutput;
        private volatile boolean mShouldExit = false;

        private ChannelThread(String name, Lock lock, Condition cond,
                              InputStream input, OutputStream output) {
            super(name);
            mLock = lock;
            mCond = cond;
            mInput = input;
            mOutput = output;
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
                // FIXME use timeout
                join();
                //join(TIMEOUT);
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

                    Log.d(TAG, "Read: " + len);
                    if (len > 0) {
                        Log.d(TAG, "Write: " + len);
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

            try {
                mLock.lock();
                mCond.signalAll();
            } finally {
                mLock.unlock();
            }

            onExit();
        }
    }

    private boolean startConnection(final LocalSocket socket) {
        if (mConnectionThread != null)
            return false;

        String address = TEST_ADDRESS;
        BluetoothDevice device =
            mBluetoothAdapter.getRemoteDevice(address);
        BluetoothSocket btSock;

        try {
            btSock =
                device.createRfcommSocketToServiceRecord(BCSC_UUID);

            // Device discovery is a heavyweight procedure
            // on the Bluetooth adapter and will
            // significantly slow a device connection.
            mBluetoothAdapter.cancelDiscovery();

            Log.i(TAG, "BT socket try connect");
            btSock.connect();
            Log.i(TAG, "BT socket connected");
            mConnectionThread = new ConnectionThread(socket, btSock);
            mConnectionThread.start();
            return true;
        } catch(IOException e) {
            Log.e(TAG, "BT connect failed", e);
            return false;
        }
    }

    private static class ConnectionThread extends Thread {
        private Lock lock = new ReentrantLock();
        private Condition stop = lock.newCondition();
        private LocalSocket socket;
        private BluetoothSocket btSock;

        private ConnectionThread(LocalSocket socket_, BluetoothSocket btSock_) {
            socket = socket_;
            btSock = btSock_;
        }

        private void cancel() {
            Log.d(TAG, "cancel connection");
            try {
                lock.lock();
                stop.signalAll();
            } finally {
                lock.unlock();
            }
        }

                public void run() {
                    try {
                        ChannelThread thread1;
                        ChannelThread thread2;

                        thread1 =
                            new ChannelThread("BTtoUN", lock, stop,
                                              btSock.getInputStream(),
                                              socket.getOutputStream());
                        thread2 =
                            new ChannelThread("UNtoBT", lock, stop,
                                              socket.getInputStream(),
                                              btSock.getOutputStream());

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
                }
    }

    private void stop() {
        try {
            Log.d(TAG, "Stopping thread");
            mStopping = true;
            LocalSocket mSock = new LocalSocket();
            mSock.connect(mServer.getLocalSocketAddress());
            mSock.close();
            // mThread.interrupt();
            // Log.i(TAG, "Interrupt thread");
            try {
                mThread.join(TIMEOUT);
            } catch(InterruptedException e) {
                Log.w(TAG, "Thread join failed");
            } finally {
                mThread = null;
            }
            if (mConnectionThread != null) {
                mConnectionThread.cancel();
                try {
                    mConnectionThread.join(TIMEOUT);
                } catch(InterruptedException e) {
                    Log.w(TAG, "ConnectionThread join failed");
                } finally {
                    mConnectionThread = null;
                }
            }
            Log.d(TAG, "Thread stopped");
        } catch(IOException e) {
            Log.d(TAG, "Stop failes", e);
        }
    }

    private boolean isAllowed(Credentials creds) {
        return checkPermission(ACCESS_PCSC, creds.getPid(), creds.getUid())
            == PackageManager.PERMISSION_GRANTED;
    }

    private boolean isStarted() {
        return mThread != null;
    }

    private boolean start() {
        if (isStarted()) {
            Log.w(TAG, "Already started");
            return false;
        }

        mStopping = false;
        mThread = new Thread(new Runnable() {
                public void run() {
                    try {
                        mServer = new LocalServerSocket(mSocketName);

                        Log.i(TAG, "Waiting for connection on: " + mSocketName);

                        LocalSocket socket;

                        while (!mStopping
                               && ((socket = mServer.accept()) != null)) {
                            Log.i(TAG, "Accepted: " + socket);

                            if (mStopping) {
                                Log.i(TAG, "Stopped");
                                if (socket != null)
                                    socket.close();
                                mServer.close();
                                mServer = null;
                                break;
                            }


                            Credentials creds = socket.getPeerCredentials();

                            if (isAllowed(creds)) {
                                Log.i(TAG, "Permission granted");
                                if (startConnection(socket))
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
                }
            });
        mThread.start();
        return true;
    }

    private final PCSCDaemon.Stub mBinder = new PCSCProxyBinder();

    final class PCSCProxyBinder extends PCSCDaemon.Stub {
        public boolean start() {
            Log.d(TAG, "start " + String.format("pid:%d uid:%d", getCallingPid(), getCallingUid()));
            if (PCSCProxyService.this.isStarted()) {
                Log.e(TAG, "Already started");
                return false;
            }
            PCSCProxyService.this.start();
            return true;
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
