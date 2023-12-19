#include <WindowsX.h>
#include <CommCtrl.h>
#include <tchar.h>
#include "WebDiffWindow.hpp"
#include "WinWebDiffLib.h"
#include "resource.h"

#pragma once

class CWebToolWindow : public IWebToolWindow
{
public:
	CWebToolWindow() :
		  m_hWnd(NULL)
		, m_hInstance(NULL)
		, m_pWebDiffWindow(NULL)
		, m_bInSync(false)
	{
	}

	~CWebToolWindow()
	{
	}

	bool Create(HINSTANCE hInstance, HWND hWndParent)
	{
		m_hInstance = hInstance;
		m_hWnd = CreateDialogParam(hInstance, MAKEINTRESOURCE(IDD_DIALOGBAR), hWndParent, DlgProc, reinterpret_cast<LPARAM>(this));
		return m_hWnd ? true : false;
	}

	bool Destroy()
	{
		BOOL bSucceeded = DestroyWindow(m_hWnd);
		m_hWnd = NULL;
		return !!bSucceeded;
	}

	HWND GetHWND() const override
	{
		return m_hWnd;
	}

	void Sync() override
	{
		if (!m_pWebDiffWindow)
			return;

		m_bInSync = true;

		/*
		TCHAR buf[256];
		wsprintf(buf, _T("(%d)"), m_pWebDiffWindow->GetDiffBlockSize());
		SetDlgItemText(m_hWnd, IDC_DIFF_BLOCKSIZE_STATIC, buf);
		wsprintf(buf, _T("(%d)"), static_cast<int>(m_pWebDiffWindow->GetDiffColorAlpha() * 100));
		SetDlgItemText(m_hWnd, IDC_DIFF_BLOCKALPHA_STATIC, buf);
		wsprintf(buf, _T("(%d)"), static_cast<int>(m_pWebDiffWindow->GetColorDistanceThreshold()));
		SetDlgItemText(m_hWnd, IDC_DIFF_CDTHRESHOLD_STATIC, buf);
		wsprintf(buf, _T("(%d)"), static_cast<int>(m_pWebDiffWindow->GetOverlayAlpha() * 100));
		SetDlgItemText(m_hWnd, IDC_OVERLAY_ALPHA_STATIC, buf);
		wsprintf(buf, _T("(%d%%)"), static_cast<int>(100 * m_pWebDiffWindow->GetZoom()));
		SetDlgItemText(m_hWnd, IDC_ZOOM_STATIC, buf);

		SendDlgItemMessage(m_hWnd, IDC_DIFF_HIGHLIGHT, BM_SETCHECK, m_pWebDiffWindow->GetShowDifferences() ? BST_CHECKED : BST_UNCHECKED, 0);
		SendDlgItemMessage(m_hWnd, IDC_DIFF_BLOCKSIZE_SLIDER, TBM_SETPOS, TRUE, m_pWebDiffWindow->GetDiffBlockSize());
		SendDlgItemMessage(m_hWnd, IDC_DIFF_BLOCKALPHA_SLIDER, TBM_SETPOS, TRUE, static_cast<LPARAM>(m_pWebDiffWindow->GetDiffColorAlpha() * 100));
		SendDlgItemMessage(m_hWnd, IDC_DIFF_CDTHRESHOLD_SLIDER, TBM_SETPOS, TRUE, static_cast<LPARAM>(m_pWebDiffWindow->GetColorDistanceThreshold()));
		SendDlgItemMessage(m_hWnd, IDC_OVERLAY_ALPHA_SLIDER, TBM_SETPOS, TRUE, static_cast<LPARAM>(m_pWebDiffWindow->GetOverlayAlpha() * 100));
		SendDlgItemMessage(m_hWnd, IDC_ZOOM_SLIDER, TBM_SETPOS, TRUE, static_cast<LPARAM>(m_pWebDiffWindow->GetZoom() * 8 - 8));
		SendDlgItemMessage(m_hWnd, IDC_DIFF_INSERTION_DELETION_DETECTION_MODE, CB_SETCURSEL, m_pWebDiffWindow->GetInsertionDeletionDetectionMode(), 0);
		SendDlgItemMessage(m_hWnd, IDC_OVERLAY_MODE, CB_SETCURSEL, m_pWebDiffWindow->GetOverlayMode(), 0);
		SendDlgItemMessage(m_hWnd, IDC_PAGE_SPIN, UDM_SETRANGE, 0, MAKELONG(1, m_pWebDiffWindow->GetMaxPageCount()));
		SendDlgItemMessage(m_hWnd, IDC_PAGE_SPIN, UDM_SETPOS, 0, MAKELONG(m_pWebDiffWindow->GetCurrentMaxPage() + 1, 0));

		int w = static_cast<CWebDiffWindow *>(m_pWebDiffWindow)->GetDiffImageWidth();
		int h = static_cast<CWebDiffWindow *>(m_pWebDiffWindow)->GetDiffImageHeight();

		RECT rc;
		GetClientRect(m_hWnd, &rc);
		int cx = rc.right - rc.left;
		int cy = rc.bottom - rc.top;

		RECT rcTmp;
		HWND hwndDiffMap = GetDlgItem(m_hWnd, IDC_DIFFMAP);
		GetWindowRect(hwndDiffMap, &rcTmp);
		POINT pt = { rcTmp.left, rcTmp.top };
		ScreenToClient(m_hWnd, &pt);
		int mw = 0;
		int mh = 0;
		if (w > 0 && h > 0)
		{
			mh = h * (cx - 8) / w;
			if (mh + pt.y > cy - 8)
				mh = cy - 8 - pt.y;
			mw = mh * w / h;
		}
		RECT rcDiffMap = { (cx - mw) / 2, pt.y, (cx + mw) / 2, pt.y + mh };
		SetWindowPos(hwndDiffMap, NULL, rcDiffMap.left, rcDiffMap.top, 
			rcDiffMap.right - rcDiffMap.left, rcDiffMap.bottom - rcDiffMap.top, SWP_NOZORDER);

		InvalidateRect(GetDlgItem(m_hWnd, IDC_DIFFMAP), NULL, TRUE);
		*/

		m_bInSync = false;
	}

	void SetWebDiffWindow(IWebDiffWindow *pWebDiffWindow)
	{
	}

	void Translate(TranslateCallback translateCallback) override
	{
		auto translateString = [&](int id)
		{
			wchar_t org[256];
			wchar_t translated[256];
			::LoadString(m_hInstance, id, org, sizeof(org)/sizeof(org[0]));
			if (translateCallback)
				translateCallback(id, org, sizeof(translated)/sizeof(translated[0]), translated);
			else
				wcscpy_s(translated, org);
			return std::wstring(translated);
		};
	}

private:
	BOOL OnInitDialog(HWND hwnd, HWND hwndFocus, LPARAM lParam)
	{
		return TRUE;
	}

	void OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
	{
		if (m_bInSync)
			return;
	}

	void OnHScroll(HWND hwnd, HWND hwndCtl, UINT code, int pos)
	{
		Sync();
	}

	void OnSize(HWND hwnd, UINT nType, int cx, int cy)
	{
		static const int nIDs[] = {
			IDC_ZOOM,
		};
		HDWP hdwp = BeginDeferWindowPos(static_cast<int>(std::size(nIDs)));
		RECT rc;
		GetClientRect(m_hWnd, &rc);
		for (int id: nIDs)
		{
			RECT rcCtrl;
			HWND hwndCtrl = GetDlgItem(m_hWnd, id);
			GetWindowRect(hwndCtrl, &rcCtrl);
			POINT pt = { rcCtrl.left, rcCtrl.top };
			ScreenToClient(m_hWnd, &pt);
			DeferWindowPos(hdwp, hwndCtrl, nullptr, pt.x, pt.y, rc.right - pt.x * 2, rcCtrl.bottom - rcCtrl.top, SWP_NOMOVE | SWP_NOZORDER);
		}
		EndDeferWindowPos(hdwp);

		Sync();
	}

	void OnDrawItem(HWND hwnd, const DRAWITEMSTRUCT *pDrawItem)
	{
		if (!m_pWebDiffWindow || m_pWebDiffWindow->GetPaneCount() == 0)
			return;
		RECT rc;
		GetClientRect(pDrawItem->hwndItem, &rc);
		HWND hwndLeftPane = m_pWebDiffWindow->GetPaneHWND(0);

		SCROLLINFO sih{ sizeof(sih), SIF_POS | SIF_PAGE | SIF_RANGE };
		GetScrollInfo(hwndLeftPane, SB_HORZ, &sih);
		SCROLLINFO siv{ sizeof(siv), SIF_POS | SIF_PAGE | SIF_RANGE };
		GetScrollInfo(hwndLeftPane, SB_VERT, &siv);

		if (static_cast<int>(sih.nPage) < sih.nMax || static_cast<int>(siv.nPage) < siv.nMax)
		{
			RECT rcFrame;
			rcFrame.left = rc.left + (rc.right - rc.left) * sih.nPos / sih.nMax;
			rcFrame.right = rcFrame.left + (rc.right - rc.left) * sih.nPage / sih.nMax;
			rcFrame.top = rc.top + (rc.bottom - rc.top) * siv.nPos / siv.nMax;
			rcFrame.bottom = rcFrame.top + (rc.bottom - rc.top) * siv.nPage / siv.nMax;
			FrameRect(pDrawItem->hDC, &rcFrame, reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
		}
	}

	INT_PTR OnWndMsg(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
	{
		switch (iMsg)
		{
		case WM_INITDIALOG:
			return HANDLE_WM_INITDIALOG(hwnd, wParam, lParam, OnInitDialog);
		case WM_COMMAND:
			HANDLE_WM_COMMAND(hwnd, wParam, lParam, OnCommand);
			break;
		case WM_HSCROLL:
			HANDLE_WM_HSCROLL(hwnd, wParam, lParam, OnHScroll);
			break;
		case WM_SIZE:
			HANDLE_WM_SIZE(hwnd, wParam, lParam, OnSize);
			break;
		case WM_DRAWITEM:
			HANDLE_WM_DRAWITEM(hwnd, wParam, lParam, OnDrawItem);
			break;
		}
		return 0;
	}

	static INT_PTR CALLBACK DlgProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
	{
		if (iMsg == WM_INITDIALOG)
			SetWindowLongPtr(hwnd, DWLP_USER, reinterpret_cast<LONG_PTR>(reinterpret_cast<CWebToolWindow *>(lParam)));
		CWebToolWindow *pWebWnd = reinterpret_cast<CWebToolWindow *>(GetWindowLongPtr(hwnd, DWLP_USER));
		if (pWebWnd)
			return pWebWnd->OnWndMsg(hwnd, iMsg, wParam, lParam);
		else
			return FALSE;
	}

	static void OnEvent(const WebDiffEvent& evt)
	{
	}

	HWND m_hWnd;
	HINSTANCE m_hInstance;
	IWebDiffWindow *m_pWebDiffWindow;
	bool m_bInSync;
};
