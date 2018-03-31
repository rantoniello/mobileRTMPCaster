#ifndef DEMUXER_HH_
#define DEMUXER_HH_

/* Includes */
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
//#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>

/* Definitions */

#define NUM_ABUFS 	8
#define NUM_INPUT_BUFS 	40

#define PERSISTANT_ERROR_TOLERANCE_RFRAME_MSECS 	(1000*5) // 5 seconds

#define DEMUXER_MAX_IMAGE_WIDTH 	1920
#define DEMUXER_MAX_IMAGE_HEIGTH 	1088

//#define DO_NOT_DECODE

enum mutimedia_id
{
	MM_AUDIO= 0,
	MM_VIDEO,
	MM_IDNUM
};

enum event_code
{
	EVENT_IRESOLUTION_CHANGED= 0,
	EVENT_CODENUM
};

/* Presentation/rendering callbacks structure */
typedef struct presentationCallbackCtx
{
#define PRESENTATION_CALLBACK_CTX_MEMBERS \
    int (*prologue)(void*); \
    void (*epilogue)(void*); \
    int (*render)(void*); \
	void* opaque;
    PRESENTATION_CALLBACK_CTX_MEMBERS
} presentationCallbackCtx;

/* Initialization structure */
typedef struct demuxerInitCtx
{
    /* Multiplex related */
    const char *url;
    size_t url_length;
    /* Video decoding related */
    int width, height;
    /* Audio decoding related */
    int isMuteOn;
    /* Video presentation*/
    presentationCallbackCtx callbacksVideo;
    /* Audio presentation */
    presentationCallbackCtx callbacksAudio;
    /* Extarnal method callbacks */
    void (*scale_video_frame)(void* src, int src_width, int src_height, int src_stride,
                              void** dst, int dst_width, int dst_height, int dst_stride);
    /* Error/event callbacks */
    void (*on_error)(void*);
    int persistant_error_msecs_limit;
    void (*on_event)(void*);
} demuxerInitCtx;

/* Define threaded-decoding related structure */
typedef struct thrDecCtx
{ 
	int id;
    struct demuxerCtx* demuxer; // to access demuxer from decoding threads
	pthread_mutex_t dec_mutex;
    pthread_cond_t consume_cond;
    pthread_cond_t produce_cond;
    AVPacket *dec_pkt;
	int (*prologue)(void*);
	void (*epilogue)(void*);
	int (*decode_frame)(void*);
    void (*scale_frame)(void* src, int src_width, int src_height, int src_stride,
                        void** dst, int dst_width, int dst_height, int dst_stride);
    pthread_t dec_thread;
} thrDecCtx;

/* Define threaded-presentation related structure */
typedef struct thrPresentationCtx
{ 
	int id;
    struct demuxerCtx* demuxer; // to access demuxer from presentation threads
	pthread_mutex_t mutex;
	pthread_cond_t consume_cond;
	pthread_cond_t produce_cond;
	volatile int ready; // boolean indicating packet ready for presentation
	volatile int reset; // boolean indicating that rendering system should be reset
	volatile int next_width, next_height;
    PRESENTATION_CALLBACK_CTX_MEMBERS;
    pthread_t thread;
} thrPresentationCtx;

/* Define 'demuxer context' structure */
typedef struct demuxerCtx
{
	// General
	AVPacket pkt[NUM_INPUT_BUFS];
    volatile int pkt_level[NUM_INPUT_BUFS];
    volatile int recv_pkt_idx; // Received packet index
    volatile int dist_pkt_idx; // Distribution (consumed) packet index
	volatile int can_run_demux;
	volatile int interrupt_IO_FFmpeg;
	struct thrDecCtx* dec_ctx[MM_IDNUM];
	struct thrPresentationCtx* presentat_ctx[MM_IDNUM];
    pthread_mutex_t mutex;
    pthread_cond_t consume_cond;
    pthread_cond_t produce_cond;
	pthread_t recv_thread;
    pthread_t dist_thread;

	// Container setting
	char* uri;
	AVFormatContext *fmt_ctx;

	// Video settings
	AVCodecContext *video_dec_ctx;
	AVStream *video_stream;
	AVPicture *pFrameConverted;
	struct SwsContext *img_convert_ctx;
	int video_stream_idx;
	int v_width, v_height;
	int v_iwidth, v_iheight; //input

	// Audio setting
	AVCodecContext *audio_dec_ctx;
	AVStream *audio_stream;
	struct SwrContext *swr_ctx;
	volatile uint8_t **audio_buf[NUM_ABUFS];
volatile int audio_buf_levels[NUM_ABUFS]; //FIXME
	volatile size_t audio_buf_size;
	volatile int num_abuf;
volatile int num_oabuf;
volatile int underrun_flag;
	volatile int output_buffer_level;
	enum AVSampleFormat dst_sample_fmt;
	int dst_nb_channels;
	int dst_linesize;
	int dst_nb_samples;
	int audio_stream_idx;
	volatile int audio_mute;

	// On error callback
	void (*on_error)(void*);
	int error_code;
	int persistant_error_msecs_limit; // to set limit externally (-1== 'do not use this feature')
	uint64_t persistant_error_msecs[MM_IDNUM];
	int persistant_error_num[MM_IDNUM];

	// On event callback
	void (*on_event)(void*);
	int event_code;
} demuxerCtx;

/* Public prototypes */
int demuxer_init(demuxerCtx* demuxer, demuxerInitCtx* demuxer_init_ctx);
void demuxer_release(demuxerCtx* demuxer);

#endif /* DEMUXER_HH_ */
