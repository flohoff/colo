/*
 * (C) P.Horton 2004,2005,2006
 *
 * $Id$
 *
 * This code is covered by the GNU General Public License. For details see the file "COPYING".
 */

#include "lib.h"
#include "net.h"
#include "cpu.h"

#define ICMP_TYPE_ECHO_REQUEST					8
#define ICMP_CODE_ECHO_REQUEST_REPLY			0
#define ICMP_TYPE_ECHO_REPLY						0

static struct
{
	uint32_t src;
	unsigned seqnr;
	unsigned ticks;

} reply_queue[8];

static unsigned reply_in;
static unsigned reply_out;

/*
 * process received ICMP packet
 */
void icmp_in(struct frame *frame)
{
	unsigned size, cksum, indx;
	void *data, *reply;
	uint32_t targ;

	size = FRAME_SIZE(frame);
	data = FRAME_PAYLOAD(frame);

	if(size < ICMP_HDRSZ ||
		NET_READ_BYTE(data + 1) != ICMP_CODE_ECHO_REQUEST_REPLY ||
		ip_checksum(0, data, size) != 0xffff) {

		return;
	}

	targ = frame->ip_src;

	switch(NET_READ_BYTE(data + 0)) {

		case ICMP_TYPE_ECHO_REPLY:

			if(size >= 4 + 4 + 4 && reply_in - reply_out < elements(reply_queue)) {

				indx = reply_in++ % elements(reply_queue);

				reply_queue[indx].src = targ;
				reply_queue[indx].seqnr = NET_READ_LONG(data + 4);
				reply_queue[indx].ticks = NET_READ_LONG(data + 8);
			}
			
			break;

		case ICMP_TYPE_ECHO_REQUEST:

			FRAME_BUMP(frame);
			FRAME_INIT(frame, HARDWARE_HDRSZ + IP_HDRSZ, size);

			reply = FRAME_PAYLOAD(frame);
			memmove(reply, data, size);

			NET_WRITE_BYTE(reply + 0, ICMP_TYPE_ECHO_REPLY);

			/* fix up checksum (RFC 1624) */

			cksum = (NET_READ_SHORT(reply + 2) ^ 0xffff) +
				(ICMP_TYPE_ECHO_REQUEST ^ 0xffff) +
				ICMP_TYPE_ECHO_REPLY;

			while(cksum & 0xffff0000)
				cksum = (cksum >> 16) + (cksum & 0xffff);

			NET_WRITE_SHORT(reply + 2, ~cksum);

			ip_out(frame, targ, IPPROTO_ICMP);
	}
}


int cmnd_ping(int opsz)
{
#	define TICKS_PER_SEC			10000
#	define COUNTS_PER_TICK		((CP0_COUNT_RATE + TICKS_PER_SEC / 2) / TICKS_PER_SEC)

	unsigned mark, ticks, diff, indx, since, seqnr, cksum;
	struct frame *frame;
	uint32_t host;
	void *data;

	if(argc < 2)
		return E_ARGS_UNDER;
	if(argc > 2)
		return E_ARGS_OVER;

	if(!inet_aton(argv[1], &host)) {
		puts("invalid address");
		return E_UNSPEC;
	}

	if(!net_is_up())
		return E_NET_DOWN;

	ticks = TICKS_PER_SEC;
	since = 0;
	seqnr = 0;

	reply_out = 0;
	reply_in = 0;

	for(mark = MFC0(CP0_COUNT); !BREAK();) {

		diff = (MFC0(CP0_COUNT) - mark) / COUNTS_PER_TICK;
		mark += diff * COUNTS_PER_TICK;
		ticks += diff;
		
		if(ticks - since >= TICKS_PER_SEC) {
			since += TICKS_PER_SEC;

			frame = frame_alloc();
			if(frame) {

				FRAME_INIT(frame, HARDWARE_HDRSZ + IP_HDRSZ, 4 + 4 + 4);

				data = FRAME_PAYLOAD(frame);

				NET_WRITE_BYTE(data + 0, ICMP_TYPE_ECHO_REQUEST);
				NET_WRITE_BYTE(data + 1, ICMP_CODE_ECHO_REQUEST_REPLY);
				NET_WRITE_SHORT(data + 2, 0);

				NET_WRITE_LONG(data + 4, seqnr);
				NET_WRITE_LONG(data + 8, ticks);

				cksum = ip_checksum(0, data, 4 + 4 + 4);

				NET_WRITE_SHORT(data + 2, ~cksum);

				ip_out(frame, host, IPPROTO_ICMP);

				++seqnr;

			} else

				puts("out of frames");

		} else if(reply_in != reply_out) {

			indx = reply_out++ % elements(reply_queue);

			diff = ticks - reply_queue[indx].ticks;

			printf("%s: seq=%u time=%u.%u ms\n",
				inet_ntoa(reply_queue[indx].src), reply_queue[indx].seqnr, diff / 10, diff % 10);
		}
	}

	return E_NONE;
}

/* vi:set ts=3 sw=3 cin path=include,../include: */
