#include <tsplib.h>
#include <stdio.h>
#include <stdint.h>

int main (int argc, char **argv) {
    int fd;
    int rc;
    fd = tsp_open(argv[1]);
    printf ("tsp_open (%s) = %d\n", argv[1], fd);
    rc = tsp_reset(fd);
    printf ("tsp_reset = %d\n", rc);
    rc = tsp_analyse(fd);
    printf ("tsp_analyse = %d\n", rc);
    rc = tsp_error(fd);
    printf ("tsp_error = %d\n", rc);
    rc = tsp_protocol(fd, TSP_RAW_PROTOCOL, 4096);
    printf ("tsp_protocol = %d\n", rc);
    while (1) {
        uint8_t data[] = {0,128,255,0,128,255,0,128,255,0};
        rc = tsp_write( fd, data, 10, 2 );
        printf ("tsp_write = %d\n", rc);
    }
}
