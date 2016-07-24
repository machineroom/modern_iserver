/*-----------------------------------------------------------------------------
--
-- CMSIDENTIFIER
-- PRODUCT:ITEM.VARIANT-TYPE;0(DATE) 
--
-- File    : opsprot.h
--  linkops protocol structs & constants codes
--
-- Copyright 1990, 1991 INMOS Limited
--
-- Date    : 6/26/91
-- Version : 1.5
--
-- 18/06/90 - dave edwards
--   originated
-- 06/08/90 - dave edwards 
--   altered re. protocol changes
-- 29/10/90 - dave edwards
--   added restart bits
-- 14/03/91 - dave edwards
--   added poke16, poke32, restart reply structures & range bounding
--   values for command & event min & max values.
-- 19/03/91 - dave edwards
--   changed linkops version string to 01.00
-- 25/03/91 - dave edwards
--   fixed fragmentation sizes for all supported hosts
-- 24/06/91 - dave edwards
--   made FRAG_SIZE constant for all hosts
-- 26/06/91 - dave edwards
--   added LINKOPS_PORT for DEC/TCP versions
--
-----------------------------------------------------------------------------*/


/* names used in host network service database */
#define SERVICENAME "linkops"
#define SERVICEPROTOCOL "tcp"

/* port number used for Ultrix Connect version (no services database) */
#define LINKOPS_PORT 4047

/* service message string */
#define SERVICE_OPSOPEN "inmos.com(tcplink-linkops-01.00)"
#define VERYWRONG_STRING "-\n"

/* maximum number of bytes in a linkops packet (fragmentation size) */
#define FRAG_SIZE (8192)

/* maximum amount of data in a linkops message */
#define OPSMAXPACKET 8192

/* maximum size of linkops message fields (excluding data) */
#define OPSMAXOVERHEADS 13

/* minimum size of response message (just packet.size & tag fields) */
#define OPSMIN_RESPONSEMESSAGESIZE 4                                  

/* offsets into all linkops messages */
#define OPSPACKETSIZEFIELD 0
#define OPSTAGFIELD 2


/* tag values for linkops command messages */
#define OCMD_Open               1
#define OCMD_Close              2
#define OCMD_WriteLink          3
#define OCMD_ReadLink           4
#define OCMD_Reset              5
#define OCMD_Analyse            6
#define OCMD_TestError          7
#define OCMD_Poke16             8
#define OCMD_Poke32             9
#define OCMD_Peek16             10
#define OCMD_Peek32             11
#define OCMD_ErrorDetect        12
#define OCMD_ErrorIgnore        13
#define OCMD_CommsSynchronous   14
#define OCMD_CommsAsynchronous  15
#define OCMD_AsyncWrite         16
#define OCMD_Restart            17

/* numerically smallest command tag value */
#define OPS_MinCommandTag 1
/* numerically largest command tag value */
#define OPS_MaxCommandTag 17

/* tag values for reply messages */
#define OREPLY_Open             65
#define OREPLY_Close            66
#define OREPLY_WriteLink        67
#define OREPLY_ReadLink         68
#define OREPLY_TestError        71
#define OREPLY_Poke16           72
#define OREPLY_Poke32           73
#define OREPLY_Peek16           74
#define OREPLY_Peek32           75
#define OREPLY_CommsSynchronous 78
#define OREPLY_Restart          81

/* tag value for INMOS server replies */
#define INMOSREPLY_BadService   255
#define INMOSREPLY_BadVersion   254

/* tag values for linkops event messages */
#define OEVENT_Message          128
#define OEVENT_Request          129
#define OEVENT_ErrorTransition  130

/* numerically smallest event tag */
#define OPS_MinEventTag 128
/* numerically largest event tag */
#define OPS_MaxEventTag 130

/****
***** linkops  message structures
****/

struct OPSOpenCommand {
  char service_message[40];
  unsigned char command_tag;
  char resource_name[65];
};
#define OPSOpenCmdSize 106

struct OPSOpenReply {
  char service_message[40];
  unsigned char reply_tag;
  unsigned char status;
  char device_name[65];
};
#define OPSOpenReplySize 107

struct OPSCloseCommand {
  unsigned short packet_size;
  unsigned char command_tag;
};
#define OPSCloseCommandSize 3

struct OPSCloseReply {
  unsigned short packet_size;
  unsigned char reply_tag;
  unsigned char status;
};
#define OPSCloseReplySize 4
#define OREPLYCLOSE_packetsize 0
#define OREPLYCLOSE_replytag 2
#define OREPLYCLOSE_status     3

struct OPSResetCommand {
  unsigned short packet_size;
  unsigned char command_tag;
  char processor_id[4];
};
#define OPSResetCommandSize 7

struct OPSAnalyseCommand {
  unsigned short packet_size;
  unsigned char command_tag;
  char processor_id[4];
};
#define OPSAnalyseCommandSize 7

struct OPSCommsAsynchronousCommand {
  unsigned short packet_size;
  unsigned char command_tag;
};
#define OPSCommsAsynchronousCommandSize 3

struct OPSCommsSynchronousCommand {
  unsigned short packet_size;
  unsigned char command_tag;
};
#define OPSCommsSynchronousCommandSize 3

struct OPSCommsSynchronousReply {
  unsigned short packet_size;
  unsigned char reply_tag;
  unsigned char status;
};
#define OPSCommsSynchronousReplySize 4
#define OREPLYCOMMSSYNCHRONOUS_status 3

struct OPSTestErrorCommand {
  unsigned short packet_size;
  unsigned char command_tag;
  char processor_id[4];
};
#define OPSTestErrorCommandSize 7

struct OPSTestErrorReply {
  unsigned short packet_size;
  unsigned char reply_tag;
  unsigned char status;
  char processor_id[4];
};
#define OPSTestErrorReplySize 8
#define OREPLYTESTERROR_status 3
#define OREPLYTESTERROR_processorid 4

struct OPSWriteLinkCommand {
  unsigned short packet_size;
  unsigned char command_tag;
  unsigned char timeout[2];
  /* note the data is transmitted separately */
};
#define OPSWriteLinkCommandBasicSize 5
#define OCMDWRITELINK_packetsize 0
#define OCMDWRITELINK_timeout 3
#define OCMDWRITELINK_data 5

struct OPSWriteLinkReply {
  unsigned short packet_size;
  unsigned char reply_tag;
  unsigned char status;
  unsigned short bytes_written;
};
#define OPSWriteLinkReplySize 6
#define OREPLYWRITELINK_status 3
#define OREPLYWRITELINK_byteswritten 4

struct OPSReadLinkCommand {
  unsigned short packet_size;
  unsigned char command_tag;
  unsigned char timeout[2];
  unsigned char readsize[2];
};
#define OPSReadLinkCommandSize 7

#define OPSReadLinkReplyBasicSize 4
struct OPSReadLinkReply {
  unsigned short packet_size;
  unsigned char reply_tag;
  unsigned char status;
  unsigned char data[OPSMAXPACKET];
};
#define OREPLYREADLINK_status 3
#define OREPLYREADLINK_data   4


struct OPSPeek16Command {
  unsigned short packet_size;
  unsigned char command_tag;
  char processor_id[4];
  unsigned char peek_length[2];
  char address[2];
};
#define OPSPeek16CommandSize 11

struct OPSPeek16Reply {
  unsigned short packet_size;
  unsigned char reply_tag;
  unsigned char status;
  char processor_id[4];
  /* note the data is transmitted separately */
};
#define OPSPeek16ReplyBasicSize 8
#define OREPLYPEEK16_status 3
#define OREPLYPEEK16_processorid 4
#define OREPLYPEEK16_peekdata 8

struct OPSPeek32Command {
  unsigned short packet_size;
  unsigned char command_tag;
  char processor_id[4];
  unsigned char peek_length[2];
  char address[4];      
};
#define OPSPeek32CommandSize 13

struct OPSPeek32Reply {        
  unsigned short packet_size;
  unsigned char reply_tag;
  unsigned char status;
  char processor_id[4];
  /* note the data is transmitted separately */
};
#define OPSPeek32ReplyBasicSize 8
#define OREPLYPEEK32_status 3
#define OREPLYPEEK32_processorid 4
#define OREPLYPEEK32_peekdata 8


struct OPSPoke16Command {
  unsigned short packet_size;
  unsigned char command_tag;
  char processor_id[4];
  char address[2]; 
  /* note the data is transmitted separately */
};
#define OPSPoke16CommandBasicSize 9
#define OCMDPOKE16_pokedata 9

struct OPSPoke16Reply {
  unsigned short packet_size;
  unsigned char reply_tag;
  unsigned char status;
};
#define OPSPoke16ReplySize 4
#define OREPLYPOKE16_status 3

struct OPSPoke32Command {
  unsigned short packet_size;
  unsigned char command_tag;
  char processor_id[4];
  char address[4];
  /* note the data is transmitted separately */
};
#define OPSPoke32CommandBasicSize 11
#define OCMDPOKE32_pokedata 11

struct OPSPoke32Reply {
  unsigned short packet_size;
  unsigned char reply_tag;
  unsigned char status;
};
#define OPSPoke32ReplySize 4
#define OREPLYPOKE32_status 3
  
struct OPSErrorDetectCommand {
  unsigned short packet_size;
  unsigned char command_tag;
};
#define OPSErrorDetectCommandSize 3

struct OPSErrorIgnoreCommand {
  unsigned short packet_size;
  unsigned char command_tag;
};
#define OPSErrorIgnoreCommandSize 3

struct OPSAsyncWriteCommand {
  unsigned short packet_size;
  unsigned char command_tag;
  /* note the data is transmitted separately */
};
#define OPSAsyncWriteCommandBasicSize 3

struct OPSRestartCommand {
   unsigned short packet_size;
   unsigned char command_tag;
};
#define OPSRestartCommandSize 3

struct OPSRestartReply {
  unsigned short packet_size;
  unsigned char reply_tag;
  unsigned char status;
};
#define OPSRestartReplySize 4
#define OREPLYRESTART_status  3

#define OPSMessageEventBasicSize 4
struct OPSMessageEvent {
  unsigned short packet_size;
  unsigned char event_tag;
  unsigned char fatal;
  char string[OPSMAXPACKET];
};
#define OEVENTMESSAGE_packetsize 0
#define OEVENTMESSAGE_eventtag   2
#define OEVENTMESSAGE_fatal      3
#define OEVENTMESSAGE_string     4

#define OPSRequestEventBasicSize 4
struct OPSRequestEvent {
  unsigned short packet_size;
  unsigned char event_tag;
  unsigned char status;
  char sp_packet[OPSMAXPACKET];
};
#define OEVENTREQUEST_packetsize 0
#define OEVENTREQUEST_eventtag   2
#define OEVENTREQUEST_status     3
#define OEVENTREQUEST_sppacket   4


struct OPSErrorTransitionEvent {
  unsigned short packet_size;
  unsigned char event_tag;
  unsigned char processor_id[4];
  unsigned char state;
};
#define OPSErrorTransitionEventSize 8
#define OEVENTERRORTRANS_packetsize  0
#define OEVENTERRORTRANS_eventtag    2
#define OEVENTERRORTRANS_processorid 3
#define OEVENTERRORTRANS_state       7




