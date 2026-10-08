#include "ff.h"
#include "diskio.h"

#include <stdio.h>
#include <string.h>

#define SECTOR_SIZE 512u
#define SECTORS 2048u
#define CLUSTER_BYTES (4u * SECTOR_SIZE)
#define DATA_SECTORS (SECTORS - 7u)	/* After boot, FAT and root directory */
#define CAPACITY ((DATA_SECTORS / 4u) * CLUSTER_BYTES)
#define STALE 0xA5u	/* Left behind by deleted files */

static BYTE disk[SECTORS][SECTOR_SIZE];
static BYTE expected[CAPACITY];
static BYTE actual[CAPACITY];
static DSTATUS drive_status = STA_NOINIT;
static unsigned int failures;

static void put_word(BYTE *p, WORD value)
{
	p[0] = (BYTE)value;
	p[1] = (BYTE)(value >> 8);
}

/* FAT12: one reserved sector, one FAT of two sectors, 64 root entries and
   2 KiB clusters. The data area holds stale bytes. */
static void make_volume(void)
{
	BYTE *boot = disk[0];

	memset(disk, STALE, sizeof disk);
	memset(disk, 0, 7u * SECTOR_SIZE);
	boot[0] = 0xEB;
	boot[1] = 0x3C;
	boot[2] = 0x90;
	memcpy(boot + 3, "MSDOS5.0", 8);
	put_word(boot + 11, SECTOR_SIZE);
	boot[13] = 4;
	put_word(boot + 14, 1);
	boot[16] = 1;
	put_word(boot + 17, 64);
	put_word(boot + 19, SECTORS);
	boot[21] = 0xF8;
	put_word(boot + 22, 2);
	memcpy(boot + 54, "FAT12   ", 8);
	put_word(boot + 510, 0xAA55);
	disk[1][0] = 0xF8;
	disk[1][1] = 0xFF;
	disk[1][2] = 0xFF;
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
	if (drive != 0 || !buffer || count == 0 || sector + count > SECTORS) return RES_PARERR;
	memcpy(buffer, disk[sector], (size_t)count * SECTOR_SIZE);
	return RES_OK;
}

DRESULT disk_write(BYTE drive, const BYTE *buffer, LBA_t sector, UINT count)
{
	if (drive != 0 || !buffer || count == 0 || sector + count > SECTORS) return RES_PARERR;
	memcpy(disk[sector], buffer, (size_t)count * SECTOR_SIZE);
	return RES_OK;
}

DWORD get_fattime(void)
{
	return 0;
}

DRESULT disk_ioctl(BYTE drive, BYTE command, void *buffer)
{
	(void)buffer;
	return drive == 0 && command == CTRL_SYNC ? RES_OK : RES_PARERR;
}

static void check(int ok, const char *name, const char *what)
{
	if (!ok) {
		fprintf(stderr, "%s: %s\n", name, what);
		failures++;
	}
}

static FIL *open_file(const char *name, BYTE mode)
{
	static FIL file;

	check(f_open(&file, "DATA.BIN", mode) == FR_OK, name, "open failed");
	return &file;
}

static void put(FIL *file, BYTE value, UINT length, const char *name)
{
	static BYTE block[CLUSTER_BYTES];
	UINT written;

	memset(block, value, length);
	check(f_write(file, block, length, &written) == FR_OK && written == length,
		name, "write failed");
}

/* Compares the whole file with expected[0 .. size - 1]. */
static void verify(const char *name, FSIZE_t size)
{
	FIL *file = open_file(name, FA_READ);
	UINT got = 0;

	check(f_size(file) == size, name, "unexpected file size");
	check(f_read(file, actual, CAPACITY, &got) == FR_OK && got == size, name, "read failed");
	check(memcmp(actual, expected, (size_t)size) == 0, name, "stale or changed data");
	(void)f_close(file);
}

static void seek(FIL *file, FSIZE_t ofs, FSIZE_t position, const char *name)
{
	check(f_lseek(file, ofs) == FR_OK, name, "seek failed");
	check(f_tell(file) == position && f_size(file) >= position, name, "unexpected position");
}

static void start(FATFS *fs, const char *name)
{
	make_volume();
	memset(expected, 0, sizeof expected);
	check(f_mount(fs, "0:", 1) == FR_OK, name, "mount failed");
}

int main(void)
{
	static FATFS fs;
	const char *name;
	FIL *file;

	name = "extend after data across clusters";
	start(&fs, name);
	file = open_file(name, FA_CREATE_ALWAYS | FA_WRITE);
	put(file, 'a', 3, name);
	seek(file, 10000, 10000, name);
	put(file, 'z', 3, name);
	check(f_close(file) == FR_OK, name, "close failed");
	memset(expected, 'a', 3);
	memset(expected + 10000, 'z', 3);
	verify(name, 10003);

	name = "extend empty file";
	start(&fs, name);
	file = open_file(name, FA_CREATE_ALWAYS | FA_WRITE);
	seek(file, 3000, 3000, name);
	check(f_close(file) == FR_OK, name, "close failed");
	verify(name, 3000);

	name = "extend from a pointer before the end";
	start(&fs, name);
	file = open_file(name, FA_CREATE_ALWAYS | FA_WRITE);
	put(file, 0x5A, 1500, name);
	put(file, 0x5A, 1500, name);
	seek(file, 0, 0, name);
	seek(file, 7000, 7000, name);
	check(f_close(file) == FR_OK, name, "close failed");
	memset(expected, 0x5A, 3000);
	verify(name, 7000);

	name = "seek inside the file keeps data";
	start(&fs, name);
	file = open_file(name, FA_CREATE_ALWAYS | FA_WRITE);
	put(file, 0x5A, 2000, name);
	put(file, 0x5A, 2000, name);
	seek(file, 100, 100, name);
	put(file, 'Q', 1, name);
	check(f_close(file) == FR_OK, name, "close failed");
	memset(expected, 0x5A, 4000);
	expected[100] = 'Q';
	verify(name, 4000);

	name = "read-only seek is clipped at the end";
	start(&fs, name);
	file = open_file(name, FA_CREATE_ALWAYS | FA_WRITE);
	put(file, 0x5A, 100, name);
	check(f_close(file) == FR_OK, name, "close failed");
	file = open_file(name, FA_READ);
	seek(file, 5000, 100, name);
	check(f_size(file) == 100, name, "read-only seek changed the size");
	(void)f_close(file);
	memset(expected, 0x5A, 100);
	verify(name, 100);

	name = "disk full ends the file at the last cluster";
	start(&fs, name);
	file = open_file(name, FA_CREATE_ALWAYS | FA_WRITE);
	seek(file, 4000000, CAPACITY, name);
	check(f_size(file) == CAPACITY, name, "file does not end at the last cluster");
	check(f_close(file) == FR_OK, name, "close failed");
	verify(name, CAPACITY);

	(void)f_mount(NULL, "0:", 0);
	printf("6 seek cases, %u failures\n", failures);
	return failures ? 1 : 0;
}
