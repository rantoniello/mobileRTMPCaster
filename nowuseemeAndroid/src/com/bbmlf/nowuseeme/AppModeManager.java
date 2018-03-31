package com.bbmlf.nowuseeme;

import java.io.UnsupportedEncodingException;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;

import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.os.Bundle;
import android.util.Log;

public class AppModeManager
{
    /* Android related */
    private final static String LOG_TAG= "AppModeManager";
    private MainActivity m_parentActivity;
    private final int m_intKey = (352*288)+((352*288)/2);

    public static final byte UNKNOWN_MODE = -1;
    public static final byte LAUNCHER_MODE = 0;
    public static final byte CUSTOMWEB_MODE = 1;
    public static final byte CCN_CH_MODE = 125;
    public static final byte CCN_VI_MODE = 126;

    private byte startMode= UNKNOWN_MODE;
    public int getStartMode() { return startMode; }

    public AppModeManager(MainActivity activity)
    {
    	m_parentActivity= activity;
    }

    public void onNewIntent(Intent intent) {
    	Log.v(LOG_TAG, "Activity 'onNewIntent()'");
    	Bundle _extraParams = null;
    	final byte _newMode;
    	
    	if (intent != null &&
    			(_extraParams = intent.getExtras()) != null) {
    		_newMode = _extraParams.getByte("m", LAUNCHER_MODE);
    	} else {
    		_newMode = LAUNCHER_MODE; 
    	}
    	
    	final Intent _intentFinal = intent;
    	final Bundle _extraParamsFinal = _extraParams;
    	if (_newMode != startMode) {
// NOTE: As this is called only from "MainActivity::onCreate", "startMode==  UNKNOWN_MODE" always at this point 
//        	if (startMode != UNKNOWN_MODE) {
//	    		new AlertDialog.Builder(m_parentActivity)
//		    		.setIcon(R.drawable.icon)
//		    		.setTitle(R.string.ApplicationName)
//		    		.setMessage(
//		    				(m_parentActivity.getRtmpClientCtrl().isRecording() || m_parentActivity.getPlayerCtrl().videoPublishGet() ? 
//		    						R.string.NewInstanceWhileStreaming : R.string.NewInstanceNoStreaming)
//		    		)
//		    		.setPositiveButton(R.string.GenericOK, new DialogInterface.OnClickListener() {
//						@Override
//						public void onClick(DialogInterface dialog, int which) {
//							byte newMode = _newMode;
//							switchIntentMode(_intentFinal, _extraParamsFinal, newMode);
//						}
//					})
//					.setNegativeButton(R.string.GenericCancel, null)
//					.show();
//        	} else {
        		switchIntentMode(_intentFinal, _extraParamsFinal, _newMode);
        		startMode = _newMode;
//        	}
    	}
    }

    public void switchIntentMode(Intent intent, Bundle extraParams, final byte newMode)
    {
    	String webviewURL = null;
//    	String _md5Src = null;
    	WebViewMod webView= (WebViewMod)m_parentActivity.findViewById(R.id.myWebView);
    	
    	Log.v(LOG_TAG, "new start mode : " + newMode);
    	
        if(MainActivity.m_debugMethods)
	    {
        	webviewURL = m_parentActivity.getWebviewInternalDebugURL();
	    }
        else
        {
        	webviewURL = m_parentActivity.getWebviewInternalURL();
        }

// Note: Code below always execute 'LAUNCHER_MODE' case
//		switch(newMode) {
//			case LAUNCHER_MODE:
//				break;
//				
//			case CCN_CH_MODE:
//			case CCN_VI_MODE:
//				_md5Src = extraParams.getString("sUrl");
//				if (_md5Src != null && _md5Src.length() > 15) {
//					String _receivedMD5 = _md5Src.substring(_md5Src.length() - 7);
//					_md5Src = _md5Src.substring(0, _md5Src.length() - 7);
//					if (!_receivedMD5.equalsIgnoreCase(md5(m_parentActivity.getWebviewInternalURL() + _md5Src +  m_intKey + "JKHsdfu4uoiu4b").substring(7,14))) {
//						Log.w(LOG_TAG, "Encoding check failed - " + _md5Src + "; " + _receivedMD5);
//						startMode = LAUNCHER_MODE;
//					} else {
//						webviewURL = _md5Src;
//				        Log.v(LOG_TAG, "Page loaded");
//					}
//				}
//				break;
//				
//			default:
//				_md5Src = extraParams.getString("sUrl");
//				if (_md5Src != null && _md5Src.length() > 32) {
//					String _receivedMD5 = _md5Src.substring(_md5Src.length() - 32);
//					_md5Src = _md5Src.substring(0, _md5Src.length() - 32);
//					if (!_receivedMD5.equalsIgnoreCase(md5(m_parentActivity.getWebviewInternalURL() + _md5Src +  m_intKey + "JKHsdfu4uoiu4b").substring(3,7))) {
//						Log.w(LOG_TAG, "Encoding check failed - " + _md5Src + "; " + _receivedMD5);
//						startMode = LAUNCHER_MODE;
//					} else {
//						webviewURL = _md5Src;
//					}
//				}
//				break;
//		}
		
		if (webView.getUrl() == null || !webView.getUrl().startsWith(webviewURL)) {
			m_parentActivity.notificationRemove(0);
			Log.v(LOG_TAG, "Load URL " + webviewURL);
			//_toClearHistory = true; //FIXME: set in kind-of Chrome-manager function
			
			m_parentActivity.loadUrl(webviewURL);
			startMode = newMode;
			m_parentActivity.setIntent(intent);
		}
		//	m_webView.loadDataWithBaseURL(baseUrl, data, mimeType, encoding, historyUrl)
    }
    
//	public static String md5(String string) {
//	    byte[] hash;
//
//	    try {
//	        hash = MessageDigest.getInstance("MD5").digest(string.getBytes("UTF-8"));
//	    } catch (NoSuchAlgorithmException e) {
//	        throw new RuntimeException("Huh, MD5 should be supported?", e);
//	    } catch (UnsupportedEncodingException e) {
//	        throw new RuntimeException("Huh, UTF-8 should be supported?", e);
//	    }
//
//	    StringBuilder hex = new StringBuilder(hash.length * 2);
//
//	    for (byte b : hash) {
//	        int i = (b & 0xFF);
//	        if (i < 0x10) hex.append('0');
//	        hex.append(Integer.toHexString(i));
//	    }
//
//	    return hex.toString();
//	}
}
