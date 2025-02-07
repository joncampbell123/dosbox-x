
#ifndef ZIPSTREAMBUF
#define ZIPSTREAMBUF

#include "zip.h"
#include "unzip.h"
#include "ioapi.h"

#include <assert.h>

/* std::streambuf for writing to ZIP archive directly.
 * ZIP archive writer can only write one file at a time, do not
 * use multiple instances of this C++ class at a time! No seeking,
 * only sequential output! */
class zip_ostreambuf : public std::streambuf {
public:
	using Base = std::streambuf;
public:
	zip_ostreambuf(zipFile &n_zf) : basic_streambuf(), zf(n_zf) { }
	virtual ~zip_ostreambuf() { }
public:
	virtual std::streamsize xsputn(const Base::char_type *s, std::streamsize count) override {
		assert(zf != NULL);

		const int err = zipWriteInFileInZip(zf, (void*)s, count);
		if (err != ZIP_OK) {
			zf_err = err;
			return 0;
		}

		return count;
	}
public:
	int close(void) {
		int err;

		if ((err=zipCloseFileInZip(zf)) != ZIP_OK) return err;
		if (zf_err != ZIP_OK) return zf_err;
		return ZIP_OK;
	}
private:
	zipFile zf = NULL;
	int zf_err = ZIP_OK;
};

#endif

