//
//  ErrorCodes.cpp
//  nowuseeme
//
//  
//

#include "ErrorCodes.h"

const std::map<_retcodes, std::string> ErrorCodes::m_map=
{
    {SUCCESS, "Ok"},
    {ERROR, "Not catalogued/generic error."},
    {ENCODER_NOT_SUPPORTED_ERROR, "A/V encoder not supported."},
    {VIDEO_ENCODER_NOT_SUPPORTED_ERROR, "Video encoder not supported."},
    {AUDIO_ENCODER_NOT_SUPPORTED_ERROR, "Audio encoder not supported."},
    {STREAM_ALLOCATION_ERROR, "Stream allocation error."},
    {ADD_VIDEO_STREAM_ERROR, "Adding video stream error."},
    {ADD_AUDIO_STREAM_ERROR, "Adding audio stream error."},
    {OPEN_VIDEO_CODEC_ERROR, "Open video codec error (review video parameters)."},
    {OPEN_AUDIO_CODEC_ERROR, "Open audio codec error (review audio parameters)."},
    {VIDEO_FRAME_BUFFER_ALLOC_ERROR, "Video frame buffer allocation error."},
    {AUDIO_SAMPLE_BUFFER_ALLOC_ERROR, "Audio sample buffer allocation error."},
    {MUX_FORMAT_NOT_SUPPORTED, "Multiplexer format not supported."},
    {OPEN_CONNECTION_OR_OFILE_ERRROR, "Could not open connection or file opening error."},
    {SENDING_DATA_OR_WRITING_ERROR, "Sending data or writing error."},
    {VIDEO_FRAME_ENCODING_ERROR, "Video frame encoding error."},
    {AUDIO_FRAME_ENCODING_ERROR, "Audio frame encoding error."},
    {INTERLEAVING_ERROR, "A/V Interleaving error."},
    {INTERLEAVING_BUF_OVERFLOW_ERROR, "A/V Interleaving buffer overflow error"},
    {JSON_COMMAND_PARSER_ERROR, "JSON parameters/command parser error."},
    {RTMP_CLIENT_ALREADY_INITIALIZED, "RTMP client already initialized."},
    {RTMP_CLIENT_NOT_PREV_INITIALIZED, "RTMP client must be previously initialized."},
    {EMPTY_AUDIO_FRAME_ERROR, "Empty audio frame recieved error."},
    {VIDEO_RESOLUTION_OUT_OF_BOUNDS_ERROR, "Video resolution out of bound, max is 1920x1080"},
    {ALREADY_IN_BACKGROUND_ERROR, "RTMP client is already running in background mode"},
    {NOT_IN_BACKGROUND_ERROR, "RTMP client is not running in background mode"},
    {BAD_ARGUMENT_OR_INITIALIZATION_ERROR, "Method has received a bad argument"},
    {NO_MULTIMEDIA_FOUND_IN_STREAM, "No multimedia content found in stream"},
    {COULD_NOT_START_PLAYER, "Could not start player"},
    {PLAYER_DECODING_ERROR, "Decoding A/V error in player"},
    {BAD_URL_PLAYER, "Player could not open given URL"},
    {CONNECTION_RESET_ERROR, "RTMP connection has been reset"},
    // VideoRecorder related
    {OPEN_CAMERA_ERROR, "The camera is in use by another process or device policy manager has disabled camera access"},
    {SET_PREVIEW_DISPLAY_ERROR, "Preview display surface is unavailable or unsuitable."},
    {SET_CAMERA_PARAMS_ERROR, "Some camera parameters are not supported by your device/camera"},
    {OPEN_MIC_ERROR, "The microphone is in use by another process or device policy manager has disabled access"},
    {END_ERROR, "null"} // Dummy
};
