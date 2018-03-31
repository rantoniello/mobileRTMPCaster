package com.bbmlf.nowuseeme;

import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;

import android.util.Log;
import android.content.Context;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.WindowManager.LayoutParams;
import android.widget.RelativeLayout;

class PlayerView extends SurfaceView implements SurfaceHolder.Callback
{
    /* Private attributes */
    private final static String LOG_TAG= "PlayerView";
    Context m_context= null;
    private volatile boolean m_playing= false;
    private volatile boolean m_isPlayerSurfaceCreated= false;
    private String m_url= null, m_urlDefault= "rtmp://10.0.0.103:1935/myapp/mystream"; // default value;
    private int m_top= 0, m_left= 0; // defaults
    private int m_width= 640, m_height= 360; //defaults
    private Lock m_lock= new ReentrantLock();
    private boolean m_a_mute_on= false;

    public PlayerView(Context context)
    {
    	super(context);

    	Log.d(LOG_TAG, "Creating PlayerView...");

		m_context= context;
		getHolder().addCallback(this);
		requestFocus();
		setFocusableInTouchMode(true);

		/* Add Player layout (size 1x1~ hidden layout) */ 
		RelativeLayout.LayoutParams layoutParams= new RelativeLayout.LayoutParams(
				LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT);
		layoutParams.width= 1; layoutParams.height= 1;
		((MainActivity)m_context).addContentView(this, layoutParams);

		Log.d(LOG_TAG, "PlayerView created O.K.");
    }

    public void surfaceChanged(SurfaceHolder holder, int format, int w, int h)
    {
    	Log.d(LOG_TAG, "Executing 'surfaceChanged()'..."); // FIXME!!
    	resetSurface(m_width, m_height, getHolder().getSurface() ); //FIXME!!
    }

    public void surfaceCreated(SurfaceHolder holder)
    {
    	Log.d(LOG_TAG, "Executing 'surfaceCreated()'...");
    	m_isPlayerSurfaceCreated= true;
    }

    public void surfaceDestroyed(SurfaceHolder holder)
    {
    	Log.d(LOG_TAG, "Executing 'surfaceDestroyed()'...");
    	m_isPlayerSurfaceCreated= false;
    }

    /**
     * Set layout characteristics
     */
    public void setLayoutParams(final boolean display, int width, 
    		int height, int x, int y)
    {
    	m_lock.lock();

    	Log.d(LOG_TAG, "Executing 'setLayoutParams()'... ");
    	try
    	{
    		/* Set position and dimension in pixels */
    		m_top= y; m_left= x;
    		m_width= width; m_height= height;
        	DisplayCtrl.changeDisplayParams(this, (MainActivity)m_context, 
        			display, width, height, x, y);
    	}
    	finally
    	{
    		Log.d(LOG_TAG, "Exiting 'setLayoutParams()'... ");
    		m_lock.unlock();
    	}
    }

	/**
	 * Create new running thread to start playing A/V samples.
	 */
    public void start(String url)
    {
    	m_lock.lock();
    	try
    	{
    		start2(url);
    	}
    	finally
    	{
    		m_lock.unlock();
    	}
    }
	private void start2(String url)
	{
		Log.d(LOG_TAG, "Starting video player...");

		if(m_playing)
		{
			Log.w(LOG_TAG, "Could not start player thread, already " +
					"running. Please stop player before starting it.");
			return;
		}
		if(url== null || url.isEmpty() )
		{
			Log.e(LOG_TAG, "Could not start player thread, non valid URL");
			return;
		}
		m_url= url;

		/* Start running thread */
		m_playing= true;

		/* Start native demuxer thread */
		playerStart(m_url.getBytes(), m_width, m_height, m_a_mute_on? 1: 0, getHolder().getSurface() );

		Log.v(LOG_TAG, "Video player started O.K.");
	}

	/**
	 * Stop already running video player instance: waits player thread 
	 * to end (blocking) and release it.
	 */
	public void stop()
    {
		m_lock.lock();
    	try
    	{
    		stop2();
    	}
    	finally
    	{
    		m_lock.unlock();
    	}
    }
	private void stop2()
	{
		Log.d(LOG_TAG, "Stopping video player...");

		/* Release native player */
        playerStop();
		m_playing= false;

		Log.v(LOG_TAG, "Video player stopped O.K.");
	}

	/**
	 * Check if PlayerView is playing A/V.
	 * @return Boolean indicating if PlayerView is currently playing.
	 */
	public boolean isPlaying()
	{
		Log.d(LOG_TAG, "isPlaying() returns: "+ m_playing);
		return m_playing;
	}

	/**
	 * Check if PlayerView has its surface created
	 * @return Boolean indicating if surface is already created
	 */
	public boolean isSurfaceCreated()
	{
		return m_isPlayerSurfaceCreated;
	}

	/**
	 * Mute player audio
	 * @param on boolean to mute/un-mute
	 */
	public void setMute(boolean on)
    {
		m_lock.lock();
    	try
    	{
    		m_a_mute_on= on;
    		playerMute(on? 1: 0);
    	}
    	finally
    	{
    		m_lock.unlock();
    	}
    }

    /*
     * API IMPLEMENTATION SUPPORT
     */
	public String viewVideoGet()
	{
		String s= "{\"url\":\""+ m_url+ "\","+ 
				  "\"top\":\""+ m_top+ "\","+
				  "\"left\":\""+ m_left+ "\","+
				  "\"width\":\""+ m_width+ "\","+
				  "\"height\":\""+ m_height+ "\"}";
		return s;
	}
	public String getDefaultPublishURL()
	{
		return (String)m_urlDefault;
	}

    /* 
     * Private native methods implemented by native library packaged
     * with this application.
     */
	private native void playerStart(byte[] url, int width, int height, int isMuteOn, Surface surface);
	private native void playerStop();
	private native void playerMute(int on);
	private native void resetSurface(int width, int height, Surface surface); //FIXME!!
}
