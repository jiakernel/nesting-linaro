/*
 * Copyright (C) 2015 - ARM Ltd
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

#include <asm/kvm_hyp.h>
#include <asm/tlbflush.h>

static void __hyp_text __tlb_switch_to_guest_vhe(u64 vttbr)
{
	u64 val;

	/*
	 * With VHE enabled, we have HCR_EL2.{E2H,TGE} = {1,1}, and
	 * most TLB operations target EL2/EL0. In order to affect the
	 * guest TLBs (EL1/EL0), we need to change one of these two
	 * bits. Changing E2H is impossible (goodbye TTBR1_EL2), so
	 * let's flip TGE before executing the TLB operation.
	 */
	write_sysreg(vttbr, vttbr_el2);
	val = read_sysreg(hcr_el2);
	val &= ~HCR_TGE;
	write_sysreg(val, hcr_el2);
	isb();
}

static void __hyp_text __tlb_switch_to_guest_nvhe(u64 vttbr)
{
	write_sysreg(vttbr, vttbr_el2);
	isb();
}

static hyp_alternate_select(__tlb_switch_to_guest,
			    __tlb_switch_to_guest_nvhe,
			    __tlb_switch_to_guest_vhe,
			    ARM64_HAS_VIRT_HOST_EXTN);

static void __hyp_text __tlb_switch_to_host_vhe(void)
{
	/*
	 * We're done with the TLB operation, let's restore the host's
	 * view of HCR_EL2.
	 */
	write_sysreg(0, vttbr_el2);
	write_sysreg(HCR_HOST_VHE_FLAGS, hcr_el2);
}

static void __hyp_text __tlb_switch_to_host_nvhe(void)
{
	write_sysreg(0, vttbr_el2);
}

static hyp_alternate_select(__tlb_switch_to_host,
			    __tlb_switch_to_host_nvhe,
			    __tlb_switch_to_host_vhe,
			    ARM64_HAS_VIRT_HOST_EXTN);

void __hyp_text __kvm_tlb_flush_vmid_ipa(u64 vttbr, phys_addr_t ipa)
{
	dsb(ishst);

	/* Switch to requested VMID */
	__tlb_switch_to_guest()(vttbr);

	/*
	 * We could do so much better if we had the VA as well.
	 * Instead, we invalidate Stage-2 for this IPA, and the
	 * whole of Stage-1. Weep...
	 */
	ipa >>= 12;
	__tlbi(ipas2e1is, ipa);

	/*
	 * We have to ensure completion of the invalidation at Stage-2,
	 * since a table walk on another CPU could refill a TLB with a
	 * complete (S1 + S2) walk based on the old Stage-2 mapping if
	 * the Stage-1 invalidation happened first.
	 */
	dsb(ish);
	__tlbi(vmalle1is);
	dsb(ish);
	isb();

	/*
	 * If the host is running at EL1 and we have a VPIPT I-cache,
	 * then we must perform I-cache maintenance at EL2 in order for
	 * it to have an effect on the guest. Since the guest cannot hit
	 * I-cache lines allocated with a different VMID, we don't need
	 * to worry about junk out of guest reset (we nuke the I-cache on
	 * VMID rollover), but we do need to be careful when remapping
	 * executable pages for the same guest. This can happen when KSM
	 * takes a CoW fault on an executable page, copies the page into
	 * a page that was previously mapped in the guest and then needs
	 * to invalidate the guest view of the I-cache for that page
	 * from EL1. To solve this, we invalidate the entire I-cache when
	 * unmapping a page from a guest if we have a VPIPT I-cache but
	 * the host is running at EL1. As above, we could do better if
	 * we had the VA.
	 *
	 * The moral of this story is: if you have a VPIPT I-cache, then
	 * you should be running with VHE enabled.
	 */
	if (!has_vhe() && icache_is_vpipt())
		__flush_icache_all();

	__tlb_switch_to_host()();
}

void __hyp_text __kvm_tlb_flush_vmid(u64 vttbr)
{
	dsb(ishst);

	/* Switch to requested VMID */
	__tlb_switch_to_guest()(vttbr);

	__tlbi(vmalls12e1is);
	dsb(ish);
	isb();

	__tlb_switch_to_host()();
}

void __hyp_text __kvm_tlb_flush_local_vmid(u64 vttbr)
{
	/* Switch to requested VMID */
	__tlb_switch_to_guest()(vttbr);

	__tlbi(vmalle1);
	dsb(nsh);
	isb();

	__tlb_switch_to_host()();
}

void __hyp_text __kvm_flush_vm_context(void)
{
	dsb(ishst);
	__tlbi(alle1is);
	asm volatile("ic ialluis" : : );
	dsb(ish);
}

void __hyp_text __kvm_tlb_vae2(u64 vttbr, u64 va, u64 sys_encoding)
{
	/* Switch to requested VMID */
	__tlb_switch_to_guest()(vttbr);

	/* Execute the EL1 version of TLBI VAE2* instruction */
	switch(sys_encoding) {
	case TLBI_VAE2IS:
		__tlbi(vae1is, va);
		break;
	case TLBI_VALE2IS:
		__tlbi(vale1is, va);
		break;
	case TLBI_VAE2:
		__tlbi(vae1, va);
		break;
	case TLBI_VALE2:
		__tlbi(vale1, va);
		break;
	default:
		break;
	}
	dsb(nsh);
	isb();

	__tlb_switch_to_host()();
}

void __hyp_text __kvm_tlb_el1_instr(u64 vttbr, u64 val, u64 sys_encoding)
{
	/* Switch to requested VMID */
	__tlb_switch_to_guest()(vttbr);

	/* Execute the same instruction as the guest hypervisor did */
	switch(sys_encoding) {
	case TLBI_VMALLE1IS:
		__tlbi(vmalle1is);
		break;
	case TLBI_VAE1IS:
		__tlbi(vae1is, val);
		break;
	case TLBI_ASIDE1IS:
		__tlbi(aside1is, val);
		break;
	case TLBI_VAAE1IS:
		__tlbi(vaae1is, val);
		break;
	case TLBI_VALE1IS:
		__tlbi(vale1is, val);
		break;
	case TLBI_VAALE1IS:
		__tlbi(vaale1is, val);
		break;
	case TLBI_VMALLE1:
		__tlbi(vmalle1);
		break;
	case TLBI_VAE1:
		__tlbi(vae1, val);
		break;
	case TLBI_ASIDE1:
		__tlbi(aside1, val);
		break;
	case TLBI_VAAE1:
		__tlbi(vaae1, val);
		break;
	case TLBI_VALE1:
		__tlbi(vale1, val);
		break;
	case TLBI_VAALE1:
		__tlbi(vaale1, val);
		break;
	default:
		break;
	}
	dsb(nsh);
	isb();

	__tlb_switch_to_host()();
}
