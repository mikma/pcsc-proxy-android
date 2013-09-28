package se.m7n.android.pcsc_proxy;

import android.app.Service;
import android.content.ComponentName;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.Binder;
import android.os.IBinder;
import android.util.Log;

import java.util.concurrent.locks.Condition;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;

import java.io.*;
import java.security.*;
import java.util.*;

import se.m7n.android.env.Setenv;

public class PCSCProxyService extends Service {
    public static final String TAG = "pcsc-proxy";

    private final Lock pkcs11Lock = new ReentrantLock();
    private final Condition pkcs11Bound = pkcs11Lock.newCondition();

    @Override
    public void onCreate() {
        super.onCreate();

        Log.d(TAG, "onCreate");
        Setenv.setenv("test", "value", 1);
    }

    @Override
    public void onDestroy() {
        super.onDestroy();

        Log.d(TAG, "onDestroy");
    }

    @Override
    public IBinder onBind(Intent intent) {
        // Return the interface

        Log.d(TAG, "onBind");
        return mBinder;
    }

    private final IBinder mBinder = new PCSCProxyBinder();

    final class PCSCProxyBinder extends Binder {
        public PCSCProxyService getService() {
            return PCSCProxyService.this;
        }
    }
}
