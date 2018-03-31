package com.bbmlf.nowuseeme;

import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;

import android.content.Context;
import android.hardware.Camera;
import android.os.Looper;
import android.util.Log;

/**
 * Class for handling camera on a user Thread.
 * "Camera" object is created in a event Thread (owning a Looper) 
 * to avoid executing in main UI thread
 */
public class CameraThread
{
	private final static String LOG_TAG= "CameraThread";
	public final int MAX_WAITTIME_MILLIS= 10*1000;
	private Thread m_thread= null;
	private Looper m_threadLooper= null;
	private Camera m_camera= null;
	private int m_cameraId= 0; // default
	private MainActivity m_parentActivity= null;
	private ErrorCodes m_errorCode= ErrorCodes.SUCCESS;
	private Lock m_lock= new ReentrantLock(); // mutual-exclusion lock for public interface

	public CameraThread(Context context, int cameraId)
	{
		m_parentActivity= (MainActivity)context;
		m_cameraId= cameraId;
	}

	public Camera open()
	{
		Log.d(LOG_TAG, "Opening camera");

		Camera camera;
		m_lock.lock();
		try
		{
			camera= open2();
		}
    	finally
    	{
    		m_lock.unlock();
    	}

		if(camera!= null)
			Log.d(LOG_TAG, "Camera successfuly opened");
		else
			Log.d(LOG_TAG, "Camera failed to open");
		return camera;
	}

	public void close()
	{
		Log.d(LOG_TAG, "Closing camera");

		m_lock.lock();
		try
		{
			close2();
		}
    	finally
    	{
    		m_lock.unlock();
    	}
		Log.d(LOG_TAG, "Camera closed");
	}

    private Camera open2()
    {
		m_thread= new Thread()
		{
			@Override
			public void run()
			{
				Looper.prepare();
				m_threadLooper= Looper.myLooper();
				synchronized(this)
				{
	        		// Open camera
	        		try
	        		{
	        			m_camera= Camera.open(m_cameraId);
	        			Log.v(LOG_TAG, "Native camara successfuly opened");
	        		}
	        		catch(Exception e)
	        		{
	        			Log.e(LOG_TAG, "Could not open designed camera; id= "+ m_cameraId);
	        			m_errorCode= ErrorCodes.OPEN_CAMERA_ERROR;
	        		}
					this.notify();
				}
				Looper.loop();
			}
		};
		synchronized(m_thread)
		{
			m_thread.start(); //will block until we wait
			Log.v(LOG_TAG, "Wait for camera instance to be created...");
			try
			{
				m_thread.wait(MAX_WAITTIME_MILLIS); // wait no more than 10 secs... 
			}
			catch(Exception e)
			{
				Log.e(LOG_TAG, "Camera instance creation was interrupted");
				m_errorCode= ErrorCodes.OPEN_CAMERA_ERROR;
			}
		}

		if(m_camera== null || m_errorCode!= ErrorCodes.SUCCESS)
		{
			m_parentActivity.setLastErrorCode(m_errorCode);
			return null;
		}

		Log.v(LOG_TAG, "... camera instance successfuly created, quit form waiting.");
		return m_camera;
    }

    private void close2()
    {
    	Log.v(LOG_TAG, "Stopping camera preview");
		if(m_camera!= null)
		{
			m_camera.stopPreview();
		}

		Log.v(LOG_TAG, "Quit camera thread looper");
        if(m_threadLooper!= null)
        {
        	m_threadLooper.quit();
        	m_threadLooper= null;
        }

        Log.v(LOG_TAG, "Join camera thread");
		if(m_thread!= null && m_thread.isAlive() )
		{
			Log.v(LOG_TAG, "Camera thread is alive; wait join...");
			try
			{
				m_thread.join();
			}
			catch(InterruptedException e)
			{
				Log.e(LOG_TAG, "Camera thread join wait was interrupted.");
			}
			Log.v(LOG_TAG, "Camera thread joined.");
		}
		if(m_thread!= null) m_thread= null;

		Log.v(LOG_TAG, "Release camera");
		if(m_camera!= null)
		{
			m_camera.setPreviewCallback(null);
			m_camera.release();
			m_camera= null;
		}
    }
}
