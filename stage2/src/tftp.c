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

#define TFTP_SEND_PACKETS_MAX		10

#define TRANSFER_MODE				"octet"

#define TFTP_PORT_SERVER			69

#define OPCODE_RRQ					1
#define OPCODE_DATA					3
#define OPCODE_ACK					4
#define OPCODE_ERROR					5

static void tftp_error(const void *data, unsigned size)
{
	putstring("server reported error");
	if(size >= 4) {
		printf(" #%u", NET_READ_SHORT(data + 2));
		if(size > 4) {
			putstring(" \"");
			putstring_safe(data + 4, size - 4);
			putchar('"');
		}
	}
	putchar('\n');
}

static size_t tftp_transfer(int sock, void *mem, size_t max, struct frame *frame)
{
	unsigned size, block, mark, diff;
	void *data, *ptr;

	data = FRAME_PAYLOAD(frame);
	size = FRAME_SIZE(frame);

	ptr = mem;

	for(block = 1;;) {

		if(size >= 4) {

			data += 4;
			size -= 4;

			if(size > max) {
				frame_free(frame);
				puts("too big");
				return -1;
			}

			memcpy(ptr, data, size);
			ptr += size;
			max -= size;

			/* have we done ? */

			size = (size < 512);
		}
		
		/* reuse received frame */

		FRAME_INIT(frame, HARDWARE_HDRSZ + IP_HDRSZ + UDP_HDRSZ, 4);
		data = FRAME_PAYLOAD(frame);
		NET_WRITE_SHORT(data + 0, OPCODE_ACK);
		NET_WRITE_SHORT(data + 2, block);

		udp_send(sock, frame);

		if(size)
			return ptr - mem;

		mark = MFC0(CP0_COUNT);
		do {

			if(kbhit() && getch() == ' ') {
				puts("aborted");
				return -1;
			}

			if(MFC0(CP0_COUNT) - mark >= CP0_COUNT_RATE * 10) {
				puts("no response");
				return -1;
			}

			frame = udp_recv(sock);
			if(!frame)
				continue;

			data = FRAME_PAYLOAD(frame);
			size = FRAME_SIZE(frame);

			if(size < 2 || size > 512 + 4) {
				frame_free(frame);
				continue;
			}

			switch(NET_READ_SHORT(data + 0)) {

				case OPCODE_ERROR:
					tftp_error(data, size);
					frame_free(frame);
					return -1;

				case OPCODE_DATA:
					if(size >= 4) {
						diff = (NET_READ_SHORT(data + 2) - block) & 0xffff;
						if(diff < 2) {
							if(diff)
								++block;
							else
								size = 0;	/* ignore duplicates */
							break;
						}
					}
					/* */

				default:
					frame_free(frame);
					frame = NULL;
			}

		} while(!frame);
	}
}

/*
 * retrieve file via TFTP
 */
size_t tftp_get(uint32_t server, const char *path, void *mem, size_t max)
{
	unsigned rrqsz, mark, size, retry;
	struct frame *frame;
	size_t stat;
	void *data;
	int sock;

	rrqsz = 2 + (strlen(path) + 1) + sizeof(TRANSFER_MODE);
	if(rrqsz > 512) {
		puts("path too long");
		return -1;
	}

	sock = udp_socket();
	if(sock < 0) {
		puts("no socket");
		return -1;
	}

	udp_bind(sock, 0);

	for(retry = 0; retry < TFTP_SEND_PACKETS_MAX; ++retry) {

		frame = frame_alloc();
		if(frame) {

			FRAME_INIT(frame, HARDWARE_HDRSZ + IP_HDRSZ + UDP_HDRSZ, rrqsz);
			data = FRAME_PAYLOAD(frame);

			NET_WRITE_SHORT(data + 0, OPCODE_RRQ);
			strcpy(data + 2, path);
			strcpy(data + rrqsz - sizeof(TRANSFER_MODE), TRANSFER_MODE);

			udp_sendto(sock, frame, server, TFTP_PORT_SERVER);
		}

		for(mark = MFC0(CP0_COUNT); MFC0(CP0_COUNT) - mark < CP0_COUNT_RATE * 2;) {

			if(kbhit() && getch() == ' ') {
				udp_close(sock);
				puts("aborted");
				return -1;
			}

			frame = udp_recv(sock);
			if(frame) {

				if(frame->ip_src == server) {

					data = FRAME_PAYLOAD(frame);
					size = FRAME_SIZE(frame);

					if(size >= 2)
						switch(NET_READ_SHORT(data + 0)) {

							case OPCODE_ERROR:
								tftp_error(data, size);
								frame_free(frame);
								udp_close(sock);
								return -1;

							case OPCODE_DATA:
								if(size >= 4 && size <= 4 + 512 && NET_READ_SHORT(data + 2) == 1) {
									udp_connect(sock, server, frame->udp_src);
									stat = tftp_transfer(sock, mem, max, frame);
									udp_close(sock);
									return stat;
								}
						}
				}

				frame_free(frame);
			}
		}
	}

	udp_close(sock);
	puts("no response");

	return -1;
}

int cmnd_tftp(int opsz)
{
	void *base, *copy;
	uint32_t server;
	size_t size;

	if(argc < 3)
		return E_ARGS_UNDER;

	if(argc > 4)
		return E_ARGS_OVER;

	if(!inet_aton(argv[1], &server)) {
		puts("invalid address");
		return E_UNSPEC;
	}

	heap_reset();

	if(argc > 3) {

		base = heap_reserve_lo(0);

		size = tftp_get(server, argv[3], base, heap_space());
		if((long) size < 0)
			return E_UNSPEC;

		/* move to top of heap */

		copy = heap_reserve_hi(size);

		if(copy <= base + size) {
			puts("too big to move");
			return E_UNSPEC;
		}

		memcpy(copy, base, size);

		heap_alloc();
		heap_mark();
	}

	size = tftp_get(server, argv[2], heap_reserve_lo(2), heap_space());
	if((long) size < 0)
		return E_UNSPEC;

	heap_reserve_lo(size);
	heap_alloc();
	heap_info();

	return E_NONE;
}

/* vi:set ts=3 sw=3 cin path=include,../include: */
