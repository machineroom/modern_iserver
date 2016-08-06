#include <tsplib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define BLKSIZE (4096)

//simple loopback performance test

int main (int argc, char **argv) {
    int fd;
    int rc;
    fd = tsp_open(argv[1]);
    printf ("tsp_open (%s) = %d\n", argv[1], fd);
    rc = tsp_reset(fd);
    printf ("tsp_reset = %d\n", rc);
    rc = tsp_protocol(fd, TSP_RAW_PROTOCOL, 4096);
    printf ("tsp_protocol = %d\n", rc);
    int i=0;
    long total=0;
    while (1) {
        uint8_t buf[BLKSIZE];
        uint8_t buf2[BLKSIZE];
        memset (buf, i, sizeof(buf));
        rc = tsp_write(fd, buf, sizeof(buf), 2);
        rc = tsp_read (fd, buf2, sizeof(buf2), 2);
        if (memcmp (buf, buf2, sizeof(buf)) != 0) {
            printf ("buffers differ!\n");
            break;
        }
        i++;
        total += BLKSIZE;
        if (i%100==0) {
            printf ("total = %ld KB\n", total/1024);
        }
    }
    tsp_close(fd);
}


