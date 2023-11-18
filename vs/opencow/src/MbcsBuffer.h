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

#ifndef INCLUDED_MbcsBuffer
#define INCLUDED_MbcsBuffer

#include <stdlib.h>
#include <errno.h>

#if !defined(OCOW_API)
#define OCOW_API WINAPI
#endif

//for static linking (hack. they are read only symbols)
#if defined(STATIC_OPENCOW)

//a faked "imported entry" is added as if the imported symbol is a function pointer
//naked function _imp__xxx() {
//raw addr of next (as a 'pointer')
//next: jmp xxx()
//}
#   define OCOW_DEF(RetT, x, ...) RetT OCOW_API x __VA_ARGS__ asm("_ocow_"#x); \
                                   RetT OCOW_API __attribute__((naked)) __MINGW_IMP_SYMBOL(x) __VA_ARGS__ {asm(".long 1f \n\t 1: jmp _ocow_"#x);} \
                                   RetT OCOW_API x __VA_ARGS__
#else

#   define OCOW_DEF(RetT, x, ...) RetT OCOW_API x __VA_ARGS__

#endif

#define ARRAY_SIZE(x)   (sizeof(x)/sizeof((x)[0]))


// ----------------------------------------------------------------------------
// UNICODE -> MBCS conversion and general buffer

class CMbcsBuffer
{
public:
    CMbcsBuffer()
        : mBuffer(mStackBuffer),
          mBufferSize(sizeof(mStackBuffer)),
          mLength(0)
    {
        mStackBuffer[0] = '\0';
    }

    ~CMbcsBuffer()
    {
        if (IsBufferAllocated())
            ::free(mBuffer);
    }

    bool SetCapacity(int aMinCapacity);

    // if the source string is NULL then we will return NULL, regardless of the setting
    // for aMinCapacity. If you want a resizable buffer then use SetCapacity() instead.
    bool FromUnicode(LPCWSTR aString = 0, int aStringLen = -1, int aMinCapacity = 0);

    void SetNull()
    {
        if (IsBufferAllocated())
            ::free(mBuffer);
        mBuffer     = 0;
        mBufferSize = 0;
        mLength     = 0;
    }

    bool IsBufferAllocated() const { return (mBuffer && mBuffer != mStackBuffer); }

    char * get()            { return mBuffer; }
    operator LPSTR()        { return mBuffer; }
    int BufferSize() const  { return mBufferSize; }
    int Length() const      { return mLength; }

private:
    char    mStackBuffer[256];
    char *  mBuffer;
    int     mBufferSize;
    int     mLength;
};

#endif // INCLUDED_MbcsBuffer
