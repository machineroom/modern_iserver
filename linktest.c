#include <tsplib.h>
#include <stdio.h>

int main (int argc, char **argv) {
    int fd;
    int rc;
    fd = tsp_open(argv[1]);
    printf ("tsp_open (%s) = %d\n", argv[1], fd);
    rc = tsp_reset(fd);
    printf ("tsp_reset = %d\n", rc);
    rc = tsp_analyse(fd);
    printf ("tsp_analyse = %d\n", rc);
}
