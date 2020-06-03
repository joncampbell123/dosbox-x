
/* Emscripten does not have endian.h */
# if defined(EMSCRIPTEN)

#  include <endian.h>

/* MinGW implements some MSVC idioms, so always test for MinGW first. */
# elif defined(__MINGW32__) || defined(__riscos__)

# if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__

#  define htobe16(x) htons(x)
#  define htole16(x) (x)
#  define be16toh(x) ntohs(x)
#  define le16toh(x) (x)

#  define htobe32(x) htonl(x)
#  define htole32(x) (x)
#  define be32toh(x) ntohl(x)
#  define le32toh(x) (x)

#  define htobe64(x) htonll(x)
#  define htole64(x) (x)
#  define be64toh(x) ntohll(x)
#  define le64toh(x) (x)

# elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__

#  define htobe16(x) (x)
#  define htole16(x) __builtin_bswap16(x)
#  define be16toh(x) (x)
#  define le16toh(x) __builtin_bswap16(x)

#  define htobe32(x) (x)
#  define htole32(x) __builtin_bswap32(x)
#  define be32toh(x) (x)
#  define le32toh(x) __builtin_bswap32(x)

#  define htobe64(x) (x)
#  define htole64(x) __builtin_bswap64(x)
#  define be64toh(x) (x)
#  define le64toh(x) __builtin_bswap64(x)

# else
#  error Unexpected __BYTE_ORDER__

# endif /* __MINGW__ __BYTE_ORDER__ */

#elif defined(_MSC_VER)

# if BYTE_ORDER == LITTLE_ENDIAN

# define htobe16(x) htons(x)
# define htole16(x) (x)
# define be16toh(x) ntohs(x)
# define le16toh(x) (x)

# define htobe32(x) htonl(x)
# define htole32(x) (x)
# define be32toh(x) ntohl(x)
# define le32toh(x) (x)

# define htobe64(x) htonll(x)
# define htole64(x) (x)
# define be64toh(x) ntohll(x)
# define le64toh(x) (x)

# elif BYTE_ORDER == BIG_ENDIAN

# define htobe16(x) (x)
# define htole16(x) __builtin_bswap16(x)
# define be16toh(x) (x)
# define le16toh(x) __builtin_bswap16(x)

# define htobe32(x) (x)
# define htole32(x) __builtin_bswap32(x)
# define be32toh(x) (x)
# define le32toh(x) __builtin_bswap32(x)

# define htobe64(x) (x)
# define htole64(x) __builtin_bswap64(x)
# define be64toh(x) (x)
# define le64toh(x) __builtin_bswap64(x)

# else
# error Unexpected BYTE_ORDER.

# endif /* _MSC_VER BYTE_ORDER */

#elif defined(__APPLE__)
 /* This is a simple compatibility shim to convert
 * BSD/Linux endian macros to the Mac OS X equivalents. */
#include <libkern/OSByteOrder.h>
#define htobe16(x) OSSwapHostToBigInt16(x)
#define htole16(x) OSSwapHostToLittleInt16(x)
#define be16toh(x) OSSwapBigToHostInt16(x)
#define le16toh(x) OSSwapLittleToHostInt16(x)

#define htobe32(x) OSSwapHostToBigInt32(x)
#define htole32(x) OSSwapHostToLittleInt32(x)
#define be32toh(x) OSSwapBigToHostInt32(x)
#define le32toh(x) OSSwapLittleToHostInt32(x)

#define htobe64(x) OSSwapHostToBigInt64(x)
#define htole64(x) OSSwapHostToLittleInt64(x)
#define be64toh(x) OSSwapBigToHostInt64(x)
#define le64toh(x) OSSwapLittleToHostInt64(x)

#elif defined(__linux__) || defined(__CYGWIN__)

#include <endian.h>

#elif defined(__HAIKU__)

#define _BSD_SOURCE
#include <endian.h>

#elif defined(__NetBSD__) || defined(__OpenBSD__)

#include <sys/endian.h>

#elif defined(__FreeBSD__) || defined(__DragonFly__)

#include <sys/endian.h>

#define be16toh(x) betoh16(x)
#define le16toh(x) letoh16(x)

#define be32toh(x) betoh32(x)
#define le32toh(x) letoh32(x)

#define be64toh(x) betoh64(x)
#define le64toh(x) letoh64(x)

#endif

