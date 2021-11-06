#include <Windows.h>
#include <stdio.h>
__declspec(dllimport) void SetProcessName(wchar_t* processName);
__declspec(dllimport) void InstallMsgHook();
__declspec(dllimport) void UninstallMsgHook();

// wmain
int wmain(int argc, wchar_t* argv[])
{
    if (argc <= 1) {
        // print argv[0]
        wprintf(L"Usage: %s [process-name-to-attach]\n", argv[0]);
        return 0;
    }
    wprintf(L"[+] Attach to process: %s\n", argv[1]);
    SetProcessName(argv[1]);
    InstallMsgHook();
    printf("[+] You can run your application now.\n[*] Press any key to exit...\n");
    getchar();
    UninstallMsgHook();
    printf("[*] Have a nice day!");
    return 0;
}