
#include <iostream>
#include <exception>
#include <stdexcept>

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#if defined(__MINGW32__)
# define ICONV_LITTLE_ENDIAN 1234
# define ICONV_BIG_ENDIAN 4321
# define ICONV_BYTE_ORDER ICONV_LITTLE_ENDIAN
#elif defined(__APPLE__)
# include <libkern/OSByteOrder.h>
# define ICONV_LITTLE_ENDIAN 1234
# define ICONV_BIG_ENDIAN 4321
# if defined(__LITTLE_ENDIAN__)
#  define ICONV_BYTE_ORDER ICONV_LITTLE_ENDIAN
# elif defined(__BIG_ENDIAN__)
#  define ICONV_BYTE_ORDER ICONV_BIG_ENDIAN
# else
#  error Unable to determine byte order
# endif
#else
# include <endian.h>
# define ICONV_BYTE_ORDER BYTE_ORDER
# define ICONV_LITTLE_ENDIAN LITTLE_ENDIAN
# define ICONV_BIG_ENDIAN BIG_ENDIAN
#endif

#if defined(_MSC_VER)
/* no */
#else
# define ENABLE_ICONV 1
#endif

#if defined(_WIN32)
# define ENABLE_ICONV_WIN32 1
#endif

/* common code to any templated version of _IconvBase */
class _Iconv_CommonBase {
public:
    static const char *errstring(int x);
    inline size_t get_src_last_read(void) const { /* in units of sizeof(srcT) */
        return src_adv;
    }
    inline size_t get_dest_last_written(void) const { /* in units of sizeof(dstT) */
        return dst_adv;
    }
public:
    size_t                      dst_adv = 0;
    size_t                      src_adv = 0;
public:
    static constexpr int        err_noinit = -EBADF;
    static constexpr int        err_noroom = -E2BIG;
    static constexpr int        err_notvalid = -EILSEQ;
    static constexpr int        err_incomplete = -EINVAL;
protected:
    static constexpr bool big_endian(void) {
        return (ICONV_BYTE_ORDER == ICONV_BIG_ENDIAN);
    }
};

template <typename srcT,typename dstT> class _Iconv;

/* base _Iconv implementation, common to all implementations */
template <typename srcT,typename dstT> class _IconvBase : public _Iconv_CommonBase {
public:
    /* NTS: The C++ standard defines std::string as std::basic_string<char>.
     *      These typedefs will match if srcT = char and dstT = char */
    typedef std::basic_string<srcT> src_string;
    typedef std::basic_string<dstT> dst_string;
public:
    _IconvBase() { }
    virtual ~_IconvBase() { }
public:
    void finish(void) {
        dst_ptr = NULL;
        dst_ptr_fence = NULL;
        src_ptr = NULL;
        src_ptr_fence = NULL;
    }

    void set_dest(dstT * const dst,dstT * const dst_fence) {
        if (dst == NULL || dst_fence == NULL || dst > dst_fence)
            throw std::invalid_argument("Iconv set_dest pointer out of range");

        dst_adv = 0;
        dst_ptr = dst;
        dst_ptr_fence = dst_fence;
    }
    void set_dest(dstT * const dst,const size_t len/*in units of sizeof(dstT)*/) {
        set_dest(dst,dst+len);
    }
    void set_dest(dstT * const dst) = delete; /* <- NO! Prevent C-string calls to std::string &dst function! */

    void set_src(const srcT * const src,const srcT * const src_fence) {
        if (src == NULL || src_fence == NULL || src > src_fence)
            throw std::invalid_argument("Iconv set_src pointer out of range");

        src_adv = 0;
        src_ptr = src;
        src_ptr_fence = src_fence;
    }
    void set_src(const srcT * const src,const size_t len) {
        set_src(src,src+len);
    }
    void set_src(const srcT * const src) { // C-string
        set_src(src,my_strlen(src));
    }
public:
    virtual int _do_convert(void) {
        return err_noinit;
    }
    int string_convert(dst_string &dst,const src_string &src) {
        dst.resize(std::max(dst.size(),((src.length()+4u)*4u)+2u)); // maximum 4 bytes/char expansion UTF-8 or bigger if caller resized already
        set_dest(dst); /* will apply new size to dst/fence pointers */

        int err = string_convert_src(src);

        dst.resize(get_dest_last_written());

        finish();
        return err;
    }
    int string_convert(void) {
        if (dst_ptr == NULL || src_ptr == NULL)
            return err_notvalid;
        if (dst_ptr > dst_ptr_fence)
            return err_notvalid;
        if (src_ptr > src_ptr_fence)
            return err_notvalid;

        int ret = _do_convert();

        if (ret >= 0) {
            /* add NUL */
            if (dst_ptr >= dst_ptr_fence)
                return err_noroom;

            *dst_ptr++ = 0;
        }

        return ret;
    }
    int string_convert_dest(dst_string &dst) {
        size_t srcl = (size_t)((uintptr_t)src_ptr_fence - (uintptr_t)src_ptr);

        dst.resize(std::max(dst.size(),((srcl+4u)*4u)+2u));
        set_dest(dst);

        int err = string_convert();

        finish();
        return err;
    }
    int string_convert_src(const src_string &src) {
        set_src(src);

        int err = string_convert();

        finish();
        return err;
    }
    dst_string string_convert(const src_string &src) {
        dst_string res;

        string_convert(res,src);

        return res;
    }
public:
    inline bool eof(void) const {
        return src_ptr >= src_ptr_fence;
    }
    inline bool eof_dest(void) const {
        return dst_ptr >= dst_ptr_fence;
    }
    inline const srcT *get_srcp(void) const {
        return src_ptr;
    }
    inline const dstT *get_destp(void) const {
        return dst_ptr;
    }
protected:
    static inline size_t my_strlen(const char *s) {
        return strlen(s);
    }
    static inline size_t my_strlen(const wchar_t *s) {
        return wcslen(s);
    }
    template <typename X> static inline size_t my_strlen(const X *s) {
        size_t c = 0;

        while ((*s++) != 0) c++;

        return c;
    }
protected:
    void set_dest(dst_string &dst) { /* PRIVATE: External use can easily cause use-after-free bugs */
        set_dest(&dst[0],dst.size());
    }
    void set_src(const src_string &src) { /* PRIVATE: External use can easily cause use-after-free bugs */
        set_src(src.c_str(),src.length());
    }
protected:
    dstT*                       dst_ptr = NULL;
    dstT*                       dst_ptr_fence = NULL;
    const srcT*                 src_ptr = NULL;
    const srcT*                 src_ptr_fence = NULL;

    friend _Iconv<srcT,dstT>;
};

#if defined(ENABLE_ICONV)
# include <iconv.h>

/* _Iconv implementation of _IconvBase using GNU libiconv or GLIBC iconv, for Linux and Mac OS X systems. */
/* See also: "man iconv"
 * See also: [http://man7.org/linux/man-pages/man3/iconv.3.html] */
template <typename srcT,typename dstT> class _Iconv : public _IconvBase<srcT,dstT> {
protected:
    using pclass = _IconvBase<srcT,dstT>;
public:
    explicit _Iconv(const iconv_t &ctx) : context(ctx) {/* takes ownership of ctx */
    }
    _Iconv(const _Iconv *p) = delete;
    _Iconv(const _Iconv &other) = delete; /* no copying */
    _Iconv(const _Iconv &&other) = delete; /* no moving */
    _Iconv() = delete;
    virtual ~_Iconv() {
        close();
    }
public:
    virtual int _do_convert(void) {
        if (context != NULL) {
            dstT *i_dst = pclass::dst_ptr;
            const srcT *i_src = pclass::src_ptr;
            size_t src_left = (size_t)((uintptr_t)((char*)pclass::src_ptr_fence) - (uintptr_t)((char*)pclass::src_ptr));
            size_t dst_left = (size_t)((uintptr_t)((char*)pclass::dst_ptr_fence) - (uintptr_t)((char*)pclass::dst_ptr));

            iconv(context,NULL,NULL,NULL,NULL);

            /* Ref: [http://man7.org/linux/man-pages/man3/iconv.3.html] */
            int ret = iconv(context,(char**)(&(pclass::src_ptr)),&src_left,(char**)(&(pclass::dst_ptr)),&dst_left);

            pclass::src_adv = (size_t)(pclass::src_ptr - i_src);
            pclass::dst_adv = (size_t)(pclass::dst_ptr - i_dst);

            if (ret < 0) {
                if (errno == E2BIG)
                    return pclass::err_noroom;
                else if (errno == EILSEQ)
                    return pclass::err_notvalid;
                else if (errno == EINVAL)
                    return pclass::err_incomplete;

                return pclass::err_notvalid;
            }

            return ret;
        }

        return pclass::err_noinit;
    }
public:
    static _Iconv<srcT,dstT> *create(const char *nw) { /* factory function, wide to char, or char to wide */
        if (sizeof(dstT) == sizeof(char) && sizeof(srcT) > sizeof(char)) {
            const char *wchar_encoding = _get_wchar_encoding<srcT>();
            if (wchar_encoding == NULL) return NULL;

            iconv_t ctx = iconv_open(/*TO*/nw,/*FROM*/wchar_encoding); /* from wchar to codepage nw */
            if (ctx != iconv_t(-1)) return new(std::nothrow) _Iconv<srcT,dstT>(ctx);
        }
        else if (sizeof(dstT) > sizeof(char) && sizeof(srcT) == sizeof(char)) {
            const char *wchar_encoding = _get_wchar_encoding<dstT>();
            if (wchar_encoding == NULL) return NULL;

            iconv_t ctx = iconv_open(/*TO*/wchar_encoding,/*FROM*/nw); /* from codepage new to wchar */
            if (ctx != iconv_t(-1)) return new(std::nothrow) _Iconv<srcT,dstT>(ctx);
        }

        return NULL;
    }
    static _Iconv<srcT,dstT> *create(const char *to,const char *from) { /* factory function */
        if (sizeof(dstT) == sizeof(char) && sizeof(srcT) == sizeof(char)) {
            iconv_t ctx = iconv_open(to,from);
            if (ctx != iconv_t(-1)) return new(std::nothrow) _Iconv<srcT,dstT>(ctx);
        }

        return NULL;
    }
protected:
    void close(void) {
        if (context != NULL) {
            iconv_close(context);
            context = NULL;
        }
    }
    template <typename W> static const char *_get_wchar_encoding(void) {
        if (sizeof(W) == 4)
            return pclass::big_endian() ? "UTF-32BE" : "UTF-32LE";
        else if (sizeof(W) == 2)
            return pclass::big_endian() ? "UTF-16BE" : "UTF-16LE";

        return NULL;
    }
protected:
    iconv_t                     context = NULL;
};

/* Most of the time the Iconv form will be used, for Mac OS X and Linux platforms where UTF-8 is common.
 *
 * Conversion to/from wchar is intended for platforms like Microsoft Windows 98/ME/2000/XP/Vista/7/8/10/etc
 * where the Win32 API functions take WCHAR (UTF-16 or UCS-16), in which case, the code will continue to
 * use UTF-8 internally but convert to WCHAR when needed. For example, Win32 function CreateFileW().
 *
 * Note that because of the UTF-16 world of Windows, Microsoft C++ defines wchar_t as an unsigned 16-bit
 * integer.
 *
 * Linux and other OSes however define wchar_t as a 32-bit integer, but do not use wchar_t APIs, and often
 * instead use UTF-8 for unicode, so the wchar_t versions will not see much use there. */
typedef _Iconv<char,char>    Iconv;
typedef _Iconv<char,wchar_t> IconvToW;
typedef _Iconv<wchar_t,char> IconvFromW;

#endif // ENABLE_ICONV

#if defined(ENABLE_ICONV_WIN32)
# include <windows.h>

/* Alternative implementation (char to WCHAR, or WCHAR to char only) using Microsoft Win32 APIs instead of libiconv.
 * For use with embedded or low memory Windows installations or environments where the added load of libiconv would
 * be undesirable. */

/* _IconvWin32 implementation of _IconvBase using Microsoft Win32 code page and WCHAR support functions for Windows 2000/XP/Vista/7/8/10/etc */
template <typename srcT,typename dstT> class _IconvWin32 : public _IconvBase<srcT,dstT> {
protected:
    using pclass = _IconvBase<srcT,dstT>;
public:
    explicit _IconvWin32(const UINT _codepage) : codepage(_codepage) {
    }
    _IconvWin32(const _IconvWin32 *p) = delete;
    _IconvWin32(const _IconvWin32 &other) = delete; /* no copying */
    _IconvWin32(const _IconvWin32 &&other) = delete; /* no moving */
    _IconvWin32() = delete;
    virtual ~_IconvWin32() {
    }
public:
    virtual int _do_convert(void) {
        if (codepage != 0u) {
            size_t src_left = (size_t)((uintptr_t)((char*)pclass::src_ptr_fence) - (uintptr_t)((char*)pclass::src_ptr));
            size_t dst_left = (size_t)((uintptr_t)((char*)pclass::dst_ptr_fence) - (uintptr_t)((char*)pclass::dst_ptr));
            int ret;

            if (sizeof(dstT) == sizeof(char) && sizeof(srcT) == sizeof(WCHAR)) {
                /* Convert wide char to multibyte using the Win32 API.
                 * See also: [https://docs.microsoft.com/en-us/windows/desktop/api/stringapiset/nf-stringapiset-widechartomultibyte] */
                ret = WideCharToMultiByte(codepage,0,(WCHAR*)pclass::src_ptr,src_left/sizeof(srcT),(char*)pclass::dst_ptr,dst_left,NULL,NULL);
                pclass::src_adv = src_left;
                pclass::src_ptr += pclass::src_adv;
                pclass::dst_adv = ret;
                pclass::dst_ptr += pclass::dst_adv;
            }
            else if (sizeof(dstT) == sizeof(WCHAR) && sizeof(srcT) == sizeof(char)) {
                /* Convert multibyte to wide char using the Win32 API.
                 * See also: [https://docs.microsoft.com/en-us/windows/desktop/api/stringapiset/nf-stringapiset-multibytetowidechar] */
                ret = MultiByteToWideChar(codepage,0,(char*)pclass::src_ptr,src_left,(WCHAR*)pclass::dst_ptr,dst_left/sizeof(dstT));
                pclass::src_adv = src_left;
                pclass::src_ptr += pclass::src_adv;
                pclass::dst_adv = ret;
                pclass::dst_ptr += pclass::dst_adv;
            }
            else {
                pclass::src_adv = 0;
                pclass::dst_adv = 0;
                ret = 0;
            }

            if (ret == 0) {
                DWORD err = GetLastError();

                if (err == ERROR_INSUFFICIENT_BUFFER)
                    return pclass::err_noroom;
                else if (err == ERROR_NO_UNICODE_TRANSLATION)
                    return pclass::err_notvalid;

                return pclass::err_noinit;
            }

            return 0;
        }

        return pclass::err_noinit;
    }
public:
    static _IconvWin32<srcT,dstT> *create(const UINT codepage) { /* factory function, WCHAR to char or char to WCHAR */
	CPINFO cpi;

	/* Test whether the code page exists */
	if (!GetCPInfo(codepage,&cpi))
		return NULL;

        if ((sizeof(dstT) == sizeof(char) && sizeof(srcT) == sizeof(WCHAR)) ||
            (sizeof(dstT) == sizeof(WCHAR) && sizeof(srcT) == sizeof(char)))
            return new(std::nothrow) _IconvWin32<srcT,dstT>(codepage);

        return NULL;
    }
protected:
    UINT                        codepage = 0;
};

typedef _IconvWin32<char,WCHAR> IconvWin32ToW;
typedef _IconvWin32<WCHAR,char> IconvWin32FromW;

#endif // ENABLE_ICONV_WIN32

