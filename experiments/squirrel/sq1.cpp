
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <iostream>
#include <fstream>

using namespace std;

extern "C" {
#include <squirrel.h>
#include <sqstdio.h>
#include <sqstdblob.h>
#include <sqstdsystem.h>
#include <sqstdio.h>
#include <sqstdmath.h>
#include <sqstdstring.h>
#include <sqstdaux.h>
}

#ifdef SQUNICODE
#define scfprintf fwprintf
#define scvprintf vfwprintf
#else
#define scfprintf fprintf
#define scvprintf vfprintf
#endif

void printfunc(HSQUIRRELVM SQ_UNUSED_ARG(v),const SQChar *s,...)
{
    va_list vl;
    va_start(vl, s);
    scvprintf(stdout, s, vl);
    va_end(vl);
    (void)v; /* UNUSED */
}

void errorfunc(HSQUIRRELVM SQ_UNUSED_ARG(v),const SQChar *s,...)
{
    va_list vl;
    va_start(vl, s);
    scvprintf(stderr, s, vl);
    va_end(vl);
}

int main(int argc,char **argv) {
    HSQUIRRELVM sq;
    ifstream i;

    if (argc > 1)
        i.open(argv[1],ios_base::in);
    else
        i.open("sq1.sq",ios_base::in);

    if (!i.is_open()) return 1;

    sq = sq_open(1024);
    if (sq == NULL) return 1;

    sqstd_register_bloblib(sq);
    sqstd_register_iolib(sq);
    sqstd_register_systemlib(sq);
    sqstd_register_mathlib(sq);
    sqstd_register_stringlib(sq);

    sq_setprintfunc(sq,printfunc,errorfunc);

    sqstd_seterrorhandlers(sq);

    char *blob;
    off_t sz;

    sz = 65536;
    blob = new char[sz]; /* or throw a C++ exception on fail */

    i.read(blob,sz-1);
    {
        streamsize rd = i.gcount();
        assert(rd < sz);
        blob[rd] = 0;
    }

    if (SQ_SUCCEEDED(sq_compilebuffer(sq,blob,strlen(blob),"file",SQTrue))) {
        sq_pushroottable(sq);
        if (SQ_SUCCEEDED(sq_call(sq,1,0,SQTrue))) {
        }
        else {
            fprintf(stderr,"Failed to call function\n");
        }
    }
    else {
        fprintf(stderr,"Failed to compile buffer\n");
    }

    delete[] blob;

    sq_close(sq);
    i.close();
    return 0;
}

