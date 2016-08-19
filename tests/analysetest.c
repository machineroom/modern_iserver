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
    rc = tsp_analyse(fd);
    printf ("tsp_analyse = %d\n", rc);
    tsp_close(fd);
}


