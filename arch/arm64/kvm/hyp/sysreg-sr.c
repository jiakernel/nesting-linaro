/*
 * Copyright (C) 2012-2015 - ARM Ltd
 * Author: Marc Zyngier <marc.zyngier@arm.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/compiler.h>
#include <linux/kvm_host.h>

#include <asm/kvm_asm.h>
#include <asm/kvm_emulate.h>
#include <asm/kvm_hyp.h>

/* Yes, this does nothing, on purpose */
static void __hyp_text __sysreg_do_nothing(struct kvm_cpu_context *ctxt) { }

/*
 * Non-VHE: Both host and guest must save everything.
 *
 * VHE: Host must save tpidr*_el[01], actlr_el1, mdscr_el1, sp0, pc,
 * pstate, and guest must save everything.
 */

static void __hyp_text __sysreg_save_common_state(struct kvm_cpu_context *ctxt)
{
	u64 *sys_regs = kern_hyp_va(ctxt->hw_sys_regs);

	sys_regs[ACTLR_EL1]	= read_sysreg(actlr_el1);
	sys_regs[TPIDR_EL0]	= read_sysreg(tpidr_el0);
	sys_regs[TPIDRRO_EL0]	= read_sysreg(tpidrro_el0);
	sys_regs[TPIDR_EL1]	= read_sysreg(tpidr_el1);
	sys_regs[MDSCR_EL1]	= read_sysreg(mdscr_el1);
	ctxt->gp_regs.regs.sp		= read_sysreg(sp_el0);
	ctxt->gp_regs.regs.pc		= read_sysreg_el2(elr);
	ctxt->hw_pstate			= read_sysreg_el2(spsr);
}

static void __hyp_text __sysreg_save_state(struct kvm_cpu_context *ctxt)
{
	u64 *sys_regs = kern_hyp_va(ctxt->hw_sys_regs);

	sys_regs[MPIDR_EL1]	= read_sysreg(vmpidr_el2);
	sys_regs[CSSELR_EL1]	= read_sysreg(csselr_el1);
	sys_regs[SCTLR_EL1]	= read_sysreg_el1(sctlr);
	sys_regs[CPACR_EL1]	= read_sysreg_el1(cpacr);
	sys_regs[TTBR0_EL1]	= read_sysreg_el1(ttbr0);
	sys_regs[TTBR1_EL1]	= read_sysreg_el1(ttbr1);
	sys_regs[TCR_EL1]	= read_sysreg_el1(tcr);
	sys_regs[ESR_EL1]	= read_sysreg_el1(esr);
	sys_regs[AFSR0_EL1]	= read_sysreg_el1(afsr0);
	sys_regs[AFSR1_EL1]	= read_sysreg_el1(afsr1);
	sys_regs[FAR_EL1]	= read_sysreg_el1(far);
	sys_regs[MAIR_EL1]	= read_sysreg_el1(mair);
	sys_regs[VBAR_EL1]	= read_sysreg_el1(vbar);
	sys_regs[CONTEXTIDR_EL1]	= read_sysreg_el1(contextidr);
	sys_regs[AMAIR_EL1]	= read_sysreg_el1(amair);
	sys_regs[CNTKCTL_EL1]	= read_sysreg_el1(cntkctl);
	sys_regs[PAR_EL1]		= read_sysreg(par_el1);

	ctxt->hw_sp_el1			= read_sysreg(sp_el1);
	ctxt->hw_elr_el1		= read_sysreg_el1(elr);
	ctxt->hw_spsr_el1		= read_sysreg_el1(spsr);
}

static hyp_alternate_select(__sysreg_call_save_host_state,
			    __sysreg_save_state, __sysreg_do_nothing,
			    ARM64_HAS_VIRT_HOST_EXTN);

void __hyp_text __sysreg_save_host_state(struct kvm_cpu_context *ctxt)
{
	__sysreg_call_save_host_state()(ctxt);
	__sysreg_save_common_state(ctxt);
}

void __hyp_text __sysreg_save_guest_state(struct kvm_cpu_context *ctxt)
{
	__sysreg_save_state(ctxt);
	__sysreg_save_common_state(ctxt);
}

static void __hyp_text __sysreg_restore_common_state(struct kvm_cpu_context *ctxt)
{
	u64 *sys_regs = kern_hyp_va(ctxt->hw_sys_regs);

	write_sysreg(sys_regs[ACTLR_EL1],	  actlr_el1);
	write_sysreg(sys_regs[TPIDR_EL0],	  tpidr_el0);
	write_sysreg(sys_regs[TPIDRRO_EL0],	tpidrro_el0);
	write_sysreg(sys_regs[TPIDR_EL1],	  tpidr_el1);
	write_sysreg(sys_regs[MDSCR_EL1],	  mdscr_el1);
	write_sysreg(ctxt->gp_regs.regs.sp,	  sp_el0);
	write_sysreg_el2(ctxt->gp_regs.regs.pc,	  elr);
	write_sysreg_el2(ctxt->hw_pstate,	  spsr);
}

static void __hyp_text __sysreg_restore_state(struct kvm_cpu_context *ctxt)
{
	u64 *sys_regs = kern_hyp_va(ctxt->hw_sys_regs);

	write_sysreg(sys_regs[MPIDR_EL1],	vmpidr_el2);
	write_sysreg(sys_regs[CSSELR_EL1],	csselr_el1);
	write_sysreg_el1(sys_regs[SCTLR_EL1],	sctlr);
	write_sysreg_el1(sys_regs[CPACR_EL1],	cpacr);
	write_sysreg_el1(sys_regs[TTBR0_EL1],	ttbr0);
	write_sysreg_el1(sys_regs[TTBR1_EL1],	ttbr1);
	write_sysreg_el1(sys_regs[TCR_EL1],	tcr);
	write_sysreg_el1(sys_regs[ESR_EL1],	esr);
	write_sysreg_el1(sys_regs[AFSR0_EL1],	afsr0);
	write_sysreg_el1(sys_regs[AFSR1_EL1],	afsr1);
	write_sysreg_el1(sys_regs[FAR_EL1],	far);
	write_sysreg_el1(sys_regs[MAIR_EL1],	mair);
	write_sysreg_el1(sys_regs[VBAR_EL1],	vbar);
	write_sysreg_el1(sys_regs[CONTEXTIDR_EL1], contextidr);
	write_sysreg_el1(sys_regs[AMAIR_EL1],	amair);
	write_sysreg_el1(sys_regs[CNTKCTL_EL1], cntkctl);
	write_sysreg(sys_regs[PAR_EL1],		par_el1);

	write_sysreg(ctxt->hw_sp_el1,			sp_el1);
	write_sysreg_el1(ctxt->hw_elr_el1,		elr);
	write_sysreg_el1(ctxt->hw_spsr_el1,		spsr);
}

static hyp_alternate_select(__sysreg_call_restore_host_state,
			    __sysreg_restore_state, __sysreg_do_nothing,
			    ARM64_HAS_VIRT_HOST_EXTN);

void __hyp_text __sysreg_restore_host_state(struct kvm_cpu_context *ctxt)
{
	__sysreg_call_restore_host_state()(ctxt);
	__sysreg_restore_common_state(ctxt);
}

void __hyp_text __sysreg_restore_guest_state(struct kvm_cpu_context *ctxt)
{
	__sysreg_restore_state(ctxt);
	__sysreg_restore_common_state(ctxt);
}

void __hyp_text __sysreg32_save_state(struct kvm_vcpu *vcpu)
{
	u64 *spsr, *sysreg;

	if (read_sysreg(hcr_el2) & HCR_RW)
		return;

	spsr = vcpu->arch.ctxt.gp_regs.spsr;
	sysreg = vcpu->arch.ctxt.sys_regs;

	spsr[KVM_SPSR_ABT] = read_sysreg(spsr_abt);
	spsr[KVM_SPSR_UND] = read_sysreg(spsr_und);
	spsr[KVM_SPSR_IRQ] = read_sysreg(spsr_irq);
	spsr[KVM_SPSR_FIQ] = read_sysreg(spsr_fiq);

	sysreg[DACR32_EL2] = read_sysreg(dacr32_el2);
	sysreg[IFSR32_EL2] = read_sysreg(ifsr32_el2);

	if (__fpsimd_enabled())
		sysreg[FPEXC32_EL2] = read_sysreg(fpexc32_el2);

	if (vcpu->arch.debug_flags & KVM_ARM64_DEBUG_DIRTY)
		sysreg[DBGVCR32_EL2] = read_sysreg(dbgvcr32_el2);
}

void __hyp_text __sysreg32_restore_state(struct kvm_vcpu *vcpu)
{
	u64 *spsr, *sysreg;

	if (read_sysreg(hcr_el2) & HCR_RW)
		return;

	spsr = vcpu->arch.ctxt.gp_regs.spsr;
	sysreg = vcpu->arch.ctxt.sys_regs;

	write_sysreg(spsr[KVM_SPSR_ABT], spsr_abt);
	write_sysreg(spsr[KVM_SPSR_UND], spsr_und);
	write_sysreg(spsr[KVM_SPSR_IRQ], spsr_irq);
	write_sysreg(spsr[KVM_SPSR_FIQ], spsr_fiq);

	write_sysreg(sysreg[DACR32_EL2], dacr32_el2);
	write_sysreg(sysreg[IFSR32_EL2], ifsr32_el2);

	if (vcpu->arch.debug_flags & KVM_ARM64_DEBUG_DIRTY)
		write_sysreg(sysreg[DBGVCR32_EL2], dbgvcr32_el2);
}
