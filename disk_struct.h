#ifndef DISK_STRUCT_H_
#define DISK_STRUCT_H_

#include <stdint.h>

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

typedef struct {
    UBYTE fill0[4];
    UBYTE type;
    UBYTE fill5[3];
    ULONG start;        /* little-endian */
    ULONG size;         /* little-endian */
} __attribute__((packed)) PARTENTRY;

typedef struct {
    UBYTE filler[446];
    PARTENTRY entry[4];
    UWORD bootsig;
} __attribute__((packed)) MBR;

#define MAXPHYSSECTSIZE 512
typedef union
{
    UBYTE sect[MAXPHYSSECTSIZE];
    struct rootsector rs;
    MBR mbr;
} __attribute__((packed)) PHYSSECT;

struct fat16_bs {
  /*   0 */  UBYTE bra[2];
  /*   2 */  UBYTE loader[6];
  /*   8 */  UBYTE serial[3];
  /*   b */  UBYTE bps[2];    /* bytes per sector */
  /*   d */  UBYTE spc;       /* sectors per cluster */
  /*   e */  UBYTE res[2];    /* number of reserved sectors */
  /*  10 */  UBYTE fat;       /* number of FATs */
  /*  11 */  UBYTE dir[2];    /* number of DIR root entries */
  /*  13 */  UBYTE sec[2];    /* total number of sectors */
  /*  15 */  UBYTE media;     /* media descriptor */
  /*  16 */  UBYTE spf[2];    /* sectors per FAT */
  /*  18 */  UBYTE spt[2];    /* sectors per track */
  /*  1a */  UBYTE sides[2];  /* number of sides */
  /*  1c */  UBYTE hid[4];    /* number of hidden sectors (earlier: 2 bytes) */
  /*  20 */  UBYTE sec2[4];   /* total number of sectors (if not in sec) */
  /*  24 */  UBYTE ldn;       /* logical drive number */
  /*  25 */  UBYTE dirty;     /* dirty filesystem flags */
  /*  26 */  UBYTE ext;       /* extended signature */
  /*  27 */  UBYTE serial2[4]; /* extended serial number */
  /*  2b */  UBYTE label[11]; /* volume label */
  /*  36 */  UBYTE fstype[8]; /* file system type */
  /*  3e */  UBYTE data[0x1c0];
  /* 1fe */  UBYTE cksum[2];
} __attribute__((packed));

#endif
