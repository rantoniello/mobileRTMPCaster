package com.bbmlf.nowuseeme;

import android.util.Log;
import android.content.Context;
import android.content.SharedPreferences;

/*
 * User preferences class
 */
public class Preferences
{
	private MainActivity m_parentActivity;

    public Preferences(MainActivity activity)
    {
        m_parentActivity= activity;
    }

    /**
     * Open 'Preferences' class.
     */
    public void open()
    {
        SharedPreferences preferences= m_parentActivity.getSharedPreferences("nowuseeme", Context.MODE_PRIVATE);
        if (!preferences.getBoolean("isFirstRun", false)) {
            SharedPreferences.Editor edit= preferences.edit();
            edit.putBoolean("isFirstRun", true);
            edit.commit();
            onFirstRun();
        }
    }

    /**
     * TODO
     */
    public void onFirstRun() {
        // TODO
    	return;
    }
}
