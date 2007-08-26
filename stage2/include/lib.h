/*
 * (C) P.Horton 2004,2005,2006
 *
 * $Id$
 *
 * This code is covered by the GNU General Public License. For details see the file "COPYING".
 */

#ifndef _CLIB_H_
#define _CLIB_H_

#ifndef _STDARG_H
# include <stdarg.h>
#endif

#define NULL					((void *) 0)

#define elements(x)			((int)(sizeof(x)/sizeof((x)[0])))
#define offsetof(s,m)		((int)&((s*)0)->m)

#define __STR(x)				#x
#define _STR(x)				__STR(x)

#ifndef NDEBUG
# define assert(x)			do{\
										if(!(x)) {\
											puts("\n***  ASSERTION FAILED [" __FILE__ ":" _STR(__LINE__) "] (" #x ")  ***\n");\
											for(;;)\
												;\
										}\
									}while(0)
#else
# define assert(x)
#endif

#ifdef _DEBUG
# define DPUTS(s)				do{puts(s);}while(0)
# define DPRINTF(f,a...)	do{printf((f),##a);}while(0)
# define DPUTCHAR(c)			do{putchar(c);}while(0)
#else
# define DPUTS(s)
# define DPRINTF(f,a...)
# define DPUTCHAR(c)
#endif

#define align_up(p,n)		({ unsigned long a=(n), b=(unsigned long)(p); (__typeof(p))((b+a-1)&-a); })

#define isspace(c)			(!!((c)==' '))
#define isdigit(c)			({unsigned _c=(unsigned char)(c);_c>='0'&&_c<='9';})
#define isxdigit(c)			({unsigned _c=(unsigned char)(c);(_c>='0'&&_c<='9')||(_c>='A'&&_c<='F')||(_c>='a'&&_c<='f');})
#define isalnum(c)			({unsigned _c=(unsigned char)(c);(_c>='0'&&_c<='9')||(_c>='A'&&_c<='Z')||(_c>='a'&&_c<='z');})
#define isprint(c)			({unsigned _c=(unsigned char)(c);_c>=' '&&_c<='~';})
#define isalpha(c)			({unsigned _c=(unsigned char)(c)&~0x20;_c>='A'&&_c<='Z';})
#define toupper(c)			({unsigned _c=(unsigned char)(c);(_c>='a'&&_c<='z')?(_c^0x20):_c;})
#define tolower(c)			({unsigned _c=(unsigned char)(c);(_c>='A'&&_c<='Z')?(_c^0x20):_c;})

typedef unsigned char		uint8_t;
typedef unsigned short		uint16_t;
typedef unsigned				uint32_t;
typedef unsigned long long	uint64_t;

typedef unsigned				size_t;

typedef unsigned char		__u8;
typedef unsigned short		__u16;
typedef unsigned				__u32;
typedef unsigned long long	__u64;
typedef short					__s16;
typedef int						__s32;
typedef long long				__s64;

typedef unsigned				UWORD32;

union double_word
{
	unsigned long long		d;
	struct {
		unsigned					l;
		unsigned					h;
	} w;
};

#define double_word_ptr(v)	((union double_word*)&(v))
#define double_word_lo(v)	(double_word_ptr(v)->w.l)
#define double_word_hi(v)	(double_word_ptr(v)->w.h)

#define SIGN_EXTEND_64(v)	((long long)(long)(v))

/* ext2.c & nfs.c */

#define S_IFMT					0170000

#define S_IFIFO				0010000
#define S_IFCHR				0020000
#define S_IFDIR				0040000
#define S_IFBLK				0060000
#define S_IFREG				0100000
#define S_IFLNK				0120000
#define S_IFSOCK				0140000

#define S_ISFIFO(m)			(((m)&S_IFMT)==S_IFIFO)
#define S_ISCHR(m)			(((m)&S_IFMT)==S_IFCHR)
#define S_ISDIR(m)			(((m)&S_IFMT)==S_IFDIR)
#define S_ISBLK(m)			(((m)&S_IFMT)==S_IFBLK)
#define S_ISREG(m)			(((m)&S_IFMT)==S_IFREG)
#define S_ISLNK(m)			(((m)&S_IFMT)==S_IFLNK)
#define S_ISSOCK(m)			(((m)&S_IFMT)==S_IFSOCK)

/* lib.c */

extern size_t strlen(const char *);
extern char *strchr(const char *, int);
extern char *strcpy(char *, const char *);
extern char *stpcpy(char *, const char *);
extern int strncasecmp(const char *, const char *, size_t);
extern int strcasecmp(const char *, const char *);
extern int strncmp(const char *, const char *, size_t);
extern int strcmp(const char *, const char *);
extern int sprintf(char *, const char *, ...);
extern int printf(const char *, ...);
extern unsigned long strtoul(const char *, char **, int);
extern void putstring_safe(const void *, int);
extern int glob(const char *, const char *);
extern const char *inet_ntoa(unsigned);
extern int inet_aton(const char *, unsigned *);

/* libmem.c */

extern void *memcpy(void *, const void *, size_t);
extern void *memset(void *, int, size_t);
extern int memcmp(const void *, const void *, size_t);
extern void *memmove(void *, const void *, size_t);

/* vsprintf.c */

extern int vsprintf(char *, const char *, va_list);

/* serial.c */

extern void serial_enable(int);
extern int kbhit(void);
extern int getch(void);
extern void putchar(int);
extern void putstring(const char *);
extern void puts(const char *str);
extern void drain(void);
extern void serial_scan(void);

#define BREAK()							({ char c; kbhit() && ((c = getch()) == ' ' || c == '\003'); })

/* history.c */

extern int history_add(const char *);
extern int history_fetch(char *, size_t, unsigned);
extern void history_discard(void);
extern unsigned history_count(void);

/* edit.c */

extern void line_edit(char *, size_t);

/* shell.c */

#define MAX_CMND_ARGS					32

enum {
	E_NONE,
	E_UNSPEC,
	E_INVALID_CMND,
	E_ARGS_OVER,
	E_ARGS_UNDER,
	E_ARGS_COUNT,
	E_BAD_EXPR,
	E_BAD_VALUE,
	E_NO_SUCH_VAR,
	E_NET_DOWN,
	E_NO_SCRIPT,
	E_EXIT_SCRIPT,
};

extern void __attribute__((noreturn)) shell(void);
extern int execute_line(const char *, int *);
extern const char *error_text(int);

extern size_t argsz[];
extern unsigned argc;
extern char *argv[];

/* script.c */

extern int script_exec(const char *, int);

/* pci.c */

#define UNIT_ID_QUBE1					3
#define UNIT_ID_RAQ1						4
#define UNIT_ID_QUBE2					5
#define UNIT_ID_RAQ2						6

extern unsigned cpu_clock_khz(void);
extern void pci_init(size_t, size_t);
extern unsigned pci_unit_id(void);
extern const char *pci_unit_name(void);

/* ide.c */

extern void ide_init(void);
extern int ide_read_sectors(void *, void *, unsigned long, unsigned);
extern void *ide_open(const char *);
extern int ide_block_size(void *);
extern const char *ide_dev_name(void *);

/* expr.c */

extern unsigned long evaluate(const char *, char **);

/* cache.c */

extern void dcache_flush(unsigned long, unsigned);
extern void dcache_flush_all(void);
extern void icache_flush_all(void);

/* block.c */

extern int block_read_raw(void *, void *, unsigned long, size_t, size_t);
extern void *block_read(void *, unsigned long, size_t, size_t);
extern void block_flush(void *);
extern int block_init(void);

/* elf32.c */

struct elf_info
{
	unsigned long			load_phys;
	unsigned					load_size;
	unsigned long long	entry_point;
	unsigned long long	data_sect;
};

extern int elf32_validate(const void *, size_t, struct elf_info *);
extern void elf32_load(const void *);

/* elf64.c */

extern int elf64_validate(const void *, size_t, struct elf_info *);
extern void elf64_load(const void *);

/* inflate.c */

extern int gzip_check(const void *, size_t);
extern int unzip(const void *, size_t);

/* -- error codes 1 ... 3 are returned by inflate() */

#define INFLATE_ERR_NOT_GZIP				-4
#define INFLATE_ERR_NOT_DEFLATE			-5
#define INFLATE_ERR_BAD_CRC				-6
#define INFLATE_ERR_BAD_LENGTH			-7

/* tulip.c */

extern void tulip_init(void);

/* main.c */

extern size_t ram_size;
extern size_t ram_restrict;

/* heap.c */

extern void heap_reset(void);
extern size_t heap_space(void);
extern void *heap_reserve_lo(size_t);
extern void *heap_reserve_hi(size_t);
extern void heap_alloc(void);
extern void heap_info(void);
extern void *heap_image(size_t *);
extern void heap_mark(void);
extern void *heap_mark_image(size_t *);
extern void heap_initrd_vars(void);
extern void heap_set_initrd(void *, size_t);

/* ext2.c */

extern void *file_open(const char *, unsigned long *);
extern int file_load(void *, void *, unsigned long);

/* net.c */

extern int net_up(void);
extern void net_down(int);

#define net_is_up()							({ extern int net_alive; net_alive; })

static inline void yield(void)
{
	extern void tulip_poll(void);
	extern int net_alive;

	if(net_alive)
		tulip_poll();
}

/* lcd.c */

#define LCD_MENU_TIMEOUT					(-1)
#define LCD_MENU_CANCEL						(-2)
#define LCD_MENU_BREAK						(-3)
#define LCD_MENU_BAD_ARGS					(-4)

extern void lcd_init(void);
extern void lcd_line(int, const char *);
extern int (*lcd_menu)(const char **, unsigned, unsigned);

/* env.c */

#define VAR_OTHER								0
#define VAR_NET								1
#define VAR_DHCP								2
#define VAR_INITRD							3
#define VAR_NETCON							4

extern int env_put(const char *, const char *, unsigned);
extern const char *env_get(const char *, int);
extern void env_remove_tag(unsigned);

/* boot.c */

#define BOOT_DEFAULT							-1
#define BOOT_MENU								0

extern int boot(int);

/* nv.c */

#define NVFLAG_IDE_DISABLE_LBA			(1 << 0)
#define NVFLAG_IDE_DISABLE_LBA48			(1 << 1)
#define NVFLAG_IDE_DISABLE_TIMING		(1 << 2)
#define NVFLAG_IDE_ENABLE_SLAVE			(1 << 3)
#define NVFLAG_CONSOLE_DISABLE			(1 << 4)
#define NVFLAG_HORZ_MENU					(1 << 5)
#define NVFLAG_CONSOLE_PCI_SERIAL		(1 << 7)

#define NV_STORE_VERSION					3

struct nv_store
{
	uint8_t	crc;		/* must be first */
	uint8_t	vers;
	uint8_t	size;

	uint8_t	flags;
	uint8_t	boot;
	uint8_t	baud;
	uint8_t	keymap;	/* added in version 3 */

} __attribute__((packed));

extern struct nv_store nv_store;

extern void nv_get(int);
extern void nv_put(void);

/* netcon.c */

extern int netcon_poll(void);
extern void netcon_disable(void);
extern unsigned netcon_read(void *, unsigned);
extern unsigned netcon_write(const void *, unsigned);
extern int netcon_enabled(void);

/* exec.c */

extern void clear_reloc(void);

#endif

/* vi:set ts=3 sw=3 cin path=include,../include: */
