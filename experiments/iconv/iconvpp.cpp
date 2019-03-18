
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
    Iconv(const iconv_t &ctx) : context(ctx) { /* takes ownership of ctx */
    }
    ~Iconv() {
        close();
    }
public:
    void finish(void) {
        dst_adv = 0;
        dst_ptr = NULL;
        dst_ptr_fence = NULL;
        src_adv = 0;
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
    void set_src(const char * const src,const char * const src_fence) {
        if (src == NULL || src_fence == NULL || src > src_fence)
            throw std::invalid_argument("Iconv set_src pointer out of range");

        src_adv = 0;
        src_ptr = src;
        src_ptr_fence = src_fence;
    }
    void set_src(const char * const src) { // C-string
        if (src == NULL)
            throw std::invalid_argument("Iconv set_src pointer out of range");

        const size_t len = strlen(src);
        set_src(src,src+len);
    }
    size_t get_src_last_read(void) const {
        return src_adv;
    }
    size_t get_dest_last_written(void) const {
        return dst_adv;
    }
public:
    int raw_convert(void) {
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
    int cstring_convert(const char *src) {
        set_src(src);
        return cstring_convert();
    }
    int cstring_convert(void) {
        int err = raw_convert();

        if (err >= 0) {
            /* and then a NUL */
            if (dst_ptr >= dst_ptr_fence)
                return err_noroom;

            *dst_ptr++ = 0;
        }

        return err;
    }
    bool eof(void) const {
        return src_ptr >= src_ptr_fence;
    }
    bool eof_dest(void) const {
        return dst_ptr >= dst_ptr_fence;
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
    static Iconv *create(const char *to,const char *from) { /* factory function */
        iconv_t ctx = iconv_open(to,from);
        if (ctx != notalloc) return new(std::nothrow) Iconv(ctx);
        return NULL;
    }
private:
    void close(void) {
        if (context != notalloc) {
            iconv_close(context);
            context = notalloc;
        }
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

using namespace std;

int main() {
    Iconv *x = Iconv::create(/*TO*/"UTF-8",/*FROM*/"ISO_8859-1");
    if (x == NULL) {
        cerr << "Failed to create context" << endl;
        return 1;
    }

    {
        char tmp[512];
        const char *src = "Hello world \xC0\xC1\xC2";

        x->set_src(src);
        x->set_dest(tmp,sizeof(tmp));

        int err = x->cstring_convert();

        if (err < 0) {
            cerr << "Conversion failed, " << Iconv::errstring(err) << endl;
            return 1;
        }

        cout << "Test 1: " << src << endl;
        cout << "   Res: " << tmp << endl;
        cout << "  Read: " << x->get_src_last_read() << endl;
        cout << " Wrote: " << x->get_dest_last_written() << endl;

        x->finish();
    }

    {
        char tmp[512];
        const char *src = "\xC8\xC9\xCA Hello world \xC0\xC1\xC2";

        x->set_dest(tmp,sizeof(tmp));

        int err = x->cstring_convert(src);

        if (err < 0) {
            cerr << "Conversion failed, " << Iconv::errstring(err) << endl;
            return 1;
        }

        cout << "Test 1: " << src << endl;
        cout << "   Res: " << tmp << endl;
        cout << "  Read: " << x->get_src_last_read() << endl;
        cout << " Wrote: " << x->get_dest_last_written() << endl;

        x->finish();
    }

    delete x;
    return 0;
}

