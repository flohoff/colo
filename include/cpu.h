/*
 * (C) P.Horton 2004
 *
 * $Id$
 *
 * This code is covered by the GNU General Public License. For details see the file "COPYING".
 */

#ifndef _CPU_H_
#define _CPU_H_

#define CPU_CLOCK_DEFAULT				(250 * 1000 * 1000)
#define CP0_COUNT_RATE_DEFAULT		(CPU_CLOCK_DEFAULT / 2)

#ifdef CP0_COUNT_RATE_FIXED
# define CP0_COUNT_RATE					CP0_COUNT_RATE_DEFAULT
#else
# define CP0_COUNT_RATE					({extern unsigned cp0_count_freq;cp0_count_freq;})
#endif

#define CP0_INDEX							0
#define CP0_ENTRYLO0						2
#define CP0_ENTRYLO1						3
#define CP0_BADVADDR						8
#define CP0_COUNT							9
#define CP0_ENTRYHI						10
#define CP0_COMPARE						11
#define CP0_STATUS						12
# define CP0_STATUS_IE					(1 << 0)
# define CP0_STATUS_ERL					(1 << 2)
# define CP0_STATUS_KX					(1 << 7)
# define CP0_STATUS_IM(n)				(1 << (8 + (n)))
# define CP0_STATUS_BEV					(1 << 22)
# define CP0_STATUS_DL					(1 << 24)
#define CP0_CAUSE							13
# define CP0_CAUSE_IV					(1 << 23)
# define CP0_CAUSE_BD					(1 << 31)
#define CP0_EPC							14
#define CP0_CONFIG						16
# define CP0_CONFIG_K0_WRITEBACK		(3 << 0)
#define CP0_TAGLO							28

#define DCACHE_TOTAL_SIZE				(32 << 10)
#define DCACHE_LINE_SIZE				32
#define DCACHE_WAY_COUNT				2

#define ICACHE_TOTAL_SIZE				(32 << 10)
#define ICACHE_LINE_SIZE				32
#define ICACHE_WAY_COUNT				2

#define CACHE_IndexInvalidateI		((0 << 2) | 0)
#define CACHE_IndexWritebackInvD		((0 << 2) | 1)
#define CACHE_IndexStoreTagI			((2 << 2) | 0)
#define CACHE_IndexStoreTagD			((2 << 2) | 1)
#define CACHE_HitWritebackInvD		((5 << 2) | 1)

#define TLB_ENTRY_COUNT					48

#define _MFC0(n)							({uint32_t v;asm volatile("mfc0 %0,$"#n:"=r"(v));v;})
#define _MTC0(n,v)						do{asm volatile("mtc0 %0,$"#n::"r"(v));}while(0)
#define _CACHE(o,p)						do{asm volatile(".set mips3\ncache "#o",0(%0)\n.set mips0"::"r"(p));}while(0)
#define NOP()								do{asm volatile("nop");}while(0)
#define TLBWI()							do{asm volatile("tlbwi");}while(0)

#define MFC0(n)							_MFC0(n)
#define MTC0(n,v)							_MTC0(n,v)
#define CACHE(o,p)						_CACHE(o,p)

#define KPHYS(a)							((void *)((unsigned long)(a)&0x1fffffff))
#define KSEG0(a)							((void *)((unsigned long)KPHYS(a)|0x80000000))
#define KSEG1(a)							((void *)((unsigned long)KPHYS(a)|0xa0000000))
#define KSEG1SIZE							0x20000000

static inline void udelay(unsigned delay)
{
	unsigned mark;

	delay *= (CP0_COUNT_RATE + 500000) / 1000000;

	for(mark = MFC0(CP0_COUNT); MFC0(CP0_COUNT) - mark < delay;)
		;
}

static inline unsigned unaligned_load(void *addr)
{
	struct unaligned { unsigned word; } __attribute__((packed));

	return ((struct unaligned *) addr)->word;
}

static inline void unaligned_store(void *addr, unsigned data)
{
	struct unaligned { unsigned word; } __attribute__((packed));

	((struct unaligned *) addr)->word = data;
}

#endif

/* vi:set ts=3 sw=3 cin path=include,../include: */
