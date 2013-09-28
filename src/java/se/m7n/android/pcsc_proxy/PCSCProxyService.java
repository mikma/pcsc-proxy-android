package se.m7n.android.pcsc_proxy;

import android.app.Service;
import android.content.ComponentName;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.Binder;
import android.os.IBinder;
import android.util.Log;

import android.net.Credentials;
import android.net.LocalServerSocket;
import android.net.LocalSocket;

import java.util.concurrent.locks.Condition;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;

import java.io.*;
import java.security.*;
import java.util.*;

import se.m7n.android.env.Setenv;

public class PCSCProxyService extends Service {
    public static final String TAG = "PCSCProxyService";

    private final Lock pkcs11Lock = new ReentrantLock();
    private final Condition pkcs11Bound = pkcs11Lock.newCondition();

    private String mSocketName;
    private Thread mThread;
    private LocalServerSocket mServer;
    private boolean mStopping = false;

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

    private void start() {
        mStopping = false;
        mThread = new Thread(new Runnable() {
                public void run() {
                    try {
                        mServer = new LocalServerSocket(mSocketName);

                        Log.i(TAG, "Waiting for connection on: " + mServer);

                        LocalSocket socket;

                        while ((socket = mServer.accept()) != null) {
                            Credentials creds = socket.getPeerCredentials();

                            Log.i(TAG, "Accepted: " + socket);
                            Log.i(TAG, "Credentials: " + creds);
                            socket.close();

                            if (mStopping) {
                                Log.i(TAG, "Stopped");
                                mServer.close();
                                mServer = null;
                                break;
                            }
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
