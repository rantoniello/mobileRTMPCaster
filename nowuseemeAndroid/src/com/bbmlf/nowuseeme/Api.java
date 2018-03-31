package com.bbmlf.nowuseeme;

//import android.webkit.JavascriptInterface;  // for version>=17
import android.widget.Toast;
import android.util.Log;

/*
 * This is a JavaScript handler class // for version>=17
 */
public class Api
{
	private MainActivity m_parentActivity;
	private RtmpClientCtrl m_RtmpClientCtrl;
	private PlayerCtrl m_PlayerCtrl;

    public Api(MainActivity activity)
    {
        m_parentActivity= activity;
        m_RtmpClientCtrl= m_parentActivity.getRtmpClientCtrl();
        m_PlayerCtrl= m_parentActivity.getPlayerCtrl();
    }

    /*=== ERROR HANDLING ===*/
    //@JavascriptInterface // for version>=17
    public int getLatestErrorCode()
    {	
    	return this.m_parentActivity.getLatestErrorCode();
    }

    //@JavascriptInterface // for version>=17
    public String getLatestErrorText()
    {
    	return this.m_parentActivity.getLatestErrorText();
    }

    /*==== MUX SETTINGS ====*/

    // connect to rtmp server
    //@JavascriptInterface // for version>=17
    public void connect(String url)
    {
    	m_RtmpClientCtrl.connect(url);
    }

    public void disconnect()
    {
    	m_RtmpClientCtrl.stop();
    }

    public void autoReconnectRTMPCtrlSet(int autoReconnectTimeOut)
    {
    	m_RtmpClientCtrl.autoReconnectSet(autoReconnectTimeOut);
    }

    public int autoReconnectRTMPCtrlGet()
    {
    	return m_RtmpClientCtrl.autoReconnectGet();
    }

    //@JavascriptInterface // for version>=17
    public String videoServerURLGet()
	{
    	return m_RtmpClientCtrl.videoServerURLGet();
    }

    /*==== VIDEO SETTINGS ====*/

    // get current camera width
    //@JavascriptInterface // for version>=17
    public int videoResolutionWidthGet()
    {
    	return m_RtmpClientCtrl.videoResolutionWidthGet();
    }
    //get current camera height
    //@JavascriptInterface // for version>=17
    public int videoResolutionHeightGet()
    {
    	return m_RtmpClientCtrl.videoResolutionHeightGet();
    }
    //set current resolution
    //@JavascriptInterface // for version>=17
	public boolean videoResolutionSet(int width, int height)
    {
		return m_RtmpClientCtrl.videoResolutionSet(width, height);
    }
	// test capture from camera on this resolution and return closest resolution available
    //@JavascriptInterface // for version>=17
	public String videoResolutionTest(int width, int height, int keepRatio)
    {
		return m_RtmpClientCtrl.videoResolutionTest(width, height, keepRatio);
    }
	// get list of supported resolutions
    //@JavascriptInterface // for version>=17
	public String videoResolutionsGet()
    {
		return m_RtmpClientCtrl.videoResolutionsGet();
    }
	
	//get list of supported FPS ranges
	public String videoFPSRangesGet()
	{
		return m_RtmpClientCtrl.videoFPSRangesGet();
	}
	
    // get FPS
    //@JavascriptInterface // for version>=17
    public int videoFPSGet()
    {
    	return m_RtmpClientCtrl.videoFPSGet();
    }
    // set flash.media.Camera.setMode.fps
    //@JavascriptInterface // for version>=17
    public boolean videoFPSSet(int fps)
    {
    	return m_RtmpClientCtrl.videoFPSSet(fps);
    }
    // set flash.media.Camera.setKeyFrameInterval
    //@JavascriptInterface // for version>=17
    public boolean videoKeyFrameIntervalSet(int keyFrameInterval)
    {
    	return m_RtmpClientCtrl.videoKeyFrameIntervalSet(keyFrameInterval);
    }
    // get flash.media.Camera.setKeyFrameInterval
    //@JavascriptInterface // for version>=17
    public int videoKeyFrameIntervalGet()
    {
    	return m_RtmpClientCtrl.videoKeyFrameIntervalGet();
    }
    // get codec name
    //@JavascriptInterface // for version>=17
    public String videoCodecNameGet()
    {
    	return m_RtmpClientCtrl.videoCodecNameGet();
    }
    // get list of compatible codecs
    //@JavascriptInterface // for version>=17
    public String videoCodecsGet()
    {
    	return m_RtmpClientCtrl.videoCodecsGet();
    }
    // set codec
    //@JavascriptInterface // for version>=17
    public boolean videoCodecSet(String codec)
    {
    	return m_RtmpClientCtrl.videoCodecSet(codec);
    }
    // get/set flash.media.Camera.bandwidth
    //@JavascriptInterface // for version>=17
    public boolean videoBandwidthSet(int bw)
    {
    	return m_RtmpClientCtrl.videoBandwidthSet(bw);
    }
    // get/set flash.media.Camera.bandwidth
    //@JavascriptInterface // for version>=17
    public int videoBandwidthGet()
    {
    	return m_RtmpClientCtrl.videoBandwidthGet();
    }
    // get/set flash.media.Camera.quality
    //@JavascriptInterface // for version>=17
    public boolean videoQualitySet(String quality)
    {
		return m_RtmpClientCtrl.videoQualitySet(quality);
    }
    // get/set flash.media.Camera.quality
    //@JavascriptInterface // for version>=17
    public String videoQualityGet()
    {
    	return m_RtmpClientCtrl.videoQualityGet();
    }
    // get/set rate control tolerance (in % units; -1 o 0 means no control)
    //@JavascriptInterface // for version>=17
    public boolean rateCtrlToleranceSet(int rc)
    {
    	return m_RtmpClientCtrl.rateCtrlToleranceSet(rc);
    }
    //@JavascriptInterface // for version>=17
    public int rateCtrlToleranceGet()
    {
    	return m_RtmpClientCtrl.rateCtrlToleranceGet();
    }
    // is it capturing from the camera?
    //@JavascriptInterface // for version>=17
    public boolean isVideoMuted()
    {
    	return m_RtmpClientCtrl.isVideoMuted();
    }

    /*==== AUDIO SETTINGS ====*/

    //get codec name
    //@JavascriptInterface // for version>=17
    public String audioCodecNameGet()
    {
    	return m_RtmpClientCtrl.audioCodecNameGet();
    }
    // get list of compatible codecs
    //@JavascriptInterface // for version>=17
    public String audioCodecsGet()
    {
    	return m_RtmpClientCtrl.audioCodecsGet();
    }
    // set codec
    //@JavascriptInterface // for version>=17
    public boolean audioCodecSet(String codec)
    {
    	return m_RtmpClientCtrl.audioCodecSet(codec);
    }
    // set/get mute on sender
	//@JavascriptInterface // for version>=17
    public boolean setMuteSender(boolean on)
    {
    	return m_RtmpClientCtrl.setMuteSender(on);
    }
    //@JavascriptInterface // for version>=17
    public boolean getMuteSender()
    {
    	return m_RtmpClientCtrl.getMuteSender();
    }

    /*==== H/W SETTINGS ====*/

	// get list of connected cameras
    //@JavascriptInterface // for version>=17
	public String videoCamerasGet()
	{
		return m_RtmpClientCtrl.videoCamerasGet();
	}
    // get info regarding camera
    //@JavascriptInterface // for version>=17
	public String videoCameraDetailsGet(int cameraId)
	{
    	return m_RtmpClientCtrl.videoCameraDetailsGet(cameraId);
	}
	// get current camera ID
    //@JavascriptInterface // for version>=17
	public int videoCameraGet()
	{
		return m_RtmpClientCtrl.videoCameraGet();
	}
	// set current camera
    //@JavascriptInterface // for version>=17
	public boolean videoCameraSet(int cameraID)
	{
		return m_RtmpClientCtrl.videoCameraSet(cameraID);
	}

	/*==== A/V Player ====*/

	// set video thumbnail (for 2 way video) to hide, and the position
    //@JavascriptInterface // for version>=17
	public void viewVideoDisplay(final boolean display, final int width, final int height, 
			final int x, final int y)
	{
    	m_PlayerCtrl.viewVideoDisplay(display, width, height, x, y);
	}
    //@JavascriptInterface // for version>=17
	public String viewVideoGet()
	{
		return m_PlayerCtrl.viewVideoGet();
	}
	// get/set publish status
    //@JavascriptInterface // for version>=17
    public void videoPublishSet(String url, boolean start)
	{
    	m_PlayerCtrl.videoPublishSet(url, start);
	}
    //@JavascriptInterface // for version>=17
    public boolean videoPublishGet()
	{
		return m_PlayerCtrl.videoPublishGet();
	}
    // set/get mute on player
	//@JavascriptInterface // for version>=17
    public boolean setMutePlayer(boolean on)
    {
    	return m_PlayerCtrl.setMutePlayer(on);
    }
    //@JavascriptInterface // for version>=17
    public boolean getMutePlayer()
    {
    	return m_PlayerCtrl.getMutePlayer();
    }
    //@JavascriptInterface // for version>=17
    public void autoReconnectPlayerSet(int autoReconnectTimeOut)
    {
    	m_PlayerCtrl.autoReconnectSet(autoReconnectTimeOut);
    }
    //@JavascriptInterface // for version>=17
    public int autoReconnectPlayerGet()
    {
    	return m_PlayerCtrl.autoReconnectGet();
    }

	/*==== Preview window ====*/
    //@JavascriptInterface // for version>=17
	public void localPreviewDisplay(final boolean display, 
			final int width, final int height, final int x, final int y)
	{
		m_RtmpClientCtrl.localPreviewDisplay(display, width, height, x, y);
	}

	/*==== Scan Bar ====*/
    //@JavascriptInterface // for version>=17
    public boolean barcodeScan()
	{
    	return m_RtmpClientCtrl.barcodeScan();
    }

	//{ // Bogdan
    /*========= HTML init =========*/
    public void javascriptInit(boolean available, String jsPrefix, boolean useQueue) 
    {
    	m_parentActivity.javascriptInit(available, jsPrefix, useQueue);
    }

    /*======= Other functionality ======*/
    // app control helper functions
    public void exit(){
    	m_parentActivity.finish();
    }

    // system bridge helper functions
    public void toggleFullscreen(boolean value) {
    	m_parentActivity.toggleFullscreen(value);
    }
    
    public void toggleActionBar(boolean value) {
    	m_parentActivity.toggleActionBar(value);
    }

    public String displayInfoGet() {
    	return m_parentActivity.displayInfoGet();
    }
    
    public void showToast(String value) {
    	m_parentActivity.showToast(value);
    }

    public void notificationAdd(String title, String text, String ticker, int intType) {
    	m_parentActivity.notificationAdd(title, text, ticker, intType);
    }
    
  	public String jsQueueGet() {
  		return m_parentActivity.jsQueueGet();
  	}
  	
  	//public void browserReload() { // Not currently used
  	//	m_parentActivity.browserReload();
  	//}
  	
  	//public void browserBackgroundColorSet(String color) { // Not currently used
  	//	m_parentActivity.browserBackgroundColorSet(color);
  	//}
  	
  	public void onError(String val) {
  		return;
  	}
  	
  	public void sendEmail(String emailAddress, String subject, String body) {
  		m_parentActivity.sendEmail(emailAddress, subject, body);
  	}
  	
  	public void openWebBrowser(String sURL) {
  		m_parentActivity.openWebBrowser(sURL);
  	}
	//} //Bogdan
}
