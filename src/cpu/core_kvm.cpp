/*
 *  Copyright (C) 2002-2021  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/* The KVM CPU core (see core_kvm.h and KVM-TODO.md) */

#include "dosbox.h"

#if C_KVM

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <linux/kvm.h>

#include <string>
#include <vector>

#include "core_kvm.h"
#include "cpu.h"
#include "logging.h"
#include "regs.h"
#include "setup.h"

/* Guest physical address of the 3 pages that Intel hosts need for the TSS KVM uses to run real
 * mode code without unrestricted guest support (KVM_SET_TSS_ADDR), and of the page KVM uses for
 * its identity map below it (KVM's default, 0xfffbc000). No memory slot may cover
 * 0xfffbc000-0xfffbffff. DOSBox-X maps nothing there (the ROM BIOS alias is at 0xffff0000). */
static const unsigned long kvm_tss_addr = 0xfffbd000ul;

static struct {
	int		dev = -1;	/* /dev/kvm */
	int		vm = -1;
	int		vcpu = -1;
	struct kvm_run*	run = NULL;	/* the vCPU's shared kvm_run area */
	size_t		run_size = 0;
	int		memslots = 0;	/* how many memory slots the host allows */
	bool		exit_registered = false;
} kvm;

static std::string KVM_Error(const char *what) {
	return std::string(what) + ": " + strerror(errno);
}

static void KVM_CoreShutdownSection(Section *sec) {
	(void)sec;
	KVM_CoreShutdown();
}

void KVM_CoreShutdown(void) {
	if (kvm.run != NULL) {
		munmap(kvm.run,kvm.run_size);
		kvm.run = NULL;
	}
	if (kvm.vcpu >= 0) { close(kvm.vcpu); kvm.vcpu = -1; }
	if (kvm.vm >= 0) { close(kvm.vm); kvm.vm = -1; }
	if (kvm.dev >= 0) { close(kvm.dev); kvm.dev = -1; }
}

/* Whether the host can run this core: /dev/kvm, the API version, the capabilities the core
 * needs, and real mode support */
static bool KVM_CheckHost(std::string &reason) {
	kvm.dev = open("/dev/kvm",O_RDWR|O_CLOEXEC);
	if (kvm.dev < 0) {
		if (errno == ENOENT)
			reason = "this host has no /dev/kvm (KVM is not enabled, or the CPU has no hardware virtualization)";
		else if (errno == EACCES)
			reason = "no permission to use /dev/kvm (the user is usually given access through the kvm group)";
		else
			reason = KVM_Error("cannot open /dev/kvm");
		return false;
	}

	const int api = ioctl(kvm.dev,KVM_GET_API_VERSION,0);
	if (api != KVM_API_VERSION) {
		reason = "the host KVM API version is " + std::to_string(api) + ", not " + std::to_string(KVM_API_VERSION);
		return false;
	}

	static const struct {
		int		cap;
		const char*	name;
	} needed[] = {
		{ KVM_CAP_USER_MEMORY,		"memory slots in user memory (KVM_CAP_USER_MEMORY)" },
		{ KVM_CAP_READONLY_MEM,		"read-only memory slots (KVM_CAP_READONLY_MEM)" },
		{ KVM_CAP_SET_GUEST_DEBUG,	"guest debugging (KVM_CAP_SET_GUEST_DEBUG), for callback instructions" },
		{ KVM_CAP_EXT_CPUID,		"setting the CPUID (KVM_CAP_EXT_CPUID)" },
		{ KVM_CAP_SET_TSS_ADDR,		"KVM_CAP_SET_TSS_ADDR" },
	};
	for (const auto &n : needed) {
		if (ioctl(kvm.dev,KVM_CHECK_EXTENSION,n.cap) <= 0) {
			reason = std::string("the host KVM does not support ") + n.name;
			return false;
		}
	}

	/* Intel CPUs run real mode and big real mode guests only with "unrestricted guest" (AMD
	 * CPUs always can). Without it, KVM can only emulate such code, slowly and incompletely. */
	FILE *fp = fopen("/sys/module/kvm_intel/parameters/unrestricted_guest","r");
	if (fp != NULL) {
		const int c = fgetc(fp);
		fclose(fp);
		if (c == 'N' || c == 'n' || c == '0') {
			reason = "the host KVM runs without unrestricted guest support (kvm_intel unrestricted_guest=N), which real mode code needs";
			return false;
		}
	}

	kvm.memslots = ioctl(kvm.dev,KVM_CHECK_EXTENSION,KVM_CAP_NR_MEMSLOTS);
	if (kvm.memslots < 32) {
		reason = "the host KVM allows only " + std::to_string(kvm.memslots) + " memory slots";
		return false;
	}

	return true;
}

static uint32_t KVM_FindSupported(const struct kvm_cpuid2 *sup,uint32_t function,int reg) {
	for (uint32_t i=0;i < sup->nent;i++) {
		const struct kvm_cpuid_entry2 &e = sup->entries[i];
		if (e.function == function && e.index == 0)
			return reg == 2 ? e.ecx : e.edx;
	}
	return 0;
}

/* Give the vCPU the CPUID of the emulated CPU type (DOSBox-X's own CPUID instruction), without
 * the features the host cannot provide */
static bool KVM_SetCPUID(std::string &reason) {
	std::vector<uint8_t> supbuf;
	struct kvm_cpuid2 *sup = NULL;
	for (uint32_t nent=64;;nent*=2) {
		supbuf.assign(sizeof(struct kvm_cpuid2) + nent*sizeof(struct kvm_cpuid_entry2),0);
		sup = (struct kvm_cpuid2*)supbuf.data();
		sup->nent = nent;
		if (ioctl(kvm.dev,KVM_GET_SUPPORTED_CPUID,sup) == 0) break;
		if (errno != E2BIG || nent >= 4096) {
			reason = KVM_Error("KVM_GET_SUPPORTED_CPUID");
			return false;
		}
	}

	std::vector<struct kvm_cpuid_entry2> entries;
	const uint32_t save_eax = reg_eax,save_ebx = reg_ebx,save_ecx = reg_ecx,save_edx = reg_edx;

	reg_eax = 0; reg_ecx = 0;
	if (CPU_CPUID()) {
		const uint32_t max_leaf = reg_eax;
		for (uint32_t leaf=0;leaf <= max_leaf && leaf < 32;leaf++) {
			reg_eax = leaf; reg_ebx = 0; reg_ecx = 0; reg_edx = 0;
			CPU_CPUID();

			struct kvm_cpuid_entry2 e;
			memset(&e,0,sizeof(e));
			e.function = leaf;
			e.eax = reg_eax; e.ebx = reg_ebx; e.ecx = reg_ecx; e.edx = reg_edx;
			if (leaf == 1) {
				const uint32_t sup_edx = KVM_FindSupported(sup,1,3),sup_ecx = KVM_FindSupported(sup,1,2);
				if ((e.edx & ~sup_edx) != 0 || (e.ecx & ~sup_ecx) != 0)
					LOG_MSG("KVM: the host cannot provide CPUID features EDX=%08x ECX=%08x of the emulated CPU, the guest will not see them",
						(unsigned int)(e.edx & ~sup_edx),(unsigned int)(e.ecx & ~sup_ecx));
				e.edx &= sup_edx;
				e.ecx &= sup_ecx;
			}
			entries.push_back(e);
		}
	}
	else {
		/* An emulated CPU without CPUID: the instruction still works under KVM (and the
		 * EFLAGS ID bit can be toggled), but it reports nothing */
		LOG_MSG("KVM: the emulated CPU type has no CPUID instruction, but under KVM CPUID still runs and returns zeros");
	}

	reg_eax = save_eax; reg_ebx = save_ebx; reg_ecx = save_ecx; reg_edx = save_edx;

	std::vector<uint8_t> setbuf(sizeof(struct kvm_cpuid2) + entries.size()*sizeof(struct kvm_cpuid_entry2),0);
	struct kvm_cpuid2 *set = (struct kvm_cpuid2*)setbuf.data();
	set->nent = (uint32_t)entries.size();
	for (size_t i=0;i < entries.size();i++) set->entries[i] = entries[i];
	if (ioctl(kvm.vcpu,KVM_SET_CPUID2,set) < 0) {
		reason = KVM_Error("KVM_SET_CPUID2");
		return false;
	}

	return true;
}

bool KVM_CoreInit(std::string &reason) {
	/* already created: only the CPU settings may have changed */
	if (kvm.vcpu >= 0)
		return KVM_SetCPUID(reason);

	if (!KVM_CheckHost(reason)) {
		KVM_CoreShutdown();
		return false;
	}

	kvm.vm = ioctl(kvm.dev,KVM_CREATE_VM,0);
	if (kvm.vm < 0) {
		reason = KVM_Error("KVM_CREATE_VM");
		KVM_CoreShutdown();
		return false;
	}
	if (ioctl(kvm.vm,KVM_SET_TSS_ADDR,kvm_tss_addr) < 0) {
		reason = KVM_Error("KVM_SET_TSS_ADDR");
		KVM_CoreShutdown();
		return false;
	}

	/* No KVM_CREATE_IRQCHIP: DOSBox-X emulates the PIC and PIT itself */

	kvm.vcpu = ioctl(kvm.vm,KVM_CREATE_VCPU,0);
	if (kvm.vcpu < 0) {
		reason = KVM_Error("KVM_CREATE_VCPU");
		KVM_CoreShutdown();
		return false;
	}

	const int run_size = ioctl(kvm.dev,KVM_GET_VCPU_MMAP_SIZE,0);
	if (run_size < (int)sizeof(struct kvm_run)) {
		reason = KVM_Error("KVM_GET_VCPU_MMAP_SIZE");
		KVM_CoreShutdown();
		return false;
	}
	kvm.run_size = (size_t)run_size;
	void *run = mmap(NULL,kvm.run_size,PROT_READ|PROT_WRITE,MAP_SHARED,kvm.vcpu,0);
	if (run == MAP_FAILED) {
		reason = KVM_Error("mmap of the vCPU run area");
		KVM_CoreShutdown();
		return false;
	}
	kvm.run = (struct kvm_run*)run;

	if (!KVM_SetCPUID(reason)) {
		KVM_CoreShutdown();
		return false;
	}

	if (!kvm.exit_registered) {
		kvm.exit_registered = true;
		AddExitFunction(AddExitFunctionFuncPair(KVM_CoreShutdownSection));
	}

	LOG_MSG("KVM: virtual machine and vCPU created (API %d, %d memory slots, immediate exit %s)",
		KVM_API_VERSION,kvm.memslots,ioctl(kvm.dev,KVM_CHECK_EXTENSION,KVM_CAP_IMMEDIATE_EXIT) > 0 ? "supported" : "not supported");
	return true;
}

#endif /* C_KVM */
