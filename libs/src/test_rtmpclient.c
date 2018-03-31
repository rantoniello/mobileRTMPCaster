#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include <time.h>
#include <sys/time.h>
#include <unistd.h> //usleep

#include "rtmpclient.h"

//#define SIMULATE_RT_READING
//#define TEST_BACKGROUND_VIDEO

/*---------------------------------------*/

#ifndef TEST_BACKGROUND_VIDEO

/* Auxiliary testing functions (for "PC" environment) */
static void *thrVideoEnc(void *p)
{
    rtmpMuxerCtx *muxer= (rtmpMuxerCtx*)p;

	/* get monotonic clock time */
	uint64_t t= get_monotonic_clock_time(muxer);

	rtmpclient_encode_vframe(muxer, t);

#ifdef SIMULATE_RT_READING
	pthread_exit(NULL);
#else
	return NULL;
#endif
}

static void *thrVideo(void *t)
{
#ifdef SIMULATE_RT_READING
	pthread_t vencthread;
	pthread_attr_t attr;
#endif
    rtmpMuxerCtx *muxer= (rtmpMuxerCtx*)t;

    /* Open sample video YUV file */
    FILE *yuv_file= fopen("./pipev"/*"./myfile.yuv"*/, "r");


	while(1)
	{
		/* Fill YUV buffers */
		unsigned int Y_size= muxer->v_height*muxer->v_width;
		unsigned int C_size= Y_size>>2;

	    if(muxer->picture->data[0]== NULL)
	    {
	    	LOGI("Error: picture buffer is not still initialized!");
	    	pthread_exit(NULL);
	    }

	    //memcpy(muxer->picture->data[0], framebuffer, c->width*c->height);
	    if(fread(muxer->picture->data[0], 1, Y_size, yuv_file)!= Y_size)
	    {
	    	LOGI("Error: error while reading Luma");
	    	pthread_exit(NULL);
	    }

		//memcpy(muxer->picture->data[2], &framebuffer[c->width*c->height], c->width*c->height/4);
	    if(fread(muxer->picture->data[1], 1, C_size, yuv_file)!= C_size)
	    {
	    	LOGI("Error: error while reading Cr");
	    	pthread_exit(NULL);
	    }
		//memcpy(muxer->picture->data[1], &framebuffer[c->width*c->height+c->width*c->height/4], c->width*c->height/4);
	    if(fread(muxer->picture->data[2], 1, C_size, yuv_file)!= C_size)
	    {
	    	LOGI("Error: error while reading Cb");
	    	pthread_exit(NULL);
	    }

	    /* Encode Picture! */
#ifdef SIMULATE_RT_READING
		pthread_attr_init(&attr);
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
		pthread_create(&vencthread, &attr, thrVideoEnc, (void *)muxer);
	    // Simulate real XX fps source
	    usleep((1000*1000)/muxer->v_frame_rate);
#else
	    thrVideoEnc((void *)muxer);
#endif
	} // while

#ifdef SIMULATE_RT_READING
	pthread_attr_destroy(&attr);
#endif
	pthread_exit(NULL);
}

static void *thrAudioEnc(void *p)
{
	rtmpMuxerCtx *muxer= (rtmpMuxerCtx*)p;

	/* get monotonic clock time */
	uint64_t t= get_monotonic_clock_time(muxer);

	rtmpclient_encode_aframe(muxer, t);

#ifdef SIMULATE_RT_READING
	pthread_exit(NULL);
#else
	return NULL;
#endif
}

static void *thrAudio(void *t)
{
#ifdef SIMULATE_RT_READING
	pthread_t aencthread;
	pthread_attr_t attr;
#endif
    rtmpMuxerCtx *muxer= (rtmpMuxerCtx*)t;

    /* Open sample video YUV file */
    FILE *pcm_file= fopen("./pipea"/*"./myfile.pcm_s16le"*/, "r");
	if(pcm_file== NULL)
	{
    	LOGI("Error: audio pipe could not be opened!");
    	goto end_thrAudio;
	}

	/* Allocate audio sample buffer */
    LOGI("muxer->adata_size: %d\n", muxer->adata_size);
	muxer->adata= av_malloc(muxer->adata_size*
					   av_get_bytes_per_sample(AV_SAMPLE_FMT_S16)*
					   muxer->a_nb_channels);
	if(muxer->adata== NULL || muxer->adata_size== 0)
	{
    	LOGI("Error: audio buffer could not be allocated!");
    	goto end_thrAudio;
	}

	while(1/*select(1, &rfds, NULL, NULL, &tv)== 0*/) //No data in stdin
	{
#ifdef SIMULATE_RT_READING
		uint32_t usecs_per_sample= (1000*1000)/muxer->a_sample_rate;
		unsigned int num_of_samples_read= muxer->adata_size; // 16-bit PCM samples!
		//num_of_samples_read/= 2; // 2 channels!! (stereo)
		// 44100 samples/sec
		// 1024 [samples buffer]
#endif

		/* Get a frame of audio samples */
	    //memcpy(...);
	    if(fread(muxer->adata, 2, muxer->adata_size, pcm_file)!= muxer->adata_size)
	    {
	    	LOGI("Error: error while reading audio frame!!");
	    	goto end_thrAudio;
	    }

	    /* Encode audio frame! */
#ifdef SIMULATE_RT_READING
		pthread_attr_init(&attr);
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
		pthread_create(&aencthread, &attr, thrAudioEnc, (void *)muxer);
	    // Simulate real XX fps source
	    usleep(usecs_per_sample*num_of_samples_read);
#else
	    thrAudioEnc((void *)muxer);
#endif
	}

end_thrAudio:

	if(pcm_file!= NULL)
	{
		fclose(pcm_file);
	}
	if(muxer->adata!= NULL)
	{
		av_freep(muxer->adata);
		muxer->adata= NULL;
	}

#ifdef SIMULATE_RT_READING
	pthread_attr_destroy(&attr);
#endif
	pthread_exit(NULL);
}

int static call_system_demultiplexor(const char *ifile, const char* duration)
{
	int ret= -1;
	char cmd[1024];
	sprintf(cmd, "LD_LIBRARY_PATH=../_install_dir_x86/lib/ "
			"../_install_dir_x86/bin/ffmpeg "
			"-re -i %s "
			"-vcodec rawvideo -r 30 -s 352x288 -f rawvideo -t %s -y ./pipev "
			"-acodec pcm_s16le -sample_rate 44100 -ac 1 -f s16le -t %s -y ./pipea",
			ifile, duration, duration);
	ret= system("mkfifo pipea; mkfifo pipev;");
	LOGI("Executing system call: '%s'\n", cmd);
	ret= system(cmd);
	return ret;
}

#endif // #ifndef TEST_BACKGROUND_VIDEO

int main(int argc, char **argv)
{
	rtmpMuxerCtx muxer;

	/* Check arguments */
    if(
       argc< (1+2) || argc> (1+3) //|| 
       //(strncmp(argv[2], "rtmp://", strlen("rtmp://"))!= 0 && strncmp(argv[2], "udp://", strlen("udp://"))!= 0)
    )
    {
        printf("usage: %s input_FILE output_RTMP_URI[rtmp://...] {duration[secs]};\n"
        	   "where {duration} is optative.\n"
               "API testing program:\n"
               "This program demultiplexes 'input_FILE' into raw audio/video, encodes both\n"
               "raw streams and muxes them into a RTMP stream 'output_RTMP_URI'.\n"
               "\n", argv[0]);
        return 1;
    }

	/* Initialize rtmp client context */
    rtmpclient_open(&muxer);

    /* Perform some customizations... */
    // video settings
	muxer.v_bit_rate= 400*1024;
	muxer.v_frame_rate= 30;
	muxer.v_width= 352;
	muxer.v_height= 288;
	muxer.v_gop_size= 15;
	muxer.v_pix_format= AV_PIX_FMT_YUV420P;
	muxer.v_codec_id= AV_CODEC_ID_H264;
	// audio settings
	muxer.a_bit_rate= 64000;
	muxer.a_sample_rate= 44100;
	muxer.a_nb_channels= 1; //mono
	muxer.a_codec_id= AV_CODEC_ID_AAC;
	//Container setting
	muxer.uri= strdup(argv[2]);
	muxer.o_mux_format= "flv"; //"flv"; //"mpegts";

	/* Init RTMP client */
    if(rtmpclient_init(&muxer)< 0 || muxer.oc== NULL)
    {
    	LOGI("'rtmpclient_init()' failed\n");
    	exit(-1);
    }

#ifndef TEST_BACKGROUND_VIDEO
    {
    	pthread_t vthread;
    	pthread_t athread;
    	pthread_attr_t attr;

		/* Create a/v threads in a joinable state */
		pthread_attr_init(&attr);
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
		pthread_create(&vthread, &attr, thrVideo, (void *)&muxer);
		pthread_create(&athread, &attr, thrAudio, (void *)&muxer);
	
		/* Once app. is running, fire demultiplexor... */
		if(call_system_demultiplexor(argv[1], argv[3]? argv[3]: "86400")!= 0)
		{
			LOGE("Error occurred while calling system demultiplexor\n");
			exit(-1);
		}
	
		/* Wait for all threads to complete */
		LOGI("Waiting on all threads to join...\n")
		pthread_join(vthread, NULL);
		pthread_join(athread, NULL);
		LOGI("Waited on all threads to join O.K.!\n")
		pthread_attr_destroy(&attr);
    }
#else
    {
    	int yuv_size= 0;
    	int size_read= 0;

    	/* Open YUV source file */
    	FILE *yuv_file= fopen("/home/ral/Dropbox/RAL/Projects/android/rtmpclient/rtmpclientAndroid/assets/NoVideoAvailable.yuv", "r");
    	if(yuv_file== NULL)
    	{
    		LOGE("Error occurred while opening 'yuv_file'\n");
    		exit(-1);
    	}

    	/* Allocate memory for background still image */
    	yuv_size= muxer.bckground_width* muxer.bckground_height;
    	yuv_size+= (yuv_size>>1);
    	muxer.bckgroundLogo= (char*)malloc(yuv_size);
    	if(muxer.bckgroundLogo== NULL)
    	{
	    	LOGI("Could not allocate memory buffer for background still image\n");
	    	pthread_exit(NULL);
    	}
    	
    	/* Read file into memory buffer; close file */
    	size_read= fread(muxer.bckgroundLogo, 1, yuv_size, yuv_file);
	    if(size_read!= yuv_size)
	    {
	    	LOGI("Error: error while reading YUV file "
	    			"(YUV size= %d; read bytes= %d; errno= %d %s)", 
	    			yuv_size, size_read, errno, strerror(errno) );
	    	pthread_exit(NULL);
	    }
    	fclose(yuv_file);
    	
    	/* Run "background video" thread */
    	rtmp_client_runBackground(&muxer);
    	usleep(1000*1000*60*10); // play for 10 minutes
    	//rtmp_client_stopBackground(&muxer); //NOTE: performed in 'rtmpclient_release()'
    }
#endif

	/* Clean up and exit */
    rtmpclient_release(&muxer.oc, &muxer);
    rtmpclient_close(&muxer);
    exit(0);
}
