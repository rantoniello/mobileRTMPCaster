#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h> //usleep
#include <pthread.h>
#include <stdint.h>

#include <x264.h>

#include "log.h"
#include "error_codes.h"
#include "rtmpclient.h"

#ifdef __APPLE__
#include "clock_gettime_stub.h"
#endif

/* Private prototypes */
static int add_stream(rtmpMuxerCtx* muxer, AVCodec **codec, enum AVCodecID codec_id, AVStream**);
static int open_audio(rtmpMuxerCtx* muxer, AVCodec *codec);
static void close_audio(rtmpMuxerCtx* muxer);
static int open_video(rtmpMuxerCtx* muxer, AVCodec *codec);
static void close_video(rtmpMuxerCtx* muxer);
static void get_parameters(rtmpMuxerCtx* muxer, const char *tag, const char *type, const char *val);
static int interleaved_write_frame(rtmpMuxerCtx* muxer, AVPacket** pkt);
static int interleaved_write_frame2(rtmpMuxerCtx* muxer, AVPacket** pkt);
static int write_frame(rtmpMuxerCtx* muxer, AVPacket* pkt);
#ifdef _DEBUG
#ifdef USE_FFMPEG_INTERLEAVE_BUFFER
	static void interleaving_buff_test(AVFormatContext *oc); // For testing
#endif
#endif
static void ffmpeg_log_callback(void *, int, const char *, va_list);
static void amf_message_event(const char* amf_msg);

/**
 *  Add an output stream; new stream is returned in AVStream** argument
 *  @return 0 if success, Error code if fails.
 */
static int add_stream(rtmpMuxerCtx* muxer, AVCodec **codec,
		enum AVCodecID codec_id, AVStream** retStream)
{
	AVFormatContext *oc= muxer->oc;
    AVCodecContext *c= NULL;
    AVStream *st= NULL;
    int ret= -1;

    /* Find the encoder */
    *codec= avcodec_find_encoder(codec_id);
    if(*codec== NULL)
    {
        LOGE("Could not find encoder for '%s'\n", avcodec_get_name(codec_id));
        ret= ENCODER_NOT_SUPPORTED_ERROR;
        goto end_add_stream;
    }

    /* Cretae new setram */
    st= avformat_new_stream(oc, *codec);
    if(st== NULL)
    {
        LOGE("Could not allocate stream\n");
        ret= STREAM_ALLOCATION_ERROR;
        goto end_add_stream;
    }
    st->id= oc->nb_streams-1;
    c= st->codec;

    switch((*codec)->type)
    {
    case AVMEDIA_TYPE_AUDIO:
    {
        //st->id= 1;
        c->sample_fmt = AV_SAMPLE_FMT_S16;
        c->bit_rate   = muxer->a_bit_rate;
        c->sample_rate= muxer->a_sample_rate;
        c->channels   = muxer->a_nb_channels;
        c->time_base  = (AVRational){1, c->sample_rate};
        //c->frame_size: let FFmpeg set automatically!!
//#ifndef _ANDROID_ // e.g. for iOS
//        c->frame_size= muxer->a_frame_size;
//#endif
        //(*codec)->capabilities:: let FFmpeg set automatically!!

        /* AAC case */
        if(c->codec_id== AV_CODEC_ID_AAC)
        {
        	c->profile= FF_PROFILE_AAC_LOW;
        }
        break;
    }
    case AVMEDIA_TYPE_VIDEO:
    {
        c->codec_id= muxer->v_codec_id;
        c->bit_rate= muxer->v_bit_rate;
        if(muxer->v_rc!= -1 && muxer->v_rc> 0) // video birate control
        {
            c->rc_max_rate= c->rc_min_rate= c->bit_rate;
            c->rc_buffer_size= (c->bit_rate*(muxer->v_rc))/100; //set "% of VBR"
        }
        c->width= muxer->v_width;
        c->height= muxer->v_height;
        c->sample_rate= muxer->v_frame_rate;
        c->time_base= (AVRational){1, muxer->v_frame_rate};
        st->avg_frame_rate= (AVRational){muxer->v_frame_rate, 1};
        st->r_frame_rate= (AVRational){muxer->v_frame_rate, 1};
        c->gop_size= muxer->v_gop_size;
        c->keyint_min= 3;
        c->pix_fmt= muxer->v_pix_format;

        /* H.264 case */
        if(c->codec_id== AV_CODEC_ID_H264)
        {
        	c->profile= FF_PROFILE_H264_BASELINE;
        	c->refs= 2; // max. number of reference frames
        	//av_opt_set(c->priv_data, "crf", "23", 0);
        	if(muxer->av_quality!= NULL)
        	{
        		av_opt_set(c->priv_data, "preset", muxer->av_quality, 0); // set quality
        	}
        }
        /* VP8 case */
        /* Set reasonable defaults to VP8, based on
           libvpx-720p preset from libvpx ffmpeg-patch */
        if(c->codec_id== AV_CODEC_ID_VP8)
        {
        	/* default to 120 frames between keyframe */
        	c->gop_size = 120;
            c->level = 216;
            c->profile = 0;
        }
        /* MPEG-2 case */
        if (c->codec_id == AV_CODEC_ID_MPEG2VIDEO)
        {
            /* add B frames */
            c->max_b_frames= 2;
        }
        /* MPEG-1 case */
        if (c->codec_id == AV_CODEC_ID_MPEG1VIDEO)
        {
            /* Needed to avoid using macroblocks in which some coeffs overflow.
             * This does not happen with normal video, it just happens here as
             * the motion of the chroma plane does not match the luma plane. */
            c->mb_decision = 2;
        }   
    }
    break;
    default:
    {
        break;
    }
    }

    /* Some formats want stream headers to be separate */
    if (oc->oformat->flags & AVFMT_GLOBALHEADER)
    {
    	c->flags|= CODEC_FLAG_GLOBAL_HEADER;
    }

    /* Success */
    ret= SUCCESS;

end_add_stream:
	if(ret!= SUCCESS) // error occurred
	{
		/*
		 * FFmpeg doc: "User is required to call avcodec_close() and
		 * avformat_free_context() to clean up the allocation by
		 * avformat_new_stream()".
		 */
		LOGI("Releasing stream (error occurred)...\n");
		if(st!= NULL)
		{
			if(st->codec!= NULL)
			{
				avcodec_close(st->codec);
				st->codec= NULL;
			}
			avformat_free_context(oc); // does not put "oc" to NULL
			st= NULL;
			oc= NULL;
		}
	} // error

	*retStream= st;
    return ret;
}

/**
 *  Open audio codec and allocates/initializes necessary resources.
 *  @return 0 if success, Error code if fails.
 */
static int open_audio(rtmpMuxerCtx* muxer, AVCodec *codec)
{
    int ret= -1; // ret< 0 if error; else O.K.
    AVStream *st= muxer->audio_st;
    AVCodecContext *c= st->codec;

    /* open audio codec */
    ret= avcodec_open2(c, codec, NULL);
    if(ret< 0)
    {
        LOGE("Could not open audio codec: %s\n", av_err2str(ret));
        return OPEN_AUDIO_CODEC_ERROR;
    }

    muxer->adata_size= c->frame_size;
    return SUCCESS;
}

/**
 *  Release resources allocated in 'open_audio()' call.
 */
static void close_audio(rtmpMuxerCtx* muxer)
{
}

/**
 *  Open video codec and allocates/initializes necessary resources.
 *  @return 0 if success, Error code if fails.
 */
static int open_video(rtmpMuxerCtx* muxer, AVCodec *codec)
{
    int ret= -1; // ret< 0 if error; else O.K.
    AVStream *st= muxer->video_st;
    AVCodecContext *c = st->codec;

    /* open the codec */
    ret= avcodec_open2(c, codec, NULL);
    if(ret< 0)
    {
        LOGE("Could not open video codec: %s\n", av_err2str(ret));
        ret= OPEN_VIDEO_CODEC_ERROR;
        goto end_open_video;
    }

    /* Allocate and init a video frame */
    muxer->vframe= av_frame_alloc();
    if(muxer->vframe== NULL)
    {
        LOGE("Could not allocate video frame\n");
        ret= VIDEO_FRAME_BUFFER_ALLOC_ERROR;
        goto end_open_video;
    }

    /*
     * Allocate the encoded raw picture.
     * FFmpeg doc.: "The allocated image buffer has to be freed by using
     * av_freep(&pointers[0])".
     */
    if(c->width> RTMPCLIENT_MAX_IMAGE_WIDTH ||
       c->height> RTMPCLIENT_MAX_IMAGE_HEIGTH
       )
    {
    	LOGE("Could not allocate picture: video resol. out of bounds\n");
    	ret= VIDEO_RESOLUTION_OUT_OF_BOUNDS_ERROR;
    	goto end_open_video;
    }

    AVPicture *picture= (AVPicture*)av_mallocz(sizeof(AVPicture));
    if(picture== NULL)
    {
        LOGE("Could not allocate picture\n");
        ret= VIDEO_FRAME_BUFFER_ALLOC_ERROR;
        goto end_open_video;
    }
    muxer->picture= picture;

    muxer->picture->data[0]= NULL;
    ret= avpicture_alloc(muxer->picture, c->pix_fmt, c->width, c->height);
    if(ret< 0)
    {
        LOGE("Could not allocate picture: %s\n", av_err2str(ret));
        ret= VIDEO_FRAME_BUFFER_ALLOC_ERROR;
        goto end_open_video;
    }

    /* Copy data and linesize picture pointers to frame */
    //*((AVPicture*)muxer->vframe)= *muxer->picture;
    {
    	int ndp;
    	for(ndp= 0; ndp< AV_NUM_DATA_POINTERS; ndp++)
    	{
    		muxer->vframe->data[ndp]= muxer->picture->data[ndp];
    		muxer->vframe->linesize[ndp]= muxer->picture->linesize[ndp];
    	}
    }

    /* Exit O.K.!! */
    ret= SUCCESS;

end_open_video:

    if(ret!= SUCCESS)
    {
    	close_video(muxer);
    }
    return ret;
}

/**
 *  Release resources allocated in 'open_video()' call.
 */
static void close_video(rtmpMuxerCtx* muxer)
{
	if(muxer->picture)
	{
		if(muxer->picture->data[0]!= NULL)
		{
			av_free(muxer->picture->data[0]);
			muxer->picture->data[0]= NULL;
		}
		av_freep(muxer->picture);
		muxer->picture= NULL;
	}

	if(muxer->vframe!= NULL)
	{
		av_frame_free(&muxer->vframe);
		muxer->vframe= NULL;
	}
}

/**
 * Initializes RTMP client context. Call it only once before using any other
 * method of client.
 */
void rtmpclient_open(rtmpMuxerCtx* muxer)
{
	LOGI("'rtmpclient_open()'...\n");

	/* Initialize 'rtmpMuxerCtx' context */
	muxer->oc= NULL;
	pthread_mutex_init(&muxer->write_mutex, NULL);
	pthread_mutex_init(&muxer->audio_mutex, NULL);
	pthread_mutex_init(&muxer->video_mutex, NULL);
	pthread_mutex_init(&muxer->bckground_mutex, NULL);
	muxer->inBackground= 0;
	muxer->bckground_width= 352; muxer->bckground_height= 288;
	muxer->bckgroundLogo= NULL;
	muxer->doInterleave= 1;

	/* video */
	muxer->v_available= 1;
	muxer->video_st= NULL;
	muxer->picture= NULL;
	muxer->vframe= NULL;
	// video settings
	muxer->v_bit_rate= 300*1024;
	muxer->v_rc= -1;
	muxer->v_frame_rate= 15;
	muxer->v_width= 352; muxer->v_height= 288;
	muxer->v_gop_size= 15;
	muxer->v_pix_format= AV_PIX_FMT_YUV420P;
	muxer->v_codec_id= AV_CODEC_ID_H264;
	strcpy(muxer->av_quality, "ultrafast");

	/* audio */
	muxer->a_available= 1;
	muxer->audio_st= NULL;
	muxer->adata= NULL;
	muxer->adata_size= 0;
	muxer->aframe= NULL;
	// Circular Interleaving buffer algorithm
	muxer->af= 0; // circular buffer "front pointer"
	muxer->ab= 0; // circular buffer "back pointer"
	muxer->level= 0; // circular buffer level
	muxer->alevel= 0; // circular audio pkts buffer level
	muxer->vlevel= 0; // circular video pkts buffer level	
	// audio settings
	muxer->a_bit_rate= 64*1024;
	muxer->a_sample_rate= 24000;
	muxer->a_nb_channels= 1; // mono by default
    muxer->a_frame_size= 1024;
	muxer->a_codec_id= AV_CODEC_ID_AAC;

	/* Container setting */
	muxer->uri= NULL;
	muxer->o_mux_format= "flv"; //"mpegts"; //"flv"; //RAL: temporal: FIXME!!

	/* On error callback */
	muxer->on_error= NULL;
	muxer->error_code= SUCCESS;
	muxer->persistant_error_usecs_limit= 
			PERSISTANT_ERROR_TOLERANCE_WFRAME_USECS; // default value
	muxer->persistant_error_usecs= 0;
}

/**
 * Finally de-initialize RTMP client. Call it at the very end of application.
 */
void rtmpclient_close(rtmpMuxerCtx* muxer)
{
	LOGI("'rtmpclient_close()'...\n");
	pthread_mutex_destroy(&muxer->write_mutex);
	pthread_mutex_destroy(&muxer->audio_mutex);
	pthread_mutex_destroy(&muxer->video_mutex);
	pthread_mutex_destroy(&muxer->bckground_mutex);
}

/**
 *  Fully initializes RTMP client.
 *  Previously, client context structure 'rtmpMuxerCtx' (passed as argument)
 *  must be initialized.
 *  @param muxer Previously initialized 'rtmpMuxerCtx' context structure.
 *  @return 0 if success, Error code if fails.
 */
int rtmpclient_init(rtmpMuxerCtx* muxer)
{
    int ret= -1; // ret<0 is error; else is OK
    AVFormatContext **oc= &muxer->oc;
    AVOutputFormat *fmt= NULL;
    AVCodec *audio_codec= NULL, *video_codec= NULL;

    /* Initialize libavcodec, and register all codecs and formats. */
    //av_register_all();

    /* Initialize log callback */
    //void av_log_set_callback( 	void(*)(void *, int, const char *, va_list)  	callback	);
    av_log_set_callback(ffmpeg_log_callback);

    /* Initialize networking. */
    avformat_network_init();

    /* allocate the output media context */
    avformat_alloc_output_context2(oc, NULL, muxer->o_mux_format, muxer->uri);
    if(*oc== NULL)
    {
        LOGE("Could not deduce output format from file extension\n");
        ret= MUX_FORMAT_NOT_SUPPORTED;
        goto end_rtmpclient_init;
    }
    fmt= (*oc)->oformat;
    //(*oc)->flags|= AVFMT_FLAG_SORT_DTS;
    //LOGI("FormatcontexFlags: 0x%0x\n", (*oc)->flags);
    fmt->video_codec= muxer->v_available? muxer->v_codec_id: AV_CODEC_ID_NONE;
    fmt->audio_codec= muxer->a_available? muxer->a_codec_id: AV_CODEC_ID_NONE;
    (*oc)->nb_streams= 0;
    //(*oc)->max_delay= 1000*200; // in usecs
    //(*oc)->max_picture_buffer= 4096;

    /* get monotonic clock time */
    clock_gettime(CLOCK_MONOTONIC, &muxer->monotime_init);
    {
    /* Obtain initial time */
    AVRational monotime_rate= {1, 1000000 }; //[usec]
    uint64_t t=
        ((uint64_t)muxer->monotime_init.tv_sec*1000000+ (uint64_t)muxer->monotime_init.tv_nsec/1000); //[usec]

	AVRational av_time_base_q= (AVRational){1, AV_TIME_BASE};
    (*oc)->start_time=
    		av_rescale_q(t, monotime_rate, av_time_base_q);
	}

    /* Add the audio and video streams using the default format codecs
     * and initialize the codecs. */
    muxer->video_st= NULL;
    if(fmt->video_codec!= AV_CODEC_ID_NONE)
    {
    	int retcode= add_stream(muxer, &video_codec, fmt->video_codec, &muxer->video_st);
        if(retcode!= SUCCESS || muxer->video_st== NULL)
        {
        	ret= (retcode== ENCODER_NOT_SUPPORTED_ERROR)?
        	     VIDEO_ENCODER_NOT_SUPPORTED_ERROR:
        		 ADD_VIDEO_STREAM_ERROR;
        	goto end_rtmpclient_init;
        }
    }
    muxer->audio_st= NULL;
    if(fmt->audio_codec!= AV_CODEC_ID_NONE)
    {
    	int retcode= add_stream(muxer, &audio_codec, fmt->audio_codec, &muxer->audio_st);
        if(retcode!= SUCCESS || muxer->audio_st== NULL)
        {
        	ret= (retcode== ENCODER_NOT_SUPPORTED_ERROR)?
        	     AUDIO_ENCODER_NOT_SUPPORTED_ERROR:
        	     ADD_AUDIO_STREAM_ERROR;
        	goto end_rtmpclient_init;
        }
    }

    /* Now that all the parameters are set, we can open the audio and
     * video codecs and allocate the necessary encode buffers. */
    if(muxer->video_st)
    {
    	int retcode= open_video(muxer, video_codec);
    	if(retcode< SUCCESS)
    	{
    		ret= retcode;
    		goto end_rtmpclient_init;
    	}
    }
    if(muxer->audio_st)
    {
    	int retcode= open_audio(muxer, audio_codec);
        if(retcode< SUCCESS)
        {
        	ret= retcode;
        	goto end_rtmpclient_init;
        }
    }

    av_dump_format(*oc, 0, muxer->uri, 1);

    /* open the output file, if needed */
    if(!(fmt->flags & AVFMT_NOFILE))
    {
        int retcode= avio_open(&(*oc)->pb, muxer->uri, AVIO_FLAG_READ_WRITE); //AVIO_FLAG_WRITE
        if(retcode< 0)
        {
            LOGE("Could not open '%s': %s\n", muxer->uri, av_err2str(retcode));
            ret= OPEN_CONNECTION_OR_OFILE_ERRROR;
            goto end_rtmpclient_init;
        }
    }

    /* Write the stream header, if any. */
    {
    	int retcode= avformat_write_header(*oc, NULL);
		if(retcode< 0)
		{
			LOGE("Error occurred when opening output file: %s\n", av_err2str(retcode));
			ret= SENDING_DATA_OR_WRITING_ERROR;
			goto end_rtmpclient_init;
		}
    }

#ifdef _DEBUG //RAL: debugging purposes 
	LOGI("* OC offset_timebase.num: %d; offset_timebase.den: %d\n",
		(*oc)->offset_timebase.num, (*oc)->offset_timebase.den);

    if(muxer->audio_st)
    {
    	LOGI("* AUDIO c->time_base.den: %d; st->time_base.den: %d\n",
    		muxer->audio_st->codec->time_base.den,
    		muxer->audio_st->time_base.den);
    	LOGI("* AUDIO c->frame_size: %d\n", muxer->audio_st->codec->frame_size);
    }
    if(muxer->video_st)
    {
    	LOGI("* VIDEO c->time_base.den: %d; st->time_base.den: %d\n",
    		muxer->video_st->codec->time_base.den,
    		muxer->video_st->time_base.den);
    }
#endif

    /* Success */
    LOGI("Init O.K.!\n");
    ret= SUCCESS;

end_rtmpclient_init:
	if(ret!= SUCCESS) // error case, release resources
	{
		/*
		 * FFmpeg doc: avformat_free_context() can be used to free the context
		 * and everything allocated by the framework within it.
		 */
		//if(*oc!= NULL)
		//{
		//	avformat_free_context(*oc);
		//	*oc= NULL;
		//}
		rtmpclient_release(&muxer->oc, muxer);
	}
    return ret;
}

/**
 *  Encodes video frame.
 *  Previously, RTMP client must be successfully initialized by calling
 *  'rtmpclient_init()' method.
 *  @param muxer Previously initialized 'rtmpMuxerCtx' context structure.
 *  @param t Timestamp for this video frame
 *  @return 0 if success, Error code if fails.
 */
int rtmpclient_encode_vframe(rtmpMuxerCtx* muxer, uint64_t t)
{
	int ret= ERROR; // ret< 0 error; else OK
	AVStream *st= muxer->video_st;
	AVCodecContext *c= st->codec;
	int got_output= 0;
	AVPacket *pkt= (AVPacket *)av_mallocz(sizeof(AVPacket));
	int64_t prev_vframe_pts= muxer->vframe->pts;

	/* Init video packet */
	av_init_packet(pkt);
	pkt->data = NULL;    // packet data will be allocated by the encoder
	pkt->size = 0;

    /* Obtain frame pts in codec time base (~ frame number) */
    AVRational monotime_rate= {1, 1000000 }; //[usec]
    muxer->vframe->pts=
    		av_rescale_q(t, monotime_rate, c->time_base);

	/* Hack to avoid non-strictly-monotonic PTS/DTS problems */
    if(prev_vframe_pts>= muxer->vframe->pts)
    {
    	muxer->vframe->pts++;
    }
//#ifdef _DEBUG
    //LOGI("**** prev. v PTS: %lld; curr. v PTS: %lld\n",
    //		prev_vframe_pts, muxer->vframe->pts);
//#endif

    /* encode the image */
	ret= avcodec_encode_video2(c, pkt, muxer->vframe, &got_output);
	if(ret< 0)
	{
		LOGE("Error encoding video frame: %s\n", av_err2str(ret));
		ret= VIDEO_FRAME_ENCODING_ERROR;
		goto end_rtmpclient_encode_vframe;
	}

	/* If size is zero, it means the image was buffered. */
	if(got_output== 0)
	{
		LOGI("No video output!! (Image may be buffered)\n");
	}
	else
	{
		/* Signal Key frame if it is the case */
		if (c->coded_frame->key_frame) pkt->flags|= AV_PKT_FLAG_KEY;

		/* Restore PTSs/DTSs time-base to stream base */
		pkt->pts = av_rescale_q(pkt->pts, c->time_base, st->time_base);
		pkt->dts = av_rescale_q(pkt->dts, c->time_base, st->time_base);

		/* Set video stream index */
		pkt->stream_index = st->index;

		/* Write the compressed frame to the media file. */
#ifdef _DEBUG
#ifdef USE_FFMPEG_INTERLEAVE_BUFFER
		interleaving_buff_test(muxer->oc); //debugging purposes
#endif
#endif
		ret= interleaved_write_frame(muxer, &pkt);
		if(ret!= SUCCESS)
		{
			if(ret!= RETRY)
			{
				LOGE("Error while writing video frame: %s\n", av_err2str(ret));
				ret= SENDING_DATA_OR_WRITING_ERROR;
			}
			goto end_rtmpclient_encode_vframe;
		}
	}

	/* Success */
	ret= SUCCESS;

end_rtmpclient_encode_vframe:

	if(pkt!= NULL)
	{
		av_free_packet(pkt);
		free(pkt);
		pkt= NULL;
	}
    return ret;
}

/**
 *  Encodes audio frame.
 *  Previously, RTMP client must be successfully initialized by calling
 *  'rtmpclient_init()' method.
 *  @param muxer Previously initialized 'rtmpMuxerCtx' context structure.
 *  @param t Timestamp for this audio frame
 *  @return 0 if success, Error code if fails.
 */
int rtmpclient_encode_aframe(rtmpMuxerCtx* muxer, uint64_t t)
{
	int ret= ERROR;
	AVStream *st= muxer->audio_st;
	AVCodecContext *c= st->codec;
	AVPacket *pkt= (AVPacket *)av_mallocz(sizeof(AVPacket));
	int got_packet= 0;
	AVFrame *aframe= av_frame_alloc();

	/* Init audio packet */
	av_init_packet(pkt);
    pkt->data= NULL;    // packet data will be allocated by the encoder
    pkt->size= 0;

    /* Allocate and init input audio frame */
    //aframe->format= c->sample_fmt;
    //aframe->channel_layout= c->channel_layout;
	aframe->nb_samples= muxer->adata_size;
	avcodec_fill_audio_frame(aframe, c->channels, c->sample_fmt,
							 (uint8_t *)muxer->adata,
							 muxer->adata_size *
							 av_get_bytes_per_sample(c->sample_fmt) *
							 c->channels, 1);

	/* Obtain frame pts in codec time base (~ frame number) */
	AVRational monotime_rate= {1, 1000000 }; //[usec]
	aframe->pts= aframe->pkt_dts=
			av_rescale_q(t, monotime_rate, c->time_base);

	/* Encode audio frame */
	ret= avcodec_encode_audio2(c, pkt, aframe, &got_packet);
	if(ret< 0)
	{
		LOGE("Error encoding audio aframe: %s\n", av_err2str(ret));
		ret= AUDIO_FRAME_ENCODING_ERROR;
		goto end_rtmpclient_encode_aframe;
	}

	if(!got_packet)
	{
		LOGI("No audio output!!\n");
		ret= SUCCESS;
		goto end_rtmpclient_encode_aframe;
	}

	/* Restore PTSs/DTSs time-base to stream base; update stream index */
	pkt->pts= av_rescale_q(pkt->pts, c->time_base, st->time_base);
	pkt->dts= av_rescale_q(pkt->dts, c->time_base, st->time_base);
	pkt->stream_index = st->index;

	/* Write the compressed audio frame to the media file */
	ret= interleaved_write_frame(muxer, &pkt);
	if(ret!= SUCCESS)
	{
		if(ret!= RETRY)
		{
			LOGE("Error while writing audio frame\n");
			ret= SENDING_DATA_OR_WRITING_ERROR;
		}
		goto end_rtmpclient_encode_aframe;
	}

	/* Success */
	ret= SUCCESS;

end_rtmpclient_encode_aframe:

	if(aframe!= NULL)
	{
		av_frame_free(&aframe);
		aframe= NULL;
	}
	if(pkt!= NULL)
	{
		av_free_packet(pkt);
		free(pkt);
		pkt= NULL;
	}
	return ret;
}

/**
 *  Releases all RTMP client allocated resources.
 *  RTMP client must be released always by calling this method (client is
 *  supposed to be previously initialized/allocated by calling
 *  'rtmpclient_init()' method).
 */
void rtmpclient_release(AVFormatContext **oc, rtmpMuxerCtx* muxer)
{
	unsigned int i;

	LOGI("Releasing resources...\n");
	if((*oc)== NULL)
	{
		LOGW("'rtmpclientRelease()' already called\n");
		return;
	}

    /* Write the trailer, if any. The trailer must be written before you
     * close the CodecContexts open when you wrote the header; otherwise
     * av_write_trailer() may try to use memory that was freed on
     * av_codec_close(). */
	/*if((*oc)!= NULL)
	{
		av_write_trailer((*oc));
	}*/

	/* Check if it is running in background; release if it is the case */
    if(muxer->inBackground!= 0)
    {
    	LOGI("Muxer was running in background, releasing...");
    	rtmp_client_stopBackground(muxer);
    }

    if(muxer->bckgroundLogo!= NULL)
    {
    	free(muxer->bckgroundLogo);
    	muxer->bckgroundLogo= NULL;
    }

    /* Close each codec. */
    for(i= 0; i< (*oc)->nb_streams; i++){
    	if((*oc)->streams[i] && (*oc)->streams[i]->codec)
    	{
    		avcodec_close((*oc)->streams[i]->codec);
		}
	}

    if (!((*oc)->oformat->flags & AVFMT_NOFILE))
    {
		/* Close the output file. */
		avio_close((*oc)->pb);
    }

    /* free the stream */
    avformat_free_context((*oc));
    (*oc)= NULL;

    /* Free A/V buffers */
    close_video(muxer);
    close_audio(muxer);

	/* Free other muxer context resources */
	if(muxer->uri!= NULL)
	{
		free(muxer->uri);
		muxer->uri= NULL;
	}

	LOGI("Resources released.\n");
}

static void *thrBackgroundVideo(void *t)
{
	LOGI("'thrBackgroundVideo()' ...\n");
    rtmpMuxerCtx *muxer= (rtmpMuxerCtx*)t;
    struct SwsContext *img_convert_ctx= NULL;

    /* Sanity check */
    if(muxer->picture== NULL || muxer->picture->data[0]== NULL)
    {
    	LOGI("Error: picture buffer is not still initialized!\n");
    	goto exit_thrBackgroundVideo;
    }

    /* We will now sclae background still image */
	int src_w= muxer->bckground_width;
	int src_h= muxer->bckground_height;
	int dst_w= muxer->v_width;
	int dst_h= muxer->v_height;
	int pix_fmt= muxer->v_pix_format;

	/* Allocate scaling context structure (FFmpeg) */
    if(img_convert_ctx== NULL)
    {
    	LOGD("Creating scaling context...\n");
    	img_convert_ctx= sws_getCachedContext(img_convert_ctx, 
    			src_w, src_h, pix_fmt,
    			dst_w, dst_h, pix_fmt, 
    			SWS_FAST_BILINEAR, NULL, NULL, NULL);
    	if(img_convert_ctx== NULL) //FFmpeg: 
    	{
    		LOGE("Cannot initialize the conversion context!\n");
    		goto exit_thrBackgroundVideo;
    	}
    }

    /* Allocate new fixed raw picture for background */
    LOGD("Creating source picture (w: %d h: %d)\n", src_w, src_h);
    AVPicture src_picture;
    src_picture.data[0]= NULL;
    if(avpicture_alloc(&src_picture, pix_fmt, src_w, src_h)< 0)
    {
        LOGE("Could not allocate source picture data\n");
        goto exit_thrBackgroundVideo;
    }

    /* Fill background raw picture YUV buffers */
    LOGD("Filling source picture...\n");
    unsigned int Y_size= src_w*src_h;
    unsigned int C_size= Y_size>>2;
    const char* framebuffer= muxer->bckgroundLogo;
	memcpy(src_picture.data[0], framebuffer, Y_size);
	memcpy(src_picture.data[1], &framebuffer[Y_size], C_size);
	memcpy(src_picture.data[2], &framebuffer[Y_size+C_size], C_size);

	/* Scale... */
	LOGD("Starting scaling...\n");
    sws_scale(img_convert_ctx, (uint8_t const * const*)src_picture.data, src_picture.linesize,
    		0, src_h, muxer->picture->data, muxer->picture->linesize);
    LOGD("Scaling O.K.\n");

	while(muxer->inBackground)
	{
	    /* Simulate real XX fps source */
	    usleep((1000*1000)/muxer->v_frame_rate);

	    /* get monotonic clock time */
	    uint64_t t= get_monotonic_clock_time(muxer);

	    /* Encode Picture! */
		rtmpclient_encode_vframe(muxer, t);
	}

exit_thrBackgroundVideo:

	/* Release sclaing context */
	if(img_convert_ctx!= NULL)
	{
		sws_freeContext(img_convert_ctx);
		img_convert_ctx= NULL;
	}

	/* Release allocated picture */
	if(src_picture.data[0]!= NULL)
	{
		av_free(src_picture.data[0]);
		src_picture.data[0]= NULL;
	}

    /* Release thread */
    LOGI("'thrBackgroundVideo()' ends successfuly\n");
	pthread_exit(NULL);
}

void rtmp_client_runBackground(rtmpMuxerCtx* muxer)
{
	LOGI("'rtmp_client_runBackground()' ...\n");

	/* Set background flag to 'true' */
	muxer->inBackground= 1;

	/* Create a/v threads in a joinable state */
	pthread_attr_init(&muxer->bckgroundAttr);
	pthread_attr_setdetachstate(&muxer->bckgroundAttr, PTHREAD_CREATE_JOINABLE);
	pthread_create(&muxer->bckgroundThread, &muxer->bckgroundAttr, thrBackgroundVideo, (void *)muxer);
	LOGI("'rtmp_client_runBackground()' successfully created thread...\n");
}

void rtmp_client_stopBackground(rtmpMuxerCtx* muxer)
{
	LOGI("'stopBackground()' ...\n");

	if(muxer->inBackground!= 0) muxer->inBackground= 0;

	/* Wait for all threads to complete */
	LOGI("Waiting on all threads to join...\n");
	pthread_join(muxer->bckgroundThread, NULL);
	LOGI("Waited on all threads to join O.K.!\n");

	/* Clean up and exit */
	pthread_attr_destroy(&muxer->bckgroundAttr);
	LOGI("'stopBackground()' finished O.K.\n");
}

/**
 *  This method is intended to be auxiliary for initializing some of the
 *  parameters of the context structure 'rtmpMuxerCtx'. Those parameters
 *  of the mentioned structure not initialized here, must be set outside
 *  this method.
 *  All initialization values are passed by argument in the form of a single
 *  JSON string or "command".
 *  @param muxer Previously partially initialized 'rtmpMuxerCtx' context
 *  structure.
 *  @param cmd Single JSON string with initialization parameters.
 *  @return 0 if success, Error code if fails.
 */
int cmd_parser(rtmpMuxerCtx* muxer, const char* cmd)
{
	int ret= JSON_COMMAND_PARSER_ERROR; // error return code set by default
	char *pos1, *pos2;
	// parse commands of the type(Json):
	// [{"tag":"mycmd1","type":"string","val":"testing"},{"tag":"mycmd2","type":"int","val":200}]
	LOGI("'cmd_parser()' input: '%s'\n", cmd);
	if(cmd[0]!= '[') goto cmd_parser_end; // error

	pos2= (char*)cmd;
	while((pos2!= NULL) && (pos2-cmd< strlen(cmd)) && (pos2[1]!= ']'))
	{
		int i;
		char *substring, *end_substring;
		char atag[JSON_PARAMETER_SIZE];
		char atype[JSON_PARAMETER_SIZE];
		char aval[JSON_PARAMETER_SIZE];

		// Find parameter set
		pos2++;
		pos1= strchr(pos2, '{');
		pos2= strchr(pos2, '}');
		if(pos2-cmd>= strlen(cmd)) break;

		// Match TAG, TYPE and VAL
		const char *json_scan[JSON_SCAN_SIZE][2]=
		{
			{"\"tag\":\"", atag},
			{"\"type\":\"", atype},
			{"\"val\":\"", aval}
		};
		for(i= 0; i< JSON_SCAN_SIZE; i++)
		{
			substring= strstr((const char*)pos1, json_scan[i][0]);
			//if(substring) LOGD("substring-cmd: '%d'\n", substring-cmd);
			if(substring== NULL || substring> pos2)
			{LOGD("'cmd_parser()' '%s' start pointer error\n", json_scan[i][0]); goto cmd_parser_end;} // error
			substring+= strlen(json_scan[i][0]);
			end_substring= strchr(substring, '\"');
			if(end_substring== NULL || end_substring> pos2)
			{LOGD("'cmd_parser()' '%s' end pointer error\n", json_scan[i][0]); goto cmd_parser_end;} // error
			if(end_substring-substring> JSON_PARAMETER_SIZE)
			{LOGD("'cmd_parser()' '%s' size error\n", json_scan[i][0]); goto cmd_parser_end;} // error
			strncpy((char*)json_scan[i][1], (const char*)substring, end_substring-substring);
			((char*)json_scan[i][1])[end_substring-substring]= '\0';
		}
		LOGI("tag: '%s'; type: '%s'; val: '%s'\n", atag, atype, aval);
		get_parameters(muxer, atag, atype, aval);
	}
	/* Success */
	ret= SUCCESS;

cmd_parser_end:
	if(ret!= SUCCESS)
	{
		LOGE("Syntax error found in command string\n");
	}
	return ret;
}

/**
 *  Auxiliary method.
 *  Please refer to 'cmd_parser()' method.
 */
static void get_parameters(rtmpMuxerCtx* muxer,
		const char *tag, const char *type, const char *val)
{
	if(strcmp(tag, "streaming_url")== 0)
	{
		muxer->uri= strdup(val);
		LOGD("Streaming URL: %s\n", muxer->uri);
	}
	else if(strcmp(tag, "v_width")== 0)
	{
		muxer->v_width= atoi(val);
		LOGD("Video width: %d\n", muxer->v_width);
	}
	else if(strcmp(tag, "v_height")== 0)
	{
		muxer->v_height= atoi(val);
		LOGD("Video height: %d\n", muxer->v_height);
	}
	else if(strcmp(tag, "v_frame_rate")== 0)
	{
		muxer->v_frame_rate= atoi(val);
		LOGD("Video frame rate: %d\n", muxer->v_frame_rate);
	}
	else if(strcmp(tag, "v_bit_rate")== 0)
	{
		muxer->v_bit_rate= atoi(val);
		LOGD("Video bit rate: %d\n", muxer->v_bit_rate);
	}
	else if(strcmp(tag, "v_rc")== 0)
	{
		muxer->v_rc= atoi(val);
		LOGD("Video rate control: %d[percentage]\n", muxer->v_rc);
	}
	else if(strcmp(tag, "v_gop_size")== 0)
	{
		muxer->v_gop_size= atoi(val);
		LOGD("Video GOP size: %d\n", muxer->v_gop_size);
	}
	else if(strcmp(tag, "v_codec_id")== 0)
	{
		if(strcmp(val, "h264")== 0)  muxer->v_codec_id= AV_CODEC_ID_H264;
		else if(strcmp(val, "vp8")== 0)  muxer->v_codec_id= AV_CODEC_ID_VP8;
		else if(strcmp(val, "flv1")== 0)  muxer->v_codec_id= AV_CODEC_ID_FLV1;
		else muxer->v_codec_id= AV_CODEC_ID_H264; // H.264 by default
		LOGD("Video codec Id: %d\n", muxer->v_codec_id);
	}
	else if(strcmp(tag, "av_quality")== 0)
	{
		int i;
		const char *qualities[][2]= {
				{"q1", "ultrafast"}, {"q2", "superfast"}, {"q3", "veryfast"},
				{NULL, NULL}
		};
		// decode quality
		strcpy(muxer->av_quality, "ultrafast"); // default
		for(i= 0; qualities[i][0]!= NULL; i++)
		{
			if(strcmp(val, qualities[i][0])== 0)
			{
				strcpy(muxer->av_quality, qualities[i][1]);
			}
		}
		LOGD("A/V quality is : %s\n", val);
	}
	else if(strcmp(tag, "a_bit_rate")== 0)
	{
		muxer->a_bit_rate= atoi(val);
		LOGD("Audio bit rate: %d\n", muxer->a_bit_rate);
	}
	else if(strcmp(tag, "a_sample_rate")== 0)
	{
		muxer->a_sample_rate= atoi(val);
		LOGD("Audio sample rate: %d\n", muxer->a_sample_rate);
	}
    else if(strcmp(tag, "a_channel_num")== 0)
    {
        muxer->a_nb_channels= atoi(val);
        LOGD("Audio number of channels: %d\n", muxer->a_nb_channels);
    }
    else if(strcmp(tag, "a_frame_size")== 0)
    {
        muxer->a_frame_size= atoi(val);
        LOGD("Audio frame-size: %d\n", muxer->a_frame_size);
    }
	else if(strcmp(tag, "a_codec_id")== 0)
	{
		if(strcmp(val, "aac")== 0)  muxer->a_codec_id= AV_CODEC_ID_AAC;
		else if(strcmp(val, "mp3")== 0)  muxer->a_codec_id= AV_CODEC_ID_MP3;
		else muxer->a_codec_id= AV_CODEC_ID_AAC; // by default
		LOGD("Audio codec Id: %d\n", muxer->a_codec_id);
	}
}

/* get monotonic clock time in microseconds */
uint64_t get_monotonic_clock_time(rtmpMuxerCtx* muxer)
{
	struct timespec monotime_curr;
	clock_gettime(CLOCK_MONOTONIC, &monotime_curr);
	uint64_t t=
			((uint64_t)monotime_curr.tv_sec*1000000+ (uint64_t)monotime_curr.tv_nsec/1000)-
			((uint64_t)muxer->monotime_init.tv_sec*1000000+ (uint64_t)muxer->monotime_init.tv_nsec/1000); //[usec]
	return t;
}

/* Write buffer interleaving methods */
static int interleaved_write_frame(rtmpMuxerCtx* muxer, AVPacket** pkt)
{
	int ret;
	pthread_mutex_lock(&muxer->write_mutex);
	ret= interleaved_write_frame2(muxer, pkt);
	pthread_mutex_unlock(&muxer->write_mutex);
	return ret;
}

static int interleaved_write_frame2(rtmpMuxerCtx* muxer, AVPacket** pkt)
{
#define WRITE_BUF_FLUSH_SIZE 	3
	int ret= ERROR;
	int idx= 0;
	AVPacket *opkt= NULL; //output packet
	AVPacket **insert_pkt= &muxer->apkt_aray[muxer->af]; // points to the front of buffer '+1' by default

	/* Do not interleave if: */
	if( (muxer->audio_st== NULL) || (muxer->video_st== NULL) || // only audio or only video
		(muxer->inBackground!= 0) || // is in background (video is a dummy still image...)
	    (muxer->doInterleave!= 1) )
	{
		/* Flush write buffer */ //FIXME!!
		while
		(
			/* (muxer->vlevel> WRITE_BUF_FLUSH_SIZE) && */
			(muxer->ab!= muxer->af) && (muxer->level> 0) // sanity check
		)
		{
			/* Get the packet at the beginning of the buffer
			 * (we use array-back 'muxer->ab' index).
			 * This packet is the oldest, as these are orderer increasingly 
			 * regarding to DTS value.
			 * */
			opkt= muxer->apkt_aray[muxer->ab];
			if(opkt== NULL)
			{
				LOGE("Error on circular interleaving buffer\n");
				ret= INTERLEAVING_ERROR;
				goto end_interleaved_write_frame;
			}
			muxer->apkt_aray[muxer->ab]= NULL;
			
			/* Increment circular "back-buffer" index, decrement buffer levels
			 * */
			muxer->ab++; muxer->ab&= (MOD_SIZE-1); // circular
			muxer->level--;
			if(opkt->stream_index== muxer->audio_st->index) muxer->alevel--;
			if(opkt->stream_index== muxer->video_st->index) muxer->vlevel--;

			/* Write the compressed frame to the media file. */
	#ifdef _DEBUG
			LOGI("* PACKET (%s) opkt->dts: %lld\n",
					(opkt->stream_index== muxer->video_st->index)? "V": "X",
					opkt->dts);
			LOGI("* INTERLEAVE LEVEL: %u (a: %u; v: %u; buf-back: %u)\n", 
					muxer->level, muxer->alevel, muxer->vlevel, muxer->ab);
	#endif
			ret= write_frame(muxer, opkt);
			if(ret!= 0)
			{
				//LOGE("Error while writing output frame: %s\n", av_err2str(ret));
				ret= SENDING_DATA_OR_WRITING_ERROR;
				goto end_interleaved_write_frame;
			}
		} // while (flush write buffer)

		ret= write_frame(muxer, (*pkt) );
		if(ret!= 0)
		{
			//LOGE("Error while writing output frame: %s\n", av_err2str(ret));
			ret= SENDING_DATA_OR_WRITING_ERROR;
		}
		else ret= SUCCESS;
		goto end_interleaved_write_frame;
	}

	/* Wait for video sequence */
	if(
	   (*pkt) &&
	   ((*pkt)->stream_index== muxer->audio_st->index) && // if audio pkt.
	   (muxer->vlevel== 0) // no video yet
	)
	{
		LOGD("Wait video seq. for starting buffering...\n");
		ret= RETRY;
		goto end_interleaved_write_frame;
	}

	/* Flush write buffer */
	//LOGV("vlevel: %d > WRITE_BUF_FLUSH_SIZE: %d\n", muxer->vlevel, WRITE_BUF_FLUSH_SIZE); //comment-me
	//LOGV("level: %d > 0\n", muxer->level); //comment-me
	while
	(
		(muxer->vlevel> WRITE_BUF_FLUSH_SIZE) &&
		(muxer->ab!= muxer->af) && (muxer->level> 0) // sanity check
	)
	{
		/* Get the packet at the beginning of the buffer
		 * (we use array-back 'muxer->ab' index).
		 * This packet is the oldest, as these are orderer increasingly 
		 * regarding to DTS value.
		 * */
		opkt= muxer->apkt_aray[muxer->ab];
		if(opkt== NULL)
		{
			LOGE("Error on circular interleaving buffer\n");
			ret= INTERLEAVING_ERROR;
			goto end_interleaved_write_frame;
		}
		muxer->apkt_aray[muxer->ab]= NULL;
		
		/* Increment circular "back-buffer" index, decrement buffer levels
		 * */
		muxer->ab++; muxer->ab&= (MOD_SIZE-1); // circular
		muxer->level--;
		if(opkt->stream_index== muxer->audio_st->index) muxer->alevel--;
		if(opkt->stream_index== muxer->video_st->index) muxer->vlevel--;

		/* Write the compressed frame to the media file. */
#ifdef _DEBUG
		LOGI("* PACKET (%s) opkt->dts: %lld\n",
				(opkt->stream_index== muxer->video_st->index)? "V": "X",
				opkt->dts);
		LOGI("* INTERLEAVE LEVEL: %u (a: %u; v: %u; buf-back: %u)\n", 
				muxer->level, muxer->alevel, muxer->vlevel, muxer->ab);
#endif
		ret= write_frame(muxer, opkt);
		if(ret!= 0)
		{
			//LOGE("Error while writing output frame: %s\n", av_err2str(ret));
			ret= SENDING_DATA_OR_WRITING_ERROR;
			goto end_interleaved_write_frame;
		}
        // release after using packet
        if(opkt!= NULL)
        {
            av_free_packet(opkt);
            free(opkt);
            opkt= NULL;
        }
	} // while (flush write buffer)

	/* Check if buffer is to overflow with new packet */
	if((muxer->level+1)>= MOD_SIZE)
	{
		LOGE("Error: interleaving buffer overflow; pkt will be discarded\n");
		ret= INTERLEAVING_BUF_OVERFLOW_ERROR;
		goto end_interleaved_write_frame;

	}

	if((*pkt)== NULL) // sanity check
	{
		LOGD("Input packet is empty, will not be written!\n");
		ret= RETRY;
		goto end_interleaved_write_frame;
	}

#ifdef _DEBUG
		LOGI("* NEW PACKET (%s) pkt->dts: %lld\n",
				((*pkt)->stream_index== muxer->video_st->index)? "V": "X",
				(*pkt)->dts);
		LOGI("* INTERLEAVE LEVEL: %u (a: %u; v: %u; buf-back: %u)\n", 
				muxer->level, muxer->alevel, muxer->vlevel, muxer->ab);
#endif

	/* Found position in which new incoming packet will be inserted */
	for(idx= 0; idx< muxer->level; idx++)
	{
		AVPacket *pointed_pkt= 
				muxer->apkt_aray[(muxer->ab+idx)&(MOD_SIZE-1)]; //pointed packet

		if(pointed_pkt== NULL) // sanity check
		{
			LOGE("Error on circular interleaving buffer (idx= %d; level: %d; buf-back: %u)\n", 
					idx, muxer->level, muxer->ab);
			ret= INTERLEAVING_ERROR;
			goto end_interleaved_write_frame;
		}
		
		/* If current DTS does not goes in current buffer position, continue 
		 * ordering iteration (we are ordering from the minor DTS value to 
		 * the greatest); else, we found position!.
		 * */
		if((*pkt)->dts> pointed_pkt->dts)
		{
			continue;
		}
		else
		{
			/* We have found position within circular buffer.
			 * Reorder buffer (make space for insertion) and upgarde insertion 
			 * pointer.
			 */
			int j;
			for(j= muxer->level; j> idx; j--)
			{
				//LOGD("j= %d, buf-front= %d, idx= %d\n", j, muxer->af, idx);
				muxer->apkt_aray[(muxer->ab+j)&(MOD_SIZE-1)]= 
						muxer->apkt_aray[(muxer->ab+j-1)&(MOD_SIZE-1)];
				//LOGD("muxer->apkt_aray[%u]= %p\n", j&(MOD_SIZE-1), muxer->apkt_aray[j&(MOD_SIZE-1)]);
			}
			insert_pkt= &muxer->apkt_aray[(muxer->ab+idx)&(MOD_SIZE-1)];
			break; 
		}
	} // for

	/* Finally interleave new incoming packet */
	(*insert_pkt)= (*pkt);
	(*pkt)= NULL; // to avoid freeing when returning from function

	/* Update "buffer-front index */
	muxer->af++; muxer->af&= (MOD_SIZE-1); // circular
	muxer->level++;
	if((*insert_pkt)->stream_index== muxer->audio_st->index) muxer->alevel++;
	if((*insert_pkt)->stream_index== muxer->video_st->index) muxer->vlevel++;	

	/* Success */
	ret= SUCCESS;

end_interleaved_write_frame:

	if(opkt!= NULL)
	{
		av_free_packet(opkt);
		free(opkt);
		opkt= NULL;
	}

	return ret;
}

static int write_frame(rtmpMuxerCtx* muxer, AVPacket* pkt)
{
	int ret;
	ret= AV_WRITE_FRAME(muxer->oc, pkt);

	// Treat errors
	if(ret!= 0)
	{
		/* Treat persistant connection issues (e.g. reset connection) */
		if(muxer->persistant_error_usecs== 0)
		{
			muxer->persistant_error_usecs= get_monotonic_clock_time(muxer);
			//LOGV("muxer->persistant_error_usecs= '%lld'\n", muxer->persistant_error_usecs); //comment-me
		}
		else
		{
			uint64_t persistant_error_usecs= get_monotonic_clock_time(muxer)- 
					muxer->persistant_error_usecs;
			//LOGV("persistant error elapsed time= '%lld'\n", persistant_error_usecs); //comment-me
			if(persistant_error_usecs> (uint64_t)muxer->persistant_error_usecs_limit)
			{
				/* Error tolerance reached */
				LOGV("Multiplexer: persistant connection issue time-limit reached "
						"(set to %d secs), resetting connection\n", 
						muxer->persistant_error_usecs_limit/1000000);
				muxer->persistant_error_usecs= 0;
				muxer->error_code= CONNECTION_RESET_ERROR;
				if(muxer->on_error) muxer->on_error((void*)muxer);
			}
		}
	}
	else //ret== 0 (O.K.)
	{
		/* Reset error tolerance clk */
		muxer->persistant_error_usecs= 0;
	}

	return ret;
}

/* FFmpeg log callback implementation */
//(void* ptr, int level, const char* fmt, va_list vl)
static void ffmpeg_log_callback(void *ctx, int level, const char *fmt, va_list vl)
{
	char *buf= NULL;
	int vsprintf_ret= -1;
	int alloc_size= 1024, max_alloc_size= 4096; // RAL: this may be changed

	/* Lock log 'mutex' defined statically in FFmpeg library */
#if HAVE_PTHREADS
	pthread_mutex_lock(&mutex);
#endif

	while(alloc_size<= max_alloc_size)
	{
		buf= (char*)malloc(alloc_size);
		if(buf== NULL)
		{
			LOGE("Could not allocate buffer\n");
			goto ffmpeg_log_callback_end;
		}
		vsprintf_ret= vsprintf(buf, fmt, vl);
		if(vsprintf_ret>=0)
		{
			break; // break 'while' loop, buffer size was enough
		}
		else
		{
			free(buf);
			buf= NULL;
			alloc_size*= 2; // increment alloc. size
		}
	}

	if(buf== NULL)
	{
		LOGE("Could not allocate enough memory to print message\n");
		goto ffmpeg_log_callback_end;
	}

	switch(level)
	{
	case -1:
	{
		/* woraround: special cases e.g. to pass AMF messages */
		if(strncmp(buf, "amf_tag_contents__get_string:", 29)== 0)
		{
			// NOTE: IMPORTANT!
			// This method is globl to all threads -attached or not- running ffmpeg functions.
			// Then, thread that call 'amf_message_event' without attaching (Android) will crash.
			// Solution: Adding a field to identify which thread is making current call.
			//amf_message_event(&buf[29]); //RAL: won't be used
		}
		break;
	}
	case AV_LOG_WARNING:
	{
		//LOGW("%s", buf); // Better to not publish!
		break;
	}
	case AV_LOG_ERROR:
	case AV_LOG_FATAL:
	case AV_LOG_PANIC:
	{
		LOGE("%s", buf);
		break;
	}
	default:
	{
#ifdef _DEBUG
		LOGD("%s", buf);
#endif
		break;
	}
	} // switch

ffmpeg_log_callback_end:

	if(buf!= NULL)
	{
		free(buf);
		buf= NULL;
	}

	/* Unlock log 'mutex' defined statically in FFmpeg library */
#if HAVE_PTHREADS
	pthread_mutex_unlock(&mutex);
#endif
}

/* AMF message get event */
#ifdef _ANDROID_
	//extern void Java_com_bbmlf_nowuseeme_MainActivity_onAMFMessage(const char* amf_msg);
#endif
static void amf_message_event(const char* amf_msg)
{
	LOGD("The following AMF message has been sent by the server: '%s'\n", amf_msg);

#ifdef _ANDROID_
	//Java_com_bbmlf_nowuseeme_MainActivity_onAMFMessage(amf_msg); // will not use
#endif
}

#ifdef _DEBUG
/* Auxiliary function to test FFmpeg's interleaving buffer increase */
static void interleaving_buff_test(AVFormatContext *oc)
{
	unsigned int i;
	AVFormatContext *s= oc;
	int64_t delta_dts_max = 0;
	for(i=0; i< s->nb_streams; i++)
	{
		if(s->streams[i]->last_in_packet_buffer)
		{
			int64_t delta_dts=
				av_rescale_q(s->streams[i]->last_in_packet_buffer->pkt.dts,
							s->streams[i]->time_base,
							AV_TIME_BASE_Q)-
				av_rescale_q(s->packet_buffer->pkt.dts,
							s->streams[s->packet_buffer->pkt.stream_index]->time_base,
							AV_TIME_BASE_Q);
			delta_dts_max= FFMAX(delta_dts_max, delta_dts);
		}
	} // for
	LOGI("**** delta_dts_max: %lld\n", delta_dts_max);
}
#endif
