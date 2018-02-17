#include	"compiler.h"
#include	"getsnd.h"


#define	DNBASEBITS	12
#define	DNMIXBASE	(1 << DNBASEBITS)

#define	UPBASEBITS	12
#define	UPMIXBASE	(1 << UPBASEBITS)


// 偽物てんぷれーと
// マイクロソフトはマクロ展開下手だから動作チェキするように。

// ---- モノラル出力

#define	MIX_INPUTBIT	8
#define	MIX_OUTPUTBIT	16
#define	MIX_CHANNELS	1
#define	FUNC_NOR		m8m16nr
#define	FUNC_DOWN		m8m16dn
#define	FUNC_UP			m8m16up
#include "getsndmn.mcr"

#define	MIX_INPUTBIT	8
#define	MIX_OUTPUTBIT	16
#define	MIX_CHANNELS	2
#define	FUNC_NOR		s8m16nr
#define	FUNC_DOWN		s8m16dn
#define	FUNC_UP			s8m16up
#include "getsndmn.mcr"

#define	MIX_INPUTBIT	16
#define	MIX_OUTPUTBIT	16
#define	MIX_CHANNELS	1
#define	FUNC_NOR		m16m16nr
#define	FUNC_DOWN		m16m16dn
#define	FUNC_UP			m16m16up
#include "getsndmn.mcr"

#define	MIX_INPUTBIT	16
#define	MIX_OUTPUTBIT	16
#define	MIX_CHANNELS	2
#define	FUNC_NOR		s16m16nr
#define	FUNC_DOWN		s16m16dn
#define	FUNC_UP			s16m16up
#include "getsndmn.mcr"


// ---- ステレオ出力

#define	MIX_INPUTBIT	8
#define	MIX_OUTPUTBIT	16
#define	MIX_CHANNELS	1
#define	FUNC_NOR		m8s16nr
#define	FUNC_DOWN		m8s16dn
#define	FUNC_UP			m8s16up
#include "getsndst.mcr"

#define	MIX_INPUTBIT	8
#define	MIX_OUTPUTBIT	16
#define	MIX_CHANNELS	2
#define	FUNC_NOR		s8s16nr
#define	FUNC_DOWN		s8s16dn
#define	FUNC_UP			s8s16up
#include "getsndst.mcr"

#define	MIX_INPUTBIT	16
#define	MIX_OUTPUTBIT	16
#define	MIX_CHANNELS	1
#define	FUNC_NOR		m16s16nr
#define	FUNC_DOWN		m16s16dn
#define	FUNC_UP			m16s16up
#include "getsndst.mcr"

#define	MIX_INPUTBIT	16
#define	MIX_OUTPUTBIT	16
#define	MIX_CHANNELS	2
#define	FUNC_NOR		s16s16nr
#define	FUNC_DOWN		s16s16dn
#define	FUNC_UP			s16s16up
#include "getsndst.mcr"


static const GSCNV cnvfunc[] = {
			(GSCNV)m8m16nr,		(GSCNV)m8m16dn,		(GSCNV)m8m16up,
			(GSCNV)s8m16nr,		(GSCNV)s8m16dn,		(GSCNV)s8m16up,
			(GSCNV)m16m16nr,	(GSCNV)m16m16dn,	(GSCNV)m16m16up,
			(GSCNV)s16m16nr,	(GSCNV)s16m16dn,	(GSCNV)s16m16up,

			(GSCNV)m8s16nr,		(GSCNV)m8s16dn,		(GSCNV)m8s16up,
			(GSCNV)s8s16nr,		(GSCNV)s8s16dn,		(GSCNV)s8s16up,
			(GSCNV)m16s16nr,	(GSCNV)m16s16dn,	(GSCNV)m16s16up,
			(GSCNV)s16s16nr,	(GSCNV)s16s16dn,	(GSCNV)s16s16up};


BOOL getsnd_setmixproc(GETSND snd, UINT samprate, UINT channles) {

	int		funcnum;

	if ((snd->samplingrate < 8000) || (snd->samplingrate > 96000)) {
		goto gssmp_err;
	}
	if ((samprate < 8000) || (samprate > 96000)) {
		goto gssmp_err;
	}

	funcnum = 0;
	if (snd->channels == 1) {
	}
	else if (snd->channels == 2) {
		funcnum |= 1;
	}
	else {
		goto gssmp_err;
	}

	if (snd->bit == 8) {
	}
	else if (snd->bit == 16) {
		funcnum |= 2;
	}
	else {
		goto gssmp_err;
	}

	if (channles == 1) {
	}
	else if (channles == 2) {
		funcnum |= 4;
	}
	else {
		goto gssmp_err;
	}

	funcnum *= 3;

	if (snd->samplingrate > samprate) {
		snd->mrate = (DNMIXBASE * samprate) / snd->samplingrate;
		snd->rem = DNMIXBASE;
		funcnum += 1;
	}
	else if (snd->samplingrate < samprate) {
		snd->mrate = (UPMIXBASE * samprate) / snd->samplingrate;
		funcnum += 2;
	}
	snd->cnv = cnvfunc[funcnum];
	return(SUCCESS);

gssmp_err:
	return(FAILURE);
}

