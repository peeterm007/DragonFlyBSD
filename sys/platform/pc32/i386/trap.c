/*-
 * Copyright (C) 1994, David Greenman
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the University of Utah, and William Jolitz.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)trap.c	7.4 (Berkeley) 5/13/91
 * $FreeBSD: src/sys/i386/i386/trap.c,v 1.147.2.11 2003/02/27 19:09:59 luoqi Exp $
 * $DragonFly: src/sys/platform/pc32/i386/trap.c,v 1.47 2004/03/28 08:03:05 dillon Exp $
 */

/*
 * 386 Trap and System call handling
 */

#include "use_isa.h"
#include "use_npx.h"

#include "opt_cpu.h"
#include "opt_ddb.h"
#include "opt_ktrace.h"
#include "opt_clock.h"
#include "opt_trap.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/pioctl.h>
#include <sys/kernel.h>
#include <sys/resourcevar.h>
#include <sys/signalvar.h>
#include <sys/syscall.h>
#include <sys/sysctl.h>
#include <sys/sysent.h>
#include <sys/uio.h>
#include <sys/vmmeter.h>
#include <sys/malloc.h>
#ifdef KTRACE
#include <sys/ktrace.h>
#endif
#include <sys/upcall.h>
#include <sys/sysproto.h>
#include <sys/sysunion.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <sys/lock.h>
#include <vm/pmap.h>
#include <vm/vm_kern.h>
#include <vm/vm_map.h>
#include <vm/vm_page.h>
#include <vm/vm_extern.h>

#include <machine/cpu.h>
#include <machine/ipl.h>
#include <machine/md_var.h>
#include <machine/pcb.h>
#ifdef SMP
#include <machine/smp.h>
#endif
#include <machine/tss.h>
#include <machine/globaldata.h>

#include <i386/isa/intr_machdep.h>

#ifdef POWERFAIL_NMI
#include <sys/syslog.h>
#include <machine/clock.h>
#endif

#include <machine/vm86.h>

#include <ddb/ddb.h>
#include <sys/msgport2.h>
#include <sys/thread2.h>

int (*pmath_emulate) (struct trapframe *);

extern void trap (struct trapframe frame);
extern int trapwrite (unsigned addr);
extern void syscall2 (struct trapframe frame);
extern void sendsys2 (struct trapframe frame);

static int trap_pfault (struct trapframe *, int, vm_offset_t);
static void trap_fatal (struct trapframe *, vm_offset_t);
void dblfault_handler (void);

extern inthand_t IDTVEC(syscall);

#define MAX_TRAP_MSG		28
static char *trap_msg[] = {
	"",					/*  0 unused */
	"privileged instruction fault",		/*  1 T_PRIVINFLT */
	"",					/*  2 unused */
	"breakpoint instruction fault",		/*  3 T_BPTFLT */
	"",					/*  4 unused */
	"",					/*  5 unused */
	"arithmetic trap",			/*  6 T_ARITHTRAP */
	"system forced exception",		/*  7 T_ASTFLT */
	"",					/*  8 unused */
	"general protection fault",		/*  9 T_PROTFLT */
	"trace trap",				/* 10 T_TRCTRAP */
	"",					/* 11 unused */
	"page fault",				/* 12 T_PAGEFLT */
	"",					/* 13 unused */
	"alignment fault",			/* 14 T_ALIGNFLT */
	"",					/* 15 unused */
	"",					/* 16 unused */
	"",					/* 17 unused */
	"integer divide fault",			/* 18 T_DIVIDE */
	"non-maskable interrupt trap",		/* 19 T_NMI */
	"overflow trap",			/* 20 T_OFLOW */
	"FPU bounds check fault",		/* 21 T_BOUND */
	"FPU device not available",		/* 22 T_DNA */
	"double fault",				/* 23 T_DOUBLEFLT */
	"FPU operand fetch fault",		/* 24 T_FPOPFLT */
	"invalid TSS fault",			/* 25 T_TSSFLT */
	"segment not present fault",		/* 26 T_SEGNPFLT */
	"stack fault",				/* 27 T_STKFLT */
	"machine check trap",			/* 28 T_MCHK */
};

#if defined(I586_CPU) && !defined(NO_F00F_HACK)
extern int has_f00f_bug;
#endif

#ifdef DDB
static int ddb_on_nmi = 1;
SYSCTL_INT(_machdep, OID_AUTO, ddb_on_nmi, CTLFLAG_RW,
	&ddb_on_nmi, 0, "Go to DDB on NMI");
#endif
static int panic_on_nmi = 1;
SYSCTL_INT(_machdep, OID_AUTO, panic_on_nmi, CTLFLAG_RW,
	&panic_on_nmi, 0, "Panic on NMI");
static int fast_release;
SYSCTL_INT(_machdep, OID_AUTO, fast_release, CTLFLAG_RW,
	&fast_release, 0, "Passive Release was optimal");
static int slow_release;
SYSCTL_INT(_machdep, OID_AUTO, slow_release, CTLFLAG_RW,
	&slow_release, 0, "Passive Release was nonoptimal");
static int pass_release;
SYSCTL_INT(_machdep, OID_AUTO, pass_release, CTLFLAG_RW,
	&pass_release, 0, "Passive Release on switch");

MALLOC_DEFINE(M_SYSMSG, "sysmsg", "sysmsg structure");

/*
 * USER->KERNEL transition.  Do not transition us out of userland from the
 * point of view of the userland scheduler unless we actually have to
 * switch.  Switching typically occurs when a process blocks in the kernel.
 *
 * passive_release is called from within a critical section and the BGL will
 * still be held.  This function is NOT called for preemptions, only for
 * switchouts.  Note that other elements of the system (uio_yield()) assume
 * that the user cruft will be released when lwkt_switch() is called.
 */
static void
passive_release(struct thread *td)
{
	struct proc *p = td->td_proc;

	td->td_release = NULL;

	/*
	 * P_CP_RELEASED prevents the userland scheduler from messing with
	 * this proc.
	 */
	if ((p->p_flag & P_CP_RELEASED) == 0) {
		p->p_flag |= P_CP_RELEASED;
		lwkt_setpri_self(TDPRI_KERN_USER);
	}

	/*
	 * Only one process will have a P_CURPROC designation for each cpu
	 * in the system.  Releasing it allows another userland process to
	 * be scheduled in case our thread blocks in the kernel.
	 */
	if (p->p_flag & P_CURPROC) {
		release_curproc(p);
		++pass_release;
	}
}

/*
 * userenter() passively intercepts the thread switch function to increase
 * the thread priority from a user priority to a kernel priority, reducing
 * syscall and trap overhead for the case where no switch occurs.
 */

static __inline void
userenter(struct thread *curtd)
{
	curtd->td_release = passive_release;
}

static __inline void
userexit(struct proc *p)
{
	struct thread *td = p->p_thread;

	/*
	 * Reacquire our P_CURPROC status and adjust the LWKT priority
	 * for our return to userland.  We can fast path the case where
	 * td_release was not called by checking particular proc flags.
	 * Otherwise we do it the slow way.
	 *
	 * Lowering our priority may make other higher priority threads
	 * runnable. lwkt_setpri_self() does not switch away, so call
	 * lwkt_maybe_switch() to deal with it.  We do this *before* we
	 * acquire P_CURPROC because another thread may also be intending
	 * to return to userland and if it has a higher user priority then
	 * us it will have to block and force us to reschedule, resulting in
	 * unnecessary extra context switches.
	 *
	 * WARNING!  Once our priority is lowered to a user level priority
	 * it is possible, once we return to user mode (or if we were to
	 * block) for a cpu-bound user process to prevent us from getting cpu
	 * again.  This is always the last step.
	 */
	td->td_release = NULL;
	if ((p->p_flag & (P_CP_RELEASED|P_CURPROC)) == P_CURPROC) {
		++fast_release;
		lwkt_maybe_switch();
	} else {
		++slow_release;
		lwkt_setpri_self(TDPRI_USER_NORM);
		lwkt_maybe_switch();
		acquire_curproc(p);
#if 0
		/* POSSIBLE FUTURE */
		switch(p->p_rtprio.type) {
		case RTP_PRIO_IDLE:
			lwkt_setpri_self(TDPRI_USER_IDLE);
			break;
		case RTP_PRIO_REALTIME:
		case RTP_PRIO_FIFO:
			lwkt_setpri_self(TDPRI_USER_REAL);
			break;
		default:
			lwkt_setpri_self(TDPRI_USER_NORM);
			break;
		}
#endif
	}
}


static void
userret(struct proc *p, struct trapframe *frame, u_quad_t oticks)
{
	int sig;

	/*
	 * Post any pending upcalls
	 */
	if (p->p_flag & P_UPCALLPEND) {
		p->p_flag &= ~P_UPCALLPEND;
		postupcall(p);
	}

	/*
	 * Post any pending signals
	 */
	while ((sig = CURSIG(p)) != 0) {
		postsig(sig);
	}

	/*
	 * If a reschedule has been requested then we release the current
	 * process in order to shift our P_CURPROC designation to another
	 * user process.  userexit() will reacquire P_CURPROC and block
	 * there.
	 */
	if (resched_wanted()) {
		p->p_thread->td_release = NULL;
		if ((p->p_flag & P_CP_RELEASED) == 0) {
			p->p_flag |= P_CP_RELEASED;
			lwkt_setpri_self(TDPRI_KERN_USER);
		}
		if (p->p_flag & P_CURPROC) {
			release_curproc(p);
		} else {
			clear_resched();
		}
	}

	/*
	 * Charge system time if profiling.  Note: times are in microseconds.
	 */
	if (p->p_flag & P_PROFIL) {
		addupc_task(p, frame->tf_eip, 
		    (u_int)(curthread->td_sticks - oticks));
	}

	/*
	 * Post any pending signals XXX
	 */
	while ((sig = CURSIG(p)) != 0)
		postsig(sig);
}

#ifdef DEVICE_POLLING
extern u_int32_t poll_in_trap;
extern int ether_poll (int count);
#endif /* DEVICE_POLLING */

/*
 * Exception, fault, and trap interface to the FreeBSD kernel.
 * This common code is called from assembly language IDT gate entry
 * routines that prepare a suitable stack frame, and restore this
 * frame after the exception has been processed.
 *
 * This function is also called from doreti in an interlock to handle ASTs.
 * For example:  hardwareint->INTROUTINE->(set ast)->doreti->trap
 *
 * NOTE!  We have to retrieve the fault address prior to obtaining the
 * MP lock because get_mplock() may switch out.  YYY cr2 really ought
 * to be retrieved by the assembly code, not here.
 */
void
trap(frame)
	struct trapframe frame;
{
	struct thread *td = curthread;
	struct proc *p;
	u_quad_t sticks = 0;
	int i = 0, ucode = 0, type, code;
	vm_offset_t eva;

	p = td->td_proc;
#ifdef DDB
	if (db_active) {
		eva = (frame.tf_trapno == T_PAGEFLT ? rcr2() : 0);
		get_mplock();
		trap_fatal(&frame, eva);
		goto out2;
	}
#endif

	eva = 0;
	if (frame.tf_trapno == T_PAGEFLT) {
		/*
		 * For some Cyrix CPUs, %cr2 is clobbered by interrupts.
		 * This problem is worked around by using an interrupt
		 * gate for the pagefault handler.  We are finally ready
		 * to read %cr2 and then must reenable interrupts.
		 *
		 * XXX this should be in the switch statement, but the
		 * NO_FOOF_HACK and VM86 goto and ifdefs obfuscate the
		 * flow of control too much for this to be obviously
		 * correct.
		 */
		eva = rcr2();
		get_mplock();
		cpu_enable_intr();
	} else {
		get_mplock();
	}
	/*
	 * MP lock is held at this point
	 */

	if (!(frame.tf_eflags & PSL_I)) {
		/*
		 * Buggy application or kernel code has disabled interrupts
		 * and then trapped.  Enabling interrupts now is wrong, but
		 * it is better than running with interrupts disabled until
		 * they are accidentally enabled later.
		 */
		type = frame.tf_trapno;
		if (ISPL(frame.tf_cs)==SEL_UPL || (frame.tf_eflags & PSL_VM)) {
			printf(
			    "pid %ld (%s): trap %d with interrupts disabled\n",
			    (long)curproc->p_pid, curproc->p_comm, type);
		} else if (type != T_BPTFLT && type != T_TRCTRAP) {
			/*
			 * XXX not quite right, since this may be for a
			 * multiple fault in user mode.
			 */
			printf("kernel trap %d with interrupts disabled\n",
			    type);
		}
		cpu_enable_intr();
	}


#ifdef DEVICE_POLLING
	if (poll_in_trap)
		ether_poll(poll_in_trap);
#endif /* DEVICE_POLLING */

#if defined(I586_CPU) && !defined(NO_F00F_HACK)
restart:
#endif
	type = frame.tf_trapno;
	code = frame.tf_err;

	if (in_vm86call) {
		if (frame.tf_eflags & PSL_VM &&
		    (type == T_PROTFLT || type == T_STKFLT)) {
#ifdef SMP
			KKASSERT(curthread->td_mpcount > 0);
#endif
			i = vm86_emulate((struct vm86frame *)&frame);
#ifdef SMP
			KKASSERT(curthread->td_mpcount > 0);
#endif
			if (i != 0) {
				/*
				 * returns to original process
				 */
				vm86_trap((struct vm86frame *)&frame);
				KKASSERT(0);
			}
			goto out2;
		}
		switch (type) {
			/*
			 * these traps want either a process context, or
			 * assume a normal userspace trap.
			 */
		case T_PROTFLT:
		case T_SEGNPFLT:
			trap_fatal(&frame, eva);
			goto out2;
		case T_TRCTRAP:
			type = T_BPTFLT;	/* kernel breakpoint */
			/* FALL THROUGH */
		}
		goto kernel_trap;	/* normal kernel trap handling */
	}

        if ((ISPL(frame.tf_cs) == SEL_UPL) || (frame.tf_eflags & PSL_VM)) {
		/* user trap */

		userenter(td);

		sticks = curthread->td_sticks;
		p->p_md.md_regs = &frame;

		switch (type) {
		case T_PRIVINFLT:	/* privileged instruction fault */
			ucode = type;
			i = SIGILL;
			break;

		case T_BPTFLT:		/* bpt instruction fault */
		case T_TRCTRAP:		/* trace trap */
			frame.tf_eflags &= ~PSL_T;
			i = SIGTRAP;
			break;

		case T_ARITHTRAP:	/* arithmetic trap */
			ucode = code;
			i = SIGFPE;
			break;

		case T_ASTFLT:		/* Allow process switch */
			mycpu->gd_cnt.v_soft++;
			if (mycpu->gd_reqflags & RQF_AST_OWEUPC) {
				atomic_clear_int_nonlocked(&mycpu->gd_reqflags,
					    RQF_AST_OWEUPC);
				addupc_task(p, p->p_stats->p_prof.pr_addr,
					    p->p_stats->p_prof.pr_ticks);
			}
			goto out;

			/*
			 * The following two traps can happen in
			 * vm86 mode, and, if so, we want to handle
			 * them specially.
			 */
		case T_PROTFLT:		/* general protection fault */
		case T_STKFLT:		/* stack fault */
			if (frame.tf_eflags & PSL_VM) {
				i = vm86_emulate((struct vm86frame *)&frame);
				if (i == 0)
					goto out;
				break;
			}
			/* FALL THROUGH */

		case T_SEGNPFLT:	/* segment not present fault */
		case T_TSSFLT:		/* invalid TSS fault */
		case T_DOUBLEFLT:	/* double fault */
		default:
			ucode = code + BUS_SEGM_FAULT ;
			i = SIGBUS;
			break;

		case T_PAGEFLT:		/* page fault */
			i = trap_pfault(&frame, TRUE, eva);
			if (i == -1)
				goto out;
#if defined(I586_CPU) && !defined(NO_F00F_HACK)
			if (i == -2)
				goto restart;
#endif
			if (i == 0)
				goto out;

			ucode = T_PAGEFLT;
			break;

		case T_DIVIDE:		/* integer divide fault */
			ucode = FPE_INTDIV;
			i = SIGFPE;
			break;

#if NISA > 0
		case T_NMI:
#ifdef POWERFAIL_NMI
			goto handle_powerfail;
#else /* !POWERFAIL_NMI */
			/* machine/parity/power fail/"kitchen sink" faults */
			if (isa_nmi(code) == 0) {
#ifdef DDB
				/*
				 * NMI can be hooked up to a pushbutton
				 * for debugging.
				 */
				if (ddb_on_nmi) {
					printf ("NMI ... going to debugger\n");
					kdb_trap (type, 0, &frame);
				}
#endif /* DDB */
				goto out2;
			} else if (panic_on_nmi)
				panic("NMI indicates hardware failure");
			break;
#endif /* POWERFAIL_NMI */
#endif /* NISA > 0 */

		case T_OFLOW:		/* integer overflow fault */
			ucode = FPE_INTOVF;
			i = SIGFPE;
			break;

		case T_BOUND:		/* bounds check fault */
			ucode = FPE_FLTSUB;
			i = SIGFPE;
			break;

		case T_DNA:
#if NNPX > 0
			/* if a transparent fault (due to context switch "late") */
			if (npxdna())
				goto out;
#endif
			if (!pmath_emulate) {
				i = SIGFPE;
				ucode = FPE_FPU_NP_TRAP;
				break;
			}
			i = (*pmath_emulate)(&frame);
			if (i == 0) {
				if (!(frame.tf_eflags & PSL_T))
					goto out2;
				frame.tf_eflags &= ~PSL_T;
				i = SIGTRAP;
			}
			/* else ucode = emulator_only_knows() XXX */
			break;

		case T_FPOPFLT:		/* FPU operand fetch fault */
			ucode = T_FPOPFLT;
			i = SIGILL;
			break;

		case T_XMMFLT:		/* SIMD floating-point exception */
			ucode = 0; /* XXX */
			i = SIGFPE;
			break;
		}
	} else {
kernel_trap:
		/* kernel trap */

		switch (type) {
		case T_PAGEFLT:			/* page fault */
			(void) trap_pfault(&frame, FALSE, eva);
			goto out2;

		case T_DNA:
#if NNPX > 0
			/*
			 * The kernel is apparently using npx for copying.
			 * XXX this should be fatal unless the kernel has
			 * registered such use.
			 */
			if (npxdna())
				goto out2;
#endif
			break;

		case T_PROTFLT:		/* general protection fault */
		case T_SEGNPFLT:	/* segment not present fault */
			/*
			 * Invalid segment selectors and out of bounds
			 * %eip's and %esp's can be set up in user mode.
			 * This causes a fault in kernel mode when the
			 * kernel tries to return to user mode.  We want
			 * to get this fault so that we can fix the
			 * problem here and not have to check all the
			 * selectors and pointers when the user changes
			 * them.
			 */
#define	MAYBE_DORETI_FAULT(where, whereto)				\
	do {								\
		if (frame.tf_eip == (int)where) {			\
			frame.tf_eip = (int)whereto;			\
			goto out2;					\
		}							\
	} while (0)
			/*
			 * Since we don't save %gs across an interrupt
			 * frame this check must occur outside the intr
			 * nesting level check.
			 */
			if (frame.tf_eip == (int)cpu_switch_load_gs) {
				curthread->td_pcb->pcb_gs = 0;
				psignal(p, SIGBUS);
				goto out2;
			}
			if (mycpu->gd_intr_nesting_level == 0) {
				/*
				 * Invalid %fs's and %gs's can be created using
				 * procfs or PT_SETREGS or by invalidating the
				 * underlying LDT entry.  This causes a fault
				 * in kernel mode when the kernel attempts to
				 * switch contexts.  Lose the bad context
				 * (XXX) so that we can continue, and generate
				 * a signal.
				 */
				MAYBE_DORETI_FAULT(doreti_iret,
						   doreti_iret_fault);
				MAYBE_DORETI_FAULT(doreti_popl_ds,
						   doreti_popl_ds_fault);
				MAYBE_DORETI_FAULT(doreti_popl_es,
						   doreti_popl_es_fault);
				MAYBE_DORETI_FAULT(doreti_popl_fs,
						   doreti_popl_fs_fault);
				if (curthread->td_pcb->pcb_onfault) {
					frame.tf_eip = (int)curthread->td_pcb->pcb_onfault;
					goto out2;
				}
			}
			break;

		case T_TSSFLT:
			/*
			 * PSL_NT can be set in user mode and isn't cleared
			 * automatically when the kernel is entered.  This
			 * causes a TSS fault when the kernel attempts to
			 * `iret' because the TSS link is uninitialized.  We
			 * want to get this fault so that we can fix the
			 * problem here and not every time the kernel is
			 * entered.
			 */
			if (frame.tf_eflags & PSL_NT) {
				frame.tf_eflags &= ~PSL_NT;
				goto out2;
			}
			break;

		case T_TRCTRAP:	 /* trace trap */
			if (frame.tf_eip == (int)IDTVEC(syscall)) {
				/*
				 * We've just entered system mode via the
				 * syscall lcall.  Continue single stepping
				 * silently until the syscall handler has
				 * saved the flags.
				 */
				goto out2;
			}
			if (frame.tf_eip == (int)IDTVEC(syscall) + 1) {
				/*
				 * The syscall handler has now saved the
				 * flags.  Stop single stepping it.
				 */
				frame.tf_eflags &= ~PSL_T;
				goto out2;
			}
                        /*
                         * Ignore debug register trace traps due to
                         * accesses in the user's address space, which
                         * can happen under several conditions such as
                         * if a user sets a watchpoint on a buffer and
                         * then passes that buffer to a system call.
                         * We still want to get TRCTRAPS for addresses
                         * in kernel space because that is useful when
                         * debugging the kernel.
                         */
                        if (user_dbreg_trap()) {
                                /*
                                 * Reset breakpoint bits because the
                                 * processor doesn't
                                 */
                                load_dr6(rdr6() & 0xfffffff0);
                                goto out2;
                        }
			/*
			 * Fall through (TRCTRAP kernel mode, kernel address)
			 */
		case T_BPTFLT:
			/*
			 * If DDB is enabled, let it handle the debugger trap.
			 * Otherwise, debugger traps "can't happen".
			 */
#ifdef DDB
			if (kdb_trap (type, 0, &frame))
				goto out2;
#endif
			break;

#if NISA > 0
		case T_NMI:
#ifdef POWERFAIL_NMI
#ifndef TIMER_FREQ
#  define TIMER_FREQ 1193182
#endif
	handle_powerfail:
		{
		  static unsigned lastalert = 0;

		  if(time_second - lastalert > 10)
		    {
		      log(LOG_WARNING, "NMI: power fail\n");
		      sysbeep(TIMER_FREQ/880, hz);
		      lastalert = time_second;
		    }
		    /* YYY mp count */
		  goto out2;
		}
#else /* !POWERFAIL_NMI */
			/* machine/parity/power fail/"kitchen sink" faults */
			if (isa_nmi(code) == 0) {
#ifdef DDB
				/*
				 * NMI can be hooked up to a pushbutton
				 * for debugging.
				 */
				if (ddb_on_nmi) {
					printf ("NMI ... going to debugger\n");
					kdb_trap (type, 0, &frame);
				}
#endif /* DDB */
				goto out2;
			} else if (panic_on_nmi == 0)
				goto out2;
			/* FALL THROUGH */
#endif /* POWERFAIL_NMI */
#endif /* NISA > 0 */
		}

		trap_fatal(&frame, eva);
		goto out2;
	}

	/* Translate fault for emulators (e.g. Linux) */
	if (*p->p_sysent->sv_transtrap)
		i = (*p->p_sysent->sv_transtrap)(i, type);

	trapsignal(p, i, ucode);

#ifdef DEBUG
	if (type <= MAX_TRAP_MSG) {
		uprintf("fatal process exception: %s",
			trap_msg[type]);
		if ((type == T_PAGEFLT) || (type == T_PROTFLT))
			uprintf(", fault VA = 0x%lx", (u_long)eva);
		uprintf("\n");
	}
#endif

out:
#ifdef SMP
        if (ISPL(frame.tf_cs) == SEL_UPL)
		KASSERT(curthread->td_mpcount == 1, ("badmpcount trap from %p", (void *)frame.tf_eip));
#endif
	userret(p, &frame, sticks);
	userexit(p);
out2:
#ifdef SMP
	KKASSERT(curthread->td_mpcount > 0);
#endif
	rel_mplock();
}

#ifdef notyet
/*
 * This version doesn't allow a page fault to user space while
 * in the kernel. The rest of the kernel needs to be made "safe"
 * before this can be used. I think the only things remaining
 * to be made safe are the iBCS2 code and the process tracing/
 * debugging code.
 */
static int
trap_pfault(frame, usermode, eva)
	struct trapframe *frame;
	int usermode;
	vm_offset_t eva;
{
	vm_offset_t va;
	struct vmspace *vm = NULL;
	vm_map_t map = 0;
	int rv = 0;
	vm_prot_t ftype;
	struct proc *p = curproc;

	if (frame->tf_err & PGEX_W)
		ftype = VM_PROT_WRITE;
	else
		ftype = VM_PROT_READ;

	va = trunc_page(eva);
	if (va < VM_MIN_KERNEL_ADDRESS) {
		vm_offset_t v;
		vm_page_t mpte;

		if (p == NULL ||
		    (!usermode && va < VM_MAXUSER_ADDRESS &&
		     (mycpu->gd_intr_nesting_level != 0 || 
		      curthread->td_pcb->pcb_onfault == NULL))) {
			trap_fatal(frame, eva);
			return (-1);
		}

		/*
		 * This is a fault on non-kernel virtual memory.
		 * vm is initialized above to NULL. If curproc is NULL
		 * or curproc->p_vmspace is NULL the fault is fatal.
		 */
		vm = p->p_vmspace;
		if (vm == NULL)
			goto nogo;

		map = &vm->vm_map;

		/*
		 * Keep swapout from messing with us during this
		 *	critical time.
		 */
		++p->p_lock;

		/*
		 * Grow the stack if necessary
		 */
		/* grow_stack returns false only if va falls into
		 * a growable stack region and the stack growth
		 * fails.  It returns true if va was not within
		 * a growable stack region, or if the stack 
		 * growth succeeded.
		 */
		if (!grow_stack (p, va)) {
			rv = KERN_FAILURE;
			--p->p_lock;
			goto nogo;
		}
		
		/* Fault in the user page: */
		rv = vm_fault(map, va, ftype,
			      (ftype & VM_PROT_WRITE) ? VM_FAULT_DIRTY
						      : VM_FAULT_NORMAL);

		--p->p_lock;
	} else {
		/*
		 * Don't allow user-mode faults in kernel address space.
		 */
		if (usermode)
			goto nogo;

		/*
		 * Since we know that kernel virtual address addresses
		 * always have pte pages mapped, we just have to fault
		 * the page.
		 */
		rv = vm_fault(kernel_map, va, ftype, VM_FAULT_NORMAL);
	}

	if (rv == KERN_SUCCESS)
		return (0);
nogo:
	if (!usermode) {
		if (mycpu->gd_intr_nesting_level == 0 && curthread->td_pcb->pcb_onfault) {
			frame->tf_eip = (int)curthread->td_pcb->pcb_onfault;
			return (0);
		}
		trap_fatal(frame, eva);
		return (-1);
	}

	/* kludge to pass faulting virtual address to sendsig */
	frame->tf_err = eva;

	return((rv == KERN_PROTECTION_FAILURE) ? SIGBUS : SIGSEGV);
}
#endif

int
trap_pfault(frame, usermode, eva)
	struct trapframe *frame;
	int usermode;
	vm_offset_t eva;
{
	vm_offset_t va;
	struct vmspace *vm = NULL;
	vm_map_t map = 0;
	int rv = 0;
	vm_prot_t ftype;
	struct proc *p = curproc;

	va = trunc_page(eva);
	if (va >= KERNBASE) {
		/*
		 * Don't allow user-mode faults in kernel address space.
		 * An exception:  if the faulting address is the invalid
		 * instruction entry in the IDT, then the Intel Pentium
		 * F00F bug workaround was triggered, and we need to
		 * treat it is as an illegal instruction, and not a page
		 * fault.
		 */
#if defined(I586_CPU) && !defined(NO_F00F_HACK)
		if ((eva == (unsigned int)&idt[6]) && has_f00f_bug) {
			frame->tf_trapno = T_PRIVINFLT;
			return -2;
		}
#endif
		if (usermode)
			goto nogo;

		map = kernel_map;
	} else {
		/*
		 * This is a fault on non-kernel virtual memory.
		 * vm is initialized above to NULL. If curproc is NULL
		 * or curproc->p_vmspace is NULL the fault is fatal.
		 */
		if (p != NULL)
			vm = p->p_vmspace;

		if (vm == NULL)
			goto nogo;

		map = &vm->vm_map;
	}

	if (frame->tf_err & PGEX_W)
		ftype = VM_PROT_WRITE;
	else
		ftype = VM_PROT_READ;

	if (map != kernel_map) {
		/*
		 * Keep swapout from messing with us during this
		 *	critical time.
		 */
		++p->p_lock;

		/*
		 * Grow the stack if necessary
		 */
		/* grow_stack returns false only if va falls into
		 * a growable stack region and the stack growth
		 * fails.  It returns true if va was not within
		 * a growable stack region, or if the stack 
		 * growth succeeded.
		 */
		if (!grow_stack (p, va)) {
			rv = KERN_FAILURE;
			--p->p_lock;
			goto nogo;
		}

		/* Fault in the user page: */
		rv = vm_fault(map, va, ftype,
			      (ftype & VM_PROT_WRITE) ? VM_FAULT_DIRTY
						      : VM_FAULT_NORMAL);

		--p->p_lock;
	} else {
		/*
		 * Don't have to worry about process locking or stacks in the kernel.
		 */
		rv = vm_fault(map, va, ftype, VM_FAULT_NORMAL);
	}

	if (rv == KERN_SUCCESS)
		return (0);
nogo:
	if (!usermode) {
		if (mycpu->gd_intr_nesting_level == 0 && curthread->td_pcb->pcb_onfault) {
			frame->tf_eip = (int)curthread->td_pcb->pcb_onfault;
			return (0);
		}
		trap_fatal(frame, eva);
		return (-1);
	}

	/* kludge to pass faulting virtual address to sendsig */
	frame->tf_err = eva;

	return((rv == KERN_PROTECTION_FAILURE) ? SIGBUS : SIGSEGV);
}

static void
trap_fatal(frame, eva)
	struct trapframe *frame;
	vm_offset_t eva;
{
	int code, type, ss, esp;
	struct soft_segment_descriptor softseg;

	code = frame->tf_err;
	type = frame->tf_trapno;
	sdtossd(&gdt[mycpu->gd_cpuid * NGDT + IDXSEL(frame->tf_cs & 0xffff)].sd, &softseg);

	if (type <= MAX_TRAP_MSG)
		printf("\n\nFatal trap %d: %s while in %s mode\n",
			type, trap_msg[type],
        		frame->tf_eflags & PSL_VM ? "vm86" :
			ISPL(frame->tf_cs) == SEL_UPL ? "user" : "kernel");
#ifdef SMP
	/* three separate prints in case of a trap on an unmapped page */
	printf("mp_lock = %08x; ", mp_lock);
	printf("cpuid = %d; ", mycpu->gd_cpuid);
	printf("lapic.id = %08x\n", lapic.id);
#endif
	if (type == T_PAGEFLT) {
		printf("fault virtual address	= 0x%x\n", eva);
		printf("fault code		= %s %s, %s\n",
			code & PGEX_U ? "user" : "supervisor",
			code & PGEX_W ? "write" : "read",
			code & PGEX_P ? "protection violation" : "page not present");
	}
	printf("instruction pointer	= 0x%x:0x%x\n",
	       frame->tf_cs & 0xffff, frame->tf_eip);
        if ((ISPL(frame->tf_cs) == SEL_UPL) || (frame->tf_eflags & PSL_VM)) {
		ss = frame->tf_ss & 0xffff;
		esp = frame->tf_esp;
	} else {
		ss = GSEL(GDATA_SEL, SEL_KPL);
		esp = (int)&frame->tf_esp;
	}
	printf("stack pointer	        = 0x%x:0x%x\n", ss, esp);
	printf("frame pointer	        = 0x%x:0x%x\n", ss, frame->tf_ebp);
	printf("code segment		= base 0x%x, limit 0x%x, type 0x%x\n",
	       softseg.ssd_base, softseg.ssd_limit, softseg.ssd_type);
	printf("			= DPL %d, pres %d, def32 %d, gran %d\n",
	       softseg.ssd_dpl, softseg.ssd_p, softseg.ssd_def32,
	       softseg.ssd_gran);
	printf("processor eflags	= ");
	if (frame->tf_eflags & PSL_T)
		printf("trace trap, ");
	if (frame->tf_eflags & PSL_I)
		printf("interrupt enabled, ");
	if (frame->tf_eflags & PSL_NT)
		printf("nested task, ");
	if (frame->tf_eflags & PSL_RF)
		printf("resume, ");
	if (frame->tf_eflags & PSL_VM)
		printf("vm86, ");
	printf("IOPL = %d\n", (frame->tf_eflags & PSL_IOPL) >> 12);
	printf("current process		= ");
	if (curproc) {
		printf("%lu (%s)\n",
		    (u_long)curproc->p_pid, curproc->p_comm ?
		    curproc->p_comm : "");
	} else {
		printf("Idle\n");
	}
	printf("current thread          = pri %d ", curthread->td_pri);
	if (curthread->td_pri >= TDPRI_CRIT)
		printf("(CRIT)");
	printf("\n");
	printf("interrupt mask		= ");
	if ((curthread->td_cpl & net_imask) == net_imask)
		printf("net ");
	if ((curthread->td_cpl & tty_imask) == tty_imask)
		printf("tty ");
	if ((curthread->td_cpl & bio_imask) == bio_imask)
		printf("bio ");
	if ((curthread->td_cpl & cam_imask) == cam_imask)
		printf("cam ");
	if (curthread->td_cpl == 0)
		printf("none");
#ifdef SMP
/**
 *  XXX FIXME:
 *	we probably SHOULD have stopped the other CPUs before now!
 *	another CPU COULD have been touching cpl at this moment...
 */
	printf(" <- SMP: XXX");
#endif
	printf("\n");

#ifdef KDB
	if (kdb_trap(&psl))
		return;
#endif
#ifdef DDB
	if ((debugger_on_panic || db_active) && kdb_trap(type, code, frame))
		return;
#endif
	printf("trap number		= %d\n", type);
	if (type <= MAX_TRAP_MSG)
		panic("%s", trap_msg[type]);
	else
		panic("unknown/reserved trap");
}

/*
 * Double fault handler. Called when a fault occurs while writing
 * a frame for a trap/exception onto the stack. This usually occurs
 * when the stack overflows (such is the case with infinite recursion,
 * for example).
 *
 * XXX Note that the current PTD gets replaced by IdlePTD when the
 * task switch occurs. This means that the stack that was active at
 * the time of the double fault is not available at <kstack> unless
 * the machine was idle when the double fault occurred. The downside
 * of this is that "trace <ebp>" in ddb won't work.
 */
void
dblfault_handler()
{
	struct mdglobaldata *gd = mdcpu;

	printf("\nFatal double fault:\n");
	printf("eip = 0x%x\n", gd->gd_common_tss.tss_eip);
	printf("esp = 0x%x\n", gd->gd_common_tss.tss_esp);
	printf("ebp = 0x%x\n", gd->gd_common_tss.tss_ebp);
#ifdef SMP
	/* three separate prints in case of a trap on an unmapped page */
	printf("mp_lock = %08x; ", mp_lock);
	printf("cpuid = %d; ", mycpu->gd_cpuid);
	printf("lapic.id = %08x\n", lapic.id);
#endif
	panic("double fault");
}

/*
 * Compensate for 386 brain damage (missing URKR).
 * This is a little simpler than the pagefault handler in trap() because
 * it the page tables have already been faulted in and high addresses
 * are thrown out early for other reasons.
 */
int trapwrite(addr)
	unsigned addr;
{
	struct proc *p;
	vm_offset_t va;
	struct vmspace *vm;
	int rv;

	va = trunc_page((vm_offset_t)addr);
	/*
	 * XXX - MAX is END.  Changed > to >= for temp. fix.
	 */
	if (va >= VM_MAXUSER_ADDRESS)
		return (1);

	p = curproc;
	vm = p->p_vmspace;

	++p->p_lock;

	if (!grow_stack (p, va)) {
		--p->p_lock;
		return (1);
	}

	/*
	 * fault the data page
	 */
	rv = vm_fault(&vm->vm_map, va, VM_PROT_WRITE, VM_FAULT_DIRTY);

	--p->p_lock;

	if (rv != KERN_SUCCESS)
		return 1;

	return (0);
}

/*
 *	syscall2 -	MP aware system call request C handler
 *
 *	A system call is essentially treated as a trap except that the
 *	MP lock is not held on entry or return.  We are responsible for
 *	obtaining the MP lock if necessary and for handling ASTs
 *	(e.g. a task switch) prior to return.
 *
 *	In general, only simple access and manipulation of curproc and
 *	the current stack is allowed without having to hold MP lock.
 */
void
syscall2(struct trapframe frame)
{
	struct thread *td = curthread;
	struct proc *p = td->td_proc;
	caddr_t params;
	int i;
	struct sysent *callp;
	register_t orig_tf_eflags;
	u_quad_t sticks;
	int error;
	int narg;
	u_int code;
	union sysunion args;

#ifdef DIAGNOSTIC
	if (ISPL(frame.tf_cs) != SEL_UPL) {
		get_mplock();
		panic("syscall");
		/* NOT REACHED */
	}
#endif

#ifdef SMP
	KASSERT(curthread->td_mpcount == 0, ("badmpcount syscall from %p", (void *)frame.tf_eip));
	get_mplock();
#endif
	/*
	 * access non-atomic field from critical section.  p_sticks is
	 * updated by the clock interrupt.  Also use this opportunity
	 * to lazy-raise our LWKT priority.
	 */
	userenter(td);
	crit_enter_quick(td);
	sticks = curthread->td_sticks;
	crit_exit_quick(td);

	p->p_md.md_regs = &frame;
	params = (caddr_t)frame.tf_esp + sizeof(int);
	code = frame.tf_eax;
	orig_tf_eflags = frame.tf_eflags;

	if (p->p_sysent->sv_prepsyscall) {
		/*
		 * The prep code is not MP aware.
		 */
		(*p->p_sysent->sv_prepsyscall)(&frame, (int *)(&args.nosys.usrmsg + 1), &code, &params);
	} else {
		/*
		 * Need to check if this is a 32 bit or 64 bit syscall.
		 * fuword is MP aware.
		 */
		if (code == SYS_syscall) {
			/*
			 * Code is first argument, followed by actual args.
			 */
			code = fuword(params);
			params += sizeof(int);
		} else if (code == SYS___syscall) {
			/*
			 * Like syscall, but code is a quad, so as to maintain
			 * quad alignment for the rest of the arguments.
			 */
			code = fuword(params);
			params += sizeof(quad_t);
		}
	}

 	if (p->p_sysent->sv_mask)
 		code &= p->p_sysent->sv_mask;

 	if (code >= p->p_sysent->sv_size)
 		callp = &p->p_sysent->sv_table[0];
  	else
 		callp = &p->p_sysent->sv_table[code];

	narg = callp->sy_narg & SYF_ARGMASK;

	/*
	 * copyin is MP aware, but the tracing code is not
	 */
	if (params && (i = narg * sizeof(register_t)) &&
	    (error = copyin(params, (caddr_t)(&args.nosys.usrmsg + 1), (u_int)i))) {
#ifdef KTRACE
		if (KTRPOINT(td, KTR_SYSCALL))
			ktrsyscall(p->p_tracep, code, narg, (void *)(&args.nosys.usrmsg + 1));
#endif
		goto bad;
	}

#if 0
	/*
	 * Try to run the syscall without the MP lock if the syscall
	 * is MP safe.  We have to obtain the MP lock no matter what if 
	 * we are ktracing
	 */
	if ((callp->sy_narg & SYF_MPSAFE) == 0) {
		get_mplock();
		have_mplock = 1;
	}
#endif

#ifdef KTRACE
	if (KTRPOINT(td, KTR_SYSCALL)) {
		ktrsyscall(p->p_tracep, code, narg, (void *)(&args.nosys.usrmsg + 1));
	}
#endif

	/*
	 * For traditional syscall code edx is left untouched when 32 bit
	 * results are returned.  Since edx is loaded from fds[1] when the 
	 * system call returns we pre-set it here.
	 */
	lwkt_initmsg_rp(&args.lmsg, &td->td_msgport, code);
	args.sysmsg_copyout = NULL;
	args.sysmsg_fds[0] = 0;
	args.sysmsg_fds[1] = frame.tf_edx;

	STOPEVENT(p, S_SCE, narg);	/* MP aware */

	error = (*callp->sy_call)(&args);

	/*
	 * MP SAFE (we may or may not have the MP lock at this point)
	 */
	switch (error) {
	case 0:
		/*
		 * Reinitialize proc pointer `p' as it may be different
		 * if this is a child returning from fork syscall.
		 */
		p = curproc;
		frame.tf_eax = args.sysmsg_fds[0];
		frame.tf_edx = args.sysmsg_fds[1];
		frame.tf_eflags &= ~PSL_C;
		break;
	case ERESTART:
		/*
		 * Reconstruct pc, assuming lcall $X,y is 7 bytes,
		 * int 0x80 is 2 bytes. We saved this in tf_err.
		 */
		frame.tf_eip -= frame.tf_err;
		break;
	case EJUSTRETURN:
		break;
	case EASYNC:
		panic("Unexpected EASYNC return value (for now)");
	default:
bad:
 		if (p->p_sysent->sv_errsize) {
 			if (error >= p->p_sysent->sv_errsize)
  				error = -1;	/* XXX */
   			else
  				error = p->p_sysent->sv_errtbl[error];
		}
		frame.tf_eax = error;
		frame.tf_eflags |= PSL_C;
		break;
	}

	/*
	 * Traced syscall.  trapsignal() is not MP aware.
	 */
	if ((orig_tf_eflags & PSL_T) && !(orig_tf_eflags & PSL_VM)) {
		frame.tf_eflags &= ~PSL_T;
		trapsignal(p, SIGTRAP, 0);
	}

	/*
	 * Handle reschedule and other end-of-syscall issues
	 */
	userret(p, &frame, sticks);

#ifdef KTRACE
	if (KTRPOINT(td, KTR_SYSRET)) {
		ktrsysret(p->p_tracep, code, error, args.sysmsg_result);
	}
#endif

	/*
	 * This works because errno is findable through the
	 * register set.  If we ever support an emulation where this
	 * is not the case, this code will need to be revisited.
	 */
	STOPEVENT(p, S_SCX, code);

	userexit(p);
#ifdef SMP
	/*
	 * Release the MP lock if we had to get it
	 */
	KASSERT(curthread->td_mpcount == 1, ("badmpcount syscall from %p", (void *)frame.tf_eip));
	rel_mplock();
#endif
}

/*
 *	sendsys2 -	MP aware system message request C handler
 */
void
sendsys2(struct trapframe frame)
{
	struct globaldata *gd;
	struct thread *td = curthread;
	struct proc *p = td->td_proc;
	register_t orig_tf_eflags;
	struct sysent *callp;
	union sysunion *sysun;
	lwkt_msg_t umsg;
	u_quad_t sticks;
	int error;
	int narg;
	u_int code = 0;
	int msgsize;
	int result;

#ifdef DIAGNOSTIC
	if (ISPL(frame.tf_cs) != SEL_UPL) {
		get_mplock();
		panic("syscall");
		/* NOT REACHED */
	}
#endif

#ifdef SMP
	KASSERT(curthread->td_mpcount == 0, ("badmpcount syscall from %p", (void *)frame.tf_eip));
	get_mplock();
#endif
	/*
	 * access non-atomic field from critical section.  p_sticks is
	 * updated by the clock interrupt.  Also use this opportunity
	 * to lazy-raise our LWKT priority.
	 */
	userenter(td);
	crit_enter_quick(td);
	sticks = curthread->td_sticks;
	crit_exit_quick(td);

	p->p_md.md_regs = &frame;
	orig_tf_eflags = frame.tf_eflags;
	result = 0;

	/*
	 * Handle the waitport/waitmsg/checkport/checkmsg case
	 *
	 * YYY MOVE THIS TO INT 0x82!  We don't really need to combine it
	 * with sendsys().
	 */
	if ((msgsize = frame.tf_edx) <= 0) {
		if (frame.tf_ecx) {
			printf("waitmsg/checkmsg not yet supported: %08x\n",
				frame.tf_ecx);
			error = ENOTSUP;
			goto bad2;
		}
		if (frame.tf_eax) {
			printf("waitport/checkport only the default port is supported at the moment\n");
			error = ENOTSUP;
			goto bad2;
		}
		switch(msgsize) {
		case 0:
			/*
			 * Wait on port for message
			 */
			sysun = lwkt_getport(&td->td_msgport);
			/* XXX block */
			break;
		case -1:
			/*
			 * Test port for message
			 */
			sysun = lwkt_getport(&td->td_msgport);
			break;
		default:
			error = ENOSYS;
			goto bad2;
		}
		if (sysun) {
			gd = td->td_gd;
			umsg = sysun->lmsg.opaque.ms_umsg;
			frame.tf_eax = (register_t)umsg;
			if (sysun->sysmsg_copyout)
				sysun->sysmsg_copyout(sysun);
			atomic_add_int_nonlocked(&td->td_msgport.mp_refs, -1);
			sysun->nosys.usrmsg.umsg.u.ms_fds[0] = sysun->lmsg.u.ms_fds[0];
			sysun->nosys.usrmsg.umsg.u.ms_fds[1] = sysun->lmsg.u.ms_fds[1];
			sysun->nosys.usrmsg.umsg.ms_error = sysun->lmsg.ms_error;
			error = sysun->lmsg.ms_error;
			result = sysun->lmsg.u.ms_fds[0]; /* for ktrace */
			if (error != 0 || code != SYS_execve) {
				error = copyout(
					    &sysun->nosys.usrmsg.umsg.ms_copyout_start,
					    &umsg->ms_copyout_start,
					    ms_copyout_size);
			}
			crit_enter_quick(td);
			sysun->lmsg.opaque.ms_sysunnext = gd->gd_freesysun;
			gd->gd_freesysun = sysun;
			crit_exit_quick(td);
		} else {
			frame.tf_eax = 0;
		}
		frame.tf_edx = 0;
		code = 0;
		error = 0;
		goto good;
	}

	/*
	 * Extract the system call message.  If msgsize is zero we are 
	 * blocking on a message and/or message port.  If msgsize is -1 
	 * we are testing a message for completion or a message port for
	 * activity.
	 *
	 * The userland system call message size includes the size of the
	 * userland lwkt_msg plus arguments.  We load it into the userland
	 * portion of our sysunion structure then we initialize the kerneland
	 * portion and go.
	 */

	/*
	 * Bad message size
	 */
	if (msgsize < sizeof(struct lwkt_msg) ||
	    msgsize > sizeof(union sysunion) - sizeof(struct sysmsg)
	) {
		error = ENOSYS;
		goto bad2;
	}

	/*
	 * Obtain a sysun from our per-cpu cache or allocate a new one.  Use
	 * the opaque field to store the original (user) message pointer.
	 * A critical section is necessary to interlock against interrupts
	 * returning system messages to the thread cache.
	 */
	gd = td->td_gd;
	crit_enter_quick(td);
	if ((sysun = gd->gd_freesysun) != NULL) {
		gd->gd_freesysun = sysun->lmsg.opaque.ms_sysunnext;
		crit_exit_quick(td);
	} else {
		crit_exit_quick(td);
		sysun = malloc(sizeof(union sysunion), M_SYSMSG, M_WAITOK);
	}
	atomic_add_int_nonlocked(&td->td_msgport.mp_refs, 1);

	/*
	 * Copy the user request into the kernel copy of the user request.
	 */
	umsg = (void *)frame.tf_ecx;
	error = copyin(umsg, &sysun->nosys.usrmsg, msgsize);
	if (error)
		goto bad1;
	if ((sysun->nosys.usrmsg.umsg.ms_flags & MSGF_ASYNC) &&
	    (error = suser(td)) != 0
	) {
		goto bad1;
	}

	/*
	 * Initialize the kernel message from the copied-in data and
	 * pull in appropriate flags from the userland message.
	 */
	lwkt_initmsg_rp(&sysun->lmsg, &td->td_msgport, 
	    sysun->nosys.usrmsg.umsg.ms_cmd);
	sysun->sysmsg_copyout = NULL;
	sysun->lmsg.opaque.ms_umsg = umsg;
	sysun->lmsg.ms_flags |= sysun->nosys.usrmsg.umsg.ms_flags & MSGF_ASYNC;

	/*
	 * Extract the system call number, lookup the system call, and
	 * set the default return value.
	 */
	code = (u_int)sysun->lmsg.ms_cmd;
	if (code >= p->p_sysent->sv_size) {
		error = ENOSYS;
		goto bad1;
	}

	callp = &p->p_sysent->sv_table[code];

	narg = (msgsize - sizeof(struct lwkt_msg)) / sizeof(register_t);

#ifdef KTRACE
	if (KTRPOINT(td, KTR_SYSCALL)) {
		ktrsyscall(p->p_tracep, code, narg, (void *)(&sysun->nosys.usrmsg + 1));
	}
#endif
	sysun->lmsg.u.ms_fds[0] = 0;
	sysun->lmsg.u.ms_fds[1] = 0;

	STOPEVENT(p, S_SCE, narg);	/* MP aware */

	/*
	 * Make the system call.  An error code is always returned, results
	 * are copied back via ms_result32 or ms_result64.  YYY temporary
	 * stage copy p_retval[] into ms_result32/64
	 *
	 * NOTE!  XXX if this is a child returning from a fork curproc
	 * might be different.  YYY huh? a child returning from a fork
	 * should never 'return' from this call, it should go right to the
	 * fork_trampoline function.
	 */
	error = (*callp->sy_call)(sysun);
	gd = td->td_gd;	/* RELOAD, might have switched cpus */

bad1:
	/*
	 * If a synchronous return copy p_retval to ms_result64 and return
	 * the sysmsg to the free pool.
	 *
	 * YYY Don't writeback message if execve() YYY
	 */
	if (error != EASYNC) {
		atomic_add_int_nonlocked(&td->td_msgport.mp_refs, -1);
		sysun->nosys.usrmsg.umsg.u.ms_fds[0] = sysun->lmsg.u.ms_fds[0];
		sysun->nosys.usrmsg.umsg.u.ms_fds[1] = sysun->lmsg.u.ms_fds[1];
		result = sysun->nosys.usrmsg.umsg.u.ms_fds[0]; /* for ktrace */
		if (error != 0 || code != SYS_execve) {
			int error2;
			error2 = copyout(&sysun->nosys.usrmsg.umsg.ms_copyout_start,
					&umsg->ms_copyout_start,
					ms_copyout_size);
			if (error == 0)
				error2 = error;
		}
		crit_enter_quick(td);
		sysun->lmsg.opaque.ms_sysunnext = gd->gd_freesysun;
		gd->gd_freesysun = sysun;
		crit_exit_quick(td);
	}
bad2:
	frame.tf_eax = error;
good:

	/*
	 * Traced syscall.  trapsignal() is not MP aware.
	 */
	if ((orig_tf_eflags & PSL_T) && !(orig_tf_eflags & PSL_VM)) {
		frame.tf_eflags &= ~PSL_T;
		trapsignal(p, SIGTRAP, 0);
	}

	/*
	 * Handle reschedule and other end-of-syscall issues
	 */
	userret(p, &frame, sticks);

#ifdef KTRACE
	if (KTRPOINT(td, KTR_SYSRET)) {
		ktrsysret(p->p_tracep, code, error, result);
	}
#endif

	/*
	 * This works because errno is findable through the
	 * register set.  If we ever support an emulation where this
	 * is not the case, this code will need to be revisited.
	 */
	STOPEVENT(p, S_SCX, code);

	userexit(p);
#ifdef SMP
	/*
	 * Release the MP lock if we had to get it
	 */
	KASSERT(curthread->td_mpcount == 1, ("badmpcount syscall from %p", (void *)frame.tf_eip));
	rel_mplock();
#endif
}

/*
 * Simplified back end of syscall(), used when returning from fork()
 * directly into user mode.  MP lock is held on entry and should be
 * released on return.  This code will return back into the fork
 * trampoline code which then runs doreti.
 */
void
fork_return(p, frame)
	struct proc *p;
	struct trapframe frame;
{
	frame.tf_eax = 0;		/* Child returns zero */
	frame.tf_eflags &= ~PSL_C;	/* success */
	frame.tf_edx = 1;

	userret(p, &frame, 0);
#ifdef KTRACE
	if (KTRPOINT(p->p_thread, KTR_SYSRET))
		ktrsysret(p->p_tracep, SYS_fork, 0, 0);
#endif
	p->p_flag |= P_PASSIVE_ACQ;
	userexit(p);
	p->p_flag &= ~P_PASSIVE_ACQ;
#ifdef SMP
	KKASSERT(curthread->td_mpcount == 1);
	rel_mplock();
#endif
}

