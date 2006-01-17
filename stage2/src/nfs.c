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

#define RPC_SEND_PACKETS_MAX			10
#define NFS_READ_BLOCK					512
#define SYMLINK_PATH_MAX				10

#define RPC_PORTMAP_PORT				111

#define RPC_PORTMAP_PROG				100000
#define RPC_PORTMAP_VERS				2
#define RPC_PORTMAP_GETPORT			3

#define RPC_NFS_PROG						100003
#define RPC_NFS_VERS						2
#define RPC_NFS_GETATTR					1
#define RPC_NFS_LOOKUP					4
#define RPC_NFS_READLINK				5
#define RPC_NFS_READ						6
#define RPC_NFS_READDIR					16

#define RPC_MOUNT_PROG					100005
#define RPC_MOUNT_VERS					1
#define RPC_MOUNT_MNT					1
#define RPC_MOUNT_UMNT_ALL				4

#define RPC_VERSION						2

#define NFS_FHSIZE						(sizeof(struct nfs_handle))
#define NFS_FASIZE						(sizeof(struct nfs_object) - offsetof(struct nfs_object, type))

enum rpc_auth
{
	RPC_AUTH_NULL			= 0,
	RPC_AUTH_UNIX			= 1,
	RPC_AUTH_SHORT			= 2,
};

enum rpc_type
{
	RPC_CALL					= 0,
	RPC_REPLY				= 1,
};

enum rpc_reply_stat
{
	RPC_MSG_ACCEPTED		= 0,
};

enum rpc_accept_stat
{
	RPC_SUCCESS				= 0,
};

enum nfs_status
{
	NFS_OK					= 0,
	NFSERR_PERM				= 1,
	NFSERR_NOENT			= 2,
	NFSERR_IO				= 5,
	NFSERR_NXIO				= 6,
	NFSERR_ACCES			= 13,
	NFSERR_EXIST			= 17,
	NFSERR_NODEV			= 19,
	NFSERR_NOTDIR			= 20,
	NFSERR_ISDIR			= 21,
	NFSERR_FBIG				= 27,
	NFSERR_NOSPC			= 28,
	NFSERR_ROFS				= 30,
	NFSERR_NAMETOOLONG	= 63,
	NFSERR_NOTEMPTY		= 66,
	NFSERR_DQUOT			= 69,
	NFSERR_STALE			= 70,
	NFSERR_WFLUSH			= 99,
};

struct nfs_handle
{
	uint8_t		p[32];
};

struct nfs_timeval
{
	uint32_t		secs;
	uint32_t		usecs;
};

struct nfs_object
{
	struct nfs_handle		handle;
	uint32_t					type;
	uint32_t					mode;
	uint32_t					nlink;
	uint32_t					uid;
	uint32_t					gid;
	uint32_t					size;
	uint32_t					blksize;
	uint32_t					rdev;
	uint32_t					blocks;
	uint32_t					fsid;
	uint32_t					fileid;
	struct nfs_timeval	atime;
	struct nfs_timeval	mtime;
	struct nfs_timeval	ctime;

} __attribute__((packed));

static union
{
	uint8_t	b[1024];
	uint32_t	w[1];

} scratch;

/*
 * issue SUN RPC call and wait for reply
 */
static struct frame *rpc_make_call(int sock, unsigned prog, unsigned vers, unsigned proc, const void *args, unsigned argsz)
{
	static unsigned xid;

	unsigned retry, mark, stat, size, hdsz;
	struct frame *frame;
	void *data;

	++xid;

	for(retry = 0; retry < RPC_SEND_PACKETS_MAX; ++retry) {

		frame = frame_alloc();
		if(frame) {

			FRAME_INIT(frame, HARDWARE_HDRSZ + IP_HDRSZ + UDP_HDRSZ, 0x3c + argsz);

			data = FRAME_PAYLOAD(frame);

			NET_WRITE_LONG(data + 0x00, xid);
			NET_WRITE_LONG(data + 0x04, RPC_CALL);
			NET_WRITE_LONG(data + 0x08, RPC_VERSION);
			NET_WRITE_LONG(data + 0x0c, prog);
			NET_WRITE_LONG(data + 0x10, vers);
			NET_WRITE_LONG(data + 0x14, proc);

			NET_WRITE_LONG(data + 0x18, RPC_AUTH_UNIX);
			NET_WRITE_LONG(data + 0x1c, 5 * 4);
			NET_WRITE_LONG(data + 0x20, 0);		/* stamp		*/
			NET_WRITE_LONG(data + 0x24, 0);		/* hostname	*/
			NET_WRITE_LONG(data + 0x28, 0);		/* uid		*/
			NET_WRITE_LONG(data + 0x2c, 0);		/* gid		*/
			NET_WRITE_LONG(data + 0x30, 0);		/* aux gids	*/

			NET_WRITE_LONG(data + 0x34, RPC_AUTH_NULL);
			NET_WRITE_LONG(data + 0x38, 0);

			if(args)
				memcpy(data + 0x3c, args, argsz);

			udp_send(sock, frame);
		}

		for(mark = MFC0(CP0_COUNT); MFC0(CP0_COUNT) - mark < CP0_COUNT_RATE * 2;) {

			if(BREAK()) {
				puts("aborted   ");
				return NULL;
			}

			frame = udp_recv(sock);
			if(frame) {

				data = FRAME_PAYLOAD(frame);
				size = FRAME_SIZE(frame);

				if(size >= 6 * 4 && 
					NET_READ_LONG(data + 0x00) == xid &&
					NET_READ_LONG(data + 0x04) == RPC_REPLY) {

					hdsz = (NET_READ_LONG(data + 0x10) + 3) & ~3;
					hdsz = 5 * 4 + hdsz + 4;

					if(hdsz > size) {
						printf("RPC call %u/%u.%u failed (invalid verifier)\n", prog, vers, proc);
						frame_free(frame);
						return 0;
					}

					stat = NET_READ_LONG(data + hdsz - 4);
					if(stat != RPC_SUCCESS || NET_READ_LONG(data + 0x08) != RPC_MSG_ACCEPTED) {
						printf("RPC call %u/%u.%u failed (status %u)\n", prog, vers, proc, stat);
						frame_free(frame);
						return NULL;
					}

					FRAME_STRIP(frame, hdsz);

					return frame;
				}

				frame_free(frame);
			}
		}
	}

	puts("no response");

	return NULL;
}

/*
 * translate NFS error code to text
 */
static const char *nfs_error(unsigned err)
{
	static const char *msg[] =
	{
		[NFS_OK]					= "success",
		[NFSERR_PERM]			= "not owner",
		[NFSERR_NOENT]			= "no such file or directory",
		[NFSERR_IO]				=	"I/O error",
		[NFSERR_NXIO]			= "no such device or address",
		[NFSERR_ACCES]			= "permission denied",
		[NFSERR_EXIST]			= "file exists",
		[NFSERR_NODEV]			= "no such device",
		[NFSERR_NOTDIR]		= "not a directory",
		[NFSERR_ISDIR]			= "is a directory",
		[NFSERR_FBIG]			= "file too big",
		[NFSERR_NOSPC]			= "no space on device",
		[NFSERR_ROFS]			= "read only filing system",
		[NFSERR_NAMETOOLONG]	= "filename too long",
/*		[NFSERR_NOTEMPTY]		= "directory not empty",
		[NFSERR_DQUOT]			= "disk quota exceeded",
		[NFSERR_STALE]			= "file not available",
		[NFSERR_WFLUSH]		= "write cache flushed",		*/
	};

	static char buf[64];

	if(err < elements(msg) && msg[err])
		return msg[err];

	sprintf(buf, "unkown error #%u", err);

	return buf;
}

/*
 * look up port for specified program using portmap service
 */
static unsigned rpc_portmap(int sock, uint32_t host, unsigned prog, unsigned vers)
{
	struct frame *frame;
	unsigned port;
	void *data;

	NET_WRITE_LONG(&scratch.w[0], prog);
	NET_WRITE_LONG(&scratch.w[1], vers);
	NET_WRITE_LONG(&scratch.w[2], IPPROTO_UDP);
	NET_WRITE_LONG(&scratch.w[3], 0);

	udp_connect(sock, host, RPC_PORTMAP_PORT);

	frame = rpc_make_call(sock, RPC_PORTMAP_PROG, RPC_PORTMAP_VERS, RPC_PORTMAP_GETPORT, scratch.w, 4 * 4);

	port = 0;
	if(frame) {
		if(FRAME_SIZE(frame) >= 4) {
			data = FRAME_PAYLOAD(frame);
			port = NET_READ_LONG(data);
		} else
			puts("portmap invalid reply");
		frame_free(frame);
	}

	return port;
}

/*
 * mount an NFS volume
 */
static int nfs_mount(int sock, const char *path, struct nfs_object *obj)
{
	struct frame *frame;
	unsigned size, stat;
	void *data;

	size = strlen(path);
	if(4 + size > sizeof(scratch)) {
		puts("path too long");
		return 0;
	}

	NET_WRITE_LONG(&scratch.w[0], size);
	scratch.w[1 + size / 4] = 0;
	memcpy(&scratch.w[1], path, size);

	frame = rpc_make_call(sock, RPC_MOUNT_PROG, RPC_MOUNT_VERS, RPC_MOUNT_MNT, scratch.w, (4 + size + 3) & ~3);
	if(!frame)
		return 0;

	size = FRAME_SIZE(frame);
	if(size < 4)
		puts("mount invalid reply");
	else {
		data = FRAME_PAYLOAD(frame);
		stat = NET_READ_LONG(data);
		if(stat == NFS_OK && size >= 4 + NFS_FHSIZE) {
			if(obj) {
				memcpy(&obj->handle, data + 4, NFS_FHSIZE);
				memset(&obj->type, 0, NFS_FASIZE);
			}
			frame_free(frame);
			return 1;
		}
		printf("mount failed (%s)\n", nfs_error(stat));
	}
	frame_free(frame);
	
	return 0;
}

/*
 * unmount all NFS volumes
 */
static int nfs_umount_all(int sock)
{
	struct frame *frame;

	frame = rpc_make_call(sock, RPC_MOUNT_PROG, RPC_MOUNT_VERS, RPC_MOUNT_UMNT_ALL, NULL, 0);
	if(!frame)
		return 0;

	frame_free(frame);
	return 1;
}

/*
 * get file attributes for specified handle
 */
static int nfs_get_attr(int sock, struct nfs_object *obj)
{
	struct frame *frame;
	unsigned size, stat;
	void *data;

	frame = rpc_make_call(sock, RPC_NFS_PROG, RPC_NFS_VERS, RPC_NFS_GETATTR, &obj->handle, sizeof(obj->handle));
	if(!frame)
		return 0;

	size = FRAME_SIZE(frame);
	if(size < 4)
		puts("get attributes invalid reply");
	else {
		data = FRAME_PAYLOAD(frame);
		stat = NET_READ_LONG(data);
		if(stat == NFS_OK && size >= 4 + NFS_FASIZE) {
			memcpy(&obj->type, data + 4, NFS_FASIZE);
			frame_free(frame);
			return 1;
		}
		printf("get attributes failed (%s)\n", nfs_error(stat));
	}

	frame_free(frame);

	return 0;
}

/*
 * look up a pathname component
 */
static int nfs_lookup(int sock, const struct nfs_object *dir, const char *path, unsigned size, struct nfs_object *obj)
{
	struct frame *frame;
	unsigned stat;
	void *data;

	if(NFS_FHSIZE + 4 + size > sizeof(scratch)) {
		puts("path too long");
		return 0;
	}

	memcpy(scratch.b, &dir->handle, NFS_FHSIZE);
	NET_WRITE_LONG(&scratch.w[NFS_FHSIZE / 4], size);
	scratch.w[NFS_FHSIZE / 4 + 1 + size / 4] = 0;
	memcpy(&scratch.w[NFS_FHSIZE / 4 + 1], path, size);

	frame = rpc_make_call(sock, RPC_NFS_PROG, RPC_NFS_VERS, RPC_NFS_LOOKUP, scratch.b, (NFS_FHSIZE + 4 + size + 3) & ~3);
	if(!frame)
		return 0;

	size = FRAME_SIZE(frame);
	if(size < 4)
		puts("lookup invalid reply");
	else {
		data = FRAME_PAYLOAD(frame);
		stat = NET_READ_LONG(data);
		if(stat == NFS_OK && size >= 4 + sizeof(struct nfs_object)) {
			if(obj)
				memcpy(obj, data + 4, sizeof(struct nfs_object));
			frame_free(frame);
			return 1;
		}
		printf("lookup failed (%s)\n", nfs_error(stat));
	}
	frame_free(frame);

	return 0;
}

/*
 * read a file over NFS
 */
static int nfs_read_file(int sock, const struct nfs_object *obj, void *buffer, unsigned total)
{
	unsigned offset, copy, size, stat, read, update, mark, tick;
	struct frame *frame;
	void *data;

	putstring(" 0KB\r");

	update = MFC0(CP0_COUNT);
	tick = 0;

	for(offset = 0; offset < total;) {

		copy = total - offset;
		if(copy > NFS_READ_BLOCK)
			copy = NFS_READ_BLOCK;

		memcpy(scratch.b, &obj->handle, NFS_FHSIZE);

		NET_WRITE_LONG(&scratch.w[NFS_FHSIZE / 4], offset);
		NET_WRITE_LONG(&scratch.w[NFS_FHSIZE / 4 + 1], copy);
		NET_WRITE_LONG(&scratch.w[NFS_FHSIZE / 4 + 2], 0);

		frame = rpc_make_call(sock, RPC_NFS_PROG, RPC_NFS_VERS, RPC_NFS_READ, scratch.b, NFS_FHSIZE + 3 * 4);
		if(!frame)
			return 0;

		size = FRAME_SIZE(frame);
		if(size < 4) {
			puts("read invalid reply");
			frame_free(frame);
			return 0;
		}

		data = FRAME_PAYLOAD(frame);
		stat = NET_READ_LONG(data);

		if(stat != NFS_OK || size < 4 + NFS_FASIZE + 4) {
			printf("read failed (%s)\n", nfs_error(stat));
			frame_free(frame);
			return 0;
		}

		read = NET_READ_LONG(data + 4 + NFS_FASIZE);
		if(4 + (sizeof(struct nfs_object) - NFS_FHSIZE) + 4 + read > size) {
			puts("read invalid reply size");
			frame_free(frame);
			return 0;
		}

		if(read != copy) {
			puts("read file size different");
			frame_free(frame);
			return 0;
		}

		memcpy(buffer + offset, data + 4 + NFS_FASIZE + 4, read);

		frame_free(frame);

		offset += read;

		mark = MFC0(CP0_COUNT);

		if(mark - update >= CP0_COUNT_RATE / 4) {
			update = mark;
			++tick;
			printf(" %uKB\r", offset / 1024);
		}
	}

	if(tick)
		printf("%uKB loaded (%uKB/sec)\n", (offset + 512) / 1024, (offset + 128) / (256 * tick));
	else
		printf("%uKB loaded\n", (offset + 512) / 1024);

	return 1;
}

/*
 * read symbolic link contents
 */
static int nfs_readlink(int sock, char *buffer, struct nfs_object *obj)
{
	unsigned size, stat, read;
	struct frame *frame;
	void *data;

	frame = rpc_make_call(sock, RPC_NFS_PROG, RPC_NFS_VERS, RPC_NFS_READLINK, &obj->handle, sizeof(obj->handle));
	if(!frame)
		return 0;

	size = FRAME_SIZE(frame);
	if(size < 4)
		puts("readlink invalid reply");
	else {
		data = FRAME_PAYLOAD(frame);
		stat = NET_READ_LONG(data);
		if(stat == NFS_OK && size >= 4 + 4) {
			read = NET_READ_LONG(data + 4);
			if(4 + 4 + read <= size) {
				if(read == NET_READ_LONG(&obj->size)) {
					if(buffer)
						memcpy(buffer, data + 8, read);
					frame_free(frame);
					return 1;
				}
				puts("readlink link size mismatch");
			} else
				puts("readlink invalid reply size");
		} else
			printf("readlink failed (%s)\n", nfs_error(stat));
	}

	frame_free(frame);
	
	return 0;
}

/*
 * read directory contents from server
 */
static int nfs_read_dir(int sock, const struct nfs_object *dir,
	int (*func)(void *, const char *, struct nfs_object *), void *arg)
{
	unsigned cookie, size, nmsz, obsz, stat;
	struct nfs_object obj;
	struct frame *frame;
	void *data;
	int code;

	cookie = 0;

	for(;;) {

		memcpy(scratch.b, &dir->handle, NFS_FHSIZE);

		NET_WRITE_LONG(&scratch.w[NFS_FHSIZE / 4], cookie);
		NET_WRITE_LONG(&scratch.w[NFS_FHSIZE / 4 + 1], NFS_READ_BLOCK);

		frame = rpc_make_call(sock, RPC_NFS_PROG, RPC_NFS_VERS, RPC_NFS_READDIR, scratch.b, NFS_FHSIZE + 2 * 4);
		if(!frame)
			return -1;

		data = FRAME_PAYLOAD(frame);
		size = FRAME_SIZE(frame);

		if(size < 4) {
invalid:
			puts("read directory invalid reply");
			frame_free(frame);
			return -1;
		}

		stat = NET_READ_LONG(data);
		if(stat != NFS_OK) {
			printf("read directory failed (%s)\n", nfs_error(stat));
			frame_free(frame);
			return -1;
		}

		for(obsz = 4;;) {

			data += obsz;
			size -= obsz;

			if(size < 2 * 4)
				goto invalid;

			if(NET_READ_LONG(data)) {
				
				if(size < 4 * 4)
					goto invalid;

				nmsz = NET_READ_LONG(data + 8);
				obsz = (3 * 4 + nmsz + 4 + 3) & ~3;

				if(size < obsz)
					goto invalid;

				if(nmsz < sizeof(scratch) && nfs_lookup(sock, dir, data + 3 * 4, nmsz, &obj)) {

					memcpy(scratch.b, data + 3 * 4, nmsz);
					scratch.b[nmsz] = '\0';

					code = func(arg, (char *) scratch.b, &obj);
					if(code)
						return code;
				}

				cookie = NET_READ_LONG(data + obsz - 4);

			} else if(NET_READ_LONG(data + 4)) {

				frame_free(frame);
				return 0;

			} else {

				if(!cookie)
					goto invalid;

				break;
			}
		}

		frame_free(frame);
	}
}

/*
 * look up a full path, following symbolic links
 */
static int nfs_path_lookup(int sock, const struct nfs_object *root, struct nfs_object *obj, const char *path)
{
	static char buffer[4096];

	unsigned curr, size, next, mode, link;
	struct nfs_object node[2];
	int which, other;

	size = strlen(path) + 1;
	if(size > sizeof(buffer)) {
		puts("path too long");
		return 0;
	}

	curr = sizeof(buffer) - size;
	strcpy(&buffer[curr], path);

	node[0] = *(buffer[curr] == '/' ? root : obj);

	which = 1;
	other = 0;
	link = 0;

	for(;;) {

		other = 1 - which;

		for(; buffer[curr] == '/'; ++curr)
			;

		if(!buffer[curr])
			break;

		mode = NET_READ_LONG(&node[other].mode);
		if(!S_ISDIR(mode)) {
			puts("not a directory");
			return 0;
		}

		for(next = curr; buffer[next] && buffer[next] != '/'; ++next)
			;

		DPRINTF("nfs: lookup \"%.*s\"\n", (int)(next - curr), &buffer[curr]);

		if(!nfs_lookup(sock, &node[other], &buffer[curr], next - curr, &node[which]))
			return 0;
		curr = next;

		mode = NET_READ_LONG(&node[which].mode);
		if(S_ISLNK(mode)) {

			if(++link == SYMLINK_PATH_MAX) {
				puts("too many symlinks");
				return 0;
			}

			size = NET_READ_LONG(&node[which].size);
			if(size > curr) {
				puts("symlinks too long");
				return 0;
			}

			curr -= size;
			if(!nfs_readlink(sock, &buffer[curr], &node[which]))
				return 0;

			DPRINTF("nfs: symlink \"%.*s\"\n", (int) size, &buffer[curr]);

			if(buffer[curr] == '/')
				node[other] = *root;

		} else

			which = other;
	}

	DPRINTF("nfs: mode <0%o>\n", NET_READ_LONG(&node[other].mode));

	if(obj)
		*obj = node[other];

	return 1;
}

static int dump_node(void *arg, const char *name, struct nfs_object *obj)
{
	static char node[16];

	unsigned mode, size, rdev;

	mode = NET_READ_LONG(&obj->mode);
	size = NET_READ_LONG(&obj->size);

	if(S_ISCHR(mode) || S_ISBLK(mode)) {
		rdev = NET_READ_LONG(&obj->rdev);
		sprintf(node, "%u,%4u", (rdev >> 8) & 0xff, rdev & 0xff);
		printf("%10s  ", node);
	} else
		printf("%10u  ", size);

	putstring_safe(name, -1);

	if(S_ISLNK(mode)) {
		if(size <= sizeof(scratch) && nfs_readlink((int) arg, (char *) scratch.b, obj)) {
			putstring(" --> ");
			putstring_safe(scratch.b, size);
		} else
			putchar('@');
	} else if(S_ISDIR(mode))
		putchar('/');
	else if(S_ISFIFO(mode))
		putchar('|');
	else if(S_ISSOCK(mode))
		putchar('=');
	else if(mode & 0111)
		putchar('*');

	putchar('\n');

	return 0;
}

int cmnd_nfs(int opsz)
{
	unsigned port_mnt, port_nfs, mode, size;
	struct nfs_object mount, file;
	uint32_t server;
	int sock, error;
	void *base;

	if(argc < 3)
		return E_ARGS_UNDER;
	if(argc > 5)
		return E_ARGS_OVER;

	if(!inet_aton(argv[1], &server)) {
		puts("invalid address");
		return E_UNSPEC;
	}

	if(!net_is_up())
		return E_NET_DOWN;

	sock = udp_socket();
	if(sock < 0) {
		puts("no socket");
		return E_UNSPEC;
	}

	udp_bind_range(sock, 768, 1024);

	port_mnt = rpc_portmap(sock, server, RPC_MOUNT_PROG, RPC_MOUNT_VERS);
	if(!port_mnt) {
		udp_close(sock);
		return E_UNSPEC;
	}

	port_nfs = rpc_portmap(sock, server, RPC_NFS_PROG, RPC_NFS_VERS);
	if(!port_nfs) {
		udp_close(sock);
		return E_UNSPEC;
	}

	udp_connect(sock, server, port_mnt);

	if(!nfs_mount(sock, argv[2], &mount)) {
		udp_close(sock);
		return E_UNSPEC;
	}

	DPRINTF("nfs: mounted \"%s\"\n", argv[2]);

	udp_connect(sock, server, port_nfs);

	error = E_UNSPEC;

	if(!nfs_get_attr(sock, &mount))
		goto umount;

	if(argc < 4) {

		if(!nfs_read_dir(sock, &mount, dump_node, (void *) sock))
			error = E_NONE;

		goto umount;
	}

	heap_reset();

	if(argc > 4) {

		file = mount;

		if(!nfs_path_lookup(sock, &mount, &file, argv[4]))
			goto umount;

		mode = NET_READ_LONG(&file.mode);
		if(!S_ISREG(mode)) {
			puts("not a file");
			goto umount;
		}

		size = NET_READ_LONG(&file.size);

		base = heap_reserve_hi(size);
		if(!base) {
			puts("file too big");
			goto umount;
		}

		if(!nfs_read_file(sock, &file, base, size))
			goto umount;

		heap_alloc();
		heap_mark();
	}

	file = mount;

	if(!nfs_path_lookup(sock, &mount, &file, argv[3])) {
		heap_reset();
		goto umount;
	}

	mode = NET_READ_LONG(&file.mode);

	if(argc < 5 && S_ISDIR(mode)) {

		if(!nfs_read_dir(sock, &file, dump_node, (void *) sock))
			error = E_NONE;

		goto umount;
	}

	if(!S_ISREG(mode)) {
		puts("not a file");
		heap_reset();
		goto umount;
	}

	size = NET_READ_LONG(&file.size);

	base = heap_reserve_hi(size);
	if(!base) {
		puts("file too big");
		heap_reset();
		goto umount;
	}

	if(!nfs_read_file(sock, &file, base, size)) {
		heap_reset();
		goto umount;
	}

	heap_alloc();
	heap_initrd_vars();
	heap_info();

	error = E_NONE;

umount:
	udp_connect(sock, server, port_mnt);

	if(nfs_umount_all(sock))
		DPRINTF("nfs: unmounted \"%s\"\n", argv[2]);

	udp_close(sock);

	return error;
}

/* vi:set ts=3 sw=3 cin path=include,../include: */
