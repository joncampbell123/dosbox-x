#include "np2glue.h"
//#include "compiler.h"

#include <sys/stat.h>
#include <time.h>

//#include "codecnv.h"
#include "dosio.h"


static OEMCHAR curpath[MAX_PATH];
static OEMCHAR *curfilep = curpath;

#define ISKANJI(c)	((((c) - 0xa1) & 0xff) < 0x5c)


void
dosio_init(void)
{

	/* nothing to do */
}

void
dosio_term(void)
{

	/* nothing to do */
}

/* ファイル操作 */
FILEH
file_open(const OEMCHAR *path)
{
	FILEH fh;

	fh = fopen(path, "rb+");
	if (fh)
		return fh;
	return fopen(path, "rb");
}

FILEH
file_open_rb(const OEMCHAR *path)
{

	return fopen(path, "rb");
}

FILEH
file_create(const OEMCHAR *path)
{

	return fopen(path, "wb+");
}

long
file_seek(FILEH handle, long pointer, int method)
{

	fseek(handle, pointer, method);
	return ftell(handle);
}

UINT
file_read(FILEH handle, void *data, UINT length)
{

	return (UINT)fread(data, 1, length, handle);
}

UINT
file_write(FILEH handle, const void *data, UINT length)
{

	return (UINT)fwrite(data, 1, length, handle);
}

short
file_close(FILEH handle)
{

	fclose(handle);
	return 0;
}

UINT
file_getsize(FILEH handle)
{
	struct stat sb;

	if (fstat(fileno(handle), &sb) == 0)
		return sb.st_size;
	return 0;
}

short
file_attr(const OEMCHAR *path)
{
	struct stat sb;
	short attr;

	if (stat(path, &sb) == 0) {
		if (S_ISDIR(sb.st_mode)) {
			return FILEATTR_DIRECTORY;
		}
		attr = 0;
		if (!(sb.st_mode & S_IWUSR)) {
			attr |= FILEATTR_READONLY;
		}
		return attr;
	}
	return -1;
}

static BOOL
cnvdatetime(struct stat *sb, DOSDATE *dosdate, DOSTIME *dostime)
{
	struct tm *ftime;

	ftime = localtime(&sb->st_mtime);
	if (ftime) {
		if (dosdate) {
			dosdate->year = ftime->tm_year + 1900;
			dosdate->month = ftime->tm_mon + 1;
			dosdate->day = ftime->tm_mday;
		}
		if (dostime) {
			dostime->hour = ftime->tm_hour;
			dostime->minute = ftime->tm_min;
			dostime->second = ftime->tm_sec;
		}
		return SUCCESS;
	}
	return FAILURE;
}

short
file_getdatetime(FILEH handle, DOSDATE *dosdate, DOSTIME *dostime)
{
	struct stat sb;

	if ((fstat(fileno(handle), &sb) == 0)
	 && (cnvdatetime(&sb, dosdate, dostime)))
		return 0;
	return -1;
}

short
file_delete(const OEMCHAR *path)
{

	return (short)unlink(path);
}

short
file_dircreate(const OEMCHAR *path)
{

	return (short)mkdir(path, 0777);
}


#if 0
/* カレントファイル操作 */
void
file_setcd(const OEMCHAR *exepath)
{

	milstr_ncpy(curpath, exepath, sizeof(curpath));
	curfilep = file_getname(curpath);
	*curfilep = '\0';
}

char *
file_getcd(const OEMCHAR *filename)
{

	*curfilep = '\0';
	file_catname(curpath, filename, sizeof(curpath));
	return curpath;
}

FILEH
file_open_c(const OEMCHAR *filename)
{

	*curfilep = '\0';
	file_catname(curpath, filename, sizeof(curpath));
	return file_open(curpath);
}

FILEH
file_open_rb_c(const OEMCHAR *filename)
{

	*curfilep = '\0';
	file_catname(curpath, filename, sizeof(curpath));
	return file_open_rb(curpath);
}

FILEH
file_create_c(const OEMCHAR *filename)
{

	*curfilep = '\0';
	file_catname(curpath, filename, sizeof(curpath));
	return file_create(curpath);
}

short
file_delete_c(const OEMCHAR *filename)
{

	*curfilep = '\0';
	file_catname(curpath, filename, sizeof(curpath));
	return file_delete(curpath);
}

short
file_attr_c(const OEMCHAR *filename)
{

	*curfilep = '\0';
	file_catname(curpath, filename, sizeof(curpath));
	return file_attr(curpath);
}
#endif

#if 0
FLISTH
file_list1st(const OEMCHAR *dir, FLINFO *fli)
{
	FLISTH ret;

	ret = (FLISTH)_MALLOC(sizeof(_FLISTH), "FLISTH");
	if (ret == NULL) {
		VERBOSE(("file_list1st: couldn't alloc memory (size = %d)", sizeof(_FLISTH)));
		return FLISTH_INVALID;
	}

	milstr_ncpy(ret->path, dir, sizeof(ret->path));
	file_setseparator(ret->path, sizeof(ret->path));
	ret->hdl = opendir(ret->path);
	VERBOSE(("file_list1st: opendir(%s)", ret->path));
	if (ret->hdl == NULL) {
		VERBOSE(("file_list1st: opendir failure"));
		_MFREE(ret);
		return FLISTH_INVALID;
	}
	if (file_listnext((FLISTH)ret, fli) == SUCCESS) {
		return (FLISTH)ret;
	}
	VERBOSE(("file_list1st: file_listnext failure"));
	closedir(ret->hdl);
	_MFREE(ret);
	return FLISTH_INVALID;
}

BOOL
file_listnext(FLISTH hdl, FLINFO *fli)
{
	OEMCHAR buf[MAX_PATH];
	struct dirent *de;
	struct stat sb;

	de = readdir(hdl->hdl);
	if (de == NULL) {
		VERBOSE(("file_listnext: readdir failure"));
		return FAILURE;
	}

	milstr_ncpy(buf, hdl->path, sizeof(buf));
	milstr_ncat(buf, de->d_name, sizeof(buf));
	if (stat(buf, &sb) != 0) {
		VERBOSE(("file_listnext: stat failure. (path = %s)", buf));
		return FAILURE;
	}

	fli->caps = FLICAPS_SIZE | FLICAPS_ATTR | FLICAPS_DATE | FLICAPS_TIME;
	fli->size = sb.st_size;
	fli->attr = 0;
	if (S_ISDIR(sb.st_mode)) {
		fli->attr |= FILEATTR_DIRECTORY;
	}
	if (!(sb.st_mode & S_IWUSR)) {
		fli->attr |= FILEATTR_READONLY;
	}
	cnvdatetime(&sb, &fli->date, &fli->time);
	milstr_ncpy(fli->path, de->d_name, sizeof(fli->path));
	VERBOSE(("file_listnext: success"));
	return SUCCESS;
}

void
file_listclose(FLISTH hdl)
{

	if (hdl) {
		closedir(hdl->hdl);
		_MFREE(hdl);
	}
}
#endif

static int
euckanji1st(const OEMCHAR *str, int pos)
{
	int ret;
	int c;

	for (ret = 0; pos >= 0; ret ^= 1) {
		c = (UINT8)str[pos--];
		if (!ISKANJI(c))
			break;
	}
	return ret;
}

void
file_cpyname(OEMCHAR *dst, const OEMCHAR *src, int maxlen)
{
	int i;

	if (maxlen-- > 0) {
		for (i = 0; i < maxlen && src[i] != '\0'; i++) {
			dst[i] = src[i];
		}
		if (i > 0) {
			if (euckanji1st(src, i-1)) {
				i--;
			}
		}
		dst[i] = '\0';
	}
}

#if 0
void
file_catname(OEMCHAR *path, const OEMCHAR *filename, int maxlen)
{

	for (; maxlen > 0; path++, maxlen--) {
		if (*path == '\0') {
			break;
		}
	}
	if (maxlen > 0) {
		milstr_ncpy(path, filename, maxlen);
		for (; *path != '\0'; path++) {
			if (!ISKANJI(*path)) {
				path++;
				if (*path == '\0') {
					break;
				}
			} else if (((*path - 0x41) & 0xff) < 26) {
				*path |= 0x20;
			} else if (*path == '\\') {
				*path = _G_DIR_SEPARATOR;
			}
		}
	}
}
#endif

BOOL
file_cmpname(const OEMCHAR *path, const OEMCHAR *path2)
{

	return strcasecmp(path, path2);
}

OEMCHAR *
file_getname(const OEMCHAR *path)
{
	const OEMCHAR *ret;

	for (ret = path; *path != '\0'; path++) {
		if (ISKANJI(*path)) {
			path++;
			if (*path == '\0') {
				break;
			}
		} else if (*path == _G_DIR_SEPARATOR) {
			ret = path + 1;
		}
	}
	return (OEMCHAR *)ret;
}

void
file_cutname(OEMCHAR *path)
{
	OEMCHAR *p;

	p = file_getname(path);
	*p = '\0';
}

OEMCHAR *
file_getext(const OEMCHAR *path)
{
	const OEMCHAR *p, *q;

	for (p = file_getname(path), q = NULL; *p != '\0'; p++) {
		if (*p == '.') {
			q = p + 1;
		}
	}
	if (q == NULL) {
		q = p;
	}
	return (OEMCHAR *)q;
}

void
file_cutext(OEMCHAR *path)
{
	OEMCHAR *p, *q;

	for (p = file_getname(path), q = NULL; *p != '\0'; p++) {
		if (*p == '.') {
			q = p;
		}
	}
	if (q != NULL) {
		*q = '\0';
	}
}

void
file_cutseparator(OEMCHAR *path)
{
	int pos;

	pos = strlen(path) - 1;
	if ((pos > 0) && (path[pos] == _G_DIR_SEPARATOR)) {
		path[pos] = '\0';
	}
}

void
file_setseparator(OEMCHAR *path, int maxlen)
{
	int pos;

	pos = strlen(path);
	if ((pos) && (path[pos-1] != _G_DIR_SEPARATOR) && ((pos + 2) < maxlen)) {
		path[pos++] = _G_DIR_SEPARATOR;
		path[pos] = '\0';
	}
}
