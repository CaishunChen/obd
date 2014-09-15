#ifndef JT808_H
#define JT808_H

#define BYTE unsigned char
#define WORD unsigned short 
#define DWORD unsigned int 

#define SWAP32(l) 	( (((l) >> 24) & 0x000000ff) | (((l) >> 8) & 0x0000ff00) | (((l) << 8) & 0x00ff0000) | (((l) << 24) & 0xff000000) )
#define SWAP16(w)	( (((w) >> 8) & 0x00ff) | (((w) << 8) & 0xff00) )

#define CLIENT_ANSWER_ID	0x0001
#define SERVER_ANSWER_ID	0x8001
#define CLIENT_HEARTBEAT_ID	0x0002
#define CLIENT_REGISTER_ID	0x0100
#define CLIENT_UNREGISTER_ID	0x0003
#define CLIENT_LOCATION_ID	0x0200
#define CLIENT_AUTH_ID		0x0102
typedef struct {
	WORD id;
	union{
		struct {
			WORD len:10;
			WORD enc:3;
			WORD pkts:1;
		};
		WORD value;
	}prop;
	BYTE phone[6];
	WORD seq;
}__attribute__((packed)) msgHdr;

typedef struct {
	WORD	seq;
	WORD	id;
	BYTE	result;
}__attribute__((packed)) msgAnswer;

typedef struct {
	WORD	province;
	WORD	county;
	BYTE	manufacture[5];	
	BYTE	deviceid[7];
	BYTE	color;
	BYTE	vehicleid[12];
}__attribute__((packed)) msgRegister;

typedef struct {
	WORD	seq;
	BYTE	result;
	BYTE	cookie[0];
}__attribute__((packed)) msgRegisterAnswer;

typedef struct {
	union{
		struct {
			DWORD	urgent:1;
					
		};
		DWORD	value;
	}alarm;

	union{
		struct {
			DWORD	acc:1;
			DWORD	gps:1;
			DWORD	ns:1;
			DWORD	ew:1;
			DWORD	oc:1;
			DWORD	loccrypt:1;
			DWORD	rsvd:4;
			DWORD	oil:1;
			DWORD	electric:1;
			DWORD	door:1;
		};
		DWORD	value;	
	}status;
}__attribute__((packed)) msgLongin;
#endif
