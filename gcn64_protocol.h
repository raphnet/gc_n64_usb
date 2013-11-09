#ifndef _gcn64_protocol_h__
#define _gcn64_protocol_h__

#define CONTROLLER_IS_ABSENT		0
#define CONTROLLER_IS_N64			1
#define CONTROLLER_IS_GC			2
#define CONTROLLER_IS_GC_KEYBOARD	3
#define CONTROLLER_IS_UNKNOWN		4


/* Return many unknown bits, but two are about the expansion port. */
#define N64_GET_CAPABILITIES		0x00
#define N64_CAPS_REPLY_LENGTH		24

#define OFFSET_EXT_REMOVED			22
#define OFFSET_EXT_PRESENT			23

/* Returns button states and axis values */
#define N64_GET_STATUS				0x01
#define N64_GET_STATUS_REPLY_LENGTH	32

/* Read from the expansion bus. */
#define N64_EXPANSION_READ			0x02

/* Write to the expansion bus. */
#define N64_EXPANSION_WRITE			0x03

/* Return information about controller. */
#define GC_GETID					0x00
#define GC_GETID_REPLY_LENGTH		24

/* 3-byte get status command. Returns axis and buttons. Also 
 * controls motor. */
#define GC_GETSTATUS1				0x40
#define GC_GETSTATUS2				0x03
#define GC_GETSTATUS3(rumbling)		((rumbling) ? 0x01 : 0x00)
#define GC_GETSTATUS_REPLY_LENGTH	64

/* 3-byte poll keyboad command.
 * Source: http://hitmen.c02.at/files/yagcd/yagcd/chap9.html#sec9.3.3
 * */
#define GC_POLL_KB1					0x54
#define GC_POLL_KB2					0x00
#define GC_POLL_KB3					0x00

#define GC_BTN_L	9
#define GC_BTN_R	10
#define GC_BTN_Z	11

void gcn64protocol_hwinit(void);
int gcn64_detectController(void);
int gcn64_transaction(unsigned char *data_out, int data_out_len);

unsigned char gcn64_protocol_getByte(int offset);
void gcn64_protocol_getBytes(int offset, int n_bytes, unsigned char *dstbuf);

extern volatile unsigned char gcn64_workbuf[];

#endif // _gcn64_protocol_h__
