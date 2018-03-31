package com.bbmlf.nowuseeme;

import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;

import android.content.Context;
import android.media.AudioFormat;
import android.media.AudioRecord;
import android.media.MediaRecorder.AudioSource;
import android.os.Looper;
import android.util.Log;

/**
 * Class for handling microphone on a user Thread.
 * "AudioRecord" object is created in a event Thread (owning a Looper) 
 * to avoid executing in main UI thread
 */
public class MicrophoneThread
{
	private final static String LOG_TAG= "MicrophoneThread";
	public final int MAX_WAITTIME_MILLIS= 10*1000;
	private Thread m_thread= null;
	private Looper m_threadLooper= null;
	private AudioRecord m_audioRecord= null;
	private MainActivity m_parentActivity= null;
	private ErrorCodes m_errorCode= ErrorCodes.SUCCESS;
	private Lock m_lock= new ReentrantLock(); // mutual-exclusion lock for public interface
	private int m_bufferSizeInBytes= 0;
	private int m_sampleRate= 0; //FIXME

	public MicrophoneThread(Context context)
	{
		m_parentActivity= (MainActivity)context;
	}

	public AudioRecord open(int bufferSizeInBytes, int sampleRate)
	{
		Log.d(LOG_TAG, "Opening microphone");

		AudioRecord audiorecord;
		m_lock.lock();
		try
		{
			audiorecord= open2(bufferSizeInBytes, sampleRate);
		}
    	finally
    	{
    		m_lock.unlock();
    	}

		if(audiorecord!= null)
			Log.d(LOG_TAG, "Microphone successfuly opened");
		else
			Log.d(LOG_TAG, "Microphone failed to open");
		return audiorecord;
	}

	public void close()
	{
		Log.d(LOG_TAG, "Closing Microphone");

		m_lock.lock();
		try
		{
			close2();
		}
    	finally
    	{
    		m_lock.unlock();
    	}
		Log.d(LOG_TAG, "Microphone closed");
	}

	private AudioRecord open2(int bufferSizeInBytes, int sampleRate)
	{
		m_bufferSizeInBytes= bufferSizeInBytes;
		m_sampleRate= sampleRate;

		m_thread= new Thread()
		{
			@Override
			public void run()
			{
				Looper.prepare();
				m_threadLooper= Looper.myLooper();
				synchronized(this)
				{
					// Open microphone
					m_audioRecord= findAudioRecord();
					if(m_audioRecord== null)
	        			Log.e(LOG_TAG, "Could not open designed audio source");
					else
						Log.v(LOG_TAG, "AudioRecord created");
					this.notify();
				}
				Looper.loop();
			}
		};
		synchronized(m_thread)
		{
			m_thread.start(); //will block until we wait
			try
			{
				m_thread.wait(MAX_WAITTIME_MILLIS); // wait no more than 10 secs... 
			}
			catch(Exception e)
			{
				Log.e(LOG_TAG, "AudioRecord instance creation was interrupted");
				m_errorCode= ErrorCodes.OPEN_MIC_ERROR;
			}
			Log.v(LOG_TAG, "... once created, continue starting audio recording...");
		}

		if(m_audioRecord== null || m_errorCode!= ErrorCodes.SUCCESS)
		{
			m_parentActivity.setLastErrorCode(m_errorCode);
			return null;
		}

		Log.v(LOG_TAG, "... AudioRecord instance successfuly created, quit form waiting.");
		return m_audioRecord;
	}

	private void close2()
	{
    	Log.v(LOG_TAG, "Stopping audio source");
        if(m_audioRecord!= null)
        {
        	if(m_audioRecord.getState()== AudioRecord.STATE_INITIALIZED)
        	{
        		try
    			{
        			m_audioRecord.stop();
    			}
        		catch(IllegalStateException e)
        		{
        			Log.e(LOG_TAG,"Could not stop 'AudioRecord' object");
        		}
        	}
        }

        Log.v(LOG_TAG, "Quit microphone thread looper");
        if(m_threadLooper!= null)
        {
        	m_threadLooper.quit();
        	m_threadLooper= null;
        }

        Log.v(LOG_TAG, "Join microphone thread");
		if(m_thread!= null && m_thread.isAlive() )
		{
			Log.v(LOG_TAG, "AudioRecord thread is alive; wait join...");
			try
			{
				m_thread.join();
			}
			catch(InterruptedException e)
			{
				Log.e(LOG_TAG, "AudioRecord thread join wait was interrupted.");
			}
			Log.v(LOG_TAG, "AudioRecord thread joined.");
		}
		if(m_thread!= null) m_thread= null;

		Log.v(LOG_TAG, "Release microphone");
        if(m_audioRecord!= null)
        {
	        m_audioRecord.release();
	        m_audioRecord= null;
        }
        Log.v(LOG_TAG,"Microphone released");
	}

    /* Hack: Auxiliary.
     * Test possible audio-format combinations to be able to support 
     * opening "AudioRecorder" on a wide set of devices/emulators
     * TODO: Merge with method 'DefaultLocalSettings::audioSampleRateSelectDefault()'.
     * */
    private AudioRecord findAudioRecord()
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
                            if(bufferSize<= m_bufferSizeInBytes)
                            	bufferSize= m_bufferSizeInBytes; // Let FFmpeg set buffer size if possible
                            else
                            	m_bufferSizeInBytes= bufferSize; // if not possible, update value

                            m_sampleRate= rate; // update sampling-rate

                            // check if we can instantiate and have a success
                        	Log.d(LOG_TAG, "Attempting rate "+ rate+ "Hz, bits: "+ audioFormat+ ", channel: "+ channelConfig+ 
                        			", buffer size: " + m_bufferSizeInBytes);
                            AudioRecord recorder= new AudioRecord(AudioSource.DEFAULT, rate, channelConfig, audioFormat, m_bufferSizeInBytes);

                            if (recorder.getState()== AudioRecord.STATE_INITIALIZED)
                                return recorder;
                        }
                    }
                    catch (Exception e)
                    {
                        Log.e(LOG_TAG, rate + "Exception, keep trying.",e);
                    }
                }
            }
        }
        return null;
    }
}
