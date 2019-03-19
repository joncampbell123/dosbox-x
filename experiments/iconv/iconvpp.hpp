
#include <iostream>
#include <exception>
#include <stdexcept>

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <iconv.h>

template <typename srcT,typename dstT> class _Iconv {
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

    void set_dest(char * const dst,char * const dst_fence) {
        if (dst == NULL || dst_fence == NULL || dst > dst_fence)
            throw std::invalid_argument("Iconv set_dest pointer out of range");

        dst_adv = 0;
        dst_ptr = dst;
        dst_ptr_fence = dst_fence;
    }
    void set_dest(char * const dst,const size_t len) {
        set_dest(dst,dst+len);
    }
    void set_dest(char * const dst) = delete; /* <- NO! Prevent C-string calls to std::string &dst function! */

    void set_src(const char * const src,const char * const src_fence) {
        if (src == NULL || src_fence == NULL || src > src_fence)
            throw std::invalid_argument("Iconv set_src pointer out of range");

        src_adv = 0;
        src_ptr = src;
        src_ptr_fence = src_fence;
    }
    void set_src(const char * const src,const size_t len) {
        set_src(src,src+len);
    }
    void set_src(const char * const src) { // C-string
        set_src(src,strlen(src));
    }
public:
    int string_convert(std::string &dst,const std::string &src) {
        dst.resize(std::max(dst.size(),((src.length()+4u)*4u)+2u)); // maximum 4 bytes/char expansion UTF-8 or bigger if caller resized already
        set_dest(dst); /* will apply new size to dst/fence pointers */

        int err = string_convert_src(src);

        dst.resize(get_dest_last_written());

        finish();
        return err;
    }
    int string_convert(void) {
        if (context != notalloc) {
            if (dst_ptr == NULL || src_ptr == NULL)
                return err_notvalid;
            if (dst_ptr > dst_ptr_fence)
                return err_notvalid;
            if (src_ptr > src_ptr_fence)
                return err_notvalid;

            char *i_dst = dst_ptr;
            const char *i_src = src_ptr;
            size_t src_left = (size_t)((uintptr_t)src_ptr_fence - (uintptr_t)src_ptr);
            size_t dst_left = (size_t)((uintptr_t)dst_ptr_fence - (uintptr_t)dst_ptr);

            iconv(context,NULL,NULL,NULL,NULL);

            int ret = iconv(context,(char**)(&src_ptr),&src_left,&dst_ptr,&dst_left);

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
    int string_convert_dest(std::string &dst) {
        size_t srcl = (size_t)((uintptr_t)src_ptr_fence - (uintptr_t)src_ptr);

        dst.resize(std::max(dst.size(),((srcl+4u)*4u)+2u));
        set_dest(dst);

        int err = string_convert();

        finish();
        return err;
    }
    int string_convert_src(const std::string &src) {
        set_src(src);

        int err = string_convert();

        finish();
        return err;
    }
    std::string string_convert(const std::string &src) {
        std::string res;

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
    inline const char *get_srcp(void) const {
        return src_ptr;
    }
    inline const char *get_destp(void) const {
        return dst_ptr;
    }
    inline size_t get_src_last_read(void) const {
        return src_adv;
    }
    inline size_t get_dest_last_written(void) const {
        return dst_adv;
    }
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
    static _Iconv<srcT,dstT> *create(const char *to,const char *from) { /* factory function */
        iconv_t ctx = iconv_open(to,from);
        if (ctx != notalloc) return new(std::nothrow) _Iconv<srcT,dstT>(ctx);
        return NULL;
    }
private:
    void close(void) {
        if (context != notalloc) {
            iconv_close(context);
            context = notalloc;
        }
    }
private:
    void set_dest(std::string &dst) { /* <- use string::resize() first before calling this */
        /* this is PRIVATE to avoid future bugs and problems where set_dest() is given a std::string
         * that goes out of scope by mistake, or other possible use-after-free bugs. */
        set_dest(&dst[0],dst.size());
    }
    void set_src(const std::string &src) { // C++-string
        /* This is PRIVATE for a good reason: This will only work 100% reliably if the std::string
         * object lasts for the conversion, which is true if called from string_convert() but may
         * not be true if called from external code that might do something to pass in a std::string
         * that is destroyed before we can operate on it.
         *
         * For example, this will cause a use after free bug:
         *
         * set_src(std::string("Hello world") + " how are you");
         *
         * it constructs a temporary to hold the result of the addition operator, and then gives that
         * to set_src(). set_src() points at the string, but on return, the std::string is destroyed.
         * it might happen to work depending on the runtime environment, but it is a serious
         * use-after-free bug.
         *
         * To avoid that, do not expose this function publicly. */
        set_src(src.c_str(),src.length());
    }
public:
    static constexpr int        err_noinit = -EBADF;
    static constexpr int        err_noroom = -E2BIG;
    static constexpr int        err_notvalid = -EILSEQ;
    static constexpr int        err_incomplete = -EINVAL;
    static constexpr iconv_t    notalloc = (iconv_t)(-1);
private:
    char*                       dst_ptr = NULL;
    char*                       dst_ptr_fence = NULL;
    const char*                 src_ptr = NULL;
    const char*                 src_ptr_fence = NULL;
    size_t                      dst_adv = 0;
    size_t                      src_adv = 0;
    iconv_t                     context = notalloc;
};

typedef _Iconv<char,char> Iconv;

