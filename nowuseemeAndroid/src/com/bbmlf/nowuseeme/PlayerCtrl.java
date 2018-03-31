package com.bbmlf.nowuseeme;

import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;

import android.content.Context;
import android.util.Log;

/**
 * class PlayerCtrl.
 */
public class PlayerCtrl
{
	/* General private attributes */
	private final static String LOG_TAG= "PlayerCtrl";
	private Context m_context= null;
	//private MainActivity m_parentActivity;
	//private PlayerCtrl m_this= this;
	private Lock m_lock= new ReentrantLock();
    private PlayerView m_player= null;
    private boolean m_isPlayingOnPause= false;
    String m_url= null;
    private boolean m_a_mute_on= false;
    private int m_autoReconnectTimeOut= 5; // [seconds]

	/**
	 * 'PlayerCtrl' class constructor
	 */
	public PlayerCtrl(Context context)
	{
		m_context= context;
		//m_parentActivity= (MainActivity)context;
	}

	/**
	 * Open 'PlayerCtrl' class.
	 */
	public void open()
	{
		m_lock.lock();

        /* Save Main-Activity environment and object handlers into globals
         * variables of JNI level 
         */
        setJavaObj();

		m_player= new PlayerView(m_context);

		m_lock.unlock();
	}

	/**
	 * Close 'PlayerCtrl' class.
	 */
	public void close()
	{
		m_lock.lock();

		m_player= null;

    	/* Release global references at JNI level */
    	releaseJavaObj();

		m_lock.unlock();
	}

	/**
	 * Pause 'PlayerCtrl' class
	 */
	public void pause()
	{
		m_lock.lock();

    	if(m_player.isPlaying() )
    	{
    		m_isPlayingOnPause= true;
    		m_player.stop();
    	}

		m_lock.unlock();
	}

	/**
	 * Resume 'PlayerCtrl' class
	 */
	public void resume()
	{
		m_lock.lock();

		/* Resume player if it is the case */
        if(m_isPlayingOnPause== true)
        {
        	Thread thread= new Thread()
        	{
        		@Override
        		public void run()
        		{
        			/* Hack (have to code this better):
        			 * Make sure player surface is created before starting rendering
        			 * */
        	        try
        	        {
        	        	int count= 0, max_count= 10;
        	            while(m_player!= null && !m_player.isSurfaceCreated() 
        	            		&& count< max_count)
        	            {
        	            	Log.v(LOG_TAG, "testing player resume interrumption'...");
        	                Thread.sleep(500); // sleep 500 milliseconds ...
        	                count++;
        	            }
        	            if(count>= max_count)
        	            {
        	            	Log.e(LOG_TAG, "Could not create player surface view");
        	            	return; // exit run()
        	            }
        	        }
        	        catch(InterruptedException e)
        	        {
        	        	Log.e(LOG_TAG, "Could not start video player");
        	        	return; // exit run()
        	        }

        	        /* Start, on resume, the player */
            		m_player.start(m_url);
            		m_isPlayingOnPause= false;
        		} // run()
        	};
        	thread.start();
        }

		m_lock.unlock();
	}

	/**
	 * Start 'PlayerCtrl' class
	 */
	public void start()
	{
		m_lock.lock();
		m_lock.unlock();
	}

	/**
	 * Stop 'PlayerCtrl' class
	 */
	public void stop()
	{
		m_lock.lock();
		m_player.stop();
		m_lock.unlock();
	}

    /****************************************************************
     ***************** API IMPLEMENTATION ***************************
     ****************************************************************/

	/*==== A/V Player ====*/

	// set video thumbnail URL (for 2 way video) or null to hide, and the position
	public void viewVideoDisplay(final boolean display, final int width, final int height, 
			final int x, final int y) //(String url, int top, int left, int width, int height)
	{
		m_lock.lock();

		m_player.setLayoutParams(display, width, height, x, y);
		Log.v(LOG_TAG, "'viewVideoDisplay(display: "+ display+ " width: "+ width+ " height: "+ height+ 
				" x: "+ x+ " y: "+ y + ")'");

		m_lock.unlock();
	}

	public String viewVideoGet()
	{
		String s;
		m_lock.lock();

		Log.d(LOG_TAG, "'viewVideoGet()'");
		s= m_player.viewVideoGet();

		m_lock.unlock();
		return s;
	}

	// get/set publish status
	public void videoPublishSet(String url, boolean start)
	{
		m_lock.lock();

		/* Use default if noURL/"null" is specified */
		if(url== null || url.isEmpty() )
		{
			m_url= m_player.getDefaultPublishURL();
		}
		else
		{
			m_url= url;
		}

		if(start== true)
		{
			m_player.start(m_url);
		}
		else
		{
			m_player.stop();
		}

		m_lock.unlock();
	}

	public boolean videoPublishGet()
	{
		boolean ret;
		m_lock.lock();

		ret= (m_player!= null && m_player.isPlaying() );
		Log.v(LOG_TAG, "'videoPublishGet()' returns: "+ ret);

		m_lock.unlock();
		return ret;
	}

	// set/get mute on player
    public boolean setMutePlayer(boolean on)
    {
    	Log.v(LOG_TAG, "'setMutePlayer()' set to: "+ on);
    	this.m_a_mute_on= on;
    	m_player.setMute(on);
		if(this.getMutePlayer()== on)
		{
			return true; // OK
		}
		return false; // KO
    }

    public boolean getMutePlayer()
    {
    	return this.m_a_mute_on;
    }

    public void autoReconnectSet(int timeout)
    {
    	m_autoReconnectTimeOut= timeout;
    	connectionTimeOutSet(Math.abs(timeout));
    }

    public int autoReconnectGet()
    {
    	return m_autoReconnectTimeOut;
    }

	/**
	 * JNI to Java bridge on-error callback
	 * @param errorCode Integer indicating error code
	 */
	private void onError(int errorCode)
	{
		Log.v(LOG_TAG,"'onError()'...");

		/* Set error code */
		((MainActivity)m_context).setLastErrorCode(ErrorCodes.getByCode(errorCode) );

		if(errorCode!= ErrorCodes.SUCCESS.getCode() )
		{
			/* Stop/disconnect RTMP server */
			this.stop();
	
			/* Auto-reconnect if apply */
			if(m_autoReconnectTimeOut>= 0)
			{
				m_player.start(m_url);
			}
		}
	}

	private void onInputResolutionChanged(int width, int heigth)
	{
		Log.v(LOG_TAG,"'onInputResolutionChanged(" +
				""+ width+ ", "+ heigth+ ")'...");
		((MainActivity)m_context).onPlayerInputResolutionChanged(width, heigth);
	}

    /* 
     * Private native methods implemented by native library packaged
     * with this application.
     */
	private native void setJavaObj();
	private native void releaseJavaObj();
	private native void connectionTimeOutSet(int tout);
}
