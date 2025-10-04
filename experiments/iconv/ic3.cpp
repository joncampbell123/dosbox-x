
#include "iconvpp.hpp"

using namespace std;

int main() {
    IconvFromW *x = IconvFromW::create(/*TO*/"UTF-8");
    if (x == NULL) {
        cerr << "Failed to create context" << endl;
        return 1;
    }

    {
        char tmp[512];
        const wchar_t *src = L"Hello world \xC0\xC1\xC2";

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
        char tmp[512];
        const wchar_t *src = L"\xC8\xC9\xCA Hello world \xC0\xC1\xC2";

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
        const wchar_t *src = L"\xC8\xC9\xCA Hello world \xC0\xC1\xC2";

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
        const std::wstring src = L"\xC8\xC9\xCA Hello world \xC0\xC1\xC2";

        x->set_dest(tmp,sizeof(tmp));

        int err = x->string_convert_src(src);

        if (err < 0) {
            cerr << "Conversion failed, " << Iconv::errstring(err) << endl;
            return 1;
        }

       wcout <<L"Test 1: " << src << endl;
        cout << "   Res: " << tmp << endl;
        cout << "  Read: " << x->get_src_last_read() << endl;
        cout << " Wrote: " << x->get_dest_last_written() << endl;
    }

    {
        std::string dst;
        const std::wstring src = L"\xC8\xC9\xCA Hello world \xC0\xC1\xC2";

        int err = x->string_convert(dst,src);

        if (err < 0) {
            cerr << "Conversion failed, " << Iconv::errstring(err) << endl;
            return 1;
        }

       wcout <<L"Test 1: " << src << endl;
        cout << "   Res: " << dst << endl;
        cout << "  Read: " << x->get_src_last_read() << endl;
        cout << " Wrote: " << x->get_dest_last_written() << endl;
    }

    {
        const std::wstring src = L"\xC8\xC9\xCA Hello world \xC0\xC1\xC2";
        std::string dst = x->string_convert(src);

       wcout <<L"Test 1: " << src << endl;
        cout << "   Res: " << dst << endl;
        cout << "  Read: " << x->get_src_last_read() << endl;
        cout << " Wrote: " << x->get_dest_last_written() << endl;
    }

    {
        const wchar_t *src = L"\xC8\xC9\xCA Hello world \xC0\xC1\xC2";
        std::string dst = x->string_convert(src);

       wcout <<L"Test 1: " << src << endl;
        cout << "   Res: " << dst << endl;
        cout << "  Read: " << x->get_src_last_read() << endl;
        cout << " Wrote: " << x->get_dest_last_written() << endl;
    }

    delete x;
    return 0;
}

