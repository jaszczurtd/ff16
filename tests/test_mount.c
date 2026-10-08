#include "ff.h"
#include "diskio.h"

#include <stdio.h>
#include <string.h>

static BYTE boot_sector[512];
static DSTATUS drive_status = STA_NOINIT;

static void put_word(BYTE *p, WORD value)
{
	p[0] = (BYTE)value;
	p[1] = (BYTE)(value >> 8);
}

static void put_dword(BYTE *p, DWORD value)
{
	put_word(p, (WORD)value);
	put_word(p + 2, (WORD)(value >> 16));
}

static void make_volume(DWORD sectors, DWORD fat_sectors, BYTE fats,
		WORD reserved, WORD root_entries)
{
	memset(boot_sector, 0, sizeof boot_sector);
	boot_sector[0] = 0xEB;
	boot_sector[1] = 0x3C;
	boot_sector[2] = 0x90;
	memcpy(boot_sector + 3, "MSDOS5.0", 8);
	put_word(boot_sector + 11, sizeof boot_sector);
	boot_sector[13] = 1;
	put_word(boot_sector + 14, reserved);
	boot_sector[16] = fats;
	put_word(boot_sector + 17, root_entries);
	if (sectors <= 0xFFFFUL) {
		put_word(boot_sector + 19, (WORD)sectors);
	} else {
		put_dword(boot_sector + 32, sectors);
	}
	boot_sector[21] = 0xF8;
	if (root_entries) {
		put_word(boot_sector + 22, (WORD)fat_sectors);
		memcpy(boot_sector + 54, "FAT16   ", 8);
	} else {
		put_dword(boot_sector + 36, fat_sectors);
		put_dword(boot_sector + 44, 2);
		memcpy(boot_sector + 82, "FAT32   ", 8);
	}
	put_word(boot_sector + 510, 0xAA55);
	drive_status = STA_NOINIT;
}

DSTATUS disk_initialize(BYTE drive)
{
	if (drive != 0) return STA_NOINIT;
	drive_status = 0;
	return drive_status;
}

DSTATUS disk_status(BYTE drive)
{
	return drive == 0 ? drive_status : STA_NOINIT;
}

DRESULT disk_read(BYTE drive, BYTE *buffer, LBA_t sector, UINT count)
{
	if (drive != 0 || !buffer || count == 0) return RES_PARERR;
	memset(buffer, 0, (size_t)count * sizeof boot_sector);
	if (sector == 0) memcpy(buffer, boot_sector, sizeof boot_sector);
	return RES_OK;
}

#if !FF_FS_READONLY
DRESULT disk_write(BYTE drive, const BYTE *buffer, LBA_t sector, UINT count)
{
	(void)drive;
	(void)buffer;
	(void)sector;
	(void)count;
	return RES_WRPRT;
}

DWORD get_fattime(void)
{
	return 0;
}
#endif

DRESULT disk_ioctl(BYTE drive, BYTE command, void *buffer)
{
	(void)buffer;
	return drive == 0 && command == CTRL_SYNC ? RES_OK : RES_PARERR;
}

struct mount_case {
	const char *name;
	DWORD sectors;
	DWORD fat_sectors;
	BYTE fats;
	WORD reserved;
	WORD root_entries;
	FRESULT expected;
	BYTE type;
};

int main(void)
{
	static const struct mount_case cases[] = {
		{"valid FAT12", 2048, 8, 1, 1, 128, FR_OK, FS_FAT12},
		{"valid FAT16, one FAT", 8192, 32, 1, 1, 512, FR_OK, FS_FAT16},
		{"valid FAT16, two FATs", 8192, 32, 2, 1, 512, FR_OK, FS_FAT16},
		{"valid FAT32, one FAT", 70000, 600, 1, 32, 0, FR_OK, FS_FAT32},
		{"valid FAT32, two FATs", 70000, 600, 2, 32, 0, FR_OK, FS_FAT32},
		{"CVE-2026-6682 multiply overflow", 70000, 0x80000001UL, 2, 32, 0, FR_NO_FILESYSTEM, 0},
		{"system area addition overflow", 70000, 0xFFFFFFF0UL, 1, 32, 0, FR_NO_FILESYSTEM, 0},
		{"largest nonoverflowing multiply", 70000, 0x7FFFFFFFUL, 2, 32, 0, FR_NO_FILESYSTEM, 0},
		{"system area consumes volume", 70000, 34984, 2, 32, 0, FR_NO_FILESYSTEM, 0},
		{"system area exceeds volume", 70000, 34985, 2, 32, 0, FR_NO_FILESYSTEM, 0},
		{"no reserved sectors", 70000, 600, 2, 0, 0, FR_NO_FILESYSTEM, 0},
		{"unsupported FAT count", 70000, 600, 3, 32, 0, FR_NO_FILESYSTEM, 0},
		{NULL, 0, 0, 0, 0, 0, FR_OK, 0}
	};
	unsigned int i, failures = 0;
	FATFS fs;

	for (i = 0; cases[i].name; i++) {
		FRESULT result;
		memset(&fs, 0, sizeof fs);
		make_volume(cases[i].sectors, cases[i].fat_sectors, cases[i].fats,
			cases[i].reserved, cases[i].root_entries);
		result = f_mount(&fs, "0:", 1);
		if (result != cases[i].expected || fs.fs_type != cases[i].type) {
			fprintf(stderr, "%s: result %u/type %u, expected %u/type %u\n",
				cases[i].name, (unsigned int)result, (unsigned int)fs.fs_type,
				(unsigned int)cases[i].expected, (unsigned int)cases[i].type);
			failures++;
		}
		if (result == FR_OK && fs.database !=
			(LBA_t)cases[i].reserved + (LBA_t)cases[i].fat_sectors * cases[i].fats +
			cases[i].root_entries / (sizeof boot_sector / 32)) {
			fprintf(stderr, "%s: incorrect data area\n", cases[i].name);
			failures++;
		}
		(void)f_mount(NULL, "0:", 0);
	}
	printf("%u mount cases, %u failures\n", i, failures);
	return failures ? 1 : 0;
}
