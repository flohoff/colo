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

// XXX expire old ARP entries

#define ARP_PAYLOAD_SIZE				28
#define ARP_OP_REQUEST					1
#define ARP_OP_REPLY						2

uint32_t	ip_addr;
uint32_t ip_mask;
uint32_t ip_gway;

static struct
{
	uint32_t ip;
	uint16_t hw[3];
	enum {
		ARP_UNUSED = 0,
		ARP_UNRESOLVED,
		ARP_RESOLVED,
	} state;

} arp_table[8];

void arp_out(struct frame *frame, const void *dest, unsigned prot)
{
	const uint16_t bcast[] = { 0xffff, 0xffff, 0xffff };
	void *data;

	if(!dest)
		dest = bcast;

	FRAME_HEADER(frame, HARDWARE_HDRSZ);

	data = FRAME_PAYLOAD(frame);

	COPY_HW_ADDR(data + 0, dest);
	COPY_HW_ADDR(data + 6, hw_addr);
	NET_WRITE_SHORT(data + 12, prot);

	tulip_out(frame);
}

void arp_in(struct frame *frame)
{
	void *data, *reply;
	unsigned indx;
	uint32_t ip;

	if(FRAME_SIZE(frame) < ARP_PAYLOAD_SIZE)
		return;

	data = FRAME_PAYLOAD(frame);

	if(NET_READ_SHORT(data + 0) != HARDWARE_ADDR_ETHER ||
		NET_READ_SHORT(data + 2) != HARDWARE_PROTO_IP ||
		NET_READ_BYTE(data + 4) != HARDWARE_ADDR_SIZE ||
		NET_READ_BYTE(data + 5) != IP_ADDR_SIZE) {

		return;
	}

	ip = NET_READ_LONG(data + 14);

	for(indx = 0; indx < elements(arp_table); ++indx)
		if(arp_table[indx].state != ARP_UNUSED && arp_table[indx].ip == ip) {

			if(arp_table[indx].state == ARP_UNRESOLVED)
				DPRINTF("arp: resolved %s\n", inet_ntoa(ip));

			COPY_HW_ADDR(arp_table[indx].hw, data + 8);
			arp_table[indx].state = ARP_RESOLVED;

			goto matched;
		}

#if 0
	if(!((ip ^ ip_addr) & ip_mask))
		for(indx = 0; indx < elements(arp_table); ++indx)
			if(arp_table[indx].state == ARP_UNUSED) {

				DPRINTF("arp: pre-resolved %s\n", inet_ntoa(ip));

				arp_table[indx].ip = ip;
				COPY_HW_ADDR(arp_table[indx].hw, data + 8);
				arp_table[indx].state = ARP_RESOLVED;

				break;
			}
#endif

matched:

	if(NET_READ_SHORT(data + 6) == ARP_OP_REQUEST && NET_READ_LONG(data + 24) == ip_addr) {

		frame = frame_alloc();
		if(!frame)
			return;

		FRAME_INIT(frame, HARDWARE_HDRSZ, ARP_PAYLOAD_SIZE);

		reply = FRAME_PAYLOAD(frame);

		NET_WRITE_SHORT(reply + 0, HARDWARE_ADDR_ETHER);
		NET_WRITE_SHORT(reply + 2, HARDWARE_PROTO_IP);
		NET_WRITE_BYTE(reply + 4, HARDWARE_ADDR_SIZE);
		NET_WRITE_BYTE(reply + 5, IP_ADDR_SIZE);
		NET_WRITE_SHORT(reply + 6, ARP_OP_REPLY);

		COPY_HW_ADDR(reply + 8, hw_addr);
		NET_WRITE_LONG(reply + 14, ip_addr);

		COPY_HW_ADDR(reply + 18, data + 8);
		NET_WRITE_LONG(reply + 24, ip);

		arp_out(frame, data + 8, HARDWARE_PROTO_ARP);

		DPRINTF("arp: request from %s\n", inet_ntoa(ip));
	}
}

static int arp_out_resolved(struct frame *frame, uint32_t ip)
{
	unsigned indx;

	for(indx = 0; indx < elements(arp_table); ++indx)
		if(arp_table[indx].state == ARP_RESOLVED && arp_table[indx].ip == ip) {
			arp_out(frame, arp_table[indx].hw, HARDWARE_PROTO_IP);
			return 1;
		}
	
	return 0;
}

// XXX ARP lookup should be done in the background

void arp_ip_out(struct frame *frame, uint32_t ip)
{
	static struct frame *head, *tail;

	unsigned indx, mark, count;
	struct frame *next;
	void *request;

	if(!ip ||
		ip == INADDR_BROADCAST ||
		ip == (ip_addr & ip_mask) ||
		ip == (ip_addr | ~ip_mask)) {

		arp_out(frame, NULL, HARDWARE_PROTO_IP);
		return;
	}

	if(ip_gway && ((ip ^ ip_addr) & ip_mask))
		ip = ip_gway;

	if(arp_out_resolved(frame, ip))
		return;

	frame->ip_dst = ip;

	frame->link = NULL;

	if(head) {
		tail->link = frame;
		tail = frame;
		return;
	}

	/* not recursive here */

	tail = frame;

	DPRINTF("arp: resolving %s\n", inet_ntoa(ip));

	for(head = frame; head; head = next) {

		ip = head->ip_dst;

		next = head->link;
		if(arp_out_resolved(head, ip))
			continue;

		for(indx = 0;; ++indx) {

			if(indx == elements(arp_table)) {
				indx = (MFC0(CP0_COUNT) / 16) % elements(arp_table);
				break;
			}

			if(arp_table[indx].state == ARP_UNUSED)
				break;
		}

		arp_table[indx].ip = ip;
		arp_table[indx].state = ARP_UNRESOLVED;

		for(count = 0; arp_table[indx].state != ARP_RESOLVED && count < 10; ++count) {

			frame = frame_alloc();

			if(frame) {

				FRAME_INIT(frame, HARDWARE_HDRSZ, ARP_PAYLOAD_SIZE);

				request = FRAME_PAYLOAD(frame);

				NET_WRITE_SHORT(request + 0, HARDWARE_ADDR_ETHER);
				NET_WRITE_SHORT(request + 2, HARDWARE_PROTO_IP);
				NET_WRITE_BYTE(request + 4, HARDWARE_ADDR_SIZE);
				NET_WRITE_BYTE(request + 5, IP_ADDR_SIZE);
				NET_WRITE_SHORT(request + 6, ARP_OP_REQUEST);

				COPY_HW_ADDR(request + 8, hw_addr);
				NET_WRITE_LONG(request + 14, ip_addr);

				memset(request + 18, 0, HARDWARE_ADDR_SIZE);
				NET_WRITE_LONG(request + 24, ip);

				DPRINTF("arp: sent request for %s\n", inet_ntoa(ip));

				arp_out(frame, NULL, HARDWARE_PROTO_ARP);
			}

			for(mark = MFC0(CP0_COUNT); arp_table[indx].state != ARP_RESOLVED && MFC0(CP0_COUNT) - mark < CP0_COUNT_RATE;)
				yield();
		}

		next = head->link;

		if(arp_table[indx].state == ARP_RESOLVED)

			arp_out(head, arp_table[indx].hw, HARDWARE_PROTO_IP);

		else {

			arp_table[indx].state = ARP_UNUSED;

			frame_free(head);

			DPRINTF("arp: failed resolving %s\n", inet_ntoa(ip));
		}
	}
}

void arp_flush_all(void)
{
	unsigned indx;

	for(indx = 0; indx < elements(arp_table); ++indx)
		arp_table[indx].state = ARP_UNUSED;
}

/* vi:set ts=3 sw=3 cin path=include,../include: */
