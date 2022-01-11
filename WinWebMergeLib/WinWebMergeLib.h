#pragma once

#include <Windows.h>
#include <wtypes.h>

struct IWebMergeWindow
{
	virtual bool OpenUrls(const wchar_t* url1, const wchar_t* url2) = 0;
	virtual bool OpenUrls(const wchar_t* url1, const wchar_t* url2, const wchar_t* url3) = 0;
	virtual int  GetPaneCount() const = 0;
	virtual RECT GetPaneWindowRect(int pane) const = 0;
	virtual RECT GetWindowRect() const = 0;
	virtual bool SetWindowRect(const RECT& rc) = 0;
	virtual bool SaveScreenshot(int pane, const wchar_t* filename) = 0;
	virtual bool SaveHTML(int pane, const wchar_t* filename) = 0;
};

extern "C"
{
	IWebMergeWindow* WinWebMerge_CreateWindow(HINSTANCE hInstance, HWND hWndParent, int nID = 0);
	bool WinWebMerge_DestroyWindow(IWebMergeWindow* pWebMergeWindow);
};

