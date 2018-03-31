package com.bbmlf.nowuseeme;

import java.util.Arrays;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;

import android.content.Context;
import android.media.MediaRecorder;
import android.media.AudioRecord;
import android.media.AudioRecord.OnRecordPositionUpdateListener;
import android.util.Log;

/**
 * class AudioRecorder:
 * This class implements a Runnable object performing operations within an own 
 * Thread. The run() method is in charge of capturing audio samples from 
 * microphone and pass them to JNI RTMP encoding/multiplexing application.
 * This CPU demanding task is performed on an independent thread with high 
 * priority to make sure smooth user experience; it can be executed as a 
 * long-running operations in the background.
 */
class AudioRecorder implements Runnable
{
    /* Private attributes */
    private final static String LOG_TAG= "AudioRecorder";
    private Lock m_lock= new ReentrantLock(); // mutual-exclusion lock for public interface
    private Context m_context= null;
    private Thread m_audioThread= null;
    private AudioRecord m_audioRecord= null;
    private MicrophoneThread m_MicrophoneThread= null;
	private volatile boolean m_recording= false;
    private int m_bufferSizeInBytes, m_bufferSizeInShorts;
    private volatile int m_bufferReadResult;
    private short[] m_audioData= null;
    private int m_sampleRate;
	private boolean m_setMuteOn= false;

	public AudioRecorder(Context context)
	{
		Log.d(LOG_TAG, "Creating AudioRecorder...");
		m_context= context;
	}

	/**
	 * Create new AudioRecorder running thread and starts capturing audio samples.
	 * @param srate Audio sampling rate in Hz.
	 */
	public void start(int srate, boolean setMuteOn)
	{
		Log.d(LOG_TAG, "Starting audio recorder...");
		if(m_recording || m_audioThread!= null)
		{
			Log.w(LOG_TAG, "Could not start thread, already running. " +
					"Please stop recorder before starting it.");
			return;
		}
		m_sampleRate= srate;
		m_setMuteOn= setMuteOn;
		m_recording= true;
		m_audioThread= new Thread(this);
		try
		{
			m_audioThread.start();
		}
		catch(Exception e)
		{
			Log.e(LOG_TAG, "Could not start thread, already running");
		}
	}

	/**
	 * Stop already running AudioRecorder instance: waits AudioRecorder thread 
	 * to end (blocking) and release it.
	 */
	public void stop()
	{
		Log.d(LOG_TAG, "Stopping audio recorder...");
		m_recording= false;
		if(m_audioThread!= null && m_audioThread.isAlive() )
		{
			Log.d(LOG_TAG, "Audio thread is alive; wait join...");
			try
			{
				m_audioThread.join();
			}
			catch(InterruptedException e)
			{
				Log.e(LOG_TAG, "Audio thread join wait was interrupted.");
			}
			Log.d(LOG_TAG, "Audio thread joined.");
		}
		if(m_audioThread!= null) m_audioThread= null;
	}

	/**
	 * Check if AudioRecorder is recording.
	 * @return Boolean indicating if AudioRecorder is currently recording.
	 */
	public boolean isRecording()
	{
		boolean isRecording= (m_recording && 
				m_audioRecord!=null && 
				m_audioRecord.getRecordingState()== AudioRecord.RECORDSTATE_RECORDING);
		Log.d(LOG_TAG, "isRecording() returns: "+ isRecording);
		return isRecording;
	}

	/**
	 * Check if microphone H/W is available
	 * @return Integer value 1 if true, 0 if false
	 */
	public int isAvailable() // Hack
	{
		boolean isAvailable= true;
		MediaRecorder recorder= new MediaRecorder();
		try
		{
			recorder.setAudioSource(MediaRecorder.AudioSource.MIC);
			recorder.setOutputFormat(MediaRecorder.OutputFormat.DEFAULT);
			recorder.setAudioEncoder(MediaRecorder.AudioEncoder.DEFAULT);
			recorder.setOutputFile("/dev/null");
			recorder.prepare();
			recorder.start();
			recorder.stop();
		}
		catch(Exception e)
		{
			isAvailable= false;
		}
		recorder.release();
		Log.d(LOG_TAG, "isAvailable() returns: "+ isAvailable);
		return isAvailable? 1: 0;
	}

	/**
	 * Mute audio recorder
	 * @param on boolean to mute/un-mute
	 */
	public void setMute(boolean on)
	{
		Log.d(LOG_TAG, "Setting microphone Mute: "+ on);

		m_lock.lock();
		try
		{
			m_setMuteOn= on;
		}
    	finally
    	{
    		m_lock.unlock();
    	}
	}

	public boolean getMute()
	{
		boolean on;
		Log.d(LOG_TAG, "Getting microphone Mute... ");
		m_lock.lock();
		try
		{
			on= m_setMuteOn;
		}
    	finally
    	{
    		m_lock.unlock();
    	}
		Log.d(LOG_TAG, "Microphone Mute set to: "+ on);
		return on;
	}

	private OnRecordPositionUpdateListener mListener= new OnRecordPositionUpdateListener()
	{
		public void onPeriodicNotification(AudioRecord recorder)
		{
			//Log.v(LOG_TAG, "OnRecordPositionUpdateListener on thread: " + Thread.currentThread().getName()); //comment-me

        	if(!m_recording) return;

        	m_bufferReadResult= m_audioRecord.read(m_audioData, 0, m_bufferSizeInShorts);
            if(m_bufferReadResult< 0)
            {
            	Log.e(LOG_TAG, "Could not read from the AudioRecord instance");
    			m_recording= false;
            }

            /* Put audio frame into encoder-multiplexer */
            if(m_bufferReadResult> 0)
            {
            	//Log.v(LOG_TAG + " mListener(onPeriodicNotification)", "time is " + System.currentTimeMillis()); //comment-me

            	/* Set mute if apply */
            	if(m_setMuteOn)
            	{
            		if(m_audioData!= null) Arrays.fill(m_audioData, (short)0);
            	}

            	/*int retcode=*/ encodeAFrame(m_audioData, m_bufferReadResult);
    			//if(retcode<= 0) //comment-me
    			//{
    			//	// Error occurred; set last error code in parent/main activity
    			//	((MainActivity)m_context).setLastErrorCode(
    			//			ErrorCodes.getByCode(retcode)
    			//	);
    			//}
            }
        }
        public void onMarkerReached(AudioRecord recorder)
        {
        }
    };

    @Override
    public void run()
    {
    	/* Set the thread priority */
		try
		{
			android.os.Process.setThreadPriority(android.os.Process.THREAD_PRIORITY_URGENT_AUDIO);
		}
		catch(Exception e)
		{
			Log.e(LOG_TAG, "Exception: Could not set priority to AudioRecorder thread");
			m_recording= false;
			return;
		}

        /* Get minimum buffer size to work */
        //m_bufferSizeInBytes= AudioRecord.getMinBufferSize(sampleAudioRateInHz, 
        //        AudioFormat.CHANNEL_IN_MONO, AudioFormat.ENCODING_PCM_16BIT);
        m_bufferSizeInBytes= rtmpclientGetABufSize()*2; //Let FFmpeg define buffer size (shorts)
        if(m_bufferSizeInBytes<= 0)
        {
        	Log.e(LOG_TAG, "Not valid audio buffer size");
			m_recording= false;
			return;
        }

        /* Allocate samples buffer [shorts] */
        m_bufferSizeInShorts= m_bufferSizeInBytes/2;
        m_audioData= new short[m_bufferSizeInShorts];

        /* Create audio record object */
	    if(m_MicrophoneThread== null)
	    {
	    	m_MicrophoneThread= new MicrophoneThread(m_context);
	    }
	    else
	    {
	    	Log.e(LOG_TAG, "Audio source should not be already instantiated");
			m_recording= false;
			releaseAudioRecord();
			return;
	    }

	    synchronized(m_MicrophoneThread)
	    {
	    	m_audioRecord= m_MicrophoneThread.open(m_bufferSizeInBytes, m_sampleRate);
	    	if(m_audioRecord== null)
	    	{
				m_recording= false;
				releaseAudioRecord();
				return;
	    	}
	    }

        /* Set notifications call-backs */
        if(m_audioRecord.setPositionNotificationPeriod(m_bufferSizeInShorts)!= AudioRecord.SUCCESS)
        {
        	Log.e(LOG_TAG, "Could not set position notification period");
			m_recording= false;
			releaseAudioRecord();
			return;
        }
        m_audioRecord.setRecordPositionUpdateListener(mListener);

        /* Start audio sampling */
        try
        {
        	m_audioRecord.startRecording();
        }
        catch(Exception e)
        {
        	Log.e(LOG_TAG, "Could not start recording from the AudioRecord");
			m_recording= false;
			releaseAudioRecord();
			return;
        }
        Log.d(LOG_TAG, "configured audio format: " + m_audioRecord.getAudioFormat() );
        Log.d(LOG_TAG, "configured audio s.config: " + m_audioRecord.getChannelConfiguration() );
        Log.d(LOG_TAG, "configured audio s.rate: " + m_audioRecord.getSampleRate() );
        Log.d(LOG_TAG, "configured audio buf size: " + m_bufferSizeInBytes );
        Log.d(LOG_TAG, "AudioRecoreder is recording!");

        // Audio Capture/Encoding Loop
        m_bufferReadResult= m_audioRecord.read(m_audioData, 0, m_bufferSizeInShorts);
        if(m_bufferReadResult< 0)
        {
        	Log.e(LOG_TAG, "Could not read from the AudioRecord instance");
			m_recording= false;
        }
        while(m_recording)
        {
        	try
        	{
        	    Thread.sleep(1000);
        	}
        	catch(InterruptedException e) 
        	{
        		Log.w(LOG_TAG, "AudioRecorder thraed was interrumpted; " +
        				"thread is going down!");
        		m_recording= false;
        	}
        }

        Log.w(LOG_TAG,"AudioThread Finished");

        /* Capture/Encoding finished, release recorder */
        releaseAudioRecord();
    }

    private void releaseAudioRecord()
    {
		if(m_MicrophoneThread!= null)
		{
			m_MicrophoneThread.close();
			m_MicrophoneThread= null;
		}
    }

    /* 
     * Private native methods implemented by native library packaged
     * with this application.
     */
	public native int rtmpclientGetABufSize();
    public native int encodeAFrame(short[] buffer, int framesize);
}
