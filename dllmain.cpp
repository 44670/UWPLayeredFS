// A DLL that redirects file access

#include <Windows.h>
#include "mhook.h"
#include <stdio.h>
#include <winternl.h>

// link to ntdll
#pragma comment(lib, "ntdll.lib")

// This section shares between all processes
#pragma data_seg("myshared")
__declspec (allocate("myshared")) wchar_t targetProcessName[MAX_PATH];
#pragma data_seg()
#pragma comment(linker, "/SECTION:myshared,RWS")


typedef NTSTATUS(NTAPI *TrueNtCreateFile_t)(
    PHANDLE FileHandle, ACCESS_MASK DesiredAccess,
    POBJECT_ATTRIBUTES ObjectAttributes, PIO_STATUS_BLOCK IoStatusBlock,
    PLARGE_INTEGER AllocationSize, ULONG FileAttributes, ULONG ShareAccess,
    ULONG CreateDisposition, ULONG CreateOptions, PVOID EaBuffer,
    ULONG EaLength);

TrueNtCreateFile_t TrueNtCreateFile;
wchar_t myDllPath[MAX_PATH];
wchar_t layeredFSPath[MAX_PATH];
HMODULE hCurrentModule;

extern "C" void DbgPrintf(const char *format, ...) {
  va_list args;
  va_start(args, format);
  char buffer[1024];
  vsprintf_s(buffer, format, args);
  OutputDebugStringA(buffer);
  va_end(args);
}

extern "C" void DbgPrintfW(const wchar_t *format, ...) {
  va_list args;
  va_start(args, format);
  wchar_t buffer[1024];
  vswprintf_s(buffer, format, args);
  OutputDebugStringW(buffer);
  va_end(args);
}

// Hooked NtCreateFile function
NTSTATUS NTAPI HookedNtCreateFile(PHANDLE FileHandle, ACCESS_MASK DesiredAccess,
                                  POBJECT_ATTRIBUTES ObjectAttributes,
                                  PIO_STATUS_BLOCK IoStatusBlock,
                                  PLARGE_INTEGER AllocationSize,
                                  ULONG FileAttributes, ULONG ShareAccess,
                                  ULONG CreateDisposition, ULONG CreateOptions,
                                  PVOID EaBuffer, ULONG EaLength) {
  UNICODE_STRING tmpUnicodeStr;

  wchar_t tmpPath[MAX_PATH];
  wchar_t fileName[MAX_PATH];
  // Copy the UNICODE_STRING to a wchar_t array by its length
  wcsncpy_s(fileName, MAX_PATH, ObjectAttributes->ObjectName->Buffer,
            ObjectAttributes->ObjectName->Length / sizeof(wchar_t));

  DbgPrintfW(L"[L] NtCreateFile: %s\n", fileName);

  // Get last part of the file name
  wchar_t *lastPart = wcsrchr(fileName, L'\\');
  if (lastPart == NULL) {
    lastPart = fileName;
  } else {
    lastPart++;
  }
  // check lastPart Length
  if (wcslen(lastPart) > 0) {
    // tmpPath = layeredFSPath + lastPart
    wcscpy_s(tmpPath, MAX_PATH, layeredFSPath);
    wcscat_s(tmpPath, MAX_PATH, lastPart);
    // Check if tmpPath file exists
    if (GetFileAttributesW(tmpPath) != INVALID_FILE_ATTRIBUTES) {
      // fileName = "\\??\\" + tmpPath
      wcscpy_s(fileName, MAX_PATH, L"\\??\\");
      wcscat_s(fileName, MAX_PATH, tmpPath);
      
      // Build tmpUnicodeStr with fileName
      RtlInitUnicodeString(&tmpUnicodeStr, fileName);
      // Replace ObjectAttributes->ObjectName with tmpUnicodeStr
      ObjectAttributes->ObjectName = &tmpUnicodeStr;
      DbgPrintfW(L"[+] Redirected to: %s\n", fileName);
    }
  }

  NTSTATUS status = TrueNtCreateFile(
      FileHandle, DesiredAccess, ObjectAttributes, IoStatusBlock,
      AllocationSize, FileAttributes, ShareAccess, CreateDisposition,
      CreateOptions, EaBuffer, EaLength);
  return status;
}

int isApiHookInstalled = 0;

void OnProcessDetach(HMODULE hModule) {
  if (isApiHookInstalled) {
    Mhook_Unhook((PVOID *) &TrueNtCreateFile);
    DbgPrintfW(L"[-] Unhooked NtCreateFile\n");
    isApiHookInstalled = 0;
  }
}

void OnProcessAttach(HMODULE hModule) {
  // Get the path of the host exe file
  wchar_t exePath[MAX_PATH];
  GetModuleFileNameW(NULL, exePath, MAX_PATH);
  // Get the last path segment of exePath as process name
  wchar_t *processName = wcsrchr(exePath, L'\\');
  if (processName == NULL) {
    processName = exePath;
  } else {
    processName = processName + 1;
  }
  //DbgPrintfW(L"[L] Process name: %s\n", processName);
  // print target process name
  //DbgPrintfW(L"[L] Target process name: %s\n", targetProcessName);

  // Compare the process name with the target process name, ignore case
  if (_wcsicmp(processName, targetProcessName) == 0) {
    DbgPrintfW(L"[L] Target process found\n");
  } else {
    return;
  }

  // Get the full path of my DLL
  GetModuleFileNameW(hModule, myDllPath, MAX_PATH);
  DbgPrintfW(L"[L] DLL path: %s\n", myDllPath);

  // Copy the path to layeredFSPath
  wcscpy_s(layeredFSPath, myDllPath);

  // Find last "\\" and replace it with "\\layered"
  wchar_t *lastSlash = wcsrchr(layeredFSPath, L'\\');
  if (lastSlash) {
    wcscpy_s(lastSlash, MAX_PATH - (lastSlash - layeredFSPath), L"\\layered\\");
  }
  DbgPrintfW(L"[L] LayeredFS path: %s\n", layeredFSPath);

  // Hook NtCreateFile
  TrueNtCreateFile = (TrueNtCreateFile_t)GetProcAddress(
      GetModuleHandleW(L"ntdll.dll"), "NtCreateFile");
  Mhook_SetHook((PVOID *)&TrueNtCreateFile, HookedNtCreateFile);
  isApiHookInstalled = 1;
  DbgPrintfW(L"[+] Hooked NtCreateFile\n");
}

// DllMain function
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call,
                      LPVOID lpReserved) {
  switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
      hCurrentModule = hModule;
      DisableThreadLibraryCalls(hModule);

      OnProcessAttach(hModule);
      break;
    case DLL_PROCESS_DETACH:
      OnProcessDetach(hModule);
      break;
  }
  return TRUE;
}

HHOOK myHook;

// Define GetMsgProc function
LRESULT CALLBACK GetMsgProc(int code, WPARAM wParam, LPARAM lParam) {
  return CallNextHookEx(NULL, code, wParam, lParam);
}

extern "C" __declspec(dllexport) void SetProcessName(wchar_t* processName) {
  wcscpy_s(targetProcessName, MAX_PATH, processName);
}

// Setup WH_GETMESSAGE hook, this is a exported function of our DLL
extern "C" __declspec(dllexport) void InstallMsgHook() {
  // Hook WH_GETMESSAGE
  myHook = SetWindowsHookEx(WH_CBT, GetMsgProc, hCurrentModule, 0);
  if (myHook == NULL) {
    DbgPrintfW(L"[L] SetWindowsHookEx failed: %d\n", GetLastError());
  }
}

extern "C" __declspec(dllexport) void UninstallMsgHook(HMODULE hModule) {
  DbgPrintfW(L"[L] UninstallMsgHook\n");
  if (!UnhookWindowsHookEx(myHook)) {
    DbgPrintfW(L"[L] UnhookWindowsHookEx failed: %d\n", GetLastError());
  }

}