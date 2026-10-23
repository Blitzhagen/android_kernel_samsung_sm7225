// SPDX-License-Identifier: GPL-2.0

#include <linux/compiler.h>
#include <linux/context_tracking.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/nospec.h>
#include <linux/ptrace.h>
#include <linux/sched.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>

#include <asm/cacheflush.h>
#include <asm/daifflags.h>
#include <asm/debug-monitors.h>
#include <asm/fpsimd.h>
#include <asm/syscall.h>
#include <asm/thread_info.h>
#include <asm/unistd.h>

#ifdef CONFIG_SECURITY_DEFEX
#include <linux/defex.h>

extern void dbg_fault_stage(unsigned int stage, unsigned long addr);
#endif
long compat_arm_syscall(struct pt_regs *regs, int scno);
long sys_ni_syscall(void);

static long do_ni_syscall(struct pt_regs *regs, int scno)
{
#ifdef CONFIG_COMPAT
	long ret;
	if (is_compat_task()) {
#ifdef CONFIG_SECURITY_DEFEX
		ret = defex_syscall_enter(scno, regs);
		if (!ret)
#endif /* CONFIG_SECURITY_DEFEX */
		ret = compat_arm_syscall(regs, scno);
		if (ret != -ENOSYS)
			return ret;
	}
#endif

	return sys_ni_syscall();
}

static long __invoke_syscall(struct pt_regs *regs, syscall_fn_t syscall_fn)
{
	return syscall_fn(regs);
}

static void invoke_syscall(struct pt_regs *regs, unsigned int scno,
			   unsigned int sc_nr,
			   const syscall_fn_t syscall_table[])
{
	long ret;

	if (scno < sc_nr) {
		syscall_fn_t syscall_fn;
		syscall_fn = syscall_table[array_index_nospec(scno, sc_nr)];
#ifdef CONFIG_SECURITY_DEFEX
		ret = defex_syscall_enter(scno, regs);
		if (!ret)
#endif /* CONFIG_SECURITY_DEFEX */
		ret = __invoke_syscall(regs, syscall_fn);
	} else {
		ret = do_ni_syscall(regs, scno);
	}

	if (is_compat_task())
		ret = lower_32_bits(ret);

	regs->regs[0] = ret;
}

static inline bool has_syscall_work(unsigned long flags)
{
	return unlikely(flags & _TIF_SYSCALL_WORK);
}

int syscall_trace_enter(struct pt_regs *regs);
void syscall_trace_exit(struct pt_regs *regs);

#ifdef CONFIG_ARM64_ERRATUM_1463225
DECLARE_PER_CPU(int, __in_cortex_a76_erratum_1463225_wa);

static void cortex_a76_erratum_1463225_svc_handler(void)
{
	u32 reg, val;

	if (!unlikely(test_thread_flag(TIF_SINGLESTEP)))
		return;

	if (!unlikely(this_cpu_has_cap(ARM64_WORKAROUND_1463225)))
		return;

	__this_cpu_write(__in_cortex_a76_erratum_1463225_wa, 1);
	reg = read_sysreg(mdscr_el1);
	val = reg | DBG_MDSCR_SS | DBG_MDSCR_KDE;
	write_sysreg(val, mdscr_el1);
	asm volatile("msr daifclr, #8");
	isb();

	/* We will have taken a single-step exception by this point */

	write_sysreg(reg, mdscr_el1);
	__this_cpu_write(__in_cortex_a76_erratum_1463225_wa, 0);
}
#else
static void cortex_a76_erratum_1463225_svc_handler(void) { }
#endif /* CONFIG_ARM64_ERRATUM_1463225 */

static void el0_svc_common(struct pt_regs *regs, int scno, int sc_nr,
			   const syscall_fn_t syscall_table[])
{
	unsigned long flags = current_thread_info()->flags;

	regs->orig_x0 = regs->regs[0];
	regs->syscallno = scno;

	cortex_a76_erratum_1463225_svc_handler();
	user_exit_irqoff();
	local_daif_restore(DAIF_PROCCTX);

	if (has_syscall_work(flags)) {
		/* set default errno for user-issued syscall(-1) */
		if (scno == NO_SYSCALL)
			regs->regs[0] = -ENOSYS;
		scno = syscall_trace_enter(regs);
		if (scno == NO_SYSCALL)
			goto trace_exit;
	}

	dbg_fault_stage(0x90, scno);
	if ((scno == 240 || scno == 131) && user_mode(regs)) {
		unsigned long sig = (scno == 240) ? regs->regs[2] : regs->regs[1];
		if (sig == 6) {
			u64 w[64];
			int i;
			pr_info("DBG-ABT: pid=%d scno=%d pc=%llx lr=%llx sp=%llx\n",
				current->pid, scno, regs->pc, regs->regs[30], regs->sp);
			if (!strcmp(current->comm, "linkerconfig") &&
			    !copy_from_user(w, (void __user *)regs->sp, sizeof(w)))
				for (i = 0; i < 64; i += 8)
					pr_info("DBG-STK: %016llx %016llx %016llx %016llx %016llx %016llx %016llx %016llx\n",
						w[i], w[i+1], w[i+2], w[i+3],
						w[i+4], w[i+5], w[i+6], w[i+7]);
		}
	}
	{
		unsigned long a0 = regs->regs[0], a1 = regs->regs[1];
		unsigned long a2 = regs->regs[2], a3 = regs->regs[3];
		unsigned long a4 = regs->regs[4], a5 = regs->regs[5];
		invoke_syscall(regs, scno, sc_nr, syscall_table);
		if (scno == 40 || scno == 39) {
			long ret = (long)regs->regs[0];
			char p1[64] = {0}, p2[32] = {0}, p3[32] = {0};
			strncpy_from_user(p1, (void __user *)a0, 63);
			strncpy_from_user(p2, (void __user *)a1, 31);
			strncpy_from_user(p3, (void __user *)a2, 31);
			pr_info("DBG-MNT: %s pid=%d scno=%d ret=%ld src='%s' tgt='%s' typ='%s' fl=%llx\n",
				current->comm, current->pid, scno, ret, p1, p2, p3, a3);
		}
		if (current->pid == 1) {
			long ret = (long)regs->regs[0];
			if (ret < 0 && ret >= -4095 &&
			    ret != -2 && ret != -4 && ret != -9 && ret != -11 &&
			    ret != -17 && ret != -22 && ret != -25 && ret != -1 && ret != -5
			    /* ENOENT,EINTR,EBADF,EAGAIN,EEXIST,EINVAL,ENOTTY,EPERM,EIO */) {
				char p1[64] = {0}, p2[32] = {0};
				if (scno == 56 || scno == 34 || scno == 79 || scno == 48 ||
				    scno == 33 || scno == 35 || scno == 46 || scno == 78 ||
				    scno == 260 || scno == 37 || scno == 38 || scno == 278)
					strncpy_from_user(p1, (void __user *)a1, 63);
				if (scno == 40) {
					strncpy_from_user(p1, (void __user *)a1, 63);
					strncpy_from_user(p2, (void __user *)a2, 31);
				}
				pr_info("DBG-ERR: pid=%d scno=%d ret=%ld x0=%llx x1=%llx x2=%llx x3=%llx x4=%llx x5=%llx p='%s' t='%s'\n",
					current->pid, scno, ret, a0, a1, a2, a3, a4, a5, p1, p2);
			}
		}
	}
	dbg_fault_stage(0x91, scno);

	/*
	 * The tracing status may have changed under our feet, so we have to
	 * check again. However, if we were tracing entry, then we always trace
	 * exit regardless, as the old entry assembly did.
	 */
	if (!has_syscall_work(flags) && !IS_ENABLED(CONFIG_DEBUG_RSEQ)) {
		local_daif_mask();
		flags = current_thread_info()->flags;
		if (!has_syscall_work(flags) && !(flags & _TIF_SINGLESTEP)) {
			/*
			 * We're off to userspace, where interrupts are
			 * always enabled after we restore the flags from
			 * the SPSR.
			 */
			trace_hardirqs_on();
			return;
		}
		local_daif_restore(DAIF_PROCCTX);
	}

trace_exit:
	syscall_trace_exit(regs);
}

static inline void sve_user_discard(void)
{
	if (!system_supports_sve())
		return;

	clear_thread_flag(TIF_SVE);

	/*
	 * task_fpsimd_load() won't be called to update CPACR_EL1 in
	 * ret_to_user unless TIF_FOREIGN_FPSTATE is still set, which only
	 * happens if a context switch or kernel_neon_begin() or context
	 * modification (sigreturn, ptrace) intervenes.
	 * So, ensure that CPACR_EL1 is already correct for the fast-path case.
	 */
	sve_user_disable();
}

asmlinkage void el0_svc_handler(struct pt_regs *regs)
{
	sve_user_discard();
	el0_svc_common(regs, regs->regs[8], __NR_syscalls, sys_call_table);
}

#ifdef CONFIG_COMPAT
asmlinkage void el0_svc_compat_handler(struct pt_regs *regs)
{
	el0_svc_common(regs, regs->regs[7], __NR_compat_syscalls,
		       compat_sys_call_table);
}
#endif
