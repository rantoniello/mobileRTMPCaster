package com.bbmlf.nowuseeme;

import android.content.Context;
import android.content.pm.PackageManager;
import android.hardware.Camera;
import android.hardware.Camera.Parameters;
import android.hardware.Camera.PreviewCallback;
import android.util.Log;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.WindowManager.LayoutParams;
import android.widget.RelativeLayout;

import java.util.List;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;

/**
 * class VideoRecorder:
 * This class is in charge of capturing video frames from available cameras
 * and pass them to JNI RTMP encoding/multiplexing application.
 */
class VideoRecorder extends SurfaceView implements SurfaceHolder.Callback, PreviewCallback
{
    /* Private attributes */
    private final static String LOG_TAG= "VideoRecorder";
    public final int MAX_WAITTIME_MILLIS= 10*1000;
	private SurfaceHolder m_holder= null;
	private Camera m_camera= null;
	private Context m_context= null;
	private MainActivity m_parentActivity;
	private int m_cameraId= 0; // Default camera
	private volatile boolean m_previewRunning= false;
	private volatile boolean m_recording= false;
	private volatile boolean m_isSurfaceCreated= false;
	private int m_width= 352, m_height= 288, m_frameRate= 15;
	private byte m_callbackBuffer[]= null;
	private CameraThread m_CameraThread= null;
	private Lock m_lock= new ReentrantLock();

	public VideoRecorder(Context context)
	{
		super(context);

		Log.d(LOG_TAG, "Creating 'VideoRecorder'...");

		m_context= context;
		m_parentActivity= (MainActivity)context;
		m_holder= this.getHolder();
		m_holder.addCallback(this);
		m_holder.setType(SurfaceHolder.SURFACE_TYPE_PUSH_BUFFERS);

        /* Add VideoRecorder layout (size 1x1~ hidden layout) */
        RelativeLayout.LayoutParams layoutParams= new RelativeLayout.LayoutParams(
        		LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT);
        layoutParams.width= 1; layoutParams.height= 1;
        ((MainActivity)m_context).addContentView(this, layoutParams);

		Log.d(LOG_TAG, "'VideoRecorder' successfuly created.");
	}

	/**
	 * Reset SurfaceView parameters and start recording.
	 * @param cameraId Camera id.
	 * @param width Image width.
	 * @param height Image height.
	 * @param rate Camera video frame-rate.
	 */
	public void start(int cameraId, int width, int height, int rate)
	{
		Log.d(LOG_TAG, "Starting video recorder...");
		if(m_recording)
		{
			Log.w(LOG_TAG, "video recorder was already running");
			return;
		}

		m_lock.lock();
		try
		{
			/* Reset preview parameters */
			m_cameraId= cameraId;
			m_width= width;
			m_height= height;
			m_frameRate= rate;
			m_recording= true; //NOTE: set this before calling 'resetSurfaceParameters()'!!
			resetSurfaceParameters();
		}
    	finally
    	{
    		m_lock.unlock();
    	}
	}

	/**
	 * Stop VideoRecording: stop capturing, stop preview and release camera 
	 * resource.
	 */
	public void stop()
	{
		Log.d(LOG_TAG, "Stopping video recorder...");
		m_recording= false; // do not lock
		m_previewRunning= false;  // do not lock
		
		m_lock.lock();
		try
		{
			if(m_CameraThread!= null)
			{
				m_CameraThread.close();
				m_CameraThread= null;
				m_camera= null;
			}
		}
    	finally
    	{
    		m_lock.unlock();
    	}
	}

	/**
	 * Set VideoRecording preview layout characteristics
	 */
	public void localPreviewDisplay(final boolean display, 
			final int width, final int height, final int x, final int y)
	{
		Log.d(LOG_TAG, "Executing 'localPreviewDisplay()' - " + display + 
				"/" + width + "x" + height + " - " + x + "x" + y);
		m_lock.lock();
		try
		{
			DisplayCtrl.changeDisplayParams(this, m_parentActivity, 
					display, width, height, x, y);
		}
    	finally
    	{
    		m_lock.unlock();
    	}
	}

	/**
	 * Open/release camera. This method is intended to open camera at main 
	 * activity to get default supported setting (as opening camera is a 
	 * heavy process, it should be avoided to be performed several times 
	 * to test each supported parameter). 'takeCamera(false)' should be 
	 * called when all default parameters are set, and always before starting
	 * video recording.
	 * @param take boolean indicating open/release camera.
	 * @param The hardware camera to access, between 0 and 
	 * getNumberOfCameras()-1.
	 */
	public void takeCmaera(boolean take, int cameraId)
	{
		Log.d(LOG_TAG, "Executing 'takeCmaera("+ take+ ", "+ cameraId+ ")'...");
		m_lock.lock();
		try
		{
			if(take)
			{
				Camera camera= null;
				try
				{
					camera= Camera.open(cameraId);
					if(camera!= null) m_camera= camera;
				}
				catch(Exception e)
				{
					Log.d(LOG_TAG, "Camera seems to be locked by other " +
							"application");
				}
			}
			else
			{
				if(m_camera!= null)
				{
					m_camera.release();
					m_camera= null;
				}
			}
		}
    	finally
    	{
    		m_lock.unlock();
    	}
	}

	/**
	 * Check if VideoRecorder is recording.
	 * @return Boolean indicating if VideoRecorder is currently recording.
	 */
	public boolean isRecording()
	{
		boolean isRecording= (m_recording && m_previewRunning);
		Log.d(LOG_TAG, "isRecording() returns: "+ isRecording);
		return isRecording;
	}

	/**
	 * Check if VideoRecorder has its surface created
	 * @return Boolean indicating if surface is already created
	 */
	public boolean isSurfaceCreated()
	{
		return m_isSurfaceCreated;
	}

	@Override
	public void surfaceCreated(SurfaceHolder holder)
	{
		Log.v(LOG_TAG, "Surface Created");
		m_isSurfaceCreated= true;
	}

	@Override
	public void surfaceChanged(SurfaceHolder holder, int format, int width, int height)
	{
		Log.v(LOG_TAG, "Surface Changed: width " + width + " height: " + height);
		// do nothing!
	}

	@Override
	public void surfaceDestroyed(SurfaceHolder holder)
	{
		Log.v(LOG_TAG, "Surface destroyed");
		m_previewRunning= false;
		if(m_CameraThread!= null)
		{
			m_CameraThread.close();
			m_CameraThread= null;
		}
		m_isSurfaceCreated= false;
	}

	@Override
	public void onPreviewFrame(byte[] data, Camera camera)
	{
		//Log.v(LOG_TAG, "'onPreviewFrame()' called on thread: " + Thread.currentThread().getName()); //comment-me
		if(m_previewRunning && m_recording)
		{
			/* Pass camera frame to JNI */
			/*int retcode=*/ encodeVFrame(data);
			//if(retcode<= 0) // comment-me
			//{
			//	// Error occurred; set last error code in parent/main activity
			//	m_parentActivity.setLastErrorCode(
			//			ErrorCodes.getByCode(retcode)
			//	);
			//}
			m_camera.addCallbackBuffer(m_callbackBuffer);
		}
	}

	/*
	 * Private method: Stop and release camera if being used. Re-opens camera
	 * (note that the camera released could be a different one to the new 
	 * camera opened; this is signaled by the 'camera id.' parameter), 
	 * set new parameters and finally restart preview.
	 */
	private void resetSurfaceParameters()
	{
		boolean recordingStateBkp= m_recording;

		/* Stop preview and release camera */
		if(m_previewRunning)
		{
			m_previewRunning= false;
			if(m_CameraThread!= null)
			{
				m_CameraThread.close();
				m_CameraThread= null;
			}
		}

		/* Open camera with new id. */
	    if(m_CameraThread== null)
	    {
	    	m_CameraThread= new CameraThread(m_context, m_cameraId);
	    }
	    synchronized(m_CameraThread)
	    {
	    	m_camera= m_CameraThread.open();
	    	if(m_camera== null)
	    	{
	    		this.stop();
	    		return;
	    	}
	    }

		/* Set preview properties */
		m_callbackBuffer= new byte[m_width*m_height*2];
		m_camera.addCallbackBuffer(m_callbackBuffer);
		m_camera.setPreviewCallbackWithBuffer(this); //setPreviewCallback(this);
		// IMPORTANT NOTE: We do not use 'setPreviewCallback' as it over-uses heap and cause
		// garbage collector intensive work halting CPU.
		try
		{
			m_camera.setPreviewDisplay(m_holder);
		}
		catch(Exception e)
		{
			Log.e(LOG_TAG, "Could not set preview display");
			m_parentActivity.setLastErrorCode(ErrorCodes.SET_PREVIEW_DISPLAY_ERROR);
			this.stop();
			return;
		}
		//m_camera.setDisplayOrientation(90); // keep in landscape

		/* Set preview parameters */
		Camera.Parameters currentParams= m_camera.getParameters();
		try
		{
			if(currentParams== null)
			{
				Log.e(LOG_TAG, "Could not get current camera parameters");
				m_parentActivity.setLastErrorCode(ErrorCodes.SET_CAMERA_PARAMS_ERROR);
				this.stop();
				return;
			}
			currentParams.setPreviewSize(m_width, m_height);
			int [] fps_range= this.getNearestSupportedFps(m_cameraId, m_frameRate);
			if(!(fps_range[Parameters.PREVIEW_FPS_MAX_INDEX]== 0 &&
					fps_range[Parameters.PREVIEW_FPS_MIN_INDEX]== 0) )
			{
				currentParams.setPreviewFpsRange(
						fps_range[Parameters.PREVIEW_FPS_MIN_INDEX], 
						fps_range[Parameters.PREVIEW_FPS_MAX_INDEX]);
			}
			currentParams.setPreviewFormat(android.graphics.ImageFormat.YV12);
			//currentParams.set("orientation", "portrait"); // keep in landscape
			//currentParams.setWhiteBalance
			//currentParams.setSceneMode
			//currentParams.setExposureCompensation
			try
			{
				if (currentParams.getSupportedFocusModes() != null &&
						currentParams.getSupportedFocusModes().contains(Camera.Parameters.FOCUS_MODE_CONTINUOUS_VIDEO)) {
					currentParams.setFocusMode(Camera.Parameters.FOCUS_MODE_CONTINUOUS_VIDEO);
				}
			}
			catch (Exception e)
			{
				Log.v(LOG_TAG, "Cannot set focus mode");
			}
			try
			{
			    if(currentParams.getSupportedSceneModes()!= null && 
			       currentParams.getSupportedSceneModes().contains(Camera.Parameters.SCENE_MODE_AUTO) )
				{
			    	currentParams.setSceneMode(Camera.Parameters.SCENE_MODE_AUTO);
			        Log.d(LOG_TAG, "scene mode auto");
			    }
			}
			catch(Exception e)
			{
				Log.v(LOG_TAG, "Cannot set scene type");
			}
			m_camera.setParameters(currentParams);
		}
		catch(Exception e)
		{
			Log.e(LOG_TAG, "Wrong parameters exception set for camera");
			m_parentActivity.setLastErrorCode(ErrorCodes.SET_CAMERA_PARAMS_ERROR);
			this.stop();
			return;
		}

		Log.v(LOG_TAG, "**** Preview ****\n" +
				" image width: " + currentParams.getPreviewSize().width + 
				" image height: " + currentParams.getPreviewSize().height +
				" frame-rate: " + m_frameRate
				);

		/* Restart preview */
		m_camera.startPreview();
		m_previewRunning= true;
		m_recording= recordingStateBkp;
	}

    /*
     * API IMPLEMENTATION SUPPORT
     */

	/**
	 * Check if available camera exist in device.
	 * @return Integer value 1 if true, 0 if false
	 */
	public boolean isCameraAvailable(int cameraId)
	{
		boolean isAvailable= false;

		m_lock.lock();
		try
		{
			PackageManager pm= m_context.getPackageManager();
			if(pm.hasSystemFeature(PackageManager.FEATURE_CAMERA))
			{
				Camera camera= null;
				try
				{
					camera= Camera.open(cameraId);
					camera.release();
					camera= null;
					isAvailable= true;
				}
				catch(Exception e)
				{
					Log.d(LOG_TAG, "Camera seems to be locked by other application");
					isAvailable= false; // paranoid set
				}
			}
		}
    	finally
    	{
    		m_lock.unlock();
    	}

		Log.d(LOG_TAG, "isCameraAvailable() returns: "+ isAvailable);
		return isAvailable;
	}

	/**
	 * Test capture from camera on this resolution and return list of 
	 * available resolutions.
	 * @param cameraId The hardware camera to access, between 0 and 
	 * getNumberOfCameras()-1.
	 * @return list of available resolutions, null if cameraId is not available
	 * Note that for an available cameraID, this method will always return 
	 * a list with at least one element. 
	 */
	public List<Camera.Size> getSupportedVideoSizes(int cameraId)
    {
		Camera.Parameters currentParams= null;
		List<Camera.Size> ret_list= null;

		m_lock.lock();
		try
		{
			currentParams= this.getCameraParameters(cameraId);
			if(currentParams!= null)
			{
				ret_list= currentParams.getSupportedVideoSizes();
				if(ret_list== null)
				{
					// This method will always return a list with at 
					// least one element (Android doc.) 
					ret_list= currentParams.getSupportedPreviewSizes();
				}
			}
		}
    	finally
    	{
    		m_lock.unlock();
    	}
		return ret_list; // will return null if cameraId is not availale
    }

	/**
	 * Get preferred or recommended preview size (width and height) in pixels 
	 * for video recording if possible. Else, it returns at least a supported
	 * resolution.
	 * @param cameraId The hardware camera to access, between 0 and 
	 * getNumberOfCameras()-1.
	 * @return recommended preview size, null if cameraId is not available.
	 * Note that for an available cameraID, this method will always return 
	 * a recommended preview size.
	 */
	public Camera.Size getPreferredPreviewSizeForVideo(int cameraId)
	{
		Camera.Size resolution= null;
		Camera.Parameters currentParams= null;

		m_lock.lock();
		try
		{
			if((currentParams= this.getCameraParameters(cameraId))!= null)
			{
				resolution= currentParams.getPreferredPreviewSizeForVideo();
				if(resolution== null)
				{
					// 'getSupportedPreviewSizes()' returns at least one 
					// resolution
					List<Camera.Size> list= 
							currentParams.getSupportedPreviewSizes();
					resolution= list.get(0);
				}
			}
		}
    	finally
    	{
    		m_lock.unlock();
    	}
		return resolution;
	}

	/**
	 * Gets the supported preview fps (frame-per-second) ranges.
	 * @param cameraId The hardware camera to access, between 0 and 
	 * getNumberOfCameras()-1.
	 * @return a list of supported preview fps ranges, or null if cameraId 
	 * is not available.
	 * Note that for an available cameraID, this method returns a list with 
	 * at least one element.
	 * (every element is an int array of two values - minimum fps and 
	 * maximum fps-; the list is sorted from small to large -first by 
	 * maximum fps and then minimum fps-).
	 */
	public List<int[]> getSupportedPreviewFpsRange(int cameraId)
	{
		List<int[]> range_list= null;

		m_lock.lock();
		try
		{
			range_list= this.getSupportedPreviewFpsRange2(cameraId);
		}
    	finally
    	{
    		m_lock.unlock();
    	}
		return range_list;
	}
	private List<int[]> getSupportedPreviewFpsRange2(int cameraId)
	{
		Camera.Parameters currentParams= null;
		List<int[]> range_list= null;

		if((currentParams= this.getCameraParameters(cameraId))!= null)
		{
			range_list= currentParams.getSupportedPreviewFpsRange();

			//{ //RAL: HACK for stupid buggy emulators! //FIXME
	        for(int i= 0; i< range_list.size(); i++)
	        {
	        	int fps_max= range_list.get(i)[Parameters.PREVIEW_FPS_MAX_INDEX], 
	        		fps_min= range_list.get(i)[Parameters.PREVIEW_FPS_MIN_INDEX];
	    		Log.v(LOG_TAG, "'getSupportedPreviewFpsRange()' ret["+ i+ "]= '{" +
	    				fps_min+ ", "+ fps_max+ "}");
	    		if(fps_max<1000 || fps_min<1000)
	    		{
	    			int[] dummy_object= {fps_min*1000, fps_max*1000};
	    			range_list.set(i, dummy_object);
	    		}
	        }
	        //} // EndOfHack
		}
		return range_list;
	}

	/**
	 * Return the nearest supported range of frames-per-second (fps) given a 
	 * reference value, for a specified camera device. If the reference 'fps' 
	 * value is not within any supported range, the nearest range is returned.
	 * @param cameraId The hardware camera to access, between 0 and 
	 * getNumberOfCameras()-1.
	 * @param objective_fps Reference FPS value
	 * @return Array of two integers representing the nearest 'fps' range, 
	 * or '{0, 0}' if cameraId is not available. Note that for an available 
	 * cameraID, this method returns always a valid range.
	 */
	public int[] getNearestSupportedFps(int cameraId, int objective_fps)
	{
		int ret[];
		m_lock.lock();
		try
		{
			ret= this.getNearestSupportedFps2(cameraId, objective_fps*1000);
		}
    	finally
    	{
    		m_lock.unlock();
    	}
		return ret;
	}
	private int[] getNearestSupportedFps2(int cameraId, int objective_fps)
	{
		int ret[]= {0, 0};
		int diff= Integer.MAX_VALUE, ret_idx= 0;

		List<int[]> range_list= this.getSupportedPreviewFpsRange(cameraId);
		if(range_list== null)
		{
			Log.e(LOG_TAG, "Unavailable 'cameraId' passed to method " +
					"'getNearestSupportedFps()");
			return ret;
		}

        for(int i= 0; i< range_list.size(); i++)
        {
        	int fps_max= range_list.get(i)[Parameters.PREVIEW_FPS_MAX_INDEX], 
        		fps_min= range_list.get(i)[Parameters.PREVIEW_FPS_MIN_INDEX];
    		Log.v(LOG_TAG, "'getNearestSupportedFps()' ret["+ i+ "]= '{" +
    				fps_min+ ", "+ fps_max+ "}'; fps-objective= "+ 
    				objective_fps);

        	if((objective_fps<= fps_max) && (objective_fps>= fps_min))
        	{
        		Log.d(LOG_TAG, "'getNearestSupportedFps()' returns '{" +
        				fps_min+ ", "+ fps_max+ "}'");
        		return range_list.get(i);
        	}
        	else
        	{
        		int curr_diff= Math.min(Math.abs(objective_fps- fps_max), Math.abs(objective_fps- fps_min));
        		if(curr_diff< diff)
        		{
        			diff= curr_diff;
        			ret_idx= i;
        		}
        	}
        }
		Log.d(LOG_TAG, "'getNearestSupportedFps()' returns '{" +
				range_list.get(ret_idx)[Parameters.PREVIEW_FPS_MIN_INDEX]+ ", "+ 
				range_list.get(ret_idx)[Parameters.PREVIEW_FPS_MAX_INDEX]+ "}'");
		return range_list.get(ret_idx);
	}

	/**
	 * Access a particular hardware camera and get its parameters.
	 * @param cameraId (camera identifier)
	 * @return camera parameters (android.hardware.Camera.Parameters Class) 
	 * on success, null on error.
	 */
	private Camera.Parameters getCameraParameters(int cameraId)
	{
		Camera.Parameters currentParams= null;

		if(this.m_camera!= null)
		{
			currentParams= m_camera.getParameters();
			Log.v(LOG_TAG, "Camera is available and already open by this app.;" +
					", getting parameters...");
		}
		else
		{
			Camera camera= null;
			try
			{
				camera= Camera.open(cameraId);
				Log.v(LOG_TAG, "Camera is available, getting parameters...");
				currentParams= camera.getParameters();
				camera.release();
				camera= null;
			}
			catch(Exception e)
			{
				Log.e(LOG_TAG, "Could not open camera (id: "+ cameraId+ ")");
			}
		}
		return currentParams;
	}

    /* Private native methods implemented by native library packaged
     * with this application.
     */
    private native int encodeVFrame(byte[] buffer);
}
