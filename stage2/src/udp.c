/*
 * (C) P.Horton 2004
 *
 * $Id$
 *
 * This code is covered by the GNU General Public License. For details see the file "COPYING".
 */

#include "lib.h"
#include "net.h"

static struct
{
	uint32_t			peer_ip;
	unsigned			peer_port;
	unsigned			port;
	int				inuse;
	struct frame	*head;
	struct frame	*tail;

} socks[8];

void udp_in(struct frame *frame)
{
	unsigned size, port, indx, cksum;
	void *data;

	data = FRAME_PAYLOAD(frame);

	size = NET_READ_SHORT(data + 4);

	if(size < UDP_HDRSZ || size > FRAME_SIZE(frame))
		return;

	port = NET_READ_SHORT(data + 2);
	if(!port)
		return;

	frame->udp_src = NET_READ_SHORT(data + 0);

	for(indx = 0;; ++indx) {

		if(indx == elements(socks)) {
			DPRINTF("udp: no matching socket %s:%u --> ", inet_ntoa(frame->ip_src), frame->udp_src);
			DPRINTF("%s:%u\n", inet_ntoa(frame->ip_dst), port);
			return;
		}

		if(socks[indx].inuse && socks[indx].port == port && (!socks[indx].peer_port ||
			(socks[indx].peer_port == frame->udp_src && socks[indx].peer_ip == frame->ip_src))) {

			break;
		}
	}

	if(NET_READ_SHORT(data + 6)) {

		cksum = frame->ip_src >> 16;
		cksum += frame->ip_src & 0xffff;
		cksum += frame->ip_dst >> 16;
		cksum += frame->ip_dst & 0xffff;
		cksum += IPPROTO_UDP;
		cksum += size;

		if(ip_checksum(cksum, data, size) != 0xffff)
			return;
	}

	FRAME_CLIP(frame, size);
	FRAME_STRIP(frame, UDP_HDRSZ);
	FRAME_BUMP(frame);

	frame->link = NULL;
	if(socks[indx].head)
		socks[indx].tail->link = frame;
	else
		socks[indx].head = frame;
	socks[indx].tail = frame;
}

int udp_socket(void)
{
	unsigned indx;

	for(indx = 0; indx < elements(socks); ++indx)
		if(!socks[indx].inuse) {
			socks[indx].peer_ip = 0;
			socks[indx].peer_port = 0;
			socks[indx].port = 0;
			socks[indx].inuse = 1;
			assert(!socks[indx].head);
			return indx;
		}

	return -1;
}

void udp_close(int s)
{
	static struct frame *frame;

	while(socks[s].head) {
		frame = socks[s].head;
		socks[s].head = frame->link;
		frame_free(frame);
	}

	socks[s].inuse = 0;
}

static unsigned alloc_port(void)
{
	static unsigned next;
	unsigned indx;

	for(;;) {

		if(++next < 1024)
			next = 1024;

		for(indx = 0; !socks[indx].inuse || socks[indx].port != next; ++indx)
			if(indx == elements(socks))
				return next;
	}
}

unsigned udp_bind(int s, unsigned port)
{
	assert(!socks[s].port);

	socks[s].port = port ? port : alloc_port();

	return socks[s].port;
}

unsigned udp_connect(int s, uint32_t ip, unsigned port)
{
	if(!socks[s].port)
		socks[s].port = alloc_port();

	socks[s].peer_ip = ip;
	socks[s].peer_port = port;

	return socks[s].port;
}

struct frame *udp_read(int s)
{
	struct frame *frame;

	for(;;) {

		frame = socks[s].head;
		if(!frame)
			break;
		socks[s].head = frame->link;

		if(!socks[s].peer_port || (socks[s].peer_port == frame->udp_src && socks[s].peer_ip == frame->ip_src))
			break;

		frame_free(frame);
	}

	return frame;
}

void udp_sendto(int s, struct frame *frame, uint32_t ip, unsigned port)
{
	unsigned size, cksum;
	void *data;

	assert(socks[s].port);

	FRAME_HEADER(frame, UDP_HDRSZ);

	size = FRAME_SIZE(frame);
	data = FRAME_PAYLOAD(frame);

	NET_WRITE_SHORT(data + 0, socks[s].port);
	NET_WRITE_SHORT(data + 2, port);
	NET_WRITE_SHORT(data + 4, size);
	NET_WRITE_SHORT(data + 6, 0);

	cksum = ip_addr >> 16;
	cksum += ip_addr & 0xffff;
	cksum += ip >> 16;
	cksum += ip & 0xffff;
	cksum += IPPROTO_UDP;
	cksum += size;

	cksum = ip_checksum(cksum, data, size);

	if(cksum != 0xffff)
		cksum = ~cksum;

	NET_WRITE_SHORT(data + 6, cksum);
	
	ip_out(frame, ip, IPPROTO_UDP);
}

void udp_send(int s, struct frame *frame)
{
	udp_sendto(s, frame, socks[s].peer_ip, socks[s].peer_port);
}

void udp_close_all(void)
{
	unsigned indx;

	for(indx = 0; indx < elements(socks); ++indx)
		if(socks[indx].inuse)
			udp_close(indx);
}

/* vi:set ts=3 sw=3 cin path=include,../include: */
