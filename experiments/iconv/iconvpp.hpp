
#include <iostream>
#include <exception>
#include <stdexcept>

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <iconv.h>

class Iconv {
public:
    explicit Iconv(const iconv_t &ctx); /* takes ownership of ctx */
    Iconv(const Iconv *p) = delete;
    Iconv(const Iconv &other) = delete; /* no copying */
    Iconv(const Iconv &&other) = delete; /* no moving */
    Iconv() = delete;
    ~Iconv();
public:
    void finish(void);

    void set_dest(char * const dst,char * const dst_fence);
    void set_dest(char * const dst,const size_t len) {
        set_dest(dst,dst+len);
    }
    void set_dest(char * const dst) = delete; /* <- NO! Prevent C-string calls to std::string &dst function! */
    void set_dest(std::string &dst) { /* <- use string::resize() first before calling this */
        if (dst.size() == 0) dst.resize(256);
        set_dest(&dst[0],dst.size());
    }

    void set_src(const char * const src,const char * const src_fence);
    void set_src(const char * const src,const size_t len) {
        set_src(src,src+len);
    }
    void set_src(const std::string &src) { // C++-string
        set_src(src.c_str(),src.length());
    }
    void set_src(const char * const src) { // C-string
        set_src(src,strlen(src));
    }
public:
    int raw_convert(void);
    int string_convert(std::string &dst,const std::string &src);
    int cstring_convert(void);
    int string_convert_src(const std::string &src) {
        set_src(src);

        int err = cstring_convert();

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
    static const char *errstring(int x);
    static Iconv *create(const char *to,const char *from);
private:
    void close(void);
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

