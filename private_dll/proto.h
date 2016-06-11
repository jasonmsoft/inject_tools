#pragma once

#define MSG_TYPE_BOOTSTRAP_LOCAL_PATH    0x0001
#define MSG_TYPE_GET_BOOTSTRAP_VERSION   0x0002
#define MSG_TYPE_REPLY_BOOTSTRAP_VERSION 0x0003
#define MSG_TYPE_REGISTER                0x0004
#define MSG_TYPE_DOWNLOAD_BOOTSTRAP      0x0005



/*
msg header
msg body
+++++msg type++++++++++++++++direction++++

MSG_TYPE_BOOTSTRAP_LOCAL_PATH  just receive from inj.exe
msg header
path string


MSG_TYPE_GET_BOOTSTRAP_VERSION  send to server
msg header


MSG_TYPE_REPLY_BOOTSTRAP_VERSION reply from server
msg header
version  8 bytes


MSG_TYPE_REGISTER            send to server
msg header
os version  4bytes
local ip address 4bytes
number of partions 1byte
mac address        6 bytes


MSG_TYPE_DOWNLOAD_BOOTSTRAP send to server
msg header



*/

#define PROTO_VERSION  1.0


#pragma pack(1)

typedef struct local_proto{
	float version;
	unsigned short msg_type;
	unsigned short body_len;
}LOCAL_PROTO_T;


#define UDP_SERVER_PORT 4789


#pragma pack()