package com.bbmlf.nowuseeme;

import org.acra.*;
import org.acra.annotation.*;
import org.apache.commons.validator.routines.UrlValidator;
import org.json.JSONObject;

import android.annotation.SuppressLint;
import android.app.ActionBar;
import android.app.Activity;
import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.media.AudioManager;
import android.media.RingtoneManager;
import android.net.Uri;
import android.net.wifi.WifiManager;
import android.net.wifi.WifiManager.WifiLock;
import android.os.Bundle;
import android.os.PowerManager;
import android.support.v4.app.NotificationCompat;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.KeyEvent;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.view.ViewTreeObserver.OnGlobalLayoutListener;
import android.widget.Toast;

import com.google.zxing.integration.android.*;
import com.bbmlf.nowuseeme.R;
import com.bbmlf.nowuseeme.Preferences;
import com.bbmlf.nowuseeme.AppModeManager;

@SuppressLint("SetJavaScriptEnabled")
public class MainActivity extends Activity
{
    /* 
     * Native libraries packaged with this application.
     * Warning: respect current loading order!
     */
    static
    {
    	System.loadLibrary("vo-aacenc");
    	System.loadLibrary("mp3lame");
    	System.loadLibrary("x264");
    	System.loadLibrary("avutil");
    	System.loadLibrary("swresample");
    	System.loadLibrary("avcodec");
    	System.loadLibrary("avformat");
    	System.loadLibrary("swscale");
    	System.loadLibrary("postproc");
    	System.loadLibrary("avfilter");
    	System.loadLibrary("avdevice");
    	System.loadLibrary("main-jni");
    	System.loadLibrary("rtmpclient-jni");
    	System.loadLibrary("player-jni");
    }

    /* Android related */
    private final static String LOG_TAG= "MainActivity";

    /* App. control related */
    public final static boolean m_debugMethods= false; //true; // debugging purposes: put to "false" in RELEASE version
    private ErrorCodes m_lastErrorCode= ErrorCodes.SUCCESS;
    private WebViewMod m_webView;
    private Preferences m_preferences= new Preferences(this);
    private AppModeManager m_AppModeManager= new AppModeManager(this);
    public AppModeManager getAppModeManager() { return m_AppModeManager; }
    private RtmpClientCtrl m_RtmpClientCtrl= new RtmpClientCtrl(this);
    public RtmpClientCtrl getRtmpClientCtrl() { return m_RtmpClientCtrl; }
    private PlayerCtrl m_PlayerCtrl= new PlayerCtrl(this);
    public PlayerCtrl getPlayerCtrl() { return m_PlayerCtrl; }
    protected NotificationManager NM;
    public DisplayMetrics displayMetrics = new DisplayMetrics(); //FIXME!: duplicated
	protected StringBuilder mPendingJS;
	protected boolean javascriptUseQueue;
    protected boolean javascriptAvailable = false;
    protected String javascriptPrefix = null;
    protected boolean isKeyboardDisplayed = false;
    protected boolean isKeyboardDisplayedAnnounced = false;
    protected String webviewInternalURL = "file:///android_asset/index.html";
    public String getWebviewInternalURL() { return webviewInternalURL; }
    protected String webviewInternalDebugURL = "file:///android_asset/index_debug.html";
    public String getWebviewInternalDebugURL() { return webviewInternalDebugURL; }
    protected Uri uriSoundAlarm;
    protected Uri uriSoundRingtone;
    protected Uri uriSoundNotification;
    protected PowerManager.WakeLock fullWakeLock;
    protected PowerManager.WakeLock backgroundWakeLock;
	protected String rtmpURL = "";
	protected boolean isBackground;
	protected Toast backtoast;
	protected final Object LOCK = new Object();
	private final static DisplayMetrics m_displayMetrics= new DisplayMetrics();
	public static DisplayMetrics getDisplayMetrics() { return m_displayMetrics; }
    protected boolean m_disableCache = true;
    private Toast lastToast; // FIXME: Take Toast messaging to specific class.
    public Toast getLastToast() { return lastToast; };
    public void setLastToast(Toast t) { lastToast= t; };

    public static final int NOTIFICATION_ID_RUNNING = 5;
    public static final int NOTIFICATION_ID_MESSAGE = 10;
    public static final int NOTIFICATION_ID_RING = 20;

    @Override
    public void onCreate(Bundle savedInstanceState)
    {
    	Log.v(LOG_TAG, "Activity 'onCreate()'");
        super.onCreate(savedInstanceState);
        getWindow().requestFeature(Window.FEATURE_PROGRESS);
        wifiLockAcquire();
        getWindowManager().getDefaultDisplay().getMetrics(displayMetrics);
        NM = (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);
        setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        setContentView(R.layout.activity_main);

        /* Initialize display metrics */
		getWindowManager().getDefaultDisplay().getMetrics(m_displayMetrics);

        /* Crate initial application layout */
        initLayout();
        onNewIntent(getIntent());

        /* Initialize preferences */
        m_preferences.open();

        createWakeLocks();
        
        uriSoundsLoad();
        
        Runtime.getRuntime().addShutdownHook(new Thread() {
        	public void run() {
        		NotificationManager NM = (NotificationManager)getSystemService(Context.NOTIFICATION_SERVICE);
                NM.cancel(5);
                NM.cancel(10);
                NM.cancel(20);
                System.out.println("Running Shutdown Hook");
            }
        });

        /* GLobally initialize FFmpeg libraries */
        initFFmpeg();

        /* Initialize rtmp-client */
        m_RtmpClientCtrl.open();

        /* Initialize player */
        m_PlayerCtrl.open();
    }

    @Override
    public void onNewIntent(Intent intent)
    {
    	Log.v(LOG_TAG, "Activity 'onNewIntent()'");
    	super.onNewIntent(intent);
    	m_AppModeManager.onNewIntent(getIntent());
    }

    @Override
    protected void onDestroy()
    {
    	Log.v(LOG_TAG, "Activity 'onDestroy()'...");

    	/* Un-initialize rtmp-client */
    	m_RtmpClientCtrl.close();

    	/* Un-initialize player */
    	m_PlayerCtrl.close();

        super.onDestroy();
    }

    @Override
    protected void onPause()
    {
    	Log.v(LOG_TAG, "Activity 'onPause()'...");

    	/* Pause rtmp-client */
    	m_RtmpClientCtrl.pause();

    	/* Pause player */
    	m_PlayerCtrl.pause();

    	/* Launch "on-background" event (=true) )to Javascript interface */
        onBackgroundStateChange(true);

    	super.onPause();
    }

	@Override
    protected void onResume()
    {
		Log.v(LOG_TAG, "Activity 'onResume()'...");

        super.onResume();

        wakeDevice();

        /* Resume rtmp-client */
        m_RtmpClientCtrl.resume();

        /* Resume player */
        m_PlayerCtrl.resume();

    	/* Launch "on-background" event (=false) )to Javascript interface */
        onBackgroundStateChange(false);
        
        NM.cancel(NOTIFICATION_ID_RING);
		NM.cancel(NOTIFICATION_ID_MESSAGE);
    }

    /**
     * Initialize application Layouts
     */
    private void initLayout()
    {
    	final View activityRootView = findViewById(R.id.record_layout);
    	activityRootView.getViewTreeObserver().addOnGlobalLayoutListener(new OnGlobalLayoutListener() {
    		@Override
    		public void onGlobalLayout() {
    			int heightDiff = activityRootView.getRootView().getHeight() - 
    					activityRootView.getHeight();
    			
    	        boolean _isKeyboardDisplayed;
    			if (heightDiff > 200) {
    				_isKeyboardDisplayed = true;
    			} else {
    				_isKeyboardDisplayed = false;
    			}
    			if (isKeyboardDisplayed != _isKeyboardDisplayed ||
    				!isKeyboardDisplayedAnnounced) {
    				isKeyboardDisplayedAnnounced = true;
    				isKeyboardDisplayed = _isKeyboardDisplayed;
    				javaFnCall("onKeyboardShown(" + (isKeyboardDisplayed ? "true" : "false") + ")");
    			}
    		}
    	});
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

        /* Get WebBiew*/
        m_webView= (WebViewMod)this.findViewById(R.id.myWebView);
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event)
    {
    	/* Quit when back button is pushed */
/*        if ((keyCode== KeyEvent.KEYCODE_BACK) && m_webView.canGoBack())
        {
        	Log.v(LOG_TAG, "onKeyDown - can go back ? " + m_webView.canGoBack() + "; url: " + m_webView.getUrl());
        	m_webView.goBack();
        	return true;
        }*/
        return super.onKeyDown(keyCode, event);
    }
    
    //when the application actually exits (not paused)
    @Override
    public void onSaveInstanceState(Bundle savedInstanceState) {
        // Always call the superclass so it can save the view hierarchy state
    	Log.v("MainActivity", "onSaveInstanceState()");
        super.onSaveInstanceState(savedInstanceState);
    }

    @Override
    public void onBackPressed() {
    	if (isKeyboardDisplayed) {
    		super.onBackPressed();
    	} else {
	        if(backtoast!=null && 
	        		backtoast.getView().getWindowToken()!=null) {
	            finish();
	        } else {
	      		if (lastToast != null) {
	      			try { lastToast.cancel();} catch (Exception e){}
	      			lastToast = null;
	      		}
	      		backtoast = Toast.makeText(this, R.string.ExitWhileStreaming, Toast.LENGTH_SHORT);
	            backtoast.show();
	        }
    	}
//        super.onBackPressed();
    }

    
    @Override
    public void finish() {
    	/* cancel any running javascript */
  		jsQueueAdd("finish()");
  		m_webView.post(new Runnable() {
            @Override
            public void run() {
            	m_webView.loadUrl("about:blank");
            }
        });

        if(backtoast!=null) {
        	backtoast.cancel();
        }
    	
    	/* remove all notifications */
    	notificationRemove(0);
    	
    	/* Stop rtmp-client */
    	m_RtmpClientCtrl.stop();

        /* Stop player */
        m_PlayerCtrl.stop();
        
    	super.finish();
    }

/*    public void onActivityResult(int requestCode, int resultCode, Intent intent)
    {
    	// Get bar-code scanner application result
    	IntentResult scanResult= IntentIntegrator.parseActivityResult(requestCode, resultCode, intent);
    	if(scanResult!= null && m_RtmpClientCtrl!= null)
    	{
    		Log.d(LOG_TAG, "'SCAN RESULT()': "+ scanResult.getContents());
    		m_RtmpClientCtrl.barcodeScanResultSet(scanResult.getContents());
    	}
    }*/
    
	/* Get bar-code scanner application result */
    public void onActivityResult(int requestCode, int resultCode, Intent intent) {
    	String[] schemes = {"rtmp"};
    	UrlValidator urlValidator = new UrlValidator(schemes, UrlValidator.ALLOW_ALL_SCHEMES);

    	switch (requestCode) {
    		case IntentIntegrator.REQUEST_CODE:
    			if (resultCode == Activity.RESULT_OK) {
    				IntentResult intentResult =
    					IntentIntegrator.parseActivityResult(requestCode, resultCode, intent);
    				if (intentResult != null) {
    					String contents = intentResult.getContents();
    					if (urlValidator.isValid(contents)) {
    						Log.v(LOG_TAG, "url is valid");
        		      		if (lastToast != null) {
        		      			try { lastToast.cancel();} catch (Exception e){}
        		      			lastToast = null;
        		      		}
        					lastToast = Toast.makeText(this, "QR URL: " + contents, Toast.LENGTH_LONG);
        		      		lastToast.show();
        		      		javaFnCall("onRTMPURLUpdate('" + contents + "')" );
    					} else {
        		      		if (lastToast != null) {
        		      			try { lastToast.cancel();} catch (Exception e){}
        		      			lastToast = null;
        		      		}
        					lastToast = Toast.makeText(this, "QR Scan returned an invalid URL", Toast.LENGTH_LONG);
        		      		lastToast.show();
    						Log.v(LOG_TAG, "url is invalid");
    					}
    				} else {
    					Log.e("SEARCH_EAN", "IntentResult je NULL!");
    				}
    			} else if (resultCode == Activity.RESULT_CANCELED) {
    				Log.e("SEARCH_EAN", "CANCEL");
    			}
    		break;
    	}
    }

    /****************************************************************
     ***************** API IMPLEMENTATION ***************************
     ****************************************************************/

    /*=== ERROR HANDLING ===*/

    public void setLastErrorCode(ErrorCodes err)
    {
    	this.m_lastErrorCode= err;
    }

    public int getLatestErrorCode()
    {
    	return this.m_lastErrorCode.getCode();
    }
    
    public String getLatestErrorText()
    {
    	return this.m_lastErrorCode.getDescription();
    }

	/*==== EVENTS ====*/
	private String m_onAMFMessageString;
	public void onAMFMessageSetString(String msg)
	{
		m_onAMFMessageString= msg;
	}
	public String onAMFMessageGetString()
	{
		return m_onAMFMessageString;
	}
	public void onAMFMessage(String msg)
	{
		Log.d(LOG_TAG, "'onAMFMessage()' recieved: "+ msg);
		// TODO
	}

	// application sent to background/foreground
	public void onBackgroundStateChange(boolean _isBackground)
	{
		if (isBackground != _isBackground) {
			isBackground = _isBackground;
			Log.d(LOG_TAG, "'onBackgroundStateChange()' recieved: "+ isBackground);
			javaFnCall("onBackgroundStateChange(" + isBackground + ")");
		}

	}

  	//client is connected/disconnected
	public void onConnectStatusChanged(boolean connected)
	{
		Log.d(LOG_TAG, "'onConnectStatusChanged()' recieved: "+ connected);
		javaFnCall("onConnectStatusChanged(" + connected + ")");
	}

	//player input resolution changed
	public void onPlayerInputResolutionChanged(int width, int heigth)
	{
		Log.d(LOG_TAG, "onPlayerInputResolutionChanged(" + width + "," + heigth + ")");
		javaFnCall("onPlayerInputResolutionChanged("+width+","+heigth+")");
	}

	//make JS calls to the current webview context
  	public void javaFnCall(final String jsString) 
  	{
  		if (!javascriptAvailable)
  			return;
  		
  		
		if (javascriptUseQueue) {
	  		jsQueueAdd(jsString);
			return;
		}
		
		try {
			// Add this to avoid android.view.windowmanager$badtokenexception unable to add window
			if(!isFinishing())
				// loadurl on UI main thread
/*	            if(Android.OS.Build.VERSION.SdkInt >= Android.OS.BuildVersionCodes.Kitkat) {
	                m_webView.EvaluateJavascript(webUrl, null);
	            } else {*/
		  		m_webView.post(new Runnable() {
		            @Override
		            public void run() {
						String webUrl = jsQueueGet();
						Log.v(LOG_TAG, "Calling javascript: " + jsString);
						m_webView.loadUrl(webUrl); 
		            }
		        });

/*	            runOnUiThread(new Runnable() {			
						@Override
						public void run() {
							String webUrl = jsQueueGet();
							Log.v(LOG_TAG, "Calling javascript: " + jsString);
							m_webView.loadUrl(webUrl); 
						}
					});*/
	            /*}*/
		} catch (Exception e ){
		}
	}

  	public void jsQueueAdd(String js) {
  		synchronized(LOCK) {
  			if (mPendingJS == null) {
  				mPendingJS = new StringBuilder();
  				mPendingJS.append("javascript: try{ " + (javascriptPrefix == null ? "" : javascriptPrefix));
  			}
  			mPendingJS
  				.append(js)
  				.append("; ");
  		}
  	}
  	
  	public String jsQueueGet() {
  		synchronized(LOCK) {
  			if (mPendingJS != null) {
	  			String pendingCommands = mPendingJS.toString();
	  			mPendingJS = null;
	  			return pendingCommands + " } catch (error) {nowuseeme.onError(error.message);}";
  			} else {
  				return null;
  			}
  		}
  	}

  	//called by HTML when ready to receive events
    public void javascriptInit(boolean available, String jsPrefix, boolean useQueue) 
    {
    	javascriptUseQueue = useQueue;
    	javascriptAvailable = available;
    	if (jsPrefix != null)
    		javascriptPrefix = jsPrefix;
    	Log.v(LOG_TAG, "javascript init");
    }
    
    //toggle system status bar, allowing the app to go full screen
    public void toggleFullscreen(boolean fullscreen){
        WindowManager.LayoutParams attrs = getWindow().getAttributes();
        if (fullscreen)
        {
            attrs.flags |= WindowManager.LayoutParams.FLAG_FULLSCREEN;
        }
        else
        {
            attrs.flags &= ~WindowManager.LayoutParams.FLAG_FULLSCREEN;
        }
        getWindow().setAttributes(attrs);
    }

    //toggle title bar / action bar visibility
    public void toggleActionBar(final boolean val) {
    	runOnUiThread(new Runnable() {
    	     @Override
    	     public void run() {
    	     	ActionBar actionBar = getActionBar();
    		    if (val) {
    		    	actionBar.show();
    		    } else {
    		    	actionBar.hide();
    		    }

    	    }
    	});
    }

    //retrieve display information
    public String displayInfoGet() {
    	return "{\"width\" : " + this.displayMetrics.widthPixels + 
    			", \"height\" : " + this.displayMetrics.heightPixels + 
    			", \"density\" : " + this.displayMetrics.density + "}";
    }

    public void notificationRemove(int intType) {
    	if (intType != 0) {
    		NM.cancel(intType);
    	} else {
    		NM.cancel(NOTIFICATION_ID_RING);
    		NM.cancel(NOTIFICATION_ID_MESSAGE);
    		NM.cancel(NOTIFICATION_ID_RUNNING);
    	}
    }
    
    @SuppressWarnings("deprecation")
	public void notificationAdd(String title, String text, String ticker, int intType) {
    	int priority = NotificationCompat.PRIORITY_HIGH;
    	boolean ongoing = false;
    	Uri uriSound = uriSoundNotification;
    	int audioStream = AudioManager.STREAM_NOTIFICATION;
    	int extraFlags = 0;
    	PendingIntent priorityIntent = null;
    	
    	if (title == null) {
        	notificationRemove(intType);
    		return;
    	}

    	switch (intType) {
    		//'connected' type notification, ongoing, no sound
    		case MainActivity.NOTIFICATION_ID_RUNNING:
    			priority = NotificationCompat.PRIORITY_MIN;
    			uriSound = null;
    			ongoing = true;
    			break;
    			
    		//Ring type, over lock screen, loop ringtone sound
    		case MainActivity.NOTIFICATION_ID_RING:
    			uriSound = uriSoundRingtone;
    			priority = NotificationCompat.PRIORITY_MAX;
    			audioStream = AudioManager.STREAM_RING;
    			extraFlags = Notification.FLAG_INSISTENT;
    			
    			Intent msgIntent = getIntent();
    			msgIntent.setFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP);
    			priorityIntent = PendingIntent.getActivity(MainActivity.this, 0, msgIntent, PendingIntent.FLAG_UPDATE_CURRENT);
    			
    	    	runOnUiThread(new Runnable() {
    	    		@Override
    	    		public void run() {
    	    			wakeDevice();
    	    			Window window = getWindow();
    	    			window.addFlags(WindowManager.LayoutParams.FLAG_DISMISS_KEYGUARD);
    	    			window.addFlags(WindowManager.LayoutParams.FLAG_SHOW_WHEN_LOCKED);
    	    			window.addFlags(WindowManager.LayoutParams.FLAG_TURN_SCREEN_ON);
    	    		}
    	    	});

    			ongoing = true;
    			
    			break;

    		//
    		case MainActivity.NOTIFICATION_ID_MESSAGE:
    		default:
    	    	runOnUiThread(new Runnable() {
    	    		@Override
    	    		public void run() {
    	    			wakeDevice();
    	    			Window window = getWindow();
    	    			window.addFlags(WindowManager.LayoutParams.FLAG_TURN_SCREEN_ON);
    	    		}
    	    	});
    			break;
    	}	  	

    	Intent startActivity = getIntent();
	  	startActivity.setFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP);
	  	PendingIntent pi = PendingIntent.getActivity(this, 0, startActivity, PendingIntent.FLAG_UPDATE_CURRENT);
	  	Notification notify = new NotificationCompat.Builder(getApplicationContext())
	  			.setContentIntent(pi)
	  			.setWhen(System.currentTimeMillis())
	  			.setAutoCancel(!ongoing)
	  			.setContentTitle(title)
	  			.setContentText(text)
	  			.setTicker(ticker)
	  			.setSmallIcon(R.drawable.icon)
	  			.setPriority(priority)
	  			.setOngoing(ongoing)
	  			.setFullScreenIntent(priorityIntent, false)
	  			.build();
	  
	  	if (uriSound != null) notify.sound = uriSound;
	  	notify.flags = notify.flags | extraFlags;
	  	notify.audioStreamType = audioStream;
	  	NM.notify(intType, notify);
    }
    
	@SuppressWarnings("deprecation")
	protected void createWakeLocks() {
		PowerManager powerManager = (PowerManager) getSystemService(Context.POWER_SERVICE);
	    fullWakeLock = powerManager.newWakeLock(PowerManager.FULL_WAKE_LOCK | PowerManager.ACQUIRE_CAUSES_WAKEUP | PowerManager.ON_AFTER_RELEASE, LOG_TAG);
	    backgroundWakeLock = powerManager.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, LOG_TAG);
	}
	
	public void wakeDevice() {
		fullWakeLock.acquire();
		fullWakeLock.release();
	}
	
    private void wifiLockAcquire(){
    	final WifiManager wifiManager=(WifiManager) getApplicationContext().getSystemService(Context.WIFI_SERVICE);
    	final WifiLock wifiLock=wifiManager.createWifiLock(LOG_TAG);
    	try {
    		wifiLock.acquire();
    		Log.v(LOG_TAG, "aquired");
    	} catch (  SecurityException e) {
    		Log.v(LOG_TAG, "failed");
    	}
    }
    
  	private void uriSoundsLoad() {
        uriSoundNotification =  RingtoneManager.getDefaultUri(RingtoneManager.TYPE_NOTIFICATION);
        uriSoundRingtone = RingtoneManager.getDefaultUri(RingtoneManager.TYPE_RINGTONE);
        uriSoundAlarm = RingtoneManager.getDefaultUri(RingtoneManager.TYPE_ALARM);
        if (uriSoundNotification == null) {
        	if (uriSoundRingtone != null) {
        		uriSoundNotification = uriSoundRingtone;
        	} else if (uriSoundAlarm != null) {
        		uriSoundNotification = uriSoundAlarm;
        	}
        }
        if (uriSoundRingtone == null) {
        	if (uriSoundAlarm != null) 
        		uriSoundRingtone = uriSoundAlarm;
        	else
        		uriSoundRingtone = uriSoundNotification;
        }
        if (uriSoundAlarm == null) {
        	if (uriSoundRingtone != null) 
        		uriSoundAlarm = uriSoundRingtone;
        	else
        		uriSoundAlarm = uriSoundNotification;
        }
    }

    public void sendEmail(String emailAddress, String subject, String body) {
        System.out.println("Email address::::" + emailAddress);
    
        final Intent emailIntent = new Intent(
                android.content.Intent.ACTION_SEND);
        emailIntent.setType("plain/text");
        emailIntent.putExtra(android.content.Intent.EXTRA_EMAIL, new String[] { emailAddress });
        emailIntent.putExtra(android.content.Intent.EXTRA_SUBJECT, subject);
        emailIntent.putExtra(android.content.Intent.EXTRA_TEXT, body);
        MainActivity.this.startActivity(Intent.createChooser(emailIntent, "Send mail..."));
    }
    
    public void openWebBrowser(String sURL) {
    	m_webView.openWebBrowser(sURL);
    }

  	public void loadUrl(final String url) { //FIXME
  		m_webView.loadUrl2(url, true);
  	}

    public void showToast(String value) {
  		if (lastToast != null) {
  			try { lastToast.cancel();} catch (Exception e){}
  			lastToast = null;
  		}
    	lastToast = Toast.makeText(this, (CharSequence) value, Toast.LENGTH_LONG);
  		lastToast.show();
    }
    /* 
     * Private native methods implemented by native library packaged
     * with this application.
     */
	private native void initFFmpeg();

	//public enum TrackerName { // Not used
	//	APP_TRACKER,  // Tracker used only in this app.
	//	GLOBAL_TRACKER, // Tracker used by all the apps from a company. eg: roll-up tracking.
	//	ECOMMERCE_TRACKER, // Tracker used by all ecommerce transactions from a company.
  	//}
	
  	//public interface OnTaskCompleted{ // Not used
  	//    void onTaskCompleted(JSONObject result);
  	//}
  	
  	//public class Callback implements OnTaskCompleted{
  	//    @Override
  	//    public void onTaskCompleted(JSONObject result) {
  	//        // do something with result here!
  	//    }
  	//}

  	//public void browserReload() { // Not used
    //    m_webView.post(new Runnable() {
    //        @Override
    //        public void run() {
    //            String r = m_webView.getOriginalUrl();
    //            if (r.equals("about:blank")) {
    //            	r = lastRequestedUrl;
    //            }
    //      		loadUrl(r);
    //        }
    //    });
  	//}

  	//public void browserBackgroundColorSet(String color) { // Not used
    //    m_webView.setBackgroundColor(Color.parseColor(color));
  	//}
}
