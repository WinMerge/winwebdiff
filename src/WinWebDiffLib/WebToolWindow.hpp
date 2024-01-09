#include <WindowsX.h>
#include <CommCtrl.h>
#include <tchar.h>
#include "WebDiffWindow.hpp"
#include "WinWebDiffLib.h"
#include "resource.h"

#pragma once

class CWebToolWindow : public IWebToolWindow, IWebDiffEventHandler
{
public:
	CWebToolWindow() :
		  m_hWnd(nullptr)
		, m_hInstance(nullptr)
		, m_hComparePopup(nullptr)
		, m_hEventSyncPopup(nullptr)
		, m_pWebDiffWindow(nullptr)
		, m_bInSync(false)
		, m_nRef(0)
	{
	}

	~CWebToolWindow()
	{
	}

	bool Create(HINSTANCE hInstance, HWND hWndParent)
	{
		m_hInstance = hInstance;
		m_hWnd = CreateDialogParam(hInstance, MAKEINTRESOURCE(IDD_DIALOGBAR), hWndParent, DlgProc, reinterpret_cast<LPARAM>(this));
		m_hComparePopup = LoadMenu(hInstance, MAKEINTRESOURCE(IDR_POPUP_WEBPAGE_COMPARE));
		m_hEventSyncPopup = LoadMenu(hInstance, MAKEINTRESOURCE(IDR_POPUP_WEBPAGE_SYNC_EVENTS));
		return m_hWnd ? true : false;
	}

	bool Destroy()
	{
		BOOL bSucceeded = DestroyWindow(m_hWnd);
		m_hWnd = nullptr;
		DestroyMenu(m_hComparePopup);
		m_hComparePopup = nullptr;
		DestroyMenu(m_hEventSyncPopup);
		m_hEventSyncPopup = nullptr;
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

		SIZE size = m_pWebDiffWindow->GetSize();
		SendDlgItemMessage(m_hWnd, IDC_SHOWDIFFERENCES, BM_SETCHECK, m_pWebDiffWindow->GetShowDifferences() ? BST_CHECKED : BST_UNCHECKED, 0);
		SendDlgItemMessage(m_hWnd, IDC_FITTOWINDOW, BM_SETCHECK, m_pWebDiffWindow->GetFitToWindow() ? BST_CHECKED : BST_UNCHECKED, 0);
		SetDlgItemText(m_hWnd, IDC_USERAGENT, m_pWebDiffWindow->GetUserAgent());
		SetDlgItemInt(m_hWnd, IDC_WIDTH, size.cx, false);
		SetDlgItemInt(m_hWnd, IDC_HEIGHT, size.cy, false);
		wchar_t zoomstr[256];
		swprintf_s(zoomstr, L"%.1f%%", m_pWebDiffWindow->GetZoom() * 100.0);
		SetDlgItemText(m_hWnd, IDC_ZOOM, zoomstr);
		EnableWindow(GetDlgItem(m_hWnd, IDC_WIDTH), !m_pWebDiffWindow->GetFitToWindow());
		EnableWindow(GetDlgItem(m_hWnd, IDC_HEIGHT), !m_pWebDiffWindow->GetFitToWindow());

		/*
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

	void RedrawDiffMap()
	{
		InvalidateRect(GetDlgItem(m_hWnd, IDC_DIFFMAP), NULL, TRUE);
	}

	void SetWebDiffWindow(IWebDiffWindow *pWebDiffWindow)
	{
		m_pWebDiffWindow = pWebDiffWindow;
		m_pWebDiffWindow->AddEventListener(this);
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

		struct StringIdControlId
		{
			int stringId;
			int controlId;
		};
		static const StringIdControlId stringIdControlId[] = {
			{IDS_COMPARE, IDC_COMPARE},
			{IDS_ZOOM, IDC_ZOOM_LABEL},
			{IDS_SYNC_EVENTS, IDC_SYNC_EVENTS},
			{IDS_SHOWDIFFERENCES, IDC_SHOWDIFFERENCES},
		};
		for (auto& e: stringIdControlId)
			::SetDlgItemText(m_hWnd, e.controlId, translateString(e.stringId).c_str());

		static const StringIdControlId stringIdControlId1[] = {
			{IDS_WEB_COMPARE_SCREENSHOTS, ID_WEB_COMPARE_SCREENSHOTS},
			{IDS_WEB_COMPARE_FULLSIZE_SCREENSHOTS, ID_WEB_COMPARE_FULLSIZE_SCREENSHOTS},
			{IDS_WEB_COMPARE_HTMLS, ID_WEB_COMPARE_HTMLS},
			{IDS_WEB_COMPARE_TEXTS, ID_WEB_COMPARE_TEXTS},
			{IDS_WEB_COMPARE_RESOURCETREES, ID_WEB_COMPARE_RESOURCETREES},
		};
		for (auto& e : stringIdControlId1)
		{
			std::wstring str = translateString(e.stringId);
			MENUITEMINFO mii = { sizeof(MENUITEMINFO) };
			mii.fMask = MIIM_STRING;
			mii.dwTypeData = const_cast<LPWSTR>(str.c_str());
			::SetMenuItemInfo(m_hComparePopup, e.controlId, FALSE, &mii);
		}
		static const StringIdControlId stringIdControlId2[] = {
			{IDS_WEB_SYNC_ENABLED, ID_WEB_SYNC_ENABLED},
			{IDS_WEB_SYNC_SCROLL, ID_WEB_SYNC_SCROLL},
			{IDS_WEB_SYNC_CLICK, ID_WEB_SYNC_CLICK},
			{IDS_WEB_SYNC_INPUT, ID_WEB_SYNC_INPUT},
			{IDS_WEB_SYNC_GOBACKFORWARD, ID_WEB_SYNC_GOBACKFORWARD},
		};
		for (auto& e : stringIdControlId2)
		{
			std::wstring str = translateString(e.stringId);
			MENUITEMINFO mii = { sizeof(MENUITEMINFO) };
			mii.fMask = MIIM_STRING;
			mii.dwTypeData = const_cast<LPWSTR>(str.c_str());
			::SetMenuItemInfo(m_hEventSyncPopup, e.controlId, FALSE, &mii);
		}
	}

private:
	BOOL OnInitDialog(HWND hwnd, HWND hwndFocus, LPARAM lParam)
	{
		HWND hwndZoom = GetDlgItem(hwnd, IDC_ZOOM);
		for (const auto v :
			{L"25%", L"33.3%", L"50%", L"66.7%", L"75%", L"80%", L"90%", L"100%",
			 L"110%", L"125%", L"150%", L"175%", L"200%", L"250%", L"300%"})
			ComboBox_AddString(hwndZoom, v);
		return TRUE;
	}

	void ShowComparePopupMenu()
	{
		RECT rc;
		GetWindowRect(GetDlgItem(m_hWnd, IDC_COMPARE), &rc);
		POINT point{ rc.left, rc.bottom };
		HMENU hSubMenu = GetSubMenu(m_hComparePopup, 0);
		TrackPopupMenu(hSubMenu, TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y, 0, m_hWnd, nullptr);
	}

	void ShowSyncEventsPopupMenu()
	{
		RECT rc;
		GetWindowRect(GetDlgItem(m_hWnd, IDC_SYNC_EVENTS), &rc);
		POINT point{ rc.left, rc.bottom };
		HMENU hSubMenu = GetSubMenu(m_hEventSyncPopup, 0);
		CheckMenuItem(hSubMenu, ID_WEB_SYNC_ENABLED, m_pWebDiffWindow->GetSyncEvents() ? MF_CHECKED : MF_UNCHECKED);
		CheckMenuItem(hSubMenu, ID_WEB_SYNC_SCROLL, m_pWebDiffWindow->GetSyncEventFlag(IWebDiffWindow::EVENT_SCROLL) ? MF_CHECKED : MF_UNCHECKED);
		CheckMenuItem(hSubMenu, ID_WEB_SYNC_CLICK, m_pWebDiffWindow->GetSyncEventFlag(IWebDiffWindow::EVENT_CLICK) ? MF_CHECKED : MF_UNCHECKED);
		CheckMenuItem(hSubMenu, ID_WEB_SYNC_INPUT, m_pWebDiffWindow->GetSyncEventFlag(IWebDiffWindow::EVENT_INPUT) ? MF_CHECKED : MF_UNCHECKED);
		CheckMenuItem(hSubMenu, ID_WEB_SYNC_GOBACKFORWARD, m_pWebDiffWindow->GetSyncEventFlag(IWebDiffWindow::EVENT_GOBACKFORWARD) ? MF_CHECKED : MF_UNCHECKED);
		TrackPopupMenu(hSubMenu, TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y, 0, m_hWnd, nullptr);
	}

	void OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
	{
		if (m_bInSync)
			return;

		switch (id)
		{
		case IDC_SHOWDIFFERENCES:
			if (codeNotify == BN_CLICKED)
				m_pWebDiffWindow->SetShowDifferences(Button_GetCheck(hwndCtl) == BST_CHECKED);
			break;
		case IDC_FITTOWINDOW:
			if (codeNotify == BN_CLICKED)
			{
				m_pWebDiffWindow->SetFitToWindow(Button_GetCheck(hwndCtl) == BST_CHECKED);
				Sync();
			}
			break;
		case IDC_WIDTH:
			if (codeNotify == EN_CHANGE)
				m_pWebDiffWindow->SetSize(
					{ static_cast<long>(GetDlgItemInt(m_hWnd, IDC_WIDTH, nullptr, false)),
					  m_pWebDiffWindow->GetSize().cy });
			break;
		case IDC_HEIGHT:
			if (codeNotify == EN_CHANGE)
				m_pWebDiffWindow->SetSize(
					{ m_pWebDiffWindow->GetSize().cx,
					  static_cast<long>(GetDlgItemInt(m_hWnd, IDC_HEIGHT, nullptr, false)) });
			break;
		case IDC_ZOOM:
			if (codeNotify == CBN_EDITCHANGE)
			{
				wchar_t zoom[256]{};
				GetDlgItemText(m_hWnd, IDC_ZOOM, zoom, sizeof(zoom)/sizeof(zoom[0]));
				float zoomf = wcstof(zoom, nullptr);
				m_pWebDiffWindow->SetZoom(zoomf / 100.0f);
			}
			else if (codeNotify == CBN_SELCHANGE)
			{
				HWND hZoom = GetDlgItem(m_hWnd, IDC_ZOOM);
				int cursel = ComboBox_GetCurSel(hZoom);
				wchar_t zoom[256]{};
				ComboBox_GetLBText(hZoom, cursel, zoom);
				float zoomf = wcstof(zoom, nullptr);
				m_pWebDiffWindow->SetZoom(zoomf / 100.0f);
			}
			break;
		case IDC_USERAGENT:
			if (codeNotify == EN_CHANGE)
			{
				wchar_t ua[256];
				GetDlgItemText(m_hWnd, IDC_USERAGENT, ua, sizeof(ua)/sizeof(ua[0]));
				m_pWebDiffWindow->SetUserAgent(ua);
			}
			break;
		case IDC_COMPARE:
			if (codeNotify == BN_CLICKED)
				m_pWebDiffWindow->Recompare(nullptr);
			break;
		case IDC_SYNC_EVENTS:
			if (codeNotify == BN_CLICKED)
				ShowSyncEventsPopupMenu();
			break;
		case ID_WEB_SYNC_ENABLED:
			m_pWebDiffWindow->SetSyncEvents(!m_pWebDiffWindow->GetSyncEvents());
			break;
		case ID_WEB_SYNC_SCROLL:
			m_pWebDiffWindow->SetSyncEventFlag(IWebDiffWindow::EVENT_SCROLL, !m_pWebDiffWindow->GetSyncEventFlag(IWebDiffWindow::EVENT_SCROLL));
			break;
		case ID_WEB_SYNC_CLICK:
			m_pWebDiffWindow->SetSyncEventFlag(IWebDiffWindow::EVENT_CLICK, !m_pWebDiffWindow->GetSyncEventFlag(IWebDiffWindow::EVENT_CLICK));
			break;
		case ID_WEB_SYNC_INPUT:
			m_pWebDiffWindow->SetSyncEventFlag(IWebDiffWindow::EVENT_INPUT, !m_pWebDiffWindow->GetSyncEventFlag(IWebDiffWindow::EVENT_INPUT));
			break;
		case ID_WEB_SYNC_GOBACKFORWARD:
			m_pWebDiffWindow->SetSyncEventFlag(IWebDiffWindow::EVENT_GOBACKFORWARD, !m_pWebDiffWindow->GetSyncEventFlag(IWebDiffWindow::EVENT_GOBACKFORWARD));
			break;
		case ID_WEB_COMPARE_SCREENSHOTS:
			m_pWebDiffWindow->RaiseEvent({ WebDiffEvent::CompareScreenshotsSelected, -1 });
			break;
		case ID_WEB_COMPARE_FULLSIZE_SCREENSHOTS:
			m_pWebDiffWindow->RaiseEvent({ WebDiffEvent::CompareFullsizeScreenshotsSelected, -1 });
			break;
		case ID_WEB_COMPARE_HTMLS:
			m_pWebDiffWindow->RaiseEvent({ WebDiffEvent::CompareHTMLsSelected, -1 });
			break;
		case ID_WEB_COMPARE_TEXTS:
			m_pWebDiffWindow->RaiseEvent({ WebDiffEvent::CompareTextsSelected, -1 });
			break;
		case ID_WEB_COMPARE_RESOURCETREES:
			m_pWebDiffWindow->RaiseEvent({ WebDiffEvent::CompareResourceTreesSelected, -1 });
			break;
		case IDC_DIFFMAP:
			if (codeNotify == STN_CLICKED)
			{
				POINT pt;
				GetCursorPos(&pt);
				ScreenToClient(hwndCtl, &pt);
				RECT rc;
				GetClientRect(hwndCtl, &rc);
				/*
				m_pWebDiffWindow->ScrollTo(
					pt.x * pImgMergeWindow->GetDiffImageWidth() / rc.right,
					pt.y * pImgMergeWindow->GetDiffImageHeight() / rc.bottom,
					true);
					*/
			}
			break;
		}
	}

	void OnNotify(HWND hwnd, WPARAM idControl, NMHDR *pNMHDR)
	{
		if (pNMHDR->code == BCN_DROPDOWN)
		{
			switch (pNMHDR->idFrom)
			{
			case IDC_COMPARE:
				ShowComparePopupMenu();
				break;
			case IDC_SYNC_EVENTS:
				ShowSyncEventsPopupMenu();
				break;
			}
		}
	}

	void OnHScroll(HWND hwnd, HWND hwndCtl, UINT code, int pos)
	{
		Sync();
	}

	void OnSize(HWND hwnd, UINT nType, int cx, int cy)
	{
		static const int nIDs[] = {
			IDC_COMPARE,
			IDC_ZOOM,
			IDC_USERAGENT,
			IDC_SYNC_EVENTS,
			IDC_SHOWDIFFERENCES,
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
			DeferWindowPos(hdwp, hwndCtrl, nullptr, pt.x, pt.y, rc.right - pt.x - 2, rcCtrl.bottom - rcCtrl.top, SWP_NOMOVE | SWP_NOZORDER);
		}
		RECT rcWidth, rcHeight, rcBy;
		HWND hwndWidth = GetDlgItem(m_hWnd, IDC_WIDTH);
		HWND hwndBy = GetDlgItem(m_hWnd, IDC_BY);
		HWND hwndHeight = GetDlgItem(m_hWnd, IDC_HEIGHT);
		GetWindowRect(hwndWidth, &rcWidth);
		GetWindowRect(hwndBy, &rcBy);
		GetWindowRect(hwndHeight, &rcHeight);
		POINT ptWidth = { rcWidth.left, rcWidth.top };
		ScreenToClient(m_hWnd, &ptWidth);
		int wby = rcBy.right - rcBy.left;
		int w = (rc.right - 2 - ptWidth.x - wby - 4) / 2;
		int h = rcWidth.bottom - rcWidth.top;
		DeferWindowPos(hdwp, hwndWidth,  nullptr, ptWidth.x, ptWidth.y, w, h, SWP_NOMOVE | SWP_NOZORDER);
		DeferWindowPos(hdwp, hwndBy,     nullptr, ptWidth.x + w + 2, ptWidth.y, rcBy.right - rcBy.left, h, SWP_NOZORDER);
		DeferWindowPos(hdwp, hwndHeight, nullptr, ptWidth.x + w + 2 + wby + 2 , ptWidth.y, w, h, SWP_NOZORDER);

		RECT rcTmp;
		HWND hwndDiffMap = GetDlgItem(m_hWnd, IDC_DIFFMAP);
		GetWindowRect(hwndDiffMap, &rcTmp);
		POINT pt = { rcTmp.left, rcTmp.top };
		ScreenToClient(m_hWnd, &pt);
		const int margin = pt.x;
		DeferWindowPos(hdwp, hwndDiffMap, nullptr,
			pt.x, pt.y, rc.right - rc.left - margin * 2, rc.bottom - rc.top - pt.y - margin, SWP_NOZORDER);
		InvalidateRect(hwndDiffMap, nullptr, TRUE);
		EndDeferWindowPos(hdwp);

		Sync();
	}

	void FillSolidRect(HDC hdc, const RECT& rc, COLORREF clr)
	{
		::SetBkColor(hdc, clr);
		::ExtTextOut(hdc, 0, 0, ETO_OPAQUE, &rc, nullptr, 0, nullptr);
	}

	void OnDrawItem(HWND hwnd, const DRAWITEMSTRUCT *pDrawItem)
	{
		if (!m_pWebDiffWindow || m_pWebDiffWindow->GetPaneCount() == 0)
			return;
		IWebDiffWindow::ColorSettings colors;
		m_pWebDiffWindow->GetDiffColorSettings(colors);
		RECT rc;
		GetClientRect(pDrawItem->hwndItem, &rc);
		FillSolidRect(pDrawItem->hDC, { 0, 0, rc.right, rc.bottom }, RGB(255, 255, 255));
		const int left = rc.left;
		const int top = rc.top;
		const int width = rc.right - rc.left;
		const int height = rc.bottom - rc.top;

		const IWebDiffWindow::ContainerRect* containerRects[3];
		const IWebDiffWindow::DiffRect* rects[3];
		int counts[3], containerCounts[3];
		float maxPaneWidth = 0, maxPaneHeight = 0;
		const int paneCount = m_pWebDiffWindow->GetPaneCount(); 
		for (int pane = 0; pane < paneCount; ++pane)
		{
			containerRects[pane] = m_pWebDiffWindow->GetContainerRectArray(pane, containerCounts[pane]);
			rects[pane] = m_pWebDiffWindow->GetDiffRectArray(pane, counts[pane]);
			if (containerCounts[pane] > 0)
			{
				if (maxPaneWidth < containerRects[pane][0].scrollWidth)
					maxPaneWidth = containerRects[pane][0].scrollWidth;
				if (maxPaneHeight < containerRects[pane][0].scrollHeight)
					maxPaneHeight = containerRects[pane][0].scrollHeight;
			}
		}
		const int margin = 2;
		double ratiox = static_cast<double>(width - margin * (paneCount * 2)) / (maxPaneWidth * paneCount);
		double ratioy = static_cast<double>(height - margin * 2) / maxPaneHeight;
		auto getContainerRect = [&](int pane, int i) -> RECT {
			RECT rc;
			if (i == 0)
			{
				rc.left = left + margin + width * pane / paneCount;
				rc.top = top + margin;
				rc.right = rc.left + static_cast<int>(containerRects[pane][i].scrollWidth * ratiox);
				rc.bottom = rc.top + static_cast<int>(containerRects[pane][i].scrollHeight * ratioy);
			}
			else
			{
				rc.left = left + margin + width * pane / paneCount + static_cast<int>(containerRects[pane][i].left * ratiox);
				rc.top = top + margin + static_cast<int>(containerRects[pane][i].top * ratioy);
				rc.right = rc.left - margin + static_cast<int>(containerRects[pane][i].width * ratiox);
				rc.bottom = rc.top - margin + static_cast<int>(containerRects[pane][i].height * ratioy);
			}
			return rc;
		};
		const int curDiff = m_pWebDiffWindow->GetCurrentDiffIndex();
		for (int pane = 0; pane < paneCount; ++pane)
		{
			for (int i = 0; i < counts[pane]; ++i)
			{
				const RECT rcContainer = getContainerRect(pane, rects[pane][i].containerId);
				const int diffLeft = rcContainer.left + static_cast<int>(rects[pane][i].left * ratiox);
				const int diffTop = rcContainer.top + static_cast<int>(rects[pane][i].top * ratioy);
				const int diffRight = rcContainer.left + static_cast<int>((rects[pane][i].left + rects[pane][i].width) * ratiox);
				const int diffBottom = rcContainer.top + static_cast<int>((rects[pane][i].top + rects[pane][i].height) * ratioy);
				const RECT rc = {diffLeft, diffTop, diffRight, diffBottom};
				FillSolidRect(pDrawItem->hDC, rc, (curDiff == i) ? colors.clrSelDiff : colors.clrDiff);
			}
			HBRUSH hOldBrush = SelectBrush(pDrawItem->hDC, GetStockBrush(NULL_BRUSH));
			for (int i = 0; i < containerCounts[pane]; ++i)
			{
				const RECT rcContainer = getContainerRect(pane, i);
				Rectangle(pDrawItem->hDC, rcContainer.left, rcContainer.top, rcContainer.right, rcContainer.bottom);
			}

			if (containerCounts[pane] > 0)
			{
				RECT rcContainer = getContainerRect(pane, 0);
				auto visibleArea = m_pWebDiffWindow->GetVisibleAreaRect(pane);

				rcContainer.left += static_cast<int>(visibleArea.left * ratiox);
				rcContainer.right = static_cast<int>(rcContainer.left + visibleArea.width * ratiox);
				rcContainer.top += static_cast<int>(visibleArea.top * ratioy);
				rcContainer.bottom = static_cast<int>(rcContainer.top + visibleArea.height * ratioy);
				Rectangle(pDrawItem->hDC, rcContainer.left, rcContainer.top, rcContainer.right, rcContainer.bottom);
			}

			SelectBrush(pDrawItem->hDC, hOldBrush);
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
		case WM_NOTIFY:
			HANDLE_WM_NOTIFY(hwnd, wParam, lParam, OnNotify);
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

	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override { return E_NOTIMPL; }
	ULONG STDMETHODCALLTYPE AddRef(void) override { return ++m_nRef; }
	ULONG STDMETHODCALLTYPE Release(void) override { if (--m_nRef == 0) { return 0; } return m_nRef; }

	HRESULT Invoke(const WebDiffEvent& event)
	{
		switch (event.type)
		{
		case WebDiffEvent::ZoomFactorChanged:
			Sync();
			break;
		case WebDiffEvent::WebMessageReceived:
			RedrawDiffMap();
			break;
		}
		return S_OK;
	}

	HWND m_hWnd;
	HINSTANCE m_hInstance;
	HMENU m_hComparePopup;
	HMENU m_hEventSyncPopup;
	IWebDiffWindow *m_pWebDiffWindow;
	bool m_bInSync;
	int m_nRef;
	
};
