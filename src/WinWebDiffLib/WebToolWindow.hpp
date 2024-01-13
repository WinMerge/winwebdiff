#include <WindowsX.h>
#include <CommCtrl.h>
#include <tchar.h>
#include "WebDiffWindow.hpp"
#include "WinWebDiffLib.h"
#include "resource.h"

#pragma once
#pragma comment(lib, "msimg32.lib")

class CWebToolWindow : public IWebToolWindow, IWebDiffEventHandler
{
public:
	const int MARGIN = 2;

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

	void OnClickDiffMap(HWND hwndDiffMap)
	{
		POINT pt;
		GetCursorPos(&pt);
		ScreenToClient(hwndDiffMap, &pt);
		RECT rc;
		GetClientRect(hwndDiffMap, &rc);
		const int paneCount = m_pWebDiffWindow->GetPaneCount();

		auto [scaleX, scaleY] = CalcScalingFactor(rc);
		for (int pane = 0; pane < paneCount; ++pane)
		{
			RECT rcContainer = GetContainerRect(pane, 0, rc, scaleX, scaleY);
			if (pt.x < rcContainer.right)
			{
				auto visibleArea = static_cast<CWebDiffWindow*>(m_pWebDiffWindow)->GetVisibleAreaRect(pane);
				const float scrollX = std::clamp(static_cast<float>((pt.x - rcContainer.left) / scaleX - visibleArea.width / 2), 0.0f, FLT_MAX);
				const float scrollY = std::clamp(static_cast<float>((pt.y - rcContainer.top) / scaleY - visibleArea.height / 2), 0.0f, FLT_MAX);
				static_cast<CWebDiffWindow*>(m_pWebDiffWindow)->ScrollTo(pane, scrollX, scrollY);
				break;
			}
		}
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
				OnClickDiffMap(hwndCtl);
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
		SetBkColor(hdc, clr);
		ExtTextOut(hdc, 0, 0, ETO_OPAQUE, &rc, nullptr, 0, nullptr);
	}

	void DrawTransparentRectangle(HDC hdc, int left, int top, int right, int bottom, COLORREF clr, BYTE alpha) {
		int width = right - left;
		int height = bottom - top;
		HDC hdcMem = CreateCompatibleDC(hdc);
		HBITMAP hBitmap = CreateCompatibleBitmap(hdc, width, height);
		SelectObject(hdcMem, hBitmap);

		RECT rect = { 0, 0, width, height };
		FillSolidRect(hdcMem, rect, clr);

		BLENDFUNCTION blendFunc{};
		blendFunc.BlendOp = AC_SRC_OVER;
		blendFunc.SourceConstantAlpha = alpha;

		AlphaBlend(hdc, left, top, width, height, hdcMem, 0, 0, width, height, blendFunc);

		DeleteDC(hdcMem);
		DeleteObject(hBitmap);
	}

	std::pair<double, double> CalcScalingFactor(const RECT& rc) const
	{
		float maxPaneWidth = 0, maxPaneHeight = 0;
		const int width = rc.right - rc.left;
		const int height = rc.bottom - rc.top;
		const int paneCount = m_pWebDiffWindow->GetPaneCount(); 
		for (int pane = 0; pane < paneCount; ++pane)
		{
			if (!m_containerRects[pane].empty())
			{
				if (maxPaneWidth < m_containerRects[pane][0].scrollWidth)
					maxPaneWidth = m_containerRects[pane][0].scrollWidth;
				if (maxPaneHeight < m_containerRects[pane][0].scrollHeight)
					maxPaneHeight = m_containerRects[pane][0].scrollHeight;
			}
		}
		double scaleX = static_cast<double>(width - MARGIN * (paneCount * 2)) / (maxPaneWidth * paneCount);
		double scaleY = static_cast<double>(height - MARGIN * 2) / maxPaneHeight;
		return { scaleX, scaleY };
	}

	RECT GetContainerRect(int pane, int i, const RECT& rcDiffMap, double scaleX, double scaleY) const
	{
		RECT rc{};
		if (m_containerRects[pane].empty())
			return rc;
		const int left = rcDiffMap.left;
		const int top = rcDiffMap.top;
		const int width = rcDiffMap.right - rcDiffMap.left;
		const int height = rcDiffMap.bottom - rcDiffMap.top;
		const int paneCount = m_pWebDiffWindow->GetPaneCount(); 
		if (i == 0)
		{
			rc.left = left + MARGIN + width * pane / paneCount;
			rc.top = top + MARGIN;
			rc.right = rc.left + static_cast<int>(m_containerRects[pane][i].scrollWidth * scaleX);
			rc.bottom = rc.top + static_cast<int>(m_containerRects[pane][i].scrollHeight * scaleY);
		}
		else
		{
			rc.left = left + MARGIN + width * pane / paneCount + static_cast<int>(m_containerRects[pane][i].left * scaleX);
			rc.top = top + MARGIN + static_cast<int>(m_containerRects[pane][i].top * scaleY);
			rc.right = rc.left - MARGIN + static_cast<int>(m_containerRects[pane][i].width * scaleX);
			rc.bottom = rc.top - MARGIN + static_cast<int>(m_containerRects[pane][i].height * scaleY);
		}
		return rc;
	};

	void DrawDiffMap(HDC hdcMem, const RECT& rc)
	{
		FillSolidRect(hdcMem, { 0, 0, rc.right, rc.bottom }, RGB(255, 255, 255));

		const int paneCount = m_pWebDiffWindow->GetPaneCount(); 
		if (!m_pWebDiffWindow || paneCount == 0)
			return;
		for (int pane = 0; pane < paneCount; ++pane)
		{
			m_containerRects[pane] = static_cast<CWebDiffWindow*>(m_pWebDiffWindow)->GetContainerRectArray(pane);
			m_rects[pane] = static_cast<CWebDiffWindow*>(m_pWebDiffWindow)->GetDiffRectArray(pane);
		}
		for (int pane = 0; pane < paneCount; ++pane)
		{
			if (m_containerRects[pane].size() == 0)
				return;
		}

		IWebDiffWindow::ColorSettings colors;
		m_pWebDiffWindow->GetDiffColorSettings(colors);
		auto [scaleX, scaleY] = CalcScalingFactor(rc);

		const int curDiff = m_pWebDiffWindow->GetCurrentDiffIndex();
		for (int pane = 0; pane < paneCount; ++pane)
		{
			if (m_rects[pane].size() > 0)
			{
				const RECT rcContainer = GetContainerRect(pane, 0, rc, scaleX, scaleY);
				for (int i = 0; i < m_rects[pane].size(); ++i)
				{
					if (m_rects[pane][i].left != -99999.9f || m_rects[pane][i].top != -99999.9f)
					{
						const int diffLeft = rcContainer.left + static_cast<int>(m_rects[pane][i].left * scaleX);
						const int diffTop = rcContainer.top + static_cast<int>(m_rects[pane][i].top * scaleY);
						const int diffRight = rcContainer.left + static_cast<int>((m_rects[pane][i].left + m_rects[pane][i].width) * scaleX);
						const int diffBottom = rcContainer.top + static_cast<int>((m_rects[pane][i].top + m_rects[pane][i].height) * scaleY);
						const RECT rc = { diffLeft, diffTop, diffRight, diffBottom };
						FillSolidRect(hdcMem, rc, (curDiff == m_rects[pane][i].id) ? colors.clrSelDiff : colors.clrDiff);
					}
				}
			}
			HBRUSH hOldBrush = SelectBrush(hdcMem, GetStockBrush(NULL_BRUSH));
			for (int i = 0; i < m_containerRects[pane].size(); ++i)
			{
				const RECT rcContainer = GetContainerRect(pane, i, rc, scaleX, scaleY);
				Rectangle(hdcMem, rcContainer.left, rcContainer.top, rcContainer.right, rcContainer.bottom);
			}

			RECT rcContainer = GetContainerRect(pane, 0, rc, scaleX, scaleY);
			auto visibleArea = static_cast<CWebDiffWindow*>(m_pWebDiffWindow)->GetVisibleAreaRect(pane);

			rcContainer.left += static_cast<int>(visibleArea.left * scaleX);
			rcContainer.right = static_cast<int>(rcContainer.left + visibleArea.width * scaleX);
			rcContainer.top += static_cast<int>(visibleArea.top * scaleY);
			rcContainer.bottom = static_cast<int>(rcContainer.top + visibleArea.height * scaleY);
			DrawTransparentRectangle(hdcMem, 
				rcContainer.left, rcContainer.top, rcContainer.right, rcContainer.bottom,
				RGB(96, 96, 255), 64);

			SelectBrush(hdcMem, hOldBrush);
		}
	}

	void OnDrawItem(HWND hwnd, const DRAWITEMSTRUCT *pDrawItem)
	{
		RECT rc;
		GetClientRect(pDrawItem->hwndItem, &rc);

		HDC hdcMem = CreateCompatibleDC(pDrawItem->hDC);
		HBITMAP hBitmap = CreateCompatibleBitmap(pDrawItem->hDC, rc.right, rc.bottom);
		HBITMAP hOldBitmap = SelectBitmap(hdcMem, hBitmap);

		DrawDiffMap(hdcMem, rc);

		BitBlt(pDrawItem->hDC, 0, 0, rc.right, rc.bottom, hdcMem, 0, 0, SRCCOPY);

		SelectBitmap(hdcMem, hOldBitmap);
		DeleteBitmap(hBitmap);
		DeleteDC(hdcMem);
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
		case WebDiffEvent::DiffSelected:
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
	std::vector<DiffLocation::ContainerRect> m_containerRects[3];
	std::vector<DiffLocation::DiffRect> m_rects[3];
	bool m_bInSync;
	int m_nRef;
	
};
