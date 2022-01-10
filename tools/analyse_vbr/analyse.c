#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint16_t be16(const uint8_t* src)
{
    return (src[0] << 8) | src[1];
}

static uint16_t le16(const uint8_t* src)
{
    return *(uint16_t*)src;
}

static uint32_t le32(const uint8_t* src)
{
    return *(uint32_t*)src;
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
    
    uint8_t sect[128 * 1024];
    fread(sect, sizeof(sect[0]), sizeof(sect)/sizeof(sect[0]), f);
    
    printf("Jump instruction: %02x %02x %02x\n", sect[0], sect[1], sect[2]);

    printf("OEM Name: ");
    char str[1023+1];
    memcpy(str, sect+3, 8);
    str[8] = '\n';
    str[9] = '\0';
    printf("%s", str);

    uint16_t bytesPerSector = le16(&sect[0x00B]);
    printf("Bytes per sector: %d\n", bytesPerSector);

    printf("Logical sectors per cluster: %d\n", sect[0x00D]);

    uint16_t numberOfReservedSectors = le16(&sect[0x00E]);
    printf("Count of reserved logical sectors: %d\n", numberOfReservedSectors);

    uint8_t numberOfFats = sect[0x010];
    printf("Number of File Allocation Tables: %d\n", numberOfFats);

    printf("Maximum number of root directory entries: %d\n", le16(&sect[0x011]));

    printf("Total logical sectors: %d\n", le16(&sect[0x013]));

    printf("Media descriptor: %02x\n", sect[0x015]);

    uint16_t sectorsPerFat = le16(&sect[0x016]);
    printf("Logical sectors per File Allocation Table: %d\n", sectorsPerFat);

    printf("Physical sectors per track: %d\n", le16(&sect[0x018]));

    printf("Number of heads: %d\n", le16(&sect[0x01A]));

    //printf("Count of hidden sectors preceding the partition that contains this FAT volume: %d\n", le16(&sect[0x01C]));

    printf("Count of hidden sectors: %d\n", le32(&sect[0x01C]));

    //printf("Total logical sectors including hidden sectors: %d\n", le16(&sect[0x01E]));

    printf("Total logical sectors including hidden sectors: %d\n", le16(&sect[0x020]));

    printf("Physical drive number: %02x\n", sect[0x024]);

    printf("Reserved: %02x\n", sect[0x025]);

    printf("Extended boot signature: %02x\n", sect[0x026]);

    printf("Volume ID: %02x%02x%02x%02x\n", sect[0x027], sect[0x028], sect[0x029], sect[0x030]);

    printf("Partition Volume Label: ");
    memcpy(str, sect+0x02B, 11);
    str[11] = '\n';
    str[12] = '\0';
    printf("%s", str);

    printf("File system type: ");
    memcpy(str, sect+0x036, 8);
    str[8] = '\n';
    str[9] = '\0';
    printf("%s", str);

    printf("Boot sector signature (0x1FE): %04x\n", le16(&sect[0x1FE]));
    printf("Boot sector signature (end of sector): %04x\n", le16(&sect[bytesPerSector-2]));

    {
        uint16_t* sect16 = (uint16_t*)sect;
        uint16_t crc = 0;

        for (int i = 0; i < 256; ++i)
            crc += be16((uint8_t*)&sect16[i]);

        printf("Boot sector (512B) checksum: %04x\n", crc);
    }

    printf("First FAT starts at byte offset: 0x%08lx\n", offset + (numberOfReservedSectors + 0) * bytesPerSector);
    if (numberOfFats == 2) {
        printf("Second FAT starts at byte offset: 0x%08lx\n", offset + (numberOfReservedSectors + sectorsPerFat) * bytesPerSector);
    }

    printf("Data start at byte offset: 0x%08lx\n", offset + (numberOfReservedSectors + numberOfFats*sectorsPerFat) * bytesPerSector);
    
    return EXIT_SUCCESS;
}
