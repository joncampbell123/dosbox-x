#include	"compiler.h"
#include	"sound.h"
#include	"sndcsec.h"


#if defined(SOUND_CRITICAL)

#if defined(WIN32) || defined(_WIN32_WCE)

	CRITICAL_SECTION	sndcsec;

#elif defined(MACOS)

	MPCriticalRegionID	sndcsec;

#elif defined(X11) || defined(SLZAURUS)

	pthread_mutex_t		sndcsec;		// = PTHREAD_MUTEX_INITIALIZER;

#endif

#endif

