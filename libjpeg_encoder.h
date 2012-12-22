#ifndef __LIBJPEG_ENCODER_H__
#define __LIBJPEG_ENCODER_H__
/*
 * libjpeg_encoder.h: 
 * 
 * Declares class libjpeg_encoder_t for use in producing JPEG-encoded
 * blob from a YUV image.
 *
 */

class libjpeg_encoder_t {
public:
	libjpeg_encoder_t( unsigned width,
			   unsigned height,
			   unsigned fourcc,
			   unsigned char const *data,
			   unsigned dataSize);
	~libjpeg_encoder_t( void );
	bool worked( void ) const { return (0 != jpegData_); }
	unsigned char const *jpegData(void) const { return jpegData_; }
	unsigned dataSize(void) const { return jpegSize_ ; }
private:
	unsigned char  *jpegData_ ;
	unsigned	jpegSize_ ;
};

#endif
