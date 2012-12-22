/*
 * libjpeg_encoder.cpp: define an encoder class to produce JPEG encoded
 * output from a YUV image.
 *
 */

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "hexDump.h"
#include "fourcc.h"

extern "C" {
#include <jpeglib.h>
};

#define BYTESPERMEMCHUNK 16384

struct memChunk_t {
   unsigned long  length_ ; // bytes used so far
   memChunk_t    *next_ ;   // next chunk
   JOCTET         data_[BYTESPERMEMCHUNK-8];
};

struct memDest_t {
   struct       jpeg_destination_mgr pub; /* public fields */
   memChunk_t  *chunkHead_ ;
   memChunk_t  *chunkTail_ ;
};

typedef memDest_t  *memDestPtr_t ;

static void dumpcinfo( jpeg_compress_struct const &cinfo )
{
   hexDumper_t dump( &cinfo, sizeof( cinfo ) );
   while( dump.nextLine() )
      printf( "%s\n", dump.getLine() );
   fflush( stdout );
}

static void dumpMemDest( memDest_t const &md )
{
   hexDumper_t dump( &md, sizeof( md ) );
   while( dump.nextLine() )
      printf( "%s\n", dump.getLine() );
   fflush( stdout );
}

/*
 * Initialize destination --- called by jpeg_start_compress
 * before any data is actually written.
 */
static void init_destination (j_compress_ptr cinfo)
{
   memDestPtr_t dest = (memDestPtr_t) cinfo->dest;
   
   assert( 0 == dest->chunkHead_ );

   /* Allocate first memChunk */
   dest->chunkHead_ = dest->chunkTail_ = new memChunk_t ;
   memset( dest->chunkHead_, 0, sizeof( *dest->chunkHead_ ) );
   dest->pub.next_output_byte = dest->chunkHead_->data_ ;
   dest->pub.free_in_buffer   = sizeof( dest->chunkHead_->data_ );
}


/*
 * Empty the output buffer --- called whenever buffer fills up.
 */

static boolean empty_output_buffer (j_compress_ptr cinfo)
{
   memDestPtr_t const dest = (memDestPtr_t) cinfo->dest;
   
   assert( 0 != dest->chunkTail_ );

   //
   // free_in_buffer member doesn't seem to be filled in
   //
   dest->chunkTail_->length_ = sizeof( dest->chunkTail_->data_ ); //  - dest->pub.free_in_buffer ;

   memChunk_t * const next = new memChunk_t ;
   memset( next, 0, sizeof( *next ) );
   dest->chunkTail_->next_ = next ;
   dest->chunkTail_ = next ;
   
   dest->pub.next_output_byte = next->data_ ;
   dest->pub.free_in_buffer   = sizeof( next->data_ );

   return TRUE;
}


/*
 * Terminate destination --- called by jpeg_finish_compress
 * after all data has been written.  Usually needs to flush buffer.
 *
 * NB: *not* called by jpeg_abort or jpeg_destroy; surrounding
 * application must deal with any cleanup that should happen even
 * for error exit.
 */
static void term_destination (j_compress_ptr cinfo)
{
   //
   // just account for data used
   //
   memDest_t  *const dest = (memDestPtr_t) cinfo->dest ;
   memChunk_t  *tail = dest->chunkTail_ ;
   assert( 0 != tail );   
   assert( sizeof( tail->data_ ) >= dest->pub.free_in_buffer );
   assert( tail->data_ <= dest->pub.next_output_byte );
   assert( tail->data_ + sizeof( tail->data_ ) >= dest->pub.next_output_byte + dest->pub.free_in_buffer );

   tail->length_ = sizeof( tail->data_ ) - dest->pub.free_in_buffer ;
}

/*
 * Prepare for output to a chunked memory stream.
 */
void jpeg_mem_dest( j_compress_ptr cinfo )
{
   assert( 0 == cinfo->dest );
   cinfo->dest = (struct jpeg_destination_mgr *)
                 (*cinfo->mem->alloc_small)
                     ( (j_common_ptr) cinfo, 
                       JPOOL_IMAGE,
		       sizeof(memDest_t)
                     );
   memDest_t *dest = (memDest_t *) cinfo->dest ;
   dest->pub.init_destination    = init_destination ;
   dest->pub.empty_output_buffer = empty_output_buffer ;
   dest->pub.term_destination    = term_destination ;
   dest->chunkHead_ = 
   dest->chunkTail_ = 0 ;
}

struct resolution_t {
	unsigned w ;
	unsigned h ;
};

static struct resolution_t const known_resolutions[] = {
	{ 640, 480 }
};

#define ARRAY_SIZE(__arr) (sizeof(__arr)/sizeof(__arr[0]))

static resolution_t const *find_res(unsigned ybytes){
	for (int i = 0 ; i < ARRAY_SIZE(known_resolutions); i++) {
		if (ybytes == (known_resolutions[i].w*known_resolutions[i].h)) {
			return known_resolutions+i;
		}
	}
	return 0 ;
}

#include "libjpeg_encoder.h"

libjpeg_encoder_t::libjpeg_encoder_t
	( unsigned width,
	  unsigned height,
	  unsigned fourcc,
	  unsigned char const *data,
	  unsigned dataSize)
	: jpegData_(0)
	, jpegSize_(0)
{
	if (!supported_fourcc(fourcc)) {
		fprintf (stderr, "Unsupported fourcc %s\n", fourcc_str(fourcc));
		return ;
	}
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
	if (!fourccOffsets(fourcc,
                           width,
                           height,
                           ysize,
                           yoffs,
                           yadder,
                           uvsize,
                           uvrowdiv,
                           uvcoldiv,
                           uoffs, 
                           voffs, 
                           uvadder,
                           totalsize)) {
		fprintf (stderr, "Error calculating params for fourcc %s\n", fourcc_str(fourcc));
		return ;
	}
	if (totalsize != dataSize) {
		fprintf (stderr, "data size mismatch: %u != %u\n\n", totalsize, dataSize);
		return ;
	}
        struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;
        cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_compress( &cinfo );
	cinfo.in_color_space = JCS_YCbCr; /* arbitrary guess */
	jpeg_set_defaults(&cinfo);
	cinfo.dct_method = JDCT_ISLOW;
	cinfo.in_color_space = JCS_YCbCr;
	cinfo.input_components = 3;
	cinfo.data_precision = 8;
	cinfo.image_width = (JDIMENSION)width;
	cinfo.image_height = (JDIMENSION)height;
	jpeg_set_colorspace(&cinfo,JCS_YCbCr);
	jpeg_set_quality(&cinfo,100,0);
	jpeg_mem_dest( &cinfo );
	jpeg_start_compress( &cinfo, TRUE );
	unsigned const row_stride = 3*sizeof(JSAMPLE)*width; // RGB
	JSAMPARRAY const buffer = (*cinfo.mem->alloc_sarray)( (j_common_ptr)&cinfo, JPOOL_IMAGE, row_stride, 1);
	unsigned char const *yIn = data+yoffs;
	unsigned char const *uIn = yIn+uoffs ;
	unsigned char const *vIn = yIn+voffs ;
	unsigned const uvstride = width/uvcoldiv;
	for( int row = 0 ; row < height ; row++ )
	{
		JSAMPLE *nextOut = buffer[0];
		unsigned rowoffs = (row/uvrowdiv)*uvstride;
		for( int col = 0 ; col < width ; col++ )
		{
			unsigned coloffs = col/uvcoldiv;
			unsigned char u = (uIn+rowoffs)[coloffs];
			unsigned char v = (vIn+rowoffs)[coloffs];
			*nextOut++ = *yIn++ ;
			*nextOut++ = u ;
			*nextOut++ = v ;
		} // for each column
		jpeg_write_scanlines( &cinfo, buffer, 1 );
	} // for each row
	jpeg_finish_compress( &cinfo );

        memDest_t * const dest = (memDest_t *)cinfo.dest ;
	memChunk_t *chunk = dest->chunkHead_ ;
	unsigned numChunks = 0 ;
	totalsize=0;
	while (chunk) {
		++numChunks ;
		totalsize += chunk->length_;
		chunk = chunk->next_ ;
	}

	jpegSize_ = totalsize ;
	unsigned char *nextOut = jpegData_ = new unsigned char [totalsize];
	totalsize=0;
	
	while (dest->chunkHead_) {
		chunk = dest->chunkHead_->next_ ;
		memcpy(nextOut,dest->chunkHead_->data_,dest->chunkHead_->length_);
		nextOut += dest->chunkHead_->length_;
		delete dest->chunkHead_ ;
                dest->chunkHead_ = chunk ;
	}
        jpeg_destroy_compress(&cinfo);
}

libjpeg_encoder_t::~libjpeg_encoder_t( void )
{
	if (jpegData_) {
		delete [] jpegData_ ;
	}
}


#ifdef __MODULETEST_LIBJPEG_ENCODER__
#include "memFile.h"
#include <stdlib.h>

int main (int argc, char const * const argv[])
{
	if (4 > argc) {
		printf( "Usage: %s infile w h\n", argv[0]);
		return -1 ;
	}
	memFile_t fIn(argv[1]);
	if (!fIn.worked()) {
		perror(argv[1]);
		return -1;
	}
	unsigned w = strtoul(argv[2],0,0);
	unsigned h = strtoul(argv[3],0,0);
	unsigned iterations = 0 ;
	while (1) {
		libjpeg_encoder_t encoder(w,h,0x32315559,
					  (unsigned char *)fIn.getData(),
					  fIn.getLength());
		if (encoder.worked()) {
			if ((4 < argc) && (0 == iterations)) {
				FILE *fOut = fopen(argv[4],"wb");
				if (fOut) {
					fwrite(encoder.jpegData(),encoder.dataSize(),1,fOut);
					fclose(fOut);
					printf( "wrote %u bytes to %s\n", encoder.dataSize(),argv[4]);
				} else
					perror(argv[4]);
			} else
				printf( "%u bytes\n", encoder.dataSize());
		} else {
			fprintf(stderr, "Error converting %s\n", argv[1]);
			break;
		}
		iterations++ ;
	}
	return 0 ;
}
#endif
