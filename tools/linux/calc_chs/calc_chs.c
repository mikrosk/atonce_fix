#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#define BYTES_PER_SECTOR 512

int main(int argc, const char* argv[])
{
    if (argc != 2)
        return EXIT_FAILURE;

    long size;
    size = strtol(argv[1], NULL, 0);

    if (size == 0) {
        fprintf(stderr, "%s can't be converted to a number\n", argv[1]);
        return EXIT_FAILURE;
    }

    if (size % BYTES_PER_SECTOR != 0) {
        long new_size = (size / 512) * 512;
        fprintf(stderr, "Rounding size from %ld to %ld\n", size, new_size);
        size = new_size;
    }

    int       heads = -1;
    int   cylinders = -1;
    int     sectors = -1;
    long final_size = -1;

    for (long head = 1; head <= 255; ++head) {    // avoid head 255 due to a bug in MS-DOS
        for (long cylinder = 1; cylinder <= 1024; ++cylinder) {
            for (long sector = 1; sector <= 63; ++sector) {   // sectors start at 1 => 63 sectors max
                long test_size = head * cylinder * sector * BYTES_PER_SECTOR;

                if (test_size > size)
                    continue;

                if (test_size > final_size) {
                    final_size = test_size;
                    heads      = head;
                    cylinders  = cylinder;
                    sectors    = sector;
                }
            }
        }
    }

    printf("C: %d, H: %d, S: %d, size: %ld (diff %ld)\n", cylinders, heads, sectors, final_size, size - final_size);

    return EXIT_SUCCESS;
}
