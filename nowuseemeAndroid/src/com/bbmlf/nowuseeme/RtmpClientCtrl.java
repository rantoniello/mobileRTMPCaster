package com.bbmlf.nowuseeme;

import java.io.InputStream;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;

import com.google.zxing.integration.android.IntentIntegrator;

import android.content.Context;
import android.content.SharedPreferences;
//import android.content.SharedPreferences; //FIXME!!
import android.content.res.AssetManager;
import android.hardware.Camera;
import android.hardware.Camera.CameraInfo;
import android.hardware.Camera.Parameters;
import android.util.Log;

/**
 * class RtmpClientCtrl.
 */
public class RtmpClientCtrl
{
	/* General private attributes */
	private final static String LOG_TAG= "RtmpClientCtrl";
	private Context m_context= null;
	private MainActivity m_parentActivity;
	private RtmpClientCtrl m_this= this;
	private Lock m_lock= new ReentrantLock();
    private boolean m_inBackground= false;
    private DefaultLocalSettings m_defaultLocalSettings= null;

    /* Streaming multiplex related attributes */
    private boolean m_isConnected= false;

	/* Video coding related attributes */	
	private VideoRecorder m_vrecorder= null;
    private int m_cameraId= -1; // Default value
    private int m_v_width= 640;
    private int m_v_height= 360;
    private int m_v_frame_rate= 30;
    private int m_v_gop_size= 30;
    private int m_v_bit_rate= 256*1024;
    private int m_v_rc= -1; // "rate-control"; -1 or 0 means "no control"
    private String m_v_codec_id= "h264";
    private List<String> m_v_codecs_list= Arrays.asList("h264", "flv1");

	/* Audio coding related attributes */
    private AudioRecorder m_arecorder= null;
    private int m_a_bit_rate= 64000;
    private int m_a_sample_rate= 22050; //[Hz]
    private String m_a_codec_id= "aac";
    private List<String> m_a_codecs_list= Arrays.asList("aac", "mp3");
    private boolean m_a_mute_on= false;

    /* Streaming multiplex related */
	private String m_streaming_url= "";
	private int m_autoReconnectTimeOut= 5; // [seconds]
	private String m_av_quality= "q2"; // possible values: q1-q2-q3
	// Following object (supported range of qualities) can be extended/modified
	private String m_av_supported_q[]= {"q1", "q2", "q3"};

	/* Scanner related */
	private boolean m_onScannerActivityResult= false;

	/**
	 * 'RtmpClientCtrl' class constructor
	 */
	public RtmpClientCtrl(Context context)
	{
		m_context= context;
		m_parentActivity= (MainActivity)context;
	}

	/**
	 * Open 'RtmpClientCtrl' class.
	 */
	public void open()
	{
		m_lock.lock();

        /* Save Main-Activity environment and object handlers into globals
         * variables of JNI level 
         */
        setJavaObj();

        /* Initialize native rtmp-client context */
        rtmpclientOpen();

        /* Create A/V recorders
         * (including video recorder surface)
         */
        m_vrecorder= new VideoRecorder(m_context);
        m_arecorder= new AudioRecorder(m_context);

        /* Set an initial working settings (e.g. video camera, resolutions, etc.) 
         * 'VideoRecorder' and 'AudioRecorder' classes must be already 
         * instantiated.
         */
		// Get user preferences
		SharedPreferences preferences= m_context.getSharedPreferences("nowuseeme", MainActivity.MODE_PRIVATE);
        if(preferences!= null)
		{
			m_streaming_url= preferences.getString("defaultServerURL", null);
			m_cameraId= preferences.getInt("defaultCameraID", -1);
        }

		// Get default working settings for this specific device
        m_defaultLocalSettings= new DefaultLocalSettings(m_parentActivity, m_vrecorder, m_arecorder);

        // Select preferred/default camera
        if((m_cameraId= m_defaultLocalSettings.cameraSelectDefault(m_cameraId))< 0)
        {
    		Log.e(LOG_TAG,"Error: No cameras available!");
    		m_parentActivity.setLastErrorCode(ErrorCodes.OPEN_CAMERA_ERROR);
        }

        // Take camera for setting the rest of default parameters
        m_vrecorder.takeCmaera(true, m_cameraId);

        // Select initial working resolution
        this.selectValidResolution();

        // Select initial valid fps value
        this.selectValidFPS();

        // Release camera when all video-default parameters were set
        m_vrecorder.takeCmaera(false, m_cameraId);

        // Select initial working audio-sampling-rate value
        m_a_sample_rate= m_defaultLocalSettings.audioSampleRateSelectDefault();
        if(m_a_sample_rate== -1) m_a_sample_rate= 44100;

		/* Immediately publish default connection status in initialization */
		m_parentActivity.onConnectStatusChanged(m_isConnected);

        m_lock.unlock();
	}

	/**
	 * Close 'RtmpClientCtrl' class.
	 */
	public void close()
	{
		m_lock.lock();

		/* De-initialize native rtmp-client context */
    	rtmpclientClose();

    	/* Release global references at JNI level */
    	releaseJavaObj();

        m_lock.unlock();
	}

	/**
	 * Pause 'RtmpClientCtrl' class
	 */
	public void pause()
	{
		m_lock.lock();

        /* Run rtmp-client in background mode */
        if(!m_inBackground && m_isConnected)
        {
        	Log.d(LOG_TAG, "Rtmp-client running in background mode");
			int retcode= runBackground();
			if(retcode<= 0)
			{
				// Set last error code in parent/main activity
				m_parentActivity.setLastErrorCode(ErrorCodes.getByCode(retcode) );
			}
        	m_inBackground= true;
        }

        m_lock.unlock();
	}

	/**
	 * Resume 'RtmpClientCtrl' class
	 */
	public void resume()
	{
		m_lock.lock();

		/* Restore rtmp-client from background mode */
        if(m_inBackground)
        {
			int retcode= stopBackground();
			if(retcode<= 0)
			{
				// Set last error code in parent/main activity
				m_parentActivity.setLastErrorCode(ErrorCodes.getByCode(retcode) );
			}
        	m_inBackground= false;

        	// If running, we need to restart encoders
        	if(m_vrecorder.isRecording() || m_arecorder.isRecording() )
        	{
        		this.stop();
        		this.start();
        	}
        }

        /* Resuming from scanner activity */
        if(m_onScannerActivityResult)
        {
        	Thread thread= new Thread()
        	{
        		@Override
        		public void run()
        		{
        			/* Hack (have to code this better):
        			 * Make sure surface is created before starting managing camera
        			 * */
        	        try
        	        {
        	        	int count= 0, max_count= 10;
        	            while(m_vrecorder!= null && !m_vrecorder.isSurfaceCreated() 
        	            		&& count< max_count)
        	            {
        	            	Log.v(LOG_TAG, "testing sleep interrumption'...");
        	                Thread.sleep(500); // sleep 500 milliseconds ...
        	                count++;
        	            }
        	            if(count>= max_count)
        	            {
        	            	Log.e(LOG_TAG, "Could not create video recorder surface view");
        	            	return; // exit run()
        	            }
        	        }
        	        catch(InterruptedException e)
        	        {
        	        	Log.e(LOG_TAG, "Could not start video recorder");
        	        	return; // exit run()
        	        }

        	        /* Start, on resume, rtmp-client */
        			m_onScannerActivityResult= false;
        			m_this.start();
        		} // run()
        	};
        	thread.start();
        }

        m_lock.unlock();
	}

	/**
	 * Start 'RtmpClientCtrl' class.
     * A 'Json' formated String is passed to native RTMP-Client initialization 
     * method specifying initialization parameters for A/V encoding and muxing.
     * A/V capturing threads are started. 
	 */
	public void start()
	{
    	Thread thread= new Thread()
    	{
    		@Override
    		public void run()
    		{
    			m_lock.lock();
    			start2();
    			m_isConnected= true;
    			m_parentActivity.onConnectStatusChanged(true);
    			m_lock.unlock();
    		}
    	};
    	thread.start();
	}
	private void start2() // auxiliary method (do not use locks)
	{
    	/* Initialize native RTMP client before start recording A/V */
    	String cmd=
	    	"["+
	    	"{\"tag\":\"streaming_url\",\"type\":\"String\",\"val\":\""+m_streaming_url+"\"}"+
	    	"{\"tag\":\"v_bit_rate\",\"type\":\"int\",\"val\":\""+m_v_bit_rate+"\"}"+
	    	"{\"tag\":\"v_rc\",\"type\":\"int\",\"val\":\""+m_v_rc+"\"}"+
	    	"{\"tag\":\"v_frame_rate\",\"type\":\"int\",\"val\":\""+m_v_frame_rate+"\"}"+
	    	"{\"tag\":\"v_width\",\"type\":\"int\",\"val\":\""+m_v_width+"\"}"+
	    	"{\"tag\":\"v_height\",\"type\":\"int\",\"val\":\""+m_v_height+"\"}"+
	    	"{\"tag\":\"v_gop_size\",\"type\":\"int\",\"val\":\""+m_v_gop_size+"\"}"+
	    	"{\"tag\":\"v_codec_id\",\"type\":\"String\",\"val\":\""+m_v_codec_id+"\"}"+
	    	"{\"tag\":\"a_bit_rate\",\"type\":\"int\",\"val\":\""+m_a_bit_rate+"\"}"+
	    	"{\"tag\":\"a_sample_rate\",\"type\":\"int\",\"val\":\""+m_a_sample_rate+"\"}"+
	    	"{\"tag\":\"a_codec_id\",\"type\":\"String\",\"val\":\""+m_a_codec_id+"\"}"+
	    	"{\"tag\":\"av_quality\",\"type\":\"String\",\"val\":\""+m_av_quality+"\"}"+
	    	"]";
    	int retcode= rtmpclientInit(cmd.getBytes(), 1/*m_vrecorder.isAvailable()*/, 1/*m_arecorder.isAvailable()*/ );
    	if(retcode<= 0)
    	{
    		m_parentActivity.setLastErrorCode(ErrorCodes.getByCode(retcode) );
    	}
    	if(m_parentActivity.getLatestErrorCode()!= ErrorCodes.SUCCESS.getCode() )
    	{
    		Log.e(LOG_TAG,"'rtmpclientInit()' failed: ");
    		return;
    	}

    	/* Set background still-image */
    	// (will be shown when Main-Activity goes background)
    	loadInBackgroundImage();
    	if(m_parentActivity.getLatestErrorCode()!= ErrorCodes.SUCCESS.getCode() )
    	{
    		Log.e(LOG_TAG,"'loadInBackgroundImage()' failed");
    		return;
    	}

		/* Start A/V recorders (capturing from camera/microphone) */
		if(m_arecorder!= null)
			m_arecorder.start(m_a_sample_rate, m_a_mute_on);
		if(m_vrecorder!= null)
			m_vrecorder.start(m_cameraId, m_v_width, m_v_height, m_v_frame_rate);
	}

	/**
	 * Stop 'RtmpClientCtrl' class.
     * A/V capturing threads are stopped/destroyed.
     * JNI RTMP-Client is released. 
	 */
	public void stop()
	{
		m_lock.lock();

    	Log.v(LOG_TAG,"Finishing recording, calling stop and releaseing");

    	/* Stop A/V recorders */
		if(m_arecorder!= null) m_arecorder.stop();
		if(m_vrecorder!= null) m_vrecorder.stop();

		/* Release rtmp-client dynamically allocated resources */
		rtmpclientRelease();

		if(m_isConnected)
		{
			m_isConnected= false;
			m_parentActivity.onConnectStatusChanged(false);
		}

    	Log.v(LOG_TAG,"'stopRecording()' successfuly executed");

		m_lock.unlock();
	}

	/**
	 * JNI to Java bridge on-error callback
	 * @param errorCode Integer indicating error code
	 */
	private void onError(int errorCode)
	{
		Log.v(LOG_TAG,"'onError()'...");

		/* Set error code */
		m_parentActivity.setLastErrorCode(ErrorCodes.getByCode(errorCode) );

		/* Stop/disconnect RTMP server */
		this.stop();

		/* Auto-reconnect if apply */
		if(m_autoReconnectTimeOut>= 0)
		{
			this.start();
		}
	}

	//{ //Bogdan
	public boolean isRecording()
	{
		return (m_vrecorder != null &&
				m_vrecorder.isRecording()) ||
			   (m_arecorder != null &&
			    m_arecorder.isRecording());
	}
	//} //Bogdan

    /****************************************************************
     ***************** API IMPLEMENTATION ***************************
     ****************************************************************/

    /*==== MUX SETTINGS ====*/

    /**
     * Connect to RTMP server.
     * Note that for disconnection, stop() method applies.
     * @param url
     */
    public void connect(String url)
    {
    	Log.w(LOG_TAG, "Conecting to URL: "+ url);

    	/* Check if it is already connected and running */
    	if(m_vrecorder.isRecording() || m_arecorder.isRecording() )
    	{
    		Log.w(LOG_TAG, "... already connected to URL: "+ m_streaming_url);
    		return;
    	}

    	/* Check if the demanded camera is available on this device */
    	if(!m_vrecorder.isCameraAvailable(m_cameraId) )
    	{
    		Log.e(LOG_TAG,"Error: Demanded camera not available");
    		m_parentActivity.setLastErrorCode(ErrorCodes.OPEN_CAMERA_ERROR);
    		return;
    	}

    	/* Check and set given URL */
    	if(url== null)
    	{
    		Log.e(LOG_TAG,"Error: connection URL should not be NULL");
    		m_parentActivity.setLastErrorCode(ErrorCodes.OPEN_CONNECTION_OR_OFILE_ERRROR);
    		m_onScannerActivityResult= false; // reset boolean
    		return;
    	}
    	else if(!url.isEmpty() )
    	{
    		m_streaming_url= url;
    	}
    	else if(m_parentActivity.m_debugMethods)
    	{//RAL: debugging purposes (define 'm_debugMethods= false' for release version)
    		m_streaming_url= "rtmp://10.0.0.103:1935/myapp/mystream";
    	}
    	else if(m_streaming_url.isEmpty() )
    	{
    		/* NOTE: When scanner app. is launched, this app. goes background.
    		 * For that reason, method 'start()' will be applied in 'resume()' 
    		 * instead, and we must take special care for 'VideRecorder' 
    		 * surface to be previously created. Refer to 'resume()' method
    		 * for the rest of the code.
    		 */
			m_parentActivity.setLastErrorCode(ErrorCodes.OPEN_CONNECTION_OR_OFILE_ERRROR);
			m_onScannerActivityResult= false; // reset boolean
//    		m_onScannerActivityResult= true;
//    		barcodeScan();
    	}

		// Set some user configuration preferences on start
    	if(m_parentActivity.getAppModeManager().getStartMode()== AppModeManager.LAUNCHER_MODE)
		{
			SharedPreferences preferences= m_context.getSharedPreferences("nowuseeme", Context.MODE_PRIVATE);
			SharedPreferences.Editor edit= preferences.edit();
			edit.putString("defaultServerURL", m_streaming_url);
			edit.putInt("defaultCameraID", m_cameraId);
			edit.commit();
		}

        if(m_onScannerActivityResult== false)
        {
	        this.start();
        }
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

    public String videoServerURLGet()
    {
    	return m_streaming_url;
    }

    /*==== VIDEO SETTINGS ====*/

    // get current camera width
    public int videoResolutionWidthGet()
    {
    	return this.m_v_width;
    }

    //get current camera height
    public int videoResolutionHeightGet()
    {
    	return this.m_v_height;
    }

    //set current resolution
	public boolean videoResolutionSet(int width, int height)
    {
    	Log.d(LOG_TAG, "'videoResolutionSet()'; width: "+ width+ " height: "+ height);

		// change resolution
		m_v_width= width;
		m_v_height= height;

    	// If running, we need to restart encoder to change resolution
    	if(m_vrecorder.isRecording() || m_arecorder.isRecording() )
    	{
    		this.stop();
    		this.start();
    	}

		// Check resolution changed
		if(videoResolutionWidthGet()== width && 
				videoResolutionHeightGet()== height)
		{
			return true; // set resolution OK
		}
		return false; // set resolution KO
    }

	// test capture from camera on this resolution and return closest resolution available
	public String videoResolutionTest(int width, int height, int keepRatio)
    {
		List<Camera.Size> camera_sizes= m_vrecorder.getSupportedVideoSizes(m_cameraId);
		
        // Iterate List to find the closest resolution available; 
		// we use area as verisimilitude criterion
		int area= width*height;
		float ratio= (float)width/(float)height;
		int idx_closest= 0; // index of 'closest' element in List
		int area_closest= 0;
        for(int i= 0; i< camera_sizes.size(); i++)
        {
        	Camera.Size objsize= camera_sizes.get(i);
        	Log.d(LOG_TAG, "Video resolution list ["+ i+ "]: "+ objsize.width+ "x"+ objsize.height);

        	// If resolution matches, is done!
        	if((objsize.width== width) && (objsize.height==height))
        	{
        		return String.format("{%dx%d}", objsize.width, objsize.height);
        	}
        	
        	// If *MUST* keep ratio, we skip non-matching ratios
        	float ratio_curr= (float)objsize.width/(float)objsize.height;
        	if((keepRatio!= 0)&&(ratio_curr!=ratio))
        	{
        		Log.d(LOG_TAG, "Current resolution does not respect ratio: "+ ratio_curr);
        		continue;
        	}

        	// Measure verisimilitude for current resolution
        	int area_curr= objsize.width*objsize.height;
        	if(
        	   (i== 0) ||
        	   (Math.abs(area_curr-area)<= Math.abs(area_closest-area))
        	)
        	{
        		area_closest= area_curr;
        		idx_closest= i;
        	}
        }
        // Return closest resolution found
        if(area_closest!= 0)
        {
        	Camera.Size objsize= camera_sizes.get(idx_closest);
        	return String.format("{%dx%d}", objsize.width, objsize.height);
        }
        Log.w(LOG_TAG, "Non resolution found for specified criterion");
        return "{}";
    }

	// get list of supported resolutions
	public String videoResolutionsGet()
    {
		List<Camera.Size> camera_sizes= m_vrecorder.getSupportedVideoSizes(m_cameraId);
		if(camera_sizes== null)
		{
			return "{}";
		}
		String s= "[{";
        for(int i= 0; i< camera_sizes.size(); i++)
        {
        	Camera.Size objsize= camera_sizes.get(i);
        	if(i!= 0) s= s.concat("},{");
        	s= s.concat(String.format("\"w\": \"%d\",\"h\": \"%d\"", objsize.width, objsize.height));
        }
        s= s.concat("}]");
        Log.d(LOG_TAG, "'videoResolutionsGet()': "+ s);
        return s;
    }

	// get list of supported fps ranges
	public String videoFPSRangesGet()
	{
		List<int[]> range_list= null;
		range_list= m_vrecorder.getSupportedPreviewFpsRange(m_cameraId);
		if(range_list== null)
		{
			return "[]";
		}
		String s= "[{";
		for(int i= 0; i< range_list.size(); i++)
		{
			if(i!= 0) s= s.concat("},{");
			int fps_max= range_list.get(i)[Parameters.PREVIEW_FPS_MAX_INDEX], 
				fps_min= range_list.get(i)[Parameters.PREVIEW_FPS_MIN_INDEX];
			//{ //RAL: HACK for stupid buggy emulators! // HACK
			if(fps_max<1000 || fps_min<1000)
			{
				int[] dummy_object= {fps_min*1000, fps_max*1000};
				range_list.set(i, dummy_object);
			}
			//} // EndOfHack
			s= s.concat(String.format("\"min\": \"%d\",\"max\": \"%d\"", fps_min, fps_max));
		}
        s= s.concat("}]");
        Log.d(LOG_TAG, "'videoFPSRangesGet()': "+ s);
        return s;
    }

    // get FPS
    public int videoFPSGet()
    {
    	return this.m_v_frame_rate;
    }

    // set flash.media.Camera.setMode.fps
    public boolean videoFPSSet(int fps)
    {
    	m_v_frame_rate= fps;

    	// set the nearest working fps
    	this.selectValidFPS();

    	// If running, we need to restart encoder to change resolution
    	if(m_vrecorder.isRecording() || m_arecorder.isRecording() )
    	{
    		this.stop();
    		this.start();
    	}

		// Check resolution changed
		if(videoFPSGet()== fps)
		{
			return true; // set fps OK
		}
		return false; // set fps KO
    }

    // set flash.media.Camera.setKeyFrameInterval
    public boolean videoKeyFrameIntervalSet(int keyFrameInterval)
    {
    	m_v_gop_size= keyFrameInterval;

    	// If running, we need to restart encoder to change resolution
    	if(m_vrecorder.isRecording() || m_arecorder.isRecording() )
    	{
    		this.stop();
    		this.start();
    	}
    	return true;
    }

    // get flash.media.Camera.setKeyFrameInterval
    public int videoKeyFrameIntervalGet()
    {
    	return m_v_gop_size;
    }

    //get codec name
    public String videoCodecNameGet()
    {
    	return this.m_v_codec_id;
    }

    // get list of compatible codecs
    public String videoCodecsGet()
    {
		String s= "[\"";
        for(int i= 0; i< m_v_codecs_list.size(); i++)
        {
        	if(i!= 0) s= s.concat("\",\"");
        	s= s.concat(m_v_codecs_list.get(i));
        }
        s= s.concat("\"]");
        Log.d(LOG_TAG, "'videoCodecsGet()': "+ s);
        return s;
    }

    // set codec
    public boolean videoCodecSet(String codec)
    {
    	// Check if given codec is supported
        for(int i= 0; i< m_v_codecs_list.size(); i++)
        {
        	if(m_v_codecs_list.get(i).equals(codec) )
        	{
        		this.m_v_codec_id= m_v_codecs_list.get(i);

            	// If running, we need to restart encoder to change resolution
            	if(m_vrecorder.isRecording() || m_arecorder.isRecording() )
            	{
            		this.stop();
            		this.start();
            	}
        		return true;
        	}
        }
    	return false;
    }

    // get/set flash.media.Camera.bandwidth
    public boolean videoBandwidthSet(int bw)
    {
    	m_v_bit_rate= bw- m_a_bit_rate; // m_a_bit_rate is not configurable

    	// If running, we need to restart encoder to change BW
    	if(m_vrecorder.isRecording() || m_arecorder.isRecording() )
    	{
    		this.stop();
    		this.start();
    	}

		// Check BW changed
		if(videoBandwidthGet()== bw)
		{
			return true; // OK
		}
		return false; // KO
    }

    // get/set flash.media.Camera.bandwidth
    public int videoBandwidthGet()
    {
    	return (this.m_v_bit_rate+ this.m_a_bit_rate);
    }

    // get/set flash.media.Camera.quality
    public boolean videoQualitySet(String quality)
    {
    	boolean ret= false; // K.O. by default

    	for(int i= 0; i< m_av_supported_q.length; i++)
    	{
    		if(m_av_supported_q[i].compareTo(quality)== 0)
    		{
    			// quality specified is supported
    	    	m_av_quality= quality;

    	    	// If running, we need to restart encoder to change quality
    	    	if(m_vrecorder.isRecording() || m_arecorder.isRecording() )
    	    	{
    	    		this.stop();
    	    		this.start();
    	    	}

    			// Check quality changed
    			if(videoQualityGet()== quality)
    			{
    				ret= true;; // OK
    			}
    			break; // break 'for' loop
    		}
    	}
		return ret;
    }

    // get/set flash.media.Camera.quality
    public String videoQualityGet()
    {
    	return this.m_av_quality;
    }

    // get/set rate control tolerance (in % units; -1 o 0 means no control)
    public boolean rateCtrlToleranceSet(int rc)
    {
    	m_v_rc= rc;

    	// If running, we need to restart encoder to change RC
    	if(m_vrecorder.isRecording() || m_arecorder.isRecording() )
    	{
    		this.stop();
    		this.start();
    	}

		// Check RC changed
		if(rateCtrlToleranceGet()== rc)
		{
			return true; // OK
		}
		return false; // KO
    }

    public int rateCtrlToleranceGet()
    {
    	return this.m_v_rc;
    }

    // is it capturing from the camera?
    public boolean isVideoMuted()
    {
    	return this.m_vrecorder.isRecording();
    }

    /*==== AUDIO SETTINGS ====*/

    //get codec name
    public String audioCodecNameGet()
    {
    	return this.m_a_codec_id;
    }

    // get list of compatible codecs
    public String audioCodecsGet()
    {
		String s= "[\"";
        for(int i= 0; i< m_a_codecs_list.size(); i++)
        {
        	if(i!= 0) s= s.concat("\",\"");
        	s= s.concat(m_a_codecs_list.get(i));
        }
        s= s.concat("\"]");
        Log.d(LOG_TAG, "'audioCodecsGet()': "+ s);
        return s;
    }

    // set codec
    public boolean audioCodecSet(String codec)
    {
    	// Check if given codec is supported
        for(int i= 0; i< m_a_codecs_list.size(); i++)
        {
        	if(m_a_codecs_list.get(i).equals(codec) )
        	{
        		this.m_a_codec_id= m_a_codecs_list.get(i);

            	// If running, we need to restart encoder to change resolution
            	if(m_vrecorder.isRecording() || m_arecorder.isRecording() )
            	{
            		this.stop();
            		this.start();
            	}
        		return true;
        	}
        }
    	return false;
    }

    // set/get mute on sender
    public boolean setMuteSender(boolean on)
    {
    	this.m_a_mute_on= on;
    	m_arecorder.setMute(on);
		if(m_arecorder.getMute()== on)
		{
			return true; // OK
		}
		return false; // KO
    }

    public boolean getMuteSender()
    {
    	return this.m_a_mute_on;
    }

    /*==== H/W SETTINGS ====*/

	// get list of connected cameras
	public String videoCamerasGet()
	{
		// Iterate over available cameras and get further details
		String s= "[";
		for(int i= 0; i< Camera.getNumberOfCameras(); i++)
		{
			if(i!= 0) s= s.concat(",");
			s= s.concat(this.videoCameraDetailsGet(i) );
		}
        s= s.concat("]");
        Log.d(LOG_TAG, "'videoCameraDetailsGet()': "+ s);
		return s;
	}

	// get info regarding camera
	public String videoCameraDetailsGet(int cameraId)
	{
		// Check camera Id existence
		if(cameraId>= Camera.getNumberOfCameras() || cameraId< 0)
		{
			return "{}";
		}

		// Get camera info
		CameraInfo cameraInfo= new CameraInfo();
		Camera.getCameraInfo(cameraId, cameraInfo);
		// Construct info string for this camera
		String facing= (cameraInfo.facing== CameraInfo.CAMERA_FACING_BACK)? 
				"back": "front";
		String s= "{\"id\":\""+ cameraId+ "\","+
                  "\"availability\":\""+ m_vrecorder.isCameraAvailable(cameraId)+ "\","+
				  "\"facing\":\""+ facing+ "\","+
				  "\"orientation\":\""+ cameraInfo.orientation+ "\"}";
		Log.d(LOG_TAG, "'videoCameraDetailsGet()': "+ s);
		return s;
	}

	// get current camera ID
	public int videoCameraGet()
	{
		return this.m_cameraId;
	}

	// set current camera
	public boolean videoCameraSet(int cameraID)
	{
		// Check camera Id existence
		if(cameraID>= Camera.getNumberOfCameras() || cameraID< 0)
		{
			m_parentActivity.setLastErrorCode(ErrorCodes.OPEN_CAMERA_ERROR);
			return false;
		}
		if(m_cameraId== cameraID)
		{
			return true; // set fps OK
		}

    	m_cameraId= cameraID;

		// Set an initial working resolution ('m_vrecorder' must be already initialized)
    	this.selectValidResolution();

        // set an initial working fps
        this.selectValidFPS();

    	// If running, we need to restart encoder to change camera
    	if(m_arecorder.isRecording() || m_vrecorder.isRecording() )
    	{
    		this.stop();
    		this.start();
    	}

		// Check camera changed
		if(videoCameraGet()== cameraID)
		{
			return true; // set camera OK
		}
		m_parentActivity.setLastErrorCode(ErrorCodes.OPEN_CAMERA_ERROR);
		return false; // set camera KO
	}

	/*==== Scan Bar ====*/
    public boolean barcodeScan()
    {
    	Log.d(LOG_TAG, "Executing 'barcodeScan()'...");
    	IntentIntegrator integrator= new IntentIntegrator(m_parentActivity);
    	integrator.initiateScan();
    	integrator= null;
    	Log.d(LOG_TAG, "'barcodeScan()' successfuly executed.");
    	return true;
    }

	/*==== Preview window ====*/
	public void localPreviewDisplay(final boolean display, 
			final int width, final int height, final int x, final int y)
	{
		m_vrecorder.localPreviewDisplay(display, width, height, x, y);
	}

	/*
	 * PRIVATE METHODS
	 */

    /**
     * Set background image to be used in background thread.
     * Image provided should be in planar YUV 4:2:0 format.
     */
    // Following parameter is intended to be used for "background still-image"
    // initialization. This feature can be made "dynamic" in the future: 
    // e.g. with a user wizard for choosing a personalized image. (//TODO)
    private int m_backgroundImageSize= (352*288)+((352*288)/2);
    private void loadInBackgroundImage()
    {
    	InputStream inputStream= null;
    	AssetManager am= m_parentActivity.getAssets();
    	byte buffer_background[]= new byte[m_backgroundImageSize];
		try
		{
			inputStream= am.open("NoVideoAvailable.yuv"); // at assets folder
		}
		catch(Exception e)
		{
			Log.e(LOG_TAG, "Could not load background image\n");
			return;
		}

		if(inputStream!= null)
		{
    		try
    		{
    			int bytesRead= inputStream.read(buffer_background, 0, m_backgroundImageSize);
    			if(bytesRead!= m_backgroundImageSize)
    			{
    				Log.e(LOG_TAG, "Error reading background image " +
    						"(read size: "+ bytesRead +")\n");
    			}
    			int retcode= setBackgroundImage(buffer_background);
				if(retcode<= 0)
				{
					// Set last error code in parent/main activity
					m_parentActivity.setLastErrorCode(ErrorCodes.getByCode(retcode) );
				}
    		}
    		catch(Exception e)
    		{
    			Log.e(LOG_TAG, "Could not read background image\n");
    		}
		}
    }

	private void selectValidResolution()
	{
        Camera.Size default_resolution= 
        		m_defaultLocalSettings.resolutionSelectDefault(m_cameraId);
        if(default_resolution!= null)
        {
			m_v_width= default_resolution.width;
			m_v_height= default_resolution.height;
        }
        else
        {
    		Log.e(LOG_TAG,"Error: No cameras available!");
    		m_parentActivity.setLastErrorCode(ErrorCodes.OPEN_CAMERA_ERROR);
        }
	}

	private void selectValidFPS()
	{
        int [] default_fps= m_defaultLocalSettings.fpsSelectDefault
        		(m_cameraId, m_v_frame_rate);
        if(default_fps[Parameters.PREVIEW_FPS_MAX_INDEX]== 0 &&
           default_fps[Parameters.PREVIEW_FPS_MIN_INDEX]== 0)
        {
    		Log.e(LOG_TAG,"Error: No cameras available!");
    		m_parentActivity.setLastErrorCode(ErrorCodes.OPEN_CAMERA_ERROR);
        }
        // Correctly bound fps if is the case
        int fps_max= default_fps[Parameters.PREVIEW_FPS_MAX_INDEX]/1000;
        int fps_min= default_fps[Parameters.PREVIEW_FPS_MIN_INDEX]/1000;
        if(m_v_frame_rate> fps_max)
        	m_v_frame_rate= fps_max;
        else if(m_v_frame_rate< fps_min)
        	m_v_frame_rate= fps_min;
	}

    /* 
     * Private native methods implemented by native library packaged
     * with this application.
     */
	private native void setJavaObj();
	private native void releaseJavaObj();
	private native void rtmpclientOpen();
	private native void rtmpclientClose();
	private native int runBackground();
	private native int stopBackground();
	private native int rtmpclientInit(byte[] buffer, int vavailable, int aavailable);
	private native void rtmpclientRelease();
	private native int setBackgroundImage(byte[] buffer);
	private native void connectionTimeOutSet(int tout);
}
