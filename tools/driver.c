#include <stdio.h>
#include <string.h>

int PackMsix(void);
int SignMsix(void);

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "pack") == 0) return PackMsix();
    if (argc == 2 && strcmp(argv[1], "sign") == 0) return SignMsix();
    fprintf(stderr, "usage: packsign pack|sign\n");
    return 2;
}
