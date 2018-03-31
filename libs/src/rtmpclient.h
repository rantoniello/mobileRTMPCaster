#ifndef RTMPCLIENT_HH_
#define RTMPCLIENT_HH_

/* Definitions */
//#define _DEBUG
//#define USE_FFMPEG_INTERLEAVE_BUFFER

#define RTMPCLIENT_MAX_IMAGE_WIDTH 		1920
#define RTMPCLIENT_MAX_IMAGE_HEIGTH 	1088

#define PERSISTANT_ERROR_TOLERANCE_WFRAME_USECS 	(1000*1000*5) // 5 seconds

#define MOD_SIZE 1024//256 //**MUST BE POWER OF ^2**

#define JSON_SCAN_SIZE 3
#define JSON_PARAMETER_SIZE 1024

#ifdef USE_FFMPEG_INTERLEAVE_BUFFER
	#define AV_WRITE_FRAME av_interleaved_write_frame
#else
	#define AV_WRITE_FRAME av_write_frame
#endif

/* Includes */
#include <libavcodec/avcodec.h>
#include <libavutil/mathematics.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>

/* Define 'rtmp muxer context' structure */
typedef struct rtmpMuxerCtx
{
	AVFormatContext *oc;
	struct timespec monotime_init;
	pthread_mutex_t write_mutex;
	pthread_mutex_t audio_mutex;
	pthread_mutex_t video_mutex;
	pthread_mutex_t bckground_mutex;
	int inBackground;
	pthread_t bckgroundThread;
	pthread_attr_t bckgroundAttr;
	int bckground_width, bckground_height;
	char* bckgroundLogo;
	int doInterleave;

	/* video */
	int v_available;
	AVStream *video_st;
	/**
	*typedef struct AVPicture {
	*	uint8_t *data[AV_NUM_DATA_POINTERS];
	*	int linesize[AV_NUM_DATA_POINTERS]; ///< number of bytes per line
	*} AVPicture;
	*/
	AVPicture *picture;
	AVFrame *vframe;
	// video settings
	int v_bit_rate;
	int v_rc;
	int v_frame_rate;
	int v_width, v_height;
	int v_gop_size;
	enum AVPixelFormat v_pix_format;
	enum AVCodecID v_codec_id;
	char av_quality[32];

	/* audio */
	int a_available;
	AVStream *audio_st;
	int16_t* adata;
	unsigned int adata_size;
	AVFrame *aframe;
	// Circular Interleaving buffer algorithm
	AVPacket *apkt_aray[MOD_SIZE];
	uint32_t af; // circular buffer "front pointer"
	uint32_t ab; // circular buffer "back pointer"
	uint32_t level; // circular buffer level
	uint32_t alevel; // circular audio pkts buffer level
	uint32_t vlevel; // circular video pkts buffer level
	// audio settings
	int a_bit_rate;
	int a_sample_rate;
	int a_nb_channels;
    int a_frame_size;
	enum AVCodecID a_codec_id;
#ifndef _ANDROID_
#define SAMLE_FIFO_MAX_SIZE 4096
    uint16_t sample_fifo[SAMLE_FIFO_MAX_SIZE];
    int sample_fifo_level;
#endif

	/* Container setting */
	char* uri;
	char* o_mux_format;

	// On error callback
	void (*on_error)(void*);
	int error_code;
	int persistant_error_usecs_limit; // to set limit externally (-1== 'do not use this feature')
	uint64_t persistant_error_usecs;
} rtmpMuxerCtx;

/* Public prototypes */
void rtmpclient_open(rtmpMuxerCtx* muxer);
void rtmpclient_close(rtmpMuxerCtx* muxer);
int rtmpclient_init(rtmpMuxerCtx* muxer);
int rtmpclient_encode_vframe(rtmpMuxerCtx* muxer, uint64_t t);
int rtmpclient_encode_aframe(rtmpMuxerCtx* muxer, uint64_t t);
void rtmpclient_release(AVFormatContext **ocrtmp, rtmpMuxerCtx* muxer);
void rtmp_client_runBackground(rtmpMuxerCtx* muxer);
void rtmp_client_stopBackground(rtmpMuxerCtx* muxer);
int cmd_parser(rtmpMuxerCtx* muxer, const char* cmd);
uint64_t get_monotonic_clock_time(rtmpMuxerCtx* muxer);

#endif /* RTMPCLIENT_HH_ */
