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

#define DHCP_PORT_SERVER					67
#define DHCP_PORT_CLIENT					68

#define OPTION_SIG							0x63825363

#define BOOTP_BOOTREQUEST					1
#define BOOTP_BOOTREPLY						2

#define MESSAGE_SIZE_BASE					0xec

#define DHCP_DISCOVER						1
#define DHCP_OFFER							2
#define DHCP_REQUEST							3
#define DHCP_ACK								5

static unsigned xid;
static uint32_t addr;
static int done;

static void dhcp_build(struct frame *frame, int type)
{
	unsigned opts;
	void *data;

	data = FRAME_PAYLOAD(frame);

	memset(data, 0, MESSAGE_SIZE_BASE);

	NET_WRITE_BYTE(data + 0x00, BOOTP_BOOTREQUEST);
	NET_WRITE_BYTE(data + 0x01, HARDWARE_ADDR_ETHER);
	NET_WRITE_BYTE(data + 0x02, HARDWARE_ADDR_SIZE);

	NET_WRITE_LONG(data + 0x04, xid);

	COPY_HW_ADDR(data + 0x1c, hw_addr);

	NET_WRITE_LONG(data + MESSAGE_SIZE_BASE, OPTION_SIG);

	opts = MESSAGE_SIZE_BASE + 4;

	NET_WRITE_BYTE(data + opts++, 53);
	NET_WRITE_BYTE(data + opts++, 1);
	NET_WRITE_BYTE(data + opts++, type);

	NET_WRITE_BYTE(data + opts++, 55);
	NET_WRITE_BYTE(data + opts++, 2);
	NET_WRITE_BYTE(data + opts++, 1);		// netmask (RFC 2132)
	NET_WRITE_BYTE(data + opts++, 3);		// router

	switch(type) {

		case DHCP_DISCOVER:
			DPUTS("dhcp: DISCOVER");
			break;

		case DHCP_REQUEST:
			DPUTS("dhcp: REQUEST");

			NET_WRITE_BYTE(data + opts++, 50);		// requested IP
			NET_WRITE_BYTE(data + opts++, 4);
			NET_WRITE_LONG(data + opts, addr);
			opts += 4;
	}

	NET_WRITE_BYTE(data + opts, 255);

	FRAME_CLIP(frame, opts);
}

static void dhcp_receive(struct frame *frame)
{
	unsigned opt, siz, msg;
	void *data, *top;

	data = FRAME_PAYLOAD(frame);
	top = data + FRAME_SIZE(frame);

	if(top - data < MESSAGE_SIZE_BASE + 4 ||
		NET_READ_BYTE(data + 0x00) != BOOTP_BOOTREPLY ||
		NET_READ_BYTE(data + 0x01) != HARDWARE_ADDR_ETHER ||
		NET_READ_BYTE(data + 0x02) != HARDWARE_ADDR_SIZE ||
		NET_READ_LONG(data + 0x04) != xid ||
		NET_READ_LONG(data + MESSAGE_SIZE_BASE) != OPTION_SIG ||
		(addr && NET_READ_LONG(data + 0x10) != addr)) {

		frame_free(frame);

		DPUTS("dhcp: invalid response");

		return;
	}

	opt = 0;
	msg = 0;

	for(data += MESSAGE_SIZE_BASE + 4; data < top;) {

		opt = NET_READ_BYTE(data++);
		if(opt == 0)
			continue;
		if(opt == 255)
			break;

		if(data >= top)
			break;
		siz = NET_READ_BYTE(data++);
		if(data + siz > top)
			break;

		{
			unsigned idx;

			DPRINTF("dhcp: option %02x %u -->", opt, siz);

			for(idx = 0; idx < siz; ++idx)
				DPRINTF(" %02x", NET_READ_BYTE(data + idx));
			
			DPUTCHAR('\n');
		}

		if(opt == 53 && siz == 1)
			msg = NET_READ_BYTE(data);

		data += siz;
	}


	if(opt == 255) {

		switch(msg) {

			case DHCP_OFFER:
				data = FRAME_PAYLOAD(frame);
				addr = NET_READ_LONG(data + 0x10);

				DPRINTF("dhcp: OFFER %s\n", inet_ntoa(addr));

				break;

			case DHCP_ACK:

				DPUTS("dhcp: ACK");

				done = 1;
		}

	} else

		DPUTS("dhcp: invalid options");

	frame_free(frame);
}

int dhcp(void)
{
	struct frame *frame;
	unsigned mark;
	int sock;

	net_down();

	ip_addr = 0;
	ip_mask = 0;
	ip_gway = 0;

	if(!net_up()) {
		puts("no interface");
		return 0;
	}

	sock = udp_socket();
	if(sock < 0) {
		puts("no socket");
		return 0;
	}

	udp_bind(sock, DHCP_PORT_CLIENT);

	xid = MFC0(CP0_COUNT);
	addr = 0;
	done = 0;

	for(mark = MFC0(CP0_COUNT); !kbhit() && !done;) {

		frame = udp_read(sock);
		if(frame)
			dhcp_receive(frame);

		if(MFC0(CP0_COUNT) - mark < CP0_COUNT_RATE * 2)
			continue;
		mark += CP0_COUNT_RATE * 2;
		
		frame = frame_alloc();
		if(!frame)
			continue;

		FRAME_INIT(frame, HARDWARE_HDRSZ + IP_HDRSZ + UDP_HDRSZ, 576);

		dhcp_build(frame, addr ? DHCP_REQUEST : DHCP_DISCOVER);

		udp_sendto(sock, frame, INADDR_BROADCAST, DHCP_PORT_SERVER);
	}

	udp_close(sock);

	if(done) {

		net_down();

		ip_addr = addr;
		ip_mask = 0xffffff00;
		ip_gway = 0xc0a80101;

		DPRINTF("net: address %s\n", inet_ntoa(ip_addr));
		DPRINTF("     netmask %s\n", inet_ntoa(ip_mask));
		DPRINTF("     gateway %s\n", inet_ntoa(ip_gway));

		if(!net_up()) {
			puts("no interface");
			return 0;
		}

	} else

		getch();

	return 1;
}

/* vi:set ts=3 sw=3 cin path=include,../include: */
