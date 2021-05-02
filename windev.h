//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this sample source code is subject to the terms of the Microsoft
// license agreement under which you licensed this sample source code. If
// you did not accept the terms of the license agreement, you are not
// authorized to use this sample source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the LICENSE.RTF on your install media or the root of your tools installation.
// THE SAMPLE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
/*++
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Module Name:  

windev.h

Abstract:  

Notes: 


--*/

#pragma once

#ifndef __WINDEV_H__
#define __WINDEV_H__ 1

#include <winioctl.h>

#ifdef __cplusplus
extern "C" {
#endif

// @doc OSDEVICE

//      @func PVOID | ttt_Init | Device initialization routine
//  @parm DWORD | dwInfo | info passed to RegisterDevice
//  @rdesc      Returns a DWORD which will be passed to Open & Deinit or NULL if
//                      unable to initialize the device.
//      @comm   Routine exported by a device driver.  "ttt" is the string passed in as
//                      lpszType in RegisterDevice

DWORD ttt_Init(DWORD dwInfo);

//      @func PVOID | ttt_Deinit | Device deinitialization routine
//  @parm DWORD | dwData | value returned from ttt_Init call
//  @rdesc      Returns TRUE for success, FALSE for failure.
//      @comm   Routine exported by a device driver.  "ttt" is the string passed in as
//                      lpszType in RegisterDevice

BOOL ttt_Deinit(DWORD dwData);

//      @func PVOID | ttt_Open | Device open routine
//  @parm DWORD | dwData | value returned from ttt_Init call
//  @parm DWORD | dwAccess | requested access (combination of GENERIC_READ and GENERIC_WRITE)
//  @parm DWORD | dwShareMode | requested share mode (combination of FILE_SHARE_READ and FILE_SHARE_WRITE)
//  @rdesc      Returns a DWORD which will be passed to Read, Write, etc or NULL if
//                      unable to open device.
//      @comm   Routine exported by a device driver.  "ttt" is the string passed in as
//                      lpszType in RegisterDevice

DWORD ttt_Open(DWORD dwData, DWORD dwAccess, DWORD dwShareMode);

//      @func BOOL | ttt_Close | Device close routine
//  @parm DWORD | dwOpenData | value returned from ttt_Open call
//  @rdesc      Returns TRUE for success, FALSE for failure
//      @comm   Routine exported by a device driver.  "ttt" is the string passed in as
//                      lpszType in RegisterDevice

BOOL ttt_Close(DWORD dwOpenData);

//      @func DWORD | ttt_Read | Device read routine
//  @parm DWORD | dwOpenData | value returned from ttt_Open call
//  @parm LPVOID | pBuf | buffer to receive data
//  @parm DWORD | len | maximum length to read
//  @rdesc      Returns 0 for end of file, -1 for error, otherwise the number of
//                      bytes read.  The length returned is guaranteed to be the length
//                      requested unless end of file or an error condition occurs.
//      @comm   Routine exported by a device driver.  "ttt" is the string passed in as
//                      lpszType in RegisterDevice

DWORD ttt_Read(DWORD dwOpenData, LPVOID pBuf, DWORD len);

//      @func DWORD | ttt_Write | Device write routine
//  @parm DWORD | dwOpenData | value returned from ttt_Open call
//  @parm LPCVOID | pBuf | buffer containing data
//  @parm DWORD | len | maximum length to write
//  @rdesc      Returns -1 for error, otherwise the number of bytes written.  The
//                      length returned is guaranteed to be the length requested unless an
//                      error condition occurs.
//      @comm   Routine exported by a device driver.  "ttt" is the string passed in as
//                      lpszType in RegisterDevice

DWORD ttt_Write(DWORD dwOpenData, LPCVOID pBuf, DWORD len);

//      @func DWORD | ttt_Seek | Device seek routine
//  @parm DWORD | dwOpenData | value returned from ttt_Open call
//  @parm long | pos | position to seek to (relative to type)
//  @parm DWORD | type | FILE_BEGIN, FILE_CURRENT, or FILE_END
//  @rdesc      Returns current position relative to start of file, or -1 on error
//      @comm   Routine exported by a device driver.  "ttt" is the string passed in as
//                      lpszType in RegisterDevice

DWORD ttt_Seek(DWORD dwOpenData, long pos, DWORD type);

//      @func BOOL | ttt_IOControl | Device IO control routine
//  @parm DWORD | dwOpenData | value returned from ttt_Open call
//  @parm DWORD | dwCode | io control code to be performed
//  @parm PBYTE | pBufIn | input data to the device
//  @parm DWORD | dwLenIn | number of bytes being passed in
//  @parm PBYTE | pBufOut | output data from the device
//  @parm DWORD | dwLenOut |maximum number of bytes to receive from device
//  @parm PDWORD | dwActualOut | actual number of bytes received from device
//  @rdesc      Returns TRUE for success, FALSE for failure
//      @comm   Routine exported by a device driver.  "ttt" is the string passed in as
//                      lpszType in RegisterDevice

BOOL ttt_IOControl(DWORD dwOpenData, DWORD dwCode, PBYTE pBufIn, DWORD dwLenIn, PBYTE pBufOut, DWORD dwLenOut, PDWORD dwActualOut);

//      @func void | ttt_PowerDown | Device powerdown routine
//  @parm DWORD | dwData | value returned from ttt_Init call
//      @comm   Called to suspend device.  You cannot call any routines aside from
//                      those in your dll in this call.

void ttt_PowerDown(DWORD dwData);

//      @func void | ttt_PowerUp | Device powerup routine
//  @parm DWORD | dwData | value returned from ttt_Init call
//      @comm   Called to restore device from suspend mode.  You cannot call any
//                      routines aside from those in your dll in this call.

void ttt_PowerUp(DWORD dwData);

// EnumDevices enumerates the active devices in the system.
// The return buffer is filled with a double-NULL terminated list of NULL
// terminated strings of the device names, for example: COM1:\0COM2:\0PGR1:\0\0.
DWORD EnumDevices(LPTSTR DevList, LPDWORD lpBuflen);

// GetDeviceKeys returns the registry paths for the device driver key and the
// active device key for the specified device.
DWORD GetDeviceKeys(LPCTSTR DevName, LPTSTR ActiveKey, LPDWORD lpActiveLen,
                    LPTSTR DriverKey, LPDWORD lpDriverLen);


#ifdef __cplusplus
}
#endif

#endif  // __WINDEV_H__
