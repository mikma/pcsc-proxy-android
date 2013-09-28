package se.m7n.android.pcsc_proxy;

import android.app.Service;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothSocket;
import android.content.ComponentName;
import android.content.Intent;
import android.content.ServiceConnection;
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

public class PCSCProxyService extends Service {
    public static final String TAG = "PCSCProxyService";
    public static final int TIMEOUT = 500;
    public static final UUID BCSC_UUID = UUID.fromString("1ad34c3e-7872-4142-981d-98b280416ce8");

    private final Lock pkcs11Lock = new ReentrantLock();
    private final Condition pkcs11Bound = pkcs11Lock.newCondition();

    private String mSocketName;
    private Thread mThread;
    private Thread mConnectionThread;
    private LocalServerSocket mServer;
    private boolean mStopping = false;
    private BluetoothAdapter mBluetoothAdapter;

    @Override
    public void onCreate() {
        super.onCreate();

        mSocketName = toString();

        Log.d(TAG, "onCreate: " + mSocketName);
        Setenv.setenv("test", "value", 1);
        start();
    }

    @Override
    public void onDestroy() {
        super.onDestroy();

        Log.d(TAG, "onDestroy");
        stop();
    }

    @Override
    public IBinder onBind(Intent intent) {
        // Return the interface

        Log.d(TAG, "onBind");
        return mBinder;
    }


    private class ChannelThread extends Thread {
        private Condition mCond;
        private InputStream mInput;
        private OutputStream mOutput;

        private ChannelThread(Condition cond,
                              InputStream input, OutputStream output) {
            mCond = cond;
            mInput = input;
            mOutput = output;
        }

        private void cancel() {
            try {
                mInput.close();
            } catch(IOException e) {
                Log.d(TAG, "Close input", e);
            }

            try {
                mOutput.close();
            } catch(IOException e) {
                Log.d(TAG, "Close output", e);
            }
        }

        public void run() {
            byte[] buf = new byte[1024];
            int len;

            try {
                while((len = mInput.read(buf, 0, buf.length)) >= 0) {
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
                Log.d(TAG, "Connection", e);
            }

            mCond.signalAll();
        }
    }

    private boolean startConnection(final LocalSocket socket) {
        if (mConnectionThread != null)
            return false;

        mConnectionThread = new Thread(new Runnable() {
                public void run() {
                    try {
                        Lock lock = new ReentrantLock();
                        Condition stop = lock.newCondition();
                        String address = TEST_ADDRESS;
                        BluetoothDevice device =
                            mBluetoothAdapter.getRemoteDevice(address);
                        BluetoothSocket btSock;

                        btSock =
                            device.createRfcommSocketToServiceRecord(BCSC_UUID);

                        ChannelThread thread1;
                        ChannelThread thread2;

                        thread1 =
                            new ChannelThread(stop,
                                              btSock.getInputStream(),
                                              socket.getOutputStream());
                        thread2 =
                            new ChannelThread(stop,
                                              socket.getInputStream(),
                                              btSock.getOutputStream());

                        thread1.start();
                        thread2.start();

                        Log.i(TAG, "ConnectionThread await");
                        try {
                            stop.await();
                        } catch (InterruptedException e) {
                            Log.d(TAG, "ConnectionThread await", e);
                        }
                        Log.i(TAG, "ConnectionThread signalled");
                        thread1.cancel();
                        thread2.cancel();
                        try {
                            thread1.join(TIMEOUT);
                        } catch (InterruptedException e) {
                            Log.d(TAG, "Thread 1 join");
                        }
                        try {
                            thread2.join(TIMEOUT);
                        } catch (InterruptedException e) {
                            Log.d(TAG, "Thread 2 join");
                        }
                    } catch(IOException e) {
                        throw new RuntimeException(e);
                    }
                    Log.i(TAG, "ConnectionThread end");
                    if (mConnectionThread == Thread.currentThread()) {
                        Log.i(TAG, "ConnectionThread cleared");
                        mConnectionThread = null;
                    }
                }
            });
        mConnectionThread.start();
        return true;
    }

    private void stop() {
        try {
            mStopping = true;
            LocalSocket mSock = new LocalSocket();
            mSock.connect(mServer.getLocalSocketAddress());
            mSock.close();
        } catch(IOException e) {
            Log.d(TAG, "Stop", e);
        }
    }

    private boolean isAllowed(Credentials creds) {
        // FIXME
        return true;
    }

    private void start() {
        mStopping = false;
        mThread = new Thread(new Runnable() {
                public void run() {
                    try {
                        mServer = new LocalServerSocket(mSocketName);

                        Log.i(TAG, "Waiting for connection on: " + mServer);

                        LocalSocket socket;

                        while (!mStopping
                               && ((socket = mServer.accept()) != null)) {
                            Log.i(TAG, "Accepted: " + socket);

                            if (mStopping) {
                                Log.i(TAG, "Stopped");
                                mServer.close();
                                mServer = null;
                                break;
                            }


                            Credentials creds = socket.getPeerCredentials();
                            Log.i(TAG, "Credentials: " + creds);

                            if (isAllowed(creds)) {
                                if (startConnection(socket))
                                    continue;
                            }

                            socket.close();
                        }
                    } catch(IOException e) {
                        throw new RuntimeException(e);
                    }
                    Log.i(TAG, "Thread end");
                    mThread = null;
                }
            });
        mThread.start();
    }

    private final IBinder mBinder = new PCSCProxyBinder();

    final class PCSCProxyBinder extends Binder {
        public PCSCProxyService getService() {
            return PCSCProxyService.this;
        }
    }
}
