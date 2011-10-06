/*
 * Module imx_mpeg4_encoder.cpp
 *
 * This module defines the methods of the mpeg4_encoder_t class
 * as declared in imx_mpeg4_encoder.h
 *
 * Copyright Boundary Devices, Inc. 2010
 */

#include "imx_mpeg4_encoder.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#define DEBUGPRINT
#include "debugPrint.h"
#include "fourcc.h"
#include <assert.h>

#include <linux/videodev2.h>
#include <sys/ioctl.h>

#define STREAM_BUF_SIZE		0x80000

mpeg4_encoder_t::mpeg4_encoder_t(
	vpu_t &vpu,
	unsigned w,
	unsigned h,
	unsigned fourcc,
	unsigned gopSize,
	struct v4l2_buffer *v4lbuffers,
	unsigned numBuffers,
	unsigned char **cameraBuffers)
	: initialized_(false)
	, fourcc_(fourcc)
	, w_(w)
	, h_(h)
	, imgSize_(0)
	, handle_(0)
	, buffers(cameraBuffers)
	, yoffs(0)
	, uoffs(0)
	, voffs(0)
	, spsdata(0)
	, spslen(0)
	, ppsdata(0)
	, ppslen(0)
{
	if( (0 == w) || (0 == h) ) {
		fprintf(stderr, "Invalid w or h (%ux%u)\n", w, h );
		return ;
	}
	fprintf(stderr, "%s: %ux%u - %u buffers\n", __func__, w_, h_, numBuffers );
	vpu_mem_desc mem_desc = {0};

	unsigned ysize ;
	unsigned yadder ;
	unsigned uvsize ;
	unsigned uvrowdiv ;
	unsigned uvcoldiv ;
	unsigned uvadder ;
	unsigned totalsize ;
	if( !fourccOffsets(fourcc,w,h,ysize,yoffs,yadder,uvsize,uvrowdiv,uvcoldiv,uoffs,voffs,uvadder,totalsize) ){
		fprintf(stderr, "Invalid fourcc 0x%x\n", fourcc);
		return ;
	}
	imgSize_ = totalsize ;

printf( "%s: fourcc offsets %u/%u/%u, adders %u/%u\n", __func__, yoffs, uoffs,voffs, yadder,uvadder);
printf( "%s: sizes %u/%u: %u\n", __func__, ysize, uvsize, totalsize);

	/* get physical contigous bit stream buffer */
	mem_desc.size = STREAM_BUF_SIZE;
	if (0 != IOGetPhyMem(&mem_desc)) {
		fprintf(stderr,"Unable to obtain physical memory\n");
		return ;
	}

	/* mmap that physical buffer */
	virt_bsbuf_addr = IOGetVirtMem(&mem_desc);
	if (virt_bsbuf_addr <= 0) {
		fprintf(stderr,"Unable to map physical memory\n");
		IOFreePhyMem(&mem_desc);
		return ;
	}

	debugPrint( "stream buffer: %u bytes: phys 0x%lx, cpu 0x%lx, virt 0x%lx (%x)\n",
		    mem_desc.size, mem_desc.phy_addr, mem_desc.cpu_addr, mem_desc.virt_uaddr, virt_bsbuf_addr );

	phy_bsbuf_addr = mem_desc.phy_addr;

	EncOpenParam encop = {0};
	/* Fill up parameters for encoding */
	encop.bitstreamBuffer = phy_bsbuf_addr;
	encop.bitstreamBufferSize = STREAM_BUF_SIZE;
	encop.bitstreamFormat = STD_MPEG4 ;

	encop.picWidth = picwidth = w;
	encop.picHeight = picheight = h;

	/*Note: Frame rate cannot be less than 15fps per H.263 spec */
	encop.frameRateInfo = 30;
	encop.bitRate = 0 ;
	encop.gopSize = gopSize ;
	encop.slicemode.sliceMode = 0;	/* 0: 1 slice per picture; 1: Multiple slices per picture */
	encop.slicemode.sliceSizeMode = 0; /* 0: silceSize defined by bits; 1: sliceSize defined by MB number*/
	encop.slicemode.sliceSize = 4000;  /* Size of a slice in bits or MB numbers */

	encop.initialDelay = 0;
	encop.vbvBufferSize = 0;        /* 0 = ignore 8 */
	encop.intraRefresh = 0;
	encop.sliceReport = 0;
	encop.mbReport = 0;
	encop.mbQpReport = 0;
	encop.rcIntraQp = -1;
	encop.userQpMax = 0;
	encop.userGamma = (Uint32)(0.75*32768);         /*  (0*32768 <= gamma <= 1*32768) */
	encop.RcIntervalMode= 1;        /* 0:normal, 1:frame_level, 2:slice_level, 3: user defined Mb_level */
	encop.MbInterval = 0;

	if (uvsize == ysize/2) {
	    encop.EncStdParam.mjpgParam.mjpg_sourceFormat = 1 ; // YUV422 horizontal
	} else if (uvsize == ysize/4) {
	    encop.EncStdParam.mjpgParam.mjpg_sourceFormat = 0 ; // YUV420
	} else
		printf( "%s: unknown input format: %u/%u\n", __func__,ysize,uvsize );
printf( "%s: mjpg_source format %d\n", __func__, encop.EncStdParam.mjpgParam.mjpg_sourceFormat);
	encop.ringBufferEnable = 0;
	encop.dynamicAllocEnable = 0;
	encop.chromaInterleave = (uoffs < voffs) ? (uoffs+uvsize > voffs)
						 : (voffs+uvsize > uoffs);
	debugPrint("check open params\n");

	if (encop.bitstreamBuffer % 4) {	/* not 4-bit aligned */
		printf( "--> bitstreamBuffer %lx\n", encop.bitstreamBuffer);
	}
	if (encop.bitstreamBufferSize % 1024 ||
	    encop.bitstreamBufferSize < 1024 ||
	    encop.bitstreamBufferSize > 16383 * 1024) {
		printf( "--> bitstreamBufferSize %ld\n", encop.bitstreamBufferSize);
	}
	if (encop.bitstreamFormat != STD_MPEG4 &&
	    encop.bitstreamFormat != STD_H263 &&
	    encop.bitstreamFormat != STD_AVC &&
	    encop.bitstreamFormat != STD_MJPG) {
		printf( "--> bitstreamFormat %x\n", encop.bitstreamFormat);
	}
	if (encop.bitRate > 32767 || encop.bitRate < 0) {
		printf( "--> bitrate %d\n", encop.bitRate);
	}
	if (encop.bitRate != 0 && encop.initialDelay > 32767) {
		printf( "--> bitrate %d, initial delay %d\n", encop.bitRate, encop.initialDelay);
	}
	if (encop.bitRate != 0 && encop.initialDelay != 0 &&
	    encop.vbvBufferSize < 0) {
		printf( "--> bitrate %d, initial delay %d, vbvBufferSize %d\n", encop.bitRate, encop.initialDelay, encop.vbvBufferSize );
	}
	if (encop.gopSize > 60) {
		printf( "--> gopSize %d\n", encop.gopSize );
	}
	if (encop.slicemode.sliceMode != 0 && encop.slicemode.sliceMode != 1) {
		printf( "--> sliceMode %d\n", encop.slicemode.sliceMode );
	}
	if (encop.slicemode.sliceMode == 1) {
		if (encop.slicemode.sliceSizeMode != 0 &&
		    encop.slicemode.sliceSizeMode != 1) {
			printf( "--> slicemode.sliceSizeMode %d\n", encop.slicemode.sliceSizeMode );
		}
		if (encop.slicemode.sliceSize == 0) {
			printf( "--> slicemode.sliceSize %d\n", encop.slicemode.sliceSize );
		}
	}
	if (cpu_is_mx27()) {
		if (encop.sliceReport != 0 && encop.sliceReport != 1) {
			printf( "--> sliceReport %d\n", encop.sliceReport );
		}
		if (encop.mbReport != 0 && encop.mbReport != 1) {
			printf( "--> mbReport %d\n", encop.mbReport );
		}
	}
	if (encop.intraRefresh < 0 || encop.intraRefresh >=
	    (encop.picWidth * encop.picHeight / 256)) {
		debugPrint( "--> intraRefresh %d, width %d, height %d\n", encop.intraRefresh, encop.picWidth, encop.picHeight );
	}

	debugPrint( "format %d, %ux%u\n", encop.bitstreamFormat, encop.picWidth, encop.picHeight );

	if (encop.picWidth < 32 || encop.picHeight < 16) {
	debugPrint( "bad size\n");
	}

debugPrint( "opening encoder\n" );

	RetCode ret = vpu_EncOpen(&handle_, &encop);
	if (ret != RETCODE_SUCCESS) {
		fprintf(stderr,"Encoder open failed %d\n", ret);
		IOFreePhyMem(&mem_desc);
		return ;
	}

debugPrint( "encoder initialized\n" );

	SearchRamParam search_pa = {0};
	iram_t iram;
	int ram_size;

	memset(&iram, 0, sizeof(iram_t));
	ram_size = ((picwidth + 15) & ~15) * 36 + 2048;
	IOGetIramBase(&iram);
	if ((iram.end - iram.start) < ram_size) {
		debugPrint("vpu iram is less than needed: %u..%u/%u\n", iram.start,iram.end,ram_size);
		debugPrint("NOT Using IRAM for ME\n" );
	} else {
		/* Allocate max iram for vpu encoder search ram*/
		ram_size = iram.end - iram.start;
		search_pa.searchRamAddr = iram.start;
		search_pa.SearchRamSize = ram_size;
		debugPrint( "search iram %u..%u for %u bytes\n", iram.start, iram.end, ram_size );
		ret = vpu_EncGiveCommand(handle_, ENC_SET_SEARCHRAM_PARAM, &search_pa);
		if (ret != RETCODE_SUCCESS) {
			fprintf(stderr, "Encoder SET_SEARCHRAM_PARAM failed\n");
			IOFreePhyMem(&mem_desc);
			return ;
		}
		else {
			debugPrint("Using IRAM for ME\n" );
		}
	}

	debugPrint( "get initial info\n");
	EncInitialInfo initinfo = {0};
	ret = vpu_EncGetInitialInfo(handle_, &initinfo);
	if (ret != RETCODE_SUCCESS) {
		fprintf(stderr,"Encoder GetInitialInfo failed\n");
		IOFreePhyMem(&mem_desc);
		return ;
	}

	debugPrint( "have initial info\n" );

	fbcount = numBuffers ;
	int stride = ((picwidth + 15) & ~15)*((0 != encop.EncStdParam.mjpgParam.mjpg_sourceFormat)+1);

	fb = (FrameBuffer *)calloc(fbcount, sizeof(FrameBuffer));
	if (fb == NULL) {
		fprintf(stderr,"Failed to allocate fb\n");
		IOFreePhyMem(&mem_desc);
		return ;
	}
debugPrint( "allocated FrameBuffer fb: %p\n", fb );

	for (int i = 0; i < fbcount; i++) {
		struct v4l2_buffer const &buf = v4lbuffers[i];
		fb[i].bufY = buf.m.offset+yoffs;
		fb[i].bufCb = buf.m.offset+uoffs;
		fb[i].bufCr = buf.m.offset+voffs;
		fb[i].strideY = ((w+7)/8)*8;
		fb[i].strideC = (((w/2)+7)/8)*8;
	}
debugPrint( "registering frame buffer\n" );
	ret = vpu_EncRegisterFrameBuffer(handle_, fb, fbcount, stride, stride);
	if (ret != RETCODE_SUCCESS) {
		fprintf(stderr,"Register frame buffer failed\n");
		free(fb);
		IOFreePhyMem(&mem_desc);
		return ;
	}
	else
		debugPrint( "registered frame buffer\n" );

debugPrint( "%u frame buffers allocated and registered\n", fbcount );
	initialized_ = true ;
}

mpeg4_encoder_t::~mpeg4_encoder_t(void) {

	debugPrint( "closing encoder\n" );
	RetCode rc = vpu_EncClose(handle_);
	if( RETCODE_SUCCESS != rc )
		fprintf(stderr, "Error %d closing encoder\n", rc );
	else {
		debugPrint( "encoder closed\n" );
	}
}

bool mpeg4_encoder_t::get_bufs( unsigned index, unsigned char *&y, unsigned char *&u, unsigned char *&v )
{
	unsigned char *base = buffers[index];
	y = base + yoffs ;
	u = base + uoffs ;
	v = base + voffs ;
	return true ;
}

bool mpeg4_encoder_t::encode(unsigned index, void const *&outData, unsigned &outLength)
{
	EncParam  enc_param = {0};

	enc_param.sourceFrame = &fb[index];
	enc_param.quantParam = 23;
	enc_param.forceIPicture = 0;
	enc_param.skipPicture = 0;
	RetCode ret = vpu_EncStartOneFrame(handle_, &enc_param);
	if (ret != RETCODE_SUCCESS) {
		fprintf(stderr,"vpu_EncStartOneFrame failed Err code:%d\n",
								ret);
		return false ;
	}

	while (vpu_IsBusy()) {
		vpu_WaitForInt(30);
		if(vpu_IsBusy()){
			debugPrint( "busy\n");
		}
	}

	EncOutputInfo outinfo = {0};
	ret = vpu_EncGetOutputInfo(handle_, &outinfo);
	if (ret != RETCODE_SUCCESS) {
		fprintf(stderr,"vpu_EncGetOutputInfo failed Err code: %d\n",
								ret);
		return false ;
	}

	outData = (void *)(virt_bsbuf_addr + outinfo.bitstreamBuffer - phy_bsbuf_addr);
	outLength = outinfo.bitstreamSize ;
	return true ;
}

#ifdef MODULETEST
#include "cameraParams.h"
#define NUMBUFFERS 4
extern "C" {
#include "libavformat/avformat.h"
};

#define STREAM_DURATION   5.0
#define STREAM_FRAME_RATE 25 /* 25 images/s */
#define STREAM_NB_FRAMES  ((int)(STREAM_DURATION * STREAM_FRAME_RATE))

/* add a video output stream */
static AVStream *add_video_stream(AVFormatContext *oc,cameraParams_t &params)
{
    AVCodecContext *c;
    AVStream *st;

    st = av_new_stream(oc, 0);
    if (!st) {
        fprintf(stderr, "Could not alloc stream\n");
        exit(1);
    }

    c = st->codec;
    c->codec_id = CODEC_ID_H264;
    c->codec_type = AVMEDIA_TYPE_VIDEO;

    /* put sample parameters */
    c->bit_rate = 400000;
    /* resolution must be a multiple of two */
    c->width = params.getCameraWidth();
    c->height = params.getCameraHeight();
    /* time base: this is the fundamental unit of time (in seconds) in terms
       of which frame timestamps are represented. for fixed-fps content,
       timebase should be 1/framerate and timestamp increments should be
       identically 1. */
    c->time_base.den = params.getCameraFPS();
    c->time_base.num = 1;
    c->gop_size = params.getGOP(); /* emit one intra frame every twelve frames at most */
    c->pix_fmt = PIX_FMT_YUV420P ;
    c->flags |= CODEC_FLAG_GLOBAL_HEADER;

    return st;
}

static void dump_avoutfmt(struct AVOutputFormat *outfmt) {
	printf("--> avformat: %s/%s/%s/%s\n",
	       outfmt->name, outfmt->long_name, outfmt->mime_type, outfmt->extensions);
	printf("audio codec %d, video codec %d\n", outfmt->audio_codec, outfmt->video_codec);
}

// Clamp range of y
static unsigned yvalue(unsigned i){
	return (i%220)+16 ;
}

struct imgFile_t {
	char 		 *name ;
	unsigned 	  seconds ;
	unsigned	  iterations ;
	unsigned char 	 *yuvBytes ;
	struct imgFile_t *next ;
};

static imgFile_t *parseImgFiles(int &argc, char const **&argv,unsigned totalsize) {
        imgFile_t *rval = 0 ;
	for (int arg = argc-1 ; arg > 1 ; arg--) {
		char const *a = argv[arg];
		char const *colon = strchr(a,':');
		if (colon) {
			unsigned seconds = strtoul(colon+1,0,0);
			if (0 < seconds) {
				unsigned len = colon-a ;
				imgFile_t *img = new imgFile_t ;
				img->name=strdup(a);
				img->name[len] = '\0' ;
				img->seconds = seconds ;
				img->iterations = 0 ;
				img->next = rval ;
				FILE *fIn = fopen(img->name,"rb");
				if (fIn) {
					img->yuvBytes = new unsigned char [totalsize];
					if (totalsize != (unsigned)fread(img->yuvBytes,1,totalsize,fIn)) {
						perror(img->name);
					}
					rval = img ;
					--argc ;
					fclose(fIn);
				} else {
					perror(img->name);
					break;
				}
			} else
				fprintf (stderr, "%s: must be at least 1 second duration\n", a);
		} else
			break;
	}

	return rval ;
}

static bool fill_yuv(unsigned &iteration,cameraParams_t &params, imgFile_t *&images,unsigned char *yuv, unsigned ysize, unsigned uvsize){
	if (images) {
		if (0 == images->iterations) {
			printf( "reading %s\n", images->name);
		}
		images->iterations++ ;
		if (images->iterations <= (30*images->seconds)) {
			memcpy(yuv,images->yuvBytes,ysize+(2*uvsize));
			return true ;
		}
		images=images->next ;
		if (images) {
                        printf( "reading %s\n", images->name);
			images->iterations = 1 ;
			memcpy(yuv,images->yuvBytes,ysize+(2*uvsize));
			return true ;
		}
		else
			return false ;
	} else {
		unsigned frameCount = params.getIterations();
		if (0 == frameCount) {
			frameCount = NUMBUFFERS ;
		}
		if (iteration < frameCount) {
			unsigned    yval = yvalue(iteration);
			memset (yuv, yval,ysize);
			memset (yuv+ysize,0x80,2*uvsize);
			return true ;
		}
		else
			return false ;
	}
}

int main (int argc, char const **argv) {
	cameraParams_t params(argc,argv);
	if (1 < argc) {
		char const *outfile = argv[1];
		printf("save output to %s\n", outfile);
		char const *format = (2<argc) ? argv[2] : "MP4" ;
		vpu_t vpu ;
		if (vpu.worked()) {
			printf("vpu opened\n");
			params.dump();
			unsigned ysize;
			unsigned yoffs;
			unsigned yadder;
			unsigned uvsize;
			unsigned uvrowdiv;
			unsigned uvcoldiv;
			unsigned uoffs;
			unsigned voffs;
			unsigned uvadder;
			unsigned totalsize;
			if (fourccOffsets(params.getCameraFourcc(),params.getCameraWidth(),params.getCameraHeight(),
					  ysize, yoffs, yadder, uvsize, uvrowdiv, uvcoldiv, uoffs,  voffs,  uvadder, totalsize)) {
				imgFile_t *images = parseImgFiles(argc,argv,totalsize);
                                imgFile_t *im = images ;
				while (im) {
					printf("%s: %u seconds\n", im->name, im->seconds);
					im = im->next ;
				}
				printf("%s: %ux%u, total size %u\n",
				       fourcc_str(params.getCameraFourcc()),
				       params.getCameraWidth(),
				       params.getCameraHeight(),
				       totalsize);
				printf("yOffs: %u, uoffs %u, voffs %u\n", yoffs, uoffs, voffs);
				printf("ySize: %u, uvsize %u\n", ysize, uvsize);
				struct v4l2_buffer v4lbuffers[NUMBUFFERS];
				unsigned char *buffers[NUMBUFFERS];
				for (unsigned i = 0 ; i < NUMBUFFERS ; i++) {
					vpu_mem_desc mem_desc = {0};
					mem_desc.size = totalsize ;
					if (0 != IOGetPhyMem(&mem_desc)) {
						fprintf(stderr,"Unable to obtain physical memory\n");
						return -1 ;
					}
					v4lbuffers[i].m.offset = mem_desc.phy_addr ;
					int virt_bsbuf_addr = IOGetVirtMem(&mem_desc);
					if (virt_bsbuf_addr <= 0) {
						fprintf(stderr,"Unable to map physical memory\n");
						IOFreePhyMem(&mem_desc);
						return -1 ;
					}
                                        buffers[i] = (unsigned char *)virt_bsbuf_addr ;
				}
				printf("allocated %u buffers of %u bytes each\n", NUMBUFFERS,totalsize);
				mpeg4_encoder_t encoder(vpu,
						       params.getCameraWidth(),
						       params.getCameraHeight(),
						       params.getCameraFourcc(),
						       params.getGOP(),
						       v4lbuffers,
						       NUMBUFFERS,
						       buffers);
				if (encoder.initialized()) {
					AVRational codec_timebase = {1, 30};
					printf("Initialized encoder\n");
					av_register_all();
					struct AVOutputFormat *outfmt = av_oformat_next(0);
#if 1
					while (outfmt) {
						dump_avoutfmt(outfmt);
                                                outfmt = av_oformat_next(outfmt);
					}
#endif
					AVFormatContext *oc ;
					avformat_alloc_output_context2(&oc,NULL,NULL,outfile);
					
					outfmt= oc->oformat ;
					if (0 == outfmt) {
						fprintf (stderr, "unknown file format\n");
						return -1 ;
					}

					dump_avoutfmt(outfmt);

					AVStream *video_st = add_video_stream(oc, params);

					if (avio_open(&oc->pb, outfile, AVIO_FLAG_WRITE) < 0) {
						fprintf(stderr, "Could not open '%s'\n", outfile);
						return 1 ;
					}

					printf( "writing header: %d  programs, %d streams\n", oc->nb_programs, oc->nb_streams);
					printf("ocodec write_header: %p:%p/%p\n", oc->oformat ? oc->oformat->write_header : 0, outfmt, outfmt->write_header );
					av_write_header(oc);

					int64_t pts = 0LL ;
					unsigned i = 0 ;
					unsigned gopSize = (0 == params.getGOP()) ? 0xFFFFFFFF : params.getGOP();

					while (fill_yuv(i,params,images,buffers[i%NUMBUFFERS],ysize,uvsize)) {
                                                void const *outData ;
                                                unsigned    outLength ;
						if (encoder.encode(i%NUMBUFFERS,outData,outLength)) {
printf( "%02x %02x %02x %02x %02x\n", ((uint8_t *)outData)[0], ((uint8_t *)outData)[1], ((uint8_t *)outData)[2], ((uint8_t *)outData)[3], ((uint8_t *)outData)[4]);
							AVPacket pkt;
							av_init_packet(&pkt);

							if (0 == (i%gopSize)){
//								pkt.flags |= PKT_FLAG_KEY;
//                                                                ((uint8_t *)outData)[4] |= 0x05 ;
							}
							pkt.pts = 0x8000000000000000LL ; // av_rescale_q(i, codec_timebase, video_st->time_base); ;
							pkt.dts = pkt.pts ;
							pkt.stream_index= video_st->index;
							pkt.data= (uint8_t *)outData;
							pkt.size= outLength;
#if 1
							int ret = av_write_frame(oc, &pkt);
							if (0 != ret) {
								fprintf (stderr, "Error %d:%m writing video frame\n", ret);
								break;
							}
#endif
						} else
							fprintf (stderr, "encode error(%d): %p/%u\n", i,outData,outLength);
						i++ ;
					}
					int rval = av_write_trailer(oc);
					printf( "write trailer: %d\n", rval);
					avio_close(oc->pb);
				} else
					fprintf (stderr, "Error initializing encoder\n");
			} else {
				fprintf (stderr, "Unsupported fourcc\n");
			}
		} else
			fprintf (stderr, "Error connecting to VPU\n");
	}
	else
		fprintf (stderr, "Usage: %s [cameraparams] outfile[.mp4]\n", argv[0]);
	return 0 ;
}

#endif
