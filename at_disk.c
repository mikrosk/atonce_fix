#include <mint/osbind.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define VERSION "0.1"

static uint8_t sect[512];

static void scan_disk(int bus_offset)
{
    for (int i = 0; i < 8; ++i) {
        printf("Checking disk #%d...", i);

        int32_t ret = Rwabs((0<<RW_WRITE) | (1<<RW_NOMEDIACH) | (0<<RW_NORETRIES) | (1<<RW_NOTRANSLATE), sect, 1, 0, i + 2 + bus_offset);
        if (ret == 0) {
            printf("success.\r\n");
        } else {
            printf("failed.\r\n");
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
    scan_disk(0);

    //printf("Scanning SCSI disks...\r\n");
    //scan_disk(8);

    //printf("Scanning IDE disks...\r\n");
    //scan_disk(16);

    getchar();

    return EXIT_SUCCESS;
}
