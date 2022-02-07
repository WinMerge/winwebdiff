// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include "WebDiffWindow.hpp"

extern "C" IWebDiffWindow *
WinWebDiff_CreateWindow(HINSTANCE hInstance, HWND hWndParent, int nID)
{
    RECT rc = { 0 };
    CWebDiffWindow* pWebDiffWindow = new CWebDiffWindow();
    pWebDiffWindow->Create(hInstance, hWndParent, nID, rc);
    return static_cast<IWebDiffWindow*>(pWebDiffWindow);
}

extern "C" bool
WinWebDiff_DestroyWindow(IWebDiffWindow * pWebDiffWindow)
{
    CWebDiffWindow* pWebDiffWindow2 = static_cast<CWebDiffWindow*>(pWebDiffWindow);
    pWebDiffWindow2->Destroy();
    delete pWebDiffWindow2;
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

