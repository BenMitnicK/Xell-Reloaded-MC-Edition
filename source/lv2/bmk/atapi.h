#ifndef H_ATAPI
#define H_ATAPI

#include <xetypes.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define XENON_ATA_REG_DATA      0
#define XENON_ATA_REG_ERROR     1
#define XENON_ATA_REG_FEATURES  1
#define XENON_ATA_REG_SECTORS   2
#define XENON_ATA_REG_SECTNUM   3
#define XENON_ATA_REG_CYLLSB    4
#define XENON_ATA_REG_CYLMSB    5
#define XENON_ATA_REG_LBALOW    3
#define XENON_ATA_REG_LBAMID    4
#define XENON_ATA_REG_LBAHIGH   5
#define XENON_ATA_REG_DISK      6
#define XENON_ATA_REG_CMD       7
#define XENON_ATA_REG_STATUS    7

#define XENON_ATA_REG2_CONTROL   0

#include <diskio/disc_io.h>

	enum xenon_ata_commands2 {
		XENON_ATA_CMD_READ_SECTORS2 = 0x20,
		XENON_ATA_CMD_READ_SECTORS_EXT2 = 0x24,
		XENON_ATA_CMD_READ_DMA_EXT2 = 0x25,
		XENON_ATA_CMD_WRITE_SECTORS2 = 0x30,
		XENON_ATA_CMD_WRITE_SECTORS_EXT2 = 0x34,
		XENON_ATA_CMD_IDENTIFY_DEVICE2 = 0xEC,
		XENON_ATA_CMD_IDENTIFY_PACKET_DEVICE2 = 0xA1,
		XENON_ATA_CMD_PACKET2 = 0xA0,
		XENON_ATA_CMD_SET_FEATURES2 = 0xEF,
	};

	enum {
		XENON_ATA_CHS2 = 0,
		XENON_ATA_LBA2,
		XENON_ATA_LBA48_2
	};

#define XENON_DISK_SECTOR_SIZE 0x200
#define XENON_CDROM_SECTOR_SIZE 2048

	enum {
		XENON_ATA_DMA_TABLE_OFS2 = 4,
		XENON_ATA_DMA_STATUS2 = 2,
		XENON_ATA_DMA_CMD2 = 0,
		XENON_ATA_DMA_WR2 = (1 << 3),
		XENON_ATA_DMA_START2 = (1 << 0),
		XENON_ATA_DMA_INTR2 = (1 << 2),
		XENON_ATA_DMA_ERR2 = (1 << 1),
		XENON_ATA_DMA_ACTIVE2 = (1 << 0),
	};

#define MAX_PRDS 16

	struct xenon_ata_dma_prd2 {
		uint32_t address;
		uint32_t size_flags;
	} __attribute__((packed));

	struct xenon_ata_device2 {
		uint32_t ioaddress;
		uint32_t ioaddress2;

		int atapi;

		int addressing_mode;

		uint16_t cylinders;
		uint16_t heads;
		uint16_t sectors_per_track;

		uint32_t size;
		struct bdev *bdev;

		struct xenon_ata_dma_prd2 * prds;
	};

	struct xenon_atapi_read2 {
		uint8_t code;
		uint8_t reserved1;
		uint32_t lba;
		uint8_t reserved2;
		uint16_t length;
		uint8_t reserved3[3];
	} __attribute__((packed));

	int xenon_ata_init2();
	int xenon_atapi_init2();
	void xenon_atapi_set_modeb2(void);
    //int xenon_atapi_get_dvd_key_tsh943a(unsigned char *dvdkey);
	int xenon_atapi_set_dvd_key2(unsigned char *dvdkey);
	
	extern struct xenon_ata_device2 ata2;
	extern struct xenon_ata_device2 atapi2;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
