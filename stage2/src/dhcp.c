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

#define DHCP_SEND_PACKETS_MAX				10

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

#define OPT_PAD								0
#define OPT_NETMASK							1
#define OPT_ROUTERS							3
#define OPT_REQUESTED_IP					50
#define OPT_MESSAGE_TYPE					53
#define OPT_REQ_PARAM_LIST					55
#define OPT_END								255

static uint32_t dhcp_mask;
static uint32_t dhcp_gway;
static uint32_t dhcp_addr;
static unsigned dhcp_xid;

/*
 * build DHCP request frame (DISCOVER/REQUEST)
 */
static void dhcp_build(struct frame *frame)
{
	unsigned opts;
	void *data;

	data = FRAME_PAYLOAD(frame);

	memset(data, 0, MESSAGE_SIZE_BASE);

	NET_WRITE_BYTE(data + 0x00, BOOTP_BOOTREQUEST);
	NET_WRITE_BYTE(data + 0x01, HARDWARE_ADDR_ETHER);
	NET_WRITE_BYTE(data + 0x02, HARDWARE_ADDR_SIZE);

	NET_WRITE_LONG(data + 0x04, dhcp_xid);

	COPY_HW_ADDR(data + 0x1c, hw_addr);

	opts = MESSAGE_SIZE_BASE;

	NET_WRITE_LONG(data + opts, OPTION_SIG);
	opts += 4;

	NET_WRITE_BYTE(data + opts++, OPT_MESSAGE_TYPE);
	NET_WRITE_BYTE(data + opts++, 1);
	NET_WRITE_BYTE(data + opts++, dhcp_addr ? DHCP_REQUEST : DHCP_DISCOVER);

	NET_WRITE_BYTE(data + opts++, OPT_REQ_PARAM_LIST);
	NET_WRITE_BYTE(data + opts++, 2);
	NET_WRITE_BYTE(data + opts++, OPT_NETMASK);
	NET_WRITE_BYTE(data + opts++, OPT_ROUTERS);

	if(dhcp_addr) {
		NET_WRITE_BYTE(data + opts++, OPT_REQUESTED_IP);
		NET_WRITE_BYTE(data + opts++, 4);
		NET_WRITE_LONG(data + opts, dhcp_addr);
		opts += 4;
	}

	NET_WRITE_BYTE(data + opts, OPT_END);

	FRAME_CLIP(frame, opts);

	DPUTS(dhcp_addr ? "dhcp: REQUEST" : "dhcp: DISCOVER");
}

/*
 * process received DHCP reply (OFFER/ACK)
 */
static int dhcp_receive(int sock, struct frame *frame)
{
	uint32_t gway, mask, addr, peer;
	unsigned opt, siz, msg;
	void *data, *top;

	DPRINTF("dhcp: receive %s\n", inet_ntoa(frame->ip_src));

	data = FRAME_PAYLOAD(frame);
	top = data + FRAME_SIZE(frame);

	if(top - data < MESSAGE_SIZE_BASE + 4 ||
		NET_READ_BYTE(data + 0x00) != BOOTP_BOOTREPLY ||
		NET_READ_BYTE(data + 0x01) != HARDWARE_ADDR_ETHER ||
		NET_READ_BYTE(data + 0x02) != HARDWARE_ADDR_SIZE ||
		NET_READ_LONG(data + 0x04) != dhcp_xid ||
		NET_READ_LONG(data + MESSAGE_SIZE_BASE) != OPTION_SIG) {

		frame_free(frame);
		return -1;
	}

	peer = frame->ip_src;
	addr = NET_READ_LONG(data + 0x10);

	opt = OPT_PAD;
	msg = 0;
	gway = 0;
	mask = 0xffffff00;			/* XXX */

	for(data += MESSAGE_SIZE_BASE + 4; data < top;) {

		opt = NET_READ_BYTE(data++);
		if(opt == OPT_PAD)
			continue;
		if(opt == OPT_END)
			break;

		if(data >= top)
			break;
		siz = NET_READ_BYTE(data++);
		if(data + siz > top)
			break;

		switch(opt) {

			case OPT_NETMASK:
				if(siz == 4)
					mask = NET_READ_LONG(data);
				break;

			case OPT_ROUTERS:
				if(siz >= 4 && !(siz % 4))
					gway = NET_READ_LONG(data);
				break;

			case OPT_MESSAGE_TYPE:
				if(siz == 1)
					msg = NET_READ_BYTE(data);
		}

#ifdef _DEBUG
		{
			unsigned idx;

			DPRINTF("dhcp: option %02x)", opt);

			for(idx = 0; idx < siz; ++idx)
				DPRINTF(" %02x", NET_READ_BYTE(data + idx));
			
			DPUTCHAR('\n');
		}
#endif

		data += siz;
	}

	frame_free(frame);
	
	if(!addr || opt != OPT_END)
		return -1;

	switch(msg) {

		case DHCP_OFFER:
			if(dhcp_addr)
				return -1;
			dhcp_addr = addr;

			udp_connect(sock, peer, DHCP_PORT_SERVER);
			
			DPRINTF("dhcp: OFFER %s <-- ", inet_ntoa(dhcp_addr));
			DPUTS(inet_ntoa(peer));
			return 0;

		case DHCP_ACK:
			if(dhcp_addr == addr) {
				dhcp_mask = mask;
				dhcp_gway = gway;

				DPUTS("dhcp: ACK");
				return 1;
			}
	}

	return -1;
}

/*
 * apply configuration from DHCP
 */
static int dhcp_config(void)
{
	ip_addr = dhcp_addr;
	ip_mask = dhcp_mask;
	ip_gway = dhcp_gway;

	if(!net_up()) {
		puts("no interface");
		return 0;
	}

	DPRINTF("dhcp: address %s\n", inet_ntoa(ip_addr));
	DPRINTF("      netmask %s\n", inet_ntoa(ip_mask));
	DPRINTF("      gateway %s\n", inet_ntoa(ip_gway));

	return 1;
}

/*
 * get network configuration from DHCP server
 */
int dhcp(void)
{
	unsigned mark, retries;
	struct frame *frame;
	int sock, stat;

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

	dhcp_xid = MFC0(CP0_COUNT);
	dhcp_addr = 0;
	retries = 0;

	for(;;) {

		frame = frame_alloc();
		if(frame) {
			FRAME_INIT(frame, HARDWARE_HDRSZ + IP_HDRSZ + UDP_HDRSZ, 1024);
			dhcp_build(frame);
			if(dhcp_addr)
				udp_send(sock, frame);
			else
				udp_sendto(sock, frame, INADDR_BROADCAST, DHCP_PORT_SERVER);
		}

		for(mark = MFC0(CP0_COUNT);;) {

			if(kbhit()) {

				udp_close(sock);
				net_down();

				getch();
				puts("aborted");
				return 0;
			}

			if(MFC0(CP0_COUNT) - mark >= CP0_COUNT_RATE * 3) {

				if(++retries == DHCP_SEND_PACKETS_MAX) {

					udp_close(sock);
					net_down();

					puts("no response");
					return 0;
				}

				/* back to DISCOVER state */

				dhcp_addr = 0;
				udp_connect(sock, 0, 0);

				break;
			}

			frame = udp_recv(sock);
			if(frame) {
				
				stat = dhcp_receive(sock, frame);
				if(stat >= 0) {
					if(stat > 0) {

						udp_close(sock);
						net_down();

						return dhcp_config();
					}

					break;
				}
			}
		}
	}
}

/* vi:set ts=3 sw=3 cin path=include,../include: */
