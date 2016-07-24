/*
    macihenroomfiddling@google.com implementation of transtech tsplib on Linux sg driver
 */

//Transtech headers
#include <tsplib.h>     //the thing we're implementing
#include <scsilink.h>   //commands understood by the Matchbox

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <scsi/sg.h>

static void dump_error (sg_io_hdr_t *io_hdr) {
    bool info_ok = true;
    bool host_ok = false;
    bool driver_ok = false;
    if (io_hdr->info & 0x01) {
        printf ("info = INFO_CHECK\n");
        info_ok = false;
    }
    if (!info_ok) {
        switch (io_hdr->host_status) {
            case 0x0:
                host_ok = true;
                printf ("host status = OK\n");
                break;
            case 0x1:
                printf ("host status = SG_ERR_DID_NO_CONNECT\n");
                break;
            case 0x2:
                printf ("host status = SG_ERR_DID_BUS_BUSY\n");
                break;
            case 0x3:
                printf ("host status = SG_ERR_DID_TIME_OUT\n");
                break;
            case 0x4:
                printf ("host status = SG_ERR_DID_BAD_TARGET\n");
                break;
            case 0x5:
                printf ("host status = SG_ERR_DID_ABORT\n");
                break;
            case 0x6:
                printf ("host status = SG_ERR_DID_PARITY\n");
                break;
            case 0x7:
                printf ("host status = SG_ERR_DID_ERROR\n");
                break;
            case 0x8:
                printf ("host status = SG_ERR_DID_RESET\n");
                break;
            case 0x9:
                printf ("host status = SG_ERR_DID_BAD_INTR\n");
                break;
            case 0xA:
                printf ("host status = SG_ERR_DID_PASSTHROUGH\n");
                break;
            case 0xB:
                printf ("host status = SG_ERR_DID_SOFT_ERROR\n");
                break;
            default:
                printf ("unknown host status 0x%X\n", io_hdr->host_status);
                break;
        }
        //LS nibble is error, MS nibble is suggestion
        switch (io_hdr->driver_status & 0x0F) {
            case 0:
                driver_ok = true;
                printf ("driver status = OK\n");
                break;
            case 1:
                printf ("*E* DRIVER_BUSY\n");
                break;
            case 2:
                printf ("*E* DRIVER_SOFT\n");
                break;
            case 3:
                printf ("*E* DRIVER_MEDIA\n");
                break;
            case 4:
                printf ("*E* DRIVER_ERROR\n");
                break;
            case 5:
                printf ("*E* DRIVER_INVALID\n");
                break;
            case 6:
                printf ("*E* DRIVER_TIMEOUT\n");
                break;
            case 7:
                printf ("*E* DRIVER_HARD\n");
                break;
            case 8:
                printf ("*E* DRIVER_SENSE\n");
                if (io_hdr->sb_len_wr > 0) {
                    int i;
                    printf ("sense buffer :");
                    for (i=0; i < io_hdr->sb_len_wr; i++) {
                        printf (" %02X", io_hdr->sbp[i]);
                    }
                    uint8_t sense_key = io_hdr->sbp[2];
                    printf ("\nsense key = 0x%X = ", sense_key);
                    switch (sense_key) {
                        case 0x00: printf ("NO SENSE"); break;
                        case 0x01: printf ("RECOVERED ERROR"); break;
                        case 0x02: printf ("NOT READY"); break;
                        case 0x03: printf ("MEDIUM ERROR"); break;
                        case 0x04: printf ("HARDWARE ERROR"); break;
                        case 0x05: printf ("ILLEGAL REQUEST"); break;
                        case 0x06: printf ("UNIT ATTENTION"); break;
                        case 0x07: printf ("DATA PROTECT"); break;
                        case 0x08: printf ("BLANK CHECK"); break;
                        case 0x09: printf ("VENDOR SPECIFIC"); break;
                        case 0x0A: printf ("COPY ABORTED"); break;
                        case 0x0B: printf ("ABORTED COMMAND"); break;
                        case 0x0D: printf ("VOLUME OVERFLOW"); break;
                        case 0x0E: printf ("MISCOMPARE"); break;
                        case 0x0F: printf ("COMPLETED"); break;
                        case 0x0C: printf ("**UNKNOWN**"); break;
                    }
                    printf ("\n");
                    if (io_hdr->sbp[7] == 0x0A) {
                        //additional sense bytes
                        uint8_t ASC = io_hdr->sbp[12];
                        uint8_t ASCQ = io_hdr->sbp[13];
                        printf ("ASC = 0x%02X ASCQ = 0x%02X\n", ASC, ASCQ);
                    }
                }
                break;
            default:
                printf ("unknown driver_status %X\n", io_hdr->driver_status);
                break;
        }
        if (!driver_ok) {
            switch (io_hdr->driver_status & 0xF0 >> 4) {
                case 1:
                    printf ("suggest = RETRY\n");
                    break;
                case 2:
                    printf ("suggest = ABORT\n");
                    break;
                case 3:
                    printf ("suggest = REMAP\n");
                    break;
                case 4:
                    printf ("suggest = DIE\n");
                    break;
                case 8:
                    printf ("suggest = SENSE\n");
                    break;
                default:
                    printf ("unknown suggestion %X\n", io_hdr->driver_status);
            }
        }
    }
}

//take syscall (ioctl) sttaus and SG header
//TSP lib expects calls to return 0 or -1
int check_error(int syscall_rc, char *scsi_op, sg_io_hdr_t *io_hdr) {
    if (syscall_rc == 0) {
        return 0;
    } else {
        printf ("SCSI op %s failed\n", scsi_op);
        dump_error(io_hdr);
        return -1;
    }
}

/* Library function prototypes */
int tsp_open( char *device ) {
    int sg_fd;
    sg_fd = open(device, O_RDWR);
    return sg_fd;
}

int tsp_close( int fd ) {
    return close(fd);
}

static int do_command(int fd, uint8_t command, char *command_name, int dir, uint8_t *buffer, unsigned int buffer_size) {
    unsigned char cmdBlk[] = {command, 0, 0, 0, buffer_size, 0};
    unsigned char sense_buffer[32];
    sg_io_hdr_t io_hdr;
    int rc, ret;
    memset(&io_hdr, 0, sizeof(sg_io_hdr_t));
    io_hdr.interface_id = 'S';
    io_hdr.cmd_len = sizeof(cmdBlk);
    io_hdr.mx_sb_len = sizeof(sense_buffer);
    io_hdr.dxfer_direction = dir;
    io_hdr.dxfer_len = buffer_size;
    io_hdr.dxferp = buffer;
    io_hdr.cmdp = cmdBlk;
    io_hdr.sbp = sense_buffer;
    io_hdr.timeout = 20000;     /* 20000 millisecs == 20 seconds */
    rc = ioctl(fd, SG_IO, &io_hdr);
    ret = check_error (rc, command_name, &io_hdr);
    return ret;
}

//Reset a host link connection
//Returns zero on success or -1 on error.
int tsp_reset( int fd ) {
    uint8_t buff[5];
    int ret;
    //read FLAGS
    ret = do_command (fd, SCMD_READ_FLAGS, "SCMD_READ_FLAGS", SG_DXFER_FROM_DEV, buff, sizeof(buff));
    if (ret == 0) {
        //write FLAGS
        buff[0] = LFLAG_RESET_LINK;
        ret = do_command (fd, SCMD_WRITE_FLAGS, "SCMD_WRITE_FLAGS", SG_DXFER_TO_DEV, buff, sizeof(buff));
    }
    return ret;
}

//resets the link and (with analyse asserted) the connected devices of the host link
//Returns zero on success or -1 on error.
int tsp_analyse( int fd ) {
    uint8_t buff[5];
    int ret;
    //read FLAGS
    ret = do_command (fd, SCMD_READ_FLAGS, "SCMD_READ_FLAGS", SG_DXFER_FROM_DEV, buff, sizeof(buff));
    if (ret == 0) {
        //write FLAGS
        buff[0] = LFLAG_SUBSYSTEM_ANALYSE;
        ret = do_command (fd, SCMD_WRITE_FLAGS, "SCMD_WRITE_FLAGS", SG_DXFER_TO_DEV, buff, sizeof(buff));
    }
    return ret;
}

//returns the state of the error line of the host link connection specified by
//Returns zero or one indicating the state of the error line on success or -1 on error.
int tsp_error( int fd ) {
    uint8_t buff[5];
    int ret;
    //read FLAGS
    ret = do_command (fd, SCMD_READ_FLAGS, "SCMD_READ_FLAGS", SG_DXFER_FROM_DEV, buff, sizeof(buff));
    if (ret == 0) {
        if ((buff[0] & LFLAG_SUBSYSTEM_ERROR) == LFLAG_SUBSYSTEM_ERROR) {
            return 1;
        }
    }
    return ret;
}

//By default a raw link protocol is used in which the target attempts to input
//length bytes of data (in 4096 byte blocks) until a timeout occurs.
//If iserver link protocol is used, an iserver packet of total length of up to
//length (or 4096) bytes containing a two byte count and count bytes of data is input.
//In block link protocol mode, an attempt is made to read a block of data of
//length length. The link protocol and block size is set using tsp_protocol().
//In protocol modes other than raw mode, the length of data requested must be
//the same as the block size specified by the function tsp_protocol().
//If a timeout occurs during a read, it is an error to subsequently request a 
//lesser amount of data until either the original request is satisfied or 
//until the connection is reset using tsp_reset().

//data: Pointer to memory into which data is read.
//length: The maximum amount of data to be read.
//timeout: Timeout in seconds. Zero for no timeout.
//Returns the number of bytes read (which may be less than that requested) on success or -1 on error.
int tsp_read( int fd, void *data, size_t length, int timeout ) {
    assert(false);
}

int tsp_write( int fd, void *data, size_t length, int timeout ) {
    assert(false);
}

int tsp_protocol( int fd, int protocol, int block_size ) {
    assert(false);
}

int tsp_reset_config( int fd ) {
    assert(false);
}

int tsp_read_config( int fd, void *data, size_t length, int timeout ) {
    assert(false);
}

int tsp_write_config( int fd, void *data, size_t length, int timeout ) {
    assert(false);
}

int tsp_connect( int fd, char n1, char l1, char n2, char l2 ) {
    assert(false);
}

int tsp_mknod( char *device ) {
    assert(false);
}


