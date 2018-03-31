package com.bbmlf.nowuseeme;

public enum ErrorCodes
{
	// JNI related: define exactly the same errors codes as thrown by JNI layer
	SUCCESS(0, "Ok"),
	ERROR(-1, "Not catalogued/generic error."),
	ENCODER_NOT_SUPPORTED_ERROR(-2, "A/V encoder not supported."),
	VIDEO_ENCODER_NOT_SUPPORTED_ERROR(-3, "Video encoder not supported."),
	AUDIO_ENCODER_NOT_SUPPORTED_ERROR(-4, "Audio encoder not supported."),
	STREAM_ALLOCATION_ERROR(-5, "Stream allocation error."),
	ADD_VIDEO_STREAM_ERROR(-6, "Adding video stream error."),
	ADD_AUDIO_STREAM_ERROR(-7, "Adding audio stream error."),
	OPEN_VIDEO_CODEC_ERROR(-8, "Open video codec error (review video parameters)."),
	OPEN_AUDIO_CODEC_ERROR(-9, "Open audio codec error (review audio parameters)."),
	VIDEO_FRAME_BUFFER_ALLOC_ERROR(-10, "Video frame buffer allocation error."),
	AUDIO_SAMPLE_BUFFER_ALLOC_ERROR(-11, "Audio sample buffer allocation error."),
	MUX_FORMAT_NOT_SUPPORTED(-12, "Multiplexer format not supported."),
	OPEN_CONNECTION_OR_OFILE_ERRROR(-13, "Could not open connection or file opening error."),
	SENDING_DATA_OR_WRITING_ERROR(-14, "Sending data or writing error."),
	VIDEO_FRAME_ENCODING_ERROR(-15, "Video frame encoding error."),
	AUDIO_FRAME_ENCODING_ERROR(-16, "Audio frame encoding error."),
	INTERLEAVING_ERROR(-17, "A/V Interleaving error."),
	INTERLEAVING_BUF_OVERFLOW_ERROR(-18, "A/V Interleaving buffer overflow error"),
	JSON_COMMAND_PARSER_ERROR(-19, "JSON parameters/command parser error."),
	RTMP_CLIENT_ALREADY_INITIALIZED(-20, "RTMP client already initialized."),
	RTMP_CLIENT_NOT_PREV_INITIALIZED(-21, "RTMP client must be previously initialized."),
	EMPTY_AUDIO_FRAME_ERROR(-22, "Empty audio frame recieved error."),
	VIDEO_RESOLUTION_OUT_OF_BOUNDS_ERROR(-23, "Video resolution out of bound, max is 1920x1080"),
	ALREADY_IN_BACKGROUND_ERROR(-24, "RTMP client is already running in background mode"),
	NOT_IN_BACKGROUND_ERROR(-25, "RTMP client is not running in background mode"),
	BAD_ARGUMENT_OR_INITIALIZATION_ERROR(-26, "Method has received a bad argument"),
	NO_MULTIMEDIA_FOUND_IN_STREAM(-27, "No multimedia content found in stream"),
	COULD_NOT_START_PLAYER(-28, "Could not start player"),
	PLAYER_DECODING_ERROR(-29, "Decoding A/V error in player"),
	BAD_URL_PLAYER(-30, "Player could not open given URL"),
	CONNECTION_RESET_ERROR(-31, "RTMP connection has been reset"),

	// VideoRecorder related
	OPEN_CAMERA_ERROR(-32, "The camera is in use by another process or device policy manager has disabled camera access"),
	SET_PREVIEW_DISPLAY_ERROR(-33, "Preview display surface is unavailable or unsuitable."),
	SET_CAMERA_PARAMS_ERROR(-34, "Some camera parameters are not supported by your device/camera"),
	OPEN_MIC_ERROR(-35, "The microphone is in use by another process or device policy manager has disabled access"),
	END_ERROR(-128, ""); // Dummy

	private final int code;
	private final String description;

	private ErrorCodes(int code, String description)
	{
		this.code = code;
		this.description = description;
	}

	public String getDescription()
	{
		return description;
	}

	public int getCode()
	{
		return code;
	}

	@Override
	public String toString()
	{
		return code + ": " + description;
	}

	public static ErrorCodes getByCode(int code)
	{
	    for(ErrorCodes e: values() )
	    {
	        if(e.code== code) return e;
	    }
	    return ERROR;
	 }
}
