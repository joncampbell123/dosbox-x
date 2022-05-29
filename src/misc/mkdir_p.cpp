
/* mkdir_p equivalent to GNU/Linux shell command "mkdir -p" */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

using namespace std;

#if defined (WIN32)

/* Windows uses \\ (though since Windows 7 or so, / is also accepted apparently) */
# define PSEP '\\'

/* TODO: Maybe this could be combined with C++ templating and an #ifdef WIN32? */
int _wmkdir_p(const wchar_t *pathname) {
	const size_t pathlen = wcslen(pathname);
	wchar_t *pc = new wchar_t[pathlen+1];
	const wchar_t *ps = pathname;
	wchar_t *pwf = pc + pathlen;
	wchar_t *pw = pc;
	struct stat st;
	int result = 0;
	errno = 0;

	/* copy initial leading / if caller wants absolute paths */
	while (*ps == PSEP) *pw++ = *ps++;
	assert(pw <= pwf);

	if (*ps != 0) {
		/* for each path element, copy element and then mkdir */
		do {
			if (*ps == PSEP) {
				*pw++ = *ps++; /* extra path separator? ok... */
				assert(pw <= pwf);
			}
			else {
				/* one element */
				while (!(*ps == 0 || *ps == PSEP)) *pw++ = *ps++;
				assert(pw <= pwf);

				/* and mkdir() with it if needed */
				*pw = 0; /* NUL terminate pc we copied so far */
				if (_wstat(pc,&st) == 0) {
					if (S_ISDIR(st.st_mode)) {
						/* expected, do nothing */
					}
					else {
						/* not a directory? stop and do not continue */
						result = -1;
						errno = ENOTDIR;
						break; /* out of while (*ps != 0) */
					}
				}
				else {
					if (errno == ENOENT) {
						/* expected, create directory */
						if (_wmkdir(pc,mode)/*failed*/) {
							/* create failed, stop and do not continue. */
							result = -1;
							/* errno already set by mkdir() */
							break;
						}
					}
					else {
						/* not expected, stop and do not continue */
						result = -1;
						/* errno already set by stat() */
						break;
					}
				}

				/* skip path separator and continue */
				if (*ps == PSEP) *pw++ = *ps++;
				assert(pw <= pwf);
			}
		} while (*ps != 0);
	}
	else {
		errno = ENOTSUP; /* trying to mkdir "" or "/", why?? */
	}

	delete[] pc;
	return result;

	errno = ENOSYS;
	return -1;
}
#else

// NTS: If the compile target uses backslash for path separator (DOS/WINDOWS) then this can change for those targets
# define PSEP '/'

int mkdir_p(const char *pathname, mode_t mode) {
	const size_t pathlen = strlen(pathname);
	char *pc = new char[pathlen+1];
	const char *ps = pathname;
	char *pwf = pc + pathlen;
	struct stat st;
	int result = 0;
	char *pw = pc;
	errno = 0;

	/* copy initial leading / if caller wants absolute paths */
	while (*ps == PSEP) *pw++ = *ps++;
	assert(pw <= pwf);

	if (*ps != 0) {
		/* for each path element, copy element and then mkdir */
		do {
			if (*ps == PSEP) {
				*pw++ = *ps++; /* extra path separator? ok... */
				assert(pw <= pwf);
			}
			else {
				/* one element */
				while (!(*ps == 0 || *ps == PSEP)) *pw++ = *ps++;
				assert(pw <= pwf);

				/* and mkdir() with it if needed */
				*pw = 0; /* NUL terminate pc we copied so far */
				if (stat(pc,&st) == 0) {
					if (S_ISDIR(st.st_mode)) {
						/* expected, do nothing */
					}
					else {
						/* not a directory? stop and do not continue */
						result = -1;
						errno = ENOTDIR;
						break; /* out of while (*ps != 0) */
					}
				}
				else {
					if (errno == ENOENT) {
						/* expected, create directory */
						if (mkdir(pc,mode)/*failed*/) {
							/* create failed, stop and do not continue. */
							result = -1;
							/* errno already set by mkdir() */
							break;
						}
					}
					else {
						/* not expected, stop and do not continue */
						result = -1;
						/* errno already set by stat() */
						break;
					}
				}

				/* skip path separator and continue */
				if (*ps == PSEP) *pw++ = *ps++;
				assert(pw <= pwf);
			}
		} while (*ps != 0);
	}
	else {
		errno = ENOTSUP; /* trying to mkdir "" or "/", why?? */
	}

	delete[] pc;
	return result;
}
#endif

