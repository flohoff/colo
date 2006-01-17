/*
 * (C) P.Horton 2004,2005,2006
 *
 * $Id$
 *
 * This code is covered by the GNU General Public License. For details see the file "COPYING".
 */

#include "lib.h"
#include "net.h"

unsigned ip_checksum(unsigned sum, const void *data, unsigned size)
{
	unsigned indx;

	assert(!((unsigned long) data & 1));
	assert(size);

	sum = (sum >> 16) + (sum & 0xffff);

	for(indx = 0; indx < size - 1; indx += 2)
		sum += NET_READ_SHORT(data + indx);

	if(indx < size)
		sum += NET_READ_BYTE(data + indx) << 8;

	sum = (sum >> 16) + (sum & 0xffff);
	sum = (sum >> 16) + (sum & 0xffff);

	return sum;
}

void ip_in(struct frame *frame)
{
	unsigned size, hdrsz, totsz;
	uint32_t ip;
	void *data;

	size = FRAME_SIZE(frame);
	if(size < IP_HDRSZ)
		return;

	data = FRAME_PAYLOAD(frame);

	hdrsz = NET_READ_BYTE(data + 0);

	if((hdrsz >> 4) != IP_VERSION || (NET_READ_SHORT(data + 6) & 0x3fff))
		return;

	hdrsz = (hdrsz & 0xf) * 4;
	totsz = NET_READ_SHORT(data + 2);

	if(hdrsz < IP_HDRSZ || hdrsz > totsz || totsz > size)
		return;

	ip = NET_READ_LONG(data + 16);

	if(ip_addr &&
		ip != ip_addr &&
		ip &&
		ip != INADDR_BROADCAST &&
		ip != (ip_addr & ip_mask) &&
		ip != (ip_addr | ~ip_mask)) {

		return;
	}

	if(ip_checksum(0, data, hdrsz) != 0xffff)
		return;

	frame->ip_src = NET_READ_LONG(data + 12);
	frame->ip_dst = ip;

	FRAME_CLIP(frame, totsz);
	FRAME_STRIP(frame, hdrsz);

	switch(NET_READ_BYTE(data + 9)) {

		case IPPROTO_ICMP:
			icmp_in(frame);
			break;

		case IPPROTO_UDP:
			udp_in(frame);
	}
}

void ip_out(struct frame *frame, uint32_t ip, unsigned proto)
{
	unsigned size, cksum;
	void *data;

	FRAME_HEADER(frame, IP_HDRSZ);

	size = FRAME_SIZE(frame);
	data = FRAME_PAYLOAD(frame);

	NET_WRITE_BYTE(data + 0, (IP_VERSION << 4) | (IP_HDRSZ / 4));
	NET_WRITE_BYTE(data + 1, 0);
	NET_WRITE_SHORT(data + 2, size);
	NET_WRITE_SHORT(data + 4, 0);
	NET_WRITE_SHORT(data + 6, 0x4000);
	NET_WRITE_BYTE(data + 8, 128);
	NET_WRITE_BYTE(data + 9, proto);
	NET_WRITE_SHORT(data + 10, 0);
	NET_WRITE_LONG(data + 12, ip_addr);
	NET_WRITE_LONG(data + 16, ip);

	cksum = ip_checksum(0, data, IP_HDRSZ);

	NET_WRITE_SHORT(data + 10, ~cksum);

	arp_ip_out(frame, ip);
}

/* vi:set ts=3 sw=3 cin path=include,../include: */
