/*
 * (C) P.Horton 2004
 *
 * $Id$
 *
 * This code is covered by the GNU General Public License. For details see the file "COPYING".
 */

#include "lib.h"
#include "net.h"
#include "cpu.h"

#define BUFFER_COUNT						16

int net_alive;

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

static void net_init(void)
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

/*
 * bring up network interface
 */
int net_up(void)
{
	if(net_alive)
		return 1;

	net_init();
	arp_flush_all();
	udp_close_all();
	net_alive = tulip_up();

	if(net_alive)
		DPUTS("net: interface up");

	return net_alive;
}

/*
 * shutdown network interface
 */
void net_down(void)
{
	if(!net_alive)
		return;

	net_alive = 0;
	tulip_down();

	DPUTS("net: interface down");
}

int cmnd_net(int opsz)
{
	unsigned addr, mask, gway, argn;
	char *ptr, *end;

	if(argc < 2)
		return dhcp() ? E_NONE : E_UNSPEC;

	if(!strcasecmp(argv[1], "down")) {

		if(argc > 2)
			return E_ARGS_OVER;

		net_down();

		return E_NONE;
	}

	/* parse addresses from arguments */

	ptr = strchr(argv[1], '/');
	if(ptr) {

		*ptr++ = '\0';

		mask = strtoul(ptr, &end, 10);
		if(*end || ptr == end || mask > 32) {
			puts("invalid subnet specification");
			return E_UNSPEC;
		}

		if(mask)
			mask = -1 << (32 - mask);

		argn = 2;

	} else {

		if(argc < 3)
			return E_ARGS_UNDER;

		if(!inet_aton(argv[2], &mask)) {
			puts("invalid subnet mask");
			return E_UNSPEC;
		}

		argn = 3;
	}

	switch(argc - argn) {

		case 0:
			gway = 0;
			break;

		case 1:
			if(!inet_aton(argv[argn], &gway)) {
				puts("invalid gateway address");
				return E_UNSPEC;
			}
			break;

		default:
			return E_ARGS_OVER;
	}

	if(!inet_aton(argv[1], &addr)) {
		puts("invalid IP address");
		return E_UNSPEC;
	}

	net_down();

	ip_addr = addr;
	ip_mask = mask;
	ip_gway = gway;

	if(!net_up()) {
		puts("no interface");
		return E_UNSPEC;
	}

	DPRINTF("net: address %s\n", inet_ntoa(ip_addr));
	DPRINTF("     netmask %s\n", inet_ntoa(ip_mask));
	DPRINTF("     gateway %s\n", inet_ntoa(ip_gway));

	return E_NONE;
}

/* vi:set ts=3 sw=3 cin path=include,../include: */
