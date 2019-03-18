
#include <iostream>

#include <iconv.h>

using namespace std;

class Iconv {
public:
    Iconv(const iconv_t &ctx) : context(ctx) { /* takes ownership of ctx */
    }
    ~Iconv() {
        close();
    }
public:
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
    static constexpr iconv_t    notalloc = (iconv_t)(-1);
private:
    iconv_t                     context = notalloc;
};

int main() {
    Iconv *x = Iconv::create("UTF-8","ISO_8859-1");
    if (x == NULL) {
        cerr << "Failed to create context" << endl;
        return 1;
    }

    delete x;
    return 0;
}

