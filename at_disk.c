#include <mint/osbind.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, const char* argv[])
{
    printf(
"Purpose of this program is to correct invalid disk image parameters created\n"
" by MS-DOS in ATonce PC emulator. If you don't have this hardware installed\n"
" or you don't have any dedicated partitions for MS-DOS, you don't need it.\n");

    printf("Press ENTER to continue/CTRL+C to exit: \n");
    getchar();

    return EXIT_SUCCESS;
}
