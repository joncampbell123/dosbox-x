/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is for Open Layer for Unicode (opencow).
 *
 * The Initial Developer of the Original Code is Brodie Thiesfield.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// define these symbols so that we don't get dllimport linkage 
// from the system headers
#define _KERNEL32_

#include <windows.h>
#include <winspool.h>

#include "MbcsBuffer.h"


// ----------------------------------------------------------------------------
// API
EXTERN_C {
// AddJobW
// AddMonitorW
// AddPortW
// AddPrintProcessorW
// AddPrintProvidorW
// AddPrinterDriverW
// AddPrinterW
// AdvancedDocumentPropertiesW
// ConfigurePortW
// DeleteMonitorW
// DeletePortW
// DeletePrintProcessorW
// DeletePrintProvidorW
// DeletePrinterDriverW
// DeviceCapabilitiesW

OCOW_DEF(LONG, DocumentPropertiesW,
    (IN HWND      hWnd,
    IN HANDLE    hPrinter,
    IN LPWSTR    pDeviceName,
    OUT PDEVMODEW pDevModeOutput,
    IN PDEVMODEW pDevModeInput,
    IN DWORD     fMode 
    ))
{
    CMbcsBuffer mbcsDeviceName;
    if (!mbcsDeviceName.FromUnicode(pDeviceName))
        return -1;

    LPDEVMODEA pDevModeInputA = 0;
    DEVMODEA devModeInputAStatic;
    if (fMode & DM_IN_BUFFER) {
        if (pDevModeInput->dmDriverExtra > 0) {
            pDevModeInputA = (LPDEVMODEA) ::malloc(sizeof(DEVMODEA) + pDevModeInput->dmDriverExtra);
            if (!pDevModeInputA) {
                ::SetLastError(ERROR_NOT_ENOUGH_MEMORY);
                return -1;
            }
            ::CopyMemory(pDevModeInputA + sizeof(DEVMODEA), pDevModeInput + pDevModeInput->dmSize, 
                pDevModeInput->dmDriverExtra);
        }
        else {
            pDevModeInputA = &devModeInputAStatic;
        }
        ::ZeroMemory(pDevModeInputA, sizeof(DEVMODEA)); // don't zero the extra bytes

        pDevModeInputA->dmSpecVersion       = DM_SPECVERSION;
        pDevModeInputA->dmSize              = sizeof(DEVMODEA);
        pDevModeInputA->dmDriverVersion     = pDevModeInput->dmDriverVersion;
        pDevModeInputA->dmFields            = pDevModeInput->dmFields;
        pDevModeInputA->dmOrientation       = pDevModeInput->dmOrientation;
        pDevModeInputA->dmPaperSize         = pDevModeInput->dmPaperSize;
        pDevModeInputA->dmPaperLength       = pDevModeInput->dmPaperLength;
        pDevModeInputA->dmPaperWidth        = pDevModeInput->dmPaperWidth;
        pDevModeInputA->dmScale             = pDevModeInput->dmScale;
        pDevModeInputA->dmCopies            = pDevModeInput->dmCopies;
        pDevModeInputA->dmDefaultSource     = pDevModeInput->dmDefaultSource;
        pDevModeInputA->dmPrintQuality      = pDevModeInput->dmPrintQuality;
        pDevModeInputA->dmColor             = pDevModeInput->dmColor;
        pDevModeInputA->dmDuplex            = pDevModeInput->dmDuplex;
        pDevModeInputA->dmYResolution       = pDevModeInput->dmYResolution;
        pDevModeInputA->dmTTOption          = pDevModeInput->dmTTOption;
        pDevModeInputA->dmCollate           = pDevModeInput->dmCollate;
        pDevModeInputA->dmLogPixels         = pDevModeInput->dmLogPixels;
        pDevModeInputA->dmBitsPerPel        = pDevModeInput->dmBitsPerPel;
        pDevModeInputA->dmPelsWidth         = pDevModeInput->dmPelsWidth;
        pDevModeInputA->dmPelsHeight        = pDevModeInput->dmPelsHeight;
        pDevModeInputA->dmDisplayFlags      = pDevModeInput->dmDisplayFlags;
        pDevModeInputA->dmDisplayFrequency  = pDevModeInput->dmDisplayFrequency;

        ::WideCharToMultiByte(CP_ACP, 0, pDevModeInput->dmDeviceName, -1, 
            (LPSTR) pDevModeInputA->dmDeviceName, CCHDEVICENAME, NULL, NULL);
        if (pDevModeInputA->dmFields & DM_FORMNAME) {
            ::WideCharToMultiByte(CP_ACP, 0, pDevModeInput->dmFormName, -1, 
                (LPSTR) pDevModeInputA->dmFormName, CCHFORMNAME, NULL, NULL);
        }
    }

    LPDEVMODEA pDevModeOutputA = 0;
    DEVMODEA devModeOutputAStatic;
    if (fMode & DM_OUT_BUFFER) {
        if (pDevModeOutput->dmDriverExtra > 0) {
            pDevModeOutputA = (LPDEVMODEA) ::malloc(sizeof(DEVMODEA) + pDevModeOutput->dmDriverExtra);
            if (!pDevModeOutputA) {
                SetLastError(ERROR_NOT_ENOUGH_MEMORY);
                return -1;
            }
        }
        else {
            pDevModeOutputA = &devModeOutputAStatic;
        }
        ::ZeroMemory(pDevModeOutputA, sizeof(DEVMODEA) + pDevModeOutput->dmDriverExtra);

        pDevModeOutputA->dmSpecVersion = DM_SPECVERSION;
        pDevModeOutputA->dmSize = sizeof(DEVMODEA);
        pDevModeOutputA->dmDriverExtra = pDevModeOutput->dmDriverExtra;
    }

    LONG lResult = ::DocumentPropertiesA(hWnd, hPrinter, mbcsDeviceName,
        pDevModeOutputA, pDevModeInputA, fMode);
    
    if (pDevModeInputA != &devModeInputAStatic)
        ::free(pDevModeInputA);

    if (lResult == IDOK && pDevModeOutputA) {
        if (pDevModeOutputA->dmDriverExtra > 0) {
            ::CopyMemory(pDevModeOutput + pDevModeOutput->dmSize, pDevModeOutputA + pDevModeOutputA->dmSize, 
                pDevModeOutputA->dmDriverExtra);
        }

        pDevModeOutput->dmDriverVersion     = pDevModeOutputA->dmDriverVersion;
        pDevModeOutput->dmFields            = pDevModeOutputA->dmFields;
        pDevModeOutput->dmOrientation       = pDevModeOutputA->dmOrientation;
        pDevModeOutput->dmPaperSize         = pDevModeOutputA->dmPaperSize;
        pDevModeOutput->dmPaperLength       = pDevModeOutputA->dmPaperLength;
        pDevModeOutput->dmPaperWidth        = pDevModeOutputA->dmPaperWidth;
        pDevModeOutput->dmScale             = pDevModeOutputA->dmScale;
        pDevModeOutput->dmCopies            = pDevModeOutputA->dmCopies;
        pDevModeOutput->dmDefaultSource     = pDevModeOutputA->dmDefaultSource;
        pDevModeOutput->dmPrintQuality      = pDevModeOutputA->dmPrintQuality;
        pDevModeOutput->dmColor             = pDevModeOutputA->dmColor;
        pDevModeOutput->dmDuplex            = pDevModeOutputA->dmDuplex;
        pDevModeOutput->dmYResolution       = pDevModeOutputA->dmYResolution;
        pDevModeOutput->dmTTOption          = pDevModeOutputA->dmTTOption;
        pDevModeOutput->dmCollate           = pDevModeOutputA->dmCollate;
        pDevModeOutput->dmLogPixels         = pDevModeOutputA->dmLogPixels;
        pDevModeOutput->dmBitsPerPel        = pDevModeOutputA->dmBitsPerPel;
        pDevModeOutput->dmPelsWidth         = pDevModeOutputA->dmPelsWidth;
        pDevModeOutput->dmPelsHeight        = pDevModeOutputA->dmPelsHeight;
        pDevModeOutput->dmDisplayFlags      = pDevModeOutputA->dmDisplayFlags;
        pDevModeOutput->dmDisplayFrequency  = pDevModeOutputA->dmDisplayFrequency;

        ::MultiByteToWideChar(CP_ACP, 0, (LPSTR) pDevModeOutputA->dmDeviceName, -1, 
            (LPWSTR) pDevModeOutput->dmDeviceName, CCHDEVICENAME);
        if (pDevModeOutput->dmFields & DM_FORMNAME) {
            ::MultiByteToWideChar(CP_ACP, 0, (LPSTR) pDevModeOutputA->dmFormName, -1, 
                (LPWSTR) pDevModeOutput->dmFormName, CCHFORMNAME);
        }
    }

    if (pDevModeOutputA != &devModeOutputAStatic)
        ::free(pDevModeOutputA);
    return lResult;
}

// EnumMonitorsW
// EnumPortsW
// EnumPrintProcessorDatatypesW
// EnumPrintProcessorsW
// EnumPrinterDriversW
// EnumPrintersW
// GetJobW
// GetPrintProcessorDirectoryW
// GetPrinterDataW
// GetPrinterDriverDirectoryW
// GetPrinterDriverW
// GetPrinterW
// OpenPrinterW
// ResetPrinterW
// SetJobW
// SetPrinterDataW
// SetPrinterW
// StartDocPrinterW
}//EXTERN_C 