/*
 * (C) P.Horton 2004
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

typedef unsigned				size_t;

typedef unsigned char		__u8;
typedef unsigned short		__u16;
typedef unsigned				__u32;
typedef unsigned long long	__u64;
typedef short					__s16;
typedef int						__s32;
typedef long long				__s64;

typedef unsigned				UWORD32;

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

extern void serial_init(void);
extern int kbhit(void);
extern int getch(void);
extern void putchar(int);
extern void putstring(const char *);
extern void puts(const char *str);
extern void drain(void);

#define BREAK()							({ char c; kbhit() && ((c = getch()) == ' ' || c == '\003'); })

/* history.c */

extern int history_add(const char *);
extern int history_fetch(char *, size_t, unsigned);
extern void history_discard(void);
extern unsigned history_count(void);

/* edit.c */

extern void line_edit(char *, size_t);

/* shell.c */

enum {
	E_NONE,
	E_UNSPEC,
	E_INVALID_CMND,
	E_ARGS_OVER,
	E_ARGS_UNDER,
	E_ARGS_COUNT,
	E_BAD_EXPR,
	E_BAD_VALUE,
};

extern void __attribute__((noreturn)) shell(const char *script);
extern int argv_add(const char *);

extern size_t argsz[];
extern unsigned argc;
extern char *argv[];

/* pci.c */

extern unsigned pci_init(size_t, size_t);

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

/* elf.c */

extern void *elf_check(const void *, size_t, size_t *);
extern void *elf_load(const void *, size_t);

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

/* ext2.c */

extern void *file_open(const char *, unsigned long *);
extern int file_load(void *, void *, unsigned long);

/* net.c */

extern int net_up(void);
extern void net_down(void);

#define net_is_up()							({ extern int net_alive; net_alive; })

static inline void yield(void)
{
	extern void tulip_poll(void);
	extern int net_alive;

	if(net_alive)
		tulip_poll();
}

/* lcd.c */

extern int lcd_menu(const char **, unsigned, unsigned, unsigned);

/* env.c */

#define VAR_OTHER								0
#define VAR_DHCP								1
#define VAR_INITRD							2

extern int env_put(const char *, const char *, unsigned);
extern const char *env_get(const char *);
extern void env_remove_tag(unsigned);

/* boot.c */

extern void __attribute__((noreturn)) boot(int);

/* nv.c */

#define NVFLAG_IDE_DISABLE_LBA			(1 << 0)
#define NVFLAG_IDE_DISABLE_LBA48			(1 << 1)
#define NVFLAG_IDE_DISABLE_TIMING		(1 << 2)
#define NVFLAG_IDE_ENABLE_SLAVE			(1 << 3)
#define NVFLAG_DISABLE_BOOT_MENU			(1 << 4)

#define NV_STORE_VERSION					1

struct nv_store
{
	uint8_t	vers;
	uint8_t	size;
	uint8_t	crc;

	uint8_t	flags;

} __attribute__((packed));

extern struct nv_store nv_store;

extern int nv_get(void);
extern void nv_put(void);

#endif

/* vi:set ts=3 sw=3 cin path=include,../include: */
