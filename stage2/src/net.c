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

#define FRAME_PADDED						((sizeof(struct frame)+DCACHE_LINE_SIZE-1)&~(DCACHE_LINE_SIZE-1))

int net_alive;

static struct frame *pool;

struct frame *frame_alloc(void)
{
	struct frame *frame;

	if(!pool)
		arp_pressure();

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
	static uint8_t store[(DCACHE_LINE_SIZE - 1) + BUFFER_COUNT * FRAME_PADDED];

	unsigned curr, indx;
	struct frame *prev;
	void *base;

	curr = (unsigned long) store & (DCACHE_LINE_SIZE - 1);
	base = store - curr;
	prev = NULL;

	for(indx = 0; indx < BUFFER_COUNT; ++indx) {

		curr += -curr & (DCACHE_LINE_SIZE - 1);

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

	if(net_alive) {

		DPUTS("net: interface up");

		env_put("ip-address", inet_ntoa(ip_addr), VAR_NET);
		if(ip_addr)
			DPRINTF("  address     %s\n", inet_ntoa(ip_addr));

		env_put("ip-netmask", inet_ntoa(ip_mask), VAR_NET);
		if(ip_mask)
			DPRINTF("  netmask     %s\n", inet_ntoa(ip_mask));

		env_put("ip-gateway", inet_ntoa(ip_gway), VAR_NET);
		if(ip_gway)
			DPRINTF("  gateway     %s\n", inet_ntoa(ip_gway));

		env_put("ip-name-server", inet_ntoa(ip_nsvr), VAR_NET);
		if(ip_nsvr)
			DPRINTF("  name server %s\n", inet_ntoa(ip_nsvr));
	}

	return net_alive;
}

/*
 * shutdown network interface
 */
void net_down(int dhcp)
{
	if(!net_alive)
		return;

	netcon_disable();

	tulip_down();
	net_alive = 0;

	env_remove_tag(VAR_NET);
	if(!dhcp)
		env_remove_tag(VAR_DHCP);

	ip_addr = 0;
	ip_mask = 0;
	ip_gway = 0;
	ip_nsvr = 0;

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

		net_down(0);

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

	net_down(0);

	ip_addr = addr;
	ip_mask = mask;
	ip_gway = gway;

	if(!net_up())
		return E_NET_DOWN;

	return E_NONE;
}

/* vi:set ts=3 sw=3 cin path=include,../include: */
