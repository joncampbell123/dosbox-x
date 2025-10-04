/* master.lib 0.23 */
#if defined(MASTER_COMPACT) || defined(MASTER_MEDIUM) || defined(MASTER_FAR)
# error ˆÙ‚È‚é•¡”‚Ìƒ‚ƒfƒ‹‚Åmaster.h‚ğ•¹—p‚·‚é‚±‚Æ‚Í‚Å‚«‚Ü‚¹‚ñ!
#else
# ifndef MASTER_NEAR
#  define MASTER_NEAR
# endif
# ifndef __MASTER_H
#   include "master.h"
# endif
#endif
