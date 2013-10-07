package se.m7n.android.pcsc.btclient;

import java.util.Iterator;
import java.util.Map;

import android.os.Bundle;
import android.content.SharedPreferences;
import android.preference.CheckBoxPreference;
import android.preference.EditTextPreference;
import android.preference.ListPreference;
import android.preference.Preference;
import android.preference.PreferenceActivity;
import android.preference.PreferenceScreen;

public class PCSCPreferenceActivity
    extends PreferenceActivity
    implements SharedPreferences.OnSharedPreferenceChangeListener
{
    public static final String TAG = "PCSCPref";
    public static final String PASSWORD_PREF = "password_preference";
    public static final String BT_NAME_PREF = "btname_preference";
    public static final String BT_ADDR_PREF = "btaddr_preference";

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Load the preferences from an XML resource
        addPreferencesFromResource(R.xml.preferences);
    }

    @Override
    protected void onResume() {
        super.onResume();

        SharedPreferences sharedPreferences = getPreferenceScreen().getSharedPreferences();

        Map<String, ?> map = sharedPreferences.getAll();
        Iterator<String> iter = map.keySet().iterator();
        while(iter.hasNext()){
            String key = iter.next();
            Preference pref= getPreferenceScreen().findPreference(key);
            Object val = map.get(key);
            updateSummary(pref, key, val);
        }

        // Set up a listener whenever a key changes
        sharedPreferences.registerOnSharedPreferenceChangeListener(this);
    }

    @Override
    public boolean onPreferenceTreeClick(PreferenceScreen preferenceScreen,
                                         Preference preference) {
        return true;
    }

    @Override
    public void onSharedPreferenceChanged(SharedPreferences sharedPreferences, String key) {
        Map<String, ?> map = sharedPreferences.getAll();
        Preference pref= getPreferenceScreen().findPreference(key);
        Object val = map.get(key);
        updateSummary(pref, key, val);
    }

    private void updateSummary(Preference pref, String key, Object val) {
        if (pref == null || key == null) {
            return;
        }
        SharedPreferences sharedPreferences =
            getPreferenceScreen().getSharedPreferences();

        if (key.equals(PASSWORD_PREF)) {
            String passwd = (String)val;
            if (passwd.length() > 0) {
                pref.setSummary("********");
            } else {
                pref.setSummary("");
            }
        } else if (key.equals(BT_NAME_PREF)) {
            pref.setSummary(val.toString());
        } else if (pref instanceof EditTextPreference) {
            pref.setSummary(val.toString());
        } else if (pref instanceof CheckBoxPreference) {
        } else if (pref instanceof ListPreference) {
            ListPreference listPref = (ListPreference)pref;
            pref.setSummary(listPref.getEntry());
        }
    }
}
