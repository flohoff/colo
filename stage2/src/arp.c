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
uint32_t ip_nsvr;

static struct
{
	enum {
		ARP_UNUSED = 0,
		ARP_UNRESOLVED,
		ARP_RESOLVED,
	} state;

	struct frame	*head;
	struct frame	*tail;
	uint32_t			ip;
	uint16_t			hw[3];

} arp_table[8];

/*
 * add MAC header and transmit frame
 */
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

/*
 * discard frames queued on an ARP query
 */
static void arp_discard(unsigned indx)
{
	struct frame *frame;

	assert(arp_table[indx].state != ARP_UNUSED);

	while((frame = arp_table[indx].head)) {
		arp_table[indx].head = frame->link;
		frame_free(frame);
	}
}

/*
 * transmit frames queued on an ARP query
 */
static void arp_spool(unsigned indx)
{
	struct frame *frame;

	assert(arp_table[indx].state == ARP_RESOLVED);

	while((frame = arp_table[indx].head)) {
		arp_table[indx].head = frame->link;
		arp_out(frame, arp_table[indx].hw, HARDWARE_PROTO_IP);
	}
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

			COPY_HW_ADDR(arp_table[indx].hw, data + 8);

#if 0
			if(arp_table[indx].state == ARP_UNRESOLVED)
				DPRINTF("arp: resolved %s\n", inet_ntoa(ip));
#endif

			arp_table[indx].state = ARP_RESOLVED;

			arp_spool(indx);

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
				assert(!arp_table[indx].head);

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

#if 0
		DPRINTF("arp: request from %s\n", inet_ntoa(ip));
#endif
	}
}

void arp_ip_out(struct frame *frame, uint32_t ip)
{
	unsigned indx, unused;
	struct frame *arpreq;
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

	unused = elements(arp_table);

	for(indx = 0; indx < elements(arp_table); ++indx)
		if(arp_table[indx].state == ARP_UNUSED)
			unused = indx;
		else if(arp_table[indx].ip == ip) {
			if(arp_table[indx].state == ARP_RESOLVED) {
				arp_out(frame, arp_table[indx].hw, HARDWARE_PROTO_IP);
				return;
			}
			break;
		}

	if(indx == elements(arp_table)) {

		indx = unused;
		if(indx == elements(arp_table))
			indx = (MFC0(CP0_COUNT) / 16) % elements(arp_table);

		arp_table[indx].ip = ip;
		arp_table[indx].state = ARP_UNRESOLVED;
		assert(!arp_table[indx].head);
	}

	arpreq = frame_alloc();
	if(arpreq) {

		/* XXX only queue a single frame for now */

		arp_discard(indx);

		frame->link = NULL;
		if(arp_table[indx].head)
			arp_table[indx].tail->link = frame;
		else
			arp_table[indx].head = frame;
		arp_table[indx].tail = frame;

	} else

		arpreq = frame;

	FRAME_INIT(arpreq, HARDWARE_HDRSZ, ARP_PAYLOAD_SIZE);

	request = FRAME_PAYLOAD(arpreq);

	NET_WRITE_SHORT(request + 0, HARDWARE_ADDR_ETHER);
	NET_WRITE_SHORT(request + 2, HARDWARE_PROTO_IP);
	NET_WRITE_BYTE(request + 4, HARDWARE_ADDR_SIZE);
	NET_WRITE_BYTE(request + 5, IP_ADDR_SIZE);
	NET_WRITE_SHORT(request + 6, ARP_OP_REQUEST);

	COPY_HW_ADDR(request + 8, hw_addr);
	NET_WRITE_LONG(request + 14, ip_addr);

	memset(request + 18, 0, HARDWARE_ADDR_SIZE);
	NET_WRITE_LONG(request + 24, ip);

#if 0
	DPRINTF("arp: sent request for %s\n", inet_ntoa(ip));
#endif

	arp_out(arpreq, NULL, HARDWARE_PROTO_ARP);
}

/*
 * clear out all ARP table entries
 */
void arp_flush_all(void)
{
	unsigned indx;

	for(indx = 0; indx < elements(arp_table); ++indx)
		if(arp_table[indx].state != ARP_UNUSED) {
			arp_discard(indx);
			arp_table[indx].state = ARP_UNUSED;
		}
}

/* vi:set ts=3 sw=3 cin path=include,../include: */
