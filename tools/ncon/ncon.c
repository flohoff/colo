/*
 * (C) P. Horton 2005,2006
 *
 * $Id$
 */

#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <netdb.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define APP_NAME						"ncon"

#define DEFAULT_SRC_PORT			6666
#define DEFAULT_DST_PORT			6665

static void usage(void)
{
	puts("\nusage: " APP_NAME " [ -p source ] host [ port ]\n");

	exit(1);
}

int main(int argc, char *argv[])
{
	static char buf[4096];

	int opt, sck, flg, done, code;
	unsigned src_port, dst_port;
	struct termios org, raw;
	struct sockaddr_in sin;
	struct hostent *hst;
	fd_set dsc;
	char *ptr;

	src_port = DEFAULT_SRC_PORT;
	dst_port = DEFAULT_DST_PORT;
	opterr = 0;

	while((opt = getopt(argc, argv, "p:")) != -1)

		switch(opt) {

			case 'p':
				src_port = strtoul(optarg, &ptr, 10);
				if(*ptr || src_port > 0xffff) {
					fputs(APP_NAME ": invalid source port\n", stderr);
					return -1;
				}
				break;

			default:
				usage();
		}

	if(argc == optind || argc - optind > 2)
		usage();

	if(argc - optind > 1) {
		dst_port = strtoul(argv[optind + 1], &ptr, 10);
		if(*ptr || dst_port > 0xffff) {
			fputs(APP_NAME ": invalid destination port\n", stderr);
			return -1;
		}
	}

	sck = socket(PF_INET, SOCK_DGRAM, 0);
	if(sck == -1) {
		perror(APP_NAME ": socket");
		return -1;
	}

	if((flg = fcntl(sck, F_GETFL)) == -1 || fcntl(sck, F_SETFL, flg | O_NONBLOCK)) {
		perror(APP_NAME ": fcntl (socket)");
		return -1;
	}

	if((flg = fcntl(STDIN_FILENO, F_GETFL)) == -1 || fcntl(STDIN_FILENO, F_SETFL, flg | O_NONBLOCK)) {
		perror(APP_NAME ": fcntl (stdin)");
		return -1;
	}

	sin.sin_port = htons(src_port);
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_family = AF_INET;

	if(bind(sck, (struct sockaddr *) &sin, sizeof(sin))) {
		perror(APP_NAME ": bind");
		return -1;
	}

	if(!inet_aton(argv[optind], &sin.sin_addr)) {
		hst = gethostbyname(argv[optind]);
		if(!hst || hst->h_addrtype != PF_INET) {
			fputs(APP_NAME ": failed to resolve host\n", stderr);
			return -1;
		}
		sin.sin_addr = *(struct in_addr *) hst->h_addr_list[0];
	}

	sin.sin_port = htons(dst_port);
	sin.sin_family = AF_INET;

	if(connect(sck, (struct sockaddr *) &sin, sizeof(sin))) {
		perror(APP_NAME ": connect");
		return -1;
	}

	printf("\nConnect: %u --> %s:%u\n\n[[[  press CTRL-D to exit   ]]]\n\n", src_port, inet_ntoa(sin.sin_addr), dst_port);

	tcgetattr(STDIN_FILENO, &org);
	raw = org;
	cfmakeraw(&raw);
	raw.c_oflag |= ONLCR | OPOST;					/* CoLo outputs CR/LF, the kernel just LF */
	tcsetattr(STDIN_FILENO, TCSANOW, &raw);

	FD_ZERO(&dsc);

	for(code = -1;;) {

		FD_SET(STDIN_FILENO, &dsc);
		FD_SET(sck, &dsc);

		select(sck + 1, &dsc, NULL, NULL, NULL);

		if(FD_ISSET(STDIN_FILENO, &dsc)) {
			done = read(STDIN_FILENO, buf, sizeof(buf));
			if(done < 0 && errno != EAGAIN) {
				perror(APP_NAME ": read (stdin)");
				break;
			}
			if(memchr(buf, 4, done)) {
				code = 0;
				break;
			}
			if(done > 0)
				send(sck, buf, done, 0);
		}

		if(FD_ISSET(sck, &dsc)) {
			done = read(sck, buf, sizeof(buf));
			if(done < 0 && errno != EAGAIN) {
				perror(APP_NAME ": read (socket)");
				break;
			}
			if(done > 0)
				write(STDOUT_FILENO, buf, done);
		}
	}

	tcsetattr(STDIN_FILENO, TCSANOW, &org);

	putchar('\n');

	return code;
}

/* vi:set ts=3 sw=3 cin: */
