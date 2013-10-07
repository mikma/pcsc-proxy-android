package se.m7n.android.pcsc.btclient;

import java.io.IOException;

import android.app.Activity;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.os.Bundle;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.IBinder;
import android.os.Message;
import android.os.RemoteException;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.ServiceConnection;
import android.util.Log;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.*;

import java.security.*;
import java.security.cert.Certificate;
import java.security.cert.*;
import java.security.interfaces.*;
import javax.security.auth.callback.*;
import java.io.*;
import java.net.*;
import java.util.*;
import javax.net.*;
import javax.net.ssl.*;
import javax.net.ssl.SSLParameters;

import org.openintents.smartcard.PCSCDaemon;

public class PCSCProxyActivity extends Activity
{
    final static String TAG = "pcsc-proxy";

    private EditText mPassword;
    private Handler mHandler;
    private HandlerThread mThread;
    private PCSCDaemon mProxy;

    /** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle savedInstanceState)
    {
        mThread = new HandlerThread("network");
        mThread.start();
        mHandler = new Handler(mThread.getLooper(), new HandlerCallback());

        super.onCreate(savedInstanceState);
        setContentView(R.layout.main);

        // Intent intent = new Intent(this, PCSCProxyService.class);
        // bindService(intent, connection, BIND_AUTO_CREATE);

        // mPassword = (EditText) findViewById(R.id.password);
        // ((Button) findViewById(R.id.test)).setOnClickListener(mTest);

        // Register the BroadcastReceiver
        IntentFilter filter = new IntentFilter(BluetoothDevice.ACTION_FOUND);
        registerReceiver(mReceiver, filter);

        Intent startPref = new Intent(this, PCSCPreferenceActivity.class);
        startActivity(startPref);

        // BluetoothAdapter.getDefaultAdapter().startDiscovery();
    }

    @Override
    public void onDestroy()
    {
        try {
            if (mProxy != null)
                mProxy.stop();
        } catch(RemoteException e) {
            Log.w(TAG, "onDestroy", e);
        }
        unbindService(connection);
        stopHandler();

        unregisterReceiver(mReceiver);

        super.onDestroy();
    }

    private void stopHandler() {
        if (mHandler == null)
            return;
        mHandler.getLooper().quit();
        try {
            mThread.join(1000);
        } catch (InterruptedException e) {
        }
        mThread = null;
        mHandler = null;
    }

    private class HandlerCallback implements Handler.Callback {
        public boolean handleMessage(Message msg) {
            return false;
        }
    }

    OnClickListener mTest = new OnClickListener() {
        public void onClick(View v) {

            String pwd = mPassword.getText().toString();
            //Log.d(TAG, "foo: " + pwd);
            final char[] pass = pwd.toCharArray();

            // try {
                // mProxy.login(pass);
            // } catch (KeyStoreException e) {
            //     throw new RuntimeException(e);
            // }

            // mHandler.post(new Runnable() {
            //         public void run() {
            //             try {
            //                 // genericFind(pass);
            //             } catch (Exception e) {
            //                 throw new RuntimeException(e);
            //             }
            //         }
            //     });
        }
    };

    private final ServiceConnection connection = new ServiceConnection() {
            public void onServiceConnected(ComponentName className,
                                            IBinder service) {
                mProxy = PCSCDaemon.Stub.asInterface(service);
                try {
                    mProxy.start();
                } catch(RemoteException e) {
                    Log.e(TAG, "PCSC", e);
                }
            }
            public void onServiceDisconnected(ComponentName className) {
                mProxy = null;
            }
        };

    // Create a BroadcastReceiver for ACTION_FOUND
    private final BroadcastReceiver mReceiver = new BroadcastReceiver() {
            public void onReceive(Context context, Intent intent) {
                String action = intent.getAction();
                // When discovery finds a device
                if (BluetoothDevice.ACTION_FOUND.equals(action)) {
                    // Get the BluetoothDevice object from the Intent
                    BluetoothDevice device = intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
                    // Add the name and address to an array adapter to show in a ListView
                    //mArrayAdapter.add(device.getName() + "\n" + device.getAddress());
                    Log.d(TAG, "Found BT: " + device);
                }
            }
        };
}
