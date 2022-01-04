#pragma once

#include <Windows.h>

class CWebWindow
{
public:
	CWebWindow()
		: m_wBar{ SB_BOTH }
	{
	}

	~CWebWindow()
	{
	}

	HWND GetHWND() const
	{
		return m_hWnd;
	}

	bool Create(HINSTANCE hInstance, HWND hWndParent)
	{
		MyRegisterClass(hInstance);
		m_hWnd = CreateWindowExW(0, L"WinWebWindowClass", NULL, WS_CHILD | WS_HSCROLL | WS_VSCROLL | WS_VISIBLE,
			0, 0, 0, 0, hWndParent, NULL, hInstance, this);
		return m_hWnd ? true : false;
	}

	bool Destroy()
	{
		if (m_hWnd)
			DestroyWindow(m_hWnd);
		m_hWnd = NULL;
		return true;
	}

	RECT GetWindowRect() const
	{
		RECT rc, rcParent;
		HWND hwndParent = GetParent(m_hWnd);
		::GetWindowRect(hwndParent, &rcParent);
		::GetWindowRect(m_hWnd, &rc);
		rc.left -= rcParent.left;
		rc.top -= rcParent.top;
		rc.right -= rcParent.left;
		rc.bottom -= rcParent.top;
		return rc;
	}

	void SetWindowRect(const RECT& rc)
	{
		MoveWindow(m_hWnd, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, TRUE);
		if (m_wBar == SB_BOTH)
			::ShowScrollBar(m_hWnd, m_wBar, true);
		else if (m_wBar == SB_HORZ)
		{
			::ShowScrollBar(m_hWnd, SB_HORZ, true);
			::ShowScrollBar(m_hWnd, SB_VERT, false);
		}
		else if (m_wBar == SB_VERT)
		{
			::ShowScrollBar(m_hWnd, SB_HORZ, false);
			::ShowScrollBar(m_hWnd, SB_VERT, true);
		}
	}

	void SetFocus()
	{
		::SetFocus(m_hWnd);
	}

	void SetScrollBar(int wBar)
	{
		m_wBar = wBar;
	}

private:
	ATOM MyRegisterClass(HINSTANCE hInstance)
	{
		WNDCLASSEXW wcex = { 0 };
		if (!GetClassInfoEx(hInstance, L"WinWebWindowClass", &wcex))
		{
			wcex.cbSize = sizeof(WNDCLASSEX);
			wcex.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
			wcex.lpfnWndProc = (WNDPROC)WndProc;
			wcex.cbClsExtra = 0;
			wcex.cbWndExtra = 0;
			wcex.hInstance = hInstance;
			wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
			wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
			wcex.lpszClassName = L"WinWebWindowClass";
		}
		return RegisterClassExW(&wcex);
	}

	LRESULT OnWndMsg(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
	{
		switch (iMsg)
		{
		case WM_ERASEBKGND:
			return TRUE;
		default:
			return DefWindowProc(hwnd, iMsg, wParam, lParam);
		}
		return 0;
	}

	static LRESULT CALLBACK WndProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
	{
		if (iMsg == WM_NCCREATE)
			SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<LPCREATESTRUCT>(lParam)->lpCreateParams));
		CWebWindow* pWebWnd = reinterpret_cast<CWebWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
		LRESULT lResult = pWebWnd->OnWndMsg(hwnd, iMsg, wParam, lParam);
		return lResult;
	}

	HWND m_hWnd;
	int m_wBar;
};

