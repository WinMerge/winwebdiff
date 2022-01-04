#pragma once

#include <Windows.h>
#include "WebWindow.hpp"
#include "WinWebMergeLib.h"
#include <vector>

class CWebMergeWindow : public IWebMergeWindow
{
public:
	CWebMergeWindow()
		: m_nPanes(0)
		, m_hWnd(NULL)
		, m_hInstance(NULL)
		, m_nDraggingSplitter(-1)
		, m_bHorizontalSplit(false)
		, m_oldSplitPosX(-4)
		, m_oldSplitPosY(-4)
		, m_bDragging(false)
	{
	}

	bool Create(HINSTANCE hInstance, HWND hWndParent, int nID, const RECT& rc)
	{
		m_hInstance = hInstance;
		MyRegisterClass(hInstance);
		m_hWnd = CreateWindowExW(0, L"WinWebMergeWindowClass", NULL, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
			rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, hWndParent, reinterpret_cast<HMENU>((intptr_t)nID), hInstance, this);
		return m_hWnd ? true : false;
	}

	bool Destroy()
	{
		BOOL bSucceeded = true;
		if (m_hWnd)
			bSucceeded = DestroyWindow(m_hWnd);
		m_hWnd = NULL;
		return !!bSucceeded;
	}

	bool OpenUrls(const wchar_t* url1, const wchar_t* url2) override
	{
		return true;
	}

	bool OpenUrls(const wchar_t* url1, const wchar_t* url2, const wchar_t* url3) override
	{
		return true;
	}

	int GetPaneCount() const override
	{
		return m_nPanes;
	}

	RECT GetPaneWindowRect(int pane) const override
	{
		if (pane < 0 || pane >= m_nPanes || !m_hWnd)
		{
			RECT rc = { -1, -1, -1, -1 };
			return rc;
		}
		return m_webWindow[pane].GetWindowRect();
	}

	RECT GetWindowRect() const override
	{
		if (!m_hWnd)
			return RECT{ 0 };
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

	bool SetWindowRect(const RECT& rc) override
	{
		if (m_hWnd)
			MoveWindow(m_hWnd, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, TRUE);
		return true;
	}

private:

	std::vector<RECT> CalcChildWebWindowRect(HWND hWnd, int nPanes, bool bHorizontalSplit)
	{
		std::vector<RECT> childrects;
		RECT rcParent;
		GetClientRect(hWnd, &rcParent);
		RECT rc = rcParent;
		if (nPanes > 0)
		{
			if (!bHorizontalSplit)
			{
				int cx = GetSystemMetrics(SM_CXVSCROLL);
				int width = (rcParent.left + rcParent.right - cx) / nPanes - 2;
				rc.left = 0;
				rc.right = rc.left + width;
				for (int i = 0; i < nPanes - 1; ++i)
				{
					childrects.push_back(rc);
					rc.left = rc.right + 2 * 2;
					rc.right = rc.left + width;
				}
				rc.right = rcParent.right;
				rc.left = rc.right - width - cx;
				childrects.push_back(rc);
			}
			else
			{
				int cy = GetSystemMetrics(SM_CXVSCROLL);
				int height = (rcParent.top + rcParent.bottom - cy) / nPanes - 2;
				rc.top = 0;
				rc.bottom = rc.top + height;
				for (int i = 0; i < nPanes - 1; ++i)
				{
					childrects.push_back(rc);
					rc.top = rc.bottom + 2 * 2;
					rc.bottom = rc.top + height;
				}
				rc.bottom = rcParent.bottom;
				rc.top = rc.bottom - height - cy;
				childrects.push_back(rc);
			}
		}
		return childrects;
	}

	void MoveSplitter(int x, int y)
	{
		RECT rcParent;
		GetClientRect(m_hWnd, &rcParent);

		RECT rc[3];
		for (int i = 0; i < m_nPanes; ++i)
			rc[i] = m_webWindow[i].GetWindowRect();

		if (!m_bHorizontalSplit)
		{
			int minx = rc[m_nDraggingSplitter].left + 32;
			int maxx = rc[m_nDraggingSplitter + 1].right - 32;
			if (x < minx)
				rc[m_nDraggingSplitter].right = minx;
			else if (x > maxx)
				rc[m_nDraggingSplitter].right = maxx;
			else
				rc[m_nDraggingSplitter].right = x;
			for (int i = m_nDraggingSplitter + 1; i < m_nPanes; ++i)
			{
				int width = rc[i].right - rc[i].left;
				rc[i].left = rc[i - 1].right + 2 * 2;
				rc[i].right = rc[i].left + width;
			}
			rc[m_nPanes - 1].right = rcParent.right;
		}
		else
		{
			rc[m_nDraggingSplitter].bottom = y;
			int miny = rc[m_nDraggingSplitter].top + 32;
			int maxy = rc[m_nDraggingSplitter + 1].bottom - 32;
			if (y < miny)
				rc[m_nDraggingSplitter].bottom = miny;
			else if (y > maxy)
				rc[m_nDraggingSplitter].bottom = maxy;
			else
				rc[m_nDraggingSplitter].bottom = y;
			for (int i = m_nDraggingSplitter + 1; i < m_nPanes; ++i)
			{
				int height = rc[i].bottom - rc[i].top;
				rc[i].top = rc[i - 1].bottom + 2 * 2;
				rc[i].bottom = rc[i].top + height;
			}
			rc[m_nPanes - 1].bottom = rcParent.bottom;
		}

		for (int i = 0; i < m_nPanes; ++i)
			m_webWindow[i].SetWindowRect(rc[i]);
	}

	void DrawXorBar(HDC hdc, int x1, int y1, int width, int height)
	{
		static const WORD _dotPatternBmp[8] =
		{
			0x00aa, 0x0055, 0x00aa, 0x0055,
			0x00aa, 0x0055, 0x00aa, 0x0055
		};

		HBITMAP hbm = CreateBitmap(8, 8, 1, 1, _dotPatternBmp);
		HBRUSH hbr = CreatePatternBrush(hbm);

		SetBrushOrgEx(hdc, x1, y1, 0);
		HBRUSH hbrushOld = (HBRUSH)SelectObject(hdc, hbr);

		PatBlt(hdc, x1, y1, width, height, PATINVERT);

		SelectObject(hdc, hbrushOld);

		DeleteObject(hbr);
		DeleteObject(hbm);
	}

	void OnCreate(HWND hWnd, LPCREATESTRUCT lpCreateStruct)
	{
	}

	void OnSize(UINT nType, int cx, int cy)
	{
		std::vector<RECT> rects = CalcChildWebWindowRect(m_hWnd, m_nPanes, m_bHorizontalSplit);
		for (int i = 0; i < m_nPanes; ++i)
			m_webWindow[i].SetWindowRect(rects[i]);
	}

	void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
	{
	}

	void OnLButtonDown(UINT nFlags, int x, int y)
	{
		int i;
		for (i = 0; i < m_nPanes - 1; ++i)
		{
			if (!m_bHorizontalSplit)
			{
				if (x < m_webWindow[i + 1].GetWindowRect().left)
					break;
			}
			else
			{
				if (y < m_webWindow[i + 1].GetWindowRect().top)
					break;
			}
		}
		m_oldSplitPosX = -4;
		m_oldSplitPosY = -4;
		m_nDraggingSplitter = i;
		SetCapture(m_hWnd);
	}

	void OnLButtonUp(UINT nFlags, int x, int y)
	{
		if (m_nDraggingSplitter == -1)
			return;
		ReleaseCapture();
		HDC hdc = GetWindowDC(m_hWnd);
		if (!m_bHorizontalSplit)
			DrawXorBar(hdc, m_oldSplitPosX - 2, 1, 4, m_webWindow[0].GetWindowRect().bottom);
		else
			DrawXorBar(hdc, 1, m_oldSplitPosY - 2, m_webWindow[0].GetWindowRect().right, 4);
		ReleaseDC(m_hWnd, hdc);
		MoveSplitter(x, y);
		m_nDraggingSplitter = -1;
	}

	void OnMouseMove(UINT nFlags, int x, int y)
	{
		if (m_nPanes < 2)
			return;
		if (m_nDraggingSplitter == -1)
			return;
		HDC hdc = GetWindowDC(m_hWnd);
		if (!m_bHorizontalSplit)
		{
			DrawXorBar(hdc, m_oldSplitPosX - 2, 1, 4, m_webWindow[0].GetWindowRect().bottom);
			DrawXorBar(hdc, x - 2, 1, 4, m_webWindow[0].GetWindowRect().bottom);
		}
		else
		{
			DrawXorBar(hdc, 1, m_oldSplitPosY - 2, m_webWindow[0].GetWindowRect().right, 4);
			DrawXorBar(hdc, 1, y - 2, m_webWindow[0].GetWindowRect().right, 4);
		}
		m_oldSplitPosX = x;
		m_oldSplitPosY = y;
		ReleaseDC(m_hWnd, hdc);
	}

	LRESULT OnWndMsg(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
	{
		switch (iMsg)
		{
		case WM_CREATE:
			OnCreate(hwnd, (LPCREATESTRUCT)lParam);
			break;
		case WM_COMMAND:
			PostMessage(GetParent(m_hWnd), iMsg, wParam, lParam);
			break;
		case WM_SIZE:
			OnSize((UINT)wParam, LOWORD(lParam), HIWORD(lParam));
			break;
		case WM_KEYDOWN:
			OnKeyDown((UINT)wParam, (int)(short)LOWORD(lParam), (UINT)HIWORD(lParam));
			break;
		case WM_LBUTTONDOWN:
			OnLButtonDown((UINT)(wParam), (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam));
			break;
		case WM_LBUTTONUP:
			OnLButtonUp((UINT)(wParam), (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam));
			break;
		case WM_MOUSEMOVE:
			OnMouseMove((UINT)(wParam), (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam));
			break;
		case WM_SETCURSOR:
			SetCursor(::LoadCursor(nullptr, !m_bHorizontalSplit ? IDC_SIZEWE : IDC_SIZENS));
			break;
		default:
			return DefWindowProc(hwnd, iMsg, wParam, lParam);
		}
		return 0;
	}

	static LRESULT CALLBACK WndProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
	{
		if (iMsg == WM_NCCREATE)
			SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<LPCREATESTRUCT>(lParam)->lpCreateParams));
		CWebMergeWindow* pWebWnd = reinterpret_cast<CWebMergeWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
		LRESULT lResult = pWebWnd->OnWndMsg(hwnd, iMsg, wParam, lParam);
		return lResult;
	}

	ATOM MyRegisterClass(HINSTANCE hInstance)
	{
		WNDCLASSEXW wcex = { 0 };
		wcex.cbSize = sizeof(WNDCLASSEX);
		wcex.style = CS_HREDRAW | CS_VREDRAW;
		wcex.lpfnWndProc = (WNDPROC)CWebMergeWindow::WndProc;
		wcex.cbClsExtra = 0;
		wcex.cbWndExtra = 0;
		wcex.hInstance = hInstance;
		wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
		wcex.hbrBackground = (HBRUSH)(COLOR_3DFACE + 1);
		wcex.lpszClassName = L"WinWebMergeWindowClass";
		return RegisterClassExW(&wcex);
	}

	int m_nPanes;
	HWND m_hWnd;
	HINSTANCE m_hInstance;
	CWebWindow m_webWindow[3];
	int m_nDraggingSplitter;
	bool m_bHorizontalSplit;
	int m_oldSplitPosX;
	int m_oldSplitPosY;
	bool m_bDragging;
	POINT m_ptOrg;
	POINT m_ptPrev;
};
