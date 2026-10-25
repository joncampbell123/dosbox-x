
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
	virtual ~zip_ostreambuf() { close(); }
public:
	virtual std::streamsize xsputn(const Base::char_type *s, std::streamsize count) override {
		assert(zf != NULL);

		/* Nothing to write. This must not reach zipWriteInFileInZip(): an empty write can come
		 * with a NULL pointer (WRITE_POD_SIZE of an unallocated buffer), and zlib's crc32()
		 * returns the initial value for a NULL buffer, which reset the CRC of the file being
		 * written and left a save state whose file failed its CRC check on load. */
		if (count <= 0) return 0;

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

class zip_istreambuf : public std::streambuf {
public:
	using Base = std::streambuf;
public:
	zip_istreambuf(unzFile &n_zf) : basic_streambuf(), zf(n_zf) { }
	virtual ~zip_istreambuf() { close(); }
public:
	virtual std::streamsize xsgetn(Base::char_type *s, std::streamsize count) override {
		assert(zf != NULL);

		const int err = unzReadCurrentFile(zf, (void*)s, count);
		if (err < 0) {
			zf_err = err;
			return 0;
		}

		return std::streamsize(err);
	}
public:
	int close(void) {
		int err;

		if ((err=unzCloseCurrentFile(zf)) != UNZ_OK) return err;
		if (zf_err != UNZ_OK) return zf_err;
		return UNZ_OK;
	}
private:
	unzFile zf = NULL;
	int zf_err = UNZ_OK;
};


#endif

