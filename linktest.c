#include <tsplib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

int main (int argc, char **argv) {
    int wfd;
    int rfd;
    int rc;
    wfd = tsp_open(argv[1]);
    printf ("tsp_open (%s) = %d\n", argv[1], wfd);
    rfd = tsp_open(argv[2]);
    printf ("tsp_open (%s) = %d\n", argv[2], rfd);
    rc = tsp_reset(wfd);
    printf ("tsp_reset (write) = %d\n", rc);
    rc = tsp_reset(rfd);
    printf ("tsp_reset (read)  = %d\n", rc);
/*    rc = tsp_analyse(wfd);
    printf ("tsp_analyse = %d\n", rc);
    rc = tsp_error(wfd);
    printf ("tsp_error = %d\n", rc);*/
    rc = tsp_protocol(wfd, TSP_RAW_PROTOCOL, 4096);
    printf ("tsp_protocol (write) = %d\n", rc);
    rc = tsp_protocol(rfd, TSP_RAW_PROTOCOL, 4096);
    printf ("tsp_protocol (read)  = %d\n", rc);
    //blatant copy from https://github.com/hessch/rpilink/blob/master/utils/tdetect.py
    uint8_t data[] = {0xb1,0xd1,0x24,0xf2,0x21,0xfc,0x24,
                      0xf2,0x21,0xf8,0xf0,0x60,0x5c,0x2a,
                      0x2a,0x2a,0x4a,0xff,0x21,0x2f,0xff,
                      0x02,0x00};
    rc = tsp_write(wfd, data, sizeof(data), 2 );
    printf ("tsp_write = %d\n", rc);
    uint8_t rx[256];
    memset (rx, 0xFF, sizeof(rx));
    rc = tsp_read(rfd, rx, 4/*sizeof(rx)*/, 2 );
    printf ("tsp_read = %d [%02X %02X %02X %02X]\n", rc, rx[0], rx[1], rx[2], rx[3]);
}
