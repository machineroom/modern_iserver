#include <tsplib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

int main (int argc, char **argv) {
    int fd;
    int rc;
    fd = tsp_open(argv[1]);
    printf ("tsp_open (%s) = %d\n", argv[1], fd);
    rc = tsp_reset(fd);
    printf ("tsp_reset = %d\n", rc);
    uint8_t bit=1;
    while (1) {
        tsp_write_flags (fd, bit);
        bit <<= 1;
        if (bit == 0) bit=1;
    }
    tsp_close(fd);
}


