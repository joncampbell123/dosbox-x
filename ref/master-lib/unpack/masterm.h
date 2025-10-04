/* master.lib 0.23 */
#if defined(MASTER_COMPACT) || defined(MASTER_FAR) || defined(MASTER_NEAR)
# error ˆÙ‚È‚é•¡”‚Ìƒ‚ƒfƒ‹‚Åmaster.h‚ğ•¹—p‚·‚é‚±‚Æ‚Í‚Å‚«‚Ü‚¹‚ñ!
#else
# ifndef MASTER_MEDIUM
#  define MASTER_MEDIUM
# endif
# ifndef __MASTER_H
#   include "master.h"
# endif
#endif
