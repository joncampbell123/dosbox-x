/****************************************************************************
 *
 *  Copyright (C) 1996-1999 Microsoft Corporation.  All Rights Reserved.
 *
 *  File:       dinput.h
 *  Content:    DirectInput include file
 *
 ****************************************************************************/

#ifndef __DINPUT_INCLUDED__
#define __DINPUT_INCLUDED__

#ifndef DIJ_RINGZERO

#ifdef _WIN32
#define COM_NO_WINDOWS_H
#include <objbase.h>
#endif

#endif /* DIJ_RINGZERO */

#ifdef __cplusplus
extern "C" {
#endif




/*
 *  To build applications for older versions of DirectInput 
 *
 *  #define DIRECTINPUT_VERSION 0x0300
 *  or
 *  #define DIRECTINPUT_VERSION 0x0500
 *
 *  before #include <dinput.h>.  By default, #include <dinput.h>
 *  will produce a DirectX 7-compatible header file.
 *
 */

#ifndef DIRECTINPUT_VERSION
#define DIRECTINPUT_VERSION         0x0700
#endif

#ifndef DIJ_RINGZERO
/****************************************************************************
 *
 *      Class IDs
 *
 ****************************************************************************/

DEFINE_GUID(CLSID_DirectInput,      0x25E609E0,0xB259,0x11CF,0xBF,0xC7,0x44,0x45,0x53,0x54,0x00,0x00);
DEFINE_GUID(CLSID_DirectInputDevice,0x25E609E1,0xB259,0x11CF,0xBF,0xC7,0x44,0x45,0x53,0x54,0x00,0x00);

/****************************************************************************
 *
 *      Interfaces
 *
 ****************************************************************************/

DEFINE_GUID(IID_IDirectInputA,     0x89521360,0xAA8A,0x11CF,0xBF,0xC7,0x44,0x45,0x53,0x54,0x00,0x00);
DEFINE_GUID(IID_IDirectInputW,     0x89521361,0xAA8A,0x11CF,0xBF,0xC7,0x44,0x45,0x53,0x54,0x00,0x00);
DEFINE_GUID(IID_IDirectInput2A,    0x5944E662,0xAA8A,0x11CF,0xBF,0xC7,0x44,0x45,0x53,0x54,0x00,0x00);
DEFINE_GUID(IID_IDirectInput2W,    0x5944E663,0xAA8A,0x11CF,0xBF,0xC7,0x44,0x45,0x53,0x54,0x00,0x00);
DEFINE_GUID(IID_IDirectInput7A,    0X9A4CB684,0X236D,0X11D3,0X8E,0X9D,0X00,0XC0,0X4F,0X68,0X44,0XAE);
DEFINE_GUID(IID_IDirectInput7W,    0X9A4CB685,0X236D,0X11D3,0X8E,0X9D,0X00,0XC0,0X4F,0X68,0X44,0XAE);
DEFINE_GUID(IID_IDirectInputDeviceA, 0x5944E680,0xC92E,0x11CF,0xBF,0xC7,0x44,0x45,0x53,0x54,0x00,0x00);
DEFINE_GUID(IID_IDirectInputDeviceW, 0x5944E681,0xC92E,0x11CF,0xBF,0xC7,0x44,0x45,0x53,0x54,0x00,0x00);
DEFINE_GUID(IID_IDirectInputDevice2A,0x5944E682,0xC92E,0x11CF,0xBF,0xC7,0x44,0x45,0x53,0x54,0x00,0x00);
DEFINE_GUID(IID_IDirectInputDevice2W,0x5944E683,0xC92E,0x11CF,0xBF,0xC7,0x44,0x45,0x53,0x54,0x00,0x00);

DEFINE_GUID(IID_IDirectInputEffect,  0xE7E1F7C0,0x88D2,0x11D0,0x9A,0xD0,0x00,0xA0,0xC9,0xA0,0x6E,0x35);

DEFINE_GUID(IID_IDirectInputDevice7A,0X57D7C6BC,0X2356,0X11D3,0X8E,0X9D,0X00,0XC0,0X4F,0X68,0X44,0XAE);
DEFINE_GUID(IID_IDirectInputDevice7W,0X57D7C6BD,0X2356,0X11D3,0X8E,0X9D,0X00,0XC0,0X4F,0X68,0X44,0XAE);

/****************************************************************************
 *
 *      Predefined object types
 *
 ****************************************************************************/

DEFINE_GUID(GUID_XAxis,   0xA36D02E0,0xC9F3,0x11CF,0xBF,0xC7,0x44,0x45,0x53,0x54,0x00,0x00);
DEFINE_GUID(GUID_YAxis,   0xA36D02E1,0xC9F3,0x11CF,0xBF,0xC7,0x44,0x45,0x53,0x54,0x00,0x00);
DEFINE_GUID(GUID_ZAxis,   0xA36D02E2,0xC9F3,0x11CF,0xBF,0xC7,0x44,0x45,0x53,0x54,0x00,0x00);
DEFINE_GUID(GUID_RxAxis,  0xA36D02F4,0xC9F3,0x11CF,0xBF,0xC7,0x44,0x45,0x53,0x54,0x00,0x00);
DEFINE_GUID(GUID_RyAxis,  0xA36D02F5,0xC9F3,0x11CF,0xBF,0xC7,0x44,0x45,0x53,0x54,0x00,0x00);
DEFINE_GUID(GUID_RzAxis,  0xA36D02E3,0xC9F3,0x11CF,0xBF,0xC7,0x44,0x45,0x53,0x54,0x00,0x00);
DEFINE_GUID(GUID_Slider,  0xA36D02E4,0xC9F3,0x11CF,0xBF,0xC7,0x44,0x45,0x53,0x54,0x00,0x00);

DEFINE_GUID(GUID_Button,  0xA36D02F0,0xC9F3,0x11CF,0xBF,0xC7,0x44,0x45,0x53,0x54,0x00,0x00);
DEFINE_GUID(GUID_Key,     0x55728220,0xD33C,0x11CF,0xBF,0xC7,0x44,0x45,0x53,0x54,0x00,0x00);

DEFINE_GUID(GUID_POV,     0xA36D02F2,0xC9F3,0x11CF,0xBF,0xC7,0x44,0x45,0x53,0x54,0x00,0x00);

DEFINE_GUID(GUID_Unknown, 0xA36D02F3,0xC9F3,0x11CF,0xBF,0xC7,0x44,0x45,0x53,0x54,0x00,0x00);

/****************************************************************************
 *
 *      Predefined product GUIDs
 *
 ****************************************************************************/

DEFINE_GUID(GUID_SysMouse,   0x6F1D2B60,0xD5A0,0x11CF,0xBF,0xC7,0x44,0x45,0x53,0x54,0x00,0x00);
DEFINE_GUID(GUID_SysKeyboard,0x6F1D2B61,0xD5A0,0x11CF,0xBF,0xC7,0x44,0x45,0x53,0x54,0x00,0x00);
DEFINE_GUID(GUID_Joystick   ,0x6F1D2B70,0xD5A0,0x11CF,0xBF,0xC7,0x44,0x45,0x53,0x54,0x00,0x00);
DEFINE_GUID(GUID_SysMouseEm, 0x6F1D2B80,0xD5A0,0x11CF,0xBF,0xC7,0x44,0x45,0x53,0x54,0x00,0x00);
DEFINE_GUID(GUID_SysMouseEm2,0x6F1D2B81,0xD5A0,0x11CF,0xBF,0xC7,0x44,0x45,0x53,0x54,0x00,0x00);
DEFINE_GUID(GUID_SysKeyboardEm, 0x6F1D2B82,0xD5A0,0x11CF,0xBF,0xC7,0x44,0x45,0x53,0x54,0x00,0x00);
DEFINE_GUID(GUID_SysKeyboardEm2,0x6F1D2B83,0xD5A0,0x11CF,0xBF,0xC7,0x44,0x45,0x53,0x54,0x00,0x00);

/****************************************************************************
 *
 *      Predefined force feedback effects
 *
 ****************************************************************************/

DEFINE_GUID(GUID_ConstantForce, 0x13541C20,0x8E33,0x11D0,0x9A,0xD0,0x00,0xA0,0xC9,0xA0,0x6E,0x35);
DEFINE_GUID(GUID_RampForce,     0x13541C21,0x8E33,0x11D0,0x9A,0xD0,0x00,0xA0,0xC9,0xA0,0x6E,0x35);
DEFINE_GUID(GUID_Square,        0x13541C22,0x8E33,0x11D0,0x9A,0xD0,0x00,0xA0,0xC9,0xA0,0x6E,0x35);
DEFINE_GUID(GUID_Sine,          0x13541C23,0x8E33,0x11D0,0x9A,0xD0,0x00,0xA0,0xC9,0xA0,0x6E,0x35);
DEFINE_GUID(GUID_Triangle,      0x13541C24,0x8E33,0x11D0,0x9A,0xD0,0x00,0xA0,0xC9,0xA0,0x6E,0x35);
DEFINE_GUID(GUID_SawtoothUp,    0x13541C25,0x8E33,0x11D0,0x9A,0xD0,0x00,0xA0,0xC9,0xA0,0x6E,0x35);
DEFINE_GUID(GUID_SawtoothDown,  0x13541C26,0x8E33,0x11D0,0x9A,0xD0,0x00,0xA0,0xC9,0xA0,0x6E,0x35);
DEFINE_GUID(GUID_Spring,        0x13541C27,0x8E33,0x11D0,0x9A,0xD0,0x00,0xA0,0xC9,0xA0,0x6E,0x35);
DEFINE_GUID(GUID_Damper,        0x13541C28,0x8E33,0x11D0,0x9A,0xD0,0x00,0xA0,0xC9,0xA0,0x6E,0x35);
DEFINE_GUID(GUID_Inertia,       0x13541C29,0x8E33,0x11D0,0x9A,0xD0,0x00,0xA0,0xC9,0xA0,0x6E,0x35);
DEFINE_GUID(GUID_Friction,      0x13541C2A,0x8E33,0x11D0,0x9A,0xD0,0x00,0xA0,0xC9,0xA0,0x6E,0x35);
DEFINE_GUID(GUID_CustomForce,   0x13541C2B,0x8E33,0x11D0,0x9A,0xD0,0x00,0xA0,0xC9,0xA0,0x6E,0x35);

#endif /* DIJ_RINGZERO */

/****************************************************************************
 *
 *      Interfaces and Structures...
 *
 ****************************************************************************/

#if(DIRECTINPUT_VERSION >= 0x0500)

/****************************************************************************
 *
 *      IDirectInputEffect
 *
 ****************************************************************************/

#define DIEFT_ALL                   0x00000000

#define DIEFT_CONSTANTFORCE         0x00000001
#define DIEFT_RAMPFORCE             0x00000002
#define DIEFT_PERIODIC              0x00000003
#define DIEFT_CONDITION             0x00000004
#define DIEFT_CUSTOMFORCE           0x00000005
#define DIEFT_HARDWARE              0x000000FF
#define DIEFT_FFATTACK              0x00000200
#define DIEFT_FFFADE                0x00000400
#define DIEFT_SATURATION            0x00000800
#define DIEFT_POSNEGCOEFFICIENTS    0x00001000
#define DIEFT_POSNEGSATURATION      0x00002000
#define DIEFT_DEADBAND              0x00004000
#define DIEFT_STARTDELAY            0x00008000
#define DIEFT_GETTYPE(n)            LOBYTE(n)

#define DI_DEGREES                  100
#define DI_FFNOMINALMAX             10000
#define DI_SECONDS                  1000000

typedef struct DICONSTANTFORCE {
    LONG  lMagnitude;
} DICONSTANTFORCE, *LPDICONSTANTFORCE;
typedef const DICONSTANTFORCE *LPCDICONSTANTFORCE;

typedef struct DIRAMPFORCE {
    LONG  lStart;
    LONG  lEnd;
} DIRAMPFORCE, *LPDIRAMPFORCE;
typedef const DIRAMPFORCE *LPCDIRAMPFORCE;

typedef struct DIPERIODIC {
    DWORD dwMagnitude;
    LONG  lOffset;
    DWORD dwPhase;
    DWORD dwPeriod;
} DIPERIODIC, *LPDIPERIODIC;
typedef const DIPERIODIC *LPCDIPERIODIC;

typedef struct DICONDITION {
    LONG  lOffset;
    LONG  lPositiveCoefficient;
    LONG  lNegativeCoefficient;
    DWORD dwPositiveSaturation;
    DWORD dwNegativeSaturation;
    LONG  lDeadBand;
} DICONDITION, *LPDICONDITION;
typedef const DICONDITION *LPCDICONDITION;

typedef struct DICUSTOMFORCE {
    DWORD cChannels;
    DWORD dwSamplePeriod;
    DWORD cSamples;
    LPLONG rglForceData;
} DICUSTOMFORCE, *LPDICUSTOMFORCE;
typedef const DICUSTOMFORCE *LPCDICUSTOMFORCE;


typedef struct DIENVELOPE {
    DWORD dwSize;                   /* sizeof(DIENVELOPE)   */
    DWORD dwAttackLevel;
    DWORD dwAttackTime;             /* Microseconds         */
    DWORD dwFadeLevel;
    DWORD dwFadeTime;               /* Microseconds         */
} DIENVELOPE, *LPDIENVELOPE;
typedef const DIENVELOPE *LPCDIENVELOPE;


/* This structure is defined for DirectX 5.0 compatibility */
typedef struct DIEFFECT_DX5 {
    DWORD dwSize;                   /* sizeof(DIEFFECT_DX5) */
    DWORD dwFlags;                  /* DIEFF_*              */
    DWORD dwDuration;               /* Microseconds         */
    DWORD dwSamplePeriod;           /* Microseconds         */
    DWORD dwGain;
    DWORD dwTriggerButton;          /* or DIEB_NOTRIGGER    */
    DWORD dwTriggerRepeatInterval;  /* Microseconds         */
    DWORD cAxes;                    /* Number of axes       */
    LPDWORD rgdwAxes;               /* Array of axes        */
    LPLONG rglDirection;            /* Array of directions  */
    LPDIENVELOPE lpEnvelope;        /* Optional             */
    DWORD cbTypeSpecificParams;     /* Size of params       */
    LPVOID lpvTypeSpecificParams;   /* Pointer to params    */
} DIEFFECT_DX5, *LPDIEFFECT_DX5;
typedef const DIEFFECT_DX5 *LPCDIEFFECT_DX5;

typedef struct DIEFFECT {
    DWORD dwSize;                   /* sizeof(DIEFFECT)     */
    DWORD dwFlags;                  /* DIEFF_*              */
    DWORD dwDuration;               /* Microseconds         */
    DWORD dwSamplePeriod;           /* Microseconds         */
    DWORD dwGain;
    DWORD dwTriggerButton;          /* or DIEB_NOTRIGGER    */
    DWORD dwTriggerRepeatInterval;  /* Microseconds         */
    DWORD cAxes;                    /* Number of axes       */
    LPDWORD rgdwAxes;               /* Array of axes        */
    LPLONG rglDirection;            /* Array of directions  */
    LPDIENVELOPE lpEnvelope;        /* Optional             */
    DWORD cbTypeSpecificParams;     /* Size of params       */
    LPVOID lpvTypeSpecificParams;   /* Pointer to params    */
#if(DIRECTINPUT_VERSION >= 0x0600)
    DWORD  dwStartDelay;            /* Microseconds         */
#endif /* DIRECTINPUT_VERSION >= 0x0600 */
} DIEFFECT, *LPDIEFFECT;
typedef DIEFFECT DIEFFECT_DX6;
typedef LPDIEFFECT LPDIEFFECT_DX6;
typedef const DIEFFECT *LPCDIEFFECT;


#if(DIRECTINPUT_VERSION >= 0x0700)
#ifndef DIJ_RINGZERO
typedef struct DIFILEEFFECT{
    DWORD       dwSize;
    GUID        GuidEffect;
    LPCDIEFFECT lpDiEffect;
    CHAR        szFriendlyName[MAX_PATH];                    
}DIFILEEFFECT, *LPDIFILEEFFECT;                                            
typedef const DIFILEEFFECT *LPCDIFILEEFFECT;                                            
typedef BOOL (FAR PASCAL * LPDIENUMEFFECTSINFILECALLBACK)(LPCDIFILEEFFECT , LPVOID); 
#endif /* DIJ_RINGZERO */
#endif /* DIRECTINPUT_VERSION >= 0x0700 */

#define DIEFF_OBJECTIDS             0x00000001
#define DIEFF_OBJECTOFFSETS         0x00000002
#define DIEFF_CARTESIAN             0x00000010
#define DIEFF_POLAR                 0x00000020
#define DIEFF_SPHERICAL             0x00000040

#define DIEP_DURATION               0x00000001
#define DIEP_SAMPLEPERIOD           0x00000002
#define DIEP_GAIN                   0x00000004
#define DIEP_TRIGGERBUTTON          0x00000008
#define DIEP_TRIGGERREPEATINTERVAL  0x00000010
#define DIEP_AXES                   0x00000020
#define DIEP_DIRECTION              0x00000040
#define DIEP_ENVELOPE               0x00000080
#define DIEP_TYPESPECIFICPARAMS     0x00000100
#if(DIRECTINPUT_VERSION >= 0x0600)
#define DIEP_STARTDELAY             0x00000200
#define DIEP_ALLPARAMS_DX5          0x000001FF
#define DIEP_ALLPARAMS              0x000003FF
#else /* DIRECTINPUT_VERSION < 0x0600 */
#define DIEP_ALLPARAMS              0x000001FF
#endif /* DIRECTINPUT_VERSION < 0x0600 */
#define DIEP_START                  0x20000000
#define DIEP_NORESTART              0x40000000
#define DIEP_NODOWNLOAD             0x80000000
#define DIEB_NOTRIGGER              0xFFFFFFFF

#define DIES_SOLO                   0x00000001
#define DIES_NODOWNLOAD             0x80000000

#define DIEGES_PLAYING              0x00000001
#define DIEGES_EMULATED             0x00000002

typedef struct DIEFFESCAPE {
    DWORD   dwSize;
    DWORD   dwCommand;
    LPVOID  lpvInBuffer;
    DWORD   cbInBuffer;
    LPVOID  lpvOutBuffer;
    DWORD   cbOutBuffer;
} DIEFFESCAPE, *LPDIEFFESCAPE;

#ifndef DIJ_RINGZERO

#undef INTERFACE
#define INTERFACE IDirectInputEffect

DECLARE_INTERFACE_(IDirectInputEffect, IUnknown)
{
    /*** IUnknown methods ***/
    STDMETHOD(QueryInterface)(THIS_ REFIID riid, LPVOID * ppvObj) PURE;
    STDMETHOD_(ULONG,AddRef)(THIS) PURE;
    STDMETHOD_(ULONG,Release)(THIS) PURE;

    /*** IDirectInputEffect methods ***/
    STDMETHOD(Initialize)(THIS_ HINSTANCE,DWORD,REFGUID) PURE;
    STDMETHOD(GetEffectGuid)(THIS_ LPGUID) PURE;
    STDMETHOD(GetParameters)(THIS_ LPDIEFFECT,DWORD) PURE;
    STDMETHOD(SetParameters)(THIS_ LPCDIEFFECT,DWORD) PURE;
    STDMETHOD(Start)(THIS_ DWORD,DWORD) PURE;
    STDMETHOD(Stop)(THIS) PURE;
    STDMETHOD(GetEffectStatus)(THIS_ LPDWORD) PURE;
    STDMETHOD(Download)(THIS) PURE;
    STDMETHOD(Unload)(THIS) PURE;
    STDMETHOD(Escape)(THIS_ LPDIEFFESCAPE) PURE;
};

typedef struct IDirectInputEffect *LPDIRECTINPUTEFFECT;

#if !defined(__cplusplus) || defined(CINTERFACE)
#define IDirectInputEffect_QueryInterface(p,a,b) (p)->lpVtbl->QueryInterface(p,a,b)
#define IDirectInputEffect_AddRef(p) (p)->lpVtbl->AddRef(p)
#define IDirectInputEffect_Release(p) (p)->lpVtbl->Release(p)
#define IDirectInputEffect_Initialize(p,a,b,c) (p)->lpVtbl->Initialize(p,a,b,c)
#define IDirectInputEffect_GetEffectGuid(p,a) (p)->lpVtbl->GetEffectGuid(p,a)
#define IDirectInputEffect_GetParameters(p,a,b) (p)->lpVtbl->GetParameters(p,a,b)
#define IDirectInputEffect_SetParameters(p,a,b) (p)->lpVtbl->SetParameters(p,a,b)
#define IDirectInputEffect_Start(p,a,b) (p)->lpVtbl->Start(p,a,b)
#define IDirectInputEffect_Stop(p) (p)->lpVtbl->Stop(p)
#define IDirectInputEffect_GetEffectStatus(p,a) (p)->lpVtbl->GetEffectStatus(p,a)
#define IDirectInputEffect_Download(p) (p)->lpVtbl->Download(p)
#define IDirectInputEffect_Unload(p) (p)->lpVtbl->Unload(p)
#define IDirectInputEffect_Escape(p,a) (p)->lpVtbl->Escape(p,a)
#else
#define IDirectInputEffect_QueryInterface(p,a,b) (p)->QueryInterface(a,b)
#define IDirectInputEffect_AddRef(p) (p)->AddRef()
#define IDirectInputEffect_Release(p) (p)->Release()
#define IDirectInputEffect_Initialize(p,a,b,c) (p)->Initialize(a,b,c)
#define IDirectInputEffect_GetEffectGuid(p,a) (p)->GetEffectGuid(a)
#define IDirectInputEffect_GetParameters(p,a,b) (p)->GetParameters(a,b)
#define IDirectInputEffect_SetParameters(p,a,b) (p)->SetParameters(a,b)
#define IDirectInputEffect_Start(p,a,b) (p)->Start(a,b)
#define IDirectInputEffect_Stop(p) (p)->Stop()
#define IDirectInputEffect_GetEffectStatus(p,a) (p)->GetEffectStatus(a)
#define IDirectInputEffect_Download(p) (p)->Download()
#define IDirectInputEffect_Unload(p) (p)->Unload()
#define IDirectInputEffect_Escape(p,a) (p)->Escape(a)
#endif

#endif /* DIJ_RINGZERO */

#endif /* DIRECTINPUT_VERSION >= 0x0500 */

/****************************************************************************
 *
 *      IDirectInputDevice
 *
 ****************************************************************************/

#define DIDEVTYPE_DEVICE    1
#define DIDEVTYPE_MOUSE     2
#define DIDEVTYPE_KEYBOARD  3
#define DIDEVTYPE_JOYSTICK  4
#define DIDEVTYPE_HID       0x00010000

#define DIDEVTYPEMOUSE_UNKNOWN          1
#define DIDEVTYPEMOUSE_TRADITIONAL      2
#define DIDEVTYPEMOUSE_FINGERSTICK      3
#define DIDEVTYPEMOUSE_TOUCHPAD         4
#define DIDEVTYPEMOUSE_TRACKBALL        5

#define DIDEVTYPEKEYBOARD_UNKNOWN       0
#define DIDEVTYPEKEYBOARD_PCXT          1
#define DIDEVTYPEKEYBOARD_OLIVETTI      2
#define DIDEVTYPEKEYBOARD_PCAT          3
#define DIDEVTYPEKEYBOARD_PCENH         4
#define DIDEVTYPEKEYBOARD_NOKIA1050     5
#define DIDEVTYPEKEYBOARD_NOKIA9140     6
#define DIDEVTYPEKEYBOARD_NEC98         7
#define DIDEVTYPEKEYBOARD_NEC98LAPTOP   8
#define DIDEVTYPEKEYBOARD_NEC98106      9
#define DIDEVTYPEKEYBOARD_JAPAN106     10
#define DIDEVTYPEKEYBOARD_JAPANAX      11
#define DIDEVTYPEKEYBOARD_J3100        12

#define DIDEVTYPEJOYSTICK_UNKNOWN       1
#define DIDEVTYPEJOYSTICK_TRADITIONAL   2
#define DIDEVTYPEJOYSTICK_FLIGHTSTICK   3
#define DIDEVTYPEJOYSTICK_GAMEPAD       4
#define DIDEVTYPEJOYSTICK_RUDDER        5
#define DIDEVTYPEJOYSTICK_WHEEL         6
#define DIDEVTYPEJOYSTICK_HEADTRACKER   7

#define GET_DIDEVICE_TYPE(dwDevType)    LOBYTE(dwDevType)
#define GET_DIDEVICE_SUBTYPE(dwDevType) HIBYTE(dwDevType)

#if(DIRECTINPUT_VERSION >= 0x0500)
/* This structure is defined for DirectX 3.0 compatibility */
typedef struct DIDEVCAPS_DX3 {
    DWORD   dwSize;
    DWORD   dwFlags;
    DWORD   dwDevType;
    DWORD   dwAxes;
    DWORD   dwButtons;
    DWORD   dwPOVs;
} DIDEVCAPS_DX3, *LPDIDEVCAPS_DX3;
#endif /* DIRECTINPUT_VERSION >= 0x0500 */

typedef struct DIDEVCAPS {
    DWORD   dwSize;
    DWORD   dwFlags;
    DWORD   dwDevType;
    DWORD   dwAxes;
    DWORD   dwButtons;
    DWORD   dwPOVs;
#if(DIRECTINPUT_VERSION >= 0x0500)
    DWORD   dwFFSamplePeriod;
    DWORD   dwFFMinTimeResolution;
    DWORD   dwFirmwareRevision;
    DWORD   dwHardwareRevision;
    DWORD   dwFFDriverVersion;
#endif /* DIRECTINPUT_VERSION >= 0x0500 */
} DIDEVCAPS, *LPDIDEVCAPS;

#define DIDC_ATTACHED           0x00000001
#define DIDC_POLLEDDEVICE       0x00000002
#define DIDC_EMULATED           0x00000004
#define DIDC_POLLEDDATAFORMAT   0x00000008
#if(DIRECTINPUT_VERSION >= 0x0500)
#define DIDC_FORCEFEEDBACK      0x00000100
#define DIDC_FFATTACK           0x00000200
#define DIDC_FFFADE             0x00000400
#define DIDC_SATURATION         0x00000800
#define DIDC_POSNEGCOEFFICIENTS 0x00001000
#define DIDC_POSNEGSATURATION   0x00002000
#define DIDC_DEADBAND           0x00004000
#endif /* DIRECTINPUT_VERSION >= 0x0500 */
#define DIDC_STARTDELAY         0x00008000
#if(DIRECTINPUT_VERSION >= 0x050a)
#define DIDC_ALIAS              0x00010000
#define DIDC_PHANTOM            0x00020000
#endif /* DIRECTINPUT_VERSION >= 0x050a */

#define DIDFT_ALL           0x00000000

#define DIDFT_RELAXIS       0x00000001
#define DIDFT_ABSAXIS       0x00000002
#define DIDFT_AXIS          0x00000003

#define DIDFT_PSHBUTTON     0x00000004
#define DIDFT_TGLBUTTON     0x00000008
#define DIDFT_BUTTON        0x0000000C

#define DIDFT_POV           0x00000010
#define DIDFT_COLLECTION    0x00000040
#define DIDFT_NODATA        0x00000080

#define DIDFT_ANYINSTANCE   0x00FFFF00
#define DIDFT_INSTANCEMASK  DIDFT_ANYINSTANCE
#define DIDFT_MAKEINSTANCE(n) ((WORD)(n) << 8)
#define DIDFT_GETTYPE(n)     LOBYTE(n)
#define DIDFT_GETINSTANCE(n) LOWORD((n) >> 8)
#define DIDFT_FFACTUATOR        0x01000000
#define DIDFT_FFEFFECTTRIGGER   0x02000000
#if(DIRECTINPUT_VERSION >= 0x050a)
#define DIDFT_OUTPUT            0x10000000
#define DIDFT_VENDORDEFINED     0x04000000
#define DIDFT_ALIAS             0x08000000
#endif /* DIRECTINPUT_VERSION >= 0x050a */

#define DIDFT_ENUMCOLLECTION(n) ((WORD)(n) << 8)
#define DIDFT_NOCOLLECTION      0x00FFFF00

#ifndef DIJ_RINGZERO

typedef struct _DIOBJECTDATAFORMAT {
    const GUID *pguid;
    DWORD   dwOfs;
    DWORD   dwType;
    DWORD   dwFlags;
} DIOBJECTDATAFORMAT, *LPDIOBJECTDATAFORMAT;
typedef const DIOBJECTDATAFORMAT *LPCDIOBJECTDATAFORMAT;

typedef struct _DIDATAFORMAT {
    DWORD   dwSize;
    DWORD   dwObjSize;
    DWORD   dwFlags;
    DWORD   dwDataSize;
    DWORD   dwNumObjs;
    LPDIOBJECTDATAFORMAT rgodf;
} DIDATAFORMAT, *LPDIDATAFORMAT;
typedef const DIDATAFORMAT *LPCDIDATAFORMAT;

#define DIDF_ABSAXIS            0x00000001
#define DIDF_RELAXIS            0x00000002

#ifdef __cplusplus
extern "C" {
#endif
extern const DIDATAFORMAT c_dfDIMouse;

#if(DIRECTINPUT_VERSION >= 0x0700)
extern const DIDATAFORMAT c_dfDIMouse2;
#endif /* DIRECTINPUT_VERSION >= 0x0700 */

extern const DIDATAFORMAT c_dfDIKeyboard;

#if(DIRECTINPUT_VERSION >= 0x0500)
extern const DIDATAFORMAT c_dfDIJoystick;
extern const DIDATAFORMAT c_dfDIJoystick2;
#endif /* DIRECTINPUT_VERSION >= 0x0500 */

#ifdef __cplusplus
};
#endif

#if(DIRECTINPUT_VERSION >= 0x0500)
/* These structures are defined for DirectX 3.0 compatibility */

typedef struct DIDEVICEOBJECTINSTANCE_DX3A {
    DWORD   dwSize;
    GUID    guidType;
    DWORD   dwOfs;
    DWORD   dwType;
    DWORD   dwFlags;
    CHAR    tszName[MAX_PATH];
} DIDEVICEOBJECTINSTANCE_DX3A, *LPDIDEVICEOBJECTINSTANCE_DX3A;
typedef struct DIDEVICEOBJECTINSTANCE_DX3W {
    DWORD   dwSize;
    GUID    guidType;
    DWORD   dwOfs;
    DWORD   dwType;
    DWORD   dwFlags;
    WCHAR   tszName[MAX_PATH];
} DIDEVICEOBJECTINSTANCE_DX3W, *LPDIDEVICEOBJECTINSTANCE_DX3W;
#ifdef UNICODE
typedef DIDEVICEOBJECTINSTANCE_DX3W DIDEVICEOBJECTINSTANCE_DX3;
typedef LPDIDEVICEOBJECTINSTANCE_DX3W LPDIDEVICEOBJECTINSTANCE_DX3;
#else
typedef DIDEVICEOBJECTINSTANCE_DX3A DIDEVICEOBJECTINSTANCE_DX3;
typedef LPDIDEVICEOBJECTINSTANCE_DX3A LPDIDEVICEOBJECTINSTANCE_DX3;
#endif // UNICODE
typedef const DIDEVICEOBJECTINSTANCE_DX3A *LPCDIDEVICEOBJECTINSTANCE_DX3A;
typedef const DIDEVICEOBJECTINSTANCE_DX3W *LPCDIDEVICEOBJECTINSTANCE_DX3W;
typedef const DIDEVICEOBJECTINSTANCE_DX3  *LPCDIDEVICEOBJECTINSTANCE_DX3;
#endif /* DIRECTINPUT_VERSION >= 0x0500 */

typedef struct DIDEVICEOBJECTINSTANCEA {
    DWORD   dwSize;
    GUID    guidType;
    DWORD   dwOfs;
    DWORD   dwType;
    DWORD   dwFlags;
    CHAR    tszName[MAX_PATH];
#if(DIRECTINPUT_VERSION >= 0x0500)
    DWORD   dwFFMaxForce;
    DWORD   dwFFForceResolution;
    WORD    wCollectionNumber;
    WORD    wDesignatorIndex;
    WORD    wUsagePage;
    WORD    wUsage;
    DWORD   dwDimension;
    WORD    wExponent;
    WORD    wReportId;
#endif /* DIRECTINPUT_VERSION >= 0x0500 */
} DIDEVICEOBJECTINSTANCEA, *LPDIDEVICEOBJECTINSTANCEA;
typedef struct DIDEVICEOBJECTINSTANCEW {
    DWORD   dwSize;
    GUID    guidType;
    DWORD   dwOfs;
    DWORD   dwType;
    DWORD   dwFlags;
    WCHAR   tszName[MAX_PATH];
#if(DIRECTINPUT_VERSION >= 0x0500)
    DWORD   dwFFMaxForce;
    DWORD   dwFFForceResolution;
    WORD    wCollectionNumber;
    WORD    wDesignatorIndex;
    WORD    wUsagePage;
    WORD    wUsage;
    DWORD   dwDimension;
    WORD    wExponent;
    WORD    wReportId;
#endif /* DIRECTINPUT_VERSION >= 0x0500 */
} DIDEVICEOBJECTINSTANCEW, *LPDIDEVICEOBJECTINSTANCEW;
#ifdef UNICODE
typedef DIDEVICEOBJECTINSTANCEW DIDEVICEOBJECTINSTANCE;
typedef LPDIDEVICEOBJECTINSTANCEW LPDIDEVICEOBJECTINSTANCE;
#else
typedef DIDEVICEOBJECTINSTANCEA DIDEVICEOBJECTINSTANCE;
typedef LPDIDEVICEOBJECTINSTANCEA LPDIDEVICEOBJECTINSTANCE;
#endif // UNICODE
typedef const DIDEVICEOBJECTINSTANCEA *LPCDIDEVICEOBJECTINSTANCEA;
typedef const DIDEVICEOBJECTINSTANCEW *LPCDIDEVICEOBJECTINSTANCEW;
typedef const DIDEVICEOBJECTINSTANCE  *LPCDIDEVICEOBJECTINSTANCE;

typedef BOOL (FAR PASCAL * LPDIENUMDEVICEOBJECTSCALLBACKA)(LPCDIDEVICEOBJECTINSTANCEA, LPVOID);
typedef BOOL (FAR PASCAL * LPDIENUMDEVICEOBJECTSCALLBACKW)(LPCDIDEVICEOBJECTINSTANCEW, LPVOID);
#ifdef UNICODE
#define LPDIENUMDEVICEOBJECTSCALLBACK  LPDIENUMDEVICEOBJECTSCALLBACKW
#else
#define LPDIENUMDEVICEOBJECTSCALLBACK  LPDIENUMDEVICEOBJECTSCALLBACKA
#endif // !UNICODE

#if(DIRECTINPUT_VERSION >= 0x0500)
#define DIDOI_FFACTUATOR        0x00000001
#define DIDOI_FFEFFECTTRIGGER   0x00000002
#define DIDOI_POLLED            0x00008000
#define DIDOI_ASPECTPOSITION    0x00000100
#define DIDOI_ASPECTVELOCITY    0x00000200
#define DIDOI_ASPECTACCEL       0x00000300
#define DIDOI_ASPECTFORCE       0x00000400
#define DIDOI_ASPECTMASK        0x00000F00
#endif /* DIRECTINPUT_VERSION >= 0x0500 */
#if(DIRECTINPUT_VERSION >= 0x050a)
#define DIDOI_GUIDISUSAGE       0x00010000
#endif /* DIRECTINPUT_VERSION >= 0x050a */

typedef struct DIPROPHEADER {
    DWORD   dwSize;
    DWORD   dwHeaderSize;
    DWORD   dwObj;
    DWORD   dwHow;
} DIPROPHEADER, *LPDIPROPHEADER;
typedef const DIPROPHEADER *LPCDIPROPHEADER;

#define DIPH_DEVICE             0
#define DIPH_BYOFFSET           1
#define DIPH_BYID               2
#if(DIRECTINPUT_VERSION >= 0x050a)
#define DIPH_BYUSAGE            3

#define DIMAKEUSAGEDWORD(UsagePage, Usage) \
                                (DWORD)MAKELONG(Usage, UsagePage)
#endif /* DIRECTINPUT_VERSION >= 0x050a */

typedef struct DIPROPDWORD {
    DIPROPHEADER diph;
    DWORD   dwData;
} DIPROPDWORD, *LPDIPROPDWORD;
typedef const DIPROPDWORD *LPCDIPROPDWORD;

typedef struct DIPROPRANGE {
    DIPROPHEADER diph;
    LONG    lMin;
    LONG    lMax;
} DIPROPRANGE, *LPDIPROPRANGE;
typedef const DIPROPRANGE *LPCDIPROPRANGE;

#define DIPROPRANGE_NOMIN       ((LONG)0x80000000)
#define DIPROPRANGE_NOMAX       ((LONG)0x7FFFFFFF)

#if(DIRECTINPUT_VERSION >= 0x050a)
typedef struct DIPROPCAL {
    DIPROPHEADER diph;
    LONG    lMin;
    LONG    lCenter;
    LONG    lMax;
} DIPROPCAL, *LPDIPROPCAL;
typedef const DIPROPCAL *LPCDIPROPCAL;

typedef struct DIPROPGUIDANDPATH {
    DIPROPHEADER diph;
    GUID    guidClass;
    WCHAR   wszPath[MAX_PATH];
} DIPROPGUIDANDPATH, *LPDIPROPGUIDANDPATH;
typedef const DIPROPGUIDANDPATH *LPCDIPROPGUIDANDPATH;

typedef struct DIPROPSTRING {
    DIPROPHEADER diph;
    WCHAR   wsz[MAX_PATH];
} DIPROPSTRING, *LPDIPROPSTRING;
typedef const DIPROPSTRING *LPCDIPROPSTRING;

#endif /* DIRECTINPUT_VERSION >= 0x050a */
#ifdef __cplusplus
#define MAKEDIPROP(prop)    (*(const GUID *)(prop))
#else
#define MAKEDIPROP(prop)    ((REFGUID)(prop))
#endif

#define DIPROP_BUFFERSIZE       MAKEDIPROP(1)

#define DIPROP_AXISMODE         MAKEDIPROP(2)

#define DIPROPAXISMODE_ABS      0
#define DIPROPAXISMODE_REL      1

#define DIPROP_GRANULARITY      MAKEDIPROP(3)

#define DIPROP_RANGE            MAKEDIPROP(4)

#define DIPROP_DEADZONE         MAKEDIPROP(5)

#define DIPROP_SATURATION       MAKEDIPROP(6)

#define DIPROP_FFGAIN           MAKEDIPROP(7)

#define DIPROP_FFLOAD           MAKEDIPROP(8)

#define DIPROP_AUTOCENTER       MAKEDIPROP(9)

#define DIPROPAUTOCENTER_OFF    0
#define DIPROPAUTOCENTER_ON     1

#define DIPROP_CALIBRATIONMODE  MAKEDIPROP(10)

#define DIPROPCALIBRATIONMODE_COOKED    0
#define DIPROPCALIBRATIONMODE_RAW       1

#if(DIRECTINPUT_VERSION >= 0x050a)
#define DIPROP_CALIBRATION      MAKEDIPROP(11)

#define DIPROP_GUIDANDPATH      MAKEDIPROP(12)

#define DIPROP_INSTANCENAME     MAKEDIPROP(13)

#define DIPROP_PRODUCTNAME      MAKEDIPROP(14)
#endif /* DIRECTINPUT_VERSION >= 0x050a */

#define DIPROP_JOYSTICKID       MAKEDIPROP(15)

#define DIPROP_GETPORTDISPLAYNAME       MAKEDIPROP(16)


#define DIPROP_ENABLEREPORTID       MAKEDIPROP(17)


#define DIPROP_GETPHYSICALRANGE            MAKEDIPROP(18)

#define DIPROP_GETLOGICALRANGE            MAKEDIPROP(19)


typedef struct DIDEVICEOBJECTDATA {
    DWORD   dwOfs;
    DWORD   dwData;
    DWORD   dwTimeStamp;
    DWORD   dwSequence;
} DIDEVICEOBJECTDATA, *LPDIDEVICEOBJECTDATA;
typedef const DIDEVICEOBJECTDATA *LPCDIDEVICEOBJECTDATA;

#define DIGDD_PEEK          0x00000001

#define DISEQUENCE_COMPARE(dwSequence1, cmp, dwSequence2) \
                        ((int)((dwSequence1) - (dwSequence2)) cmp 0)
#define DISCL_EXCLUSIVE     0x00000001
#define DISCL_NONEXCLUSIVE  0x00000002
#define DISCL_FOREGROUND    0x00000004
#define DISCL_BACKGROUND    0x00000008
#define DISCL_NOWINKEY      0x00000010

#if(DIRECTINPUT_VERSION >= 0x0500)
/* These structures are defined for DirectX 3.0 compatibility */

typedef struct DIDEVICEINSTANCE_DX3A {
    DWORD   dwSize;
    GUID    guidInstance;
    GUID    guidProduct;
    DWORD   dwDevType;
    CHAR    tszInstanceName[MAX_PATH];
    CHAR    tszProductName[MAX_PATH];
} DIDEVICEINSTANCE_DX3A, *LPDIDEVICEINSTANCE_DX3A;
typedef struct DIDEVICEINSTANCE_DX3W {
    DWORD   dwSize;
    GUID    guidInstance;
    GUID    guidProduct;
    DWORD   dwDevType;
    WCHAR   tszInstanceName[MAX_PATH];
    WCHAR   tszProductName[MAX_PATH];
} DIDEVICEINSTANCE_DX3W, *LPDIDEVICEINSTANCE_DX3W;
#ifdef UNICODE
typedef DIDEVICEINSTANCE_DX3W DIDEVICEINSTANCE_DX3;
typedef LPDIDEVICEINSTANCE_DX3W LPDIDEVICEINSTANCE_DX3;
#else
typedef DIDEVICEINSTANCE_DX3A DIDEVICEINSTANCE_DX3;
typedef LPDIDEVICEINSTANCE_DX3A LPDIDEVICEINSTANCE_DX3;
#endif // UNICODE
typedef const DIDEVICEINSTANCE_DX3A *LPCDIDEVICEINSTANCE_DX3A;
typedef const DIDEVICEINSTANCE_DX3W *LPCDIDEVICEINSTANCE_DX3W;
typedef const DIDEVICEINSTANCE_DX3  *LPCDIDEVICEINSTANCE_DX3;
#endif /* DIRECTINPUT_VERSION >= 0x0500 */

typedef struct DIDEVICEINSTANCEA {
    DWORD   dwSize;
    GUID    guidInstance;
    GUID    guidProduct;
    DWORD   dwDevType;
    CHAR    tszInstanceName[MAX_PATH];
    CHAR    tszProductName[MAX_PATH];
#if(DIRECTINPUT_VERSION >= 0x0500)
    GUID    guidFFDriver;
    WORD    wUsagePage;
    WORD    wUsage;
#endif /* DIRECTINPUT_VERSION >= 0x0500 */
} DIDEVICEINSTANCEA, *LPDIDEVICEINSTANCEA;
typedef struct DIDEVICEINSTANCEW {
    DWORD   dwSize;
    GUID    guidInstance;
    GUID    guidProduct;
    DWORD   dwDevType;
    WCHAR   tszInstanceName[MAX_PATH];
    WCHAR   tszProductName[MAX_PATH];
#if(DIRECTINPUT_VERSION >= 0x0500)
    GUID    guidFFDriver;
    WORD    wUsagePage;
    WORD    wUsage;
#endif /* DIRECTINPUT_VERSION >= 0x0500 */
} DIDEVICEINSTANCEW, *LPDIDEVICEINSTANCEW;
#ifdef UNICODE
typedef DIDEVICEINSTANCEW DIDEVICEINSTANCE;
typedef LPDIDEVICEINSTANCEW LPDIDEVICEINSTANCE;
#else
typedef DIDEVICEINSTANCEA DIDEVICEINSTANCE;
typedef LPDIDEVICEINSTANCEA LPDIDEVICEINSTANCE;
#endif // UNICODE
typedef const DIDEVICEINSTANCEA *LPCDIDEVICEINSTANCEA;
typedef const DIDEVICEINSTANCEW *LPCDIDEVICEINSTANCEW;
typedef const DIDEVICEINSTANCE  *LPCDIDEVICEINSTANCE;

#undef INTERFACE
#define INTERFACE IDirectInputDeviceW

DECLARE_INTERFACE_(IDirectInputDeviceW, IUnknown)
{
    /*** IUnknown methods ***/
    STDMETHOD(QueryInterface)(THIS_ REFIID riid, LPVOID * ppvObj) PURE;
    STDMETHOD_(ULONG,AddRef)(THIS) PURE;
    STDMETHOD_(ULONG,Release)(THIS) PURE;

    /*** IDirectInputDeviceW methods ***/
    STDMETHOD(GetCapabilities)(THIS_ LPDIDEVCAPS) PURE;
    STDMETHOD(EnumObjects)(THIS_ LPDIENUMDEVICEOBJECTSCALLBACKW,LPVOID,DWORD) PURE;
    STDMETHOD(GetProperty)(THIS_ REFGUID,LPDIPROPHEADER) PURE;
    STDMETHOD(SetProperty)(THIS_ REFGUID,LPCDIPROPHEADER) PURE;
    STDMETHOD(Acquire)(THIS) PURE;
    STDMETHOD(Unacquire)(THIS) PURE;
    STDMETHOD(GetDeviceState)(THIS_ DWORD,LPVOID) PURE;
    STDMETHOD(GetDeviceData)(THIS_ DWORD,LPDIDEVICEOBJECTDATA,LPDWORD,DWORD) PURE;
    STDMETHOD(SetDataFormat)(THIS_ LPCDIDATAFORMAT) PURE;
    STDMETHOD(SetEventNotification)(THIS_ HANDLE) PURE;
    STDMETHOD(SetCooperativeLevel)(THIS_ HWND,DWORD) PURE;
    STDMETHOD(GetObjectInfo)(THIS_ LPDIDEVICEOBJECTINSTANCEW,DWORD,DWORD) PURE;
    STDMETHOD(GetDeviceInfo)(THIS_ LPDIDEVICEINSTANCEW) PURE;
    STDMETHOD(RunControlPanel)(THIS_ HWND,DWORD) PURE;
    STDMETHOD(Initialize)(THIS_ HINSTANCE,DWORD,REFGUID) PURE;
};

typedef struct IDirectInputDeviceW *LPDIRECTINPUTDEVICEW;

#undef INTERFACE
#define INTERFACE IDirectInputDeviceA

DECLARE_INTERFACE_(IDirectInputDeviceA, IUnknown)
{
    /*** IUnknown methods ***/
    STDMETHOD(QueryInterface)(THIS_ REFIID riid, LPVOID * ppvObj) PURE;
    STDMETHOD_(ULONG,AddRef)(THIS) PURE;
    STDMETHOD_(ULONG,Release)(THIS) PURE;

    /*** IDirectInputDeviceA methods ***/
    STDMETHOD(GetCapabilities)(THIS_ LPDIDEVCAPS) PURE;
    STDMETHOD(EnumObjects)(THIS_ LPDIENUMDEVICEOBJECTSCALLBACKA,LPVOID,DWORD) PURE;
    STDMETHOD(GetProperty)(THIS_ REFGUID,LPDIPROPHEADER) PURE;
    STDMETHOD(SetProperty)(THIS_ REFGUID,LPCDIPROPHEADER) PURE;
    STDMETHOD(Acquire)(THIS) PURE;
    STDMETHOD(Unacquire)(THIS) PURE;
    STDMETHOD(GetDeviceState)(THIS_ DWORD,LPVOID) PURE;
    STDMETHOD(GetDeviceData)(THIS_ DWORD,LPDIDEVICEOBJECTDATA,LPDWORD,DWORD) PURE;
    STDMETHOD(SetDataFormat)(THIS_ LPCDIDATAFORMAT) PURE;
    STDMETHOD(SetEventNotification)(THIS_ HANDLE) PURE;
    STDMETHOD(SetCooperativeLevel)(THIS_ HWND,DWORD) PURE;
    STDMETHOD(GetObjectInfo)(THIS_ LPDIDEVICEOBJECTINSTANCEA,DWORD,DWORD) PURE;
    STDMETHOD(GetDeviceInfo)(THIS_ LPDIDEVICEINSTANCEA) PURE;
    STDMETHOD(RunControlPanel)(THIS_ HWND,DWORD) PURE;
    STDMETHOD(Initialize)(THIS_ HINSTANCE,DWORD,REFGUID) PURE;
};

typedef struct IDirectInputDeviceA *LPDIRECTINPUTDEVICEA;

#ifdef UNICODE
#define IID_IDirectInputDevice IID_IDirectInputDeviceW
#define IDirectInputDevice IDirectInputDeviceW
#define IDirectInputDeviceVtbl IDirectInputDeviceWVtbl
#else
#define IID_IDirectInputDevice IID_IDirectInputDeviceA
#define IDirectInputDevice IDirectInputDeviceA
#define IDirectInputDeviceVtbl IDirectInputDeviceAVtbl
#endif
typedef struct IDirectInputDevice *LPDIRECTINPUTDEVICE;

#if !defined(__cplusplus) || defined(CINTERFACE)
#define IDirectInputDevice_QueryInterface(p,a,b) (p)->lpVtbl->QueryInterface(p,a,b)
#define IDirectInputDevice_AddRef(p) (p)->lpVtbl->AddRef(p)
#define IDirectInputDevice_Release(p) (p)->lpVtbl->Release(p)
#define IDirectInputDevice_GetCapabilities(p,a) (p)->lpVtbl->GetCapabilities(p,a)
#define IDirectInputDevice_EnumObjects(p,a,b,c) (p)->lpVtbl->EnumObjects(p,a,b,c)
#define IDirectInputDevice_GetProperty(p,a,b) (p)->lpVtbl->GetProperty(p,a,b)
#define IDirectInputDevice_SetProperty(p,a,b) (p)->lpVtbl->SetProperty(p,a,b)
#define IDirectInputDevice_Acquire(p) (p)->lpVtbl->Acquire(p)
#define IDirectInputDevice_Unacquire(p) (p)->lpVtbl->Unacquire(p)
#define IDirectInputDevice_GetDeviceState(p,a,b) (p)->lpVtbl->GetDeviceState(p,a,b)
#define IDirectInputDevice_GetDeviceData(p,a,b,c,d) (p)->lpVtbl->GetDeviceData(p,a,b,c,d)
#define IDirectInputDevice_SetDataFormat(p,a) (p)->lpVtbl->SetDataFormat(p,a)
#define IDirectInputDevice_SetEventNotification(p,a) (p)->lpVtbl->SetEventNotification(p,a)
#define IDirectInputDevice_SetCooperativeLevel(p,a,b) (p)->lpVtbl->SetCooperativeLevel(p,a,b)
#define IDirectInputDevice_GetObjectInfo(p,a,b,c) (p)->lpVtbl->GetObjectInfo(p,a,b,c)
#define IDirectInputDevice_GetDeviceInfo(p,a) (p)->lpVtbl->GetDeviceInfo(p,a)
#define IDirectInputDevice_RunControlPanel(p,a,b) (p)->lpVtbl->RunControlPanel(p,a,b)
#define IDirectInputDevice_Initialize(p,a,b,c) (p)->lpVtbl->Initialize(p,a,b,c)
#else
#define IDirectInputDevice_QueryInterface(p,a,b) (p)->QueryInterface(a,b)
#define IDirectInputDevice_AddRef(p) (p)->AddRef()
#define IDirectInputDevice_Release(p) (p)->Release()
#define IDirectInputDevice_GetCapabilities(p,a) (p)->GetCapabilities(a)
#define IDirectInputDevice_EnumObjects(p,a,b,c) (p)->EnumObjects(a,b,c)
#define IDirectInputDevice_GetProperty(p,a,b) (p)->GetProperty(a,b)
#define IDirectInputDevice_SetProperty(p,a,b) (p)->SetProperty(a,b)
#define IDirectInputDevice_Acquire(p) (p)->Acquire()
#define IDirectInputDevice_Unacquire(p) (p)->Unacquire()
#define IDirectInputDevice_GetDeviceState(p,a,b) (p)->GetDeviceState(a,b)
#define IDirectInputDevice_GetDeviceData(p,a,b,c,d) (p)->GetDeviceData(a,b,c,d)
#define IDirectInputDevice_SetDataFormat(p,a) (p)->SetDataFormat(a)
#define IDirectInputDevice_SetEventNotification(p,a) (p)->SetEventNotification(a)
#define IDirectInputDevice_SetCooperativeLevel(p,a,b) (p)->SetCooperativeLevel(a,b)
#define IDirectInputDevice_GetObjectInfo(p,a,b,c) (p)->GetObjectInfo(a,b,c)
#define IDirectInputDevice_GetDeviceInfo(p,a) (p)->GetDeviceInfo(a)
#define IDirectInputDevice_RunControlPanel(p,a,b) (p)->RunControlPanel(a,b)
#define IDirectInputDevice_Initialize(p,a,b,c) (p)->Initialize(a,b,c)
#endif

#endif /* DIJ_RINGZERO */


#if(DIRECTINPUT_VERSION >= 0x0500)

#define DISFFC_RESET            0x00000001
#define DISFFC_STOPALL          0x00000002
#define DISFFC_PAUSE            0x00000004
#define DISFFC_CONTINUE         0x00000008
#define DISFFC_SETACTUATORSON   0x00000010
#define DISFFC_SETACTUATORSOFF  0x00000020

#define DIGFFS_EMPTY            0x00000001
#define DIGFFS_STOPPED          0x00000002
#define DIGFFS_PAUSED           0x00000004
#define DIGFFS_ACTUATORSON      0x00000010
#define DIGFFS_ACTUATORSOFF     0x00000020
#define DIGFFS_POWERON          0x00000040
#define DIGFFS_POWEROFF         0x00000080
#define DIGFFS_SAFETYSWITCHON   0x00000100
#define DIGFFS_SAFETYSWITCHOFF  0x00000200
#define DIGFFS_USERFFSWITCHON   0x00000400
#define DIGFFS_USERFFSWITCHOFF  0x00000800
#define DIGFFS_DEVICELOST       0x80000000

#ifndef DIJ_RINGZERO

typedef struct DIEFFECTINFOA {
    DWORD   dwSize;
    GUID    guid;
    DWORD   dwEffType;
    DWORD   dwStaticParams;
    DWORD   dwDynamicParams;
    CHAR    tszName[MAX_PATH];
} DIEFFECTINFOA, *LPDIEFFECTINFOA;
typedef struct DIEFFECTINFOW {
    DWORD   dwSize;
    GUID    guid;
    DWORD   dwEffType;
    DWORD   dwStaticParams;
    DWORD   dwDynamicParams;
    WCHAR   tszName[MAX_PATH];
} DIEFFECTINFOW, *LPDIEFFECTINFOW;
#ifdef UNICODE
typedef DIEFFECTINFOW DIEFFECTINFO;
typedef LPDIEFFECTINFOW LPDIEFFECTINFO;
#else
typedef DIEFFECTINFOA DIEFFECTINFO;
typedef LPDIEFFECTINFOA LPDIEFFECTINFO;
#endif // UNICODE
typedef const DIEFFECTINFOA *LPCDIEFFECTINFOA;
typedef const DIEFFECTINFOW *LPCDIEFFECTINFOW;
typedef const DIEFFECTINFO  *LPCDIEFFECTINFO;

#define DISDD_CONTINUE          0x00000001

typedef BOOL (FAR PASCAL * LPDIENUMEFFECTSCALLBACKA)(LPCDIEFFECTINFOA, LPVOID);
typedef BOOL (FAR PASCAL * LPDIENUMEFFECTSCALLBACKW)(LPCDIEFFECTINFOW, LPVOID);
#ifdef UNICODE
#define LPDIENUMEFFECTSCALLBACK  LPDIENUMEFFECTSCALLBACKW
#else
#define LPDIENUMEFFECTSCALLBACK  LPDIENUMEFFECTSCALLBACKA
#endif // !UNICODE
typedef BOOL (FAR PASCAL * LPDIENUMCREATEDEFFECTOBJECTSCALLBACK)(LPDIRECTINPUTEFFECT, LPVOID);

#undef INTERFACE
#define INTERFACE IDirectInputDevice2W

DECLARE_INTERFACE_(IDirectInputDevice2W, IDirectInputDeviceW)
{
    /*** IUnknown methods ***/
    STDMETHOD(QueryInterface)(THIS_ REFIID riid, LPVOID * ppvObj) PURE;
    STDMETHOD_(ULONG,AddRef)(THIS) PURE;
    STDMETHOD_(ULONG,Release)(THIS) PURE;

    /*** IDirectInputDeviceW methods ***/
    STDMETHOD(GetCapabilities)(THIS_ LPDIDEVCAPS) PURE;
    STDMETHOD(EnumObjects)(THIS_ LPDIENUMDEVICEOBJECTSCALLBACKW,LPVOID,DWORD) PURE;
    STDMETHOD(GetProperty)(THIS_ REFGUID,LPDIPROPHEADER) PURE;
    STDMETHOD(SetProperty)(THIS_ REFGUID,LPCDIPROPHEADER) PURE;
    STDMETHOD(Acquire)(THIS) PURE;
    STDMETHOD(Unacquire)(THIS) PURE;
    STDMETHOD(GetDeviceState)(THIS_ DWORD,LPVOID) PURE;
    STDMETHOD(GetDeviceData)(THIS_ DWORD,LPDIDEVICEOBJECTDATA,LPDWORD,DWORD) PURE;
    STDMETHOD(SetDataFormat)(THIS_ LPCDIDATAFORMAT) PURE;
    STDMETHOD(SetEventNotification)(THIS_ HANDLE) PURE;
    STDMETHOD(SetCooperativeLevel)(THIS_ HWND,DWORD) PURE;
    STDMETHOD(GetObjectInfo)(THIS_ LPDIDEVICEOBJECTINSTANCEW,DWORD,DWORD) PURE;
    STDMETHOD(GetDeviceInfo)(THIS_ LPDIDEVICEINSTANCEW) PURE;
    STDMETHOD(RunControlPanel)(THIS_ HWND,DWORD) PURE;
    STDMETHOD(Initialize)(THIS_ HINSTANCE,DWORD,REFGUID) PURE;

    /*** IDirectInputDevice2W methods ***/
    STDMETHOD(CreateEffect)(THIS_ REFGUID,LPCDIEFFECT,LPDIRECTINPUTEFFECT *,LPUNKNOWN) PURE;
    STDMETHOD(EnumEffects)(THIS_ LPDIENUMEFFECTSCALLBACKW,LPVOID,DWORD) PURE;
    STDMETHOD(GetEffectInfo)(THIS_ LPDIEFFECTINFOW,REFGUID) PURE;
    STDMETHOD(GetForceFeedbackState)(THIS_ LPDWORD) PURE;
    STDMETHOD(SendForceFeedbackCommand)(THIS_ DWORD) PURE;
    STDMETHOD(EnumCreatedEffectObjects)(THIS_ LPDIENUMCREATEDEFFECTOBJECTSCALLBACK,LPVOID,DWORD) PURE;
    STDMETHOD(Escape)(THIS_ LPDIEFFESCAPE) PURE;
    STDMETHOD(Poll)(THIS) PURE;
    STDMETHOD(SendDeviceData)(THIS_ DWORD,LPCDIDEVICEOBJECTDATA,LPDWORD,DWORD) PURE;
};

typedef struct IDirectInputDevice2W *LPDIRECTINPUTDEVICE2W;

#undef INTERFACE
#define INTERFACE IDirectInputDevice2A

DECLARE_INTERFACE_(IDirectInputDevice2A, IDirectInputDeviceA)
{
    /*** IUnknown methods ***/
    STDMETHOD(QueryInterface)(THIS_ REFIID riid, LPVOID * ppvObj) PURE;
    STDMETHOD_(ULONG,AddRef)(THIS) PURE;
    STDMETHOD_(ULONG,Release)(THIS) PURE;

    /*** IDirectInputDeviceA methods ***/
    STDMETHOD(GetCapabilities)(THIS_ LPDIDEVCAPS) PURE;
    STDMETHOD(EnumObjects)(THIS_ LPDIENUMDEVICEOBJECTSCALLBACKA,LPVOID,DWORD) PURE;
    STDMETHOD(GetProperty)(THIS_ REFGUID,LPDIPROPHEADER) PURE;
    STDMETHOD(SetProperty)(THIS_ REFGUID,LPCDIPROPHEADER) PURE;
    STDMETHOD(Acquire)(THIS) PURE;
    STDMETHOD(Unacquire)(THIS) PURE;
    STDMETHOD(GetDeviceState)(THIS_ DWORD,LPVOID) PURE;
    STDMETHOD(GetDeviceData)(THIS_ DWORD,LPDIDEVICEOBJECTDATA,LPDWORD,DWORD) PURE;
    STDMETHOD(SetDataFormat)(THIS_ LPCDIDATAFORMAT) PURE;
    STDMETHOD(SetEventNotification)(THIS_ HANDLE) PURE;
    STDMETHOD(SetCooperativeLevel)(THIS_ HWND,DWORD) PURE;
    STDMETHOD(GetObjectInfo)(THIS_ LPDIDEVICEOBJECTINSTANCEA,DWORD,DWORD) PURE;
    STDMETHOD(GetDeviceInfo)(THIS_ LPDIDEVICEINSTANCEA) PURE;
    STDMETHOD(RunControlPanel)(THIS_ HWND,DWORD) PURE;
    STDMETHOD(Initialize)(THIS_ HINSTANCE,DWORD,REFGUID) PURE;

    /*** IDirectInputDevice2A methods ***/
    STDMETHOD(CreateEffect)(THIS_ REFGUID,LPCDIEFFECT,LPDIRECTINPUTEFFECT *,LPUNKNOWN) PURE;
    STDMETHOD(EnumEffects)(THIS_ LPDIENUMEFFECTSCALLBACKA,LPVOID,DWORD) PURE;
    STDMETHOD(GetEffectInfo)(THIS_ LPDIEFFECTINFOA,REFGUID) PURE;
    STDMETHOD(GetForceFeedbackState)(THIS_ LPDWORD) PURE;
    STDMETHOD(SendForceFeedbackCommand)(THIS_ DWORD) PURE;
    STDMETHOD(EnumCreatedEffectObjects)(THIS_ LPDIENUMCREATEDEFFECTOBJECTSCALLBACK,LPVOID,DWORD) PURE;
    STDMETHOD(Escape)(THIS_ LPDIEFFESCAPE) PURE;
    STDMETHOD(Poll)(THIS) PURE;
    STDMETHOD(SendDeviceData)(THIS_ DWORD,LPCDIDEVICEOBJECTDATA,LPDWORD,DWORD) PURE;
};

typedef struct IDirectInputDevice2A *LPDIRECTINPUTDEVICE2A;

#ifdef UNICODE
#define IID_IDirectInputDevice2 IID_IDirectInputDevice2W
#define IDirectInputDevice2 IDirectInputDevice2W
#define IDirectInputDevice2Vtbl IDirectInputDevice2WVtbl
#else
#define IID_IDirectInputDevice2 IID_IDirectInputDevice2A
#define IDirectInputDevice2 IDirectInputDevice2A
#define IDirectInputDevice2Vtbl IDirectInputDevice2AVtbl
#endif
typedef struct IDirectInputDevice2 *LPDIRECTINPUTDEVICE2;

#if !defined(__cplusplus) || defined(CINTERFACE)
#define IDirectInputDevice2_QueryInterface(p,a,b) (p)->lpVtbl->QueryInterface(p,a,b)
#define IDirectInputDevice2_AddRef(p) (p)->lpVtbl->AddRef(p)
#define IDirectInputDevice2_Release(p) (p)->lpVtbl->Release(p)
#define IDirectInputDevice2_GetCapabilities(p,a) (p)->lpVtbl->GetCapabilities(p,a)
#define IDirectInputDevice2_EnumObjects(p,a,b,c) (p)->lpVtbl->EnumObjects(p,a,b,c)
#define IDirectInputDevice2_GetProperty(p,a,b) (p)->lpVtbl->GetProperty(p,a,b)
#define IDirectInputDevice2_SetProperty(p,a,b) (p)->lpVtbl->SetProperty(p,a,b)
#define IDirectInputDevice2_Acquire(p) (p)->lpVtbl->Acquire(p)
#define IDirectInputDevice2_Unacquire(p) (p)->lpVtbl->Unacquire(p)
#define IDirectInputDevice2_GetDeviceState(p,a,b) (p)->lpVtbl->GetDeviceState(p,a,b)
#define IDirectInputDevice2_GetDeviceData(p,a,b,c,d) (p)->lpVtbl->GetDeviceData(p,a,b,c,d)
#define IDirectInputDevice2_SetDataFormat(p,a) (p)->lpVtbl->SetDataFormat(p,a)
#define IDirectInputDevice2_SetEventNotification(p,a) (p)->lpVtbl->SetEventNotification(p,a)
#define IDirectInputDevice2_SetCooperativeLevel(p,a,b) (p)->lpVtbl->SetCooperativeLevel(p,a,b)
#define IDirectInputDevice2_GetObjectInfo(p,a,b,c) (p)->lpVtbl->GetObjectInfo(p,a,b,c)
#define IDirectInputDevice2_GetDeviceInfo(p,a) (p)->lpVtbl->GetDeviceInfo(p,a)
#define IDirectInputDevice2_RunControlPanel(p,a,b) (p)->lpVtbl->RunControlPanel(p,a,b)
#define IDirectInputDevice2_Initialize(p,a,b,c) (p)->lpVtbl->Initialize(p,a,b,c)
#define IDirectInputDevice2_CreateEffect(p,a,b,c,d) (p)->lpVtbl->CreateEffect(p,a,b,c,d)
#define IDirectInputDevice2_EnumEffects(p,a,b,c) (p)->lpVtbl->EnumEffects(p,a,b,c)
#define IDirectInputDevice2_GetEffectInfo(p,a,b) (p)->lpVtbl->GetEffectInfo(p,a,b)
#define IDirectInputDevice2_GetForceFeedbackState(p,a) (p)->lpVtbl->GetForceFeedbackState(p,a)
#define IDirectInputDevice2_SendForceFeedbackCommand(p,a) (p)->lpVtbl->SendForceFeedbackCommand(p,a)
#define IDirectInputDevice2_EnumCreatedEffectObjects(p,a,b,c) (p)->lpVtbl->EnumCreatedEffectObjects(p,a,b,c)
#define IDirectInputDevice2_Escape(p,a) (p)->lpVtbl->Escape(p,a)
#define IDirectInputDevice2_Poll(p) (p)->lpVtbl->Poll(p)
#define IDirectInputDevice2_SendDeviceData(p,a,b,c,d) (p)->lpVtbl->SendDeviceData(p,a,b,c,d)
#else
#define IDirectInputDevice2_QueryInterface(p,a,b) (p)->QueryInterface(a,b)
#define IDirectInputDevice2_AddRef(p) (p)->AddRef()
#define IDirectInputDevice2_Release(p) (p)->Release()
#define IDirectInputDevice2_GetCapabilities(p,a) (p)->GetCapabilities(a)
#define IDirectInputDevice2_EnumObjects(p,a,b,c) (p)->EnumObjects(a,b,c)
#define IDirectInputDevice2_GetProperty(p,a,b) (p)->GetProperty(a,b)
#define IDirectInputDevice2_SetProperty(p,a,b) (p)->SetProperty(a,b)
#define IDirectInputDevice2_Acquire(p) (p)->Acquire()
#define IDirectInputDevice2_Unacquire(p) (p)->Unacquire()
#define IDirectInputDevice2_GetDeviceState(p,a,b) (p)->GetDeviceState(a,b)
#define IDirectInputDevice2_GetDeviceData(p,a,b,c,d) (p)->GetDeviceData(a,b,c,d)
#define IDirectInputDevice2_SetDataFormat(p,a) (p)->SetDataFormat(a)
#define IDirectInputDevice2_SetEventNotification(p,a) (p)->SetEventNotification(a)
#define IDirectInputDevice2_SetCooperativeLevel(p,a,b) (p)->SetCooperativeLevel(a,b)
#define IDirectInputDevice2_GetObjectInfo(p,a,b,c) (p)->GetObjectInfo(a,b,c)
#define IDirectInputDevice2_GetDeviceInfo(p,a) (p)->GetDeviceInfo(a)
#define IDirectInputDevice2_RunControlPanel(p,a,b) (p)->RunControlPanel(a,b)
#define IDirectInputDevice2_Initialize(p,a,b,c) (p)->Initialize(a,b,c)
#define IDirectInputDevice2_CreateEffect(p,a,b,c,d) (p)->CreateEffect(a,b,c,d)
#define IDirectInputDevice2_EnumEffects(p,a,b,c) (p)->EnumEffects(a,b,c)
#define IDirectInputDevice2_GetEffectInfo(p,a,b) (p)->GetEffectInfo(a,b)
#define IDirectInputDevice2_GetForceFeedbackState(p,a) (p)->GetForceFeedbackState(a)
#define IDirectInputDevice2_SendForceFeedbackCommand(p,a) (p)->SendForceFeedbackCommand(a)
#define IDirectInputDevice2_EnumCreatedEffectObjects(p,a,b,c) (p)->EnumCreatedEffectObjects(a,b,c)
#define IDirectInputDevice2_Escape(p,a) (p)->Escape(a)
#define IDirectInputDevice2_Poll(p) (p)->Poll()
#define IDirectInputDevice2_SendDeviceData(p,a,b,c,d) (p)->SendDeviceData(a,b,c,d)
#endif

#endif /* DIJ_RINGZERO */

#endif /* DIRECTINPUT_VERSION >= 0x0500 */

#if(DIRECTINPUT_VERSION >= 0x0700)
#define DIFEF_DEFAULT               0x00000000
#define DIFEF_INCLUDENONSTANDARD    0x00000001
#define DIFEF_MODIFYIFNEEDED		0x00000010

#ifndef DIJ_RINGZERO

#undef INTERFACE
#define INTERFACE IDirectInputDevice7W

DECLARE_INTERFACE_(IDirectInputDevice7W, IDirectInputDevice2W)
{
    /*** IUnknown methods ***/
    STDMETHOD(QueryInterface)(THIS_ REFIID riid, LPVOID * ppvObj) PURE;
    STDMETHOD_(ULONG,AddRef)(THIS) PURE;
    STDMETHOD_(ULONG,Release)(THIS) PURE;

    /*** IDirectInputDevice2W methods ***/
    STDMETHOD(GetCapabilities)(THIS_ LPDIDEVCAPS) PURE;
    STDMETHOD(EnumObjects)(THIS_ LPDIENUMDEVICEOBJECTSCALLBACKW,LPVOID,DWORD) PURE;
    STDMETHOD(GetProperty)(THIS_ REFGUID,LPDIPROPHEADER) PURE;
    STDMETHOD(SetProperty)(THIS_ REFGUID,LPCDIPROPHEADER) PURE;
    STDMETHOD(Acquire)(THIS) PURE;
    STDMETHOD(Unacquire)(THIS) PURE;
    STDMETHOD(GetDeviceState)(THIS_ DWORD,LPVOID) PURE;
    STDMETHOD(GetDeviceData)(THIS_ DWORD,LPDIDEVICEOBJECTDATA,LPDWORD,DWORD) PURE;
    STDMETHOD(SetDataFormat)(THIS_ LPCDIDATAFORMAT) PURE;
    STDMETHOD(SetEventNotification)(THIS_ HANDLE) PURE;
    STDMETHOD(SetCooperativeLevel)(THIS_ HWND,DWORD) PURE;
    STDMETHOD(GetObjectInfo)(THIS_ LPDIDEVICEOBJECTINSTANCEW,DWORD,DWORD) PURE;
    STDMETHOD(GetDeviceInfo)(THIS_ LPDIDEVICEINSTANCEW) PURE;
    STDMETHOD(RunControlPanel)(THIS_ HWND,DWORD) PURE;
    STDMETHOD(Initialize)(THIS_ HINSTANCE,DWORD,REFGUID) PURE;
    STDMETHOD(CreateEffect)(THIS_ REFGUID,LPCDIEFFECT,LPDIRECTINPUTEFFECT *,LPUNKNOWN) PURE;
    STDMETHOD(EnumEffects)(THIS_ LPDIENUMEFFECTSCALLBACKW,LPVOID,DWORD) PURE;
    STDMETHOD(GetEffectInfo)(THIS_ LPDIEFFECTINFOW,REFGUID) PURE;
    STDMETHOD(GetForceFeedbackState)(THIS_ LPDWORD) PURE;
    STDMETHOD(SendForceFeedbackCommand)(THIS_ DWORD) PURE;
    STDMETHOD(EnumCreatedEffectObjects)(THIS_ LPDIENUMCREATEDEFFECTOBJECTSCALLBACK,LPVOID,DWORD) PURE;
    STDMETHOD(Escape)(THIS_ LPDIEFFESCAPE) PURE;
    STDMETHOD(Poll)(THIS) PURE;
    STDMETHOD(SendDeviceData)(THIS_ DWORD,LPCDIDEVICEOBJECTDATA,LPDWORD,DWORD) PURE;

    /*** IDirectInputDevice7W methods ***/
    STDMETHOD(EnumEffectsInFile)(THIS_ LPCWSTR,LPDIENUMEFFECTSINFILECALLBACK,LPVOID,DWORD) PURE;
    STDMETHOD(WriteEffectToFile)(THIS_ LPCWSTR,DWORD,LPDIFILEEFFECT,DWORD) PURE;
};

typedef struct IDirectInputDevice7W *LPDIRECTINPUTDEVICE7W;

#undef INTERFACE
#define INTERFACE IDirectInputDevice7A

DECLARE_INTERFACE_(IDirectInputDevice7A, IDirectInputDevice2A)
{
    /*** IUnknown methods ***/
    STDMETHOD(QueryInterface)(THIS_ REFIID riid, LPVOID * ppvObj) PURE;
    STDMETHOD_(ULONG,AddRef)(THIS) PURE;
    STDMETHOD_(ULONG,Release)(THIS) PURE;

    /*** IDirectInputDevice2A methods ***/
    STDMETHOD(GetCapabilities)(THIS_ LPDIDEVCAPS) PURE;
    STDMETHOD(EnumObjects)(THIS_ LPDIENUMDEVICEOBJECTSCALLBACKA,LPVOID,DWORD) PURE;
    STDMETHOD(GetProperty)(THIS_ REFGUID,LPDIPROPHEADER) PURE;
    STDMETHOD(SetProperty)(THIS_ REFGUID,LPCDIPROPHEADER) PURE;
    STDMETHOD(Acquire)(THIS) PURE;
    STDMETHOD(Unacquire)(THIS) PURE;
    STDMETHOD(GetDeviceState)(THIS_ DWORD,LPVOID) PURE;
    STDMETHOD(GetDeviceData)(THIS_ DWORD,LPDIDEVICEOBJECTDATA,LPDWORD,DWORD) PURE;
    STDMETHOD(SetDataFormat)(THIS_ LPCDIDATAFORMAT) PURE;
    STDMETHOD(SetEventNotification)(THIS_ HANDLE) PURE;
    STDMETHOD(SetCooperativeLevel)(THIS_ HWND,DWORD) PURE;
    STDMETHOD(GetObjectInfo)(THIS_ LPDIDEVICEOBJECTINSTANCEA,DWORD,DWORD) PURE;
    STDMETHOD(GetDeviceInfo)(THIS_ LPDIDEVICEINSTANCEA) PURE;
    STDMETHOD(RunControlPanel)(THIS_ HWND,DWORD) PURE;
    STDMETHOD(Initialize)(THIS_ HINSTANCE,DWORD,REFGUID) PURE;
    STDMETHOD(CreateEffect)(THIS_ REFGUID,LPCDIEFFECT,LPDIRECTINPUTEFFECT *,LPUNKNOWN) PURE;
    STDMETHOD(EnumEffects)(THIS_ LPDIENUMEFFECTSCALLBACKA,LPVOID,DWORD) PURE;
    STDMETHOD(GetEffectInfo)(THIS_ LPDIEFFECTINFOA,REFGUID) PURE;
    STDMETHOD(GetForceFeedbackState)(THIS_ LPDWORD) PURE;
    STDMETHOD(SendForceFeedbackCommand)(THIS_ DWORD) PURE;
    STDMETHOD(EnumCreatedEffectObjects)(THIS_ LPDIENUMCREATEDEFFECTOBJECTSCALLBACK,LPVOID,DWORD) PURE;
    STDMETHOD(Escape)(THIS_ LPDIEFFESCAPE) PURE;
    STDMETHOD(Poll)(THIS) PURE;
    STDMETHOD(SendDeviceData)(THIS_ DWORD,LPCDIDEVICEOBJECTDATA,LPDWORD,DWORD) PURE;

    /*** IDirectInputDevice7A methods ***/
    STDMETHOD(EnumEffectsInFile)(THIS_ LPCSTR,LPDIENUMEFFECTSINFILECALLBACK,LPVOID,DWORD) PURE;
    STDMETHOD(WriteEffectToFile)(THIS_ LPCSTR,DWORD,LPDIFILEEFFECT,DWORD) PURE;
};

typedef struct IDirectInputDevice7A *LPDIRECTINPUTDEVICE7A;

#ifdef UNICODE
#define IID_IDirectInputDevice7 IID_IDirectInputDevice7W
#define IDirectInputDevice7 IDirectInputDevice7W
#define IDirectInputDevice7Vtbl IDirectInputDevice7WVtbl
#else
#define IID_IDirectInputDevice7 IID_IDirectInputDevice7A
#define IDirectInputDevice7 IDirectInputDevice7A
#define IDirectInputDevice7Vtbl IDirectInputDevice7AVtbl
#endif
typedef struct IDirectInputDevice7 *LPDIRECTINPUTDEVICE7;

#if !defined(__cplusplus) || defined(CINTERFACE)
#define IDirectInputDevice7_QueryInterface(p,a,b) (p)->lpVtbl->QueryInterface(p,a,b)
#define IDirectInputDevice7_AddRef(p) (p)->lpVtbl->AddRef(p)
#define IDirectInputDevice7_Release(p) (p)->lpVtbl->Release(p)
#define IDirectInputDevice7_GetCapabilities(p,a) (p)->lpVtbl->GetCapabilities(p,a)
#define IDirectInputDevice7_EnumObjects(p,a,b,c) (p)->lpVtbl->EnumObjects(p,a,b,c)
#define IDirectInputDevice7_GetProperty(p,a,b) (p)->lpVtbl->GetProperty(p,a,b)
#define IDirectInputDevice7_SetProperty(p,a,b) (p)->lpVtbl->SetProperty(p,a,b)
#define IDirectInputDevice7_Acquire(p) (p)->lpVtbl->Acquire(p)
#define IDirectInputDevice7_Unacquire(p) (p)->lpVtbl->Unacquire(p)
#define IDirectInputDevice7_GetDeviceState(p,a,b) (p)->lpVtbl->GetDeviceState(p,a,b)
#define IDirectInputDevice7_GetDeviceData(p,a,b,c,d) (p)->lpVtbl->GetDeviceData(p,a,b,c,d)
#define IDirectInputDevice7_SetDataFormat(p,a) (p)->lpVtbl->SetDataFormat(p,a)
#define IDirectInputDevice7_SetEventNotification(p,a) (p)->lpVtbl->SetEventNotification(p,a)
#define IDirectInputDevice7_SetCooperativeLevel(p,a,b) (p)->lpVtbl->SetCooperativeLevel(p,a,b)
#define IDirectInputDevice7_GetObjectInfo(p,a,b,c) (p)->lpVtbl->GetObjectInfo(p,a,b,c)
#define IDirectInputDevice7_GetDeviceInfo(p,a) (p)->lpVtbl->GetDeviceInfo(p,a)
#define IDirectInputDevice7_RunControlPanel(p,a,b) (p)->lpVtbl->RunControlPanel(p,a,b)
#define IDirectInputDevice7_Initialize(p,a,b,c) (p)->lpVtbl->Initialize(p,a,b,c)
#define IDirectInputDevice7_CreateEffect(p,a,b,c,d) (p)->lpVtbl->CreateEffect(p,a,b,c,d)
#define IDirectInputDevice7_EnumEffects(p,a,b,c) (p)->lpVtbl->EnumEffects(p,a,b,c)
#define IDirectInputDevice7_GetEffectInfo(p,a,b) (p)->lpVtbl->GetEffectInfo(p,a,b)
#define IDirectInputDevice7_GetForceFeedbackState(p,a) (p)->lpVtbl->GetForceFeedbackState(p,a)
#define IDirectInputDevice7_SendForceFeedbackCommand(p,a) (p)->lpVtbl->SendForceFeedbackCommand(p,a)
#define IDirectInputDevice7_EnumCreatedEffectObjects(p,a,b,c) (p)->lpVtbl->EnumCreatedEffectObjects(p,a,b,c)
#define IDirectInputDevice7_Escape(p,a) (p)->lpVtbl->Escape(p,a)
#define IDirectInputDevice7_Poll(p) (p)->lpVtbl->Poll(p)
#define IDirectInputDevice7_SendDeviceData(p,a,b,c,d) (p)->lpVtbl->SendDeviceData(p,a,b,c,d)
#define IDirectInputDevice7_EnumEffectsInFile(p,a,b,c,d) (p)->lpVtbl->EnumEffectsInFile(p,a,b,c,d)
#define IDirectInputDevice7_WriteEffectToFile(p,a,b,c,d) (p)->lpVtbl->WriteEffectToFile(p,a,b,c,d)
#else
#define IDirectInputDevice7_QueryInterface(p,a,b) (p)->QueryInterface(a,b)
#define IDirectInputDevice7_AddRef(p) (p)->AddRef()
#define IDirectInputDevice7_Release(p) (p)->Release()
#define IDirectInputDevice7_GetCapabilities(p,a) (p)->GetCapabilities(a)
#define IDirectInputDevice7_EnumObjects(p,a,b,c) (p)->EnumObjects(a,b,c)
#define IDirectInputDevice7_GetProperty(p,a,b) (p)->GetProperty(a,b)
#define IDirectInputDevice7_SetProperty(p,a,b) (p)->SetProperty(a,b)
#define IDirectInputDevice7_Acquire(p) (p)->Acquire()
#define IDirectInputDevice7_Unacquire(p) (p)->Unacquire()
#define IDirectInputDevice7_GetDeviceState(p,a,b) (p)->GetDeviceState(a,b)
#define IDirectInputDevice7_GetDeviceData(p,a,b,c,d) (p)->GetDeviceData(a,b,c,d)
#define IDirectInputDevice7_SetDataFormat(p,a) (p)->SetDataFormat(a)
#define IDirectInputDevice7_SetEventNotification(p,a) (p)->SetEventNotification(a)
#define IDirectInputDevice7_SetCooperativeLevel(p,a,b) (p)->SetCooperativeLevel(a,b)
#define IDirectInputDevice7_GetObjectInfo(p,a,b,c) (p)->GetObjectInfo(a,b,c)
#define IDirectInputDevice7_GetDeviceInfo(p,a) (p)->GetDeviceInfo(a)
#define IDirectInputDevice7_RunControlPanel(p,a,b) (p)->RunControlPanel(a,b)
#define IDirectInputDevice7_Initialize(p,a,b,c) (p)->Initialize(a,b,c)
#define IDirectInputDevice7_CreateEffect(p,a,b,c,d) (p)->CreateEffect(a,b,c,d)
#define IDirectInputDevice7_EnumEffects(p,a,b,c) (p)->EnumEffects(a,b,c)
#define IDirectInputDevice7_GetEffectInfo(p,a,b) (p)->GetEffectInfo(a,b)
#define IDirectInputDevice7_GetForceFeedbackState(p,a) (p)->GetForceFeedbackState(a)
#define IDirectInputDevice7_SendForceFeedbackCommand(p,a) (p)->SendForceFeedbackCommand(a)
#define IDirectInputDevice7_EnumCreatedEffectObjects(p,a,b,c) (p)->EnumCreatedEffectObjects(a,b,c)
#define IDirectInputDevice7_Escape(p,a) (p)->Escape(a)
#define IDirectInputDevice7_Poll(p) (p)->Poll()
#define IDirectInputDevice7_SendDeviceData(p,a,b,c,d) (p)->SendDeviceData(a,b,c,d)
#define IDirectInputDevice7_EnumEffectsInFile(p,a,b,c,d) (p)->EnumEffectsInFile(a,b,c,d)
#define IDirectInputDevice7_WriteEffectToFile(p,a,b,c,d) (p)->WriteEffectToFile(a,b,c,d)
#endif

#endif /* DIJ_RINGZERO */

#endif /* DIRECTINPUT_VERSION >= 0x0700 */

/****************************************************************************
 *
 *      Mouse
 *
 ****************************************************************************/

#ifndef DIJ_RINGZERO

typedef struct _DIMOUSESTATE {
    LONG    lX;
    LONG    lY;
    LONG    lZ;
    BYTE    rgbButtons[4];
} DIMOUSESTATE, *LPDIMOUSESTATE;

#if DIRECTINPUT_VERSION >= 0x0700
typedef struct _DIMOUSESTATE2 {
    LONG    lX;
    LONG    lY;
    LONG    lZ;
    BYTE    rgbButtons[8];
} DIMOUSESTATE2, *LPDIMOUSESTATE2;
#endif


#define DIMOFS_X        FIELD_OFFSET(DIMOUSESTATE, lX)
#define DIMOFS_Y        FIELD_OFFSET(DIMOUSESTATE, lY)
#define DIMOFS_Z        FIELD_OFFSET(DIMOUSESTATE, lZ)
#define DIMOFS_BUTTON0 (FIELD_OFFSET(DIMOUSESTATE, rgbButtons) + 0)
#define DIMOFS_BUTTON1 (FIELD_OFFSET(DIMOUSESTATE, rgbButtons) + 1)
#define DIMOFS_BUTTON2 (FIELD_OFFSET(DIMOUSESTATE, rgbButtons) + 2)
#define DIMOFS_BUTTON3 (FIELD_OFFSET(DIMOUSESTATE, rgbButtons) + 3)
#if (DIRECTINPUT_VERSION >= 0x0700)
#define DIMOFS_BUTTON4 (FIELD_OFFSET(DIMOUSESTATE2, rgbButtons) + 4)
#define DIMOFS_BUTTON5 (FIELD_OFFSET(DIMOUSESTATE2, rgbButtons) + 5)
#define DIMOFS_BUTTON6 (FIELD_OFFSET(DIMOUSESTATE2, rgbButtons) + 6)
#define DIMOFS_BUTTON7 (FIELD_OFFSET(DIMOUSESTATE2, rgbButtons) + 7)
#endif
#endif /* DIJ_RINGZERO */

/****************************************************************************
 *
 *      Keyboard
 *
 ****************************************************************************/

#ifndef DIJ_RINGZERO

/****************************************************************************
 *
 *      DirectInput keyboard scan codes
 *
 ****************************************************************************/

#define DIK_ESCAPE          0x01
#define DIK_1               0x02
#define DIK_2               0x03
#define DIK_3               0x04
#define DIK_4               0x05
#define DIK_5               0x06
#define DIK_6               0x07
#define DIK_7               0x08
#define DIK_8               0x09
#define DIK_9               0x0A
#define DIK_0               0x0B
#define DIK_MINUS           0x0C    /* - on main keyboard */
#define DIK_EQUALS          0x0D
#define DIK_BACK            0x0E    /* backspace */
#define DIK_TAB             0x0F
#define DIK_Q               0x10
#define DIK_W               0x11
#define DIK_E               0x12
#define DIK_R               0x13
#define DIK_T               0x14
#define DIK_Y               0x15
#define DIK_U               0x16
#define DIK_I               0x17
#define DIK_O               0x18
#define DIK_P               0x19
#define DIK_LBRACKET        0x1A
#define DIK_RBRACKET        0x1B
#define DIK_RETURN          0x1C    /* Enter on main keyboard */
#define DIK_LCONTROL        0x1D
#define DIK_A               0x1E
#define DIK_S               0x1F
#define DIK_D               0x20
#define DIK_F               0x21
#define DIK_G               0x22
#define DIK_H               0x23
#define DIK_J               0x24
#define DIK_K               0x25
#define DIK_L               0x26
#define DIK_SEMICOLON       0x27
#define DIK_APOSTROPHE      0x28
#define DIK_GRAVE           0x29    /* accent grave */
#define DIK_LSHIFT          0x2A
#define DIK_BACKSLASH       0x2B
#define DIK_Z               0x2C
#define DIK_X               0x2D
#define DIK_C               0x2E
#define DIK_V               0x2F
#define DIK_B               0x30
#define DIK_N               0x31
#define DIK_M               0x32
#define DIK_COMMA           0x33
#define DIK_PERIOD          0x34    /* . on main keyboard */
#define DIK_SLASH           0x35    /* / on main keyboard */
#define DIK_RSHIFT          0x36
#define DIK_MULTIPLY        0x37    /* * on numeric keypad */
#define DIK_LMENU           0x38    /* left Alt */
#define DIK_SPACE           0x39
#define DIK_CAPITAL         0x3A
#define DIK_F1              0x3B
#define DIK_F2              0x3C
#define DIK_F3              0x3D
#define DIK_F4              0x3E
#define DIK_F5              0x3F
#define DIK_F6              0x40
#define DIK_F7              0x41
#define DIK_F8              0x42
#define DIK_F9              0x43
#define DIK_F10             0x44
#define DIK_NUMLOCK         0x45
#define DIK_SCROLL          0x46    /* Scroll Lock */
#define DIK_NUMPAD7         0x47
#define DIK_NUMPAD8         0x48
#define DIK_NUMPAD9         0x49
#define DIK_SUBTRACT        0x4A    /* - on numeric keypad */
#define DIK_NUMPAD4         0x4B
#define DIK_NUMPAD5         0x4C
#define DIK_NUMPAD6         0x4D
#define DIK_ADD             0x4E    /* + on numeric keypad */
#define DIK_NUMPAD1         0x4F
#define DIK_NUMPAD2         0x50
#define DIK_NUMPAD3         0x51
#define DIK_NUMPAD0         0x52
#define DIK_DECIMAL         0x53    /* . on numeric keypad */
#define DIK_OEM_102         0x56    /* < > | on UK/Germany keyboards */
#define DIK_F11             0x57
#define DIK_F12             0x58

#define DIK_F13             0x64    /*                     (NEC PC98) */
#define DIK_F14             0x65    /*                     (NEC PC98) */
#define DIK_F15             0x66    /*                     (NEC PC98) */

#define DIK_KANA            0x70    /* (Japanese keyboard)            */
#define DIK_ABNT_C1         0x73    /* / ? on Portugese (Brazilian) keyboards */
#define DIK_CONVERT         0x79    /* (Japanese keyboard)            */
#define DIK_NOCONVERT       0x7B    /* (Japanese keyboard)            */
#define DIK_YEN             0x7D    /* (Japanese keyboard)            */
#define DIK_ABNT_C2         0x7E    /* Numpad . on Portugese (Brazilian) keyboards */
#define DIK_NUMPADEQUALS    0x8D    /* = on numeric keypad (NEC PC98) */
#define DIK_PREVTRACK       0x90    /* Previous Track (DIK_CIRCUMFLEX on Japanese keyboard) */
#define DIK_AT              0x91    /*                     (NEC PC98) */
#define DIK_COLON           0x92    /*                     (NEC PC98) */
#define DIK_UNDERLINE       0x93    /*                     (NEC PC98) */
#define DIK_KANJI           0x94    /* (Japanese keyboard)            */
#define DIK_STOP            0x95    /*                     (NEC PC98) */
#define DIK_AX              0x96    /*                     (Japan AX) */
#define DIK_UNLABELED       0x97    /*                        (J3100) */
#define DIK_NEXTTRACK       0x99    /* Next Track */
#define DIK_NUMPADENTER     0x9C    /* Enter on numeric keypad */
#define DIK_RCONTROL        0x9D
#define DIK_MUTE            0xA0    /* Mute */
#define DIK_CALCULATOR      0xA1    /* Calculator */
#define DIK_PLAYPAUSE       0xA2    /* Play / Pause */
#define DIK_MEDIASTOP       0xA4    /* Media Stop */
#define DIK_VOLUMEDOWN      0xAE    /* Volume - */
#define DIK_VOLUMEUP        0xB0    /* Volume + */
#define DIK_WEBHOME         0xB2    /* Web home */
#define DIK_NUMPADCOMMA     0xB3    /* , on numeric keypad (NEC PC98) */
#define DIK_DIVIDE          0xB5    /* / on numeric keypad */
#define DIK_SYSRQ           0xB7
#define DIK_RMENU           0xB8    /* right Alt */
#define DIK_PAUSE           0xC5    /* Pause */
#define DIK_HOME            0xC7    /* Home on arrow keypad */
#define DIK_UP              0xC8    /* UpArrow on arrow keypad */
#define DIK_PRIOR           0xC9    /* PgUp on arrow keypad */
#define DIK_LEFT            0xCB    /* LeftArrow on arrow keypad */
#define DIK_RIGHT           0xCD    /* RightArrow on arrow keypad */
#define DIK_END             0xCF    /* End on arrow keypad */
#define DIK_DOWN            0xD0    /* DownArrow on arrow keypad */
#define DIK_NEXT            0xD1    /* PgDn on arrow keypad */
#define DIK_INSERT          0xD2    /* Insert on arrow keypad */
#define DIK_DELETE          0xD3    /* Delete on arrow keypad */
#define DIK_LWIN            0xDB    /* Left Windows key */
#define DIK_RWIN            0xDC    /* Right Windows key */
#define DIK_APPS            0xDD    /* AppMenu key */
#define DIK_POWER           0xDE    /* System Power */
#define DIK_SLEEP           0xDF    /* System Sleep */
#define DIK_WAKE            0xE3    /* System Wake */
#define DIK_WEBSEARCH       0xE5    /* Web Search */
#define DIK_WEBFAVORITES    0xE6    /* Web Favorites */
#define DIK_WEBREFRESH      0xE7    /* Web Refresh */
#define DIK_WEBSTOP         0xE8    /* Web Stop */
#define DIK_WEBFORWARD      0xE9    /* Web Forward */
#define DIK_WEBBACK         0xEA    /* Web Back */
#define DIK_MYCOMPUTER      0xEB    /* My Computer */
#define DIK_MAIL            0xEC    /* Mail */
#define DIK_MEDIASELECT     0xED    /* Media Select */

/*
 *  Alternate names for keys, to facilitate transition from DOS.
 */
#define DIK_BACKSPACE       DIK_BACK            /* backspace */
#define DIK_NUMPADSTAR      DIK_MULTIPLY        /* * on numeric keypad */
#define DIK_LALT            DIK_LMENU           /* left Alt */
#define DIK_CAPSLOCK        DIK_CAPITAL         /* CapsLock */
#define DIK_NUMPADMINUS     DIK_SUBTRACT        /* - on numeric keypad */
#define DIK_NUMPADPLUS      DIK_ADD             /* + on numeric keypad */
#define DIK_NUMPADPERIOD    DIK_DECIMAL         /* . on numeric keypad */
#define DIK_NUMPADSLASH     DIK_DIVIDE          /* / on numeric keypad */
#define DIK_RALT            DIK_RMENU           /* right Alt */
#define DIK_UPARROW         DIK_UP              /* UpArrow on arrow keypad */
#define DIK_PGUP            DIK_PRIOR           /* PgUp on arrow keypad */
#define DIK_LEFTARROW       DIK_LEFT            /* LeftArrow on arrow keypad */
#define DIK_RIGHTARROW      DIK_RIGHT           /* RightArrow on arrow keypad */
#define DIK_DOWNARROW       DIK_DOWN            /* DownArrow on arrow keypad */
#define DIK_PGDN            DIK_NEXT            /* PgDn on arrow keypad */

/*
 *  Alternate names for keys originally not used on US keyboards.
 */
#define DIK_CIRCUMFLEX      DIK_PREVTRACK       /* Japanese keyboard */

#endif /* DIJ_RINGZERO */

/****************************************************************************
 *
 *      Joystick
 *
 ****************************************************************************/

#ifndef DIJ_RINGZERO

typedef struct DIJOYSTATE {
    LONG    lX;                     /* x-axis position              */
    LONG    lY;                     /* y-axis position              */
    LONG    lZ;                     /* z-axis position              */
    LONG    lRx;                    /* x-axis rotation              */
    LONG    lRy;                    /* y-axis rotation              */
    LONG    lRz;                    /* z-axis rotation              */
    LONG    rglSlider[2];           /* extra axes positions         */
    DWORD   rgdwPOV[4];             /* POV directions               */
    BYTE    rgbButtons[32];         /* 32 buttons                   */
} DIJOYSTATE, *LPDIJOYSTATE;

typedef struct DIJOYSTATE2 {
    LONG    lX;                     /* x-axis position              */
    LONG    lY;                     /* y-axis position              */
    LONG    lZ;                     /* z-axis position              */
    LONG    lRx;                    /* x-axis rotation              */
    LONG    lRy;                    /* y-axis rotation              */
    LONG    lRz;                    /* z-axis rotation              */
    LONG    rglSlider[2];           /* extra axes positions         */
    DWORD   rgdwPOV[4];             /* POV directions               */
    BYTE    rgbButtons[128];        /* 128 buttons                  */
    LONG    lVX;                    /* x-axis velocity              */
    LONG    lVY;                    /* y-axis velocity              */
    LONG    lVZ;                    /* z-axis velocity              */
    LONG    lVRx;                   /* x-axis angular velocity      */
    LONG    lVRy;                   /* y-axis angular velocity      */
    LONG    lVRz;                   /* z-axis angular velocity      */
    LONG    rglVSlider[2];          /* extra axes velocities        */
    LONG    lAX;                    /* x-axis acceleration          */
    LONG    lAY;                    /* y-axis acceleration          */
    LONG    lAZ;                    /* z-axis acceleration          */
    LONG    lARx;                   /* x-axis angular acceleration  */
    LONG    lARy;                   /* y-axis angular acceleration  */
    LONG    lARz;                   /* z-axis angular acceleration  */
    LONG    rglASlider[2];          /* extra axes accelerations     */
    LONG    lFX;                    /* x-axis force                 */
    LONG    lFY;                    /* y-axis force                 */
    LONG    lFZ;                    /* z-axis force                 */
    LONG    lFRx;                   /* x-axis torque                */
    LONG    lFRy;                   /* y-axis torque                */
    LONG    lFRz;                   /* z-axis torque                */
    LONG    rglFSlider[2];          /* extra axes forces            */
} DIJOYSTATE2, *LPDIJOYSTATE2;

#define DIJOFS_X            FIELD_OFFSET(DIJOYSTATE, lX)
#define DIJOFS_Y            FIELD_OFFSET(DIJOYSTATE, lY)
#define DIJOFS_Z            FIELD_OFFSET(DIJOYSTATE, lZ)
#define DIJOFS_RX           FIELD_OFFSET(DIJOYSTATE, lRx)
#define DIJOFS_RY           FIELD_OFFSET(DIJOYSTATE, lRy)
#define DIJOFS_RZ           FIELD_OFFSET(DIJOYSTATE, lRz)
#define DIJOFS_SLIDER(n)   (FIELD_OFFSET(DIJOYSTATE, rglSlider) + \
                                                        (n) * sizeof(LONG))
#define DIJOFS_POV(n)      (FIELD_OFFSET(DIJOYSTATE, rgdwPOV) + \
                                                        (n) * sizeof(DWORD))
#define DIJOFS_BUTTON(n)   (FIELD_OFFSET(DIJOYSTATE, rgbButtons) + (n))
#define DIJOFS_BUTTON0      DIJOFS_BUTTON(0)
#define DIJOFS_BUTTON1      DIJOFS_BUTTON(1)
#define DIJOFS_BUTTON2      DIJOFS_BUTTON(2)
#define DIJOFS_BUTTON3      DIJOFS_BUTTON(3)
#define DIJOFS_BUTTON4      DIJOFS_BUTTON(4)
#define DIJOFS_BUTTON5      DIJOFS_BUTTON(5)
#define DIJOFS_BUTTON6      DIJOFS_BUTTON(6)
#define DIJOFS_BUTTON7      DIJOFS_BUTTON(7)
#define DIJOFS_BUTTON8      DIJOFS_BUTTON(8)
#define DIJOFS_BUTTON9      DIJOFS_BUTTON(9)
#define DIJOFS_BUTTON10     DIJOFS_BUTTON(10)
#define DIJOFS_BUTTON11     DIJOFS_BUTTON(11)
#define DIJOFS_BUTTON12     DIJOFS_BUTTON(12)
#define DIJOFS_BUTTON13     DIJOFS_BUTTON(13)
#define DIJOFS_BUTTON14     DIJOFS_BUTTON(14)
#define DIJOFS_BUTTON15     DIJOFS_BUTTON(15)
#define DIJOFS_BUTTON16     DIJOFS_BUTTON(16)
#define DIJOFS_BUTTON17     DIJOFS_BUTTON(17)
#define DIJOFS_BUTTON18     DIJOFS_BUTTON(18)
#define DIJOFS_BUTTON19     DIJOFS_BUTTON(19)
#define DIJOFS_BUTTON20     DIJOFS_BUTTON(20)
#define DIJOFS_BUTTON21     DIJOFS_BUTTON(21)
#define DIJOFS_BUTTON22     DIJOFS_BUTTON(22)
#define DIJOFS_BUTTON23     DIJOFS_BUTTON(23)
#define DIJOFS_BUTTON24     DIJOFS_BUTTON(24)
#define DIJOFS_BUTTON25     DIJOFS_BUTTON(25)
#define DIJOFS_BUTTON26     DIJOFS_BUTTON(26)
#define DIJOFS_BUTTON27     DIJOFS_BUTTON(27)
#define DIJOFS_BUTTON28     DIJOFS_BUTTON(28)
#define DIJOFS_BUTTON29     DIJOFS_BUTTON(29)
#define DIJOFS_BUTTON30     DIJOFS_BUTTON(30)
#define DIJOFS_BUTTON31     DIJOFS_BUTTON(31)


#endif /* DIJ_RINGZERO */

/****************************************************************************
 *
 *  IDirectInput
 *
 ****************************************************************************/

#ifndef DIJ_RINGZERO

#define DIENUM_STOP             0
#define DIENUM_CONTINUE         1

typedef BOOL (FAR PASCAL * LPDIENUMDEVICESCALLBACKA)(LPCDIDEVICEINSTANCEA, LPVOID);
typedef BOOL (FAR PASCAL * LPDIENUMDEVICESCALLBACKW)(LPCDIDEVICEINSTANCEW, LPVOID);
#ifdef UNICODE
#define LPDIENUMDEVICESCALLBACK  LPDIENUMDEVICESCALLBACKW
#else
#define LPDIENUMDEVICESCALLBACK  LPDIENUMDEVICESCALLBACKA
#endif // !UNICODE

#define DIEDFL_ALLDEVICES       0x00000000
#define DIEDFL_ATTACHEDONLY     0x00000001
#if(DIRECTINPUT_VERSION >= 0x0500)
#define DIEDFL_FORCEFEEDBACK    0x00000100
#endif /* DIRECTINPUT_VERSION >= 0x0500 */
#if(DIRECTINPUT_VERSION >= 0x050a)
#define DIEDFL_INCLUDEALIASES   0x00010000
#define DIEDFL_INCLUDEPHANTOMS  0x00020000
#endif /* DIRECTINPUT_VERSION >= 0x050a */

#undef INTERFACE
#define INTERFACE IDirectInputW

DECLARE_INTERFACE_(IDirectInputW, IUnknown)
{
    /*** IUnknown methods ***/
    STDMETHOD(QueryInterface)(THIS_ REFIID riid, LPVOID * ppvObj) PURE;
    STDMETHOD_(ULONG,AddRef)(THIS) PURE;
    STDMETHOD_(ULONG,Release)(THIS) PURE;

    /*** IDirectInputW methods ***/
    STDMETHOD(CreateDevice)(THIS_ REFGUID,LPDIRECTINPUTDEVICEW *,LPUNKNOWN) PURE;
    STDMETHOD(EnumDevices)(THIS_ DWORD,LPDIENUMDEVICESCALLBACKW,LPVOID,DWORD) PURE;
    STDMETHOD(GetDeviceStatus)(THIS_ REFGUID) PURE;
    STDMETHOD(RunControlPanel)(THIS_ HWND,DWORD) PURE;
    STDMETHOD(Initialize)(THIS_ HINSTANCE,DWORD) PURE;
};

typedef struct IDirectInputW *LPDIRECTINPUTW;

#undef INTERFACE
#define INTERFACE IDirectInputA

DECLARE_INTERFACE_(IDirectInputA, IUnknown)
{
    /*** IUnknown methods ***/
    STDMETHOD(QueryInterface)(THIS_ REFIID riid, LPVOID * ppvObj) PURE;
    STDMETHOD_(ULONG,AddRef)(THIS) PURE;
    STDMETHOD_(ULONG,Release)(THIS) PURE;

    /*** IDirectInputA methods ***/
    STDMETHOD(CreateDevice)(THIS_ REFGUID,LPDIRECTINPUTDEVICEA *,LPUNKNOWN) PURE;
    STDMETHOD(EnumDevices)(THIS_ DWORD,LPDIENUMDEVICESCALLBACKA,LPVOID,DWORD) PURE;
    STDMETHOD(GetDeviceStatus)(THIS_ REFGUID) PURE;
    STDMETHOD(RunControlPanel)(THIS_ HWND,DWORD) PURE;
    STDMETHOD(Initialize)(THIS_ HINSTANCE,DWORD) PURE;
};

typedef struct IDirectInputA *LPDIRECTINPUTA;

#ifdef UNICODE
#define IID_IDirectInput IID_IDirectInputW
#define IDirectInput IDirectInputW
#define IDirectInputVtbl IDirectInputWVtbl
#else
#define IID_IDirectInput IID_IDirectInputA
#define IDirectInput IDirectInputA
#define IDirectInputVtbl IDirectInputAVtbl
#endif
typedef struct IDirectInput *LPDIRECTINPUT;

#if !defined(__cplusplus) || defined(CINTERFACE)
#define IDirectInput_QueryInterface(p,a,b) (p)->lpVtbl->QueryInterface(p,a,b)
#define IDirectInput_AddRef(p) (p)->lpVtbl->AddRef(p)
#define IDirectInput_Release(p) (p)->lpVtbl->Release(p)
#define IDirectInput_CreateDevice(p,a,b,c) (p)->lpVtbl->CreateDevice(p,a,b,c)
#define IDirectInput_EnumDevices(p,a,b,c,d) (p)->lpVtbl->EnumDevices(p,a,b,c,d)
#define IDirectInput_GetDeviceStatus(p,a) (p)->lpVtbl->GetDeviceStatus(p,a)
#define IDirectInput_RunControlPanel(p,a,b) (p)->lpVtbl->RunControlPanel(p,a,b)
#define IDirectInput_Initialize(p,a,b) (p)->lpVtbl->Initialize(p,a,b)
#else
#define IDirectInput_QueryInterface(p,a,b) (p)->QueryInterface(a,b)
#define IDirectInput_AddRef(p) (p)->AddRef()
#define IDirectInput_Release(p) (p)->Release()
#define IDirectInput_CreateDevice(p,a,b,c) (p)->CreateDevice(a,b,c)
#define IDirectInput_EnumDevices(p,a,b,c,d) (p)->EnumDevices(a,b,c,d)
#define IDirectInput_GetDeviceStatus(p,a) (p)->GetDeviceStatus(a)
#define IDirectInput_RunControlPanel(p,a,b) (p)->RunControlPanel(a,b)
#define IDirectInput_Initialize(p,a,b) (p)->Initialize(a,b)
#endif

#undef INTERFACE
#define INTERFACE IDirectInput2W

DECLARE_INTERFACE_(IDirectInput2W, IDirectInputW)
{
    /*** IUnknown methods ***/
    STDMETHOD(QueryInterface)(THIS_ REFIID riid, LPVOID * ppvObj) PURE;
    STDMETHOD_(ULONG,AddRef)(THIS) PURE;
    STDMETHOD_(ULONG,Release)(THIS) PURE;

    /*** IDirectInputW methods ***/
    STDMETHOD(CreateDevice)(THIS_ REFGUID,LPDIRECTINPUTDEVICEW *,LPUNKNOWN) PURE;
    STDMETHOD(EnumDevices)(THIS_ DWORD,LPDIENUMDEVICESCALLBACKW,LPVOID,DWORD) PURE;
    STDMETHOD(GetDeviceStatus)(THIS_ REFGUID) PURE;
    STDMETHOD(RunControlPanel)(THIS_ HWND,DWORD) PURE;
    STDMETHOD(Initialize)(THIS_ HINSTANCE,DWORD) PURE;

    /*** IDirectInput2W methods ***/
    STDMETHOD(FindDevice)(THIS_ REFGUID,LPCWSTR,LPGUID) PURE;
};

typedef struct IDirectInput2W *LPDIRECTINPUT2W;

#undef INTERFACE
#define INTERFACE IDirectInput2A

DECLARE_INTERFACE_(IDirectInput2A, IDirectInputA)
{
    /*** IUnknown methods ***/
    STDMETHOD(QueryInterface)(THIS_ REFIID riid, LPVOID * ppvObj) PURE;
    STDMETHOD_(ULONG,AddRef)(THIS) PURE;
    STDMETHOD_(ULONG,Release)(THIS) PURE;

    /*** IDirectInputA methods ***/
    STDMETHOD(CreateDevice)(THIS_ REFGUID,LPDIRECTINPUTDEVICEA *,LPUNKNOWN) PURE;
    STDMETHOD(EnumDevices)(THIS_ DWORD,LPDIENUMDEVICESCALLBACKA,LPVOID,DWORD) PURE;
    STDMETHOD(GetDeviceStatus)(THIS_ REFGUID) PURE;
    STDMETHOD(RunControlPanel)(THIS_ HWND,DWORD) PURE;
    STDMETHOD(Initialize)(THIS_ HINSTANCE,DWORD) PURE;

    /*** IDirectInput2A methods ***/
    STDMETHOD(FindDevice)(THIS_ REFGUID,LPCSTR,LPGUID) PURE;
};

typedef struct IDirectInput2A *LPDIRECTINPUT2A;

#ifdef UNICODE
#define IID_IDirectInput2 IID_IDirectInput2W
#define IDirectInput2 IDirectInput2W
#define IDirectInput2Vtbl IDirectInput2WVtbl
#else
#define IID_IDirectInput2 IID_IDirectInput2A
#define IDirectInput2 IDirectInput2A
#define IDirectInput2Vtbl IDirectInput2AVtbl
#endif
typedef struct IDirectInput2 *LPDIRECTINPUT2;

#if !defined(__cplusplus) || defined(CINTERFACE)
#define IDirectInput2_QueryInterface(p,a,b) (p)->lpVtbl->QueryInterface(p,a,b)
#define IDirectInput2_AddRef(p) (p)->lpVtbl->AddRef(p)
#define IDirectInput2_Release(p) (p)->lpVtbl->Release(p)
#define IDirectInput2_CreateDevice(p,a,b,c) (p)->lpVtbl->CreateDevice(p,a,b,c)
#define IDirectInput2_EnumDevices(p,a,b,c,d) (p)->lpVtbl->EnumDevices(p,a,b,c,d)
#define IDirectInput2_GetDeviceStatus(p,a) (p)->lpVtbl->GetDeviceStatus(p,a)
#define IDirectInput2_RunControlPanel(p,a,b) (p)->lpVtbl->RunControlPanel(p,a,b)
#define IDirectInput2_Initialize(p,a,b) (p)->lpVtbl->Initialize(p,a,b)
#define IDirectInput2_FindDevice(p,a,b,c) (p)->lpVtbl->FindDevice(p,a,b,c)
#else
#define IDirectInput2_QueryInterface(p,a,b) (p)->QueryInterface(a,b)
#define IDirectInput2_AddRef(p) (p)->AddRef()
#define IDirectInput2_Release(p) (p)->Release()
#define IDirectInput2_CreateDevice(p,a,b,c) (p)->CreateDevice(a,b,c)
#define IDirectInput2_EnumDevices(p,a,b,c,d) (p)->EnumDevices(a,b,c,d)
#define IDirectInput2_GetDeviceStatus(p,a) (p)->GetDeviceStatus(a)
#define IDirectInput2_RunControlPanel(p,a,b) (p)->RunControlPanel(a,b)
#define IDirectInput2_Initialize(p,a,b) (p)->Initialize(a,b)
#define IDirectInput2_FindDevice(p,a,b,c) (p)->FindDevice(a,b,c)
#endif


#undef INTERFACE
#define INTERFACE IDirectInput7W

DECLARE_INTERFACE_(IDirectInput7W, IDirectInput2W)
{
    /*** IUnknown methods ***/
    STDMETHOD(QueryInterface)(THIS_ REFIID riid, LPVOID * ppvObj) PURE;
    STDMETHOD_(ULONG,AddRef)(THIS) PURE;
    STDMETHOD_(ULONG,Release)(THIS) PURE;

    /*** IDirectInput2W methods ***/
    STDMETHOD(CreateDevice)(THIS_ REFGUID,LPDIRECTINPUTDEVICEW *,LPUNKNOWN) PURE;
    STDMETHOD(EnumDevices)(THIS_ DWORD,LPDIENUMDEVICESCALLBACKW,LPVOID,DWORD) PURE;
    STDMETHOD(GetDeviceStatus)(THIS_ REFGUID) PURE;
    STDMETHOD(RunControlPanel)(THIS_ HWND,DWORD) PURE;
    STDMETHOD(Initialize)(THIS_ HINSTANCE,DWORD) PURE;
    STDMETHOD(FindDevice)(THIS_ REFGUID,LPCWSTR,LPGUID) PURE;

    /*** IDirectInput7W methods ***/
    STDMETHOD(CreateDeviceEx)(THIS_ REFGUID,REFIID,LPVOID *,LPUNKNOWN) PURE;
};

typedef struct IDirectInput7W *LPDIRECTINPUT7W;

#undef INTERFACE
#define INTERFACE IDirectInput7A

DECLARE_INTERFACE_(IDirectInput7A, IDirectInput2A)
{
    /*** IUnknown methods ***/
    STDMETHOD(QueryInterface)(THIS_ REFIID riid, LPVOID * ppvObj) PURE;
    STDMETHOD_(ULONG,AddRef)(THIS) PURE;
    STDMETHOD_(ULONG,Release)(THIS) PURE;

    /*** IDirectInput2A methods ***/
    STDMETHOD(CreateDevice)(THIS_ REFGUID,LPDIRECTINPUTDEVICEA *,LPUNKNOWN) PURE;
    STDMETHOD(EnumDevices)(THIS_ DWORD,LPDIENUMDEVICESCALLBACKA,LPVOID,DWORD) PURE;
    STDMETHOD(GetDeviceStatus)(THIS_ REFGUID) PURE;
    STDMETHOD(RunControlPanel)(THIS_ HWND,DWORD) PURE;
    STDMETHOD(Initialize)(THIS_ HINSTANCE,DWORD) PURE;
    STDMETHOD(FindDevice)(THIS_ REFGUID,LPCSTR,LPGUID) PURE;

    /*** IDirectInput7A methods ***/
    STDMETHOD(CreateDeviceEx)(THIS_ REFGUID,REFIID,LPVOID *,LPUNKNOWN) PURE;
};

typedef struct IDirectInput7A *LPDIRECTINPUT7A;

#ifdef UNICODE
#define IID_IDirectInput7 IID_IDirectInput7W
#define IDirectInput7 IDirectInput7W
#define IDirectInput7Vtbl IDirectInput7WVtbl
#else
#define IID_IDirectInput7 IID_IDirectInput7A
#define IDirectInput7 IDirectInput7A
#define IDirectInput7Vtbl IDirectInput7AVtbl
#endif
typedef struct IDirectInput7 *LPDIRECTINPUT7;

#if !defined(__cplusplus) || defined(CINTERFACE)
#define IDirectInput7_QueryInterface(p,a,b) (p)->lpVtbl->QueryInterface(p,a,b)
#define IDirectInput7_AddRef(p) (p)->lpVtbl->AddRef(p)
#define IDirectInput7_Release(p) (p)->lpVtbl->Release(p)
#define IDirectInput7_CreateDevice(p,a,b,c) (p)->lpVtbl->CreateDevice(p,a,b,c)
#define IDirectInput7_EnumDevices(p,a,b,c,d) (p)->lpVtbl->EnumDevices(p,a,b,c,d)
#define IDirectInput7_GetDeviceStatus(p,a) (p)->lpVtbl->GetDeviceStatus(p,a)
#define IDirectInput7_RunControlPanel(p,a,b) (p)->lpVtbl->RunControlPanel(p,a,b)
#define IDirectInput7_Initialize(p,a,b) (p)->lpVtbl->Initialize(p,a,b)
#define IDirectInput7_FindDevice(p,a,b,c) (p)->lpVtbl->FindDevice(p,a,b,c)
#define IDirectInput7_CreateDeviceEx(p,a,b,c,d) (p)->lpVtbl->CreateDeviceEx(p,a,b,c,d)
#else
#define IDirectInput7_QueryInterface(p,a,b) (p)->QueryInterface(a,b)
#define IDirectInput7_AddRef(p) (p)->AddRef()
#define IDirectInput7_Release(p) (p)->Release()
#define IDirectInput7_CreateDevice(p,a,b,c) (p)->CreateDevice(a,b,c)
#define IDirectInput7_EnumDevices(p,a,b,c,d) (p)->EnumDevices(a,b,c,d)
#define IDirectInput7_GetDeviceStatus(p,a) (p)->GetDeviceStatus(a)
#define IDirectInput7_RunControlPanel(p,a,b) (p)->RunControlPanel(a,b)
#define IDirectInput7_Initialize(p,a,b) (p)->Initialize(a,b)
#define IDirectInput7_FindDevice(p,a,b,c) (p)->FindDevice(a,b,c)
#define IDirectInput7_CreateDeviceEx(p,a,b,c,d) (p)->CreateDeviceEx(a,b,c,d)
#endif

extern HRESULT WINAPI DirectInputCreateA(HINSTANCE hinst, DWORD dwVersion, LPDIRECTINPUTA *ppDI, LPUNKNOWN punkOuter);
extern HRESULT WINAPI DirectInputCreateW(HINSTANCE hinst, DWORD dwVersion, LPDIRECTINPUTW *ppDI, LPUNKNOWN punkOuter);
#ifdef UNICODE
#define DirectInputCreate  DirectInputCreateW
#else
#define DirectInputCreate  DirectInputCreateA
#endif // !UNICODE

extern HRESULT WINAPI DirectInputCreateEx(HINSTANCE hinst, DWORD dwVersion, REFIID riidltf, LPVOID *ppvOut, LPUNKNOWN punkOuter);
#endif /* DIJ_RINGZERO */


/****************************************************************************
 *
 *  Return Codes
 *
 ****************************************************************************/

/*
 *  The operation completed successfully.
 */
#define DI_OK                           S_OK

/*
 *  The device exists but is not currently attached.
 */
#define DI_NOTATTACHED                  S_FALSE

/*
 *  The device buffer overflowed.  Some input was lost.
 */
#define DI_BUFFEROVERFLOW               S_FALSE

/*
 *  The change in device properties had no effect.
 */
#define DI_PROPNOEFFECT                 S_FALSE

/*
 *  The operation had no effect.
 */
#define DI_NOEFFECT                     S_FALSE

/*
 *  The device is a polled device.  As a result, device buffering
 *  will not collect any data and event notifications will not be
 *  signalled until GetDeviceState is called.
 */
#define DI_POLLEDDEVICE                 ((HRESULT)0x00000002L)

/*
 *  The parameters of the effect were successfully updated by
 *  IDirectInputEffect::SetParameters, but the effect was not
 *  downloaded because the device is not exclusively acquired
 *  or because the DIEP_NODOWNLOAD flag was passed.
 */
#define DI_DOWNLOADSKIPPED              ((HRESULT)0x00000003L)

/*
 *  The parameters of the effect were successfully updated by
 *  IDirectInputEffect::SetParameters, but in order to change
 *  the parameters, the effect needed to be restarted.
 */
#define DI_EFFECTRESTARTED              ((HRESULT)0x00000004L)

/*
 *  The parameters of the effect were successfully updated by
 *  IDirectInputEffect::SetParameters, but some of them were
 *  beyond the capabilities of the device and were truncated.
 */
#define DI_TRUNCATED                    ((HRESULT)0x00000008L)

/*
 *  Equal to DI_EFFECTRESTARTED | DI_TRUNCATED.
 */
#define DI_TRUNCATEDANDRESTARTED        ((HRESULT)0x0000000CL)

/*
 *  The application requires a newer version of DirectInput.
 */
#define DIERR_OLDDIRECTINPUTVERSION     \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_WIN32, ERROR_OLD_WIN_VERSION)

/*
 *  The application was written for an unsupported prerelease version
 *  of DirectInput.
 */
#define DIERR_BETADIRECTINPUTVERSION    \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_WIN32, ERROR_RMODE_APP)

/*
 *  The object could not be created due to an incompatible driver version
 *  or mismatched or incomplete driver components.
 */
#define DIERR_BADDRIVERVER              \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_WIN32, ERROR_BAD_DRIVER_LEVEL)

/*
 * The device or device instance or effect is not registered with DirectInput.
 */
#define DIERR_DEVICENOTREG              REGDB_E_CLASSNOTREG

/*
 * The requested object does not exist.
 */
#define DIERR_NOTFOUND                  \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_WIN32, ERROR_FILE_NOT_FOUND)

/*
 * The requested object does not exist.
 */
#define DIERR_OBJECTNOTFOUND            \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_WIN32, ERROR_FILE_NOT_FOUND)

/*
 * An invalid parameter was passed to the returning function,
 * or the object was not in a state that admitted the function
 * to be called.
 */
#define DIERR_INVALIDPARAM              E_INVALIDARG

/*
 * The specified interface is not supported by the object
 */
#define DIERR_NOINTERFACE               E_NOINTERFACE

/*
 * An undetermined error occured inside the DInput subsystem
 */
#define DIERR_GENERIC                   E_FAIL

/*
 * The DInput subsystem couldn't allocate sufficient memory to complete the
 * caller's request.
 */
#define DIERR_OUTOFMEMORY               E_OUTOFMEMORY

/*
 * The function called is not supported at this time
 */
#define DIERR_UNSUPPORTED               E_NOTIMPL

/*
 * This object has not been initialized
 */
#define DIERR_NOTINITIALIZED            \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_WIN32, ERROR_NOT_READY)

/*
 * This object is already initialized
 */
#define DIERR_ALREADYINITIALIZED        \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_WIN32, ERROR_ALREADY_INITIALIZED)

/*
 * This object does not support aggregation
 */
#define DIERR_NOAGGREGATION             CLASS_E_NOAGGREGATION

/*
 * Another app has a higher priority level, preventing this call from
 * succeeding.
 */
#define DIERR_OTHERAPPHASPRIO           E_ACCESSDENIED

/*
 * Access to the device has been lost.  It must be re-acquired.
 */
#define DIERR_INPUTLOST                 \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_WIN32, ERROR_READ_FAULT)

/*
 * The operation cannot be performed while the device is acquired.
 */
#define DIERR_ACQUIRED                  \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_WIN32, ERROR_BUSY)

/*
 * The operation cannot be performed unless the device is acquired.
 */
#define DIERR_NOTACQUIRED               \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_WIN32, ERROR_INVALID_ACCESS)

/*
 * The specified property cannot be changed.
 */
#define DIERR_READONLY                  E_ACCESSDENIED

/*
 * The device already has an event notification associated with it.
 */
#define DIERR_HANDLEEXISTS              E_ACCESSDENIED

/*
 * Data is not yet available.
 */
#ifndef E_PENDING
#define E_PENDING                       0x8000000AL
#endif

/*
 * Unable to IDirectInputJoyConfig_Acquire because the user
 * does not have sufficient privileges to change the joystick
 * configuration.
 */
#define DIERR_INSUFFICIENTPRIVS         0x80040200L

/*
 * The device is full.
 */
#define DIERR_DEVICEFULL                0x80040201L

/*
 * Not all the requested information fit into the buffer.
 */
#define DIERR_MOREDATA                  0x80040202L

/*
 * The effect is not downloaded.
 */
#define DIERR_NOTDOWNLOADED             0x80040203L

/*
 *  The device cannot be reinitialized because there are still effects
 *  attached to it.
 */
#define DIERR_HASEFFECTS                0x80040204L

/*
 *  The operation cannot be performed unless the device is acquired
 *  in DISCL_EXCLUSIVE mode.
 */
#define DIERR_NOTEXCLUSIVEACQUIRED      0x80040205L

/*
 *  The effect could not be downloaded because essential information
 *  is missing.  For example, no axes have been associated with the
 *  effect, or no type-specific information has been created.
 */
#define DIERR_INCOMPLETEEFFECT          0x80040206L

/*
 *  Attempted to read buffered device data from a device that is
 *  not buffered.
 */
#define DIERR_NOTBUFFERED               0x80040207L

/*
 *  An attempt was made to modify parameters of an effect while it is
 *  playing.  Not all hardware devices support altering the parameters
 *  of an effect while it is playing.
 */
#define DIERR_EFFECTPLAYING             0x80040208L

/*
 *  The operation could not be completed because the device is not
 *  plugged in.
 */
#define DIERR_UNPLUGGED                 0x80040209L

/*
 *  SendDeviceData failed because more information was requested
 *  to be sent than can be sent to the device.  Some devices have
 *  restrictions on how much data can be sent to them.  (For example,
 *  there might be a limit on the number of buttons that can be
 *  pressed at once.)
 */
#define DIERR_REPORTFULL                0x8004020AL

#ifdef __cplusplus
};
#endif

#endif  /* __DINPUT_INCLUDED__ */

/****************************************************************************
 *
 *  Definitions for non-IDirectInput (VJoyD) features defined more recently
 *  than the current sdk files
 *
 ****************************************************************************/

#ifdef _INC_MMSYSTEM
#ifndef MMNOJOY

#ifndef __VJOYDX_INCLUDED__
#define __VJOYDX_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Flag to indicate that the dwReserved2 field of the JOYINFOEX structure
 * contains mini-driver specific data to be passed by VJoyD to the mini-
 * driver instead of doing a poll.
 */
#define JOY_PASSDRIVERDATA          0x10000000l

/*
 * Informs the joystick driver that the configuration has been changed
 * and should be reloaded from the registery.
 * dwFlags is reserved and should be set to zero
 */
WINMMAPI MMRESULT WINAPI joyConfigChanged( DWORD dwFlags );


/*
 * Hardware Setting indicating that the device is a headtracker
 */
#define JOY_HWS_ISHEADTRACKER       0x02000000l

/*
 * Hardware Setting indicating that the VxD is used to replace
 * the standard analog polling
 */
#define JOY_HWS_ISGAMEPORTDRIVER    0x04000000l

/*
 * Hardware Setting indicating that the driver needs a standard
 * gameport in order to communicate with the device.
 */
#define JOY_HWS_ISANALOGPORTDRIVER  0x08000000l

/*
 * Hardware Setting indicating that VJoyD should not load this
 * driver, it will be loaded externally and will register with
 * VJoyD of it's own accord.
 */
#define JOY_HWS_AUTOLOAD            0x10000000l

/*
 * Hardware Setting indicating that the driver acquires any
 * resources needed without needing a devnode through VJoyD.
 */
#define JOY_HWS_NODEVNODE           0x20000000l


/*
 * Hardware Setting indicating that the device is a gameport bus
 */
#define JOY_HWS_ISGAMEPORTBUS       0x80000000l
#define JOY_HWS_GAMEPORTBUSBUSY     0x00000001l

/*
 * Usage Setting indicating that the settings are volatile and
 * should be removed if still present on a reboot.
 */
#define JOY_US_VOLATILE             0x00000008L

#ifdef __cplusplus
};
#endif

#endif  /* __VJOYDX_INCLUDED__ */

#endif  /* not MMNOJOY */
#endif  /* _INC_MMSYSTEM */

/****************************************************************************
 *
 *  Definitions for non-IDirectInput (VJoyD) features defined more recently
 *  than the current ddk files
 *
 ****************************************************************************/

#ifndef DIJ_RINGZERO

#ifdef _INC_MMDDK
#ifndef MMNOJOYDEV

#ifndef __VJOYDXD_INCLUDED__
#define __VJOYDXD_INCLUDED__
/*
 * Poll type in which the do_other field of the JOYOEMPOLLDATA
 * structure contains mini-driver specific data passed from an app.
 */
#define JOY_OEMPOLL_PASSDRIVERDATA  7

#endif  /* __VJOYDXD_INCLUDED__ */

#endif  /* not MMNOJOYDEV */
#endif  /* _INC_MMDDK */

#endif /* DIJ_RINGZERO */
