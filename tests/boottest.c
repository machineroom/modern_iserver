#include <tsplib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

int main (int argc, char **argv) {
    int fd;
    int rc;
    fd = tsp_open(argv[1]);
    printf ("tsp_open (%s) = %d\n", argv[1], fd);
    rc = tsp_reset(fd);
    printf ("tsp_reset = %d\n", rc);
    rc = tsp_protocol(fd, TSP_RAW_PROTOCOL, 4096);
    printf ("tsp_protocol = %d\n", rc);
    //blatant copy from https://github.com/hessch/rpilink/blob/master/utils/tdetect.py
    // & http://www.geekdot.com/category/software/transputer-software/ (iTest)
    uint8_t boot[] = 
            {
             0,                             /* size of bootstrap */
             0xB1,                          /* AJW 1, allow for Iptr store    */
             0xD1,                          /* STL 1                          */
             0x24, 0xF2,                    /* MINT                           */
             0x21, 0xFC,                    /* STHF                           */
             0x24, 0xF2,                    /* MINT                           */
             0x21, 0xF8,                    /* STLF                           */
             0xF0,                          /* REV                            */
             0x60, 0x5C,                    /* LDNLP -4                       */
             0x2A, 0x2A, 0x2A, 0x4A,        /* LDC #AAAA                      */
             0xFF,                          /* OUTWORD                        */
             0x21, 0x2F, 0xFF              /* START                          */
             };  
    boot[0] = sizeof (boot)-1;  /*"If the first byte received is 2 or greater then that many bytes of code 
                                   will be input from the link and placed in memory starting at MEMSTART.
                                   This code will then be executed."
                                   http://www.wizzy.com/wizzy/transputer_faq.txt*/
    rc = tsp_write(fd, boot, sizeof(boot), 2 );
    printf ("tsp_write = %d\n", rc);
    uint8_t rx[256];
    int tot=0;
    do {
        rc = tsp_read(fd, &rx[tot], 1, 1 );
        if (rc == 1) {
            tot++;
        }
    } while (rc == 1);
    printf ("tsp_read tot = %d [%02X %02X %02X %02X %02X %02X]\n", tot, rx[0], rx[1], rx[2], rx[3], rx[4], rx[5]);
    tsp_close(fd);
}


