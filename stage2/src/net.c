/*
 * (C) P.Horton 2004
 *
 * $Id$
 *
 * This code is covered by the GNU General Public License. For details see the file "COPYING".
 */

#include "lib.h"
#include "net.h"

#define BUFFER_COUNT						16

static struct frame *pool;

struct frame *frame_alloc(void)
{
	struct frame *frame;

	frame = pool;

	if(frame) {
		pool = frame->link;
		frame->link = NULL;
		frame->refs = 1;
	} else
		DPUTS("net: out of buffers");

	return frame;
}

void frame_free(struct frame *frame)
{
	if(!--frame->refs) {
		frame->link = pool;
		pool = frame;
	}
}

void net_init(void)
{
	static uint8_t store[BUFFER_COUNT * ((sizeof(struct frame) + 31) & ~31) + 31];

	unsigned curr, indx;
	struct frame *prev;
	void *base;

	/* allocate buffers on 16 byte boundaries */

	curr = (unsigned long) store & 15;
	base = store - curr;
	prev = NULL;

	for(indx = 0; indx < BUFFER_COUNT; ++indx) {

		curr += -curr & 31;

		pool = (struct frame *)(base + curr);

		pool->link = prev;
		prev = pool;

		curr += sizeof(struct frame);
	}

	assert(base + curr <= (void *) store + sizeof(store));
}

void net_in(struct frame *frame)
{
	unsigned prot;
	void *data;

	if(FRAME_SIZE(frame) >= HARDWARE_HDRSZ) {

		data = FRAME_PAYLOAD(frame);
		prot = NET_READ_SHORT(data + 6 + 6);

		FRAME_STRIP(frame, HARDWARE_HDRSZ);

		switch(prot) {

			case HARDWARE_PROTO_ARP:
				arp_in(frame);
				break;

			case HARDWARE_PROTO_IP:
				ip_in(frame);
		}
	}

	frame_free(frame);
}

/* vi:set ts=3 sw=3 cin path=include,../include: */
