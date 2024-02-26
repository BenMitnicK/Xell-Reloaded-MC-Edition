#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <debug.h>
#include <xenos/xenos.h>
#include <console/console.h>
#include <time/time.h>
#include <ppc/timebase.h>
#include <usb/usbmain.h>
#include <sys/iosupport.h>
#include <ppc/register.h>
#include <xenon_nand/xenon_sfcx.h>
#include <xenon_nand/xenon_config.h>
#include <xenon_soc/xenon_secotp.h>
#include <xenon_soc/xenon_power.h>
#include <xenon_soc/xenon_io.h>
#include <xenon_sound/sound.h>
#include <xenon_smc/xenon_smc.h>
#include <xenon_smc/xenon_gpio.h>
#include <xb360/xb360.h>
#include <network/network.h>
#include <httpd/httpd.h>
#include <diskio/ata.h>
#include <elf/elf.h>
#include <version.h>
#include <byteswap.h>

#include "bmk/atapi.h"
//#include "bmk/xbox.h"
#include "asciiart.h"
#include "config.h"
#include "file.h"
#include "tftp/tftp.h"

#include "log.h"

char temp2txt[2048];
char *tmp2txt=temp2txt;

char buftxt[2048];
char *bufftxt = buftxt;

extern char *usbdrive;

void do_asciiart()
{
	char *p = asciiart;
	while (*p)
		console_putch(*p++);
	printf(asciitail);
}

void dumpana() {
	int i;
	for (i = 0; i < 0x100; ++i)
	{
		uint32_t v;
		xenon_smc_ana_read(i, &v);
		printf("0x%08x, ", (unsigned int)v);
		if ((i&0x7)==0x7)
			printf(" // %02x\n", (unsigned int)(i &~0x7));
	}
}

char FUSES[350]; /* this string stores the ascii dump of the fuses */

unsigned char stacks[6][0x10000];

void reset_timebase_task()
{
	mtspr(284,0); // TBLW
	mtspr(285,0); // TBUW
	mtspr(284,0);
}
void synchronize_timebases()
{
	xenon_thread_startup();
	
	std((void*)0x200611a0,0); // stop timebase
	
	int i;
	for(i=1;i<6;++i){
		xenon_run_thread_task(i,&stacks[i][0xff00],(void *)reset_timebase_task);
		while(xenon_is_thread_task_running(i));
	}
	
	reset_timebase_task(); // don't forget thread 0
			
	std((void*)0x200611a0,0x1ff); // restart timebase
}
void print_key_plain(char *name, unsigned char *data,int *size)
{
    int i=0;
    printf(name);
    for(i=0; i<*size; i++)
    {
        tmp2txt += sprintf(tmp2txt,"%c",data[i]);
    }
    printf(temp2txt);
    printf("\n");
}
int main(){
	LogInit();
	int i;

	printf("ANA Dump before Init:\n");
	dumpana();

	// linux needs this
	synchronize_timebases();
	
	// irqs preinit (SMC related)
	*(volatile uint32_t*)0xea00106c = 0x1000000;
	*(volatile uint32_t*)0xea001064 = 0x10;
	*(volatile uint32_t*)0xea00105c = 0xc000000;

	xenon_smc_start_bootanim();

	// flush console after each outputted char
	setbuf(stdout,NULL);

	xenos_init(VIDEO_MODE_AUTO);

	printf("ANA Dump after Init:\n");
	dumpana();

#ifdef SWIZZY_THEME
	console_set_colors(CONSOLE_COLOR_BLACK,CONSOLE_COLOR_ORANGE); // Orange text on black bg
#elif defined XTUDO_THEME
	console_set_colors(CONSOLE_COLOR_BLACK,CONSOLE_COLOR_PINK); // Pink text on black bg
#elif defined DEFAULT_THEME
	console_set_colors(CONSOLE_COLOR_BLUE,CONSOLE_COLOR_WHITE); // White text on blue bg
#else
	console_set_colors(CONSOLE_COLOR_BLACK,CONSOLE_COLOR_GREEN); // Green text on black bg
#endif
	console_init();

	printf("\nXeLL - Xenon linux loader second stage v0.994-git-5b9788e " LONGVERSION "\n");

	do_asciiart();

	//delay(3); //give the user a chance to see our splash screen <- network init should last long enough...
	
	xenon_sound_init();

	if (xenon_get_console_type() != REV_CORONA_PHISON) //Not needed for MMC type of consoles! ;)
	{
		printf(" * nand init\n");
		sfcx_init();
		if (sfc.initialized != SFCX_INITIALIZED)
		{
			printf(" ! sfcx initialization failure\n");
			printf(" ! nand related features will not be available\n");
			delay(5);
		}
	}

	xenon_config_init();

#ifndef NO_NETWORKING

	printf(" * network init\n");
	network_init();

	printf(" * starting httpd server...");
	httpd_start();
	printf("success\n");

#endif

	printf(" * usb init\n");
	usb_init();
	usb_do_poll();

	printf(" * sata hdd init\n");
	xenon_ata_init();

	//printf(" * sata dvd init\n");
	//xenon_atapi_init();

	mount_all_devices();
	/*int device_list_size = */ findDevices();
	/* display some cpu info */
	printf(" * CPU PVR: %08x\n", mfspr(287));

#ifndef NO_PRINT_CONFIG
	/*printf(" * FUSES - write them down and keep them safe:\n");
	char *fusestr = FUSES;
	for (i=0; i<12; ++i){
		u64 line;
		unsigned int hi,lo;

		line=xenon_secotp_read_line(i);
		hi=line>>32;
		lo=line&0xffffffff;

		fusestr += sprintf(fusestr, "fuseset %02d: %08x%08x\n", i, hi, lo);
	}
	printf(FUSES);

    print_cpu_dvd_keys();
	network_print_config();*/
#endif
	/* Stop logging and save it to first USB Device found that is writeable */
	LogDeInit();
	//extern char device_list[STD_MAX][10];

	//for (i = 0; i < device_list_size; i++)
	//{
	//	if (strncmp(device_list[i], "ud", 2) == 0)
	//	{
	//		char tmp[STD_MAX + 8];
	//		sprintf(tmp, "%sxell.log", device_list[i]);
	//		if (LogWriteFile(tmp) == 0)
	//			i = device_list_size;
	//	}
	//}
	
	mount_all_devices();
    console_clrscr();
    logusb();
    
    unsigned int ldv=0;
    for (i=0; i<12; ++i)//Show Fuses
    {
        u64 line;
        unsigned int hi,lo;
        unsigned int tmp;

        line=xenon_secotp_read_line(i);
        hi=line>>32;
        lo=line&0xffffffff;
        if ((i>6)&&(i<9))
        {
            tmp=hi;
            while(tmp!=0)
            {
                if (tmp!=0)ldv++;
                tmp=tmp<<4;
            }
            tmp=lo;
            while(tmp!=0)
            {
                if (tmp!=0)ldv++;
                tmp=tmp<<4;
            }
        }
        tmp2txt += sprintf(tmp2txt,"fuseset %02d: %08X%08X\n", i, hi, lo);
        //printf("fuseset %02d: %08X%08X\n", i, hi, lo);
    }
    savefile(temp2txt,"fuses.txt",0x20,1);
    //*tmp2txt = 0;
    //memset (temp2txt, '\0', sizeof (temp2txt));
    
    printf("\n\n");
    printf("******************************************************************************\n"
           "***********************    ModConsoles Edition V1.0    ***********************\n"
           "***********************         By BenMitnicK          ***********************\n"
           "******************************************************************************\n\n");
    /*bufftxt += sprintf(bufftxt, "******************************************************************************\n");
    bufftxt += sprintf(bufftxt, "***********************    ModConsoles Edition V1.0    ***********************\n");
    bufftxt += sprintf(bufftxt, "***********************         By BenMitnicK          ***********************\n");
    bufftxt += sprintf(bufftxt, "******************************************************************************\n\n");*/
    switch(xenon_get_console_type()){
        case REV_XENON:bufftxt += sprintf(bufftxt, " * Console Model: Xenon (FAT)\n"); break;
        case REV_ZEPHYR:bufftxt += sprintf(bufftxt, " * Console Model: Zephyr (FAT)\n"); break;
        case REV_FALCON:bufftxt += sprintf(bufftxt, " * Console Model: Falcon (FAT)\n"); break;
        case REV_JASPER:bufftxt += sprintf(bufftxt, " * Console Model: Jasper (FAT)\n"); break;
        case REV_TRINITY:bufftxt += sprintf(bufftxt, " * Console Model: Trinity (Slim)\n"); break;
        case REV_CORONA:bufftxt += sprintf(bufftxt, " * Console Model: Corona  (Slim)\n"); break;
        case REV_CORONA_PHISON:bufftxt += sprintf(bufftxt, " * Console Model: Corona Phison  (Slim)\n"); break;
        case REV_WINCHESTER:bufftxt += sprintf(bufftxt, " * Console Model: Winchester  (Slim)\n"); break;
        case REV_UNKNOWN:bufftxt += sprintf(bufftxt, " * Console Model: Unknown\n"); break;
        default : bufftxt += sprintf(bufftxt, " * Console Model: Unknown\n");
    }
    bufftxt += sprintf(bufftxt, " * LDV: %d\n",ldv);
    bufftxt += sprintf(bufftxt, " * Nand Size: %dMB\n", sfc.size_mb);
    xenon_atapi_init2();
    
    dumpbl();
    
    //char buff[2048];
    unsigned char key[0x10 + 1];
    memset(key,'\0',sizeof(key));    
    if (cpu_get_key(key)==0)
	{
        bufftxt += sprintf(bufftxt, "\n * CPU KEY: %016llX%016llX\n", ld(&key[0x0]),ld(&key[0x8]));
        //str += sprintf(str, "%016llX%016llX", ld(&key[0x0]),ld(&key[0x8]));
        //savefile(buf,"cpukey.txt",0x20,1);
        //*str++ = 0;
        //memset (temp2txt, 0, sizeof (temp2txt));
    }
        
	memset(key, '\0', sizeof(key));
	if (kv_get_dvd_key(key)==0)
	{
		bufftxt += sprintf(bufftxt, " * DVD KEY: %016llX%016llX\n\n", ld(&key[0x0]),ld(&key[0x8]));
        //sprintf(buff, "%016llX%016llX", ld(&key[0x0]),ld(&key[0x8]));
        //savefile(buff,"dvdkey.txt",0x20,1);
        //memset (temp2txt, 0, sizeof (temp2txt));
	}
    
    memset(key, '\0', sizeof(key));
	if (get_virtual_cpukey(key)==0)
	{
		bufftxt += sprintf(bufftxt, " * Virtual DVD KEY: %016llX%016llX\n\n", ld(&key[0x0]),ld(&key[0x8]));
	}
    
    /*memset(key, '\0', sizeof(key));
	kv_get_dvd_key2(key);*/
    //savefile(bufftxt,"LOG.txt",0x20,1);
    printf(buftxt);
    savefile(buftxt,"LOG.txt",0x20,1);
    network_print_config();
    
    //printf("Waiting for USB Devices/Files:");
    //logusb();
    
        printf("\n * Looking for files on local media and TFTP...\n\n");
        for(;;){
            fileloop();
            tftp_loop(); //less likely to find something...
            console_clrline();		
        }

        return 0;
    }

