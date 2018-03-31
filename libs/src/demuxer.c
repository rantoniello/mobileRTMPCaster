#include <pthread.h>
#include <string.h>
#include <unistd.h> //usleep

#include "log.h"
#include "error_codes.h"
#include "demuxer.h"

#ifdef __APPLE__
#include "clock_gettime_stub.h"
#endif

/* Definitions */
#define ASSERT_ACTION_LOG(CONDITION, ACTION, LOG) \
    if(!(CONDITION)) { \
        LOG; \
        ACTION; \
    }
#define ASSERT_ACT(CONDITION, ACTION) \
    ASSERT_ACTION_LOG(CONDITION, ACTION, LOGE("Assertion failed\n"))

#define ASSERT_ARG(CONDITION, RET_VALUE) \
    ASSERT_ACTION_LOG(CONDITION, return RET_VALUE, LOGE("Assertion failed\n"))

#define ASSERT_VOID // Trick

/* Prototypes */
static int interrupt_cb(void *arg);
static int prologue_audio_dec(void* t);
static void epilogue_audio_dec(void* t);
static int decode_audio_frame(void* t);
static int decode_audio_frame2(void* t);
static int prologue_video_dec(void* t);
static void epilogue_video_dec(void* t);
static int decode_video_frame(void* t);
static int get_multimedia_id(demuxerCtx* demuxer, int stream_index);
static void *thrDecFrame(void *t);
static int open_codec_context(demuxerCtx* demuxer, int *stream_idx, AVFormatContext *fmt_ctx, enum AVMediaType type);
static int demuxer_init2(demuxerCtx* demuxer);
static void *thrDemuxer(void *t);
static uint64_t get_monotonic_clock_time();
static void treat_persistant_connection_issue(demuxerCtx* demuxer, int mm_id, int reset);

static int interrupt_cb(void *arg)
{
	struct demuxerCtx *c = (struct demuxerCtx*)arg;

	if(c->interrupt_IO_FFmpeg!= 0)
	{
		LOGD("Interrupting demuxer I/O operation (FFmpeg)\n");
		return 1;
	}
	return 0;
}

// Taken from ffmpeg "
static int avcodec_private_execute(AVCodecContext *c, int (*func)(AVCodecContext *c2, void *arg2), void *arg, int *ret, int count, int size)
{
	int i;

	LOGV("Executing 'avcodec_private_execute()'...\n"); //comment-me

	for (i = 0; i < count; i++) {
		int r = func(c, (char *)arg + i * size);
		if (ret)
			ret[i] = r;
		//{ //RAL
		if(r< 0)
		{
			// workarround to detect error
			if(c->err_recognition & AV_EF_EXPLODE)
			{
				c->err_recognition= -1;
				LOGD("avcodec_private_execute (FFmpeg) error\n");
			}
		}
		//}
	}
	return 0;
}

static int prologue_audio_dec(void* t)
{
	// Get demuxer context
	demuxerCtx* demuxer= (demuxerCtx*)t;

	// Get presentation context structure
	thrPresentationCtx* presCtx= (thrPresentationCtx*)demuxer->presentat_ctx[MM_AUDIO];

	// Init opaque presentation data
	if(presCtx->prologue && presCtx->prologue((void*)demuxer)!= SUCCESS)
	{
		return ERROR;
	}

	// Init audio buffer
	int abuf;
	for(abuf= 0; abuf< NUM_ABUFS; abuf++)
	{
		demuxer->audio_buf[abuf]= NULL;
        demuxer->audio_buf_levels[abuf]= 0;
	}
	demuxer->num_abuf= 0;
    demuxer->num_oabuf= 0;
    demuxer->underrun_flag= 1;
	demuxer->output_buffer_level= 0;

	//create resampler context
	demuxer->swr_ctx= swr_alloc();
	if(!demuxer->swr_ctx)
	{
		LOGE("Could not allocate resampler context\n");
		return ERROR;
	}
	return SUCCESS;
}

static void epilogue_audio_dec(void* t)
{
	// Get demuxer context
	demuxerCtx* demuxer= (demuxerCtx*)t;

	// Get presentation context structure
	thrPresentationCtx* presCtx= (thrPresentationCtx*)demuxer->presentat_ctx[MM_AUDIO];

	// Release opaque presentation data
	if(presCtx->epilogue)
	{
		presCtx->epilogue((void*)demuxer);
	}

	//==== Release thread resources ====

	// Scaling
	int abuf;
	for(abuf= 0; abuf< NUM_ABUFS; abuf++)
	{
		if(demuxer->audio_buf[abuf]!= NULL)
		{
            if(demuxer->audio_buf[abuf][0]!= NULL)
            {
                av_freep(&demuxer->audio_buf[abuf][0]);
            }
			av_freep(&demuxer->audio_buf[abuf]);
			demuxer->audio_buf[abuf]= NULL;
		}
	}
	if(demuxer->swr_ctx!= NULL)
	{
		swr_free(&demuxer->swr_ctx);
		demuxer->swr_ctx= NULL;
	}
}

static int decode_audio_frame(void* t)
{
	// Get demuxer context
	demuxerCtx* demuxer= (demuxerCtx*)t;

	// Get thread context
	thrDecCtx* dec_ctx= demuxer->dec_ctx[MM_AUDIO];

	// decode packet
	AVPacket orig_pkt= *dec_ctx->dec_pkt;
	uint8_t *orig_dst_data= NULL;
	while(demuxer->can_run_demux!= 0)
	{
		int ret= decode_audio_frame2(t);

		if(demuxer->audio_buf[demuxer->num_abuf]!= NULL && orig_dst_data== NULL)
			orig_dst_data= (uint8_t*)demuxer->audio_buf[demuxer->num_abuf][0];

		//LOGD("audio decoded size is: %d; remaining: %d\n", ret, dec_ctx->dec_pkt.size- ret); // comment-me
		if(ret< 0)
		{
			return ERROR;
		}
		if((dec_ctx->dec_pkt->size- ret)<= 0)
		{
			break;
		}
		dec_ctx->dec_pkt->data+= ret;
		dec_ctx->dec_pkt->size-= ret;
		if(demuxer->audio_buf[demuxer->num_abuf]!= NULL) demuxer->audio_buf[demuxer->num_abuf][0]+= ret;
	}
	if(demuxer->audio_buf[demuxer->num_abuf]!= NULL) demuxer->audio_buf[demuxer->num_abuf][0]= orig_dst_data;
	//LOGD("audio decoded buf num is: %d\n", demuxer->num_abuf); // comment-me
	if((++demuxer->num_abuf)>= NUM_ABUFS) demuxer->num_abuf= 0; // circular output buffers
	//av_free_packet(&orig_pkt); // =av_free_packet(&dec_ctx->dec_pkt); //FIXME!!
	return SUCCESS;
}

static int decode_audio_frame2(void* t)
{
	AVFrame *aframe= NULL;
	int got_frame= 0;
	int decoded, decoded_ret;

	// Get demuxer context
	demuxerCtx* demuxer= (demuxerCtx*)t;

	// Get thread context
	thrDecCtx* dec_ctx= demuxer->dec_ctx[MM_AUDIO];

	// Get input packet size
	decoded= dec_ctx->dec_pkt->size;

	// Alloc decoded audio frame buffer
	if((aframe= av_frame_alloc())== NULL)
	{
		LOGE("Could not alloc audio decoding frame buffer\n");
		decoded= ERROR;
		goto end_decode_audio_frame2;
	}

	// decode audio frame
	if((decoded_ret= avcodec_decode_audio4(demuxer->audio_dec_ctx, aframe, &got_frame, 
			dec_ctx->dec_pkt))< 0)
	{
		LOGE("Error decoding audio frame\n");
		decoded= ERROR;
		goto end_decode_audio_frame2;
	}

	/* Some audio decoders decode only part of the packet, and have to be
	 * called again with the remainder of the packet data.
	 * Sample: fate-suite/lossless-audio/luckynight-partial.shn
	 * Also, some decoders might over-read the packet. */
	decoded= FFMIN(decoded_ret, dec_ctx->dec_pkt->size);

	if(got_frame)
	{
		int audio_buf_size, ret;

		// Get presentation context structure
		thrPresentationCtx* presCtx= (thrPresentationCtx*)demuxer->presentat_ctx[MM_AUDIO];

		// Initialize rest of converter parameters only once
		if(demuxer->audio_buf[demuxer->num_abuf]== NULL)
		{
			int64_t dst_ch_layout= AV_CH_LAYOUT_MONO;
			int dst_rate= aframe->sample_rate;
			demuxer->dst_sample_fmt= AV_SAMPLE_FMT_S16;

			// Set options
			av_opt_set_int(demuxer->swr_ctx, "in_channel_layout", aframe->channel_layout, 0);
			av_opt_set_int(demuxer->swr_ctx, "in_sample_rate", aframe->sample_rate, 0);
			av_opt_set_sample_fmt(demuxer->swr_ctx, "in_sample_fmt", aframe->format, 0);
			av_opt_set_int(demuxer->swr_ctx, "out_channel_layout", dst_ch_layout, 0);
			av_opt_set_int(demuxer->swr_ctx, "out_sample_rate", dst_rate, 0);
			av_opt_set_sample_fmt(demuxer->swr_ctx, "out_sample_fmt", demuxer->dst_sample_fmt, 0);
			
			// Initialize the resampling context
			if(swr_init(demuxer->swr_ctx)< 0)
			{
				LOGE("Failed to initialize the resampling context\n");
				decoded= ERROR;
				goto end_decode_audio_frame2;
			}
			
			// Compute the number of converted samples: buffering is avoided
			// ensuring that the output buffer will contain at least all the
			// converted input samples
			demuxer->dst_nb_samples= (int)av_rescale_rnd(
					aframe->nb_samples, dst_rate, 
					aframe->sample_rate, AV_ROUND_UP);
			
			// Buffer is going to be directly written to a rawaudio file, no alignment
			demuxer->dst_nb_channels= av_get_channel_layout_nb_channels(dst_ch_layout);
			if(av_samples_alloc_array_and_samples(
					   (uint8_t ***)&demuxer->audio_buf[demuxer->num_abuf], &demuxer->dst_linesize, 
					   demuxer->dst_nb_channels, demuxer->dst_nb_samples, 
					   demuxer->dst_sample_fmt, 0)< 0)
			{
				LOGE("Could not allocate destination samples\n");
				decoded= ERROR;
				goto end_decode_audio_frame2;
			}
		}
#if 0
		// compute destination number of samples 
		int check_dst_nb_samples= 
				av_rescale_rnd(swr_get_delay(demuxer->swr_ctx, aframe->sample_rate) + aframe->nb_samples, 24000, aframe->sample_rate, AV_ROUND_UP);
		//LOGD("audio 'check_dst_nb_samples' is: %d (ref: %d)\n", check_dst_nb_samples, demuxer->dst_nb_samples); // comment-me
#endif

		// Convert to destination format
		if((ret= swr_convert(demuxer->swr_ctx, (uint8_t**)demuxer->audio_buf[demuxer->num_abuf], demuxer->dst_nb_samples, 
				(const uint8_t **)aframe->extended_data, aframe->nb_samples))< 0)
		{
			LOGE("Error while converting\n");
			decoded= ERROR;
			goto end_decode_audio_frame2;
		}

		if((audio_buf_size= av_samples_get_buffer_size(&demuxer->dst_linesize,
				demuxer->dst_nb_channels, ret, demuxer->dst_sample_fmt, 1))< 0)
		{
			LOGE("Could not get sample buffer size\n");
			decoded= ERROR;
			goto end_decode_audio_frame2;
		}
        demuxer->audio_buf_size= audio_buf_size;

        /* Treat audio MUTE flag */
        if(demuxer->audio_mute)
        {
            /* Put PCM samples to zero to mute audio */
            memset((void*)demuxer->audio_buf[demuxer->num_abuf][0], 0, demuxer->audio_buf_size);
        }

		if(presCtx->render!= NULL)
		{
			//LOGD("audio presentation buffer size is: %d (inp. n° sample: %d)\n", demuxer->audio_buf_size, aframe->nb_samples); // comment-me
			presCtx->render((void*)demuxer);
		}
	} // got frame

end_decode_audio_frame2:

	if(aframe!= NULL)
	{
		av_frame_free(&aframe);
		aframe= NULL;
	}
	//av_free_packet(&dec_ctx->dec_pkt); //**MUST** be freed outside

	return decoded;
}

static int prologue_video_dec(void* t)
{
	demuxerCtx* demuxer= (demuxerCtx*)t;
	demuxer->img_convert_ctx= NULL;

	// Get presentation context structure
	thrPresentationCtx* presCtx= (thrPresentationCtx*)demuxer->presentat_ctx[MM_VIDEO];

	// Init opaque presentation data
	if(presCtx->prologue && presCtx->prologue((void*)demuxer)!= SUCCESS)
	{
		return ERROR;
	}

	return SUCCESS;
}

static void epilogue_video_dec(void* t)
{
	demuxerCtx* demuxer= (demuxerCtx*)t;

	// Get presentation context structure
	thrPresentationCtx* presCtx= (thrPresentationCtx*)demuxer->presentat_ctx[MM_VIDEO];

	// Release opaque presentation data
	if(presCtx->epilogue)
	{
		presCtx->epilogue((void*)demuxer);
	}

	//==== Release thread resources ====
	
	// Scaling
	if(demuxer->img_convert_ctx!= NULL)
	{
		sws_freeContext(demuxer->img_convert_ctx);
		demuxer->img_convert_ctx= NULL;
	}
}

static int decode_video_frame(void* t)
{
	AVFrame *vframe= NULL;
	int got_frame= 0;
	int ret= ERROR;

	// Get demuxer context
	demuxerCtx* demuxer= (demuxerCtx*)t;

	// Get thread context
	thrDecCtx* dec_ctx= demuxer->dec_ctx[MM_VIDEO];

	// Alloc decoded video frame buffer
	if((vframe= av_frame_alloc())== NULL)
	{
		LOGE("Could not alloc video decoding frame buffer\n");
		goto end_decode_video_frame;
	}
    vframe->data[0]= vframe->data[1]= vframe->data[2]= 0;

	// decode video
	demuxer->video_dec_ctx->err_recognition= AV_EF_EXPLODE;
	//LOGV("avcodec_decode_video2()..."); //comment-me
	if(avcodec_decode_video2(demuxer->video_dec_ctx, vframe, &got_frame, 
			   dec_ctx->dec_pkt)< 0)
	{
		LOGE("Error decoding video frame\n");
		goto end_decode_video_frame;
	}
	//LOGV("... O.K."); //comment-me

	// Hack to detect decoding errors on multi-context decoders
	if(demuxer->video_dec_ctx->err_recognition== -1) //HACK
	{
		// restore 'err_recognition' value
		demuxer->video_dec_ctx->err_recognition= (AV_EF_EXPLODE | AV_EF_BUFFER);
		LOGE("Error decoding video frame (alternative detection)\n");
		goto end_decode_video_frame;
	}

	// Check if resolution has changed; send event if apply
	if(demuxer->video_dec_ctx->width!= demuxer->v_iwidth || 
			demuxer->video_dec_ctx->height!= demuxer->v_iheight)
	{
		demuxer->v_iwidth= demuxer->video_dec_ctx->width;
		demuxer->v_iheight= demuxer->video_dec_ctx->height;
		if(demuxer->on_event)
		{
			demuxer->event_code= EVENT_IRESOLUTION_CHANGED;
			demuxer->on_event(demuxer);
		}
	}

	if(got_frame && demuxer->pFrameConverted!= NULL)
	{
		// Get presentation context structure
		thrPresentationCtx* presCtx= (thrPresentationCtx*)demuxer->presentat_ctx[MM_VIDEO];

        #ifdef _ANDROID_
            // Sacle video frame to output resolution
            int src_w= demuxer->video_dec_ctx->width, src_h= demuxer->video_dec_ctx->height;
            int dst_w= demuxer->v_width, dst_h= demuxer->v_height;
            int pix_fmt= demuxer->video_dec_ctx->pix_fmt;
            int dst_pix_fmt= AV_PIX_FMT_RGB565;

            // Just get or, if apply, re-allocate scaling context structure
            //LOGD("Creating scaling context...\n");
            demuxer->img_convert_ctx= sws_getCachedContext(
                    demuxer->img_convert_ctx,
                    src_w, src_h, pix_fmt,
                    dst_w, dst_h, dst_pix_fmt,
                    SWS_FAST_BILINEAR, NULL, NULL, NULL);
            if(demuxer->img_convert_ctx== NULL)
            {
                LOGE("Cannot initialize the conversion context!\n");
                goto end_decode_video_frame;
            }

            /* Scale... */
            sws_scale(demuxer->img_convert_ctx, 
                    (uint8_t const* const*)vframe->data, 
                    vframe->linesize, 0, src_h, 
                    demuxer->pFrameConverted->data, 
                    demuxer->pFrameConverted->linesize);
        #else
            if(dec_ctx->scale_frame!= NULL)
            {
                //LOGV("Using external sclaling method\n"); // Comment-me
                void **dstY= (void**)&demuxer->pFrameConverted->data[0],
                     **dstCb= (void**)&demuxer->pFrameConverted->data[1],
                     **dstCr= (void**)&demuxer->pFrameConverted->data[2];
                int src_chroma_width= vframe->width/2;
                int src_chroma_height= vframe->height/2;
                int dst_chroma_width= demuxer->v_width/2;
                int dst_chroma_height= demuxer->v_height/2;

                dec_ctx->scale_frame(vframe->data[0], vframe->width, vframe->height, vframe->linesize[0],
                                     dstY, demuxer->v_width, demuxer->v_height, demuxer->v_width);
                dec_ctx->scale_frame(vframe->data[1], src_chroma_width, src_chroma_height, vframe->linesize[1],
                                     dstCb, dst_chroma_width, dst_chroma_height, dst_chroma_width);
                dec_ctx->scale_frame(vframe->data[2], src_chroma_width, src_chroma_height, vframe->linesize[2],
                                     dstCr, dst_chroma_width, dst_chroma_height, dst_chroma_width);

                demuxer->pFrameConverted->linesize[0]= demuxer->v_width;
                demuxer->pFrameConverted->linesize[1]=
                demuxer->pFrameConverted->linesize[2]= dst_chroma_width;
            }
            else
            {
                demuxer->v_width= vframe->width;
                demuxer->v_height= vframe->height;
                demuxer->pFrameConverted->data[0]= vframe->data[0];
                demuxer->pFrameConverted->linesize[0]= vframe->linesize[0];
                demuxer->pFrameConverted->data[1]= vframe->data[1];
                demuxer->pFrameConverted->linesize[1]= vframe->linesize[1];
                demuxer->pFrameConverted->data[2]= vframe->data[2];
                demuxer->pFrameConverted->linesize[2]= vframe->linesize[2];
            }
        #endif

		/* Request to render video frame */
		if(presCtx->render!= NULL && demuxer->can_run_demux)
		{ 
			#if 0
			{ //RAL: comment-me
				static uint64_t time0, time1;
				static int first_pass= 1;
				time1= get_monotonic_clock_time();
				if(!first_pass) LOGV("rendering time...: '%lld'\n", time1- time0 );
				else first_pass= 0;
				time0= time1;
			}
			#endif
			presCtx->render((void*)demuxer);
		}

        if(dec_ctx->scale_frame!= NULL)
        {
            free(demuxer->pFrameConverted->data[0]);
            free(demuxer->pFrameConverted->data[1]);
            free(demuxer->pFrameConverted->data[2]);
        }
	} // got_frame

	ret= SUCCESS;

end_decode_video_frame:

	if(vframe!= NULL)
	{
		av_frame_free(&vframe);
		vframe= NULL;
	}
	//av_free_packet(&dec_ctx->dec_pkt); //FIXME!!
	return ret;
}
	
static inline int get_multimedia_id(demuxerCtx* demuxer, int stream_index)
{
	int id= MM_VIDEO; // set any by default

	if(stream_index== demuxer->video_stream_idx)
	{
		id= MM_VIDEO;
	}
	else if(stream_index== demuxer->audio_stream_idx)
	{
		id= MM_AUDIO;
	}
	return id;
}

/**
 * This is a generic decoding template thread-function.
 * All multimedia decoding threads (audio, video, etc.) will implement this
 * generic function.
 */
static void* thrDecFrame(void *t)
{
    int mm_id;
	thrDecCtx* dec_ctx= (thrDecCtx*)t;

    ASSERT_ARG(dec_ctx!= NULL, (void*)(intptr_t)ERROR);

	mm_id= dec_ctx->id;

	LOGD("Executing 'thrDecFrame()' for id '%d'...\n", mm_id);

	// Get demuxer context
	demuxerCtx* demuxer= (demuxerCtx*)dec_ctx->demuxer;
	
	// Prologue
	if(dec_ctx->prologue!= NULL)
	{
		if((dec_ctx->prologue((void*)demuxer))< 0)
		{
			demuxer->error_code= PLAYER_DECODING_ERROR;
			goto end_thrDecFrame;
		}
	}

	while(demuxer->can_run_demux)
	{
		//LOGV("looping 'thrDecFrame' for id '%d'\n", mm_id); // comment-me
		pthread_mutex_lock(&dec_ctx->dec_mutex);

		while(dec_ctx->dec_pkt== NULL && demuxer->can_run_demux)
			pthread_cond_wait(&dec_ctx->produce_cond, &dec_ctx->dec_mutex);

        ASSERT_ACT(dec_ctx->dec_pkt!= NULL, ASSERT_VOID);

		if(dec_ctx->decode_frame!= NULL && demuxer->can_run_demux)
		{
			int dec_ret= dec_ctx->decode_frame((void*)demuxer);
			//LOGV("'decode_frame()' for id '%d' returns '%d'\n", mm_id, dec_ret); // comment-me
			treat_persistant_connection_issue(demuxer, mm_id, dec_ret<0? 0: 1);
		}

        /* We just put 'dec_pkt' pointer to NULL; distribution thread ('thrPktDist') 
         * will be in cherge of actually freeing this packet
         */
        dec_ctx->dec_pkt= NULL;
        pthread_cond_signal(&dec_ctx->consume_cond);

        pthread_mutex_unlock(&dec_ctx->dec_mutex);
	}

end_thrDecFrame:

	// Epilogue / Release thread resources
	if(dec_ctx->epilogue!= NULL)
	{
		dec_ctx->epilogue((void*)demuxer);
	}

	// Treat errors
	if(demuxer->error_code!= SUCCESS)
	{
		if(demuxer->on_error) demuxer->on_error((void*)demuxer);
	}

	LOGD("Exiting 'thrDecFrame()' for id '%d'...\n", mm_id);
	pthread_exit(NULL);
}

//#define PROFILE_THRDEMUXER
static void* thrPktDist(void *t)
{
    demuxerCtx* demuxer= (demuxerCtx*)t;
    
    ASSERT_ARG(demuxer!= NULL, (void*)(intptr_t)ERROR);
    
    LOGV("\n");

    while(demuxer->can_run_demux)
    {
        thrDecCtx* dec_ctx;
        int mm_id;
#ifdef PROFILE_THRDEMUXER
        struct timespec start_time, stop_time;
        static uint32_t average_counter= 0;
        static uint64_t average_usecs= 0;
        clock_gettime(CLOCK_REALTIME, &start_time);
#endif

        /* Lock demuxer's mutex to check if we have new packet to distribute to decoder threads */
        pthread_mutex_lock(&demuxer->mutex);

        while(demuxer->pkt_level[demuxer->dist_pkt_idx]<= 0 && demuxer->can_run_demux)
            pthread_cond_wait(&demuxer->produce_cond, &demuxer->mutex);

        /* Get decoding thread context structure corresponding to new incoming packet */
        mm_id= get_multimedia_id(demuxer, demuxer->pkt[demuxer->dist_pkt_idx].stream_index);
        dec_ctx= demuxer->dec_ctx[mm_id];
        //LOGD("Passing new AV-packet idx= %d to thread with id '%d'\n",
        //     demuxer->dist_pkt_idx, mm_id); // comment-me

        /* unlock demuxer's mutex */
        pthread_mutex_unlock(&demuxer->mutex);

        /* Pass new packet reference to decoder thread */
        //do {
            /* We lock decoder's mutex to notify */
            pthread_mutex_lock(&dec_ctx->dec_mutex);

            /* Signal decoder to perform processing */
            ASSERT_ACT(dec_ctx->dec_pkt== NULL, ASSERT_VOID);
            dec_ctx->dec_pkt= &demuxer->pkt[demuxer->dist_pkt_idx];
            pthread_cond_signal(&dec_ctx->produce_cond);

            /* Wait for decoder to release given data packet */
            while(dec_ctx->dec_pkt!= NULL && demuxer->can_run_demux)
                pthread_cond_wait(&dec_ctx->consume_cond, &dec_ctx->dec_mutex);

            ASSERT_ACT(dec_ctx->dec_pkt== NULL, ASSERT_VOID);
            pthread_mutex_unlock(&dec_ctx->dec_mutex);
        //} while(0);

        pthread_mutex_lock(&demuxer->mutex);

        // We definitely release current data packet and signal recv thread
        av_free_packet(&demuxer->pkt[demuxer->dist_pkt_idx]);
        demuxer->pkt_level[demuxer->dist_pkt_idx]= 0;
        pthread_cond_signal(&demuxer->consume_cond);

        /* unlock demuxer's mutex */
        pthread_mutex_unlock(&demuxer->mutex);

        /* Advance distribution index in circular fashion */
        demuxer->dist_pkt_idx= (++demuxer->dist_pkt_idx)%NUM_INPUT_BUFS;

#ifdef PROFILE_THRDEMUXER
        clock_gettime(CLOCK_REALTIME, &stop_time);
        average_usecs+= (stop_time.tv_nsec/(1000))- (start_time.tv_nsec/(1000));
        if(average_counter++ >1)
        {
            LOGV("Avg. time: %llu\n", average_usecs/average_counter);
            average_counter= average_usecs= 0;
        }
#endif
    } // while
    
    LOGV("Exiting 'thrPktDist()'\n");
    return (void*)(intptr_t)SUCCESS;
    
}
#undef PROFILE_THRDEMUXER

static void *thrDemuxer(void *t)
{
    demuxerCtx* demuxer= (demuxerCtx*)t;

    ASSERT_ARG(demuxer!= NULL, (void*)(intptr_t)ERROR);

    LOGV("\n");
    
    /* Initialize demuxer context structure */
    demuxer_init2(demuxer);
    
    while(demuxer->can_run_demux)
    {
        /* Lock demuxer's mutex to check if current packet has been consumed */
        pthread_mutex_lock(&demuxer->mutex);

        while(demuxer->pkt_level[demuxer->recv_pkt_idx]> 0 && demuxer->can_run_demux)
        {
            static unsigned int error_cnt= 0;
            //struct timespec unblock_time;
            AVPacket pkt;
            if(error_cnt++> 10)
            {
                error_cnt= 0;
                LOGV("**** INPUT BUFFER OVERFLOW CONTROL ****\n");
            }
            //clock_gettime(CLOCK_REALTIME, &unblock_time);
            //unblock_time.tv_nsec+= 1000; // 1 usec
            //pthread_cond_timedwait(&demuxer->consume_cond, &demuxer->mutex, &unblock_time);
            // Avoid FFmpeg's network-stack buffering
            pthread_mutex_unlock(&demuxer->mutex);
            av_read_frame(demuxer->fmt_ctx, &pkt);
            av_free_packet(&pkt);
            pthread_mutex_lock(&demuxer->mutex);
        }

        /* unlock demuxer's mutex */
        pthread_mutex_unlock(&demuxer->mutex);

        /* Fill current packet with new data */
        //LOGV("'av_read_frame()'; recv idx= %d\n", demuxer->recv_pkt_idx); //comment-me
        if(av_read_frame(demuxer->fmt_ctx, &demuxer->pkt[demuxer->recv_pkt_idx])< 0)
        {
            /* On read error, release current packet buffer and go for the next one */
            LOGE("Could not read a/v frame form multiplex.\n");
            int mm_id= get_multimedia_id(demuxer, demuxer->pkt[demuxer->recv_pkt_idx].stream_index);
            av_free_packet(&demuxer->pkt[demuxer->recv_pkt_idx]);
            treat_persistant_connection_issue(demuxer, mm_id, 0);
            continue; // while
        }

        /* Signal distribution thread that a new packet is available for decoding */
        pthread_mutex_lock(&demuxer->mutex);
        
        // Set 'pkt_level[]' to new data size
        demuxer->pkt_level[demuxer->recv_pkt_idx]= demuxer->pkt[demuxer->recv_pkt_idx].size;
        pthread_cond_signal(&demuxer->produce_cond);
        
        /* unlock decoding thread */
        pthread_mutex_unlock(&demuxer->mutex);
        
        /* Finally, go for the next frame of the circular input buffer */
        demuxer->recv_pkt_idx= (++demuxer->recv_pkt_idx)%NUM_INPUT_BUFS;
        
    } // while

    LOGV("Exiting 'thrDemuxer()'\n");
    return (void*)(intptr_t)SUCCESS;
}

static int open_codec_context(demuxerCtx* demuxer, int *stream_idx,
		AVFormatContext *fmt_ctx, enum AVMediaType type)
{
	int ret;
	AVStream *st;
	AVCodecContext *dec_ctx= NULL;
	AVCodec *dec = NULL;

	ret= av_find_best_stream(fmt_ctx, type, -1, -1, NULL, 0);
	if(ret< 0)
	{
		LOGE("Could not find %s stream in input file '%s'\n",
				av_get_media_type_string(type), demuxer->uri);
		return ERROR;
	} 
	else 
	{
		*stream_idx= ret;
		st= fmt_ctx->streams[*stream_idx];

		// find decoder for the stream
		dec_ctx= st->codec;
		dec= avcodec_find_decoder(dec_ctx->codec_id);
		if(!dec)
		{
			LOGE("Failed to find %s codec\n", av_get_media_type_string(type) );
			return ERROR;
		}
		if(avcodec_open2(dec_ctx, dec, NULL)< 0)
		{
			LOGE("Failed to open %s codec\n", av_get_media_type_string(type) );
			return ERROR;
		}
	}
	return SUCCESS;
}

static int demuxer_init2(demuxerCtx* demuxer)
{
	int ret= COULD_NOT_START_PLAYER;
	int mm_id= 0, i;

	// Video output frame buffer
	demuxer->pFrameConverted= (AVPicture*)av_mallocz(sizeof(AVPicture));
#ifdef _ANDROID_
    if(demuxer->pFrameConverted!= NULL)
    {
        int i, dst_pix_fmt= AV_PIX_FMT_RGB565;

        if(demuxer->v_width<= 0 || demuxer->v_height<= 0)
        {
            LOGW("video resolution not initialized, asigning defaults.\n");
            demuxer->v_width= 640;
            demuxer->v_height= 360;
        }

        for(i= 0; i< AV_NUM_DATA_POINTERS; i++)
            demuxer->pFrameConverted->data[i]= NULL;

    	avpicture_alloc(demuxer->pFrameConverted, dst_pix_fmt, demuxer->v_width, demuxer->v_height);
    }
    LOGD("Output video buffer allocated; resolution is '%dx%d'\n", demuxer->v_width, demuxer->v_height);
#endif

    // FFmpeg related

	/* register all formats and codecs */
	//	av_register_all(); //RAL: outside

	demuxer->fmt_ctx= avformat_alloc_context();
	demuxer->fmt_ctx->flags |= (AVFMT_FLAG_NONBLOCK | AVFMT_FLAG_NOBUFFER);
	demuxer->fmt_ctx->interrupt_callback.callback= interrupt_cb;
	demuxer->fmt_ctx->interrupt_callback.opaque= demuxer;
	LOGD("'avformat' context allocated.\n");

	/* open input file, and allocate format context */
    AVInputFormat *iformat= av_find_input_format("flv");
    AVDictionary *options = NULL;
    av_dict_set(&options, "rtmp_buffer", "500", 0);
    av_dict_set(&options, "rtmp_live", "live", 0);
    av_dict_set(&options, "probesize", "4096", 0);
    av_dict_set(&options, "max_delay", "500000", 0);
    av_dict_set(&options, "fflags", "nobuffer", 0);

	if(avformat_open_input(&demuxer->fmt_ctx, demuxer->uri, iformat, &options)< 0)
	{
		LOGE("Could not open source file %s\n", demuxer->uri);
		ret= BAD_URL_PLAYER;
		goto end_demuxer_init;
	}
	LOGD("Input stream successfully opened.\n");

	/* retrieve stream information */
	if(avformat_find_stream_info(demuxer->fmt_ctx, NULL)< 0)
	{
		LOGE("Could not find stream information\n");
		ret= COULD_NOT_START_PLAYER;
		goto end_demuxer_init;
	}
	LOGD("Input stream successfully parsed for initialization.\n");

	if(open_codec_context(demuxer, &demuxer->video_stream_idx, demuxer->fmt_ctx, AVMEDIA_TYPE_VIDEO)>= 0)
	{
		demuxer->video_stream= demuxer->fmt_ctx->streams[demuxer->video_stream_idx];
		demuxer->video_dec_ctx= demuxer->video_stream->codec;
		//demuxer->video_dec_ctx->pix_fmt= AV_PIX_FMT_RGB565; //comment-me
		demuxer->video_dec_ctx->err_recognition= (AV_EF_EXPLODE | AV_EF_BUFFER);
		if((void*)demuxer->video_dec_ctx->execute== (void*)avcodec_default_execute)
		{
			LOGV("s->execute points to: %p (default method is %p -avcodec_default_execute-)\n", 
					demuxer->video_dec_ctx->execute, avcodec_default_execute); //delete-me
			demuxer->video_dec_ctx->execute= avcodec_private_execute;
		}
		demuxer->v_iwidth= demuxer->video_dec_ctx->width;
		demuxer->v_iheight= demuxer->video_dec_ctx->height;
		if(demuxer->on_event)
		{
			demuxer->event_code= EVENT_IRESOLUTION_CHANGED;
			demuxer->on_event(demuxer);
		}
	}
	else
	{
		LOGE("Could not open video codec context.\n");
		ret= COULD_NOT_START_PLAYER;
		goto end_demuxer_init;
	}
	LOGD("Video codec context successfully opened.\n");

	if(open_codec_context(demuxer, &demuxer->audio_stream_idx, demuxer->fmt_ctx, AVMEDIA_TYPE_AUDIO)>= 0)
	{
		demuxer->audio_stream= demuxer->fmt_ctx->streams[demuxer->audio_stream_idx];
		demuxer->audio_dec_ctx= demuxer->audio_stream->codec;
		demuxer->audio_dec_ctx->request_sample_fmt= AV_SAMPLE_FMT_S16;
	}
	else
	{
		LOGE("Could not open audio codec context.\n");
		ret= COULD_NOT_START_PLAYER;
		goto end_demuxer_init;
	}
	LOGD("Audio codec context successfully opened.\n");

	/* dump input information to stderr */
	av_dump_format(demuxer->fmt_ctx, 0, demuxer->uri, 0);
	if(!demuxer->audio_stream && !demuxer->video_stream)
	{
		LOGE("Could not find audio or video stream in the input, aborting\n");
		ret= COULD_NOT_START_PLAYER;
		goto end_demuxer_init;
	}

	/* initialize packet, set data to NULL, let the demuxer fill it */
    for(i= 0; i< NUM_INPUT_BUFS; i++) {
        av_init_packet(&demuxer->pkt[i]);
        demuxer->pkt[i].data= NULL;
        demuxer->pkt[i].size= 0;
        demuxer->pkt_level[i]= 0;
    }
    demuxer->recv_pkt_idx= 0;
    demuxer->dist_pkt_idx= 0;

	//==== Initialize decoding threads ====
	for(mm_id= 0; mm_id< MM_IDNUM; mm_id++)
	{
        thrDecCtx* c= demuxer->dec_ctx[mm_id];

		// Launch multimedia decoding thread in a joinable (default) state
		// (WARNING: create threads after 'can_run_demux' is set to '1/true'
		LOGD("Launching dec thread with multimedia id='%d'.\n", mm_id);
#ifndef DO_NOT_DECODE
		pthread_create(&c->dec_thread, NULL, thrDecFrame, (void*)c);
#endif
	}

    /* Launch demultiplexer "packet distribution" thread */
    pthread_create(&demuxer->dist_thread, NULL, thrPktDist, (void*)demuxer);

	// Everythis is ready
	ret= SUCCESS;
	LOGD("Demultiplexer successfully initialized!\n");

end_demuxer_init:

	if(ret!= SUCCESS)
	{
		//demuxer_release(demuxer); // can not be deleted from thread
		demuxer->error_code= ret;
		if(demuxer->on_error) demuxer->on_error((void*)demuxer);
	}
	return ret;
}

int demuxer_init(demuxerCtx* demuxer, demuxerInitCtx* demuxer_init_ctx)
{
    enum mutimedia_id mm_id;
    int end_code= COULD_NOT_START_PLAYER;

    ASSERT_ARG(demuxer!= NULL, ERROR);

    /* Initialize demuxer context structure */
    memset(demuxer, 0, sizeof(demuxerCtx));
    demuxer->uri= strndup(demuxer_init_ctx->url, demuxer_init_ctx->url_length);
    demuxer->v_width= demuxer_init_ctx->width;
    demuxer->v_height= demuxer_init_ctx->height;
    demuxer->audio_mute= demuxer_init_ctx->isMuteOn;
    demuxer->on_error= demuxer_init_ctx->on_error;
    demuxer->persistant_error_msecs_limit= demuxer_init_ctx->persistant_error_msecs_limit;
    demuxer->on_event= demuxer_init_ctx->on_event;
    demuxer->error_code= SUCCESS;
    pthread_mutex_init(&demuxer->mutex, NULL);
    pthread_cond_init(&demuxer->consume_cond, NULL);
    pthread_cond_init(&demuxer->produce_cond, NULL);

    //==== Initialize multimedia threaded-decoding related ====
    for(mm_id= 0; mm_id< MM_IDNUM; mm_id++)
    {
        demuxerCtx* d= demuxer;
        thrDecCtx* c;
        
        // Allocate context
        c= d->dec_ctx[mm_id]= calloc(1, sizeof(thrDecCtx));
        ASSERT_ACT(c!= NULL, goto end_demuxer_init);
        
        // Initialize
        c->id= mm_id; // set multimedia stream identificator (e.g. for video, audio, etc.)
        c->demuxer= d;
        pthread_mutex_init(&c->dec_mutex, NULL);
        pthread_cond_init(&c->consume_cond, NULL);
        pthread_cond_init(&c->produce_cond, NULL);
        c->dec_pkt= NULL;
        
        // Set methods
        if(mm_id== MM_VIDEO)
        {
            c->prologue= prologue_video_dec;
            c->epilogue= epilogue_video_dec;
            c->decode_frame= decode_video_frame;
            c->scale_frame= demuxer_init_ctx->scale_video_frame;
        }
        else if(mm_id== MM_AUDIO)
        {
            c->prologue= prologue_audio_dec;
            c->epilogue= epilogue_audio_dec;
            c->decode_frame= decode_audio_frame;
        }
        else
        {
            c->prologue= NULL;
            c->epilogue= NULL;
            c->decode_frame= NULL;
        }
    }

    /* **** Initialize demuxer's presentation context **** */
    for(mm_id= 0; mm_id< MM_IDNUM; mm_id++)
    {
        demuxerCtx* d= demuxer;
        thrPresentationCtx* c;
        
        // Allocate context
        c= d->presentat_ctx[mm_id]=
        (thrPresentationCtx*)calloc(1, sizeof(thrPresentationCtx) );
        ASSERT_ACT(c!= NULL, goto end_demuxer_init);
        
        c->ready= 0;
        c->reset= 0;
        c->id= mm_id; // set multimedia stream identificator (e.g. for video, audio, etc.)
        c->demuxer= d;
        
        // mutex
        pthread_mutex_init(&c->mutex, NULL);
        pthread_cond_init(&c->consume_cond, NULL);
        pthread_cond_init(&c->produce_cond, NULL);
        
        // Set methods
        if(mm_id== MM_VIDEO)
        {
            c->prologue= demuxer_init_ctx->callbacksVideo.prologue;
            c->epilogue= demuxer_init_ctx->callbacksVideo.epilogue;
            c->render= demuxer_init_ctx->callbacksVideo.render;
            c->opaque= demuxer_init_ctx->callbacksVideo.opaque;
        }
        else if(mm_id== MM_AUDIO)
        {
            c->prologue= demuxer_init_ctx->callbacksAudio.prologue;
            c->epilogue= demuxer_init_ctx->callbacksAudio.epilogue;
            c->render= demuxer_init_ctx->callbacksAudio.render;
            c->opaque= demuxer_init_ctx->callbacksAudio.opaque;
        }
        else
        {
            c->prologue= NULL;
            c->epilogue= NULL;
            c->render= NULL;
            c->opaque= NULL;
        }
        //TODO: presentation threading
    }

	/* Launch "receiver" thread in a joinable (default) state.
     * NOTE: Set 'can_run_demux'= 1 and 'interrupt_IO_FFmpeg'= 0 previously
     */
    demuxer->can_run_demux= 1;
    demuxer->interrupt_IO_FFmpeg= 0;
	pthread_create(&demuxer->recv_thread, NULL, thrDemuxer, (void*)demuxer);

    end_code= SUCCESS;

end_demuxer_init:

    if(end_code!= SUCCESS)
    {
        //demuxer_release(demuxer); // Must be released by calling thread
    }

	return end_code;
}

void demuxer_release(demuxerCtx* demuxer)
{
    ASSERT_ARG(demuxer!= NULL, ASSERT_VOID);

	LOGV("\n");

	int mm_id= 0;
	demuxer->can_run_demux= 0;
	demuxer->interrupt_IO_FFmpeg= 1;

	//==== Join "receiver" and "distribution" threads ====

    /* Unlock mutual exclusion areas */

    // For presentation
    for(mm_id= 0; mm_id< MM_IDNUM; mm_id++)
    {
        thrPresentationCtx* c= demuxer->presentat_ctx[mm_id];
        ASSERT_ACT(c!= NULL, continue);
        
        // wake up any waiting condition before destroying
        LOGV("Sending bradcast to mm id: %d\n", mm_id);
        pthread_mutex_lock(&c->mutex);
        demuxer->can_run_demux= 0; // redundant (for the sake of "readability")
        c->ready= 1;
        pthread_cond_broadcast(&c->consume_cond);
        pthread_cond_broadcast(&c->produce_cond);
        pthread_mutex_unlock(&c->mutex);
    }
    
    // For decoder
    for(mm_id= 0; mm_id< MM_IDNUM; mm_id++)
    {
        thrDecCtx* c= demuxer->dec_ctx[mm_id];
        ASSERT_ACT(c!= NULL, continue);
        
        // wake up any waiting condition before destroying
        LOGV("Sending bradcast to mm id: %d\n", mm_id);
        pthread_mutex_lock(&c->dec_mutex);
        demuxer->can_run_demux= 0; // redundant (for the sake of "readability")
        pthread_cond_broadcast(&c->consume_cond);
        pthread_cond_broadcast(&c->produce_cond);
        pthread_mutex_unlock(&c->dec_mutex);
    }

    // For demuxer
    LOGV("Sending bradcast to demuxer threads\n");
    pthread_mutex_lock(&demuxer->mutex);
    demuxer->can_run_demux= 0; // redundant
    pthread_cond_signal(&demuxer->produce_cond);
    pthread_cond_signal(&demuxer->consume_cond);
    pthread_mutex_unlock(&demuxer->mutex);

    // For presentation
    for(mm_id= 0; mm_id< MM_IDNUM; mm_id++)
    {
        thrPresentationCtx* c= demuxer->presentat_ctx[mm_id];
        ASSERT_ACT(c!= NULL, continue);
        
        // wake up any waiting condition before destroying
        pthread_mutex_lock(&c->mutex);
        demuxer->can_run_demux= 0; // redundant (for the sake of "readability")
        c->ready= 1;
        pthread_cond_broadcast(&c->consume_cond);
        pthread_cond_broadcast(&c->produce_cond);
        pthread_mutex_unlock(&c->mutex);
    }

    // For decoder
    for(mm_id= 0; mm_id< MM_IDNUM; mm_id++)
    {
        thrDecCtx* c= demuxer->dec_ctx[mm_id];
        ASSERT_ACT(c!= NULL, continue);
        
        // wake up any waiting condition before destroying
        pthread_mutex_lock(&c->dec_mutex);
        demuxer->can_run_demux= 0; // redundant (for the sake of "readability")
        pthread_cond_broadcast(&c->consume_cond);
        pthread_cond_broadcast(&c->produce_cond);
        pthread_mutex_unlock(&c->dec_mutex);
    }

	LOGD("Waiting on recv thread to join...\n");
	pthread_join(demuxer->recv_thread, NULL);
	LOGD("Recv thread joined O.K.!\n");

    LOGD("Waiting on dist thread to join...\n");
    pthread_join(demuxer->dist_thread, NULL);
    LOGD("Dist thread joined O.K.!\n");

	//==== Release multimedia threaded-decoding related ====
	// (IMPORTANT: set 'can_run_demux= 0' before)
	for(mm_id= 0; mm_id< MM_IDNUM; mm_id++)
	{
		thrDecCtx* c= demuxer->dec_ctx[mm_id];
        ASSERT_ACT(c!= NULL, continue);

		// join
		LOGD("Waiting on dec thread '%d' to join...\n", mm_id);
		pthread_join(c->dec_thread, NULL);
		LOGD("Thread '%d' joined O.K.!\n", mm_id);

		// release threaded-decoding related structure
		pthread_mutex_destroy(&c->dec_mutex);
		pthread_cond_destroy(&c->consume_cond);
		pthread_cond_destroy(&c->produce_cond);

		// Deallocate context
		free(c);
        demuxer->dec_ctx[mm_id]= NULL;
	}

    //==== Release multimedia threaded-presentation related ====
    // (IMPORTANT: set 'can_run_demux= 0' before)
    for(mm_id= 0; mm_id< MM_IDNUM; mm_id++)
    {
        thrPresentationCtx* c= demuxer->presentat_ctx[mm_id];
        ASSERT_ACT(c!= NULL, continue);

        // avoid entering rendering method
        // (WARNING: epilogue should not set to NULL!)
        c->render= NULL;
        
#if 0 // presentation threading not used
        // join
        LOGD("Waiting on dec thread '%d' to join...\n", mm_id);
        pthread_join(c->thread, NULL);
        LOGD("Thread '%d' joined O.K.!\n", mm_id);
#endif
        
        // release threaded-decoding related structure
        pthread_mutex_destroy(&c->mutex);
        pthread_cond_destroy(&c->consume_cond);
        pthread_cond_destroy(&c->produce_cond);

        // Deallocate context
        free(c);
        c= NULL;
    }

    pthread_mutex_destroy(&demuxer->mutex);
    pthread_cond_destroy(&demuxer->consume_cond);
    pthread_cond_destroy(&demuxer->produce_cond);

	/* Release video decoding context */
	if(demuxer->video_dec_ctx!= NULL)
	{
		avcodec_close(demuxer->video_dec_ctx);
		demuxer->video_dec_ctx= NULL;
	}

	/* Release audio decoding context */
	if(demuxer->audio_dec_ctx!= NULL)
	{
		avcodec_close(demuxer->audio_dec_ctx);
		demuxer->audio_dec_ctx= NULL;
	}

	/* Close input file/url */
	if(demuxer->fmt_ctx!= NULL)
	{
		//demuxer->fmt_ctx->interrupt_callback.callback= NULL;
		//demuxer->fmt_ctx->interrupt_callback.opaque= NULL;
		avformat_close_input(&demuxer->fmt_ctx);
		demuxer->fmt_ctx= NULL; // redundant (FFmpeg)
	}

	/* Free allocated resources in demuxing context */
	//av_free_packet(&demuxer->pkt);
	if(demuxer->uri!= NULL)
	{
		free(demuxer->uri);
		demuxer->uri= NULL;
	}

	if(demuxer->pFrameConverted!= NULL)
	{
#ifdef _ANDROID_
        avpicture_free(demuxer->pFrameConverted);
#endif
		free(demuxer->pFrameConverted);
		demuxer->pFrameConverted= NULL;
	}
	LOGD("Exiting 'demuxer_release'\n");
}

/* get monotonic clock time in milliseconds */
static uint64_t get_monotonic_clock_time()
{
	uint64_t t;
	struct timespec monotime_curr;

	clock_gettime(CLOCK_MONOTONIC, &monotime_curr);
	t= (uint64_t)monotime_curr.tv_sec*1000+ 
			(uint64_t)monotime_curr.tv_nsec/1000000; //[millisecs]
	return t;
}

static void treat_persistant_connection_issue(demuxerCtx* demuxer, int mm_id, int reset)
{
	if(!reset) // error recieved
	{
		LOGE("persistant error (mm_id= %d)\n", mm_id); //comment-me

		/* Treat persistant connection issues (e.g. reset connection) */
		if(demuxer->persistant_error_msecs[mm_id]== 0)
		{
			demuxer->persistant_error_msecs[mm_id]= get_monotonic_clock_time();
			//LOGV("demuxer->persistant_error_msecs[mm_id]= '%lld'\n", demuxer->persistant_error_msecs[mm_id]); //comment-me

			demuxer->persistant_error_num[mm_id]= 0;
		}
		else
		{
			uint64_t persistant_error_msecs= get_monotonic_clock_time()- 
					demuxer->persistant_error_msecs[mm_id];
			LOGV("persistant error elapsed time= '%lld' (mm_id= %d)\n", persistant_error_msecs, mm_id); //comment-me

			demuxer->persistant_error_num[mm_id]++;
			LOGV("persistant error num= '%d'\n", demuxer->persistant_error_num[mm_id]); //comment-me

			if(persistant_error_msecs> (uint64_t)demuxer->persistant_error_msecs_limit)
			{
				/* Error tolerance reached */
				LOGV("Demultiplexer: persistant connection issue time-limit reached "
						"(set to %d secs), resetting player connection\n", 
						demuxer->persistant_error_msecs_limit/1000);
				demuxer->persistant_error_msecs[mm_id]= 0;

				if(demuxer->persistant_error_num[mm_id]> 10)
				{
					demuxer->persistant_error_num[mm_id]= 0;
					demuxer->error_code= PLAYER_DECODING_ERROR;
					if(demuxer->on_error) demuxer->on_error((void*)demuxer);
				}
			}
		}
	}
	//else
	//{
	//	/* Reset error tolerance clk */
	//	demuxer->persistant_error_msecs[mm_id]= 0;
	//}
}
