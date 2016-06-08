#pragma once

#define MSG_TYPE_BOOTSTRAP_LOCAL_PATH  0x0001
#define MSG_TYPE_GET_BOOTSTRAP_VERSION 0x0002


#pragma pack(1)

typedef struct local_proto{
	unsigned short msg_type;
	unsigned short msg_len;
	char * data;
}LOCAL_PROTO_T;




#pragma pack()