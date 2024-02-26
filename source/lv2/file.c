/*
used for zlib support ...
*/

#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <string.h>
//#include <zlib.h>
#include <time/time.h>
#include <xetypes.h>
#include <elf/elf.h>
#include <network/network.h>
#include <console/console.h>
#include <sys/iosupport.h>
#include <usb/usbmain.h>
#include <ppc/timebase.h>
#include <xenon_nand/xenon_sfcx.h>
#include <xb360/xb360.h>

#include "config.h"
#include "file.h"
#include "kboot/kbootconf.h"
#include "tftp/tftp.h"
#include "../lv1/puff/puff.h"

#define CHUNK 16384

#define FIRST_BL_OFFSET                           0x20000000
#define FIRST_BL_SIZE                             0x8000
#define BL_KEY_PTR                                0xFE

//int i;

char *usbdrive;

extern char temp2txt[2048];
extern char *tmp2txt;

extern char buffkey[2048];
extern char buffkey2[2048];

char temptxt[2048];
char *tmptxt=temptxt;

extern char buftxt[2048];
extern char *bufftxt;

extern char dt_blob_start[];
extern char dt_blob_end[];

int boolusb = 0;

const unsigned char elfhdr[] = {0x7f, 'E', 'L', 'F'};
const unsigned char cpiohdr[] = {0x30, 0x37, 0x30, 0x37};
const unsigned char kboothdr[] = "#KBOOTCONFIG";

struct filenames filelist[] = {
    {"kboot.conf",TYPE_KBOOT},
    {"xenon.elf",TYPE_ELF},
    {"xenon.z",TYPE_ELF},
    {"vmlinux",TYPE_ELF},
	{"updxell.bin",TYPE_UPDXELL},
    {"updflash.bin",TYPE_NANDIMAGE},
	{"nandflash.bin",TYPE_NANDIMAGE},
	{" ", TYPE_ELF}
    //{NULL, NULL} //Dunno why this is here? :S
};
//Decompress a gzip file ...
//int inflate_read(char *source,int len,char **dest,int * destsize, int gzip) {
//	int ret;
//	unsigned have;
//	z_stream strm;
//	unsigned char out[CHUNK];
//	int totalsize = 0;
//
//	/* allocate inflate state */
//	strm.zalloc = Z_NULL;
//	strm.zfree = Z_NULL;
//	strm.opaque = Z_NULL;
//	strm.avail_in = 0;
//	strm.next_in = Z_NULL;
//	
//	if(gzip)
//		ret = inflateInit2(&strm, 16+MAX_WBITS);
//	else
//		ret = inflateInit(&strm);
//		
//	if (ret != Z_OK)
//		return ret;
//
//	strm.avail_in = len;
//	strm.next_in = (Bytef*)source;
//
//	/* run inflate() on input until output buffer not full */
//	do {
//		strm.avail_out = CHUNK;
//		strm.next_out = (Bytef*)out;
//		ret = inflate(&strm, Z_NO_FLUSH);
//		assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
//		switch (ret) {
//		case Z_NEED_DICT:
//			ret = Z_DATA_ERROR;     /* and fall through */
//		case Z_DATA_ERROR:
//		case Z_MEM_ERROR:
//			inflateEnd(&strm);
//			return ret;
//		}
//		have = CHUNK - strm.avail_out;
//		totalsize += have;
//                if (totalsize > ELF_MAXSIZE)
//                    return Z_BUF_ERROR;
//		//*dest = (char*)realloc(*dest,totalsize);
//		memcpy(*dest + totalsize - have,out,have);
//		*destsize = totalsize;
//	} while (strm.avail_out == 0);
//
//	/* clean up and return */
//	(void)inflateEnd(&strm);
//	return ret == Z_STREAM_END ? Z_OK : Z_DATA_ERROR;
//}

void wait_and_cleanup_line()
{
	uint64_t t=mftb();
	while(tb_diff_msec(mftb(),t)<200){ // yield to network
		network_poll();
	}
	console_clrline();
}

int savefile(char *datos,char *filename,int filelen,int modo)
{
    if (strcmp(usbdrive, "") == 0) return 1;
    int fd,rc;
    char file[20];
    sprintf(file,"%sSaves360Files/%s",usbdrive,filename);
#ifdef NO_SAVE_BIN
    if (modo==0) return 0;
#endif
    if (modo==0) modo=0X8000;
    else modo=0X4000;
    
    fd = open(file, O_CREAT |O_WRONLY|modo);
    if (fd < 0)
		return fd;
    
    rc = write(fd, datos, strlen(datos));
    
    close(fd);
    return rc;
}

void savesallfiles()
{
    savefile(buffkey,"cpukey.txt",0x20,1);
    savefile(buffkey2,"dvdkey.txt",0x20,1);
    savefile(temp2txt,"fuses.txt",0x20,1);
    savefile(buftxt,"LOG.txt",0x20,1);
    printf("|| CPUKEY.txt || DVDKEY.txt || FUSES.txt || LOG.txt || have been saved to USB :)");
    boolusb = 1;
    delay(5);
    //xenon_smc_power_shutdown();
}
int logusb()//detect if usb device
{
    static char device_list[STD_MAX][10];
    static char nand_files[20];
    int i;
    usb_do_poll();
    for (i = 3; i < STD_MAX; i++)
    {
        if (devoptab_list[i]->structSize && devoptab_list[i]->name[0]=='u')
        {
            sprintf(device_list[0], "%s:/", devoptab_list[i]->name);
            usbdrive = device_list[0];
            sprintf(nand_files, "%sSaves360Files", usbdrive);
            mkdir(nand_files, 0777);
            savesallfiles();
            return 0;
        }
    }
    //usbdrive="";
    return 1;
}

void print_key2(char *name, unsigned char *data)
{
    int i=0;
    bufftxt += sprintf(bufftxt, "\n * %s : ",name);
    //printf(temptxt);
    for(i=0; i<16; i++)
    {
        bufftxt += sprintf(bufftxt, "%02X",data[i]);
    }
}
int dumpbl()
{
    unsigned char * buf;
    
    buf = (unsigned char *) malloc(FIRST_BL_SIZE);
    int offset=0;

    memcpy(buf, FIRST_BL_OFFSET, FIRST_BL_SIZE);
    //savefile(buf,"1BL.bin",FIRST_BL_SIZE,0);

    offset =  buf[BL_KEY_PTR];
    offset <<= 8;
    offset |= buf[BL_KEY_PTR+1];
    offset+=0x148;

    print_key2("1BLKey",buf+offset);

    free(buf);
    return 0;
}
int launch_file(void * addr, unsigned len, int filetype){
	int ret = 0;
	unsigned char * gzip_file;
    switch (filetype){
            
		case TYPE_ELF:
			// check if addr point to a gzip file
            gzip_file = (unsigned char *)addr;
            if((gzip_file[0]==0x1F)&&(gzip_file[1]==0x8B)){
				//found a gzip file
                printf(" * Found a gzip file...\n");
                char * dest = malloc(ELF_MAXSIZE);
                long unsigned int destsize = 0;
                //if(inflate_read((char*)addr, len, &dest, &destsize, 1) == 0){
				if (puff((unsigned char*)dest, &destsize, addr, (long unsigned int*)&len)==0){
					//relocate elf ...
                    memcpy(addr,dest,destsize);
                    printf(" * Successfully unpacked...\n");
                    free(dest);
                    len=destsize;
                }
                else {
					printf(" * Unpacking failed...\n");
                    free(dest);
                    return -1;
                }
			}
            if (memcmp(addr, elfhdr, 4))
				return -1;
            printf(" * Launching ELF...\n");
            ret = elf_runWithDeviceTree(addr,len,dt_blob_start,dt_blob_end-dt_blob_start);
            break;
		case TYPE_INITRD:
			printf(" * Loading initrd into memory ...\n");
            ret = kernel_prepare_initrd(addr,len);
            break;
        case TYPE_KBOOT:
			printf(" * Loading kboot.conf ...\n");
            ret = try_kbootconf(addr,len);
            break;
   // This shit is broken!
   //     case TYPE_UPDXELL:
			//if (memcmp(addr + XELL_FOOTER_OFFSET, XELL_FOOTER, XELL_FOOTER_LENGTH) || len != XELL_SIZE)
			//	return -1;
   //         printf(" * Loading UpdXeLL binary...\n");
   //         ret = updateXeLL(addr,len);
   //         break;
		default:
			printf("! Unsupported filetype supplied!\n");
	}
	return ret;
}

int try_load_file(char *filename, int filetype)
{
	int ret;
	if(filetype == TYPE_NANDIMAGE){
		try_rawflash(filename);
		return -1;
	}

	if (filetype == TYPE_UPDXELL)
	{
		updateXeLL(filename);
		return -1;
	}
	
	wait_and_cleanup_line();
	printf("Trying %s...\r",filename);
	
	struct stat s;
	stat(filename, &s);

	long size = s.st_size;

	if (size <= 0) 
		return -1; //Size is invalid

	int f = open(filename, O_RDONLY);

	if (f < 0)
		return f; //File wasn't opened...

	void * buf=malloc(size);

	printf("\n * '%s' found, loading %ld...\n",filename,size);
	int r = read(f, buf, size);
	if (r < 0)
	{
		close(f);
		free(buf);
		return r;
	}
	
	if (filetype == TYPE_ELF) {
		char * argv[] = {
			filename,
		};
		int argc = sizeof (argv) / sizeof (char *);
		
		elf_setArgcArgv(argc, argv);
	}
	
	ret = launch_file(buf,r,filetype);

	free(buf);
	return ret;
}

void fileloop() {
        char filepath[255];

        int i,j=0;
        for (i = 3; i < 16; i++) {
                if (devoptab_list[i]->structSize) {
                        do{
							usb_do_poll();
							if (!devoptab_list[i]->structSize)
								break;
							sprintf(filepath, "%s:/%s", devoptab_list[i]->name,filelist[j].filename);
							if ((filelist[j].filetype == TYPE_UPDXELL || filelist[j].filetype == TYPE_NANDIMAGE) && (xenon_get_console_type() == REV_CORONA_PHISON))
							{
								wait_and_cleanup_line();
								printf("MMC Console Detected! Skipping %s...\r", filepath);
								j++;
							}
							else
							{								
								try_load_file(filepath,filelist[j].filetype);
								j++;
							}
						} while(strcmp(filelist[j].filename, " "));
						j = 0;
				}
		}
}

void tftp_loop() {
    int i=0;
    do{
		if ((filelist[i].filetype == TYPE_UPDXELL || filelist[i].filetype == TYPE_NANDIMAGE) && (xenon_get_console_type() == REV_CORONA_PHISON))
		{
			wait_and_cleanup_line();
			printf("Skipping TFTP %s:%s... MMC Detected!\r", boot_server_name(),filelist[i].filename);
			i++;
		}
		else
		{
			wait_and_cleanup_line();
			printf("Trying TFTP %s:%s... \r",boot_server_name(),filelist[i].filename);
			boot_tftp(boot_server_name(), filelist[i].filename, filelist[i].filetype);
			i++;
		}
		network_poll();
	} while(strcmp(filelist[i].filename, " "));
    wait_and_cleanup_line();
    printf("Trying TFTP %s:%s...\r",boot_server_name(),boot_file_name());
    /* Assume that bootfile delivered via DHCP is an ELF */
    boot_tftp(boot_server_name(),boot_file_name(),TYPE_ELF);
}