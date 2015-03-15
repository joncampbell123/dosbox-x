
#ifndef ISP_UTILS_MISC_UINT64_CONST_H
#define ISP_UTILS_MISC_UINT64_CONST_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef INT64_C
#define INT64_C(v)   (v ## LL)
#endif

#ifndef UINT64_C
#define UINT64_C(v)  (v ## ULL)
#endif

#ifdef __cplusplus
}
#endif

#endif
