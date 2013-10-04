package se.m7n.android.pcsc_proxy;

import java.util.Collection;
import java.util.Set;

import android.app.AlertDialog;
import android.app.Dialog;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothClass;
import android.bluetooth.BluetoothDevice;
import android.content.Context;
import android.content.SharedPreferences;
import android.preference.DialogPreference;
import android.util.AttributeSet;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.AdapterView;
import android.widget.ListView;
import android.widget.TextView;

/**
 * The PCSCAddressPreference will display a dialog, and will persist the
 * <code>true</code> when pressing the positive button and <code>false</code>
 * otherwise. It will persist to the android:key specified in xml-preference.
 */
public class PCSCAddressPreference extends DialogPreference {

    public static final String TAG = "PCSCPref";
    private ListView mList;
    private ArrayAdapter<BluetoothDevice> mAdapter;
    private BluetoothAdapter mBluetoothAdapter;
    private BluetoothDevice mSelected;

    public PCSCAddressPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        setDialogLayoutResource(R.layout.devicepicker_dialog); 
        mBluetoothAdapter = BluetoothAdapter.getDefaultAdapter();
    }

    @Override
    protected void onDialogClosed(boolean positiveResult) {
        super.onDialogClosed(positiveResult);
        Log.d(TAG, "onDialogClosed: " + positiveResult + "," + mSelected);
        if (mSelected != null) {
            persistString(mSelected.getName());

            SharedPreferences.Editor editor = getEditor();
            editor.putString(PCSCPreferenceActivity.BT_ADDR_PREF,
                             mSelected.getAddress());
            editor.commit();
        }
    }

    @Override
    protected void onPrepareDialogBuilder(AlertDialog.Builder builder) {
        // builder.setTitle(R.string.pin_changepin_title);
        builder.setPositiveButton(null, null);
        builder.setNegativeButton(null, null);
        mSelected = null;
        super.onPrepareDialogBuilder(builder);
    }

    @Override
    public void onBindDialogView(View view){
        mList = (ListView)view.findViewById(R.id.devicepicker_list);
        mList.setChoiceMode(ListView.CHOICE_MODE_SINGLE);

        mList.setOnItemClickListener(new AdapterView.OnItemClickListener() {
                @Override
                public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                    Log.d(TAG, "onItemClick: " + id + "," + position);
                    mSelected = mAdapter.getItem(position);
                    getDialog().dismiss();
                }
            });

        mAdapter = new DeviceAdapter(getContext(), R.layout.device_item);
        mList.setAdapter(mAdapter);

        Set<BluetoothDevice> bonded = mBluetoothAdapter.getBondedDevices();
        for (BluetoothDevice dev : bonded) {
            mAdapter.add(dev);
        }

        mAdapter.notifyDataSetChanged();

        super.onBindDialogView(view);
    }

    private static class DeviceAdapter extends ArrayAdapter<BluetoothDevice> {
        public DeviceAdapter(Context context, int id) {
            super(context, id);
        }
        public View getView(int position, View convertView,
                            ViewGroup parent) {
            TextView view = (TextView)super.getView(position, convertView, parent);
            BluetoothDevice dev = getItem(position);
            BluetoothClass cls = dev.getBluetoothClass();
            view.setText(dev.getName() + "\t" + dev.getAddress());
            return view;
        }
    }
}
