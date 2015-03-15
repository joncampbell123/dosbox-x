
#ifndef __ISP_UTILS_V4_WAVEMMREG_H
#define __ISP_UTILS_V4_WAVEMMREG_H

#include <stdint.h>

/* [doc] windows_WAVE_FORMAT_... constants
 * 
 * windows_WAVE_FORMAT_PCM              8 & 16-bit PCM
 * windows_WAVE_FORMAT_MULAW            mu-law PCM audio
 */
#define windows_WAVE_FORMAT_UNKNOWN			0x0000
#define windows_WAVE_FORMAT_PCM				0x0001	/* verified */
#define windows_WAVE_FORMAT_MS_ADPCM			0x0002	/* verified */
#define windows_WAVE_FORMAT_ADPCM			0x0002	/* verified */
#define windows_WAVE_FORMAT_IEEE_FLOAT			0x0003	/* verified */
#define windows_WAVE_FORMAT_VSELP			0x0004	/* TODO: Pull out your old Windows CE HPC and record a sample */
#define windows_WAVE_FORMAT_COMPAQ_VSELP		0x0004	/* ^^^ */
#define windows_WAVE_FORMAT_IBM_CVSD			0x0005
#define windows_WAVE_FORMAT_ALAW			0x0006	/* verified */
#define windows_WAVE_FORMAT_MULAW			0x0007	/* verified */
#define windows_WAVE_FORMAT_DTS				0x0008
#define windows_WAVE_FORMAT_DRM				0x0009

#define windows_WAVE_FORMAT_OKI_ADPCM			0x0010
#define windows_WAVE_FORMAT_IMA_ADPCM			0x0011
#define windows_WAVE_FORMAT_DVI_ADPCM			0x0011
#define windows_WAVE_FORMAT_MEDIASPACE_ADPCM		0x0012
#define windows_WAVE_FORMAT_SIERRA_ADPCM		0x0013
#define windows_WAVE_FORMAT_G723_ADPCM			0x0014
#define windows_WAVE_FORMAT_DIGISTD			0x0015
#define windows_WAVE_FORMAT_DIGIFIX			0x0016
#define windows_WAVE_FORMAT_DIALOGIC_OKI_ADPCM		0x0017
#define windows_WAVE_FORMAT_MEDIAVISION_ADPCM		0x0018
#define windows_WAVE_FORMAT_CU_CODEC			0x0019

#define windows_WAVE_FORMAT_YAMAHA_ADPCM		0x0020
#define windows_WAVE_FORMAT_SONARC			0x0021
#define windows_WAVE_FORMAT_DSPGROUP_TRUESPEECH		0x0022
#define windows_WAVE_FORMAT_ECHOSC1			0x0023
#define windows_WAVE_FORMAT_AUDIOFILE_AF36		0x0024
#define windows_WAVE_FORMAT_APTX			0x0025
#define windows_WAVE_FORMAT_AUDIOFILE_AF10		0x0026
#define windows_WAVE_FORMAT_PROSODY_1612		0x0027
#define windows_WAVE_FORMAT_LRC				0x0028

#define windows_WAVE_FORMAT_DOLBY_AC2			0x0030
#define windows_WAVE_FORMAT_GSM610			0x0031
#define windows_WAVE_FORMAT_MSNAUDIO			0x0032
#define windows_WAVE_FORMAT_ANTEX_ADPCME		0x0033
#define windows_WAVE_FORMAT_CONTROL_RES_VQLPC		0x0034
#define windows_WAVE_FORMAT_DIGIREAL			0x0035
#define windows_WAVE_FORMAT_DIGIADPCM			0x0036
#define windows_WAVE_FORMAT_CONTROL_RES_CR10		0x0037
#define windows_WAVE_FORMAT_NMS_VBXADPCM		0x0038
#define windows_WAVE_FORMAT_CS_IMAADPCM			0x0039
#define windows_WAVE_FORMAT_ROLAND_RDAC			0x0039
#define windows_WAVE_FORMAT_ECHOSC3			0x003A
#define windows_WAVE_FORMAT_ROCKWELL_ADPCM		0x003B
#define windows_WAVE_FORMAT_ROCKWELL_DIGITALK		0x003C
#define windows_WAVE_FORMAT_XEBEC			0x003D

#define windows_WAVE_FORMAT_G721_ADPCM			0x0040
#define windows_WAVE_FORMAT_G728_CELP			0x0041
#define windows_WAVE_FORMAT_MSG723			0x0042

#define windows_WAVE_FORMAT_MPEG			0x0050

#define windows_WAVE_FORMAT_RT24			0x0052
#define windows_WAVE_FORMAT_PAC				0x0053

#define windows_WAVE_FORMAT_MPEGLAYER3			0x0055

#define windows_WAVE_FORMAT_LUCENT_G723			0x0059

#define windows_WAVE_FORMAT_CIRRUS			0x0060
#define windows_WAVE_FORMAT_ESPCM			0x0061
#define windows_WAVE_FORMAT_VOXWARE			0x0062
#define windows_WAVE_FORMAT_CANOPUS_ATRAC		0x0063
#define windows_WAVE_FORMAT_G726_ADPCM			0x0064
#define windows_WAVE_FORMAT_G722_ADPCM			0x0065
#define windows_WAVE_FORMAT_DSAT			0x0066
#define windows_WAVE_FORMAT_DSAT_DISPLAY		0x0067

#define windows_WAVE_FORMAT_VOXWARE_BYTE_ALIGNED	0x0069 /* Voxware Byte Aligned (obsolete) */

#define windows_WAVE_FORMAT_VOXWARE_AC8			0x0070 /* Voxware AC8 (obsolete) */
#define windows_WAVE_FORMAT_VOXWARE_AC10		0x0071 /* Voxware AC10 (obsolete) */
#define windows_WAVE_FORMAT_VOXWARE_AC16		0x0072 /* Voxware AC16 (obsolete) */
#define windows_WAVE_FORMAT_VOXWARE_AC20		0x0073 /* Voxware AC20 (obsolete) */
#define windows_WAVE_FORMAT_VOXWARE_RT24		0x0074 /* Voxware MetaVoice (obsolete) */
#define windows_WAVE_FORMAT_VOXWARE_RT29		0x0075 /* Voxware MetaSound (obsolete) */
#define windows_WAVE_FORMAT_VOXWARE_RT29HW		0x0076 /* Voxware RT29HW (obsolete) */
#define windows_WAVE_FORMAT_VOXWARE_VR12		0x0077 /* Voxware VR12 (obsolete) */
#define windows_WAVE_FORMAT_VOXWARE_VR18		0x0078 /* Voxware VR18 (obsolete) */
#define windows_WAVE_FORMAT_VOXWARE_TQ40		0x0079 /* Voxware TQ40 (obsolete) */

#define windows_WAVE_FORMAT_SOFTSOUND			0x0080
#define windows_WAVE_FORMAT_VOXWARE_TQ60		0x0081 /* Voxware TQ60 (obsolete) */
#define windows_WAVE_FORMAT_MSRT24			0x0082 /* MSRT24 */
#define windows_WAVE_FORMAT_G729A			0x0083 /* G.729A */
#define windows_WAVE_FORMAT_MVI_MV12			0x0084 /* MVI MV12 */
#define windows_WAVE_FORMAT_DF_G726			0x0085 /* DF G.726 */
#define windows_WAVE_FORMAT_DF_GSM610			0x0086 /* DF GSM610 */

#define windows_WAVE_FORMAT_ISIAUDIO			0x0088 /* ISIAudio */
#define windows_WAVE_FORMAT_ONLIVE			0x0089 /* Onlive */

#define windows_WAVE_FORMAT_SBC24			0x0091 /* SBC24 */
#define windows_WAVE_FORMAT_DOLBY_AC3_SPDIF		0x0092 /* Dolby AC3 SPDIF */
#define windows_WAVE_FORMAT_MEDIASONIC_G723		0x0093
#define windows_WAVE_FORMAT_PROSODY_8KBPS		0x0094

#define windows_WAVE_FORMAT_ZYXEL_ADPCM			0x0097 /* ZyXEL ADPCM */
#define windows_WAVE_FORMAT_PHILIPS_LPCBB		0x0098 /* Philips LPCBB */
#define windows_WAVE_FORMAT_PACKED			0x0099 /* Packed */

#define windows_WAVE_FORMAT_MALDEN_PHONYTALK		0x00A0
#define windows_WAVE_FORMAT_GSM				0x00A1
#define windows_WAVE_FORMAT_G729			0x00A2
#define windows_WAVE_FORMAT_G723			0x00A3
#define windows_WAVE_FORMAT_ACELP			0x00A4

#define windows_WAVE_FORMAT_AAC_ADTS			0x00FF

#define windows_WAVE_FORMAT_RHETOREX_ADPCM		0x0100
#define windows_WAVE_FORMAT_IRAT			0x0101 /* BeCubed Software's IRAT */

#define windows_WAVE_FORMAT_VIVO_G723			0x0111 /* Vivo G.723 */
#define windows_WAVE_FORMAT_VIVO_SIREN			0x0112 /* Vivo Siren */

#define windows_WAVE_FORMAT_DIGITAL_G723		0x0123 /* Digital G.723 */

#define windows_WAVE_FORMAT_SANYO_LD_ADPCM		0x0125

#define windows_WAVE_FORMAT_SIPROLAB_ACEPLNET		0x0130
#define windows_WAVE_FORMAT_SIPROLAB_ACELP4800		0x0131
#define windows_WAVE_FORMAT_SIPROLAB_ACELP8V3		0x0132
#define windows_WAVE_FORMAT_SIPROLAB_G729		0x0133
#define windows_WAVE_FORMAT_SIPROLAB_G729A		0x0134
#define windows_WAVE_FORMAT_SIPROLAB_KELVIN		0x0135

#define windows_WAVE_FORMAT_G726ADPCM			0x0140

#define windows_WAVE_FORMAT_QUALCOMM_PUREVOICE		0x0150
#define windows_WAVE_FORMAT_QUALCOMM_HALFRATE		0x0151

#define windows_WAVE_FORMAT_TUBGSM			0x0155

#define windows_WAVE_FORMAT_MSAUDIO1			0x0160
#define windows_WAVE_FORMAT_WMAUDIO2			0x0161
#define windows_WAVE_FORMAT_WMAUDIO3			0x0162

#define windows_WAVE_FORMAT_UNISYS_NAP_ADPCM		0x0170
#define windows_WAVE_FORMAT_UNISYS_NAP_ULAW		0x0171
#define windows_WAVE_FORMAT_UNISYS_NAP_ALAW		0x0172
#define windows_WAVE_FORMAT_UNISYS_NAP_16K		0x0173

#define windows_WAVE_FORMAT_CREATIVE_ADPCM		0x0200

#define windows_WAVE_FORMAT_CREATIVE_FASTSPEECH8	0x0202
#define windows_WAVE_FORMAT_CREATIVE_FASTSPEECH10	0x0203

#define windows_WAVE_FORMAT_UHER_ADPCM			0x0210

#define windows_WAVE_FORMAT_QUARTERDECK			0x0220

#define windows_WAVE_FORMAT_ILINK_VC			0x0230

#define windows_WAVE_FORMAT_RAW_SPORT			0x0240
#define windows_WAVE_FORMAT_ESST_AC3			0x0241

#define windows_WAVE_FORMAT_IPI_HSX			0x0250
#define windows_WAVE_FORMAT_IPI_RPELP			0x0251

#define windows_WAVE_FORMAT_CS2				0x0260

#define windows_WAVE_FORMAT_SONY_SCX			0x0270

#define windows_WAVE_FORMAT_FM_TOWNS_SND		0x0300

#define windows_WAVE_FORMAT_BTV_DIGITAL			0x0400

#define windows_WAVE_FORMAT_QDESIGN_MUSIC		0x0450

#define windows_WAVE_FORMAT_VME_VMPCM			0x0680 /* VME VMPCM */
#define windows_WAVE_FORMAT_TPC				0x0681

#define windows_WAVE_FORMAT_OLIGSM			0x1000
#define windows_WAVE_FORMAT_OLIADPCM			0x1001
#define windows_WAVE_FORMAT_OLICELP			0x1002
#define windows_WAVE_FORMAT_OLISBC			0x1003
#define windows_WAVE_FORMAT_OLIOPR			0x1004

#define windows_WAVE_FORMAT_LH_CODEC			0x1100 /* (FIXME: Is this right?) */

#define windows_WAVE_FORMAT_NORRIS			0x1400
#define windows_WAVE_FORMAT_ISIAUDIO_1401		0x1401 /* ISIAudio (FIXME is this right?) */

#define windows_WAVE_FORMAT_SOUNDSPACE_MUSICOMPRESS	0x1500 /* Soundspace Music Compression */

#define windows_WAVE_FORMAT_AC3				0x2000
#define windows_WAVE_FORMAT_DVM				0x2000 /* (FIXME is this right? */

#define windows_WAVE_FORMAT_VORBIS1			0x674f
#define windows_WAVE_FORMAT_VORBIS2			0x6750
#define windows_WAVE_FORMAT_VORBIS3			0x6751

#define windows_WAVE_FORMAT_VORBIS1P			0x676f
#define windows_WAVE_FORMAT_VORBIS2P			0x6770
#define windows_WAVE_FORMAT_VORBIS3P			0x6771

#define windows_WAVE_FORMAT_EXTENSIBLE			0xFFFE
#define windows_WAVE_FORMAT_DEVELOPMENT			0xFFFF

#endif

