#pragma once

#include <windows.h>

#define VCOMDRIVER_WRITE_SIZE 254
#define VCOMDRIVER_MAX_COUNT 9

#ifdef VCOM_EXPORTS
#define VCOM_API __declspec(dllexport)
#else
#define VCOM_API __declspec(dllimport)
#endif

#define VCOM_DRIVER_API __declspec(dllexport)

//vcom exports, for users
VCOM_API int VCOM_Init (DWORD idx); //register driver and init shared memory
VCOM_API int VCOM_Write (DWORD idx, const BYTE *msg, size_t length); //send data to vcom client, max length is 254
VCOM_API int VCOM_Read (DWORD idx); //get data from vcom client NOT IMPLEMENTED
VCOM_API int VCOM_DeInit (DWORD idx); //shutdown driver, idx ignored

//com driver exports, for system
VCOM_DRIVER_API DWORD COM_Init(LPCTSTR pContext, LPCVOID lpvBusContext);
VCOM_DRIVER_API BOOL COM_Deinit( DWORD hDeviceContext );
VCOM_DRIVER_API DWORD COM_Open( DWORD hDeviceContext, DWORD AccessCode, DWORD ShareMode );
VCOM_DRIVER_API BOOL COM_Close( DWORD hOpenContext );
VCOM_DRIVER_API BOOL COM_IOControl( DWORD hOpenContext, DWORD dwCode, PBYTE pBufIn, DWORD dwLenIn, PBYTE pBufOut, DWORD dwLenOut, PDWORD pdwActualOut );
VCOM_DRIVER_API void COM_PowerUp( DWORD hDeviceContext );
VCOM_DRIVER_API void COM_PowerDown( DWORD hDeviceContext );
VCOM_DRIVER_API DWORD COM_Read( DWORD hOpenContext, LPVOID pBuffer, DWORD Count );
VCOM_DRIVER_API DWORD COM_Write( DWORD hOpenContext, LPCVOID pBuffer, DWORD Count );
VCOM_DRIVER_API DWORD COM_Seek( DWORD hOpenContext, long Amount, WORD Type );
