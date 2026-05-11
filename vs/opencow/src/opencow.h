/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is for Open Layer for Unicode (opencow).
 *
 * The Initial Developer of the Original Code is Brodie Thiesfield.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef OPENCOW_H
#define OPENCOW_H
#include <stdlib.h> //__MINGW64_VERSION_MAJOR

#if !defined(OCOW_API)
#define OCOW_API WINAPI
#endif

//for static linking
#if defined(STATIC_OPENCOW)

#if !defined(__MINGW64_VERSION_MAJOR) //https://sourceforge.net/p/predef/wiki/Compilers/
//legacy MinGW32 (no "_imp_" prefix, no indirect call)

// #ifndef __MINGW_IMP_SYMBOL
// #define __MINGW_IMP_SYMBOL(x) x
// #endif

#   define OCOW_DEF(RetT, x, ...) RetT OCOW_API x __VA_ARGS__

#else
//MinGW32 on MinGW-w64
//indrect call with __imp_wrapper

//(hack. they are read only symbols)
//a faked "imported entry" is added as if the imported symbol is a function pointer:
//naked function _imp__xxx() {
//raw addr of next (as a 'pointer')
//next: jmp xxx()
//}
//asm name (symbol) is used for the internal implementation xxx(), because name mangling for stdcall is hard to utilize.
//asm name used is used for the "export" function to avoid function re-definitions.

#   define OCOW_DEF(RetT, x, ...) RetT OCOW_API ocow_##x __VA_ARGS__ asm("_ocow_"#x); \
                                   RetT OCOW_API __attribute__((naked)) __MINGW_IMP_SYMBOL(x) __VA_ARGS__ {asm(".long 1f \n\t 1: jmp _ocow_"#x);} \
                                   RetT OCOW_API ocow_##x __VA_ARGS__

#endif

#else

#   define OCOW_DEF(RetT, x, ...) RetT OCOW_API x __VA_ARGS__

#endif

#endif //OPENCOW_H