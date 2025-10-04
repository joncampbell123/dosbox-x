
#include "iconvpp.hpp"

using namespace std;

// borrowed from DOSLIB
#define THIS_IS_JAPANESE "\x82\xb1\x82\xea\x82\xcd\x93\xfa\x96\x7b\x8c\xea\x82\xc5\x82\xb7"/* UTF-8 to Shift-JIS of "これは日本語です" */

int main() {
    Iconv *x = Iconv::create(/*TO*/"UTF-8",/*FROM*/"SHIFT-JIS");
    if (x == NULL) {
        cerr << "Failed to create context" << endl;
        return 1;
    }

    {
        char tmp[512];
        const char *src = THIS_IS_JAPANESE;

        x->set_src(src);
        x->set_dest(tmp,sizeof(tmp));

        int err = x->string_convert();

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
        std::string dst;
        const char *src = THIS_IS_JAPANESE;

        x->set_src(src);

        int err = x->string_convert_dest(dst);

        if (err < 0) {
            cerr << "Conversion failed, " << Iconv::errstring(err) << endl;
            return 1;
        }

        cout << "Test 1: " << src << endl;
        cout << "   Res: " << dst << endl;
        cout << "  Read: " << x->get_src_last_read() << endl;
        cout << " Wrote: " << x->get_dest_last_written() << endl;
    }

    {
        char tmp[512];
        const char *src = THIS_IS_JAPANESE;

        x->set_dest(tmp,sizeof(tmp));

        int err = x->string_convert_src(src);

        if (err < 0) {
            cerr << "Conversion failed, " << Iconv::errstring(err) << endl;
            return 1;
        }

        cout << "Test 1: " << src << endl;
        cout << "   Res: " << tmp << endl;
        cout << "  Read: " << x->get_src_last_read() << endl;
        cout << " Wrote: " << x->get_dest_last_written() << endl;
    }

    {
        char tmp[512];
        const std::string src = THIS_IS_JAPANESE;

        x->set_dest(tmp,sizeof(tmp));

        int err = x->string_convert_src(src);

        if (err < 0) {
            cerr << "Conversion failed, " << Iconv::errstring(err) << endl;
            return 1;
        }

        cout << "Test 1: " << src << endl;
        cout << "   Res: " << tmp << endl;
        cout << "  Read: " << x->get_src_last_read() << endl;
        cout << " Wrote: " << x->get_dest_last_written() << endl;
    }

    {
        std::string dst;
        const std::string src = THIS_IS_JAPANESE;

        int err = x->string_convert(dst,src);

        if (err < 0) {
            cerr << "Conversion failed, " << Iconv::errstring(err) << endl;
            return 1;
        }

        cout << "Test 1: " << src << endl;
        cout << "   Res: " << dst << endl;
        cout << "  Read: " << x->get_src_last_read() << endl;
        cout << " Wrote: " << x->get_dest_last_written() << endl;
    }

    {
        const std::string src = THIS_IS_JAPANESE;
        std::string dst = x->string_convert(src);

        cout << "Test 1: " << src << endl;
        cout << "   Res: " << dst << endl;
        cout << "  Read: " << x->get_src_last_read() << endl;
        cout << " Wrote: " << x->get_dest_last_written() << endl;
    }

    delete x;
    return 0;
}

