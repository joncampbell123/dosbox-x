/* master.lib 0.23 */
#if defined(MASTER_COMPACT) || defined(MASTER_FAR) || defined(MASTER_NEAR)
# error �قȂ镡���̃��f����master.h�𕹗p���邱�Ƃ͂ł��܂���!
#else
# ifndef MASTER_MEDIUM
#  define MASTER_MEDIUM
# endif
# ifndef __MASTER_H
#   include "master.h"
# endif
#endif
