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
#define OPT_NAMESERVERS						6
#define OPT_ROOT_PATH						17
#define OPT_REQUESTED_IP					50
#define OPT_MESSAGE_TYPE					53
#define OPT_SERVER_IDENTIFIER				54
#define OPT_REQ_PARAM_LIST					55
#define OPT_END								255

static unsigned dhcp_xid;
static unsigned dhcp_sid;

static uint32_t dhcp_mask;
static uint32_t dhcp_gway;
static uint32_t dhcp_addr;
static uint32_t dhcp_nsvr;

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

	if(dhcp_addr) {

		NET_WRITE_BYTE(data + opts++, DHCP_REQUEST);

		if(dhcp_sid) {
			NET_WRITE_BYTE(data + opts++, OPT_SERVER_IDENTIFIER);
			NET_WRITE_BYTE(data + opts++, 4);
			NET_WRITE_LONG(data + opts, dhcp_sid);
			opts += 4;
		}

		NET_WRITE_BYTE(data + opts++, OPT_REQUESTED_IP);
		NET_WRITE_BYTE(data + opts++, 4);
		NET_WRITE_LONG(data + opts, dhcp_addr);
		opts += 4;

		NET_WRITE_BYTE(data + opts++, OPT_REQ_PARAM_LIST);
		NET_WRITE_BYTE(data + opts++, 4);
		NET_WRITE_BYTE(data + opts++, OPT_NETMASK);
		NET_WRITE_BYTE(data + opts++, OPT_ROUTERS);
		NET_WRITE_BYTE(data + opts++, OPT_NAMESERVERS);
		NET_WRITE_BYTE(data + opts++, OPT_ROOT_PATH);

	} else

		NET_WRITE_BYTE(data + opts++, DHCP_DISCOVER);

	NET_WRITE_BYTE(data + opts++, OPT_END);

	FRAME_CLIP(frame, opts);

	DPUTS(dhcp_addr ? "dhcp: REQUEST" : "dhcp: DISCOVER");
}

/*
 * guess netmask from address
 */
static uint32_t mask_default(uint32_t ip)
{
	ip >>= 24;

	if(ip < 128)
		return 0xff000000;

	if(ip < 192)
		return 0xffff0000;

	return 0xffffff00;
}

/*
 * process received DHCP reply (OFFER/ACK)
 *
 * TODO clean up option handling
 */
static int dhcp_receive(int sock, struct frame *frame)
{
	unsigned opt, siz, msg;
	void *data, *ptr, *top;

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

	opt = OPT_PAD;
	msg = 0;

	for(ptr = data + MESSAGE_SIZE_BASE + 4; ptr < top;) {

		opt = NET_READ_BYTE(ptr++);
		if(opt == OPT_PAD)
			continue;
		if(opt == OPT_END)
			break;

		if(ptr >= top)
			break;
		siz = NET_READ_BYTE(ptr++);
		if(ptr + siz > top)
			break;

		if(opt == OPT_MESSAGE_TYPE && siz == 1)
			msg = NET_READ_BYTE(ptr);

		ptr += siz;
	}

	if(opt == OPT_END)

		switch(msg) {

			case DHCP_OFFER:

				if(!dhcp_addr) {

					dhcp_addr = NET_READ_LONG(data + 0x10);
					if(dhcp_addr) {

						for(ptr = data + MESSAGE_SIZE_BASE + 4; ptr < top;) {

							opt = NET_READ_BYTE(ptr++);
							if(opt == OPT_PAD)
								continue;
							if(opt == OPT_END)
								break;

							siz = NET_READ_BYTE(ptr++);

							if(opt == OPT_SERVER_IDENTIFIER && siz == 4)
								dhcp_sid = NET_READ_LONG(ptr);

							ptr += siz;
						}

						DPRINTF("dhcp: OFFER %s <-- ", inet_ntoa(dhcp_addr));
						DPRINTF("%s", inet_ntoa(frame->ip_src));
						if(dhcp_sid != frame->ip_src)
							DPRINTF(" (%s)", inet_ntoa(dhcp_sid));
						DPUTCHAR('\n');

						frame_free(frame);

						return 0;
					}
				}

				break;

			case DHCP_ACK:

				if(dhcp_addr && dhcp_addr == NET_READ_LONG(data + 0x10)) {

					dhcp_mask = mask_default(dhcp_addr);

					for(ptr = data + MESSAGE_SIZE_BASE + 4; ptr < top;) {

						opt = NET_READ_BYTE(ptr++);
						if(opt == OPT_PAD)
							continue;
						if(opt == OPT_END)
							break;

						siz = NET_READ_BYTE(ptr++);

						switch(opt) {

							case OPT_NETMASK:
								if(siz == 4)
									dhcp_mask = NET_READ_LONG(ptr);
								break;

							case OPT_ROUTERS:
								if(siz >= 4 && !(siz % 4))
									dhcp_gway = NET_READ_LONG(ptr);
								break;

							case OPT_NAMESERVERS:
								if(siz >= 4 && !(siz % 4))
									dhcp_nsvr = NET_READ_LONG(ptr);
								break;

							case OPT_ROOT_PATH:
								opt = ((uint8_t *) ptr)[siz];
								((uint8_t *) ptr)[siz] = '\0';
								env_put("dhcp-root-path", ptr, VAR_DHCP);
								((uint8_t *) ptr)[siz] = opt;
						}

						ptr += siz;
					}

					env_put("dhcp-next-server", inet_ntoa(NET_READ_LONG(data + 0x14)), VAR_DHCP);

					((uint8_t *) data)[0x6c + 127] = '\0';
					env_put("dhcp-boot-file", data + 0x6c, VAR_DHCP);

					frame_free(frame);

					DPUTS("dhcp: ACK");

					return 1;
				}
		}

	frame_free(frame);

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
	ip_nsvr = dhcp_nsvr;

	if(!net_up()) {
		puts("no interface");
		return 0;
	}

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

	net_down(0);

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
	dhcp_sid = 0;
	dhcp_addr = 0;
	retries = 0;

	for(;;) {

		frame = frame_alloc();
		if(frame) {
			FRAME_INIT(frame, HARDWARE_HDRSZ + IP_HDRSZ + UDP_HDRSZ, 1024);
			dhcp_build(frame);
			udp_sendto(sock, frame, INADDR_BROADCAST, DHCP_PORT_SERVER);
		}

		for(mark = MFC0(CP0_COUNT);;) {

			if(BREAK()) {

				udp_close(sock);
				net_down(0);

				puts("aborted");
				return 0;
			}

			if(MFC0(CP0_COUNT) - mark >= CP0_COUNT_RATE * 2) {

				if(++retries == DHCP_SEND_PACKETS_MAX) {

					udp_close(sock);
					net_down(0);

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
						net_down(1);

						if(!dhcp_config()) {
							env_remove_tag(VAR_DHCP);
							return 0;
						}

						return 1;
					}

					break;
				}
			}
		}
	}
}

/* vi:set ts=3 sw=3 cin path=include,../include: */
