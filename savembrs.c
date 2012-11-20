#define _LARGEFILE64_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>

#define BLOCKSIZE 512
#define BLOCKSPERFLASH 300000

static unsigned char msdos_mbr[2] = {
        0x55, 0xaa
};

static const char *const i386_sys_types[] = {
	"\x00" "Empty",
	"\x01" "FAT12",
	"\x04" "FAT16 <32M",
	"\x05" "Extended",         /* DOS 3.3+ extended partition */
#define EXTENDED_PARTITION 5
	"\x06" "FAT16",            /* DOS 16-bit >=32M */
	"\x07" "HPFS/NTFS",        /* OS/2 IFS, eg, HPFS or NTFS or QNX */
	"\x0a" "OS/2 Boot Manager",/* OS/2 Boot Manager */
	"\x0b" "Win95 FAT32",
	"\x0c" "Win95 FAT32 (LBA)",/* LBA really is 'Extended Int 13h' */
	"\x0e" "Win95 FAT16 (LBA)",
	"\x0f" "Win95 Ext'd (LBA)",
	"\x11" "Hidden FAT12",
	"\x12" "Compaq diagnostics",
	"\x14" "Hidden FAT16 <32M",
	"\x16" "Hidden FAT16",
	"\x17" "Hidden HPFS/NTFS",
	"\x1b" "Hidden Win95 FAT32",
	"\x1c" "Hidden W95 FAT32 (LBA)",
	"\x1e" "Hidden W95 FAT16 (LBA)",
	"\x3c" "Part.Magic recovery",
	"\x41" "PPC PReP Boot",
	"\x42" "SFS",
	"\x63" "GNU HURD or SysV", /* GNU HURD or Mach or Sys V/386 (such as ISC UNIX) */
	"\x80" "Old Minix",        /* Minix 1.4a and earlier */
	"\x81" "Minix / old Linux",/* Minix 1.4b and later */
	"\x82" "Linux swap",       /* also Solaris */
	"\x83" "Linux",
	"\x84" "OS/2 hidden C: drive",
	"\x85" "Linux extended",
	"\x86" "NTFS volume set",
	"\x87" "NTFS volume set",
	"\x8e" "Linux LVM",
	"\x9f" "BSD/OS",           /* BSDI */
	"\xa0" "Thinkpad hibernation",
	"\xa5" "FreeBSD",          /* various BSD flavours */
	"\xa6" "OpenBSD",
	"\xa8" "Darwin UFS",
	"\xa9" "NetBSD",
	"\xab" "Darwin boot",
	"\xb7" "BSDI fs",
	"\xb8" "BSDI swap",
	"\xbe" "Solaris boot",
	"\xeb" "BeOS fs",
	"\xee" "EFI GPT",                    /* Intel EFI GUID Partition Table */
	"\xef" "EFI (FAT-12/16/32)",         /* Intel EFI System Partition */
	"\xf0" "Linux/PA-RISC boot",         /* Linux/PA-RISC boot loader */
	"\xf2" "DOS secondary",              /* DOS 3.3+ secondary */
	"\xfd" "Linux raid autodetect",      /* New (2.2.x) raid partition with
						autodetect using persistent
						superblock */
#if 0 /* ENABLE_WEIRD_PARTITION_TYPES */
	"\x02" "XENIX root",
	"\x03" "XENIX usr",
	"\x08" "AIX",              /* AIX boot (AIX -- PS/2 port) or SplitDrive */
	"\x09" "AIX bootable",     /* AIX data or Coherent */
	"\x10" "OPUS",
	"\x18" "AST SmartSleep",
	"\x24" "NEC DOS",
	"\x39" "Plan 9",
	"\x40" "Venix 80286",
	"\x4d" "QNX4.x",
	"\x4e" "QNX4.x 2nd part",
	"\x4f" "QNX4.x 3rd part",
	"\x50" "OnTrack DM",
	"\x51" "OnTrack DM6 Aux1", /* (or Novell) */
	"\x52" "CP/M",             /* CP/M or Microport SysV/AT */
	"\x53" "OnTrack DM6 Aux3",
	"\x54" "OnTrackDM6",
	"\x55" "EZ-Drive",
	"\x56" "Golden Bow",
	"\x5c" "Priam Edisk",
	"\x61" "SpeedStor",
	"\x64" "Novell Netware 286",
	"\x65" "Novell Netware 386",
	"\x70" "DiskSecure Multi-Boot",
	"\x75" "PC/IX",
	"\x93" "Amoeba",
	"\x94" "Amoeba BBT",       /* (bad block table) */
	"\xa7" "NeXTSTEP",
	"\xbb" "Boot Wizard hidden",
	"\xc1" "DRDOS/sec (FAT-12)",
	"\xc4" "DRDOS/sec (FAT-16 < 32M)",
	"\xc6" "DRDOS/sec (FAT-16)",
	"\xc7" "Syrinx",
	"\xda" "Non-FS data",
	"\xdb" "CP/M / CTOS / ...",/* CP/M or Concurrent CP/M or
	                              Concurrent DOS or CTOS */
	"\xde" "Dell Utility",     /* Dell PowerEdge Server utilities */
	"\xdf" "BootIt",           /* BootIt EMBRM */
	"\xe1" "DOS access",       /* DOS access or SpeedStor 12-bit FAT
	                              extended partition */
	"\xe3" "DOS R/O",          /* DOS R/O or SpeedStor */
	"\xe4" "SpeedStor",        /* SpeedStor 16-bit FAT extended
	                              partition < 1024 cyl. */
	"\xf1" "SpeedStor",
	"\xf4" "SpeedStor",        /* SpeedStor large partition */
	"\xfe" "LANstep",          /* SpeedStor >1024 cyl. or LANstep */
	"\xff" "BBT",              /* Xenix Bad Block Table */
#endif
	NULL
};

static char const *find_part_type(unsigned char t)
{
        char const *const*type = i386_sys_types ;
	while (*type){
		if (t == (unsigned char)(*type)[0]) {
			return (*type)+1 ;
		}
		type++;
	}
	return "unknown" ;
}

/* based on linux/include/genhd.h */
struct partition {
	unsigned char boot_ind;		/* 0x80 - active */
	unsigned char head;		/* starting head */
	unsigned char sector;		/* starting sector */
	unsigned char cyl;		/* starting cylinder */
	unsigned char sys_ind;		/* What partition type */
	unsigned char end_head;		/* end head */
	unsigned char end_sector;	/* end sector */
	unsigned char end_cyl;		/* end cylinder */
	unsigned char start_sect[4];	/* starting sector counting from 0 */
	unsigned char nr_sects[4];	/* nr of sectors in partition */
} __attribute__ ((packed));

struct mbr {
	unsigned char boot_code[440];
	unsigned char disk_signature[4];
	unsigned char padding[2];
	struct partition partition_record[4];
	unsigned char mbr_signature[2];
} __attribute__ ((packed));

#define ARRAY_SIZE(__arr) ((sizeof(__arr))/sizeof((__arr)[0]))

static unsigned mbrs[128] = {
	0
};
static unsigned num_mbrs = 1;
static unsigned firstext = 0 ;

struct part_lba {
	unsigned partnum ;
	unsigned startsect ;
	unsigned count ;
};

static struct part_lba save_parts[128] = {
	0
};
static unsigned numsaveparts = 0 ;

static void saveMBR(unsigned offs, char *mbrbuf)
{
	int fdout ;
	char fname[512];
	snprintf(fname, sizeof(fname),"mbr%u",offs);
	fdout = open(fname,O_WRONLY|O_CREAT|O_EXCL,0666);
	if (0 > fdout) {
		perror(fname);
		exit(-1);
	}
	write(fdout,mbrbuf,BLOCKSIZE);
	close(fdout);
	printf("fastboot flash mmc0:%u %s\n", offs, fname);
}

static void savePart(int fddisk, unsigned pnum, unsigned start, unsigned count)
{
	unsigned offs = 0 ;
	unsigned const startstart = start ;
	while (0 < count) {
		int fdout ;
		char fname[512];
		int blocks = 0 ;
		snprintf(fname, sizeof(fname),"part%u.%u",pnum,offs);
		fdout = open(fname,O_WRONLY|O_CREAT|O_EXCL,0666);
		if (0 > fdout) {
			perror(fname);
			exit(-1);
		}
		printf("fastboot flash mmc0:%u %s\n", start, fname);
		while ((0 <count)&&(BLOCKSPERFLASH>blocks)) {
			unsigned char blockbuf[BLOCKSIZE];
			off64_t pos = (off64_t)start*BLOCKSIZE;
			if (pos != lseek64(fddisk,pos,SEEK_SET)) {
				perror("seek");
				exit(-1);
			}
			if (BLOCKSIZE != read(fddisk,blockbuf,sizeof(blockbuf))) {
				perror("read");
				exit(-1);
			}
			if (BLOCKSIZE != write(fdout,blockbuf,sizeof(blockbuf))) {
				perror("write");
				exit(-1);
			}
			start++;
			blocks++;
			count--;
		}
		offs++ ;
		close(fdout);
	}
}

int main (int argc, char **argv)
{
	int do_save ;
	unsigned pcount = 0 ;
	int fd;
	if (1 >= argc) {
		fprintf(stderr, "Usage: %s /path/to/disk [-d]\n", argv[0]);
		return -1 ;
	}

	fd = open(argv[1],O_RDONLY);
	if (0 > fd) {
		perror(argv[1]);
		return -1 ;
	}
	do_save = (2 < argc) && (0 == strcasecmp("-d",argv[2]));
	while (0 < num_mbrs) {
		int i;
		int numread;
		char mbrbuf[BLOCKSIZE];
		unsigned start = mbrs[0];
		off64_t pos = (off64_t)start*sizeof(mbrbuf);
		--num_mbrs ;
		if (0 < num_mbrs) {
			memmove(mbrs,mbrs+1,num_mbrs*sizeof(mbrs[0]));
		}
		if (pos != lseek64(fd,pos,SEEK_SET)) {
			perror("seek");
			return -1 ;
		}
		numread = read(fd,mbrbuf,sizeof(mbrbuf));
		if (sizeof(mbrbuf) != numread) {
			perror ("read");
			return -1 ;
		}
		struct mbr *pmbr = (struct mbr *)mbrbuf;
		if (0 != memcmp(msdos_mbr,pmbr->mbr_signature,sizeof(msdos_mbr))) {
			fprintf(stderr, "Invalid signature %02x %02x in MBR %x\n",
				pmbr->mbr_signature[0],pmbr->mbr_signature[1],start);
			return -1 ;
		}
		saveMBR(start,mbrbuf);
		for (i = 0 ; i < ARRAY_SIZE(pmbr->partition_record); i++) {
			struct partition *part = pmbr->partition_record+i;
			unsigned pstart, pend ;
			memcpy(&pstart, part->start_sect, sizeof(pstart));
			memcpy(&pend, part->nr_sects, sizeof(pend));
			pend += pstart-1;

			if ((0 != part->sys_ind)
			    &&
			    (EXTENDED_PARTITION != part->sys_ind)) {
				unsigned numsect = pend-pstart+1;
				unsigned numbytes = numsect*512;
				unsigned mb = numbytes>>20;
				printf("# %cp%u\t%12u\t%12u\t%12u\t%12u bytes\t%5u MB\t%s\n",
				       (part->boot_ind&0x80)?'*':' ',
					pcount+1,
					start+pstart, start+pend, 
					numsect, numbytes, mb,
					find_part_type(part->sys_ind));
				if (do_save) {
					save_parts[numsaveparts].partnum = pcount+1 ;
					save_parts[numsaveparts].startsect = start+pstart;
                                        save_parts[numsaveparts].count = pend-pstart+1 ;
					++numsaveparts ;
				}
			}
			if(EXTENDED_PARTITION == part->sys_ind) {
				mbrs[num_mbrs++] = pstart+firstext;
				if (0 == firstext) {
					firstext = pstart ;
				}
			}
			if ((0 == start)
			    ||
			    ((EXTENDED_PARTITION != part->sys_ind)
			     &&
			     (0 != part->sys_ind))) {
				pcount++ ;
			}
		}
	}
	if (0 < numsaveparts) {
		int i ;
		printf ("# %s: restore %u parts here\n", argv[0],numsaveparts);
		for (i = 0 ; i < numsaveparts ; i++) {
			printf("# p%d: %u.%u\n"
			       , save_parts[i].partnum
			       , save_parts[i].startsect
			       , save_parts[i].count);
			savePart(fd
				 , save_parts[i].partnum
                                 , save_parts[i].startsect
                                 , save_parts[i].count);
		}
	}
	close(fd);
	return 0 ;
}
