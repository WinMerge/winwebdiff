// WinWebDiffLib.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include "WebDiffWindow.hpp"
#include "WebToolWindow.hpp"

extern "C" IWebDiffWindow *
WinWebDiff_CreateWindow(HINSTANCE hInstance, HWND hWndParent, int nID)
{
    RECT rc{};
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

extern "C" IWebToolWindow *
WinWebDiff_CreateToolWindow(HINSTANCE hInstance, HWND hWndParent, IWebDiffWindow * pWebDiffWindow)
{
    CWebToolWindow* pWebToolWindow = new CWebToolWindow();
    pWebToolWindow->Create(hInstance, hWndParent);
    pWebToolWindow->SetWebDiffWindow(pWebDiffWindow);
    return static_cast<IWebToolWindow*>(pWebToolWindow);
}

extern "C" bool
WinWebDiff_DestroyToolWindow(IWebToolWindow * pWebToolWindow)
{
    CWebToolWindow* pWebToolWindow2 = static_cast<CWebToolWindow*>(pWebToolWindow);
    pWebToolWindow2->Destroy();
    delete pWebToolWindow2;
    return true;
}

BOOL APIENTRY
DllMain( HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
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

