/* master.lib 0.23 */
#if defined(MASTER_SMALL) || defined(MASTER_MEDIUM) || defined(MASTER_FAR)
# error ˆÙ‚È‚é•¡”‚Ìƒ‚ƒfƒ‹‚Åmaster.h‚ğ•¹—p‚·‚é‚±‚Æ‚Í‚Å‚«‚Ü‚¹‚ñ!
#else
# ifndef MASTER_COMPACT
#  define MASTER_COMPACT
# endif
# ifndef __MASTER_H
#   include "master.h"
# endif
#endif
