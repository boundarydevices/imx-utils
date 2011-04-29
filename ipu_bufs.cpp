#include "physMem.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <getopt.h>	     /* getopt_long() */

#include <fcntl.h>	      /* low-level i/o */
#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <malloc.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#include <asm/types.h>	  /* for videodev2.h */

#include <linux/fb.h>
#include <linux/videodev.h>
#include <linux/videodev2.h>
#include <linux/mxcfb.h>

#define IPU_CPMEM_REG_BASE	0x1F000000
#define PAGE_SIZE 4096

struct ipu_ch_param_word {
	unsigned data[5];
	unsigned res[3];
};

struct ipu_ch_param {
	struct ipu_ch_param_word word[2];
};

#define ARRAYSIZE(__arr) (sizeof(__arr)/sizeof(__arr[0]))

#define IPU_CM_REG_BASE		0x1E000000

static unsigned const buf_offsets[] = {
    0x268
,   0x270
};

static void print_field(
	char const *name,
	struct ipu_ch_param_word const &param_word,
	unsigned startbit,
	unsigned numbits)
{
	unsigned value ; 
	unsigned wordnum=(startbit/32); 
	if( wordnum == ((startbit+numbits-1)/32) ) { 
		unsigned shifted = (param_word.data[wordnum] >> (startbit & 31)); 
		value = shifted & ((1<<numbits)-1) ; 
	} /* fits in one word */	
	else { 
		unsigned lowbits = 32-(startbit&31); 
		unsigned low = (param_word.data[wordnum] >> (startbit & 31)) & ((1<<lowbits)-1) ; 
		unsigned highbits = numbits-lowbits ; 
		unsigned high = param_word.data[wordnum+1] & ((1<<highbits)-1); 
		value = low | (high << lowbits); 
	} 
	printf( "%s %3u:%-2u\t == 0x%08x\n", name, startbit, numbits, value ); 
}

struct bitfield {
	char const *name ;
	unsigned wordnum ;
	unsigned startbit ;
	unsigned numbits ;
};

static void set_field(
	struct ipu_ch_param_word &param_word,
	struct bitfield const &field, unsigned value)
{
	printf( "set field %s to value %u/0x%x here: start %u, count %u\n", field.name, value, value, field.startbit, field.numbits );
	unsigned const max = (1<<field.numbits)-1 ;
	if( value > max ){
		fprintf(stderr, "Error: range of %s is [0..0x%x]\n", field.name, max );
		return ;
	}
	unsigned wordnum=(field.startbit/32); 
	if( wordnum == ((field.startbit+field.numbits-1)/32) ) { 
		unsigned shifted = value << (field.startbit & 31);
		unsigned mask = ~(max << (field.startbit&31));
		unsigned oldval = param_word.data[wordnum] & ~mask ;
		printf( "single word value 0x%08x, mask 0x%08x, oldval 0x%08x\n", shifted, mask, oldval );
		if( shifted != oldval ) {
                        param_word.data[wordnum] = (param_word.data[wordnum]&mask) | shifted ;
			printf( "value changed\n");
		}
	} /* fits in one word */	
	else { 
		printf( "no multi-word support yet\n" );
		unsigned oldlow = param_word.data[wordnum];
		unsigned lowbits = 32-(field.startbit&31);
		unsigned lowval = value & ((1<<lowbits)-1);
		unsigned newlow = (oldlow&((1<<field.startbit)-1)) | (lowval<<field.startbit);
		printf( "oldlow == 0x%08x\nnewval == 0x%08x\n", oldlow, newlow);
		unsigned oldhigh = param_word.data[wordnum+1];
		unsigned highbits = field.numbits-lowbits;
		unsigned highval = value >> lowbits ;
		unsigned newhigh = (oldhigh&(~((1<<highbits)-1))) | highval ;
		printf( "oldhigh == 0x%08x\nnewval == 0x%08x\n", oldhigh, newhigh);
		param_word.data[wordnum] = newlow ;
		param_word.data[wordnum+1] = newhigh ;
	} 
}

static struct bitfield const fields[] = {
    {"XV",0,0,9},
    {"YV",0,10,9},
    {"XB",0,19,13},
    {"YB",0,32,12},
    {"NSB_B",0,44,1},
    {"CF",0,45,1},
    {"UBO",0,46,22},
    {"VBO",0,68,22},
    {"IOX",0,90,4},
    {"RDRW",0,94,1},
    {"BPP",0,107,3},
    {"SO",0,113,1},
    {"BNDM",0,114,3},
    {"BM",0,117,2},
    {"ROT",0,119,1},
    {"HF",0,120,1},
    {"VF",0,121,1},
    {"THE",0,122,1},
    {"CAP",0,123,1},
    {"CAE",0,124,1},
    {"FW",0,125,13},
    {"FH",0,138,12},
    {"EBA0",1,0,29},
    {"EBA1",1,29,29},
    {"ILO",1,58,20},
    {"NPB",1,78,7},
    {"PFS",1,85,4},
    {"ALU",1,89,1},
    {"ALBM",1,90,2},
    {"ID",1,93,2},
    {"TH",1,95,7},
    {"SLY",1,102,14},
    {"WID3",1,125,3},
    {"SLUV",1,128,14},
    {"CRE",1,149,1},
};

int main(int argc, char **argv )
{
	if( 2 > argc ){
	    fprintf(stderr, "Usage: %s buffernum [field value]\n", argv[0]);
	    return -1 ;
	}
	physMem_t cpmem(IPU_CPMEM_REG_BASE, PAGE_SIZE,O_RDWR);
	if( !cpmem.worked() ){
	perror("cpmem");
	return -1 ;
	}
	
	physMem_t cmmem(IPU_CM_REG_BASE, PAGE_SIZE);
	if( !cmmem.worked() ){
	perror("cmmem");
	return -1 ;
	}
	unsigned char const *rdy_base = (unsigned char *)cmmem.ptr();

	unsigned const *cur_buf_base = (unsigned *)(rdy_base + 0x23C);

	ipu_ch_param *params = (ipu_ch_param *)cpmem.ptr();
	unsigned chan = strtoul(argv[1],0,0);
	if( 80 >= chan ){
	    printf( "--------------- ipu ch %u ---------------\n", chan );
	    ipu_ch_param &param = params[chan];
	    for( unsigned i = 0 ; i < ARRAYSIZE(param.word); i++ ){
		    struct ipu_ch_param_word const &w = param.word[i];
		    unsigned addr = (char *)&w - (char *)params + IPU_CPMEM_REG_BASE ;
		    printf( "[%08x]: ", addr );
		    for(unsigned j = 0 ; j < ARRAYSIZE(w.data); j++ ) {
			    unsigned char *bytes = (unsigned char *)(w.data+j);
			    for( unsigned char b = 0 ; b < 4 ; b++ ){
				    printf( "%02x ", bytes[b]);
			    }
			    printf( " " );
		    }
		    printf("\n");
	    }
	    // printf( "bufs == %08x %08x\n", param.word[1].data[0], param.word[1].data[1]);
	    unsigned long addrs[] = {
		(param.word[1].data[0] & ((1<<29)-1))*8,
		((param.word[1].data[0] >> 29)|(param.word[1].data[1]<<3))*8
	    };
	    
	    unsigned curbuf_long = chan/32 ;
	    unsigned curbuf_bit = chan%31 ;
	    unsigned curbuf = (cur_buf_base[curbuf_long] >> curbuf_bit)&1 ;

	    for( unsigned i = 0 ; i < 2 ; i++ ){
		unsigned char bits = rdy_base[buf_offsets[i]+chan/8];
		unsigned char mask = 1<<(chan&7);
		bool ready = 0 != (bits & mask);
		printf( "ipu_buf[chan %u][%d] == 0x%08lx %s %s\n", chan, i, addrs[i], 
				ready ? "READY" : "NOT READY",
				(i==curbuf) ? "<-- current" : "");
	    }

	    if( 2 < argc ) {
		    char const *fieldname = argv[2];
		    for( unsigned i = 0 ; i < ARRAYSIZE(fields); i++ ){
			    if( 0 == strcmp(fields[i].name,fieldname)) {
				    print_field(fields[i].name,
						param.word[fields[i].wordnum],
						fields[i].startbit,
						fields[i].numbits);
				    if( 3 < argc ) {
					    unsigned const value = strtoul(argv[3],0,0);
					    set_field(param.word[fields[i].wordnum],fields[i],value);
				    }
				    return 0 ;
			    }
		    }
		    fprintf( stderr, "field %s not found\n", fieldname );
	    } else {
		    for( unsigned i = 0 ; i < ARRAYSIZE(fields); i++ ){
			    print_field(fields[i].name,param.word[fields[i].wordnum],fields[i].startbit,fields[i].numbits);
		    }
	    } // print all fields
	} else
	    fprintf(stderr, "Invalid channel %s, 0x%x\n", argv[1],chan);

	return 0 ;
}
