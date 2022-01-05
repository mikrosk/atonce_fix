#include <ctype.h>
#include <mint/osbind.h>
#include <mint/sysvars.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "byte_swap.h"
#include "disk_struct.h"

#define VERSION  "0.1"

#define DRIVES_MAX 16
#define BUS_ACSI   0
#define BUS_SCSI   8
#define BUS_IDE    16
#define GIGABYTE   (1024*1024*1024UL)

static struct {
    int skipped;
    char drive;
    int bus;
    char bus_str[4+1];
    int pun;
    char type[3+1];
    size_t size;
    uint32_t sector_start;
    uint32_t sector_end;
} drives[DRIVES_MAX] = {};

static PHYSSECT physsect, physsect2;


static int32_t read_sector(uint8_t* buffer, uint16_t dev, uint32_t sector)
{
    return Rwabs2((0<<RW_WRITE) | (1<<RW_NOMEDIACH) | (0<<RW_NORETRIES) | (1<<RW_NOTRANSLATE), buffer, 1, sector, dev);
}


static void print_help(int exit_code)
{
    fprintf(stderr, "Usage: %s [-h] <drv letter>...\r\n\r\n", APP_NAME);
    fprintf(stderr, "<drv letter> is one of X / X: / X:\\\r\n\r\n");
    fprintf(stderr,
    "Purpose of this program is to correct\r\n"
    "invalid disk image parameters created\r\n"
    "by MS-DOS in ATonce PC emulator. If you\r\n"
    "don't have this hardware installed or\r\n"
    "or you don't have any dedicated\r\n"
    "partitions for MS-DOS, you don't need\r\n"
    "it.\r\n");
    fprintf(stderr, "\r\n");
    fprintf(stderr, "Press Return to exit.\r\n");
    getchar();
    exit(exit_code);
}

static void parse_args(int argc, const char* argv[])
{
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-h") == 0) {
            print_help(EXIT_SUCCESS);
        } else {
            switch (strlen(argv[i])) {
                case 3:
                    if (argv[i][2] != '\\')
                        print_help(EXIT_FAILURE);
                case 2:
                    if (argv[i][1] != ':')
                        print_help(EXIT_FAILURE);
                case 1:
                    if (isalpha(argv[i][0])) {
                        char c = toupper(argv[i][0]);
                        if (c < 'C' || c > 'P')
                            fprintf(stderr, "Skipping'%c:' drive (C: - P:)\r\n", c);
                        else if (drives[c - 'A'].drive == c) {
                            fprintf(stderr, "Ignoring multiple '%c:' drives\r\n", c);
                        } else {
                            drives[c - 'A'].drive = c;
                        }
                        break;
                    }
                default:
                    print_help(EXIT_FAILURE);
            }
        }
    }
}


static HDINFO* get_pun_ptr(void)
{
  int32_t oldstack = Super(0L);
  HDINFO* p = *pun_ptr;
  Super ((void *)oldstack);

  return p;
}

static void read_pun_info(void)
{
    HDINFO* p = get_pun_ptr();
    for (int i = 2; i < DRIVES_MAX; ++i) {
        if (isalpha(drives[i].drive)) {
            uint8_t flags = p->v_p_un[i];

            if (flags & (1<<7)) {
                fprintf(stderr, "Skipping'%c:' drive (not managed)\r\n", drives[i].drive);
                drives[i].drive = '\0';
                continue;
            }

            if (flags & (1<<4)) {
                strcpy(drives[i].bus_str, "IDE");
                drives[i].bus = BUS_IDE;
            } else if (flags & (1<<3)) {
                strcpy(drives[i].bus_str, "SCSI");
                drives[i].bus = BUS_SCSI;
            } else {
                strcpy(drives[i].bus_str, "ACSI");
                drives[i].bus = BUS_ACSI;
            }
            drives[i].pun = (flags & 0x07);
            drives[i].sector_start = p->pstart[i];
        }
    }
}


static void analyze_mbr(MBR* mbr, uint16_t dev, int drive)
{
    int found = 0;
    drives[drive].skipped = 1;  // skipped by default

    for (int i = 0; i < 4; ++i) {
        PARTENTRY* pe = &mbr->entry[i];
        swpl(pe->start);
        swpl(pe->size);
        if (drives[drive].sector_start == pe->start) {
            found = 1;

            drives[drive].type[0] = '\0';
            drives[drive].type[1] = 'D';
            drives[drive].type[2] = pe->type;
            drives[drive].sector_end = pe->start + pe->size - 1;
            drives[drive].size = pe->size;
            // TODO: extended partition types (0x05 = CHS, 0x0f = LBA)
            if (pe->type != 0x00
                && (pe->type == 0x04 || pe->type == 0x06)
                && pe->start * MAXPHYSSECTSIZE < GIGABYTE) {
                drives[drive].skipped = 0;
                break;
            }
        }
    }

    if (!found) {
        fprintf(stderr, "Skipping'%c:' drive (not in partition table)\r\n", drives[drive].drive);
        drives[drive].drive = '\0';
    }
}

static void analyze_ahdi(struct rootsector* rs, uint16_t dev, int drive)
{
    int found = 0;
    drives[drive].skipped = 1;  // skipped by default

    for (int i = 0; i < 4; ++i) {
        found = 1;

        struct partition_info* pi = &physsect.rs.part[i];
        if (drives[drive].sector_start == pi->st) {
            memcpy(drives[drive].type, pi->id, 3);
            drives[drive].sector_end = pi->st + pi->siz - 1;
            drives[drive].size = pi->siz;
            // TODO: XGM
            if ((pi->flg & 0x01)
                && (strcmp(drives[drive].type, "GEM") == 0 || strcmp(drives[drive].type, "BGM") == 0)
                && pi->st * MAXPHYSSECTSIZE < GIGABYTE) {
                drives[drive].skipped = 0;
                break;
            }
        }
    }

    if (!found) {
        fprintf(stderr, "Skipping'%c:' drive (not in partition table)\r\n", drives[drive].drive);
        drives[drive].drive = '\0';
    }
}

static void read_partition_table(void)
{
    // we can't assume any order (IDE->SCSI->ACSI or ACSI#0->ACSI#1...)
    int current_bus = -1;
    int current_pun = -1;
    for (int i = 2; i < DRIVES_MAX; ++i) {
        if (isalpha(drives[i].drive)) {
            int dev = 2 + drives[i].bus + drives[i].pun;

            if (drives[i].bus != current_bus || drives[i].pun != current_pun) {
                if (read_sector(physsect.sect, dev, 0) == 0) {
                    current_bus = drives[i].bus;
                    current_pun = drives[i].pun;
                } else {
                    fprintf(stderr, "Skipping'%c:' drive (root sector failure)\r\n", drives[i].drive);
                    drives[i].drive = '\0';
                    continue;
                }
            }

            if (physsect.mbr.bootsig == 0x55aa)
                analyze_mbr(&physsect.mbr, dev, i);
            else
                analyze_ahdi(&physsect.rs, dev, i);
        }
    }
}


static void print_summary(void)
{
#if 0
    printf("Drive Bus  Unit Type Size Sector\r\n");
    printf("--------------------------------\r\n");

    beyond_gib = p->pstart[i] * MAXPHYSSECTSIZE >= GIGABYTE;
            printf("%c:    %s", drives[i].drive, drives[i].bus_str);
            if (strlen(drives[i].bus_str) == 3)
                printf(" ");
            printf(" %d    %ld%s\r\n", flags & 0x07, p->pstart[i], beyond_gib ? "*" : "");

if (beyond_gib) {
        printf("\r\n");
        printf("* beyond first GiB, skipped\r\n");
    }
    printf("\r\n");

    if ((pi->flg & 0x01) && (strcmp(str, "GEM") == 0 || strcmp(str, "BGM") == 0) && pi->st * MAXPHYSSECTSIZE < GIGABYTE) {
            printf("Partition[%s] %d (%d.%d MiB):\r\n", str, i, MAXPHYSSECTSIZE * pi->siz / (1024 * 1024), MAXPHYSSECTSIZE * pi->siz % (1024 * 1024) / 10000);
            printf("  Start: %08x, end: %08x\r\n", pi->st * MAXPHYSSECTSIZE, (pi->st + pi->siz) * MAXPHYSSECTSIZE - 1);

            if (read_sector(physsect2.sect, dev, pi->st + 1) == 0 && physsect2.mbr.bootsig == 0x55aa) {
                printf("  Partition contains a MS-DOS image!\r\n");
            }


            printf("Partition[%02x] %d (%d.%d MiB):\r\n", pe->type, i, MAXPHYSSECTSIZE * pe->size / (1024 * 1024), MAXPHYSSECTSIZE * pe->size % (1024 * 1024) / 10000);
            printf("  Start: %08x, end: %08x\r\n", pe->start * MAXPHYSSECTSIZE, (pe->start + pe->size) * MAXPHYSSECTSIZE - 1);
#endif
}


int main(int argc, const char* argv[])
{
    printf("ATonce MS-DOS partition fixer v%s\r\n", VERSION);
    printf("\r\n");

    parse_args(argc, argv);

    read_pun_info();

    read_partition_table();

    print_summary();

    getchar();

    return EXIT_SUCCESS;
}
