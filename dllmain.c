/*****************************************************************************
The MIT License

Copyright (c) 2008 Holger Kuhn (hawkear@gmx.de)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*****************************************************************************/

#include "buzzer.h"
#include <windows.h>
//#include <stdio.h>
//#include <stdlib.h>


#include <Windows.h>
extern "C"
{
#include <hidpi.h>   	
#include <setupapi.h>
#include <hidsdi.h>
}


typedef enum {
  BUZZER_NONE = 0,
  BUZZER_SONY_BUZZ = 1
} Buzzer_Type;

typedef struct
{
  Buzzer_Type Type;
  char        Buttons;
  char*       Path;
  HANDLE      ReadHandle;
  HANDLE      WriteHandle;
  OVERLAPPED  Overlapped;
} Buzzers;

Buzzers BuzzerList[BUZZER_MAXBUZZER];
BOOL ThreadRunning = FALSE;
BOOL StopThread = FALSE;
unsigned char rbuf[6];
DWORD read;

DWORD WINAPI readThread(LPVOID arg);

DLLIMPORT void BuzzerClose(void) {
  int i;

  StopThread = TRUE;  
  for(i=0;i<BUZZER_MAXBUZZER;i++){
    if(BUZZER_SONY_BUZZ == BuzzerList[i].Type) {
      if (NULL != BuzzerList[i].Path) {
        free(BuzzerList[i].Path);
        BuzzerList[i].Path = NULL;
      }
      if(INVALID_HANDLE_VALUE != BuzzerList[i].Overlapped.hEvent) {
      CloseHandle(BuzzerList[i].Overlapped.hEvent);
      BuzzerList[i].Overlapped.hEvent = INVALID_HANDLE_VALUE;
      }
    }
    if(INVALID_HANDLE_VALUE != BuzzerList[i].WriteHandle) {
      CloseHandle(BuzzerList[i].WriteHandle);
      BuzzerList[i].WriteHandle = INVALID_HANDLE_VALUE;
    }
    if(INVALID_HANDLE_VALUE != BuzzerList[i].ReadHandle) {
      CloseHandle(BuzzerList[i].ReadHandle);
      BuzzerList[i].ReadHandle = INVALID_HANDLE_VALUE;
    }
    BuzzerList[i].Type = BUZZER_NONE;
    BuzzerList[i].Buttons = 0;
  }
}

DLLIMPORT int BuzzerOpen(void) {
  int i;
  int BuzzerCount = 0;
  int j;
  DWORD size;
  GUID hidGuid;
  SP_DEVICE_INTERFACE_DATA DeviceInterface;
  SP_DEVICE_INTERFACE_DETAIL_DATA *deviceDetails = NULL;
  HDEVINFO DeviceInfoList;
  HIDD_ATTRIBUTES attr;
  HidD_GetHidGuid(&hidGuid);
  PHIDP_PREPARSED_DATA preparsed;
  HIDP_CAPS caps;

  DeviceInfoList = SetupDiGetClassDevs (&hidGuid, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
  
  DeviceInterface.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
  
  for(i=0;;i++){
    if(!SetupDiEnumDeviceInterfaces(DeviceInfoList, 0, &hidGuid, i, &DeviceInterface))
      break;
     
    SetupDiGetDeviceInterfaceDetail(DeviceInfoList, &DeviceInterface, NULL, 0, &size, NULL);
    
    if(deviceDetails != NULL) {
      free(deviceDetails);
      deviceDetails = NULL;
    }
    
    deviceDetails = malloc(size);
    deviceDetails->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
    
    SetupDiGetDeviceInterfaceDetail(DeviceInfoList, &DeviceInterface, deviceDetails, size, &size, NULL);
    
    if(INVALID_HANDLE_VALUE != BuzzerList[BuzzerCount].WriteHandle) {
      CloseHandle(BuzzerList[BuzzerCount].WriteHandle);
      BuzzerList[BuzzerCount].WriteHandle = INVALID_HANDLE_VALUE;
    }
    if(INVALID_HANDLE_VALUE != BuzzerList[BuzzerCount].ReadHandle) {
      CloseHandle(BuzzerList[BuzzerCount].ReadHandle);
      BuzzerList[BuzzerCount].ReadHandle = INVALID_HANDLE_VALUE;
    }
    BuzzerList[BuzzerCount].WriteHandle = CreateFile(deviceDetails->DevicePath, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if(INVALID_HANDLE_VALUE == BuzzerList[BuzzerCount].WriteHandle)
      continue;

    attr.Size = sizeof(HIDD_ATTRIBUTES);
    HidD_GetAttributes(BuzzerList[BuzzerCount].WriteHandle, &attr);


    // Namtai 0x054c 0x1000 or Logitech 0x054c 0x0002
    if(attr.VendorID == 0x054c && (attr.ProductID == 0x1000 || attr.ProductID == 0x0002)) {
      BuzzerList[BuzzerCount].Path = malloc(size);

      BuzzerList[BuzzerCount].ReadHandle = CreateFile(deviceDetails->DevicePath, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
      BuzzerList[BuzzerCount].Overlapped.hEvent = CreateEvent(NULL, TRUE, TRUE, "");;
      BuzzerList[BuzzerCount].Overlapped.Offset = 0;
      BuzzerList[BuzzerCount].Overlapped.OffsetHigh = 0;
      ReadFile(BuzzerList[BuzzerCount].ReadHandle, rbuf, 6, &read, (LPOVERLAPPED) &BuzzerList[BuzzerCount].Overlapped);

      strcpy(BuzzerList[BuzzerCount].Path,deviceDetails->DevicePath);
      for (j=0;BuzzerCount<BUZZER_MAXBUZZER&&j<4;j++) {
        BuzzerList[BuzzerCount].Type = BUZZER_SONY_BUZZ;
        BuzzerCount++;
      }
      continue;
    }

    // Any Generic Joystick with 6 Bytes Input and 8 Bytes Output
    if(HidD_GetPreparsedData(BuzzerList[BuzzerCount].WriteHandle, &preparsed)) {
      if (HIDP_STATUS_SUCCESS == HidP_GetCaps(preparsed,&caps)) {
        if (0x04 == caps.Usage && 0x01 == caps.UsagePage && 0x06 == caps.InputReportByteLength && 0x08 == caps.OutputReportByteLength) {
          BuzzerList[BuzzerCount].Path = malloc(size);            

          BuzzerList[BuzzerCount].ReadHandle = CreateFile(deviceDetails->DevicePath, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
          BuzzerList[BuzzerCount].Overlapped.hEvent = CreateEvent(NULL, TRUE, TRUE, "");;
          BuzzerList[BuzzerCount].Overlapped.Offset = 0;
          BuzzerList[BuzzerCount].Overlapped.OffsetHigh = 0;
          ReadFile(BuzzerList[BuzzerCount].ReadHandle, rbuf, 6, &read, (LPOVERLAPPED) &BuzzerList[BuzzerCount].Overlapped);

          strcpy(BuzzerList[BuzzerCount].Path,deviceDetails->DevicePath);
          for (j=0;BuzzerCount<BUZZER_MAXBUZZER&&j<4;j++) {
            BuzzerList[BuzzerCount].Type = BUZZER_SONY_BUZZ;
            BuzzerCount++;
          }
          continue;               
        } 
      }
    }
    HidD_FreePreparsedData(preparsed);		 
  }
    
  SetupDiDestroyDeviceInfoList (DeviceInfoList);
  if(deviceDetails != NULL) {
    free(deviceDetails);
    deviceDetails = NULL;
  }
  StopThread = FALSE;
  if (BuzzerCount && !ThreadRunning) {
    if(CreateThread(NULL, 1024, readThread, NULL, 0, NULL) == NULL)
      BuzzerCount = 0;
    if (BuzzerCount)
      while(!ThreadRunning) ;       
  }

  return BuzzerCount;
}

DWORD WINAPI readThread(LPVOID arg) {
  int i;
  DWORD	Result;

  ThreadRunning = TRUE;
  while (!StopThread) {
    for(i=0;i<BUZZER_MAXBUZZER;i++) {
      if(BUZZER_SONY_BUZZ == BuzzerList[i].Type && INVALID_HANDLE_VALUE != BuzzerList[i].ReadHandle) {
        if (WAIT_OBJECT_0 == WaitForSingleObject(BuzzerList[i].Overlapped.hEvent, 0)) {
          GetOverlappedResult(BuzzerList[i].ReadHandle, (LPOVERLAPPED) &BuzzerList[i].Overlapped,&read,FALSE);
          if (6 == read) {
            ReadFile(BuzzerList[i].ReadHandle, rbuf, 6, &read, (LPOVERLAPPED) &BuzzerList[i].Overlapped); 	            
            BuzzerList[i  ].Buttons = rbuf[3] & 0x1f;
            BuzzerList[i+1].Buttons = ((rbuf[3] >> 5) | (rbuf[4] << 3)) & 0x1f;
            BuzzerList[i+2].Buttons = (rbuf[4] >> 2) & 0x1f;
            BuzzerList[i+3].Buttons = ((rbuf[4] >> 7) | (rbuf[5] << 1)) & 0x1f;
          }
        }
        i += 3;
      }
    }
    sleep(5);
  }
  ThreadRunning = FALSE;
}

// Turn on the LED on all Buzzers, whose Value buffer[] is not 0
DLLIMPORT int BuzzerSetLEDs(char* buffer, int length) {
  int i;
  int error = 0;
  int BuzzCount = 0;
  char wbuf[8] = "\0\0\0\0\0\0\0\0";
  DWORD written;

  for(i=0;i<length&&i<BUZZER_MAXBUZZER&&!error;i++) {
    if(BUZZER_SONY_BUZZ == BuzzerList[i].Type) {            
      wbuf[2+BuzzCount] = buffer[i] ? 0xff : 0x00;
      BuzzCount++;
      if (BuzzCount >= 4) {
        if(0 == WriteFile(BuzzerList[i-3].WriteHandle, wbuf, 8, &written, NULL) || 8 != written) {
          CloseHandle(BuzzerList[i-3].WriteHandle);
          BuzzerList[i-3].WriteHandle = INVALID_HANDLE_VALUE;
          CloseHandle(BuzzerList[i-3].ReadHandle);
          BuzzerList[i-3].ReadHandle = INVALID_HANDLE_VALUE;
          error = BUZZER_ERROR_WRITEFILE;
        }
        BuzzCount = 0;
      }
    }
  }
  return error;   
}

DLLIMPORT int BuzzerGetButtons(char* buffer, int length) {
  int i;
  int error = 0;
  int BuzzCount = 0;
  for(i=0;i<length&&i<BUZZER_MAXBUZZER&&!error;i++) {
    if(BUZZER_SONY_BUZZ == BuzzerList[i].Type) {            
      if (0 == BuzzCount) {
        if (INVALID_HANDLE_VALUE == BuzzerList[i].WriteHandle || INVALID_HANDLE_VALUE == BuzzerList[i].ReadHandle)
          error = BUZZER_ERROR_READFILE;
      }
      BuzzCount++;
      if (BuzzCount >= 4)
        BuzzCount = 0;
    }
    buffer[i] = BuzzerList[i].Buttons;
  }
  return error;             
}

BOOL APIENTRY DllMain (HINSTANCE hInst, DWORD reason, LPVOID reserved) {
  int i;
  switch (reason)
  {
    case DLL_PROCESS_ATTACH:
      for(i=0;i<BUZZER_MAXBUZZER;i++){
        BuzzerList[i].WriteHandle = INVALID_HANDLE_VALUE;
        BuzzerList[i].ReadHandle = INVALID_HANDLE_VALUE;
        BuzzerList[i].Path = NULL;
        BuzzerList[i].Type = BUZZER_NONE;
        BuzzerList[i].Buttons = 0;
        BuzzerList[i].Overlapped.hEvent = INVALID_HANDLE_VALUE;
      }
      break;

    case DLL_PROCESS_DETACH:
      BuzzerClose();
      break;

    case DLL_THREAD_ATTACH:
      break;

    case DLL_THREAD_DETACH:
      break;
  }

  return TRUE;
}
