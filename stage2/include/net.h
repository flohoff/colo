/*
 * (C) P.Horton 2004
 *
 * $Id$
 *
 * This code is covered by the GNU General Public License. For details see the file "COPYING".
 */

#ifndef _NET_H_
#define _NET_H_

#define HARDWARE_HDRSZ						14
#define HARDWARE_ADDR_ETHER				1
#define HARDWARE_ADDR_SIZE					6
#define HARDWARE_MIN_FRAME_SZ				64

#define HARDWARE_PROTO_IP					0x0800
#define HARDWARE_PROTO_ARP					0x0806

#define IP_VERSION							4
#define IP_HDRSZ								20
#define IP_ADDR_SIZE							4

#define ICMP_HDRSZ							4

#define UDP_HDRSZ								8

#define INADDR_BROADCAST					0xffffffff

#define IPPROTO_ICMP							1
#define IPPROTO_UDP							17

#define NET_READ_BYTE(p)					((unsigned)*(uint8_t*)(p))
#define NET_READ_SHORT(p)					((NET_READ_BYTE(p)<<8)|NET_READ_BYTE((void*)(p)+1))
#define NET_READ_LONG(p)					((NET_READ_SHORT(p)<<16)|NET_READ_SHORT((void*)(p)+2))

#define NET_WRITE_BYTE(p,v)				do{*(uint8_t*)(p)=(v);}while(0)
#define NET_WRITE_SHORT(p,v)				do{NET_WRITE_BYTE((p),(v)>>8);NET_WRITE_BYTE((void*)(p)+1,(v)&0xff);}while(0)
#define NET_WRITE_LONG(p,v)				do{NET_WRITE_SHORT((p),(v)>>16);NET_WRITE_SHORT((void*)(p)+2,(v)&0xffff);}while(0)

/* net.c */

#define FRAME_PAYLOAD(f)					((void *)(f)->payload+(f)->offset)
#define FRAME_SIZE(f)						((f)->end-(f)->offset)
#define FRAME_STRIP(f,n)					do{(f)->offset+=(n);assert((f)->offset<=(f)->end);}while(0)
#define FRAME_HEADER(f,n)					do{assert((f)->offset>=(n));(f)->offset-=(n);}while(0)
#define FRAME_INIT(f,o,n)					do{(f)->offset=(o);(f)->end=(f)->offset+(n);assert((f)->end<=sizeof((f)->payload));}while(0)
#define FRAME_CLIP(f,n)						do{assert((n)<=(f)->end-(f)->offset);(f)->end=(f)->offset+(n);}while(0)
#define FRAME_BUMP(f)						do{++(f)->refs;}while(0)

struct frame
{
	uint8_t			payload[1520];
	struct frame	*link;
	unsigned			refs;
	unsigned			offset;
	unsigned			end;
	uint32_t			ip_src;
	uint32_t			ip_dst;
	unsigned			udp_src;
};

extern struct frame *frame_alloc(void);
extern void frame_free(struct frame *);
extern void net_in(struct frame *);
extern const char *inet_ntoa(uint32_t);

extern void net_init(void);

/* tulip.c */

extern void tulip_out(struct frame *);
extern void tulip_poll(void);
extern int tulip_init_net(void);

/* arp.c */

extern uint16_t hw_addr[3];
extern uint32_t ip_addr;
extern uint32_t ip_mask;
extern uint32_t ip_gway;

extern void arp_in(struct frame *);
extern void arp_ip_out(struct frame *, uint32_t);
extern void arp_flush_all(void);

/* ip.c */

extern void ip_in(struct frame *);

extern unsigned ip_checksum(unsigned, const void *, unsigned);
extern void ip_out(struct frame *, uint32_t, unsigned);

/* icmp.c */

extern void icmp_in(struct frame *);

/* udp.c */

extern void udp_in(struct frame *);
extern int udp_socket(void);
extern unsigned udp_bind(int, unsigned);
extern unsigned udp_connect(int, uint32_t, unsigned);
extern struct frame *udp_read(int);
extern void udp_sendto(int, struct frame *, uint32_t, unsigned);
extern void udp_send(int, struct frame *);
extern void udp_close_all(void);

#endif

/* vi:set ts=3 sw=3 cin path=include,../include: */
