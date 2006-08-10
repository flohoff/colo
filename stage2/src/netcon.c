/*
 * (C) P.Horton 2004,2005,2006
 *
 * $Id$
 *
 * This code is covered by the GNU General Public License. For details see the file "COPYING".
 */

#include "lib.h"
#include "net.h"

#define MAX_PAYLOAD								512

#define DEFAULT_SRC_PORT						6665
#define DEFAULT_DST_PORT						6666

#define TRANSMIT_POLL_COUNT					20

static struct frame *rx_head;
static struct frame *rx_tail;
static struct frame *tx_head;
static struct frame *tx_tail;
static unsigned rx_part;
static unsigned tx_seen;
static int udp_sock = -1;

/*
 * is net console enabled
 */
int netcon_enabled(void)
{
	return udp_sock >= 0;
}

/*
 * disable net console
 */
void netcon_disable(void)
{
	struct frame *frame;

	if(udp_sock >= 0) {

		udp_close(udp_sock);
		udp_sock = -1;

		while(rx_head) {

			frame = rx_head->link;
			frame_free(rx_head);
			rx_head = frame;
		}

		while(tx_head) {
			
			frame = tx_head->link;
			frame_free(tx_head);
			tx_head = frame;
		}

		DPUTS("netcon: disabled");

		env_remove_tag(VAR_NETCON);
	}
}

/*
 * do net console transmit / receive
 */
int netcon_poll(void)
{
	struct frame *frame;

	if(udp_sock >= 0) {

		while(tx_head) {

			frame = tx_head->link;
			if(!frame && ++tx_seen < TRANSMIT_POLL_COUNT)
				break;

			udp_send(udp_sock, tx_head);

			tx_head = frame;
			tx_seen = 0;
		}

		for(;;) {

			frame = udp_recv(udp_sock);
			if(!frame)
				break;

			if(FRAME_SIZE(frame)) {

				frame->link = NULL;

				if(rx_head)
					rx_tail->link = frame;
				else {
					rx_head = frame;
					rx_part = 0;
				}
				rx_tail = frame;

			} else

				frame_free(frame);
		}
	}

	return rx_head != NULL;
}

/*
 * read net console input
 */
unsigned netcon_read(void *data, unsigned size)
{
	unsigned copy, fmsz, fill;
	struct frame *frame;

	copy = 0;

	if(udp_sock >= 0)

		while(rx_head) {

			frame = rx_head;

			fmsz = FRAME_SIZE(frame);

			fill = fmsz - rx_part;
			if(copy + fill > size)
				fill = size - copy;

			memcpy(data + copy, FRAME_PAYLOAD(frame) + rx_part, fill);

			copy += fill;

			rx_part += fill;
			if(rx_part < fmsz)
				break;

			rx_head = frame->link;
			rx_part = 0;

			frame_free(frame);
		}

	return copy;
}

/*
 * write net console output
 */
unsigned netcon_write(const void *data, unsigned size)
{
	unsigned copy, fill, fmsz;
	struct frame *frame;

	copy = 0;

	if(udp_sock >= 0)

		while(copy < size) {

			fill = 0;

			if(tx_head) {

				fmsz = FRAME_SIZE(tx_tail);

				fill = size - copy;
				if(fmsz + fill > MAX_PAYLOAD)
					fill = MAX_PAYLOAD - fmsz;
			}

			if(fill) {

				memcpy(FRAME_PAYLOAD(tx_tail) + fmsz, data + copy, fill);
				FRAME_GROW(tx_tail, fill);

				copy += fill;

			} else {

				frame = frame_alloc();
				if(!frame)
					break;

				FRAME_INIT(frame, HARDWARE_HDRSZ + IP_HDRSZ + UDP_HDRSZ, 0);

				frame->link = NULL;

				if(tx_head)
					tx_tail->link = frame;
				else
					tx_head = frame;
				tx_tail = frame;
			}
		}

	return copy;
}

/*
 * 'netcon' command
 */
int cmnd_netcon(int opsz)
{
	unsigned long port;
	unsigned src, dst;
	uint32_t host;
	char buf[16];
	char *ptr;

	if(argc < 2) {
		netcon_disable();
		return E_NONE;
	}

	if(argc > 4)
		return E_ARGS_OVER;

	if(!inet_aton(argv[1], &host)) {
		puts("invalid address");
		return E_UNSPEC;
	}

	src = DEFAULT_SRC_PORT;
	dst = DEFAULT_DST_PORT;

	if(argc > 2) {

		port = strtoul(argv[2], &ptr, 10);

		if(*ptr || !port || port > 0xffff) {
			puts("invalid target port");
			return E_UNSPEC;
		}

		dst = port;

		if(argc > 3) {

			port = strtoul(argv[3], &ptr, 10);
			
			if(*ptr || port > 0xffff) {
				puts("invalid source port");
				return E_UNSPEC;
			}

			src = port;
		}
	}

	if(!net_is_up())
		return E_NET_DOWN;

	if(udp_sock >= 0)
		udp_close(udp_sock);

	udp_sock = udp_socket();

	if(udp_sock < 0) {
		puts("no socket available");
		return E_UNSPEC;
	}

	src = udp_bind(udp_sock, src);
	udp_connect(udp_sock, host, dst);

	DPRINTF("netcon: %u --> %s:%u\n", src, inet_ntoa(host), dst);

	env_put("nc-host", inet_ntoa(host), VAR_NETCON);
	sprintf(buf, "%u", dst);
	env_put("nc-port", buf, VAR_NETCON);
	sprintf(buf, "%u", src);
	env_put("nc-source", buf, VAR_NETCON);

	return E_NONE;
}

/* vi:set ts=3 sw=3 cin path=include,../include: */
