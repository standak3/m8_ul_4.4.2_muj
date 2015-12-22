/*
 *  include/asm-s390/processor.h
 *
 *  S390 version
 *    Copyright (C) 1999 IBM Deutschland Entwicklung GmbH, IBM Corporation
 *    Author(s): Hartmut Penner (hp@de.ibm.com),
 *               Martin Schwidefsky (schwidefsky@de.ibm.com)
 *
 *  Derived from "include/asm-i386/processor.h"
 *    Copyright (C) 1994, Linus Torvalds
 */

#ifndef __ASM_S390_PROCESSOR_H
#define __ASM_S390_PROCESSOR_H

#include <linux/linkage.h>
#include <linux/irqflags.h>
#include <asm/cpu.h>
#include <asm/page.h>
#include <asm/ptrace.h>
#include <asm/setup.h>

#ifdef __KERNEL__
#define current_text_addr() ({ void *pc; asm("basr %0,0" : "=a" (pc)); pc; })

static inline void get_cpu_id(struct cpuid *ptr)
{
	asm volatile("stidp %0" : "=Q" (*ptr));
}

extern void s390_adjust_jiffies(void);
extern int get_cpu_capability(unsigned int *);
extern const struct seq_operations cpuinfo_op;
extern int sysctl_ieee_emulation_warnings;

#ifndef __s390x__

#define TASK_SIZE		(1UL << 31)
#define TASK_UNMAPPED_BASE	(1UL << 30)

#else 

#define TASK_SIZE_OF(tsk)	((tsk)->mm->context.asce_limit)
#define TASK_UNMAPPED_BASE	(test_thread_flag(TIF_31BIT) ? \
					(1UL << 30) : (1UL << 41))
#define TASK_SIZE		TASK_SIZE_OF(current)

#endif 

#ifdef __KERNEL__

#ifndef __s390x__
#define STACK_TOP		(1UL << 31)
#define STACK_TOP_MAX		(1UL << 31)
#else 
#define STACK_TOP		(1UL << (test_thread_flag(TIF_31BIT) ? 31:42))
#define STACK_TOP_MAX		(1UL << 42)
#endif 


#endif

#define HAVE_ARCH_PICK_MMAP_LAYOUT

typedef struct {
        __u32 ar4;
} mm_segment_t;

struct thread_struct {
	s390_fp_regs fp_regs;
	unsigned int  acrs[NUM_ACRS];
        unsigned long ksp;              
	mm_segment_t mm_segment;
	unsigned long gmap_addr;	
	struct per_regs per_user;	
	struct per_event per_event;	
        
	unsigned long pfault_wait;
	struct list_head list;
};

typedef struct thread_struct thread_struct;

#ifndef __PACK_STACK
struct stack_frame {
	unsigned long back_chain;
	unsigned long empty1[5];
	unsigned long gprs[10];
	unsigned int  empty2[8];
};
#else
struct stack_frame {
	unsigned long empty1[5];
	unsigned int  empty2[8];
	unsigned long gprs[10];
	unsigned long back_chain;
};
#endif

#define ARCH_MIN_TASKALIGN	8

#define INIT_THREAD {							\
	.ksp = sizeof(init_stack) + (unsigned long) &init_stack,	\
}

#define start_thread(regs, new_psw, new_stackp) do {			\
	regs->psw.mask	= psw_user_bits | PSW_MASK_EA | PSW_MASK_BA;	\
	regs->psw.addr	= new_psw | PSW_ADDR_AMODE;			\
	regs->gprs[15]	= new_stackp;					\
} while (0)

#define start_thread31(regs, new_psw, new_stackp) do {			\
	regs->psw.mask	= psw_user_bits | PSW_MASK_BA;			\
	regs->psw.addr	= new_psw | PSW_ADDR_AMODE;			\
	regs->gprs[15]	= new_stackp;					\
	crst_table_downgrade(current->mm, 1UL << 31);			\
} while (0)

struct task_struct;
struct mm_struct;
struct seq_file;

extern void release_thread(struct task_struct *);
extern int kernel_thread(int (*fn)(void *), void * arg, unsigned long flags);

#define prepare_to_copy(tsk)	do { } while (0)

extern unsigned long thread_saved_pc(struct task_struct *t);

extern void show_code(struct pt_regs *regs);

unsigned long get_wchan(struct task_struct *p);
#define task_pt_regs(tsk) ((struct pt_regs *) \
        (task_stack_page(tsk) + THREAD_SIZE) - 1)
#define KSTK_EIP(tsk)	(task_pt_regs(tsk)->psw.addr)
#define KSTK_ESP(tsk)	(task_pt_regs(tsk)->gprs[15])

static inline unsigned short stap(void)
{
	unsigned short cpu_address;

	asm volatile("stap %0" : "=m" (cpu_address));
	return cpu_address;
}

static inline void cpu_relax(void)
{
	if (MACHINE_HAS_DIAG44)
		asm volatile("diag 0,0,68");
	barrier();
}

static inline void psw_set_key(unsigned int key)
{
	asm volatile("spka 0(%0)" : : "d" (key));
}

static inline void __load_psw(psw_t psw)
{
#ifndef __s390x__
	asm volatile("lpsw  %0" : : "Q" (psw) : "cc");
#else
	asm volatile("lpswe %0" : : "Q" (psw) : "cc");
#endif
}

static inline void __load_psw_mask (unsigned long mask)
{
	unsigned long addr;
	psw_t psw;

	psw.mask = mask;

#ifndef __s390x__
	asm volatile(
		"	basr	%0,0\n"
		"0:	ahi	%0,1f-0b\n"
		"	st	%0,%O1+4(%R1)\n"
		"	lpsw	%1\n"
		"1:"
		: "=&d" (addr), "=Q" (psw) : "Q" (psw) : "memory", "cc");
#else 
	asm volatile(
		"	larl	%0,1f\n"
		"	stg	%0,%O1+8(%R1)\n"
		"	lpswe	%1\n"
		"1:"
		: "=&d" (addr), "=Q" (psw) : "Q" (psw) : "memory", "cc");
#endif 
}

static inline unsigned long __rewind_psw(psw_t psw, unsigned long ilc)
{
#ifndef __s390x__
	if (psw.addr & PSW_ADDR_AMODE)
		
		return (psw.addr - ilc) | PSW_ADDR_AMODE;
	
	return (psw.addr - ilc) & ((1UL << 24) - 1);
#else
	unsigned long mask;

	mask = (psw.mask & PSW_MASK_EA) ? -1UL :
	       (psw.mask & PSW_MASK_BA) ? (1UL << 31) - 1 :
					  (1UL << 24) - 1;
	return (psw.addr - ilc) & mask;
#endif
}
 
static inline void __noreturn disabled_wait(unsigned long code)
{
        unsigned long ctl_buf;
        psw_t dw_psw;

	dw_psw.mask = PSW_MASK_BASE | PSW_MASK_WAIT | PSW_MASK_BA | PSW_MASK_EA;
        dw_psw.addr = code;
#ifndef __s390x__
	asm volatile(
		"	stctl	0,0,0(%2)\n"
		"	ni	0(%2),0xef\n"	
		"	lctl	0,0,0(%2)\n"
		"	stpt	0xd8\n"		
		"	stckc	0xe0\n"		
		"	stpx	0x108\n"	
		"	stam	0,15,0x120\n"	
		"	std	0,0x160\n"	
		"	std	2,0x168\n"	
		"	std	4,0x170\n"	
		"	std	6,0x178\n"	
		"	stm	0,15,0x180\n"	
		"	stctl	0,15,0x1c0\n"	
		"	oi	0x1c0,0x10\n"	
		"	lpsw	0(%1)"
		: "=m" (ctl_buf)
		: "a" (&dw_psw), "a" (&ctl_buf), "m" (dw_psw) : "cc");
#else 
	asm volatile(
		"	stctg	0,0,0(%2)\n"
		"	ni	4(%2),0xef\n"	
		"	lctlg	0,0,0(%2)\n"
		"	lghi	1,0x1000\n"
		"	stpt	0x328(1)\n"	
		"	stckc	0x330(1)\n"	
		"	stpx	0x318(1)\n"	
		"	stam	0,15,0x340(1)\n"
		"	stfpc	0x31c(1)\n"	
		"	std	0,0x200(1)\n"	
		"	std	1,0x208(1)\n"	
		"	std	2,0x210(1)\n"	
		"	std	3,0x218(1)\n"	
		"	std	4,0x220(1)\n"	
		"	std	5,0x228(1)\n"	
		"	std	6,0x230(1)\n"	
		"	std	7,0x238(1)\n"	
		"	std	8,0x240(1)\n"	
		"	std	9,0x248(1)\n"	
		"	std	10,0x250(1)\n"	
		"	std	11,0x258(1)\n"	
		"	std	12,0x260(1)\n"	
		"	std	13,0x268(1)\n"	
		"	std	14,0x270(1)\n"	
		"	std	15,0x278(1)\n"	
		"	stmg	0,15,0x280(1)\n"
		"	stctg	0,15,0x380(1)\n"
		"	oi	0x384(1),0x10\n"
		"	lpswe	0(%1)"
		: "=m" (ctl_buf)
		: "a" (&dw_psw), "a" (&ctl_buf), "m" (dw_psw) : "cc", "0", "1");
#endif 
	while (1);
}

static inline void
__set_psw_mask(unsigned long mask)
{
	__load_psw_mask(mask | (arch_local_save_flags() & ~(-1UL >> 8)));
}

#define local_mcck_enable() \
	__set_psw_mask(psw_kernel_bits | PSW_MASK_DAT | PSW_MASK_MCHECK)
#define local_mcck_disable() \
	__set_psw_mask(psw_kernel_bits | PSW_MASK_DAT)


extern void s390_base_mcck_handler(void);
extern void s390_base_pgm_handler(void);
extern void s390_base_ext_handler(void);

extern void (*s390_base_mcck_handler_fn)(void);
extern void (*s390_base_pgm_handler_fn)(void);
extern void (*s390_base_ext_handler_fn)(void);

#define ARCH_LOW_ADDRESS_LIMIT	0x7fffffffUL

#endif

#ifndef __s390x__
#define EX_TABLE(_fault,_target)			\
	".section __ex_table,\"a\"\n"			\
	"	.align 4\n"				\
	"	.long  " #_fault "," #_target "\n"	\
	".previous\n"
#else
#define EX_TABLE(_fault,_target)			\
	".section __ex_table,\"a\"\n"			\
	"	.align 8\n"				\
	"	.quad  " #_fault "," #_target "\n"	\
	".previous\n"
#endif

#endif                                 