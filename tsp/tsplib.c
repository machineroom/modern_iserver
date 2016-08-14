/*
    macihenroomfiddling@gmail.com implementation of transtech tsplib on Linux sg driver
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
#include <scsi/scsi.h>

#define DEFAULT_TIMEOUT (5)
//#define DEBUG

static void dump_error (sg_io_hdr_t *io_hdr) {
    bool status_ok = true;
    bool info_ok = true;
    bool host_ok = false;
    bool driver_ok = false;
    if (io_hdr->status != 0) {
        fprintf (stderr,"status = %02X : ", io_hdr->status);
        switch (io_hdr->status) {
            case 0x02: fprintf (stderr,"check condition\n"); break;
            default: fprintf (stderr,"*unknown*\n"); break;
        }
        status_ok = false;
    }
    if (status_ok) {
        if (io_hdr->info & 0x01) {
            fprintf (stderr,"info = SG_INFO_CHECK\n");
            info_ok = false;
        }
        if (!info_ok) {
            switch (io_hdr->host_status) {
                case 0x0:
                    host_ok = true;
                    fprintf (stderr,"host status = OK\n");
                    break;
                case 0x1:
                    fprintf (stderr,"host status = SG_ERR_DID_NO_CONNECT\n");
                    break;
                case 0x2:
                    fprintf (stderr,"host status = SG_ERR_DID_BUS_BUSY\n");
                    break;
                case 0x3:
                    fprintf (stderr,"host status = SG_ERR_DID_TIME_OUT\n");
                    break;
                case 0x4:
                    fprintf (stderr,"host status = SG_ERR_DID_BAD_TARGET\n");
                    break;
                case 0x5:
                    fprintf (stderr,"host status = SG_ERR_DID_ABORT\n");
                    break;
                case 0x6:
                    fprintf (stderr,"host status = SG_ERR_DID_PARITY\n");
                    break;
                case 0x7:
                    fprintf (stderr,"host status = SG_ERR_DID_ERROR\n");
                    break;
                case 0x8:
                    fprintf (stderr,"host status = SG_ERR_DID_RESET\n");
                    break;
                case 0x9:
                    fprintf (stderr,"host status = SG_ERR_DID_BAD_INTR\n");
                    break;
                case 0xA:
                    fprintf (stderr,"host status = SG_ERR_DID_PASSTHROUGH\n");
                    break;
                case 0xB:
                    fprintf (stderr,"host status = SG_ERR_DID_SOFT_ERROR\n");
                    break;
                default:
                    fprintf (stderr,"unknown host status 0x%X\n", io_hdr->host_status);
                    break;
            }
            //LS nibble is error, MS nibble is suggestion
            switch (io_hdr->driver_status & 0x0F) {
                case 0:
                    driver_ok = true;
                    fprintf (stderr,"driver status = OK\n");
                    break;
                case 1:
                    fprintf (stderr,"*E* DRIVER_BUSY\n");
                    break;
                case 2:
                    fprintf (stderr,"*E* DRIVER_SOFT\n");
                    break;
                case 3:
                    fprintf (stderr,"*E* DRIVER_MEDIA\n");
                    break;
                case 4:
                    fprintf (stderr,"*E* DRIVER_ERROR\n");
                    break;
                case 5:
                    fprintf (stderr,"*E* DRIVER_INVALID\n");
                    break;
                case 6:
                    fprintf (stderr,"*E* DRIVER_TIMEOUT\n");
                    break;
                case 7:
                    fprintf (stderr,"*E* DRIVER_HARD\n");
                    break;
                case 8:
                    fprintf (stderr,"*E* DRIVER_SENSE\n");
                    if (io_hdr->sb_len_wr > 0) {
                        int i;
                        fprintf (stderr,"sense buffer :");
                        for (i=0; i < io_hdr->sb_len_wr; i++) {
                            fprintf (stderr," %02X", io_hdr->sbp[i]);
                        }
                        uint8_t sense_key = io_hdr->sbp[2];
                        fprintf (stderr,"\nsense key = 0x%X = ", sense_key);
                        switch (sense_key) {
                            case 0x00: fprintf (stderr,"NO SENSE"); break;
                            case 0x01: fprintf (stderr,"RECOVERED ERROR"); break;
                            case 0x02: fprintf (stderr,"NOT READY"); break;
                            case 0x03: fprintf (stderr,"MEDIUM ERROR"); break;
                            case 0x04: fprintf (stderr,"HARDWARE ERROR"); break;
                            case 0x05: fprintf (stderr,"ILLEGAL REQUEST"); break;
                            case 0x06: fprintf (stderr,"UNIT ATTENTION"); break;
                            case 0x07: fprintf (stderr,"DATA PROTECT"); break;
                            case 0x08: fprintf (stderr,"BLANK CHECK"); break;
                            case 0x09: fprintf (stderr,"VENDOR SPECIFIC"); break;
                            case 0x0A: fprintf (stderr,"COPY ABORTED"); break;
                            case 0x0B: fprintf (stderr,"ABORTED COMMAND"); break;
                            case 0x0D: fprintf (stderr,"VOLUME OVERFLOW"); break;
                            case 0x0E: fprintf (stderr,"MISCOMPARE"); break;
                            case 0x0F: fprintf (stderr,"COMPLETED"); break;
                            case 0x0C: fprintf (stderr,"**UNKNOWN**"); break;
                        }
                        fprintf (stderr,"\n");
                        if (io_hdr->sbp[7] == 0x0A) {
                            //additional sense bytes
                            uint8_t ASC = io_hdr->sbp[12];
                            uint8_t ASCQ = io_hdr->sbp[13];
                            fprintf (stderr,"ASC = 0x%02X ASCQ = 0x%02X\n", ASC, ASCQ);
                        }
                    }
                    break;
                default:
                    fprintf (stderr,"unknown driver_status %X\n", io_hdr->driver_status);
                    break;
            }
            if (!driver_ok) {
                switch (io_hdr->driver_status & 0xF0 >> 4) {
                    case 1:
                        fprintf (stderr,"suggest = RETRY\n");
                        break;
                    case 2:
                        fprintf (stderr,"suggest = ABORT\n");
                        break;
                    case 3:
                        fprintf (stderr,"suggest = REMAP\n");
                        break;
                    case 4:
                        fprintf (stderr,"suggest = DIE\n");
                        break;
                    case 8:
                        fprintf (stderr,"suggest = SENSE\n");
                        break;
                    default:
                        fprintf (stderr,"unknown suggestion %X\n", io_hdr->driver_status);
                }
            }
        }
    }
}

//take syscall (ioctl) sttaus and SG header
//TSP lib expects calls to return 0 or -1
int check_error(int syscall_rc, char *scsi_op, sg_io_hdr_t *io_hdr) {
    if (syscall_rc == 0 && 
        io_hdr->status == 0 && 
        io_hdr->host_status == 0) {
        return 0;
    } else {
        fprintf (stderr,"*E*E*E*E*E*E*E*E*E*E*E*E\n");
        fprintf (stderr,"SCSI op %s failed\n", scsi_op);
        dump_error(io_hdr);
        return -1;
    }
}

//TODO assuming only single use!
//need to maintain mapping table and dish out our own virtul fd's
static int wfd=-1;
static int rfd=-1;

/* Library function prototypes */
int tsp_open( char *device ) {
    //device will be of form "sg2"
    if (wfd == -1 && rfd == -1) {
        int devnum;
        sscanf (device, "sg%i", &devnum);
        char dev[20];
        snprintf (dev, sizeof(dev), "/dev/sg%d", devnum);
        wfd = open(dev, O_RDWR);
        snprintf (dev, sizeof(dev), "/dev/sg%d", devnum+4);
        rfd = open(dev, O_RDWR);
        return 1;
    } else {
        fprintf (stderr,"URK don't know how to cope with >1 open!\n");
        assert(false);
        return -1;
    }
}

int tsp_close( int fd ) {
    close(wfd) && close(rfd);
    wfd = -1;
    rfd = -1;
    return 0;
}

static int transtech_command(int fd, 
                      uint8_t command, 
                      char *command_name, 
                      int dir, 
                      uint8_t *buffer, 
                      unsigned int *buffer_size, 
                      unsigned int timeout,
                      sg_io_hdr_t *io_hdr) {
    unsigned char cmdBlk[6];
    unsigned char sense_buffer[32];
    int rc;
    memset(&cmdBlk, 0, sizeof(cmdBlk));
    cmdBlk[0] = command;
    cmdBlk[1] = 0;  //LUN;
    cmdBlk[2] = (*buffer_size >> 16) & 0xFF;
    cmdBlk[3] = (*buffer_size >> 8)  & 0xFF;
    cmdBlk[4] = (*buffer_size >> 0)  & 0xFF;
    cmdBlk[5] = 0;  //unused
    /* for verification only*/
#ifdef DEBUG
    fprintf (stderr,"%s cmd blk = %02X %02X %02X %02X %02X %02X\n", 
             command_name, cmdBlk[0], cmdBlk[1], cmdBlk[2], cmdBlk[3], cmdBlk[4], cmdBlk[5]);
#endif
    memset(io_hdr, 0, sizeof(*io_hdr));
    io_hdr->interface_id = 'S';
    io_hdr->cmdp = cmdBlk;
    io_hdr->cmd_len = sizeof(cmdBlk);
    io_hdr->sbp = sense_buffer;
    io_hdr->mx_sb_len = sizeof(sense_buffer);
    io_hdr->dxfer_direction = dir;
    io_hdr->dxfer_len = *buffer_size;
    io_hdr->dxferp = buffer;
    io_hdr->timeout = timeout * 1000;     /* TSP lib is seconds, sg driver is ms */
    rc = ioctl(fd, SG_IO, io_hdr);
#ifdef DEBUG
    fprintf (stderr,"SG iohdr debug :\n");
    fprintf (stderr,"\tstatus = 0x%02X\n", io_hdr->status);
    fprintf (stderr,"\tmasked_status = 0x%02X\n", io_hdr->masked_status);
    fprintf (stderr,"\tmsg_status = 0x%02X\n", io_hdr->msg_status);
    fprintf (stderr,"\tsb_len_wr = %d\n", io_hdr->sb_len_wr);
    fprintf (stderr,"\thost_status = 0x%04X\n", io_hdr->host_status);
    fprintf (stderr,"\tdriver_status = 0x%04X\n", io_hdr->driver_status);
    fprintf (stderr,"\tresid = %d\n", io_hdr->resid);
    fprintf (stderr,"\tduration = %d\n", io_hdr->duration);
    fprintf (stderr,"\tinfo = 0x%04X\n", io_hdr->info);
#endif
    *buffer_size = io_hdr->dxfer_len - io_hdr->resid;
    return rc;
}

static bool ready(int fd) {
    uint8_t buff[5];
    unsigned int size = sizeof(buff);
    sg_io_hdr_t io_hdr;
    int ret;
    //read FLAGS
    ret = transtech_command (fd, SCMD_TEST_UNIT_READY, "SCMD_TEST_UNIT_READY", SG_DXFER_FROM_DEV, buff, &size, DEFAULT_TIMEOUT, &io_hdr);
    if (ret == 0 && io_hdr.status == GOOD) {
        return true;
    } else {
        return false;
    }
}

static int do_command(int fd, 
                      uint8_t command, 
                      char *command_name, 
                      int dir, 
                      uint8_t *buffer, 
                      unsigned int *buffer_size, 
                      unsigned int timeout)
{
    int rc, ret;
    while (!ready (fd)) {
#ifdef DEBUG
        fprintf (stderr,"%s !ready - waiting\n", command_name);
#endif
        usleep(100000);
    }
    sg_io_hdr_t io_hdr;
    rc = transtech_command (fd, command, command_name, dir, buffer, buffer_size, timeout, &io_hdr);
    ret = check_error (rc, command_name, &io_hdr);
    /* for verification only*/
#ifdef DEBUG
    fprintf (stderr,"%s dxfer_len = %u, resid = %u\n", command_name, io_hdr.dxfer_len, io_hdr.resid);
#endif
    return ret;
}

static int get_transtech_bits (int fd, unsigned char *flags, unsigned char *protocol, int *block_size) {
    uint8_t buff[5];
    unsigned int size = sizeof(buff);
    int ret;
    //read FLAGS
    ret = do_command (fd, SCMD_READ_FLAGS, "SCMD_READ_FLAGS", SG_DXFER_FROM_DEV, buff, &size, DEFAULT_TIMEOUT);
    if (flags != NULL) {
        *flags = buff[0];
    }
    if (protocol != NULL) {
        *protocol = buff[1];
    }
    if (block_size != NULL) {
        *block_size = buff[2];
        *block_size <<= 8;
        *block_size |= buff[3];
        *block_size <<= 8;
        *block_size |= buff[4];
    }
    return ret;
}

static int set_transtech_bits (int fd, unsigned char flags, unsigned char protocol, int block_size) {
    int ret;
    uint8_t buff[5];
    unsigned int size = sizeof(buff);
    buff[0] = flags;
    buff[1] = protocol;
    buff[2] = (block_size >> 16) & 0xFF;
    buff[3] = (block_size >> 8)  & 0xFF;
    buff[4] = (block_size >> 0)  & 0xFF;
    ret = do_command (fd, SCMD_WRITE_FLAGS, "SCMD_WRITE_FLAGS", SG_DXFER_TO_DEV, buff, &size, DEFAULT_TIMEOUT);
    return ret;
}

int tsp_reset( int fd ) {
    int ret;
    unsigned char flags;
    unsigned char protocol;
    int block_size;
    /*
     The link protocol is set to raw byte mode when the host link
     is   reset  by  a  call  to  tsp_reset(),  tsp_analyse()  or
     tsp_reset_config().  After being set by tsp_protocol()  this
     protocol persists until the link is reset.
    */
    ret = get_transtech_bits (rfd, &flags, &protocol, &block_size);
    flags = LFLAG_RESET_LINK | LFLAG_SUBSYSTEM_RESET;
    protocol = TSP_RAW_PROTOCOL;
    ret = set_transtech_bits (rfd, flags, protocol, block_size);
    if (ret == 0) {
        ret = get_transtech_bits (wfd, &flags, &protocol, &block_size);
        flags = LFLAG_RESET_LINK;
        protocol = TSP_RAW_PROTOCOL;
        ret = set_transtech_bits (wfd, flags, protocol, block_size);
    }
    return ret;
}

int tsp_analyse( int fd ) {
    int ret;
    unsigned char flags;
    unsigned char protocol;
    int block_size;
    /*
     The link protocol is set to raw byte mode when the host link
     is   reset  by  a  call  to  tsp_reset(),  tsp_analyse()  or
     tsp_reset_config().  After being set by tsp_protocol()  this
     protocol persists until the link is reset.
    */
    ret = get_transtech_bits (rfd, &flags, &protocol, &block_size);
    flags = LFLAG_RESET_LINK;
    flags |= LFLAG_SUBSYSTEM_ANALYSE;
    protocol = TSP_RAW_PROTOCOL;
    ret = set_transtech_bits (rfd, flags, protocol, block_size);
    if (ret == 0) {
        ret = get_transtech_bits (wfd, &flags, &protocol, &block_size);
        flags = LFLAG_RESET_LINK;
        flags |= LFLAG_SUBSYSTEM_ANALYSE;
        protocol = TSP_RAW_PROTOCOL;
        ret = set_transtech_bits (wfd, flags, protocol, block_size);
    }
    return ret;
}

int tsp_protocol( int fd, int protocol, int block_size ) {
    int ret;
    unsigned char flags;
    //link protocol only applies to read
    ret = get_transtech_bits (rfd, &flags, NULL, NULL);
    if (ret == 0) {
        ret = set_transtech_bits (rfd, flags, protocol, block_size);
    }
    return ret;
}

int tsp_error( int fd ) {
    int ret;
    unsigned char flags;
    ret = get_transtech_bits (rfd, &flags, NULL, NULL);
    if (ret == 0) {
        if ((flags & LFLAG_SUBSYSTEM_ERROR) == LFLAG_SUBSYSTEM_ERROR) {
            return 1;
        }
        ret = get_transtech_bits (wfd, &flags, NULL, NULL);
        if (ret == 0) {
            if ((flags & LFLAG_SUBSYSTEM_ERROR) == LFLAG_SUBSYSTEM_ERROR) {
                return 1;
            }
        }
    }
    return ret;
}

int tsp_read( int fd, void *data, size_t length, int timeout ) {
    int ret;
    unsigned int size = length;
    ret = do_command (rfd, SCMD_RECEIVE, "SCMD_RECEIVE", SG_DXFER_FROM_DEV, data, &size, timeout);
    if (ret == 0) {
        //return bytes read
        ret = size;
    }
    return ret;
}

int tsp_write( int fd, void *data, size_t length, int timeout ) {
    int ret;
    unsigned int size = length;
    ret = do_command (wfd, SCMD_SEND, "SCMD_SEND", SG_DXFER_TO_DEV, data, &size, timeout);
    if (ret == 0) {
        //return bytes written
        ret = size;
    }
    return ret;
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


