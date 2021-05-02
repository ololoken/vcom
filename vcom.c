//
#include "vcom.h"
#include "pegdser.h"

#define VCOMDRIVER_DEV_REG_KEY (TEXT("Drivers\\Builtin\\VCOM"))
#define VCOMDRIVER_DEV_KEY (TEXT("vcom.dll"))
#define VCOMDRIVER_DEV_PREFICS (TEXT("COM"))
#define VCOMDRIVER_MAP_FILE_NAME (TEXT("vcom_map_file$"))
#define VCOMDRIVER_MAP_EVENT_NAME (TEXT("vcom_map_event$"))
#define VCOMDRIVER_PAGE_SIZE 255

#ifdef _DEBUG
#define LOG_CALLS
#endif

__inline void Log (char *format, ...) {
#if defined(LOG_CALLS) || defined(FORCE_LOG)
  int nBuf;
  char szBuffer[512];
  FILE* f = fopen("\\vcomdriver.txt", "a");
  if (f) {
    va_list args;
    va_start(args, format);
    nBuf = vsprintf(szBuffer, format, args);
    fprintf(f, "%u %s\n", GetTickCount(), szBuffer);
    fflush(f);
    fclose(f);
    va_end(args);
  }
#endif
}

#define DEVICE_CONTEXT	0x1450
#define DEVICE_OPEN_CONTEXT 0x1451

#define DEV_RX_BUF_SIZE 4096*4

HANDLE hComm = INVALID_HANDLE_VALUE;
DWORD dwManagePort = 1;

HANDLE g_hFile = NULL;
HANDLE g_hFileMap = NULL;
LPVOID g_lpMemMap = NULL;
HANDLE g_hMapEvent = NULL;
DWORD g_devIdx;

BYTE g_devBuf[DEV_RX_BUF_SIZE] = {0};
size_t g_devBufLen = 0;
DWORD g_devWaitMask = 0;

HANDLE DevSharedReaderThread = NULL;
HANDLE DevWaitEventThread = NULL;

CRITICAL_SECTION g_devBufOperation;
DWORD ct_ReadIntervalTimeout = 5,
      ct_ReadTotalTimeoutConstant = 5,
      ct_ReadTotalTimeoutMultiplier = 5,
      ct_WriteTotalTimeoutConstant = 5,
      ct_WriteTotalTimeoutMultiplier = 5;
DCB g_DCB;

struct VCOM {
  HANDLE hMapFileCL;
  HANDLE hVcomCL;
  LPVOID pMapBufCL;
  HANDLE hMapEventCL;
};

typedef struct VCOM VCOM;

VCOM g_vcoms[VCOMDRIVER_MAX_COUNT];

static DWORD WINAPI SharedReaderThreadProc (LPVOID lpParameter) {
  while (TRUE) {
    DWORD waitRes = WaitForSingleObject(g_hMapEvent, INFINITE);
    switch (waitRes) {//exit thread on error
      case WAIT_ABANDONED_0:
      case WAIT_FAILED: {
        Log("Wait error %d", GetLastError());
        ExitThread(-1);
        return -1;
      }
    }

    if (g_lpMemMap) {
      BYTE msg[VCOMDRIVER_PAGE_SIZE] = {0};
      BYTE len = 0;
      EnterCriticalSection(&g_devBufOperation);
      memcpy(&msg[0], g_lpMemMap, VCOMDRIVER_PAGE_SIZE);
      len = msg[0];
      if (g_devBufLen+len > DEV_RX_BUF_SIZE) {
        g_devBufLen = DEV_RX_BUF_SIZE-len;
        memcpy(&g_devBuf[0], &g_devBuf[len], g_devBufLen);
      }
      memcpy(&g_devBuf[g_devBufLen], &msg[1], len);
      g_devBufLen += len;
      LeaveCriticalSection(&g_devBufOperation);
    }
    else {break;}
  }
  Log("Exiting read thread normally");
  ExitThread(0);
  return 0;
}
static DWORD WINAPI WaitEventThreadProc (LPVOID lpParameter) {

}

BOOL APIENTRY DllMain (HANDLE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved) {
  switch (ul_reason_for_call) {
  case DLL_PROCESS_ATTACH:
    break;
  case DLL_PROCESS_DETACH:
    break;
  case DLL_THREAD_ATTACH:
    break;
  case DLL_THREAD_DETACH:
    break;
  }
  return TRUE;
}

typedef struct {
  TCHAR *ValueName;
  DWORD Type;
  DWORD DwordValue;
  TCHAR *StringValue;
} RegEntry ;


void VCOMRegistryAdd (DWORD idx, wchar_t *drvPath, wchar_t *regKey) {
  HKEY hTargetKey;
  DWORD dwDisposition, res, Index=0;
  BYTE *Value;
  DWORD uSize;
  RegEntry RegEntries[] = {
    {TEXT("Dll"), REG_SZ, 0, drvPath},
    {TEXT("Prefix"), REG_SZ, 0, VCOMDRIVER_DEV_PREFICS},
    {TEXT("Order"), REG_DWORD, idx, NULL},
    {TEXT("Index"), REG_DWORD, idx, NULL},
    {NULL, 0, 0, NULL}
  };

  res = RegCreateKeyEx(HKEY_LOCAL_MACHINE, regKey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hTargetKey, &dwDisposition);
  if (res == ERROR_SUCCESS) {
    while (RegEntries[Index].ValueName != NULL) {
      switch (RegEntries[Index].Type) {
        case REG_DWORD: {
          Value = (BYTE *)&RegEntries[Index].DwordValue;
          uSize = sizeof(DWORD);
        } break;
        default: {
          Value = (BYTE *)RegEntries[Index].StringValue;
          uSize = (wcslen(RegEntries[Index].StringValue)+1)*sizeof(TCHAR);
        }
      }
      res = RegSetValueEx(hTargetKey, RegEntries[Index].ValueName, 0, RegEntries[Index].Type, (CONST BYTE *)Value, uSize);
      Index++;
    }
  }
}

void VCOMRegistryDel (DWORD idx) {
  wchar_t regKey[MAX_PATH];
  RegDeleteKey(HKEY_LOCAL_MACHINE, regKey);
}

#pragma region vcom

void FullPath (wchar_t* Path, const wchar_t* relPath) {
  wchar_t* s;
  GetModuleFileName(NULL, Path, MAX_PATH);
  s = wcsrchr(Path, '\\');
  if (s) {
    s[1] = 0;
  }
  wcscat(Path, relPath);
}

VCOM_API int VCOM_Init (DWORD idx) {
  //try to init virtual com driver
  wchar_t drvPath[MAX_PATH] = {0};
  wchar_t regKey[MAX_PATH] = {0};
  wchar_t mapFileName[MAX_PATH] = {0};
  wchar_t mapEventName[MAX_PATH] = {0};
  FullPath(drvPath, VCOMDRIVER_DEV_KEY);
  VCOMRegistryAdd(idx, drvPath, regKey);
  g_vcoms[idx].hVcomCL = ActivateDeviceEx(regKey, NULL, 0, &idx);
  if (g_vcoms[idx].hVcomCL != 0 && g_vcoms[idx].hVcomCL != INVALID_HANDLE_VALUE) {
    g_vcoms[idx].hMapFileCL = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 256, mapFileName);
    if (g_vcoms[idx].hMapFileCL) {
      g_vcoms[idx].pMapBufCL = MapViewOfFile(g_vcoms[idx].hMapFileCL, FILE_MAP_ALL_ACCESS, 0, 0, 0);
      if (g_vcoms[idx].pMapBufCL) {
        g_vcoms[idx].hMapEventCL = OpenEvent(EVENT_ALL_ACCESS, FALSE, mapEventName);
          if (!g_vcoms[idx].hMapEventCL) {
            UnmapViewOfFile(g_vcoms[idx].pMapBufCL);
            g_vcoms[idx].pMapBufCL = NULL;
            CloseHandle(g_vcoms[idx].hMapFileCL);
            g_vcoms[idx].hMapFileCL = NULL;
            DeregisterDevice(g_vcoms[idx].hVcomCL);
            g_vcoms[idx].hVcomCL = NULL;
          }
          else {
            return idx;
          }
      }
      else {
        CloseHandle(g_vcoms[idx].hMapFileCL);
        g_vcoms[idx].hMapFileCL = NULL;
        DeregisterDevice(g_vcoms[idx].hVcomCL);
        g_vcoms[idx].hVcomCL = NULL;
      }
    }
    else {
      DeregisterDevice(g_vcoms[idx].hVcomCL);
      g_vcoms[idx].hVcomCL = NULL;
    }
  }
  else {
    g_vcoms[idx].hVcomCL = NULL;
  }
  return 0;
}

VCOM_API int VCOM_DeInit (DWORD idx) {
  if (g_vcoms[idx].hVcomCL) {
    DeactivateDevice(g_vcoms[idx].hVcomCL);
    VCOMRegistryDel(idx);
    g_vcoms[idx].pMapBufCL = NULL;
  }
  if (g_vcoms[idx].pMapBufCL) {
    UnmapViewOfFile(g_vcoms[idx].pMapBufCL);
    g_vcoms[idx].pMapBufCL = NULL;
  }
  if (g_vcoms[idx].hMapFileCL) {
    CloseHandle(g_vcoms[idx].hMapFileCL);
    g_vcoms[idx].hMapFileCL = NULL;
  }
  if (g_vcoms[idx].hMapEventCL) {
    CloseHandle(g_vcoms[idx].hMapEventCL);
    g_vcoms[idx].hMapEventCL = NULL;
  }
  return 0;
}

VCOM_API int VCOM_Write (DWORD idx, const BYTE *msg, size_t length) {
  BYTE buf[255] = {0};
  buf[0] = length > 254 ? 254 : length;
  memcpy(&buf[1], &msg[0], buf[0]);
  memcpy(g_vcoms[idx].pMapBufCL, &buf[0], buf[0]+1);
  SetEvent(g_vcoms[idx].hMapEventCL);
  return buf[0];
}

#pragma endregion vcom

#pragma region driver_exports

VCOM_DRIVER_API DWORD COM_Init (LPCTSTR pContext, LPCVOID lpvBusContext) {
  wchar_t mapFileName[MAX_PATH] = {0};
  wchar_t mapEventName[MAX_PATH] = {0};
  memcpy(&g_devIdx, lpvBusContext, sizeof(DWORD));
  Log("Init start %ls %d", pContext, g_devIdx);
  g_hFileMap = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 256, mapFileName);
  if (g_hFileMap) {
    Log("FileMapping");
    g_lpMemMap = MapViewOfFile(g_hFileMap, FILE_MAP_READ, 0, 0, 0);
    if (g_lpMemMap) {
      Log("MapView");
      g_hMapEvent = CreateEvent(NULL, FALSE, FALSE, mapEventName);
        if (g_hMapEvent) {
          Log("Event");
          InitializeCriticalSection(&g_devBufOperation);
          DevSharedReaderThread = CreateThread(NULL, 0, &SharedReaderThreadProc, NULL, 0, NULL);
            if (DevSharedReaderThread) {
              Log("Init done");
              return DEVICE_CONTEXT;
            }
        }
    }
  }
  Log("Init failed, err=%d", GetLastError());

  return 0;
}

VCOM_DRIVER_API BOOL COM_Deinit (DWORD hDeviceContext) {
  if (hDeviceContext != DEVICE_CONTEXT) {
    Log("False Deinst");
    return FALSE;
  }
  DeleteCriticalSection(&g_devBufOperation);
  CloseHandle(g_hMapEvent);
  UnmapViewOfFile(g_lpMemMap);
  CloseHandle(g_hFileMap);
  CloseHandle(g_hFile);

  Log("DeInit");
  return TRUE;
}

VCOM_DRIVER_API DWORD COM_Open (DWORD hDeviceContext, DWORD AccessCode, DWORD ShareMode) {
  if (hDeviceContext != DEVICE_CONTEXT) {
    Log("False Open");
    return 0;
  }

  if (g_lpMemMap != NULL && g_lpMemMap != INVALID_HANDLE_VALUE) {
    Log("Open done");
    return DEVICE_OPEN_CONTEXT;
  }
  return 0;
}

VCOM_DRIVER_API BOOL COM_Close (DWORD hOpenContext) {
  if (hOpenContext != DEVICE_OPEN_CONTEXT) {
    Log("False Close");
    return 0;
  }

  Log("Close");

  return TRUE;
}

VCOM_API BOOL COM_IOControl (DWORD hOpenContext, DWORD dwCode, PBYTE pBufIn, DWORD dwLenIn, PBYTE pBufOut, DWORD dwLenOut, PDWORD pdwActualOut) {
  if (hOpenContext != DEVICE_OPEN_CONTEXT) {
    Log("False IOControl");
    return 0;
  }

  switch (dwCode) {
    case IOCTL_SERIAL_SET_TIMEOUTS: {
      COMMTIMEOUTS* ct = (COMMTIMEOUTS*)pBufIn;
      Log("IOCTL_SERIAL_SET_TIMEOUTS: %d, %d, %d, %d, %d", ct->ReadIntervalTimeout, ct->ReadTotalTimeoutConstant,
        ct->ReadTotalTimeoutMultiplier, ct->WriteTotalTimeoutConstant, ct->WriteTotalTimeoutMultiplier);
      } break;
    case IOCTL_SERIAL_GET_TIMEOUTS:
      *pdwActualOut = sizeof(COMMTIMEOUTS);
      break;
    case IOCTL_SERIAL_GET_DCB:
      Log("IOCTL_SERIAL_GET_DCB");
      (DCB*)pBufOut = &g_DCB;
      *pdwActualOut = sizeof(DCB);
      break;
    case IOCTL_SERIAL_SET_DCB:
      Log("IOCTL_SERIAL_SET_DCB");
      break;
    case IOCTL_SERIAL_PURGE:
      Log("IOCTL_SERIAL_PURGE");
      break;
    case IOCTL_SERIAL_GET_PROPERTIES:
      *pdwActualOut = sizeof(COMMPROP);
      break;
    case IOCTL_SERIAL_GET_COMMSTATUS:
      Log("IOCTL_SERIAL_GET_COMMSTATUS");
      ((SERIAL_DEV_STATUS*)pBufOut)->ComStat.cbInQue = g_devBufLen;
      *pdwActualOut = sizeof(SERIAL_DEV_STATUS);
      break;
    case IOCTL_SERIAL_SET_WAIT_MASK:
      Log("IOCTL_SERIAL_SET_WAIT_MASK");
      g_devWaitMask = (DWORD)*pBufIn;
      return FALSE;
      break;
    case IOCTL_SERIAL_GET_WAIT_MASK:
      Log("IOCTL_SERIAL_GET_WAIT_MASK");
      (DWORD*)pBufOut = &g_devWaitMask;
      *pdwActualOut = sizeof(DWORD);
      break;
    case IOCTL_SERIAL_WAIT_ON_MASK:
      Log("IOCTL_SERIAL_WAIT_ON_MASK");
      return FALSE;
      break;
    case IOCTL_SERIAL_CLR_DTR:
      Log("IOCTL_SERIAL_CLR_DTR");
      break;
    case IOCTL_SERIAL_CLR_RTS:
      Log("IOCTL_SERIAL_CLR_RTS");
      break;
    case IOCTL_SERIAL_GET_MODEMSTATUS:
      Log("IOCTL_SERIAL_GET_MODEMSTATUS");
      *pdwActualOut = sizeof(DWORD);
      break;
    case IOCTL_SERIAL_SET_BREAK_OFF:
      Log("IOCTL_SERIAL_SET_BREAK_OFF");
      break;
    case IOCTL_SERIAL_SET_BREAK_ON:
      Log("IOCTL_SERIAL_SET_BREAK_ON");
      break;
    case IOCTL_SERIAL_SET_QUEUE_SIZE:
      Log("IOCTL_SERIAL_SET_QUEUE_SIZE");
      break;
    case IOCTL_SERIAL_SET_XOFF:
      Log("IOCTL_SERIAL_SET_XOFF");
      break;
    case IOCTL_SERIAL_SET_XON:
      Log("IOCTL_SERIAL_SET_XON");
      break;
  }

  return TRUE;
}

VCOM_DRIVER_API void COM_PowerUp (DWORD hDeviceContext) {
  Log("Power Up %d", hDeviceContext);
}

VCOM_DRIVER_API void COM_PowerDown(DWORD hDeviceContext) {
  Log("Power Down %d", hDeviceContext);
}

VCOM_DRIVER_API DWORD COM_Read (DWORD hOpenContext, LPVOID pBuffer, DWORD Count) {
  DWORD dwBytes = 0;
  BOOL wait = TRUE;
  if (hOpenContext != DEVICE_OPEN_CONTEXT) {
    Log("False Read");
    return 0;
  }
  if (Count > g_devBufLen) {//primitive timeout
    Sleep(ct_ReadIntervalTimeout);
  }
  EnterCriticalSection(&g_devBufOperation);
  dwBytes = Count > g_devBufLen ? g_devBufLen : Count;
  g_devBufLen = g_devBufLen-dwBytes;
  memcpy(pBuffer, &g_devBuf[0], dwBytes);
  memcpy(&g_devBuf[0], &g_devBuf[dwBytes], DEV_RX_BUF_SIZE-dwBytes);
  LeaveCriticalSection(&g_devBufOperation);

  //Log("Read max=%u, actual=%u", Count, dwBytes);

  return dwBytes;
}

VCOM_DRIVER_API DWORD COM_Write (DWORD hOpenContext, LPCVOID pBuffer, DWORD Count) {
  DWORD dwBytes = Count;
  if (hOpenContext != DEVICE_OPEN_CONTEXT) {
    Log("False Write");
    return 0;
  }

  Log("Write bytes=%u, {%s}", dwBytes, pBuffer);

  return dwBytes;
}

VCOM_DRIVER_API DWORD COM_Seek (DWORD hOpenContext, long Amount, WORD Type) {

  Log("Seek Amount=%u, Type=%u", Amount, Type);

  return 0;
}

#pragma endregion driver_exports
