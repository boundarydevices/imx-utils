#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdio.h>
#include <linux/fb.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <stdlib.h>

inline void noprintf(...){}
#define debugPrint noprintf

class fbDevice_t {
public:
        fbDevice_t(void);
	~fbDevice_t(void);

	bool isOpen(void){return 0 <= fd_ ;}
	void clear(unsigned short color);
	void hline(unsigned l,unsigned short color);
	void vline(unsigned l,unsigned short color);
	void flip();
	unsigned getWidth(void) const { return width_; }
	unsigned getHeight(void) const { return height_; }
	unsigned short *getMem(void) const { return (unsigned short *)(((char *)mem_)+(screenSize_*curBuf_)); }
	void set_cur_buf(unsigned cur) { curBuf_ = cur; }
private:
	int             fd_ ;
	unsigned short *mem_ ;
	unsigned long   memSize_ ;
	unsigned short  width_ ;
	unsigned short  height_ ;
	unsigned short  stride_;
	unsigned	curBuf_ ;
	unsigned	screenSize_ ;
        struct fb_var_screeninfo var ;
};

#ifdef ANDROID
#define FBDEVNAME "/dev/graphics/fb0"
#else
#define FBDEVNAME "/dev/fb0"
#endif

fbDevice_t::fbDevice_t(void)
	: fd_(open(FBDEVNAME,O_RDWR))
	, mem_(0)
	, memSize_(0)
	, width_(0)
	, height_(0)
	, stride_(0)
	, curBuf_(0)
	, screenSize_(0)
{

	if( 0 <= fd_ ){
		fcntl( fd_, F_SETFD, FD_CLOEXEC );
		struct fb_fix_screeninfo fixed_info;
		int err = ioctl( fd_, FBIOGET_FSCREENINFO, &fixed_info);
		if( 0 == err ){
			struct fb_var_screeninfo variable_info;

			err = ioctl( fd_, FBIOGET_VSCREENINFO, &variable_info );
			if( 0 == err ){
				width_    = variable_info.xres ;
				height_   = variable_info.yres ;
				stride_   = fixed_info.line_length;
				memSize_  = fixed_info.smem_len ;
                                screenSize_ = height_*stride_ ;
				mem_ = (unsigned short *)mmap( 0, memSize_, PROT_WRITE|PROT_WRITE, MAP_SHARED, fd_, 0 );
				if( MAP_FAILED != (void *)mem_ )
				{
					var = variable_info ;
					debugPrint( "mem at %p\n", mem_ );
					return ;
				}
				else
					perror( "mmap fb" );
			}
			else
				perror("FBIOGET_VSCREENINFO");
		}
		else
			perror("FBIOGET_FSCREENINFO");

		::close(fd_);
		fd_ = -1 ;
        } else
		perror(FBDEVNAME);
}

fbDevice_t::~fbDevice_t(void)
{
	if( isOpen() ){
		::close(fd_);
		fd_ = -1 ;
	}
}

void fbDevice_t::clear(unsigned short color)
{
	if(isOpen()){
		unsigned long clong=(color<<16)|color;
		memset(mem_,clong,memSize_);
	}
}

void fbDevice_t::hline(unsigned l,unsigned short color){
	unsigned short *buf = getMem();
	debugPrint( "line in buf %p, color %x\n", buf,color);
	buf += (stride_*l)/sizeof(*buf);
	unsigned long clong=(color<<16)|color;
	memset(buf,color,stride_);
}

void fbDevice_t::vline(unsigned l,unsigned short color){
	unsigned short *buf = getMem();
	unsigned h = getHeight();
	debugPrint( "line in buf %p, color %x\n", buf,color);
	buf += l;
	while (h) {
		*buf = color;
		buf = (unsigned short*)(((unsigned char*)buf) + stride_);
		h--;
	}
}

#define MXCFB_WAIT_FOR_VSYNC	_IOW('F', 0x20, u_int32_t)

void fbDevice_t::flip(){
	var.yoffset = curBuf_*height_;
	int err ;
again:
	err = ioctl(fd_,MXCFB_WAIT_FOR_VSYNC,0);
	if (0 != err)
                perror("MXCFB_WAIT_FOR_VSYNC");
	err = ioctl(fd_,FBIOPAN_DISPLAY,&var);
	if(err) {
		perror("FBIOPAN_DISPLAY");
		if (-EBUSY == err) {
			goto again ;
		}
	} else
		printf( "flipped to buffer %u\n", curBuf_ );
}

int main(int argc, char const * const argv[]){
	fbDevice_t fb ;
	if( fb.isOpen() ){
		fb.clear(0xffff);
		unsigned prevline[2];
		unsigned line[2];
		unsigned cur = 0;
		prevline[0] = prevline[1] = 0;
		line[0] = 0;
		line[1] = 1;
		while(1){
			fb.set_cur_buf(cur);
			fb.vline(prevline[cur],0xffff);
			fb.vline(line[cur],0);
			prevline[cur] = line[cur];
			line[cur] = (line[cur]+2)%fb.getWidth();
			fb.flip();
			cur ^= 1;
		}
	} else
		perror( "fbdev");
	return 0 ;
}
