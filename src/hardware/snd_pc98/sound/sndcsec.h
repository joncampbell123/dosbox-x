
#if !defined(SOUND_CRITICAL)

#define	SNDCSEC_INIT
#define	SNDCSEC_TERM
#define	SNDCSEC_ENTER
#define	SNDCSEC_LEAVE

#else

#ifdef __cplusplus
extern "C" {
#endif

#if defined(WIN32) || defined(_WIN32_WCE)

extern	CRITICAL_SECTION	sndcsec;

#define	SNDCSEC_INIT	InitializeCriticalSection(&sndcsec)
#define	SNDCSEC_TERM	DeleteCriticalSection(&sndcsec)
#define	SNDCSEC_ENTER	EnterCriticalSection(&sndcsec)
#define	SNDCSEC_LEAVE	LeaveCriticalSection(&sndcsec)

#elif defined(MACOS)

extern	MPCriticalRegionID	sndcsec;

#define	SNDCSEC_INIT	MPCreateCriticalRegion(&sndcsec)
#define	SNDCSEC_TERM	MPDeleteCriticalRegion(sndcsec)
#define	SNDCSEC_ENTER	MPEnterCriticalRegion(sndcsec, kDurationForever)
#define	SNDCSEC_LEAVE	MPExitCriticalRegion(sndcsec)

#elif defined(X11) || defined(SLZAURUS)

extern	pthread_mutex_t		sndcsec;

#define	SNDCSEC_INIT	pthread_mutex_init(&sndcsec, NULL)
#define	SNDCSEC_TERM	pthread_mutex_destroy(&sndcsec)
#define	SNDCSEC_ENTER	pthread_mutex_lock(&sndcsec)
#define	SNDCSEC_LEAVE	pthread_mutex_unlock(&sndcsec)

#endif

#ifdef __cplusplus
}
#endif

#endif

