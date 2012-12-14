/*
 * devregs - tool to display and modify a device's registers at runtime
 *
 * Use cases:
 *
 *	devregs
 *		- display all registers
 *
 *	devregs register
 *		- display all registers matching register (strcasestr)
 *
 *	devregs register.field
 *		- display all registers matching register (strcasestr)
 *		- also break out specified field
 *
 *	devregs register value
 *		- set register to specified value (must match single register)
 *
 *	devregs register.field value
 *		- set register field to specified value (read/modify/write)
 *
 * Registers may be specified by name or 0xADDRESS. If specified by name, all
 * registers containing the pattern are considered. If multiple registers 
 * match on a write request (2-parameter use cases), no write will be made.
 *
 * fields may be specified by name or bit numbers of the form "start[-end]"
 *
 * (c) Copyright 2010 by Boundary Devices under GPLv2
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>

static bool word_access = false ;

struct fieldDescription_t {
	char const 		   *name ;
	unsigned    		   startbit ;
	unsigned    		   bitcount ;
	struct fieldDescription_t *next ;
};

struct registerDescription_t {
	char const 		*name ;
	fieldDescription_t 	*fields ;
};

struct reglist_t {
	unsigned long 			 address ;
	unsigned		 	 width ; // # bytes in register
	struct registerDescription_t	*reg ;
	struct fieldDescription_t	*fields ;
	struct reglist_t		*next ;
};

struct	fieldSet_t {
	char const 			*name ;
	struct fieldDescription_t	*fields ;
        struct	fieldSet_t 		*next ;
};

static char const devregsPath[] = {
	"/etc/devregs.dat"
};

static struct fieldSet_t *fieldsets = 0 ;

/* 
 * strips comments as well as skipping leading spaces
 */
char *skipSpaces(char *buf){
	char *comment = strchr(buf,'#');
	if (comment)
		*comment = '\0' ;
	comment = strstr(buf,"//");
	if (comment)
		*comment = 0 ;
	while( *buf ){
		if( isprint(*buf) && (' ' != *buf) )
			break;
		buf++ ;
	}
	return buf ;
}

static void trimCtrl(char *buf){
	char *tail = buf+strlen(buf);
	// trim trailing <CR> if needed
	while( tail > buf ){
		--tail ;
		if( iscntrl(*tail) ){
			*tail = '\0' ;
		} else
			break;
	}
}

static bool parseBits(char const *bitspec, unsigned &start, unsigned &count)
{
	char *end ;
	unsigned startbit = strtoul(bitspec,&end,0);
	if( (31 >= startbit)
	    &&
	    ( ('\0' == *end)
	      ||
	      ('-' == *end) ) ){
		unsigned endbit ;
		if( '-' == *end ){
			endbit = strtoul(end+1,&end,0);
			if('\0' != *end){
				endbit = ~startbit ;
			}
		} else {
			endbit = startbit ;
		}
		if(endbit<startbit) {
			endbit ^= startbit ;
			startbit ^= endbit ;
			endbit  ^= startbit ;
		}
		unsigned const bitcount = endbit-startbit+1 ;
		if( bitcount <= (32-startbit) ){
			start = startbit ;
			count = bitcount ;
			return true ;
		} else
			fprintf(stderr, "Invalid bitspec '%s'. Use form 'start-end' in decimal (%u,%u,%u)\n", bitspec,startbit,endbit,bitcount );
	} else
		fprintf(stderr, "Invalid field '%s'. Use form 'start-end' in decimal (%u,%x)\n", bitspec,startbit,*end );

	return false ;
}

static struct fieldDescription_t *parseFields
	( struct reglist_t const *regs,
	  char const *fieldname )
{
	if(isdigit(*fieldname)){
		unsigned start, count ;
		if (parseBits(fieldname,start,count)){
			fieldDescription_t *f = new fieldDescription_t ;
			f->name = fieldname ;
			f->startbit = start ;
			f->bitcount = count ;
			f->next = 0 ;
			return f ;
		}
	} else if( regs ){
                struct fieldDescription_t *head = 0 ;
		struct fieldDescription_t *tail = 0 ;
		while( regs ){
			if( regs->reg ){
				struct fieldDescription_t *f = regs->reg->fields ;
				while( f ){
					if( 0 == strcasecmp(f->name,fieldname) ){
						struct fieldDescription_t *newf = new struct fieldDescription_t ;
						*newf = *f ;
						newf->next = 0 ;
						tail = newf ;
						if( 0 == head )
							head = newf ;
					}
					f = f->next ;
				}
			}
			regs = regs->next ;
		}
		return head ;
	} else {
		fprintf(stderr, "Can't parse named fields without matching registers\n" );
	}
	return 0 ;
}

/*
 * registerDefs()	- parses register specs from devRegsPath
 *
 *	- Outer loop determines which type of line we're dealing with
 *	based on the first character:
 *		A-Za-z_		- Register:	Name	0xADDRESS[.w|.l|.b]
 *		:		- Field		:fieldname:startbit[-stopbit]
 *		/		- Field set	/Fieldsetname
 *
 *	state field is used to determine whether a field will be added to the
 *	most recent register or fieldset.
 */
enum ftState {
	FT_UNKNOWN	= -1,
	FT_REGISTER	= 0,
	FT_FIELDSET	= 1
};

static char const *getDataPath(unsigned cpu) {
	switch (cpu & 0xff000) {
		case 0x63000:
			return "/etc/devregs_imx6x.dat" ;
		case 0x53000:
			return "/etc/devregs_imx53.dat" ;
		default:
			printf("unsupported CPU type: %x\n", cpu);
	}
	return "/etc/devregs.dat" ;
}

static struct reglist_t const *registerDefs(unsigned cputype = 0){
	static struct reglist_t *regs = 0 ;
	if( 0 == regs ){
		struct reglist_t *head = 0, *tail = 0 ;
		FILE *fDefs = fopen(getDataPath(cputype), "rt");
		if( fDefs ){
                        enum ftState state = FT_UNKNOWN ;
			char inBuf[256];
			int lineNum = 0 ;
			while( fgets(inBuf,sizeof(inBuf),fDefs) ){
				lineNum++ ;
				// skip unprintables
                                char *next = skipSpaces(inBuf);
				if( *next && ('#' != *next) ){
					trimCtrl(next);
				} // not blank or comment
				if(isalpha(*next) || ('_' == *next)){
					char *start = next++ ;
					while(isalnum(*next) || ('_' == *next)){
						next++ ;
					}
					if(isspace(*next)){
						char *end=next-1 ;
						next=skipSpaces(next);
						if(isxdigit(*next)){
							char *addrEnd ;
							unsigned long addr = strtoul(next,&addrEnd,16);
							unsigned width = 4 ;
							if( addrEnd && ('.' == *addrEnd) ){
								char widthchar = tolower(addrEnd[1]);
								if('w' == widthchar) {
									width = 2 ;
								} else if( 'b' == widthchar) {
									width = 1 ;
								} else if( 'l' == widthchar) {
									width = 4 ;
								}
								else {
									fprintf(stderr, "Invalid width char %c on line number %u\n", widthchar, lineNum);
									continue;
								}
								addrEnd = addrEnd+2 ;
							}
							if( addrEnd && ('\0'==*addrEnd)){
								unsigned namelen = end-start+1 ;
								char *name = (char *)malloc(namelen+1);
								memcpy(name,start,namelen);
								name[namelen] = '\0' ;
                                                                struct reglist_t *newone = new reglist_t ;
								newone->address=addr ;
								newone->width = width ;
								newone->reg = new registerDescription_t ;
								newone->reg->name = name ;
								newone->reg->fields = newone->fields = 0 ;
								if(tail){
									tail->next = newone ;
								} else
									head = newone ;
								tail = newone ;
                                                                state = FT_REGISTER ;
//								printf( "%s: 0x%x, width %u\n", newone->reg->name, newone->address, newone->width);
								continue;
							}
							else
								fprintf(stderr, "expecting end of addr, not %c\n", addrEnd ? *addrEnd : '?' );
						}
						else
							fprintf(stderr, "expecting hex digit, not %02x\n", (unsigned char)*next );
					}
					fprintf(stderr, "%s: syntax error on line %u <%s>\n", getDataPath(cputype), lineNum,next );
				} else if((':' == *next) && (FT_UNKNOWN != state)) {
                                        next=skipSpaces(next+1);
					char *start = next++ ;
					while(isalnum(*next) || ('_' == *next)){
						next++ ;
					}
					unsigned nameLen = next-start ;
					char *name = new char [nameLen+1];
					memcpy(name,start,nameLen);
					name[nameLen] = 0 ;
					if( ':' == *next ){
						struct fieldDescription_t *field = parseFields(tail,next+1);
						if(field){
							field->name = name ;
							if (FT_REGISTER == state) {
								field->next = tail->fields ;
								tail->fields = field ;
							} else {
								field->next = fieldsets->fields ;
								fieldsets->fields = field ;
							}
						} else 
                                                        fprintf( stderr, "error parsing field at line %u\n", lineNum );
					} else if (('/' == *next) && (FT_REGISTER == state)) {
						struct fieldSet_t const *fs = fieldsets ;
						while (fs) {
							if (0 == strcmp(fs->name,name))
								break;
							fs = fs->next ;
						}
						if (fs) {
							if (tail->fields) {
                                                                struct fieldDescription_t *back = tail->fields ;
                                                                struct fieldDescription_t *front = back ;
								while(front) {
									back = front ;
									front = back->next ;
								}
								back->next = fs->fields ;
							} else
                                                                tail->fields = fs->fields ;
							state = FT_UNKNOWN ; /* don't allow fields to be added */
						}
					} else {
						fprintf( stderr, "missing field separator at line %u\n", lineNum );
					}
				} else if ('/' == *next) {
					char *start = ++next ;
					while(isalnum(*next) || ('_' == *next)){
						next++ ;
					}
					if ((start < next) && (isspace(*next) || ('\0'==*next))) {
						*next = '\0' ;
                                                struct	fieldSet_t  *fs = (struct fieldSet_t *)malloc(sizeof(struct fieldSet_t ));
						fs->name = strdup(start);
						fs->fields = 0 ;
						fs->next = fieldsets ;
						fieldsets = fs ;
						state = FT_FIELDSET ;
					} else
						fprintf(stderr,"Invalid fieldset name %s\n",start-1);
				} else if (*next && ('#' != *next)) {
					fprintf(stderr, "Unrecognized line <%s> at %u\n", next, lineNum );
				}
			}
			fclose(fDefs);
			regs = head ;
		} else 
			perror(getDataPath(cputype));
	}
	return regs ;
}

static struct reglist_t const *parseRegisterSpec(char const *regname)
{
	char const c = *regname ;

	if(isalpha(c) || ('_' == c)){
                struct reglist_t *out = 0 ;
                struct reglist_t const *defs = registerDefs();
		char *regPart = strdup(regname);
		char *fieldPart = strchr(regPart,'.');
		bool widthspec = false ;
		unsigned fieldLen = 0 ;
		if (0 == fieldPart) {
			fieldPart = strchr(regPart,':');
			widthspec = false ;
		}
		else
			widthspec = true ;
		if (fieldPart) {
			*fieldPart++ = '\0' ;
			fieldLen = strlen(fieldPart);
		}
		unsigned const nameLen = strlen(regname);
		while(defs){
                        if( 0 == strncasecmp(regPart,defs->reg->name,nameLen) ) {
				struct reglist_t *newOne = new struct reglist_t ;
				memcpy(newOne,defs,sizeof(*newOne));
				if (fieldPart) {
					newOne->fields = 0 ;
					if (isdigit(*fieldPart)) {
						unsigned start, count ;
						if (parseBits(fieldPart,start,count)) {
							fieldDescription_t *newf = new struct fieldDescription_t ;
							newf->name = fieldPart ;
							newf->startbit = start ;
							newf->bitcount = count ;
							newf->next = 0 ;
							newOne->fields = newf ;
						}
						else
							return 0 ;
					} else {
						fieldDescription_t *rhs = defs->fields ;
						while (rhs) {
							if( 0 == strcasecmp(fieldPart,rhs->name) ) {
								fieldDescription_t *newf = new struct fieldDescription_t ;
								memcpy(newf,rhs,sizeof(*newf));
								newf->next = newOne->fields ;
								newOne->fields = newf ;
							}
							rhs = rhs->next ;
						}
					} // search for named fields
				} // only copy specified field
				newOne->next = out ;
				out = newOne ;
			}
			defs = defs->next ;
		}
		free(regPart);
		return out ;
	} else if(isdigit(c)){
		char *end ;
		unsigned long address = strtoul(regname,&end,16);
		if( (0 == *end) || (':' == *end) || ('.' == *end) ){
                        struct fieldDescription_t *field = 0 ;
			unsigned start, count ;
			struct reglist_t *out = 0 ;
			struct reglist_t const *defs = registerDefs();
			unsigned const nameLen = strlen(regname);
			while(defs){
				if( defs->address == address ) {
					out = new struct reglist_t ;
					memcpy(out,defs,sizeof(*out));
					out->next = 0 ;
					out->fields = field ;
					break;
				}
				defs = defs->next ;
			}

			if (':' == *end) {
				unsigned start, count ;
				if (parseBits(end+1,start,count)) {
					field = new struct fieldDescription_t ;
					field->name = end+1 ;
					field->startbit = start ;
					field->bitcount = count ;
					field->next = 0 ;
				}
			}
			struct fieldDescription_t *fields = 0 ;
			unsigned width = 4 ;
			if( '.' == *end ){
				char widthchar=tolower(end[1]);
				if ('w' == widthchar) {
					width = 2 ;
				} else if ('b' == widthchar) {
					width = 1 ;
				} else if ('l' == widthchar) {
					width = 4 ;
				} else {
					fprintf( stderr, "Invalid width char <%c>\n", widthchar);
				}
			}
			if( 0 == out ){
                                out = new struct reglist_t ;
				out->address = address ;
				out->width = width ;
				out->reg = 0 ;
				out->fields = fields ;
				out->next = 0 ;
			}
			return out ;
		} else {
			fprintf( stderr, "Invalid register name or value '%s'. Use name or 0xHEX\n", regname );
		}
	} else {
		fprintf( stderr, "Invalid register name or value '%s'. Use name or 0xHEX\n", regname );
	}
	return 0 ;
}

static int getFd(void){
	static int fd = -1 ;
	if( 0 > fd ){
		fd = open("/dev/mem", O_RDWR | O_SYNC);
		if (fd<0) {
			perror("/dev/mem");
			exit(1);
		}
	}
	return fd ;
}

#define MAP_SIZE 4096
#define MAP_MASK ( MAP_SIZE - 1 )

static unsigned long volatile *getReg(unsigned long addr){
	static void *map = 0 ;
	static unsigned prevPage = -1U ;
	unsigned page = addr & ~MAP_MASK ;
	if( page != prevPage ){
		if( map ){
		   munmap(map,MAP_SIZE);
		}
		map = mmap(0, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, getFd(), page );
		if( MAP_FAILED == map ){
			perror("mmap");
			exit(1);
		}
		prevPage = page ;
	}
	unsigned offs = addr & MAP_MASK ;
	return (unsigned long volatile *)((char *)map+offs);
}

static unsigned fieldVal(struct fieldDescription_t *f, unsigned long v)
{
	v >>= f->startbit ;
	v &= (1<<f->bitcount)-1 ;
	return v ;
}

static void showReg(struct reglist_t const *reg)
{
	unsigned long rv ; 
        unsigned long volatile *regPtr = getReg(reg->address);
	if( 2 == reg->width ) {
		unsigned short volatile *p = (unsigned short volatile *)regPtr ;
		rv = *p ;
		printf( "%s:0x%08lx\t=0x%04lx\n", reg->reg ? reg->reg->name : "", reg->address, rv );
	} else if( 4 == reg->width ) {
		unsigned long volatile *p = regPtr ;
		rv = *p ;
		printf( "%s:0x%08lx\t=0x%08lx\n", reg->reg ? reg->reg->name : "", reg->address, rv );
	} else if( 1 == reg->width ) {
		unsigned char volatile *p = (unsigned char volatile *)regPtr ;
		rv = *p ;
		printf( "%s:0x%08lx\t=0x%02lx\n", reg->reg ? reg->reg->name : "", reg->address, rv );
	}
	else {
		fprintf(stderr, "Unsupported width in register %s\n", reg->reg->name);
		return ;
	}
	fflush(stdout);
	struct fieldDescription_t *f = reg->fields ;
	while(f){
		printf( "\t%-16s\t%2u-%2u\t=0x%x\n", f->name, f->startbit, f->startbit+f->bitcount-1, fieldVal(f,rv) );
		fflush(stdout);
		f=f->next ;
	}
}

static void putReg(struct reglist_t const *reg,unsigned long value){
	unsigned address = 0 ;
	unsigned shift = 0 ;
	unsigned long mask = 0xffffffff ;
	if (reg->fields) {
		// Only single field allowed
		if (0 == reg->fields->next) {
			shift = reg->fields->startbit ;
			mask = ((1<<reg->fields->bitcount)-1)<<shift ;
		} else {
			fprintf(stderr, "More than one field matched %s\n", reg->reg->name);
			return ;
		}
	}
	unsigned long maxValue = mask >> shift ;
	if (value > maxValue) {
		fprintf(stderr, "Value 0x%lx exceeds max 0x%lx for register %s\n", value, maxValue, reg->reg->name);
		return ;
	}
	if( 1 == reg->width ){
		unsigned char volatile * const rv = (unsigned char volatile *)getReg(reg->address);
		value = (*rv&~mask) | ((value<<shift)&mask);
		printf( "%s:0x%02lx == 0x%02x...", reg->reg ? reg->reg->name : "", reg->address, *rv );
		*rv = value ;
	} else if( 2 == reg->width ){
		unsigned short volatile * const rv = (unsigned short volatile *)getReg(reg->address);
		value = (*rv&~mask) | ((value<<shift)&mask);
		printf( "%s:0x%04lx == 0x%04x...", reg->reg ? reg->reg->name : "", reg->address, *rv );
		*rv = value ;
	} else {
		unsigned long volatile * const rv = getReg(reg->address);
		value = (*rv&~mask) | ((value<<shift)&mask);
		printf( "%s:0x%08lx == 0x%08lx...", reg->reg ? reg->reg->name : "", reg->address, *rv );
		*rv = value ;
	}
	printf( "0x%08lx\n", value );
}

static void parseArgs( int &argc, char const **argv )
{
	for( int arg = 1 ; arg < argc ; arg++ ){
		if( '-' == *argv[arg] ){
			char const *param = argv[arg]+1 ;
			if( 'w' == tolower(*param) ){
            			word_access = true ;
				printf("using word access\n" );
			}
			else
				printf( "unknown option %s\n", param );

			// pull from argument list
			for( int j = arg+1 ; j < argc ; j++ ){
				argv[j-1] = argv[j];
			}
			--arg ;
			--argc ;
		}
	}
}

static int getcpu(unsigned &cpu) {
	cpu = 0 ;
	FILE *fIn = fopen("/proc/cpuinfo", "r");
	if (fIn) {
		char inBuf[512];
		while (fgets(inBuf,sizeof(inBuf),fIn)) {
			char *rev = strstr(inBuf,"Revision");
			if (rev && (0 != (rev=strchr(rev+7,':')))) {
				char *next = rev+2;
				char *end = inBuf+strlen(inBuf);
				while (isxdigit(*next)) {
					cpu <<= 4 ;
					unsigned char c = toupper(*next++);
					if (('0' <= c)&&('9' >= c)) {
						cpu |= (c-'0');
					} else {
						cpu |= (10+(c-'A'));
					}
				}
			}
		}
		fclose(fIn);
	}
	return (0 != cpu);
}

int main(int argc, char const **argv)
{
	unsigned cpu ;

	parseArgs(argc,argv);

	if (!getcpu(cpu)) {
		fprintf(stderr, "Error reading CPU type\n");
		return -1 ;
	}
//	printf( "CPU type is 0x%x\n", cpu);
        registerDefs(cpu);
	if( 1 == argc ){
                struct reglist_t const *defs = registerDefs();
		while(defs){
                        showReg(defs);
			defs = defs->next ;
		}
	} else {
                struct reglist_t const *regs = parseRegisterSpec(argv[1]);
		if( regs ){
			if( 2 == argc ){
				while( regs ){
					showReg(regs);
					regs = regs->next ;
				}
			} else {
				char *end ;
				unsigned long value = strtoul(argv[2],&end,16);
				if( '\0' == *end ){
					while( regs ){
						showReg(regs);
						putReg(regs,value);
						regs = regs->next ;
					}
				} else 
					fprintf( stderr, "Invalid value '%s', use hex\n", argv[2] );
			}
		} else
			fprintf (stderr, "Nothing matched %s\n", argv[1]);
	}
	return 1;
}
