/*==========================================================================;
 *
 *  Copyright (C) 1995-1999 Microsoft Corporation.  All Rights Reserved.
 *
 *  File:       dsound.h
 *  Content:    DirectSound include file
 *
 **************************************************************************/

#ifndef __DSOUND_INCLUDED__
#define __DSOUND_INCLUDED__

#include "d3dtypes.h"

#define COM_NO_WINDOWS_H
#include <objbase.h>

#define _FACDS  0x878
#define MAKE_DSHRESULT(code)  MAKE_HRESULT(1, _FACDS, code)

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#ifndef DIRECTSOUND_VERSION
#define DIRECTSOUND_VERSION  0x0700       /* version 7.0 */
#endif /* !DIRECTSOUND_VERSION */


// DirectSound Component GUID {47D4D946-62E8-11cf-93BC-444553540000}
DEFINE_GUID(CLSID_DirectSound, 0x47d4d946, 0x62e8, 0x11cf, 0x93, 0xbc, 0x44, 0x45, 0x53, 0x54, 0x0, 0x0);

// DirectSound Capture Component GUID {B0210780-89CD-11d0-AF08-00A0C925CD16}
DEFINE_GUID(CLSID_DirectSoundCapture, 0xb0210780, 0x89cd, 0x11d0, 0xaf, 0x8, 0x0, 0xa0, 0xc9, 0x25, 0xcd, 0x16);

#ifdef __cplusplus
// 'struct' not 'class' per the way DECLARE_INTERFACE_ is defined
struct IDirectSound;
struct IDirectSoundBuffer;
struct IDirectSound3DListener;
struct IDirectSound3DBuffer;
struct IDirectSoundCapture;
struct IDirectSoundCaptureBuffer;
struct IDirectSoundNotify;
#endif // __cplusplus

typedef struct IDirectSound *LPDIRECTSOUND;
typedef struct IDirectSoundBuffer *LPDIRECTSOUNDBUFFER;
typedef struct IDirectSound3DListener *LPDIRECTSOUND3DLISTENER;
typedef struct IDirectSound3DBuffer *LPDIRECTSOUND3DBUFFER;
typedef struct IDirectSoundCapture *LPDIRECTSOUNDCAPTURE;
typedef struct IDirectSoundCaptureBuffer *LPDIRECTSOUNDCAPTUREBUFFER;
typedef struct IDirectSoundNotify *LPDIRECTSOUNDNOTIFY;

//
// Compatibility typedefs
//

#ifndef _LPCWAVEFORMATEX_DEFINED
#define _LPCWAVEFORMATEX_DEFINED
typedef const WAVEFORMATEX *LPCWAVEFORMATEX;
#endif // _LPCWAVEFORMATEX_DEFINED

#ifndef __LPCGUID_DEFINED__
#define __LPCGUID_DEFINED__
typedef const GUID *LPCGUID;
#endif // __LPCGUID_DEFINED__

typedef LPDIRECTSOUND *LPLPDIRECTSOUND;
typedef LPDIRECTSOUNDBUFFER *LPLPDIRECTSOUNDBUFFER;
typedef LPDIRECTSOUND3DLISTENER *LPLPDIRECTSOUND3DLISTENER;
typedef LPDIRECTSOUND3DBUFFER *LPLPDIRECTSOUND3DBUFFER;
typedef LPDIRECTSOUNDCAPTURE *LPLPDIRECTSOUNDCAPTURE;
typedef LPDIRECTSOUNDCAPTUREBUFFER *LPLPDIRECTSOUNDCAPTUREBUFFER;
typedef LPDIRECTSOUNDNOTIFY *LPLPDIRECTSOUNDNOTIFY;

//
// Structures
//

typedef struct _DSCAPS
{
    DWORD           dwSize;
    DWORD           dwFlags;
    DWORD           dwMinSecondarySampleRate;
    DWORD           dwMaxSecondarySampleRate;
    DWORD           dwPrimaryBuffers;
    DWORD           dwMaxHwMixingAllBuffers;
    DWORD           dwMaxHwMixingStaticBuffers;
    DWORD           dwMaxHwMixingStreamingBuffers;
    DWORD           dwFreeHwMixingAllBuffers;
    DWORD           dwFreeHwMixingStaticBuffers;
    DWORD           dwFreeHwMixingStreamingBuffers;
    DWORD           dwMaxHw3DAllBuffers;
    DWORD           dwMaxHw3DStaticBuffers;
    DWORD           dwMaxHw3DStreamingBuffers;
    DWORD           dwFreeHw3DAllBuffers;
    DWORD           dwFreeHw3DStaticBuffers;
    DWORD           dwFreeHw3DStreamingBuffers;
    DWORD           dwTotalHwMemBytes;
    DWORD           dwFreeHwMemBytes;
    DWORD           dwMaxContigFreeHwMemBytes;
    DWORD           dwUnlockTransferRateHwBuffers;
    DWORD           dwPlayCpuOverheadSwBuffers;
    DWORD           dwReserved1;
    DWORD           dwReserved2;
} DSCAPS, *LPDSCAPS;

typedef const DSCAPS *LPCDSCAPS;

typedef struct _DSBCAPS
{
    DWORD           dwSize;
    DWORD           dwFlags;
    DWORD           dwBufferBytes;
    DWORD           dwUnlockTransferRate;
    DWORD           dwPlayCpuOverhead;
} DSBCAPS, *LPDSBCAPS;

typedef const DSBCAPS *LPCDSBCAPS;

typedef struct _DSBUFFERDESC
{
    DWORD           dwSize;
    DWORD           dwFlags;
    DWORD           dwBufferBytes;
    DWORD           dwReserved;
    LPWAVEFORMATEX  lpwfxFormat;
#if DIRECTSOUND_VERSION >= 0x0700
    GUID            guid3DAlgorithm;
#endif
} DSBUFFERDESC, *LPDSBUFFERDESC;

typedef const DSBUFFERDESC *LPCDSBUFFERDESC;

typedef struct _DSBUFFERDESC1
{
    DWORD           dwSize;
    DWORD           dwFlags;
    DWORD           dwBufferBytes;
    DWORD           dwReserved;
    LPWAVEFORMATEX  lpwfxFormat;
} DSBUFFERDESC1, *LPDSBUFFERDESC1;

typedef const DSBUFFERDESC1 *LPCDSBUFFERDESC1;

typedef struct _DS3DBUFFER
{
    DWORD           dwSize;
    D3DVECTOR       vPosition;
    D3DVECTOR       vVelocity;
    DWORD           dwInsideConeAngle;
    DWORD           dwOutsideConeAngle;
    D3DVECTOR       vConeOrientation;
    LONG            lConeOutsideVolume;
    D3DVALUE        flMinDistance;
    D3DVALUE        flMaxDistance;
    DWORD           dwMode;
} DS3DBUFFER, *LPDS3DBUFFER;

typedef const DS3DBUFFER *LPCDS3DBUFFER;

typedef struct _DS3DLISTENER
{
    DWORD           dwSize;
    D3DVECTOR       vPosition;
    D3DVECTOR       vVelocity;
    D3DVECTOR       vOrientFront;
    D3DVECTOR       vOrientTop;
    D3DVALUE        flDistanceFactor;
    D3DVALUE        flRolloffFactor;
    D3DVALUE        flDopplerFactor;
} DS3DLISTENER, *LPDS3DLISTENER;

typedef const DS3DLISTENER *LPCDS3DLISTENER;

typedef struct _DSCCAPS
{
    DWORD           dwSize;
    DWORD           dwFlags;
    DWORD           dwFormats;
    DWORD           dwChannels;
} DSCCAPS, *LPDSCCAPS;

typedef const DSCCAPS *LPCDSCCAPS;

typedef struct _DSCBUFFERDESC
{
    DWORD           dwSize;
    DWORD           dwFlags;
    DWORD           dwBufferBytes;
    DWORD           dwReserved;
    LPWAVEFORMATEX  lpwfxFormat;
} DSCBUFFERDESC, *LPDSCBUFFERDESC;

typedef const DSCBUFFERDESC *LPCDSCBUFFERDESC;

typedef struct _DSCBCAPS
{
    DWORD           dwSize;
    DWORD           dwFlags;
    DWORD           dwBufferBytes;
    DWORD           dwReserved;
} DSCBCAPS, *LPDSCBCAPS;

typedef const DSCBCAPS *LPCDSCBCAPS;

typedef struct _DSBPOSITIONNOTIFY
{
    DWORD           dwOffset;
    HANDLE          hEventNotify;
} DSBPOSITIONNOTIFY, *LPDSBPOSITIONNOTIFY;

typedef const DSBPOSITIONNOTIFY *LPCDSBPOSITIONNOTIFY;

//
// DirectSound API
//

typedef BOOL (CALLBACK *LPDSENUMCALLBACKA)(LPGUID, LPCSTR, LPCSTR, LPVOID);
typedef BOOL (CALLBACK *LPDSENUMCALLBACKW)(LPGUID, LPCWSTR, LPCWSTR, LPVOID);

extern HRESULT WINAPI DirectSoundCreate(LPCGUID, LPDIRECTSOUND *, LPUNKNOWN);
extern HRESULT WINAPI DirectSoundEnumerateA(LPDSENUMCALLBACKA, LPVOID);
extern HRESULT WINAPI DirectSoundEnumerateW(LPDSENUMCALLBACKW, LPVOID);

extern HRESULT WINAPI DirectSoundCaptureCreate(LPCGUID, LPDIRECTSOUNDCAPTURE *, LPUNKNOWN);
extern HRESULT WINAPI DirectSoundCaptureEnumerateA(LPDSENUMCALLBACKA, LPVOID);
extern HRESULT WINAPI DirectSoundCaptureEnumerateW(LPDSENUMCALLBACKW, LPVOID);

#ifdef UNICODE
#define LPDSENUMCALLBACK            LPDSENUMCALLBACKW
#define DirectSoundEnumerate        DirectSoundEnumerateW
#define DirectSoundCaptureEnumerate DirectSoundCaptureEnumerateW
#else // UNICODE
#define LPDSENUMCALLBACK            LPDSENUMCALLBACKA
#define DirectSoundEnumerate        DirectSoundEnumerateA
#define DirectSoundCaptureEnumerate DirectSoundCaptureEnumerateA
#endif // UNICODE

//
// IUnknown
//

#if !defined(__cplusplus) || defined(CINTERFACE)
#ifndef IUnknown_QueryInterface
#define IUnknown_QueryInterface(p,a,b)  (p)->lpVtbl->QueryInterface(p,a,b)
#endif // IUnknown_QueryInterface
#ifndef IUnknown_AddRef
#define IUnknown_AddRef(p)              (p)->lpVtbl->AddRef(p)
#endif // IUnknown_AddRef
#ifndef IUnknown_Release
#define IUnknown_Release(p)             (p)->lpVtbl->Release(p)
#endif // IUnknown_Release
#else // !defined(__cplusplus) || defined(CINTERFACE)
#ifndef IUnknown_QueryInterface
#define IUnknown_QueryInterface(p,a,b)  (p)->QueryInterface(a,b)
#endif // IUnknown_QueryInterface
#ifndef IUnknown_AddRef
#define IUnknown_AddRef(p)              (p)->AddRef()
#endif // IUnknown_AddRef
#ifndef IUnknown_Release
#define IUnknown_Release(p)             (p)->Release()
#endif // IUnknown_Release
#endif // !defined(__cplusplus) || defined(CINTERFACE)

//
// IDirectSound
//

DEFINE_GUID(IID_IDirectSound, 0x279AFA83, 0x4981, 0x11CE, 0xA5, 0x21, 0x00, 0x20, 0xAF, 0x0B, 0xE5, 0x60);

#undef INTERFACE
#define INTERFACE IDirectSound

DECLARE_INTERFACE_(IDirectSound, IUnknown)
{
    // IUnknown methods
    STDMETHOD(QueryInterface)       (THIS_ REFIID, LPVOID *) PURE;
    STDMETHOD_(ULONG,AddRef)        (THIS) PURE;
    STDMETHOD_(ULONG,Release)       (THIS) PURE;

    // IDirectSound methods
    STDMETHOD(CreateSoundBuffer)    (THIS_ LPCDSBUFFERDESC, LPDIRECTSOUNDBUFFER *, LPUNKNOWN) PURE;
    STDMETHOD(GetCaps)              (THIS_ LPDSCAPS) PURE;
    STDMETHOD(DuplicateSoundBuffer) (THIS_ LPDIRECTSOUNDBUFFER, LPDIRECTSOUNDBUFFER *) PURE;
    STDMETHOD(SetCooperativeLevel)  (THIS_ HWND, DWORD) PURE;
    STDMETHOD(Compact)              (THIS) PURE;
    STDMETHOD(GetSpeakerConfig)     (THIS_ LPDWORD) PURE;
    STDMETHOD(SetSpeakerConfig)     (THIS_ DWORD) PURE;
    STDMETHOD(Initialize)           (THIS_ LPCGUID) PURE;
};

#define IDirectSound_QueryInterface(p,a,b)       IUnknown_QueryInterface(p,a,b)
#define IDirectSound_AddRef(p)                   IUnknown_AddRef(p)
#define IDirectSound_Release(p)                  IUnknown_Release(p)

#if !defined(__cplusplus) || defined(CINTERFACE)
#define IDirectSound_CreateSoundBuffer(p,a,b,c)  (p)->lpVtbl->CreateSoundBuffer(p,a,b,c)
#define IDirectSound_GetCaps(p,a)                (p)->lpVtbl->GetCaps(p,a)
#define IDirectSound_DuplicateSoundBuffer(p,a,b) (p)->lpVtbl->DuplicateSoundBuffer(p,a,b)
#define IDirectSound_SetCooperativeLevel(p,a,b)  (p)->lpVtbl->SetCooperativeLevel(p,a,b)
#define IDirectSound_Compact(p)                  (p)->lpVtbl->Compact(p)
#define IDirectSound_GetSpeakerConfig(p,a)       (p)->lpVtbl->GetSpeakerConfig(p,a)
#define IDirectSound_SetSpeakerConfig(p,b)       (p)->lpVtbl->SetSpeakerConfig(p,b)
#define IDirectSound_Initialize(p,a)             (p)->lpVtbl->Initialize(p,a)
#else // !defined(__cplusplus) || defined(CINTERFACE)
#define IDirectSound_CreateSoundBuffer(p,a,b,c)  (p)->CreateSoundBuffer(a,b,c)
#define IDirectSound_GetCaps(p,a)                (p)->GetCaps(a)
#define IDirectSound_DuplicateSoundBuffer(p,a,b) (p)->DuplicateSoundBuffer(a,b)
#define IDirectSound_SetCooperativeLevel(p,a,b)  (p)->SetCooperativeLevel(a,b)
#define IDirectSound_Compact(p)                  (p)->Compact()
#define IDirectSound_GetSpeakerConfig(p,a)       (p)->GetSpeakerConfig(a)
#define IDirectSound_SetSpeakerConfig(p,b)       (p)->SetSpeakerConfig(b)
#define IDirectSound_Initialize(p,a)             (p)->Initialize(a)
#endif // !defined(__cplusplus) || defined(CINTERFACE)

//
// IDirectSoundBuffer
//

DEFINE_GUID(IID_IDirectSoundBuffer, 0x279AFA85, 0x4981, 0x11CE, 0xA5, 0x21, 0x00, 0x20, 0xAF, 0x0B, 0xE5, 0x60);

#undef INTERFACE
#define INTERFACE IDirectSoundBuffer

DECLARE_INTERFACE_(IDirectSoundBuffer, IUnknown)
{
    // IUnknown methods
    STDMETHOD(QueryInterface)       (THIS_ REFIID, LPVOID *) PURE;
    STDMETHOD_(ULONG,AddRef)        (THIS) PURE;
    STDMETHOD_(ULONG,Release)       (THIS) PURE;

    // IDirectSoundBuffer methods
    STDMETHOD(GetCaps)              (THIS_ LPDSBCAPS) PURE;
    STDMETHOD(GetCurrentPosition)   (THIS_ LPDWORD, LPDWORD) PURE;
    STDMETHOD(GetFormat)            (THIS_ LPWAVEFORMATEX, DWORD, LPDWORD) PURE;
    STDMETHOD(GetVolume)            (THIS_ LPLONG) PURE;
    STDMETHOD(GetPan)               (THIS_ LPLONG) PURE;
    STDMETHOD(GetFrequency)         (THIS_ LPDWORD) PURE;
    STDMETHOD(GetStatus)            (THIS_ LPDWORD) PURE;
    STDMETHOD(Initialize)           (THIS_ LPDIRECTSOUND, LPCDSBUFFERDESC) PURE;
    STDMETHOD(Lock)                 (THIS_ DWORD, DWORD, LPVOID *, LPDWORD, LPVOID *, LPDWORD, DWORD) PURE;
    STDMETHOD(Play)                 (THIS_ DWORD, DWORD, DWORD) PURE;
    STDMETHOD(SetCurrentPosition)   (THIS_ DWORD) PURE;
    STDMETHOD(SetFormat)            (THIS_ LPCWAVEFORMATEX) PURE;
    STDMETHOD(SetVolume)            (THIS_ LONG) PURE;
    STDMETHOD(SetPan)               (THIS_ LONG) PURE;
    STDMETHOD(SetFrequency)         (THIS_ DWORD) PURE;
    STDMETHOD(Stop)                 (THIS) PURE;
    STDMETHOD(Unlock)               (THIS_ LPVOID, DWORD, LPVOID, DWORD) PURE;
    STDMETHOD(Restore)              (THIS) PURE;
};

#define IDirectSoundBuffer_QueryInterface(p,a,b)        IUnknown_QueryInterface(p,a,b)
#define IDirectSoundBuffer_AddRef(p)                    IUnknown_AddRef(p)
#define IDirectSoundBuffer_Release(p)                   IUnknown_Release(p)

#if !defined(__cplusplus) || defined(CINTERFACE)
#define IDirectSoundBuffer_GetCaps(p,a)                 (p)->lpVtbl->GetCaps(p,a)
#define IDirectSoundBuffer_GetCurrentPosition(p,a,b)    (p)->lpVtbl->GetCurrentPosition(p,a,b)
#define IDirectSoundBuffer_GetFormat(p,a,b,c)           (p)->lpVtbl->GetFormat(p,a,b,c)
#define IDirectSoundBuffer_GetVolume(p,a)               (p)->lpVtbl->GetVolume(p,a)
#define IDirectSoundBuffer_GetPan(p,a)                  (p)->lpVtbl->GetPan(p,a)
#define IDirectSoundBuffer_GetFrequency(p,a)            (p)->lpVtbl->GetFrequency(p,a)
#define IDirectSoundBuffer_GetStatus(p,a)               (p)->lpVtbl->GetStatus(p,a)
#define IDirectSoundBuffer_Initialize(p,a,b)            (p)->lpVtbl->Initialize(p,a,b)
#define IDirectSoundBuffer_Lock(p,a,b,c,d,e,f,g)        (p)->lpVtbl->Lock(p,a,b,c,d,e,f,g)
#define IDirectSoundBuffer_Play(p,a,b,c)                (p)->lpVtbl->Play(p,a,b,c)
#define IDirectSoundBuffer_SetCurrentPosition(p,a)      (p)->lpVtbl->SetCurrentPosition(p,a)
#define IDirectSoundBuffer_SetFormat(p,a)               (p)->lpVtbl->SetFormat(p,a)
#define IDirectSoundBuffer_SetVolume(p,a)               (p)->lpVtbl->SetVolume(p,a)
#define IDirectSoundBuffer_SetPan(p,a)                  (p)->lpVtbl->SetPan(p,a)
#define IDirectSoundBuffer_SetFrequency(p,a)            (p)->lpVtbl->SetFrequency(p,a)
#define IDirectSoundBuffer_Stop(p)                      (p)->lpVtbl->Stop(p)
#define IDirectSoundBuffer_Unlock(p,a,b,c,d)            (p)->lpVtbl->Unlock(p,a,b,c,d)
#define IDirectSoundBuffer_Restore(p)                   (p)->lpVtbl->Restore(p)
#else // !defined(__cplusplus) || defined(CINTERFACE)
#define IDirectSoundBuffer_GetCaps(p,a)                 (p)->GetCaps(a)
#define IDirectSoundBuffer_GetCurrentPosition(p,a,b)    (p)->GetCurrentPosition(a,b)
#define IDirectSoundBuffer_GetFormat(p,a,b,c)           (p)->GetFormat(a,b,c)
#define IDirectSoundBuffer_GetVolume(p,a)               (p)->GetVolume(a)
#define IDirectSoundBuffer_GetPan(p,a)                  (p)->GetPan(a)
#define IDirectSoundBuffer_GetFrequency(p,a)            (p)->GetFrequency(a)
#define IDirectSoundBuffer_GetStatus(p,a)               (p)->GetStatus(a)
#define IDirectSoundBuffer_Initialize(p,a,b)            (p)->Initialize(a,b)
#define IDirectSoundBuffer_Lock(p,a,b,c,d,e,f,g)        (p)->Lock(a,b,c,d,e,f,g)
#define IDirectSoundBuffer_Play(p,a,b,c)                (p)->Play(a,b,c)
#define IDirectSoundBuffer_SetCurrentPosition(p,a)      (p)->SetCurrentPosition(a)
#define IDirectSoundBuffer_SetFormat(p,a)               (p)->SetFormat(a)
#define IDirectSoundBuffer_SetVolume(p,a)               (p)->SetVolume(a)
#define IDirectSoundBuffer_SetPan(p,a)                  (p)->SetPan(a)
#define IDirectSoundBuffer_SetFrequency(p,a)            (p)->SetFrequency(a)
#define IDirectSoundBuffer_Stop(p)                      (p)->Stop()
#define IDirectSoundBuffer_Unlock(p,a,b,c,d)            (p)->Unlock(a,b,c,d)
#define IDirectSoundBuffer_Restore(p)                   (p)->Restore()
#endif // !defined(__cplusplus) || defined(CINTERFACE)

//
// IDirectSound3DListener
//

DEFINE_GUID(IID_IDirectSound3DListener, 0x279AFA84, 0x4981, 0x11CE, 0xA5, 0x21, 0x00, 0x20, 0xAF, 0x0B, 0xE5, 0x60);

#undef INTERFACE
#define INTERFACE IDirectSound3DListener

DECLARE_INTERFACE_(IDirectSound3DListener, IUnknown)
{
    // IUnknown methods
    STDMETHOD(QueryInterface)           (THIS_ REFIID, LPVOID *) PURE;
    STDMETHOD_(ULONG,AddRef)            (THIS) PURE;
    STDMETHOD_(ULONG,Release)           (THIS) PURE;

    // IDirectSound3DListener methods
    STDMETHOD(GetAllParameters)         (THIS_ LPDS3DLISTENER) PURE;
    STDMETHOD(GetDistanceFactor)        (THIS_ LPD3DVALUE) PURE;
    STDMETHOD(GetDopplerFactor)         (THIS_ LPD3DVALUE) PURE;
    STDMETHOD(GetOrientation)           (THIS_ LPD3DVECTOR, LPD3DVECTOR) PURE;
    STDMETHOD(GetPosition)              (THIS_ LPD3DVECTOR) PURE;
    STDMETHOD(GetRolloffFactor)         (THIS_ LPD3DVALUE) PURE;
    STDMETHOD(GetVelocity)              (THIS_ LPD3DVECTOR) PURE;
    STDMETHOD(SetAllParameters)         (THIS_ LPCDS3DLISTENER, DWORD) PURE;
    STDMETHOD(SetDistanceFactor)        (THIS_ D3DVALUE, DWORD) PURE;
    STDMETHOD(SetDopplerFactor)         (THIS_ D3DVALUE, DWORD) PURE;
    STDMETHOD(SetOrientation)           (THIS_ D3DVALUE, D3DVALUE, D3DVALUE, D3DVALUE, D3DVALUE, D3DVALUE, DWORD) PURE;
    STDMETHOD(SetPosition)              (THIS_ D3DVALUE, D3DVALUE, D3DVALUE, DWORD) PURE;
    STDMETHOD(SetRolloffFactor)         (THIS_ D3DVALUE, DWORD) PURE;
    STDMETHOD(SetVelocity)              (THIS_ D3DVALUE, D3DVALUE, D3DVALUE, DWORD) PURE;
    STDMETHOD(CommitDeferredSettings)   (THIS) PURE;
};

#define IDirectSound3DListener_QueryInterface(p,a,b)            IUnknown_QueryInterface(p,a,b)
#define IDirectSound3DListener_AddRef(p)                        IUnknown_AddRef(p)
#define IDirectSound3DListener_Release(p)                       IUnknown_Release(p)

#if !defined(__cplusplus) || defined(CINTERFACE)
#define IDirectSound3DListener_GetAllParameters(p,a)            (p)->lpVtbl->GetAllParameters(p,a)
#define IDirectSound3DListener_GetDistanceFactor(p,a)           (p)->lpVtbl->GetDistanceFactor(p,a)
#define IDirectSound3DListener_GetDopplerFactor(p,a)            (p)->lpVtbl->GetDopplerFactor(p,a)
#define IDirectSound3DListener_GetOrientation(p,a,b)            (p)->lpVtbl->GetOrientation(p,a,b)
#define IDirectSound3DListener_GetPosition(p,a)                 (p)->lpVtbl->GetPosition(p,a)
#define IDirectSound3DListener_GetRolloffFactor(p,a)            (p)->lpVtbl->GetRolloffFactor(p,a)
#define IDirectSound3DListener_GetVelocity(p,a)                 (p)->lpVtbl->GetVelocity(p,a)
#define IDirectSound3DListener_SetAllParameters(p,a,b)          (p)->lpVtbl->SetAllParameters(p,a,b)
#define IDirectSound3DListener_SetDistanceFactor(p,a,b)         (p)->lpVtbl->SetDistanceFactor(p,a,b)
#define IDirectSound3DListener_SetDopplerFactor(p,a,b)          (p)->lpVtbl->SetDopplerFactor(p,a,b)
#define IDirectSound3DListener_SetOrientation(p,a,b,c,d,e,f,g)  (p)->lpVtbl->SetOrientation(p,a,b,c,d,e,f,g)
#define IDirectSound3DListener_SetPosition(p,a,b,c,d)           (p)->lpVtbl->SetPosition(p,a,b,c,d)
#define IDirectSound3DListener_SetRolloffFactor(p,a,b)          (p)->lpVtbl->SetRolloffFactor(p,a,b)
#define IDirectSound3DListener_SetVelocity(p,a,b,c,d)           (p)->lpVtbl->SetVelocity(p,a,b,c,d)
#define IDirectSound3DListener_CommitDeferredSettings(p)        (p)->lpVtbl->CommitDeferredSettings(p)
#else // !defined(__cplusplus) || defined(CINTERFACE)
#define IDirectSound3DListener_GetAllParameters(p,a)            (p)->GetAllParameters(a)
#define IDirectSound3DListener_GetDistanceFactor(p,a)           (p)->GetDistanceFactor(a)
#define IDirectSound3DListener_GetDopplerFactor(p,a)            (p)->GetDopplerFactor(a)
#define IDirectSound3DListener_GetOrientation(p,a,b)            (p)->GetOrientation(a,b)
#define IDirectSound3DListener_GetPosition(p,a)                 (p)->GetPosition(a)
#define IDirectSound3DListener_GetRolloffFactor(p,a)            (p)->GetRolloffFactor(a)
#define IDirectSound3DListener_GetVelocity(p,a)                 (p)->GetVelocity(a)
#define IDirectSound3DListener_SetAllParameters(p,a,b)          (p)->SetAllParameters(a,b)
#define IDirectSound3DListener_SetDistanceFactor(p,a,b)         (p)->SetDistanceFactor(a,b)
#define IDirectSound3DListener_SetDopplerFactor(p,a,b)          (p)->SetDopplerFactor(a,b)
#define IDirectSound3DListener_SetOrientation(p,a,b,c,d,e,f,g)  (p)->SetOrientation(a,b,c,d,e,f,g)
#define IDirectSound3DListener_SetPosition(p,a,b,c,d)           (p)->SetPosition(a,b,c,d)
#define IDirectSound3DListener_SetRolloffFactor(p,a,b)          (p)->SetRolloffFactor(a,b)
#define IDirectSound3DListener_SetVelocity(p,a,b,c,d)           (p)->SetVelocity(a,b,c,d)
#define IDirectSound3DListener_CommitDeferredSettings(p)        (p)->CommitDeferredSettings()
#endif // !defined(__cplusplus) || defined(CINTERFACE)

//
// IDirectSound3DBuffer
//

DEFINE_GUID(IID_IDirectSound3DBuffer, 0x279AFA86, 0x4981, 0x11CE, 0xA5, 0x21, 0x00, 0x20, 0xAF, 0x0B, 0xE5, 0x60);

#undef INTERFACE
#define INTERFACE IDirectSound3DBuffer

DECLARE_INTERFACE_(IDirectSound3DBuffer, IUnknown)
{
    // IUnknown methods
    STDMETHOD(QueryInterface)       (THIS_ REFIID, LPVOID *) PURE;
    STDMETHOD_(ULONG,AddRef)        (THIS) PURE;
    STDMETHOD_(ULONG,Release)       (THIS) PURE;

    // IDirectSound3DBuffer methods
    STDMETHOD(GetAllParameters)     (THIS_ LPDS3DBUFFER) PURE;
    STDMETHOD(GetConeAngles)        (THIS_ LPDWORD, LPDWORD) PURE;
    STDMETHOD(GetConeOrientation)   (THIS_ LPD3DVECTOR) PURE;
    STDMETHOD(GetConeOutsideVolume) (THIS_ LPLONG) PURE;
    STDMETHOD(GetMaxDistance)       (THIS_ LPD3DVALUE) PURE;
    STDMETHOD(GetMinDistance)       (THIS_ LPD3DVALUE) PURE;
    STDMETHOD(GetMode)              (THIS_ LPDWORD) PURE;
    STDMETHOD(GetPosition)          (THIS_ LPD3DVECTOR) PURE;
    STDMETHOD(GetVelocity)          (THIS_ LPD3DVECTOR) PURE;
    STDMETHOD(SetAllParameters)     (THIS_ LPCDS3DBUFFER, DWORD) PURE;
    STDMETHOD(SetConeAngles)        (THIS_ DWORD, DWORD, DWORD) PURE;
    STDMETHOD(SetConeOrientation)   (THIS_ D3DVALUE, D3DVALUE, D3DVALUE, DWORD) PURE;
    STDMETHOD(SetConeOutsideVolume) (THIS_ LONG, DWORD) PURE;
    STDMETHOD(SetMaxDistance)       (THIS_ D3DVALUE, DWORD) PURE;
    STDMETHOD(SetMinDistance)       (THIS_ D3DVALUE, DWORD) PURE;
    STDMETHOD(SetMode)              (THIS_ DWORD, DWORD) PURE;
    STDMETHOD(SetPosition)          (THIS_ D3DVALUE, D3DVALUE, D3DVALUE, DWORD) PURE;
    STDMETHOD(SetVelocity)          (THIS_ D3DVALUE, D3DVALUE, D3DVALUE, DWORD) PURE;
};

#define IDirectSound3DBuffer_QueryInterface(p,a,b)          IUnknown_QueryInterface(p,a,b)
#define IDirectSound3DBuffer_AddRef(p)                      IUnknown_AddRef(p)
#define IDirectSound3DBuffer_Release(p)                     IUnknown_Release(p)

#if !defined(__cplusplus) || defined(CINTERFACE)
#define IDirectSound3DBuffer_GetAllParameters(p,a)          (p)->lpVtbl->GetAllParameters(p,a)
#define IDirectSound3DBuffer_GetConeAngles(p,a,b)           (p)->lpVtbl->GetConeAngles(p,a,b)
#define IDirectSound3DBuffer_GetConeOrientation(p,a)        (p)->lpVtbl->GetConeOrientation(p,a)
#define IDirectSound3DBuffer_GetConeOutsideVolume(p,a)      (p)->lpVtbl->GetConeOutsideVolume(p,a)
#define IDirectSound3DBuffer_GetPosition(p,a)               (p)->lpVtbl->GetPosition(p,a)
#define IDirectSound3DBuffer_GetMinDistance(p,a)            (p)->lpVtbl->GetMinDistance(p,a)
#define IDirectSound3DBuffer_GetMaxDistance(p,a)            (p)->lpVtbl->GetMaxDistance(p,a)
#define IDirectSound3DBuffer_GetMode(p,a)                   (p)->lpVtbl->GetMode(p,a)
#define IDirectSound3DBuffer_GetVelocity(p,a)               (p)->lpVtbl->GetVelocity(p,a)
#define IDirectSound3DBuffer_SetAllParameters(p,a,b)        (p)->lpVtbl->SetAllParameters(p,a,b)
#define IDirectSound3DBuffer_SetConeAngles(p,a,b,c)         (p)->lpVtbl->SetConeAngles(p,a,b,c)
#define IDirectSound3DBuffer_SetConeOrientation(p,a,b,c,d)  (p)->lpVtbl->SetConeOrientation(p,a,b,c,d)
#define IDirectSound3DBuffer_SetConeOutsideVolume(p,a,b)    (p)->lpVtbl->SetConeOutsideVolume(p,a,b)
#define IDirectSound3DBuffer_SetPosition(p,a,b,c,d)         (p)->lpVtbl->SetPosition(p,a,b,c,d)
#define IDirectSound3DBuffer_SetMinDistance(p,a,b)          (p)->lpVtbl->SetMinDistance(p,a,b)
#define IDirectSound3DBuffer_SetMaxDistance(p,a,b)          (p)->lpVtbl->SetMaxDistance(p,a,b)
#define IDirectSound3DBuffer_SetMode(p,a,b)                 (p)->lpVtbl->SetMode(p,a,b)
#define IDirectSound3DBuffer_SetVelocity(p,a,b,c,d)         (p)->lpVtbl->SetVelocity(p,a,b,c,d)
#else // !defined(__cplusplus) || defined(CINTERFACE)
#define IDirectSound3DBuffer_GetAllParameters(p,a)          (p)->GetAllParameters(a)
#define IDirectSound3DBuffer_GetConeAngles(p,a,b)           (p)->GetConeAngles(a,b)
#define IDirectSound3DBuffer_GetConeOrientation(p,a)        (p)->GetConeOrientation(a)
#define IDirectSound3DBuffer_GetConeOutsideVolume(p,a)      (p)->GetConeOutsideVolume(a)
#define IDirectSound3DBuffer_GetPosition(p,a)               (p)->GetPosition(a)
#define IDirectSound3DBuffer_GetMinDistance(p,a)            (p)->GetMinDistance(a)
#define IDirectSound3DBuffer_GetMaxDistance(p,a)            (p)->GetMaxDistance(a)
#define IDirectSound3DBuffer_GetMode(p,a)                   (p)->GetMode(a)
#define IDirectSound3DBuffer_GetVelocity(p,a)               (p)->GetVelocity(a)
#define IDirectSound3DBuffer_SetAllParameters(p,a,b)        (p)->SetAllParameters(a,b)
#define IDirectSound3DBuffer_SetConeAngles(p,a,b,c)         (p)->SetConeAngles(a,b,c)
#define IDirectSound3DBuffer_SetConeOrientation(p,a,b,c,d)  (p)->SetConeOrientation(a,b,c,d)
#define IDirectSound3DBuffer_SetConeOutsideVolume(p,a,b)    (p)->SetConeOutsideVolume(a,b)
#define IDirectSound3DBuffer_SetPosition(p,a,b,c,d)         (p)->SetPosition(a,b,c,d)
#define IDirectSound3DBuffer_SetMinDistance(p,a,b)          (p)->SetMinDistance(a,b)
#define IDirectSound3DBuffer_SetMaxDistance(p,a,b)          (p)->SetMaxDistance(a,b)
#define IDirectSound3DBuffer_SetMode(p,a,b)                 (p)->SetMode(a,b)
#define IDirectSound3DBuffer_SetVelocity(p,a,b,c,d)         (p)->SetVelocity(a,b,c,d)
#endif // !defined(__cplusplus) || defined(CINTERFACE)

//
// IDirectSoundCapture
//

DEFINE_GUID(IID_IDirectSoundCapture, 0xb0210781, 0x89cd, 0x11d0, 0xaf, 0x8, 0x0, 0xa0, 0xc9, 0x25, 0xcd, 0x16);

#undef INTERFACE
#define INTERFACE IDirectSoundCapture

DECLARE_INTERFACE_(IDirectSoundCapture, IUnknown)
{
    // IUnknown methods
    STDMETHOD(QueryInterface)       (THIS_ REFIID, LPVOID *) PURE;
    STDMETHOD_(ULONG,AddRef)        (THIS) PURE;
    STDMETHOD_(ULONG,Release)       (THIS) PURE;

    // IDirectSoundCapture methods
    STDMETHOD(CreateCaptureBuffer)  (THIS_ LPCDSCBUFFERDESC, LPDIRECTSOUNDCAPTUREBUFFER *, LPUNKNOWN) PURE;
    STDMETHOD(GetCaps)              (THIS_ LPDSCCAPS ) PURE;
    STDMETHOD(Initialize)           (THIS_ LPCGUID) PURE;
};

#define IDirectSoundCapture_QueryInterface(p,a,b)           IUnknown_QueryInterface(p,a,b)
#define IDirectSoundCapture_AddRef(p)                       IUnknown_AddRef(p)
#define IDirectSoundCapture_Release(p)                      IUnknown_Release(p)

#if !defined(__cplusplus) || defined(CINTERFACE)
#define IDirectSoundCapture_CreateCaptureBuffer(p,a,b,c)    (p)->lpVtbl->CreateCaptureBuffer(p,a,b,c)
#define IDirectSoundCapture_GetCaps(p,a)                    (p)->lpVtbl->GetCaps(p,a)
#define IDirectSoundCapture_Initialize(p,a)                 (p)->lpVtbl->Initialize(p,a)
#else // !defined(__cplusplus) || defined(CINTERFACE)
#define IDirectSoundCapture_CreateCaptureBuffer(p,a,b,c)    (p)->CreateCaptureBuffer(a,b,c)
#define IDirectSoundCapture_GetCaps(p,a)                    (p)->GetCaps(a)
#define IDirectSoundCapture_Initialize(p,a)                 (p)->Initialize(a)
#endif // !defined(__cplusplus) || defined(CINTERFACE)

//
// IDirectSoundCaptureBuffer
//

DEFINE_GUID(IID_IDirectSoundCaptureBuffer, 0xb0210782, 0x89cd, 0x11d0, 0xaf, 0x8, 0x0, 0xa0, 0xc9, 0x25, 0xcd, 0x16);

#undef INTERFACE
#define INTERFACE IDirectSoundCaptureBuffer

DECLARE_INTERFACE_(IDirectSoundCaptureBuffer, IUnknown)
{
    // IUnknown methods
    STDMETHOD(QueryInterface)       (THIS_ REFIID, LPVOID *) PURE;
    STDMETHOD_(ULONG,AddRef)        (THIS) PURE;
    STDMETHOD_(ULONG,Release)       (THIS) PURE;

    // IDirectSoundCaptureBuffer methods
    STDMETHOD(GetCaps)              (THIS_ LPDSCBCAPS ) PURE;
    STDMETHOD(GetCurrentPosition)   (THIS_ LPDWORD, LPDWORD ) PURE;
    STDMETHOD(GetFormat)            (THIS_ LPWAVEFORMATEX, DWORD, LPDWORD ) PURE;
    STDMETHOD(GetStatus)            (THIS_ LPDWORD ) PURE;
    STDMETHOD(Initialize)           (THIS_ LPDIRECTSOUNDCAPTURE, LPCDSCBUFFERDESC) PURE;
    STDMETHOD(Lock)                 (THIS_ DWORD, DWORD, LPVOID *, LPDWORD, LPVOID *, LPDWORD, DWORD) PURE;
    STDMETHOD(Start)                (THIS_ DWORD) PURE;
    STDMETHOD(Stop)                 (THIS) PURE;
    STDMETHOD(Unlock)               (THIS_ LPVOID, DWORD, LPVOID, DWORD) PURE;
};

#define IDirectSoundCaptureBuffer_QueryInterface(p,a,b)         IUnknown_QueryInterface(p,a,b)
#define IDirectSoundCaptureBuffer_AddRef(p)                     IUnknown_AddRef(p)
#define IDirectSoundCaptureBuffer_Release(p)                    IUnknown_Release(p)

#if !defined(__cplusplus) || defined(CINTERFACE)
#define IDirectSoundCaptureBuffer_GetCaps(p,a)                  (p)->lpVtbl->GetCaps(p,a)
#define IDirectSoundCaptureBuffer_GetCurrentPosition(p,a,b)     (p)->lpVtbl->GetCurrentPosition(p,a,b)
#define IDirectSoundCaptureBuffer_GetFormat(p,a,b,c)            (p)->lpVtbl->GetFormat(p,a,b,c)
#define IDirectSoundCaptureBuffer_GetStatus(p,a)                (p)->lpVtbl->GetStatus(p,a)
#define IDirectSoundCaptureBuffer_Initialize(p,a,b)             (p)->lpVtbl->Initialize(p,a,b)
#define IDirectSoundCaptureBuffer_Lock(p,a,b,c,d,e,f,g)         (p)->lpVtbl->Lock(p,a,b,c,d,e,f,g)
#define IDirectSoundCaptureBuffer_Start(p,a)                    (p)->lpVtbl->Start(p,a)
#define IDirectSoundCaptureBuffer_Stop(p)                       (p)->lpVtbl->Stop(p)
#define IDirectSoundCaptureBuffer_Unlock(p,a,b,c,d)             (p)->lpVtbl->Unlock(p,a,b,c,d)
#else // !defined(__cplusplus) || defined(CINTERFACE)
#define IDirectSoundCaptureBuffer_GetCaps(p,a)                  (p)->GetCaps(a)
#define IDirectSoundCaptureBuffer_GetCurrentPosition(p,a,b)     (p)->GetCurrentPosition(a,b)
#define IDirectSoundCaptureBuffer_GetFormat(p,a,b,c)            (p)->GetFormat(a,b,c)
#define IDirectSoundCaptureBuffer_GetStatus(p,a)                (p)->GetStatus(a)
#define IDirectSoundCaptureBuffer_Initialize(p,a,b)             (p)->Initialize(a,b)
#define IDirectSoundCaptureBuffer_Lock(p,a,b,c,d,e,f,g)         (p)->Lock(a,b,c,d,e,f,g)
#define IDirectSoundCaptureBuffer_Start(p,a)                    (p)->Start(a)
#define IDirectSoundCaptureBuffer_Stop(p)                       (p)->Stop()
#define IDirectSoundCaptureBuffer_Unlock(p,a,b,c,d)             (p)->Unlock(a,b,c,d)
#endif // !defined(__cplusplus) || defined(CINTERFACE)

//
// IDirectSoundNotify
//

DEFINE_GUID(IID_IDirectSoundNotify, 0xb0210783, 0x89cd, 0x11d0, 0xaf, 0x8, 0x0, 0xa0, 0xc9, 0x25, 0xcd, 0x16);

#undef INTERFACE
#define INTERFACE IDirectSoundNotify

DECLARE_INTERFACE_(IDirectSoundNotify, IUnknown)
{
    // IUnknown methods
    STDMETHOD(QueryInterface)           (THIS_ REFIID, LPVOID *) PURE;
    STDMETHOD_(ULONG,AddRef)            (THIS) PURE;
    STDMETHOD_(ULONG,Release)           (THIS) PURE;

    // IDirectSoundNotify methods
    STDMETHOD(SetNotificationPositions) (THIS_ DWORD, LPCDSBPOSITIONNOTIFY) PURE;
};

#define IDirectSoundNotify_QueryInterface(p,a,b)            IUnknown_QueryInterface(p,a,b)
#define IDirectSoundNotify_AddRef(p)                        IUnknown_AddRef(p)
#define IDirectSoundNotify_Release(p)                       IUnknown_Release(p)

#if !defined(__cplusplus) || defined(CINTERFACE)
#define IDirectSoundNotify_SetNotificationPositions(p,a,b)  (p)->lpVtbl->SetNotificationPositions(p,a,b)
#else // !defined(__cplusplus) || defined(CINTERFACE)
#define IDirectSoundNotify_SetNotificationPositions(p,a,b)  (p)->SetNotificationPositions(a,b)
#endif // !defined(__cplusplus) || defined(CINTERFACE)

//
// IKsPropertySet
//

#ifndef _IKsPropertySet_
#define _IKsPropertySet_

#ifdef __cplusplus
// 'struct' not 'class' per the way DECLARE_INTERFACE_ is defined
struct IKsPropertySet;
#endif // __cplusplus

typedef struct IKsPropertySet *LPKSPROPERTYSET;

#define KSPROPERTY_SUPPORT_GET  0x00000001
#define KSPROPERTY_SUPPORT_SET  0x00000002

DEFINE_GUID(IID_IKsPropertySet, 0x31efac30, 0x515c, 0x11d0, 0xa9, 0xaa, 0x00, 0xaa, 0x00, 0x61, 0xbe, 0x93);

#undef INTERFACE
#define INTERFACE IKsPropertySet

DECLARE_INTERFACE_(IKsPropertySet, IUnknown)
{
    // IUnknown methods
    STDMETHOD(QueryInterface)   (THIS_ REFIID, LPVOID *) PURE;
    STDMETHOD_(ULONG,AddRef)    (THIS) PURE;
    STDMETHOD_(ULONG,Release)   (THIS) PURE;

    // IKsPropertySet methods
    STDMETHOD(Get)              (THIS_ REFGUID, ULONG, LPVOID, ULONG, LPVOID, ULONG, PULONG) PURE;
    STDMETHOD(Set)              (THIS_ REFGUID, ULONG, LPVOID, ULONG, LPVOID, ULONG) PURE;
    STDMETHOD(QuerySupport)     (THIS_ REFGUID, ULONG, PULONG) PURE;
};

#define IKsPropertySet_QueryInterface(p,a,b)       IUnknown_QueryInterface(p,a,b)
#define IKsPropertySet_AddRef(p)                   IUnknown_AddRef(p)
#define IKsPropertySet_Release(p)                  IUnknown_Release(p)

#if !defined(__cplusplus) || defined(CINTERFACE)
#define IKsPropertySet_Get(p,a,b,c,d,e,f,g)        (p)->lpVtbl->Get(p,a,b,c,d,e,f,g)
#define IKsPropertySet_Set(p,a,b,c,d,e,f)          (p)->lpVtbl->Set(p,a,b,c,d,e,f)
#define IKsPropertySet_QuerySupport(p,a,b,c)       (p)->lpVtbl->QuerySupport(p,a,b,c)
#else // !defined(__cplusplus) || defined(CINTERFACE)
#define IKsPropertySet_Get(p,a,b,c,d,e,f,g)        (p)->Get(a,b,c,d,e,f,g)
#define IKsPropertySet_Set(p,a,b,c,d,e,f)          (p)->Set(a,b,c,d,e,f)
#define IKsPropertySet_QuerySupport(p,a,b,c)       (p)->QuerySupport(a,b,c)
#endif // !defined(__cplusplus) || defined(CINTERFACE)

#endif // _IKsPropertySet_


//
// Return Codes
//

// The function completed successfully
#define DS_OK                           S_OK

// The function completed successfully, but we had to substitute the 3D algorithm
#define DS_NO_VIRTUALIZATION            MAKE_HRESULT(0, _FACDS, 10)

// The call failed because resources (such as a priority level)
// were already being used by another caller
#define DSERR_ALLOCATED                 MAKE_DSHRESULT(10)

// The control (vol,pan,etc.) requested by the caller is not available
#define DSERR_CONTROLUNAVAIL            MAKE_DSHRESULT(30)

// An invalid parameter was passed to the returning function
#define DSERR_INVALIDPARAM              E_INVALIDARG

// This call is not valid for the current state of this object
#define DSERR_INVALIDCALL               MAKE_DSHRESULT(50)

// An undetermined error occured inside the DirectSound subsystem
#define DSERR_GENERIC                   E_FAIL

// The caller does not have the priority level required for the function to
// succeed
#define DSERR_PRIOLEVELNEEDED           MAKE_DSHRESULT(70)

// Not enough free memory is available to complete the operation
#define DSERR_OUTOFMEMORY               E_OUTOFMEMORY

// The specified WAVE format is not supported
#define DSERR_BADFORMAT                 MAKE_DSHRESULT(100)

// The function called is not supported at this time
#define DSERR_UNSUPPORTED               E_NOTIMPL

// No sound driver is available for use
#define DSERR_NODRIVER                  MAKE_DSHRESULT(120)

// This object is already initialized
#define DSERR_ALREADYINITIALIZED        MAKE_DSHRESULT(130)

// This object does not support aggregation
#define DSERR_NOAGGREGATION             CLASS_E_NOAGGREGATION

// The buffer memory has been lost, and must be restored
#define DSERR_BUFFERLOST                MAKE_DSHRESULT(150)

// Another app has a higher priority level, preventing this call from
// succeeding
#define DSERR_OTHERAPPHASPRIO           MAKE_DSHRESULT(160)

// This object has not been initialized
#define DSERR_UNINITIALIZED             MAKE_DSHRESULT(170)

// The requested COM interface is not available
#define DSERR_NOINTERFACE               E_NOINTERFACE

// Access is denied
#define DSERR_ACCESSDENIED              E_ACCESSDENIED

//
// Flags
//

#define DSCAPS_PRIMARYMONO          0x00000001
#define DSCAPS_PRIMARYSTEREO        0x00000002
#define DSCAPS_PRIMARY8BIT          0x00000004
#define DSCAPS_PRIMARY16BIT         0x00000008
#define DSCAPS_CONTINUOUSRATE       0x00000010
#define DSCAPS_EMULDRIVER           0x00000020
#define DSCAPS_CERTIFIED            0x00000040
#define DSCAPS_SECONDARYMONO        0x00000100
#define DSCAPS_SECONDARYSTEREO      0x00000200
#define DSCAPS_SECONDARY8BIT        0x00000400
#define DSCAPS_SECONDARY16BIT       0x00000800

#define DSSCL_NORMAL                0x00000001
#define DSSCL_PRIORITY              0x00000002
#define DSSCL_EXCLUSIVE             0x00000003
#define DSSCL_WRITEPRIMARY          0x00000004

#define DSSPEAKER_HEADPHONE         0x00000001
#define DSSPEAKER_MONO              0x00000002
#define DSSPEAKER_QUAD              0x00000003
#define DSSPEAKER_STEREO            0x00000004
#define DSSPEAKER_SURROUND          0x00000005
#define DSSPEAKER_5POINT1           0x00000006

#define DSSPEAKER_GEOMETRY_MIN      0x00000005  //   5 degrees
#define DSSPEAKER_GEOMETRY_NARROW   0x0000000A  //  10 degrees
#define DSSPEAKER_GEOMETRY_WIDE     0x00000014  //  20 degrees
#define DSSPEAKER_GEOMETRY_MAX      0x000000B4  // 180 degrees

#define DSSPEAKER_COMBINED(c, g)    ((DWORD)(((BYTE)(c)) | ((DWORD)((BYTE)(g))) << 16))
#define DSSPEAKER_CONFIG(a)         ((BYTE)(a))
#define DSSPEAKER_GEOMETRY(a)       ((BYTE)(((DWORD)(a) >> 16) & 0x00FF))

#define DSBCAPS_PRIMARYBUFFER       0x00000001
#define DSBCAPS_STATIC              0x00000002
#define DSBCAPS_LOCHARDWARE         0x00000004
#define DSBCAPS_LOCSOFTWARE         0x00000008
#define DSBCAPS_CTRL3D              0x00000010
#define DSBCAPS_CTRLFREQUENCY       0x00000020
#define DSBCAPS_CTRLPAN             0x00000040
#define DSBCAPS_CTRLVOLUME          0x00000080
#define DSBCAPS_CTRLPOSITIONNOTIFY  0x00000100
#define DSBCAPS_STICKYFOCUS         0x00004000
#define DSBCAPS_GLOBALFOCUS         0x00008000
#define DSBCAPS_GETCURRENTPOSITION2 0x00010000
#define DSBCAPS_MUTE3DATMAXDISTANCE 0x00020000
#define DSBCAPS_LOCDEFER            0x00040000

#define DSBPLAY_LOOPING             0x00000001
#define DSBPLAY_LOCHARDWARE         0x00000002
#define DSBPLAY_LOCSOFTWARE         0x00000004
#define DSBPLAY_TERMINATEBY_TIME    0x00000008
#define DSBPLAY_TERMINATEBY_DISTANCE    0x000000010
#define DSBPLAY_TERMINATEBY_PRIORITY    0x000000020

#define DSBSTATUS_PLAYING           0x00000001
#define DSBSTATUS_BUFFERLOST        0x00000002
#define DSBSTATUS_LOOPING           0x00000004
#define DSBSTATUS_LOCHARDWARE       0x00000008
#define DSBSTATUS_LOCSOFTWARE       0x00000010
#define DSBSTATUS_TERMINATED        0x00000020

#define DSBLOCK_FROMWRITECURSOR     0x00000001
#define DSBLOCK_ENTIREBUFFER        0x00000002

#define DSBFREQUENCY_MIN            100
#define DSBFREQUENCY_MAX            100000
#define DSBFREQUENCY_ORIGINAL       0

#define DSBPAN_LEFT                 -10000
#define DSBPAN_CENTER               0
#define DSBPAN_RIGHT                10000

#define DSBVOLUME_MIN               -10000
#define DSBVOLUME_MAX               0

#define DSBSIZE_MIN                 4
#define DSBSIZE_MAX                 0x0FFFFFFF

#define DS3DMODE_NORMAL             0x00000000
#define DS3DMODE_HEADRELATIVE       0x00000001
#define DS3DMODE_DISABLE            0x00000002

#define DS3D_IMMEDIATE              0x00000000
#define DS3D_DEFERRED               0x00000001

#define DS3D_MINDISTANCEFACTOR      FLT_MIN
#define DS3D_MAXDISTANCEFACTOR      FLT_MAX
#define DS3D_DEFAULTDISTANCEFACTOR  1.0f

#define DS3D_MINROLLOFFFACTOR       0.0f
#define DS3D_MAXROLLOFFFACTOR       10.0f
#define DS3D_DEFAULTROLLOFFFACTOR   1.0f

#define DS3D_MINDOPPLERFACTOR       0.0f
#define DS3D_MAXDOPPLERFACTOR       10.0f
#define DS3D_DEFAULTDOPPLERFACTOR   1.0f

#define DS3D_DEFAULTMINDISTANCE     1.0f
#define DS3D_DEFAULTMAXDISTANCE     1000000000.0f

#define DS3D_MINCONEANGLE           0
#define DS3D_MAXCONEANGLE           360
#define DS3D_DEFAULTCONEANGLE       360

#define DS3D_DEFAULTCONEOUTSIDEVOLUME DSBVOLUME_MAX

#define DSCCAPS_EMULDRIVER          DSCAPS_EMULDRIVER
#define DSCCAPS_CERTIFIED           DSCAPS_CERTIFIED

#define DSCBCAPS_WAVEMAPPED         0x80000000

#define DSCBLOCK_ENTIREBUFFER       0x00000001

#define DSCBSTATUS_CAPTURING        0x00000001
#define DSCBSTATUS_LOOPING          0x00000002

#define DSCBSTART_LOOPING           0x00000001

#define DSBPN_OFFSETSTOP            0xFFFFFFFF


//
// DirectSound3D Algorithms
//

// Default DirectSound3D algorithm {00000000-0000-0000-0000-000000000000}
#define DS3DALG_DEFAULT GUID_NULL

// No virtualization {C241333F-1C1B-11d2-94F5-00C04FC28ACA}
DEFINE_GUID(DS3DALG_NO_VIRTUALIZATION, 0xc241333f, 0x1c1b, 0x11d2, 0x94, 0xf5, 0x0, 0xc0, 0x4f, 0xc2, 0x8a, 0xca);

// High-quality HRTF algorithm {C2413340-1C1B-11d2-94F5-00C04FC28ACA}
DEFINE_GUID(DS3DALG_HRTF_FULL, 0xc2413340, 0x1c1b, 0x11d2, 0x94, 0xf5, 0x0, 0xc0, 0x4f, 0xc2, 0x8a, 0xca);

// Lower-quality HRTF algorithm {C2413342-1C1B-11d2-94F5-00C04FC28ACA}
DEFINE_GUID(DS3DALG_HRTF_LIGHT, 0xc2413342, 0x1c1b, 0x11d2, 0x94, 0xf5, 0x0, 0xc0, 0x4f, 0xc2, 0x8a, 0xca);

#ifdef __cplusplus
};
#endif // __cplusplus

#endif  // __DSOUND_INCLUDED__

