package com.bbmlf.nowuseeme;

import java.util.List;

import android.content.Context;
import android.hardware.Camera;
import android.hardware.Camera.CameraInfo;
import android.hardware.Camera.Size;
import android.media.AudioFormat;
import android.media.AudioRecord;
import android.media.MediaRecorder.AudioSource;
import android.util.Log;

/**
 * Class defining default supported parameters for initializing application.
 */
public class DefaultLocalSettings
{
	private final static String LOG_TAG= "DefaultLocalSettings";
	MainActivity m_parentActivity= null;
	VideoRecorder m_vrecorder= null;
	AudioRecorder m_arecorder= null;

	public DefaultLocalSettings(Context context, VideoRecorder vrecorder, 
			AudioRecorder arecorder)
	{
		m_parentActivity= (MainActivity)context;
		m_vrecorder= vrecorder;
		m_arecorder= arecorder;
	}

	/**
	 * Select default hardware camera, if available.
	 * @param savedCameraId Suggested camera considering user preferences.
	 * @return Suggested or default camera id, -1 if no hardware camera 
	 * is available. If user suggested camera is not available, facing-front
	 * camera is the priority.
	 */
	public int cameraSelectDefault(int savedCameraId)
	{
		int ret= -1; // not available (default)

		/* Sanity check */
		if(checkInitialization()< 0)
		{
			return ret;
		}
 
		/* Return user preferred camera if exist */
		if(savedCameraId>= 0 && savedCameraId< Camera.getNumberOfCameras() )
		{
			return savedCameraId;
		}

		/* Return "CAMERA_FACING_FRONT" if exist */
		for(int i= 0; i< Camera.getNumberOfCameras(); i++)
		{
			// Get camera info
			CameraInfo cameraInfo= new CameraInfo();
			Camera.getCameraInfo(i, cameraInfo);
			if(cameraInfo.facing== CameraInfo.CAMERA_FACING_FRONT)
			{
				return i;
			}
			else
			{
				ret= i;
			}
		}
		return ret;
	}

	/**
	 * Select default recording resolution for camera, if camera is available.
	 * @param cameraId The hardware camera to access, between 0 and 
	 * getNumberOfCameras()-1.
	 * @return default recording resolution, null if camera given is not 
	 * available.
	 */
	public Size resolutionSelectDefault(int cameraId) // Bogdan
	{
		/* Sanity check */
		if(checkInitialization()< 0)
		{
			return null;
		}

		Size size= this.videoResolutionTest(cameraId, 640, 360, (float)0.2);
		if(size== null)
			size= videoResolutionTest(cameraId, 640, 480, 1);
		if(size== null)
			size= m_vrecorder.getPreferredPreviewSizeForVideo(cameraId);

		if(size!= null)
		{
			Log.v(LOG_TAG, "Selected resolution: "+ size.width+ "x"+ size.height);
		}
		else
		{
			Log.e(LOG_TAG, "Bad camera-id passed to method 'resolutionSelectDefault()'");
		}
		return size;
	}

	private Size videoResolutionTest(int cameraId, int width, int height, float ratioTolerance)
	{
		List<Camera.Size> camera_sizes= m_vrecorder.getSupportedVideoSizes(cameraId);
		
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
        		return objsize;
        	}
        	
        	// If is out of keep ratio tolerance, we skip non-matching ratio
        	float ratio_curr= (float)objsize.width/(float)objsize.height;
			if(ratio_curr< ratio- ratioTolerance || ratio_curr> ratio+ ratioTolerance)
        	{
        		Log.v(LOG_TAG, "Current resolution does not respect ratio: "+ ratio_curr);
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
        	return objsize;
        }
        Log.w(LOG_TAG, "Non resolution found for specified criterion");
        return null;
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
	public int[] fpsSelectDefault(int cameraId, int objective_fps)
	{
		int ret[]= {0, 0};

		/* Sanity check */
		if(checkInitialization()< 0)
		{
			return ret;
		}
		return m_vrecorder.getNearestSupportedFps(cameraId, objective_fps);
	}

	/**
	 * Check if mandatory attributes are initialized
	 * @return Error code 0 if succeed, -1 if fails.
	 */
	private int checkInitialization()
	{
		if(m_parentActivity== null || m_vrecorder== null || m_arecorder== null)
		{
			Log.e(LOG_TAG, "'checkInitialization()' failed");
			return -1;
		}
		return 0;
	}

	/**
	 * Test possible audio-format combinations to be able to support 
     * opening "AudioRecorder" on a wide set of devices/emulators.
	 * @return valid audio sample rate on success, -1 on fail. 
	 */
    public int audioSampleRateSelectDefault()
    {
        for(int rate: new int[] {11025, 8000, 22050, 44100})
        {
            for(short audioFormat: new short[] {AudioFormat.ENCODING_PCM_16BIT /*, AudioFormat.ENCODING_PCM_8BIT*/})
            {
                for(short channelConfig: new short[] {AudioFormat.CHANNEL_IN_MONO /*, AudioFormat.CHANNEL_IN_STEREO*/})
                {
                    try
                    {
                        int bufferSize = AudioRecord.getMinBufferSize(rate, channelConfig, audioFormat);
                        if(bufferSize!= AudioRecord.ERROR_BAD_VALUE)
                        {
                            // check if we can instantiate and have a success
                        	Log.d(LOG_TAG, "Attempting rate "+ rate+ "Hz, bits: "+ audioFormat+ ", channel: "+ channelConfig+ 
                        			", buffer size: " + bufferSize);
                            AudioRecord recorder= new AudioRecord(AudioSource.DEFAULT, rate, channelConfig, audioFormat, bufferSize);

                            if (recorder.getState()== AudioRecord.STATE_INITIALIZED)
                            {
                        		try
                    			{
                        			recorder.stop();
                    			}
                        		catch(IllegalStateException e)
                        		{
                        			Log.e(LOG_TAG,"Could not stop 'AudioRecord' object");
                        		}
                            	recorder.release();
                                return rate;
                            }
                        }
                    }
                    catch (Exception e)
                    {
                        Log.e(LOG_TAG, rate + "Exception, keep trying.",e);
                    }
                }
            }
        }
        return -1;
    }
}
