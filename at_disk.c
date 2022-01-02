#include <mint/osbind.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "byte_swap.h"
#include "disk_struct.h"

#define VERSION "0.1"

#define GIGABYTE (1024*1024*1024UL)

static PHYSSECT physsect, physsect2;

static int32_t read_sector(uint8_t* buffer, uint16_t dev, uint32_t sector)
{
    return Rwabs2((0<<RW_WRITE) | (1<<RW_NOMEDIACH) | (0<<RW_NORETRIES) | (1<<RW_NOTRANSLATE), buffer, 1, sector, dev);
}

static void analyze_mbr(MBR* mbr, uint16_t dev)
{
    for (int i = 0; i < 4; ++i) {
        PARTENTRY* pe = &mbr->entry[i];
        swpl(pe->start);
        swpl(pe->size);
        // TODO: extended partition types (0x05 = CHS, 0x0f = LBA)
        if (pe->type != 0x00 && (pe->type == 0x04 || pe->type == 0x06) && pe->start * MAXPHYSSECTSIZE < GIGABYTE) {
            printf("Partition[%02x] %d (%d.%d MiB):\r\n", pe->type, i, MAXPHYSSECTSIZE * pe->size / (1024 * 1024), MAXPHYSSECTSIZE * pe->size % (1024 * 1024) / 10000);
            printf("  Start: %08x, end: %08x\r\n", pe->start * MAXPHYSSECTSIZE, (pe->start + pe->size) * MAXPHYSSECTSIZE - 1);
        }
    }
}

static void analyze_ahdi(struct rootsector* rs, uint16_t dev)
{
    for (int i = 0; i < 4; ++i) {
        struct partition_info* pi = &physsect.rs.part[i];
        char str[4] = {};
        memcpy(str, pi->id, 3);
        // TODO: XGM
        if ((pi->flg & 0x01) && (strcmp(str, "GEM") == 0 || strcmp(str, "BGM") == 0) && pi->st * MAXPHYSSECTSIZE < GIGABYTE) {
            printf("Partition[%s] %d (%d.%d MiB):\r\n", str, i, MAXPHYSSECTSIZE * pi->siz / (1024 * 1024), MAXPHYSSECTSIZE * pi->siz % (1024 * 1024) / 10000);
            printf("  Start: %08x, end: %08x\r\n", pi->st * MAXPHYSSECTSIZE, (pi->st + pi->siz) * MAXPHYSSECTSIZE - 1);

            if (read_sector(physsect2.sect, dev, pi->st + 1) == 0 && physsect2.mbr.bootsig == 0x55aa) {
                printf("  Partition contains a MS-DOS image!\r\n");
            }
        }
    }
}

static void scan_disk(int bus_offset)
{
    for (int i = 0; i < 8; ++i) {
        uint16_t dev = i + 2 + bus_offset;

        int32_t ret = read_sector(physsect.sect, dev, 0);
        if (ret == 0) {
            printf("Root sector of disk %d read successfully.\r\n", i);
            printf("Checking partitions within first GiB.\r\n");
            printf("\r\n");

            if (physsect.mbr.bootsig == 0x55aa) {
                // DOS MBR
                analyze_mbr(&physsect.mbr, dev);
            } else {
                // AHDI root sector
                analyze_ahdi(&physsect.rs, dev);
            }
        }
    }
    printf("\r\n");
}

int main(int argc, const char* argv[])
{
    printf("ATonce MS-DOS partition fixer v%s\r\n", VERSION);
    printf("\r\n");

    printf(
"Purpose of this program is to correct\r\n"
"invalid disk image parameters created\r\n"
"by MS-DOS in ATonce PC emulator. If you\r\n"
"don't have this hardware installed or\r\n"
"or you don't have any dedicated\r\n"
"partitions for MS-DOS, you don't need\r\n"
"it.\r\n");
    printf("\r\n");

    printf("Press ENTER to continue/CTRL+C to exit.\r\n");
    printf("\r\n");
    getchar();

    printf("Scanning ACSI disks...\r\n");
    printf("\r\n");
    scan_disk(0);

    printf("Scanning SCSI disks...\r\n");
    printf("\r\n");
    scan_disk(8);

    printf("Scanning IDE disks...\r\n");
    printf("\r\n");
    scan_disk(16);

    getchar();

    return EXIT_SUCCESS;
}
