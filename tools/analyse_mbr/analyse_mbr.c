#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef uint8_t UBYTE;
typedef uint16_t UWORD;
typedef uint32_t ULONG;

struct partition_info
{
  UBYTE flg;                    /* bit 0: active; bit 7: bootable */
  char id[3];                   /* "GEM", "BGM", "XGM", or other */
  ULONG st;                     /* start of partition */
  ULONG siz;                    /* length of partition */
} __attribute__((packed));

struct rootsector
{
  char unused[0x156];                   /* room for boot code */
  struct partition_info icdpart[8];     /* info for ICD-partitions 5..12 */
  char unused2[0xc];
  ULONG hd_siz;                         /* size of disk in blocks */
  struct partition_info part[4];
  ULONG bsl_st;                         /* start of bad sector list */
  ULONG bsl_cnt;                        /* length of bad sector list */
  UWORD checksum;                       /* checksum for bootable disks */
} __attribute__((packed));

static uint32_t be32(uint32_t num)
{
    return ((num>>24)&0xff) | // move byte 3 to byte 0
        ((num<<8)&0xff0000) | // move byte 1 to byte 2
        ((num>>8)&0xff00) | // move byte 2 to byte 1
        ((num<<24)&0xff000000);
}

int main(int argc, char* argv[])
{
    if (argc < 2 || argc > 3)
        return EXIT_FAILURE;
    
    FILE* f = fopen(argv[1], "rb");
    if (!f)
        return EXIT_FAILURE;

    unsigned long offset = 0;
    if (argc == 3) {
        offset = strtoul(argv[2], NULL, 0);
        printf("Offsetting by 0x%lx bytes\n", offset);
        fseek(f, offset, SEEK_SET);
    }
    
    uint8_t sect[1024 * 1024];
    fread(sect, sizeof(sect[0]), sizeof(sect)/sizeof(sect[0]), f);

#if 1
    // atari
    struct rootsector* rs = (struct rootsector*)sect;

    printf("Disk size: %d sectors\n\n", be32(rs->hd_siz));

    for (int i = 0; i < 4; ++i) {
        printf("Partition entry #%d:\n", i);

        printf("Status (bootable): %02x\n", rs->part[i].flg);

        char str[4] = {};
        memcpy(str, rs->part[i].id, 3);
        printf("ID: %s\n", str);

        printf("First sector %d (offset: %08x)\n", be32(rs->part[i].st), be32(rs->part[i].st) * 512);
        printf("Number of sectors: %d\n", be32(rs->part[i].siz));

        printf("\n");
    }
#else
    for (size_t i = 0x01BE; i < 0x01FE; i += 0x10) {
        printf("Partition entry #%d:\n", (i - 0x01BE) / 0x10);

        printf("Status (bootable): %02x\n", sect[i+0x00]);

        printf("First sector (head): %d\n", sect[i+0x01]);
        printf("First sector (sector): %d\n", sect[i+0x02] & 0x3f);
        printf("First sector (cylinder): %d\n", ((sect[i+0x02] & 0xC0000000) << 2) | sect[i+0x03]);

        printf("Partition type: %02x\n", sect[i+0x04]);

        printf("Last sector (head): %d\n", sect[i+0x05]);
        printf("Last sector (sector): %d\n", sect[i+0x06] & 0x3f);
        printf("Last sector (cylinder): %d\n", ((sect[i+0x06] & 0xC0000000) << 2) | sect[i+0x07]);

        printf("First sector (LBA): %d (offset: %08x; %08x)\n", *((uint32_t*)&sect[i+0x08]), *((uint32_t*)&sect[i+0x08]) * 512, (*((uint32_t*)&sect[i+0x08]) * 512) + offset);
        printf("Number of sectors (LBA): %d (offset: %08x; %08x)\n",
               *((uint32_t*)&sect[i+0x0C]),
               (*((uint32_t*)&sect[i+0x08]) + *((uint32_t*)&sect[i+0x0C])) * 512,
               (*((uint32_t*)&sect[i+0x08]) + *((uint32_t*)&sect[i+0x0C])) * 512 + offset
        );

        printf("\n");
    }

    printf("Signature: %04x\n", *((uint16_t*)&sect[0x1FE]));
#endif

    return EXIT_SUCCESS;
}
