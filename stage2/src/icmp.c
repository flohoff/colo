/*
 * (C) P.Horton 2004
 *
 * $Id$
 *
 * This code is covered by the GNU General Public License. For details see the file "COPYING".
 */

#include "lib.h"
#include "net.h"

#define ICMP_TYPE_ECHO_REQUEST					8
# define ICMP_CODE_ECHO_REQUEST					0
#define ICMP_TYPE_ECHO_REPLY						0
# define ICMP_CODE_ECHO_REPLY						0

void icmp_in(struct frame *frame)
{
	unsigned size, cksum;
	void *data, *reply;
	uint32_t targ;

	size = FRAME_SIZE(frame);
	data = FRAME_PAYLOAD(frame);

	if(size < ICMP_HDRSZ ||
		NET_READ_BYTE(data + 0) != ICMP_TYPE_ECHO_REQUEST ||
		NET_READ_BYTE(data + 1) != ICMP_CODE_ECHO_REQUEST ||
		ip_checksum(0, data, size) != 0xffff) {

		return;
	}

	targ = frame->ip_src;

	frame = frame_alloc();
	if(!frame)
		return;

	FRAME_INIT(frame, HARDWARE_HDRSZ + IP_HDRSZ, size);

	reply = FRAME_PAYLOAD(frame);

	NET_WRITE_BYTE(reply + 0, ICMP_TYPE_ECHO_REPLY);
	NET_WRITE_BYTE(reply + 1, ICMP_CODE_ECHO_REPLY);
	NET_WRITE_SHORT(reply + 2, 0);

	memcpy(reply + 4, data + 4, size - 4);

	cksum = ip_checksum(0, reply, size);

	NET_WRITE_SHORT(reply + 2, ~cksum);

	ip_out(frame, targ, IPPROTO_ICMP);
}

/* vi:set ts=3 sw=3 cin path=include,../include: */
