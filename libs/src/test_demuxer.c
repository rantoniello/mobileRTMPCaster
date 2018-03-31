#if 0
/**
 * @file
 * libavformat demuxing API use example.
 *
 * Show how to use the libavformat and libavcodec API to demux and
 * decode audio and video data.
 * @example doc/examples/demuxing.c
 */
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>
static AVFormatContext *fmt_ctx = NULL;
static AVCodecContext *video_dec_ctx = NULL, *audio_dec_ctx;
static AVStream *video_stream = NULL, *audio_stream = NULL;
static const char *src_filename = NULL;
static const char *video_dst_filename = NULL;
static const char *audio_dst_filename = NULL;
static FILE *video_dst_file = NULL;
static FILE *audio_dst_file = NULL;
static uint8_t *video_dst_data[4] = {NULL};
static int video_dst_linesize[4];
static int video_dst_bufsize;
static int video_stream_idx = -1, audio_stream_idx = -1;
static AVFrame *frame = NULL;
static AVPacket pkt;
static int video_frame_count = 0;
static int audio_frame_count = 0;
static int decode_packet(int *got_frame, int cached)
{
	int ret = 0;
	int decoded = pkt.size;
	*got_frame = 0;
	if (pkt.stream_index == video_stream_idx)
	{
		/* decode video frame */
		ret = avcodec_decode_video2(video_dec_ctx, frame, got_frame, &pkt);
		if (ret < 0) {
			fprintf(stderr, "Error decoding video frame\n");
			return ret;
		}
		if (*got_frame) {
			printf("video_frame%s n:%d coded_n:%d pts:%s\n",
					cached ? "(cached)" : "",
							video_frame_count++, frame->coded_picture_number,
							av_ts2timestr(frame->pts, &video_dec_ctx->time_base));
			/* copy decoded frame to destination buffer:
			 * this is required since rawvideo expects non aligned data */
			av_image_copy(video_dst_data, video_dst_linesize,
					(const uint8_t **)(frame->data), frame->linesize,
					video_dec_ctx->pix_fmt, video_dec_ctx->width, video_dec_ctx->height);
			/* write to rawvideo file */
			fwrite(video_dst_data[0], 1, video_dst_bufsize, video_dst_file);
		}
	} else if (pkt.stream_index == audio_stream_idx) {
		/* decode audio frame */
		ret = avcodec_decode_audio4(audio_dec_ctx, frame, got_frame, &pkt);
		if (ret < 0) {
			fprintf(stderr, "Error decoding audio frame\n");
			return ret;
		}
		/* Some audio decoders decode only part of the packet, and have to be
		 * called again with the remainder of the packet data.
		 * Sample: fate-suite/lossless-audio/luckynight-partial.shn
		 * Also, some decoders might over-read the packet. */
		decoded = FFMIN(ret, pkt.size);
		if (*got_frame) {
			size_t unpadded_linesize = frame->nb_samples * av_get_bytes_per_sample(frame->format);
			printf("audio_frame%s n:%d nb_samples:%d pts:%s\n",
					cached ? "(cached)" : "",
							audio_frame_count++, frame->nb_samples,
							av_ts2timestr(frame->pts, &audio_dec_ctx->time_base));
			/* Write the raw audio data samples of the first plane. This works
			 * fine for packed formats (e.g. AV_SAMPLE_FMT_S16). However,
			 * most audio decoders output planar audio, which uses a separate
			 * plane of audio samples for each channel (e.g. AV_SAMPLE_FMT_S16P).
			 * In other words, this code will write only the first audio channel
			 * in these cases.
			 * You should use libswresample or libavfilter to convert the frame
			 * to packed data. */
			fwrite(frame->extended_data[0], 1, unpadded_linesize, audio_dst_file);
		}
	}
	return decoded;
}
static int open_codec_context(int *stream_idx,
		AVFormatContext *fmt_ctx, enum AVMediaType type)
{
	int ret;
	AVStream *st;
	AVCodecContext *dec_ctx = NULL;
	AVCodec *dec = NULL;
	ret = av_find_best_stream(fmt_ctx, type, -1, -1, NULL, 0);
	if (ret < 0) {
		fprintf(stderr, "Could not find %s stream in input file '%s'\n",
				av_get_media_type_string(type), src_filename);
		return ret;
	} else {
		*stream_idx = ret;
		st = fmt_ctx->streams[*stream_idx];
		/* find decoder for the stream */
		dec_ctx = st->codec;
		dec = avcodec_find_decoder(dec_ctx->codec_id);
		if (!dec) {
			fprintf(stderr, "Failed to find %s codec\n",
					av_get_media_type_string(type));
			return ret;
		}
		if ((ret = avcodec_open2(dec_ctx, dec, NULL)) < 0) {
			fprintf(stderr, "Failed to open %s codec\n",
					av_get_media_type_string(type));
			return ret;
		}
	}
	return 0;
}
static int get_format_from_sample_fmt(const char **fmt,
		enum AVSampleFormat sample_fmt)
{
	int i;
	struct sample_fmt_entry {
		enum AVSampleFormat sample_fmt; const char *fmt_be, *fmt_le;
	} sample_fmt_entries[] = {
			{ AV_SAMPLE_FMT_U8, "u8", "u8" },
			{ AV_SAMPLE_FMT_S16, "s16be", "s16le" },
			{ AV_SAMPLE_FMT_S32, "s32be", "s32le" },
			{ AV_SAMPLE_FMT_FLT, "f32be", "f32le" },
			{ AV_SAMPLE_FMT_DBL, "f64be", "f64le" },
	};
	*fmt = NULL;
	for (i = 0; i < FF_ARRAY_ELEMS(sample_fmt_entries); i++) {
		struct sample_fmt_entry *entry = &sample_fmt_entries[i];
		if (sample_fmt == entry->sample_fmt) {
			*fmt = AV_NE(entry->fmt_be, entry->fmt_le);
			return 0;
		}
	}
	fprintf(stderr,
			"sample format %s is not supported as output format\n",
			av_get_sample_fmt_name(sample_fmt));
	return -1;
}
int main (int argc, char **argv)
{
	int ret = 0, got_frame;
	if (argc != 4) {
		fprintf(stderr, "usage: %s input_file video_output_file audio_output_file\n"
				"API example program to show how to read frames from an input file.\n"
				"This program reads frames from a file, decodes them, and writes decoded\n"
				"video frames to a rawvideo file named video_output_file, and decoded\n"
				"audio frames to a rawaudio file named audio_output_file.\n"
				"\n", argv[0]);
		exit(1);
	}
	src_filename = argv[1];
	video_dst_filename = argv[2];
	audio_dst_filename = argv[3];
	/* register all formats and codecs */
	av_register_all();
	/* open input file, and allocate format context */
	if (avformat_open_input(&fmt_ctx, src_filename, NULL, NULL) < 0) {
		fprintf(stderr, "Could not open source file %s\n", src_filename);
		exit(1);
	}
	/* retrieve stream information */
	if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
		fprintf(stderr, "Could not find stream information\n");
		exit(1);
	}
	if (open_codec_context(&video_stream_idx, fmt_ctx, AVMEDIA_TYPE_VIDEO) >= 0) {
		video_stream = fmt_ctx->streams[video_stream_idx];
		video_dec_ctx = video_stream->codec;
//{ //RAL
		video_dec_ctx->pix_fmt= AV_PIX_FMT_RGB565BE; //AV_PIX_FMT_YUV420P;
//} //RAL
		video_dst_file = fopen(video_dst_filename, "wb");
		if (!video_dst_file) {
			fprintf(stderr, "Could not open destination file %s\n", video_dst_filename);
			ret = 1;
			goto end;
		}
		/* allocate image where the decoded image will be put */
		ret = av_image_alloc(video_dst_data, video_dst_linesize,
				video_dec_ctx->width, video_dec_ctx->height,
				video_dec_ctx->pix_fmt, 1);
		if (ret < 0) {
			fprintf(stderr, "Could not allocate raw video buffer\n");
			goto end;
		}
		video_dst_bufsize = ret;
	}
	if (open_codec_context(&audio_stream_idx, fmt_ctx, AVMEDIA_TYPE_AUDIO) >= 0) {
		audio_stream = fmt_ctx->streams[audio_stream_idx];
		audio_dec_ctx = audio_stream->codec;
//{ //RAL
		//audio_dec_ctx->sample_fmt= AV_SAMPLE_FMT_S16;
//} //RAL
		audio_dst_file = fopen(audio_dst_filename, "wb");
		if (!audio_dst_file) {
			fprintf(stderr, "Could not open destination file %s\n", video_dst_filename);
			ret = 1;
			goto end;
		}
	}
	/* dump input information to stderr */
	av_dump_format(fmt_ctx, 0, src_filename, 0);
	if (!audio_stream && !video_stream) {
		fprintf(stderr, "Could not find audio or video stream in the input, aborting\n");
		ret = 1;
		goto end;
	}
	frame = avcodec_alloc_frame();
	if (!frame) {
		fprintf(stderr, "Could not allocate frame\n");
		ret = AVERROR(ENOMEM);
		goto end;
	}
	/* initialize packet, set data to NULL, let the demuxer fill it */
	av_init_packet(&pkt);
	pkt.data = NULL;
	pkt.size = 0;
	if (video_stream)
		printf("Demuxing video from file '%s' into '%s'\n", src_filename, video_dst_filename);
	if (audio_stream)
		printf("Demuxing audio from file '%s' into '%s'\n", src_filename, audio_dst_filename);
	/* read frames from the file */
	while (av_read_frame(fmt_ctx, &pkt) >= 0) {
		AVPacket orig_pkt = pkt;
		do {
			ret = decode_packet(&got_frame, 0);
			if (ret < 0)
				break;
			pkt.data += ret;
			pkt.size -= ret;
		} while (pkt.size > 0);
		av_free_packet(&orig_pkt);
	}
	/* flush cached frames */
	pkt.data = NULL;
	pkt.size = 0;
	do {
		decode_packet(&got_frame, 1);
	} while (got_frame);
	printf("Demuxing succeeded.\n");
	if (video_stream) {
		printf("Play the output video file with the command:\n"
				"ffplay -f rawvideo -pix_fmt %s -video_size %dx%d %s\n",
				av_get_pix_fmt_name(video_dec_ctx->pix_fmt), video_dec_ctx->width, video_dec_ctx->height,
				video_dst_filename);
	}
	if (audio_stream) {
		enum AVSampleFormat sfmt = audio_dec_ctx->sample_fmt;
		int n_channels = audio_dec_ctx->channels;
		const char *fmt;
		if (av_sample_fmt_is_planar(sfmt)) {
			const char *packed = av_get_sample_fmt_name(sfmt);
			printf("Warning: the sample format the decoder produced is planar "
					"(%s). This example will output the first channel only.\n",
					packed ? packed : "?");
			sfmt = av_get_packed_sample_fmt(sfmt);
			n_channels = 1;
		}
		if ((ret = get_format_from_sample_fmt(&fmt, sfmt)) < 0)
			goto end;
		printf("Play the output audio file with the command:\n"
				"ffplay -f %s -ac %d -ar %d %s\n",
				fmt, n_channels, audio_dec_ctx->sample_rate,
				audio_dst_filename);
	}
	end:
	if (video_dec_ctx)
		avcodec_close(video_dec_ctx);
	if (audio_dec_ctx)
		avcodec_close(audio_dec_ctx);
	avformat_close_input(&fmt_ctx);
	if (video_dst_file)
		fclose(video_dst_file);
	if (audio_dst_file)
		fclose(audio_dst_file);
	av_free(frame);
	av_free(video_dst_data[0]);
	return ret < 0;
}

#else

///////////////////////////////////////////////////////////////////////////////

/**
 * @file
 * libavformat demuxing API use example.
 *
 * Show how to use the libavformat and libavcodec API to demux and
 * decode audio and video data.
 * @example doc/examples/demuxing.c
 */
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>

#ifdef _X86_TESTING_LOG
	#define LOGV(...) printf("%s %s %d ", __FILE__, __func__, __LINE__); printf(__VA_ARGS__); fflush(stdout);
	#define LOGD(...) printf("%s %s %d ", __FILE__, __func__, __LINE__); printf(__VA_ARGS__); fflush(stdout);
	#define LOGI(...) printf("%s %s %d ", __FILE__, __func__, __LINE__); printf(__VA_ARGS__); fflush(stdout);
	#define LOGW(...) printf("%s %s %d ", __FILE__, __func__, __LINE__); printf(__VA_ARGS__); fflush(stdout);
	#define LOGE(...) printf("%s %s %d ", __FILE__, __func__, __LINE__); printf(__VA_ARGS__); fflush(stdout);
#endif
#ifdef _ANDROID_LOG
	#include <jni.h>
	#include <android/log.h>
	#define TAG "JNI"
	#define LOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, TAG, __VA_ARGS__)
	#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG  , TAG, __VA_ARGS__)
	#define LOGI(...) __android_log_print(ANDROID_LOG_INFO   , TAG, __VA_ARGS__)
	#define LOGW(...) __android_log_print(ANDROID_LOG_WARN   , TAG, __VA_ARGS__)
	#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR  , TAG, __VA_ARGS__)
#endif
#ifdef _NO_LOG
	#define LOGV(...)
	#define LOGD(...)
	#define LOGI(...)
	#define LOGW(...)
	#define LOGE(...)
#endif

static AVFormatContext *fmt_ctx = NULL;
static AVCodecContext *video_dec_ctx = NULL, *audio_dec_ctx;
static AVStream *video_stream = NULL, *audio_stream = NULL;
static const char *src_filename = NULL;
//static const char *video_dst_filename = NULL;
//static const char *audio_dst_filename = NULL;
//static FILE *video_dst_file = NULL;
//static FILE *audio_dst_file = NULL;
static uint8_t *video_dst_data[4] = {NULL};
static int video_dst_linesize[4];
static int video_dst_bufsize;
static int video_stream_idx = -1, audio_stream_idx = -1;
static AVFrame *frame = NULL;
static AVPacket pkt;
static int video_frame_count = 0;
static int audio_frame_count = 0;

static int decode_packet(int *got_frame, int cached)
{
	int ret = 0;
	int decoded = pkt.size;
	*got_frame = 0;
	if (pkt.stream_index == video_stream_idx)
	{
		/* decode video frame */
		ret = avcodec_decode_video2(video_dec_ctx, frame, got_frame, &pkt);
		if (ret < 0) {
			LOGI("Error decoding video frame\n");
			return ret;
		}
		if (*got_frame) {
			LOGI("video_frame%s n:%d coded_n:%d pts:%s\n",
					cached ? "(cached)" : "",
							video_frame_count++, frame->coded_picture_number,
							av_ts2timestr(frame->pts, &video_dec_ctx->time_base));
			/* copy decoded frame to destination buffer:
			 * this is required since rawvideo expects non aligned data */
			av_image_copy(video_dst_data, video_dst_linesize,
					(const uint8_t **)(frame->data), frame->linesize,
					video_dec_ctx->pix_fmt, video_dec_ctx->width, video_dec_ctx->height);

			/* write to rawvideo file */
//pthread_cond_signal(&s_vsync_cond); //RAL: FIXME!!
			//fwrite(video_dst_data[0], 1, video_dst_bufsize, video_dst_file);
		}
	}
#if 0
	else if (pkt.stream_index == audio_stream_idx) {
		/* decode audio frame */
		ret = avcodec_decode_audio4(audio_dec_ctx, frame, got_frame, &pkt);
		if (ret < 0) {
			LOGI("Error decoding audio frame\n");
			return ret;
		}
		/* Some audio decoders decode only part of the packet, and have to be
		 * called again with the remainder of the packet data.
		 * Sample: fate-suite/lossless-audio/luckynight-partial.shn
		 * Also, some decoders might over-read the packet. */
		decoded = FFMIN(ret, pkt.size);
		if (*got_frame) {
			size_t unpadded_linesize = frame->nb_samples * av_get_bytes_per_sample(frame->format);
			LOGI("audio_frame%s n:%d nb_samples:%d pts:%s\n",
					cached ? "(cached)" : "",
							audio_frame_count++, frame->nb_samples,
							av_ts2timestr(frame->pts, &audio_dec_ctx->time_base));
			/* Write the raw audio data samples of the first plane. This works
			 * fine for packed formats (e.g. AV_SAMPLE_FMT_S16). However,
			 * most audio decoders output planar audio, which uses a separate
			 * plane of audio samples for each channel (e.g. AV_SAMPLE_FMT_S16P).
			 * In other words, this code will write only the first audio channel
			 * in these cases.
			 * You should use libswresample or libavfilter to convert the frame
			 * to packed data. */
			//fwrite(frame->extended_data[0], 1, unpadded_linesize, audio_dst_file);
		}
	}
#endif
	return decoded;
}

static int open_codec_context(int *stream_idx,
		AVFormatContext *fmt_ctx, enum AVMediaType type)
{
	int ret;
	AVStream *st;
	AVCodecContext *dec_ctx = NULL;
	AVCodec *dec = NULL;
	ret = av_find_best_stream(fmt_ctx, type, -1, -1, NULL, 0);
	if (ret < 0) {
		fprintf(stderr, "Could not find %s stream in input file '%s'\n",
				av_get_media_type_string(type), src_filename);
		return ret;
	} else {
		*stream_idx = ret;
		st = fmt_ctx->streams[*stream_idx];
		/* find decoder for the stream */
		dec_ctx = st->codec;
		dec = avcodec_find_decoder(dec_ctx->codec_id);
		if (!dec) {
			fprintf(stderr, "Failed to find %s codec\n",
					av_get_media_type_string(type));
			return ret;
		}
		if ((ret = avcodec_open2(dec_ctx, dec, NULL)) < 0) {
			fprintf(stderr, "Failed to open %s codec\n",
					av_get_media_type_string(type));
			return ret;
		}
	}
	return 0;
}

static int get_format_from_sample_fmt(const char **fmt,
		enum AVSampleFormat sample_fmt)
{
	int i;
	struct sample_fmt_entry {
		enum AVSampleFormat sample_fmt; const char *fmt_be, *fmt_le;
	} sample_fmt_entries[] = {
			{ AV_SAMPLE_FMT_U8, "u8", "u8" },
			{ AV_SAMPLE_FMT_S16, "s16be", "s16le" },
			{ AV_SAMPLE_FMT_S32, "s32be", "s32le" },
			{ AV_SAMPLE_FMT_FLT, "f32be", "f32le" },
			{ AV_SAMPLE_FMT_DBL, "f64be", "f64le" },
	};
	*fmt = NULL;
	for (i = 0; i < FF_ARRAY_ELEMS(sample_fmt_entries); i++) {
		struct sample_fmt_entry *entry = &sample_fmt_entries[i];
		if (sample_fmt == entry->sample_fmt) {
			*fmt = AV_NE(entry->fmt_be, entry->fmt_le);
			return 0;
		}
	}
	fprintf(stderr,
			"sample format %s is not supported as output format\n",
			av_get_sample_fmt_name(sample_fmt));
	return -1;
}

//int main (int argc, char **argv)
int main() //play()
{
	int ret = 0, got_frame;
//	if (argc != 4) {
//		fprintf(stderr, "usage: %s input_file video_output_file audio_output_file\n"
//				"API example program to show how to read frames from an input file.\n"
//				"This program reads frames from a file, decodes them, and writes decoded\n"
//				"video frames to a rawvideo file named video_output_file, and decoded\n"
//				"audio frames to a rawaudio file named audio_output_file.\n"
//				"\n", argv[0]);
//		exit(1);
//	}

	src_filename= "rtmp://10.0.0.104:1935/myapp/mystream"; //argv[1]; //RAL: FIXME!! param
//	video_dst_filename = argv[2];
//	audio_dst_filename = argv[3];

	/* register all formats and codecs */
	av_register_all();

	/* open input file, and allocate format context */
	if (avformat_open_input(&fmt_ctx, src_filename, NULL, NULL) < 0) {
		fprintf(stderr, "Could not open source file %s\n", src_filename);
		exit(1);
	}

	/* retrieve stream information */
	if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
		fprintf(stderr, "Could not find stream information\n");
		exit(1);
	}

	if (open_codec_context(&video_stream_idx, fmt_ctx, AVMEDIA_TYPE_VIDEO) >= 0)
	{
		video_stream = fmt_ctx->streams[video_stream_idx];
		video_dec_ctx = video_stream->codec;
//{ //RAL
		video_dec_ctx->pix_fmt= AV_PIX_FMT_YUV420P; //AV_PIX_FMT_RGB565; //AV_PIX_FMT_YUV420P;
//} //RAL
//		video_dst_file = fopen(video_dst_filename, "wb");
//		if (!video_dst_file) {
//			fprintf(stderr, "Could not open destination file %s\n", video_dst_filename);
//			ret = 1;
//			goto end;
//		}

		/* allocate image where the decoded image will be put */
		ret = av_image_alloc(video_dst_data, video_dst_linesize,
				video_dec_ctx->width, video_dec_ctx->height,
				video_dec_ctx->pix_fmt, 1);
		if (ret < 0) {
			fprintf(stderr, "Could not allocate raw video buffer\n");
			goto end;
		}
		video_dst_bufsize = ret;
	}

	if (open_codec_context(&audio_stream_idx, fmt_ctx, AVMEDIA_TYPE_AUDIO) >= 0)
	{
		audio_stream = fmt_ctx->streams[audio_stream_idx];
		audio_dec_ctx = audio_stream->codec;
//{ //RAL
		//audio_dec_ctx->sample_fmt= AV_SAMPLE_FMT_S16;
//} //RAL
//		audio_dst_file = fopen(audio_dst_filename, "wb");
//		if (!audio_dst_file) {
//			fprintf(stderr, "Could not open destination file %s\n", video_dst_filename);
//			ret = 1;
//			goto end;
//		}
	}

	/* dump input information to stderr */
	av_dump_format(fmt_ctx, 0, src_filename, 0);
	if (!audio_stream && !video_stream)
	{
		fprintf(stderr, "Could not find audio or video stream in the input, aborting\n");
		ret = 1;
		goto end;
	}
	frame = avcodec_alloc_frame();
	if (!frame) {
		fprintf(stderr, "Could not allocate frame\n");
		ret = AVERROR(ENOMEM);
		goto end;
	}

	/* initialize packet, set data to NULL, let the demuxer fill it */
	av_init_packet(&pkt);
	pkt.data = NULL;
	pkt.size = 0;
//	if (video_stream)
//		printf("Demuxing video from file '%s' into '%s'\n", src_filename, video_dst_filename);
//	if (audio_stream)
//		printf("Demuxing audio from file '%s' into '%s'\n", src_filename, audio_dst_filename);

	/* read frames from the file */
	while(1) //RAL: //FIXME!!: added!!!
	{ //RAL: //FIXME!!: added!!!
	while (av_read_frame(fmt_ctx, &pkt) >= 0)
	{
LOGI("****frame!!\n"); //RAL: FIXME!!
#if 0
		AVPacket orig_pkt = pkt;
		do
		{
pthread_mutex_lock(&s_vsync_mutex); //RAL: FIXME!!
			//ret = decode_packet(&got_frame, 0);
pthread_mutex_unlock(&s_vsync_mutex); //RAL: FIXME!!
			if (ret < 0)
				break;
			pkt.data += ret;
			pkt.size -= ret;
		}
		while (pkt.size > 0);
		av_free_packet(&orig_pkt);
#endif
	}
	} //RAL: //FIXME!!: added!!!

	/* flush cached frames */
	pkt.data = NULL;
	pkt.size = 0;
	do {
		decode_packet(&got_frame, 1);
	} while (got_frame);
	printf("Demuxing succeeded.\n");
//	if (video_stream) {
//		printf("Play the output video file with the command:\n"
//				"ffplay -f rawvideo -pix_fmt %s -video_size %dx%d %s\n",
//				av_get_pix_fmt_name(video_dec_ctx->pix_fmt), video_dec_ctx->width, video_dec_ctx->height,
//				video_dst_filename);
//	}
//	if (audio_stream) {
//		enum AVSampleFormat sfmt = audio_dec_ctx->sample_fmt;
//		int n_channels = audio_dec_ctx->channels;
//		const char *fmt;
//		if (av_sample_fmt_is_planar(sfmt)) {
//			const char *packed = av_get_sample_fmt_name(sfmt);
//			printf("Warning: the sample format the decoder produced is planar "
//					"(%s). This example will output the first channel only.\n",
//					packed ? packed : "?");
//			sfmt = av_get_packed_sample_fmt(sfmt);
//			n_channels = 1;
//		}
//		if ((ret = get_format_from_sample_fmt(&fmt, sfmt)) < 0)
//			goto end;
//		printf("Play the output audio file with the command:\n"
//				"ffplay -f %s -ac %d -ar %d %s\n",
//				fmt, n_channels, audio_dec_ctx->sample_rate,
//				audio_dst_filename);
//	}
	end:
	if (video_dec_ctx)
		avcodec_close(video_dec_ctx);
	if (audio_dec_ctx)
		avcodec_close(audio_dec_ctx);
	avformat_close_input(&fmt_ctx);
//	if (video_dst_file)
//		fclose(video_dst_file);
//	if (audio_dst_file)
//		fclose(audio_dst_file);
	av_free(frame);
	av_free(video_dst_data[0]);
	return ret < 0;
}


#endif