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

#ifndef DOSBOX_CORE_KVM_H
#define DOSBOX_CORE_KVM_H

#include <string>

/* The KVM CPU core: runs guest code with hardware virtualization through the Linux KVM
 * interface (core=kvm, built with --enable-kvm). See KVM-TODO.md for the plan and status. */

#if C_KVM
/* Create the KVM virtual machine and its vCPU, or bring an existing one up to date with the
 * CPU settings (CPUID). Call it after the CPU type and features are known. On failure, says
 * why in 'reason' (for the log) and returns false. */
bool KVM_CoreInit(std::string &reason);

/* Close the virtual machine */
void KVM_CoreShutdown(void);
#endif

#endif
