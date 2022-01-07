#include <ctype.h>
#include <mint/osbind.h>
#include <mint/sysvars.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "byte_swap.h"
#include "disk_struct.h"

#define VERSION  "1.0 beta"

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

static int num_warnings;

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
    if (argc < 2)
        print_help(EXIT_FAILURE);

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
                        if (c < 'C' || c > 'P') {
                            fprintf(stderr, "Skipping '%c:' drive (C: - P:)\r\n", c);
                            num_warnings++;
                        }
                        else if (drives[c - 'A'].drive == c) {
                            fprintf(stderr, "Ignoring multiple '%c:' drives\r\n", c);
                            num_warnings++;
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
                fprintf(stderr, "Skipping '%c:' drive (not managed)\r\n", drives[i].drive);
                num_warnings++;
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

static int analyze_dos_partition(const PARTENTRY* pe, uint32_t pe_start, uint32_t pe_size, uint16_t dev, int drive)
{
    if (pe->type == 0x05 ||  pe->type == 0x0f || drives[drive].sector_start != pe_start)
        return 0;

    drives[drive].type[0] = '\0';
    drives[drive].type[1] = 'D';
    drives[drive].type[2] = pe->type;
    drives[drive].sector_end = pe_start + pe_size - 1;
    drives[drive].size = pe_size;

    if (pe->type != 0x00
        // 0x01: FAT12 as primary partition in first physical 32 MB of disk
        //       or as logical drive anywhere on disk (else use 06h instead).
        // 0x04: FAT16 with less than 65536 sectors (32 MB). As primary partition it must reside in first physical 32 MB of disk
        //       or as logical drive anywhere on disk (else use 06h instead).
        // 0x06: FAT16B with 65536 or more sectors. It must reside within the first 8 GB of disk
        //       unless used for logical drives in an 0Fh extended partition (else use 0Eh instead).
        //       Also used for FAT12 and FAT16 volumes in primary partitions if they are not residing in first physical 32 MB of disk.
        // 0x0e: FAT16B with LBA.
        && (pe->type == 0x01 || pe->type == 0x04 || pe->type == 0x06 || pe->type == 0x0e)
        && pe_size != 0 && pe_start * MAXPHYSSECTSIZE < GIGABYTE) {
        drives[drive].skipped = 0;
    }

    return 1;
}

static int analyze_mbr(const MBR* mbr, uint32_t prim_start, uint32_t ext_start, uint16_t dev, int drive)
{
    int found = 0;
    drives[drive].skipped = 1;  // skipped by default

    // sanity check
    if (mbr->bootsig != 0x55aa) {
        fprintf(stderr, "Skipping '%c:' drive (not a valid MBR)\r\n", drives[drive].drive);
        num_warnings++;
        drives[drive].drive = '\0';
        return -1;
    }

    for (int i = 0; i < 4 && !found; ++i) {
        const PARTENTRY* pe = &mbr->entry[i];
        uint32_t pe_start = pe->start;
        swpl(pe_start);
        uint32_t pe_size = pe->size;
        swpl(pe_size);

        if (analyze_dos_partition(pe, prim_start + pe_start, pe_size, dev, drive)) {
            found = 1;
            break;
        }

        // 0x05: Extended partition with CHS addressing. It must reside within the first physical 8 GB of disk,
        //       else use 0Fh instead
        // 0x0f: Extended partition with LBA.
        if (pe->type == 0x05 ||  pe->type == 0x0f) {
            if (ext_start == 0)
                ext_start = pe_start;
            else
                pe_start += ext_start;

            if (read_sector(physsect2.sect, dev, pe_start) != 0)
                break;

            found = analyze_mbr(&physsect2.mbr, pe_start, ext_start, dev, drive);
            if (found == -1)
                return found;
            else if (found)
                break;
        }
    }

    if (!found && ext_start == 0) {
        fprintf(stderr, "Skipping '%c:' drive (not in partition table)\r\n", drives[drive].drive);
        num_warnings++;
        drives[drive].drive = '\0';
    }

    return found;
}

static int analyze_ahdi_partition(const struct partition_info* pi, uint16_t dev, int drive)
{
    if (memcmp(pi->id, "XGM", 3) == 0 || drives[drive].sector_start != pi->st)
        return 0;

    memcpy(drives[drive].type, pi->id, 3);
    drives[drive].sector_end = pi->st + pi->siz - 1;
    drives[drive].size = pi->siz;

    if ((pi->flg & 0x01)
        && (memcmp(pi->id, "GEM", 3) == 0 || memcmp(pi->id, "BGM", 3) == 0)
        && pi->siz != 0 && pi->st * MAXPHYSSECTSIZE < GIGABYTE)
        drives[drive].skipped = 0;

    return 1;
}

static int analyze_ahdi(const struct rootsector* rs, uint16_t dev, int drive)
{
    int found = 0;
    drives[drive].skipped = 1;  // skipped by default

    for (int i = 0; i < 4 && !found; ++i) {
        const struct partition_info* pi = &rs->part[i];

        if (analyze_ahdi_partition(pi, dev, drive)) {
            found = 1;
            break;
        }

        uint32_t pi_st;
        uint32_t ext_st;
        pi_st = ext_st = pi->st;

        while ((pi->flg & 0x01) && memcmp(pi->id, "XGM", 3) == 0) {
            if (read_sector(physsect2.sect, dev, pi_st) != 0)
                break;

            struct partition_info* ext_pi = &physsect2.rs.part[0];
            ext_pi->st += pi_st;

            if (analyze_ahdi_partition(ext_pi, dev, drive)) {
                found = 1;
                break;
            }

            pi = &physsect2.rs.part[1];
            pi_st = ext_st + pi->st;
        }
    }

    if (!found) {
        fprintf(stderr, "Skipping '%c:' drive (not in part. table)\r\n", drives[drive].drive);
        num_warnings++;
        drives[drive].drive = '\0';
    }

    return found;
}

static int read_partition_table(void)
{
    int found = 0;

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
                    fprintf(stderr, "Skipping '%c:' drive (root sector failure)\r\n", drives[i].drive);
                    num_warnings++;
                    drives[i].drive = '\0';
                    continue;
                }
            }

            if (physsect.mbr.bootsig == 0x55aa)
                found |= (analyze_mbr(&physsect.mbr, 0, 0, dev, i) == 1);
            else
                found |= (analyze_ahdi(&physsect.rs, dev, i) == 1);
        }
    }

    return found;
}


static void print_summary(void)
{
    printf("Drv Bus  # Type Size   Sectors\r\n");
    printf("--------------------------------------\r\n");

    int skipped = 0;

    for (int i = 2; i < DRIVES_MAX; ++i) {
        if (isalpha(drives[i].drive)) {
            printf("%c:  %s", drives[i].drive, drives[i].bus_str);
            if (strlen(drives[i].bus_str) == 3)
                printf(" ");

            printf(" %d", drives[i].pun);

            if (drives[i].size > 0) {
                if (drives[i].type[0] == '\0' && drives[i].type[1] == 'D')
                    printf(" %02x ", drives[i].type[2]);
                else
                    printf(" %s", drives[i].type);

                printf("  %03lu.%02lu", MAXPHYSSECTSIZE * drives[i].size / (1024 * 1024), MAXPHYSSECTSIZE * drives[i].size % (1024 * 1024) / 10000);
                printf(" %07u-%07u", drives[i].sector_start, drives[i].sector_end);
            } else {
                // workaround for unsupported partition types
                printf("             %u", drives[i].sector_start);
            }
            printf("%s\r\n", drives[i].skipped ? "*" : "");
            skipped |= drives[i].skipped;
        }
    }

    if (skipped) {
        printf("\r\n");
        printf("* skipped due to 1 GiB limit / unused\r\n");
        printf("  partition / unsupported type\r\n");
    }
}


static void fix_image_mbr(const MBR* mbr, uint32_t offset, uint32_t prim_start, uint32_t ext_start, uint16_t dev, int drive)
{
    // sanity check
    if (mbr->bootsig != 0x55aa) {
        fprintf(stderr, "Skipping drive (not a valid MBR)\r\n");
        return;
    }

    for (int i = 0; i < 4; ++i) {
        const PARTENTRY* pe = &mbr->entry[i];
        uint32_t pe_start = pe->start;
        swpl(pe_start);
        uint32_t pe_size = pe->size;
        swpl(pe_size);

        if (pe_size > 0) {
            if (read_sector(physsect2.sect, dev, pe_start + offset) != 0) {
                continue;
            }

            struct fat16_bs* fat16 = (struct fat16_bs*)physsect2.sect;

            uint16_t bps = *(uint16_t*)fat16->bps;
            swpw(bps);  /* bytes per sector */
            uint32_t sec = *(uint16_t*)fat16->sec ? *(uint16_t*)fat16->sec : *(uint32_t*)fat16->sec2;
            swpl(sec);  /* total number of sectors */
            char str[8+1] = {};
            memcpy(str, fat16->fstype, sizeof(fat16->fstype));

            printf("[%s] %02x   %03u.%02u %07u-%07u\r\n", str, pe->type,
                   MAXPHYSSECTSIZE * pe_size / (1024 * 1024), MAXPHYSSECTSIZE * pe_size % (1024 * 1024) / 10000,
                   pe_start + offset, pe_start + pe_size + offset - 1);

            if (MAXPHYSSECTSIZE * pe_size < bps * sec) {
                memcpy(str, physsect2.sect+3, 8);
                str[8] = '\0';
                fprintf(stderr, "->Skipping \"%s\" (FAT16>MBR's PTE)\r\n", str);
                continue;
            }

            int32_t additional_phys_sectors = (pe_start + pe_size + offset) - (drives[drive].sector_start + drives[drive].size);
            if (additional_phys_sectors <= 0) {
                fprintf(stderr, "->Skipping (volume within limits)\r\n");
                continue;
            }

            uint32_t additional_bytes = additional_phys_sectors * MAXPHYSSECTSIZE;
            printf("->%u sectors (%u bytes) more!\r\n", additional_phys_sectors, additional_bytes);

            uint16_t res = *(uint16_t*)fat16->res;
            swpw(res);  /* number of reserved sectors */
            uint8_t fat = fat16->fat;   /* number of FATs */
            uint16_t dir = *(uint16_t*)fat16->dir;
            swpw(dir);  /* number of DIR root entries */
            uint16_t spf = *(uint16_t*)fat16->spf;
            swpw(spf);  /* sectors per FAT */

            uint32_t additional_log_sectors = additional_bytes / bps;
            if (additional_bytes % bps != 0)
                additional_log_sectors++;

            if (sec - additional_log_sectors < res + fat*spf + (dir*32/bps)) {
                fprintf(stderr, "->Skipping (can't shrink system sectors)\r\n");
                continue;
            }

            // TODO: check for used sectors (from FAT), too
            uint32_t new_pe_size = pe_size - additional_phys_sectors;
            uint32_t     new_sec = sec     - additional_log_sectors;
            printf("\r\n");

            printf("Shrink PTE[%d] by %d phys. sectors? ", i, pe_size - new_pe_size);
            char shrink_pe_size = 'n';
            scanf("%c", &shrink_pe_size);
            printf("\r\n");
            shrink_pe_size = toupper(shrink_pe_size);

            printf("Shrink volume by %d log. sectors? ", sec - new_sec);
            char shrink_sec = 'n';
            scanf("%c", &shrink_sec);
            printf("\r\n");
            shrink_sec = toupper(shrink_sec);
        }

        if (pe->type == 0x05 ||  pe->type == 0x0f) {
            if (ext_start == 0)
                ext_start = pe_start;
            else
                pe_start += ext_start;

            if (read_sector(physsect2.sect, dev, pe_start + offset) != 0)
                break;

            fix_image_mbr(&physsect2.mbr, offset, pe_start, ext_start, dev, drive);
        }
    }
}


int main(int argc, const char* argv[])
{
    printf("ATonce MS-DOS partition fixer v%s\r\n", VERSION);
    printf("\r\n");

    parse_args(argc, argv);

    read_pun_info();

    if (!read_partition_table()) {
        printf("\r\n");
        fprintf(stderr, "Press Return to exit.\r\n");
        getchar();
        exit(EXIT_FAILURE);
    }

    if (num_warnings > 0)
        printf("\r\n");

    print_summary();

    printf("\r\n");

    for (int i = 2; i < DRIVES_MAX; ++i) {
        if (isalpha(drives[i].drive) && !drives[i].skipped) {
            int dev = 2 + drives[i].bus + drives[i].pun;

            if (read_sector(physsect.sect, dev, drives[i].sector_start + 1) != 0) {
                fprintf(stderr, "Skipping '%c:' drive (root sector failure)\r\n", drives[i].drive);
                drives[i].drive = '\0';
                continue;
            }

            if (physsect.mbr.bootsig == 0x55aa) {
                printf("Drive %c: contains a MS-DOS image!\r\n", drives[i].drive);
                printf("\r\n");
                fix_image_mbr(&physsect.mbr, drives[i].sector_start + 1, 0, 0, dev, i);
            }
        }
    }

    printf("\r\n");
    printf("Done.\r\n");
    printf("\r\n");
    printf("Press Return to exit.\r\n");
    getchar();

    return EXIT_SUCCESS;
}
