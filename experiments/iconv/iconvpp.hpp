
#include <iostream>
#include <exception>
#include <stdexcept>

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#if defined(__MINGW32__)
# define LITTLE_ENDIAN 1234
# define BIG_ENDIAN 4321
# define BYTE_ORDER LITTLE_ENDIAN
#elif defined(__APPLE__)
# include <libkern/OSByteOrder.h>
# define LITTLE_ENDIAN 1234
# define BIG_ENDIAN 4321
# if defined(__LITTLE_ENDIAN__)
#  define BYTE_ORDER LITTLE_ENDIAN
# elif defined(__BIG_ENDIAN__)
#  define BYTE_ORDER BIG_ENDIAN
# else
#  error Unable to determine byte order
# endif
#else
# include <endian.h>
#endif

#include <iconv.h>

class _Iconv_CommonBase {
public:
    static const char *errstring(int x) {
        if (x >= 0)
            return "no error";
        else if (x == err_noinit)
            return "not initialized";
        else if (x == err_noroom)
            return "out of room";
        else if (x == err_notvalid)
            return "illegal multibyte sequence or invalid state";
        else if (x == err_incomplete)
            return "incomplete multibyte sequence";

        return "?";
    }
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
    template <typename W> static const char *_get_wchar_encoding(void) {
        if (sizeof(W) == 4)
            return big_endian() ? "UTF-32BE" : "UTF-32LE";
        else if (sizeof(W) == 2)
            return big_endian() ? "UTF-16BE" : "UTF-16LE";

        return NULL;
    }
    static constexpr bool big_endian(void) {
        return (BYTE_ORDER == BIG_ENDIAN);
    }
};

template <typename srcT,typename dstT> class _Iconv : public _Iconv_CommonBase {
public:
    /* NTS: The C++ standard defines std::string as std::basic_string<char>.
     *      These typedefs will match if srcT = char and dstT = char */
    typedef std::basic_string<srcT> src_string;
    typedef std::basic_string<dstT> dst_string;
public:
    explicit _Iconv(const iconv_t &ctx) : context(ctx) {/* takes ownership of ctx */
    }
    _Iconv(const _Iconv *p) = delete;
    _Iconv(const _Iconv &other) = delete; /* no copying */
    _Iconv(const _Iconv &&other) = delete; /* no moving */
    _Iconv() = delete;
    ~_Iconv() {
        close();
    }
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
    int string_convert(dst_string &dst,const src_string &src) {
        dst.resize(std::max(dst.size(),((src.length()+4u)*4u)+2u)); // maximum 4 bytes/char expansion UTF-8 or bigger if caller resized already
        set_dest(dst); /* will apply new size to dst/fence pointers */

        int err = string_convert_src(src);

        dst.resize(get_dest_last_written());

        finish();
        return err;
    }
    int string_convert(void) {
        if (context != NULL) {
            if (dst_ptr == NULL || src_ptr == NULL)
                return err_notvalid;
            if (dst_ptr > dst_ptr_fence)
                return err_notvalid;
            if (src_ptr > src_ptr_fence)
                return err_notvalid;

            dstT *i_dst = dst_ptr;
            const srcT *i_src = src_ptr;
            size_t src_left = (size_t)((uintptr_t)((char*)src_ptr_fence) - (uintptr_t)((char*)src_ptr));
            size_t dst_left = (size_t)((uintptr_t)((char*)dst_ptr_fence) - (uintptr_t)((char*)dst_ptr));

            iconv(context,NULL,NULL,NULL,NULL);

            int ret = iconv(context,(char**)(&src_ptr),&src_left,(char**)(&dst_ptr),&dst_left);

            src_adv = (size_t)(src_ptr - i_src);
            dst_adv = (size_t)(dst_ptr - i_dst);

            if (ret < 0) {
                if (errno == E2BIG)
                    return err_noroom;
                else if (errno == EILSEQ)
                    return err_notvalid;
                else if (errno == EINVAL)
                    return err_incomplete;

                return err_notvalid;
            }
            else {
                /* add NUL */
                if (dst_ptr >= dst_ptr_fence)
                    return err_noroom;

                *dst_ptr++ = 0;
            }

            return ret;
        }

        return err_noinit;
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
public:
    static _Iconv<srcT,dstT> *create(const char *nw) { /* factory function, wide to char, or char to wide */
        if (sizeof(dstT) == sizeof(char) && sizeof(srcT) > sizeof(char)) {
            const char *wchar_encoding = _get_wchar_encoding<srcT>();
            if (wchar_encoding == NULL) return NULL;

            iconv_t ctx = iconv_open(nw,wchar_encoding); /* from wchar to codepage nw */
            if (ctx != iconv_t(-1)) return new(std::nothrow) _Iconv<srcT,dstT>(ctx);
        }
        else if (sizeof(dstT) > sizeof(char) && sizeof(srcT) == sizeof(char)) {
            const char *wchar_encoding = _get_wchar_encoding<dstT>();
            if (wchar_encoding == NULL) return NULL;

            iconv_t ctx = iconv_open(wchar_encoding,nw); /* from codepage new to wchar */
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
private:
    void close(void) {
        if (context != NULL) {
            iconv_close(context);
            context = NULL;
        }
    }
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
private:
    void set_dest(dst_string &dst) { /* PRIVATE: External use can easily cause use-after-free bugs */
        set_dest(&dst[0],dst.size());
    }
    void set_src(const src_string &src) { /* PRIVATE: External use can easily cause use-after-free bugs */
        set_src(src.c_str(),src.length());
    }
private:
    dstT*                       dst_ptr = NULL;
    dstT*                       dst_ptr_fence = NULL;
    const srcT*                 src_ptr = NULL;
    const srcT*                 src_ptr_fence = NULL;
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
typedef _Iconv<char,char> Iconv;
typedef _Iconv<char,wchar_t> IconvToW;
typedef _Iconv<wchar_t,char> IconvFromW;

