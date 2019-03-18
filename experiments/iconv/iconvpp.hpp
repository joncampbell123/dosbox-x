
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
    void set_dest(char * const dst,const size_t len);
    void set_src(const char * const src,const char * const src_fence);
    void set_src(const char * const src);
    void set_src(const std::string &src);
    inline size_t get_src_last_read(void) const {
        return src_adv;
    }
    inline size_t get_dest_last_written(void) const {
        return dst_adv;
    }
public:
    int raw_convert(void);
    std::string string_convert(const std::string &src);
    int cstring_convert(std::string &dst,const std::string &src);
    int cstring_convert(const std::string &src);
    int cstring_convert(const char *src);
    int cstring_convert(void);
    inline bool eof(void) const {
        return src_ptr >= src_ptr_fence;
    }
    inline bool eof_dest(void) const {
        return dst_ptr >= dst_ptr_fence;
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

