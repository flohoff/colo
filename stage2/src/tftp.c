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

#define TFTP_SEND_PACKETS_MAX		10

#define TFTP_PORT_SERVER			69
#define TFTP_BLOCK_SIZE				512
#define TFTP_RRQ_SIZE_MAX			512

#define OPCODE_RRQ					1
#define OPCODE_DATA					3
#define OPCODE_ACK					4
#define OPCODE_ERROR					5
#define OPCODE_OACK					6

/*
 * display error message from TFTP ERROR frame
 */
static void tftp_error(const void *data, unsigned size)
{
	putstring("server reported error");
	if(size >= 4) {
		printf(" #%u", NET_READ_SHORT(data + 2));
		if(size > 4) {
			if(!((char *) data)[size - 1])
				--size;
			putstring(" \"");
			putstring_safe(data + 4, size - 4);
			putchar('"');
		}
	}
	putchar('\n');
}

/*
 * transfer data blocks using TFTP
 */
static size_t tftp_transfer(int sock, void *mem, size_t max, struct frame *frame)
{
	unsigned size, block, mark, diff, update, tick;
	size_t loaded;
	void *data;

	data = FRAME_PAYLOAD(frame);
	size = FRAME_SIZE(frame);

	loaded = 0;
	block = 1;
	tick = 0;

	putstring(" 0KB\r");

	update = MFC0(CP0_COUNT);

	for(;;) {

		if(size >= 4) {

			data += 4;
			size -= 4;

			loaded += size;
			if(loaded > max) {
				frame_free(frame);
				puts("too big   ");
				return -1;
			}

			memcpy(mem, data, size);
			mem += size;

			/* have we done ? */

			size = (size < TFTP_BLOCK_SIZE);
		}
		
		/* reuse received frame */

		FRAME_INIT(frame, HARDWARE_HDRSZ + IP_HDRSZ + UDP_HDRSZ, 4);
		data = FRAME_PAYLOAD(frame);
		NET_WRITE_SHORT(data + 0, OPCODE_ACK);
		NET_WRITE_SHORT(data + 2, block);

		udp_send(sock, frame);

		if(size) {

			if(tick)
				printf("%uKB loaded (%uKB/sec)\n", (loaded + 512) / 1024, (loaded + 128) / (256 * tick));
			else
				printf("%uKB loaded\n", (loaded + 512) / 1024);

			return loaded;
		}

		mark = MFC0(CP0_COUNT);

		if(mark - update >= CP0_COUNT_RATE / 4) {
			update = mark;
			++tick;
			printf(" %uKB\r", loaded / 1024);
		}

		do {

			if(BREAK()) {
				puts("aborted   ");
				return -1;
			}

			if(MFC0(CP0_COUNT) - mark >= CP0_COUNT_RATE * 10) {
				puts("no response");
				return -1;
			}

			frame = udp_recv(sock);
			if(!frame)
				continue;

			size = FRAME_SIZE(frame);

			if(size < 2 || size > 4 + TFTP_BLOCK_SIZE) {
				frame_free(frame);
				continue;
			}

			data = FRAME_PAYLOAD(frame);

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
 *
 * (issues RRQ then calls tftp_transfer() to receive the data)
 */
size_t tftp_get(uint32_t server, const char *path, void *mem, size_t max)
{
	static char rrq[TFTP_RRQ_SIZE_MAX + 64];

	unsigned rrqsz, mark, size, retry;
	struct frame *frame;
	size_t stat;
	void *data;
	int sock;

	rrqsz = strlen(path);
	if(rrqsz <= TFTP_RRQ_SIZE_MAX) {
		NET_WRITE_SHORT(rrq, OPCODE_RRQ);
		data = stpcpy(rrq + 2, path) + 1;
		data = stpcpy(data, "octet") + 1;
		rrqsz = (char *) data - rrq;
	}
	if(rrqsz > TFTP_RRQ_SIZE_MAX) {
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
			memcpy(FRAME_PAYLOAD(frame), rrq, rrqsz);
			udp_sendto(sock, frame, server, TFTP_PORT_SERVER);
		}

		for(mark = MFC0(CP0_COUNT); MFC0(CP0_COUNT) - mark < CP0_COUNT_RATE * 2;) {

			if(BREAK()) {
				udp_close(sock);
				puts("aborted");
				return -1;
			}

			frame = udp_recv(sock);
			if(frame) {

				if(frame->ip_src == server) {

					data = FRAME_PAYLOAD(frame);
					size = FRAME_SIZE(frame);

					if(size >= 2 && size <= 4 + TFTP_BLOCK_SIZE)
						switch(NET_READ_SHORT(data + 0)) {

							case OPCODE_ERROR:
								tftp_error(data, size);
								frame_free(frame);
								udp_close(sock);
								return -1;

							case OPCODE_DATA:
								if(size >= 4 && NET_READ_SHORT(data + 2) == 1) {
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
	uint32_t server;
	size_t size;
	void *base;

	if(argc < 3)
		return E_ARGS_UNDER;

	if(argc > 4)
		return E_ARGS_OVER;

	if(!inet_aton(argv[1], &server)) {
		puts("invalid address");
		return E_UNSPEC;
	}

	if(!net_is_up())
		return E_NET_DOWN;

	heap_reset();

	if(argc > 3) {

		base = heap_reserve_lo(0);

		size = tftp_get(server, argv[3], base, heap_space());
		if((long) size < 0)
			return E_UNSPEC;

		memmove(heap_reserve_hi(size), base, size);

		heap_alloc();
		heap_mark();
	}

	base = heap_reserve_lo(0);

	size = tftp_get(server, argv[2], base, heap_space());
	if((long) size < 0) {
		heap_reset();
		return E_UNSPEC;
	}

	memmove(heap_reserve_hi(size), base, size);

	heap_alloc();

	heap_initrd_vars();

	heap_info();

	return E_NONE;
}

/* vi:set ts=3 sw=3 cin path=include,../include: */
