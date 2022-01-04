// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include "WebMergeWindow.hpp"

extern "C" IWebMergeWindow *
WinWebMerge_CreateWindow(HINSTANCE hInstance, HWND hWndParent, int nID)
{
    RECT rc = { 0 };
    CWebMergeWindow* pWebMergeWindow = new CWebMergeWindow();
    pWebMergeWindow->Create(hInstance, hWndParent, nID, rc);
    return static_cast<IWebMergeWindow*>(pWebMergeWindow);
}

extern "C" bool
WinWebMerge_DestroyWindow(IWebMergeWindow * pWebMergeWindow)
{
    CWebMergeWindow* pWebMergeWindow2 = static_cast<CWebMergeWindow*>(pWebMergeWindow);
    pWebMergeWindow2->Destroy();
    delete pWebMergeWindow2;
    return true;
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

