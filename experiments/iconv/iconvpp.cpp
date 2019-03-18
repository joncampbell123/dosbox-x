
#include "iconvpp.hpp"

Iconv::Iconv(const iconv_t &ctx) : context(ctx) {
}

Iconv::~Iconv() {
    close();
}

void Iconv::finish(void) {
    dst_ptr = NULL;
    dst_ptr_fence = NULL;
    src_ptr = NULL;
    src_ptr_fence = NULL;
}

void Iconv::set_dest(char * const dst,char * const dst_fence) {
    if (dst == NULL || dst_fence == NULL || dst > dst_fence)
        throw std::invalid_argument("Iconv set_dest pointer out of range");

    dst_adv = 0;
    dst_ptr = dst;
    dst_ptr_fence = dst_fence;
}

void Iconv::set_src(const char * const src,const char * const src_fence) {
    if (src == NULL || src_fence == NULL || src > src_fence)
        throw std::invalid_argument("Iconv set_src pointer out of range");

    src_adv = 0;
    src_ptr = src;
    src_ptr_fence = src_fence;
}

int Iconv::raw_convert(void) {
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

        return ret;
    }

    return err_noinit;
}

int Iconv::cstring_convert(std::string &dst,const std::string &src) {
    dst.resize(std::max(dst.size(),((src.length()+4u)*4u)+2u)); // maximum 4 bytes/char expansion UTF-8 or bigger if caller resized already
    set_dest(&dst[0],dst.length());

    int err = cstring_convert(src);

    dst.resize(get_dest_last_written());

    finish();
    return err;
}

int Iconv::cstring_convert(void) {
    int err = raw_convert();

    if (err >= 0) {
        /* and then a NUL */
        if (dst_ptr >= dst_ptr_fence)
            return err_noroom;

        *dst_ptr++ = 0;
    }

    return err;
}

const char *Iconv::errstring(int x) {
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

Iconv *Iconv::create(const char *to,const char *from) { /* factory function */
    iconv_t ctx = iconv_open(to,from);
    if (ctx != notalloc) return new(std::nothrow) Iconv(ctx);
    return NULL;
}

void Iconv::close(void) {
    if (context != notalloc) {
        iconv_close(context);
        context = notalloc;
    }
}

