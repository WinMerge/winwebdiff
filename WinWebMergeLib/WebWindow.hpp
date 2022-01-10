#pragma once

#include <Windows.h>
#include <CommCtrl.h>
#include <wrl.h>
#include <wil/com.h>
#include <string>
#include <vector>
#include <memory>
#include <cassert>
#include "WebView2.h"
#include "resource.h"

using namespace Microsoft::WRL;

class CWebWindow
{
	class CWebTab
	{
	public:
		CWebTab(CWebWindow* parent, const wchar_t* url)
			: m_parent(parent)
		{
			InitializeWebView(url, nullptr, nullptr);
		}

		CWebTab(CWebWindow* parent, ICoreWebView2NewWindowRequestedEventArgs* args = nullptr, ICoreWebView2Deferral* deferral = nullptr)
			: m_parent(parent)
		{
			InitializeWebView(nullptr, args, deferral);
		}

		bool InitializeWebView(const wchar_t* url, ICoreWebView2NewWindowRequestedEventArgs* args, ICoreWebView2Deferral* deferral)
		{
			HRESULT hr = m_parent->m_webviewEnvironment->CreateCoreWebView2Controller(m_parent->m_hWnd, Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
				[this, url, args, deferral](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
					if (controller != nullptr) {
						m_webviewController = controller;
						m_webviewController->get_CoreWebView2(&m_webview);
					}

					ICoreWebView2Settings* Settings;
					m_webview->get_Settings(&Settings);
					Settings->put_IsScriptEnabled(TRUE);
					Settings->put_AreDefaultScriptDialogsEnabled(TRUE);
					Settings->put_IsWebMessageEnabled(TRUE);

					if (args && deferral)
					{
						args->put_NewWindow(m_webview.get());
						args->put_Handled(true);
						deferral->Complete();
						deferral->Release();
					}
					else
					{
						m_webview->Navigate(url);
					}
					m_parent->ResizeWebView();

					m_webview->add_NewWindowRequested(
						Callback<ICoreWebView2NewWindowRequestedEventHandler>(
							[this](ICoreWebView2* sender, ICoreWebView2NewWindowRequestedEventArgs* args) {
								return m_parent->OnNewWindowRequested(sender, args);
							})
						.Get(), nullptr);

					m_webview->add_WindowCloseRequested(
						Callback<ICoreWebView2WindowCloseRequestedEventHandler>(
							[this](ICoreWebView2* sender, IUnknown* args) {
								return m_parent->OnWindowCloseRequested(sender, args);
							})
						.Get(), nullptr);

					m_webview->add_NavigationStarting(
						Callback<ICoreWebView2NavigationStartingEventHandler>(
							[this](ICoreWebView2* sender, ICoreWebView2NavigationStartingEventArgs* args)
							-> HRESULT {
								m_navigationCompleted = false;
								return m_parent->OnNavigationStarting(sender, args);
							})
						.Get(), nullptr);

					m_webview->add_HistoryChanged(
						Callback<ICoreWebView2HistoryChangedEventHandler>(
							[this](ICoreWebView2* sender, IUnknown* args) -> HRESULT {
								return m_parent->OnHistoryChanged(sender, args);
							})
						.Get(), nullptr);

					m_webview->add_SourceChanged(
						Callback<ICoreWebView2SourceChangedEventHandler>(
							[this](ICoreWebView2* sender, ICoreWebView2SourceChangedEventArgs* args)
							-> HRESULT {
								return m_parent->OnSourceChanged(sender, args);
							})
						.Get(), nullptr);

					m_webview->add_DocumentTitleChanged(
						Callback<ICoreWebView2DocumentTitleChangedEventHandler>(
							[this](ICoreWebView2* sender, IUnknown* args) -> HRESULT {
								return m_parent->OnDocumentTitleChanged(sender, args);
							})
						.Get(), nullptr);

					m_webview->add_NavigationCompleted(
						Callback<ICoreWebView2NavigationCompletedEventHandler>(
							[this](ICoreWebView2* sender, ICoreWebView2NavigationCompletedEventArgs* args)
							-> HRESULT {
								m_navigationCompleted = true;
								return m_parent->OnNavigationCompleted(sender, args);
							})
						.Get(), nullptr);
					return S_OK;
				}).Get());
			return SUCCEEDED(hr);
		}

		wil::com_ptr<ICoreWebView2Controller> m_webviewController;
		wil::com_ptr<ICoreWebView2> m_webview;
		CWebWindow* m_parent;
		bool m_navigationCompleted = false;
	};

	friend CWebTab;
public:
	CWebWindow()
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
		m_hWnd = CreateWindowExW(0, L"WinWebWindowClass", nullptr,
			WS_CHILD | WS_VISIBLE,
			0, 0, 0, 0, hWndParent, nullptr, hInstance, this);
		if (!m_hWnd)
			return false;
		m_hTabCtrl = CreateWindowEx(0, WC_TABCONTROL, nullptr,
			WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | TCS_FLATBUTTONS,
			0, 0, 4, 4, m_hWnd, (HMENU)1, hInstance, nullptr);
		m_hToolbar = CreateWindowExW(0, TOOLBARCLASSNAME, nullptr,
			WS_CHILD | TBSTYLE_FLAT | TBSTYLE_LIST | CCS_NOMOVEY | WS_VISIBLE,
			0, 0, 0, 0, m_hWnd, (HMENU)2, hInstance, nullptr);
		HDC hDC = GetDC(m_hWnd);
		LOGFONT lfToolbar{};
		lfToolbar.lfHeight = MulDiv(-14, GetDeviceCaps(hDC, LOGPIXELSX), 72);
		lfToolbar.lfWeight = FW_NORMAL;
		lfToolbar.lfCharSet = SYMBOL_CHARSET;
		lfToolbar.lfOutPrecision = OUT_TT_ONLY_PRECIS;
		lfToolbar.lfQuality = PROOF_QUALITY;
		lfToolbar.lfPitchAndFamily = VARIABLE_PITCH | FF_DECORATIVE;
		wcscpy_s(lfToolbar.lfFaceName, L"Wingdings 3");
		NONCLIENTMETRICS info;
		info.cbSize = sizeof(info);
		SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(info), &info, 0);
		LOGFONT lfEdit = info.lfCaptionFont;
		ReleaseDC(m_hWnd, hDC);
		m_hToolbarFont = CreateFontIndirect(&lfToolbar);
		m_hEditFont = CreateFontIndirect(&lfEdit);
		SendMessage(m_hToolbar, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);
		TBBUTTON tbb[] = {
			{I_IMAGENONE, ID_GOBACK,    TBSTATE_ENABLED, BTNS_BUTTON | BTNS_AUTOSIZE, {}, 0, (INT_PTR)L"\xD1"},
			{I_IMAGENONE, ID_GOFORWARD, TBSTATE_ENABLED, BTNS_BUTTON | BTNS_AUTOSIZE, {}, 0, (INT_PTR)L"\xD2"},
			{I_IMAGENONE, ID_RELOAD,    TBSTATE_ENABLED, BTNS_BUTTON | BTNS_AUTOSIZE, {}, 0, (INT_PTR)L"\x50"},
			{I_IMAGENONE, ID_STOP,      TBSTATE_ENABLED, BTNS_BUTTON | BTNS_AUTOSIZE, {}, 0, (INT_PTR)L"\xAC"},
		};
		m_hEdit = CreateWindowEx(0, TEXT("EDIT"), TEXT(""),
			WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
			0, 0, 0, 0, m_hToolbar, (HMENU)3, hInstance, nullptr);
		SetWindowLongPtr(m_hEdit, GWLP_USERDATA, (LONG_PTR)this);
		m_oldEditWndProc = (WNDPROC)SetWindowLongPtr(m_hEdit, GWLP_WNDPROC, (LONG_PTR)EditWndProc);
		SendMessage(m_hTabCtrl, WM_SETFONT, (WPARAM)m_hEditFont, 0);
		SendMessage(m_hEdit, WM_SETFONT, (WPARAM)m_hEditFont, 0);
		SendMessage(m_hToolbar, TB_ADDBUTTONS, (WPARAM)std::size(tbb), (LPARAM)&tbb);
		SendMessage(m_hToolbar, WM_SETFONT, (WPARAM)m_hToolbarFont, 0);
		InitializeWebView();
		return true;
	}

	bool InitializeWebView()
	{
		HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(nullptr, nullptr, nullptr,
			Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
				[this](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
					m_webviewEnvironment = env;
					m_tabs.emplace_back(new CWebTab(this, L"https://www.google.com"));

					TCITEM tcItem{};
					tcItem.mask = TCIF_TEXT;
					tcItem.pszText = (PWSTR)L"";
					TabCtrl_InsertItem(m_hTabCtrl, 0, &tcItem);
					m_activeTab = 0;
					return S_OK;
				}).Get());
		return SUCCEEDED(hr);
	}

	bool Destroy()
	{
		if (m_hToolbar)
			DestroyWindow(m_hToolbar);
		if (m_hTabCtrl)
			DestroyWindow(m_hTabCtrl);
		if (m_hWnd)
			DestroyWindow(m_hWnd);
		m_hToolbar = nullptr;
		m_hTabCtrl = nullptr;
		m_hWnd = nullptr;
		DeleteObject(m_hToolbarFont);
		DeleteObject(m_hEditFont);
		return true;
	}

	int FindTabIndex(const ICoreWebView2* webview)
	{
		int i = 0;
		for (auto& tab : m_tabs)
		{
			if (tab->m_webview.get() == webview)
				return i;
			++i;
		}
		return -1;
	}
	CWebTab* GetActiveTab()
	{
		assert(m_activeTab >= 0 && m_activeTab < m_tabs.size());
		return m_tabs[m_activeTab].get();
	}

	ICoreWebView2* GetActiveWebView()
	{
		return GetActiveTab()->m_webview.get();
	}

	ICoreWebView2Controller* GetActiveWebViewController()
	{
		return GetActiveTab()->m_webviewController.get();
	}

	bool Navigate(const wchar_t* url)
	{
		HRESULT hr = GetActiveWebView()->Navigate(url);
		if (hr == E_INVALIDARG)
			hr = GetActiveWebView()->Navigate((L"http://" + std::wstring(url)).c_str());
		return SUCCEEDED(hr);
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
		ResizeUIControls();
	}

	void SetFocus()
	{
		::SetFocus(m_hWnd);
	}

private:

	void ResizeUIControls()
	{
		if (!m_hWnd)
			return;
		RECT bounds, rcToolbar, rcStop, rcTabCtrl;
		GetClientRect(m_hWnd, &bounds);
		rcTabCtrl = bounds;
		TabCtrl_AdjustRect(m_hTabCtrl, false, &rcTabCtrl);
		MoveWindow(m_hTabCtrl, bounds.left, bounds.top, bounds.right - bounds.left, rcTabCtrl.top, TRUE);
		GetClientRect(m_hTabCtrl, &rcTabCtrl);
		TabCtrl_AdjustRect(m_hTabCtrl, false, &rcTabCtrl);
		GetClientRect(m_hToolbar, &rcToolbar);
		MoveWindow(m_hToolbar, rcTabCtrl.left, rcTabCtrl.top, rcTabCtrl.right - rcTabCtrl.left, rcToolbar.bottom - rcToolbar.top, TRUE);
		SendMessage(m_hToolbar, TB_GETRECT, ID_STOP, (LPARAM)&rcStop);
		MoveWindow(m_hEdit, rcStop.right + 4, rcStop.top + 1, rcTabCtrl.right - 4 - (rcStop.right + 4), rcStop.bottom - rcStop.top - 2 * 1, TRUE);
	}

	void ResizeWebView()
	{
		RECT bounds, rcTabCtrl, rcToolbar;
		GetClientRect(m_hWnd, &bounds);
		GetClientRect(m_hToolbar, &rcTabCtrl);
		GetClientRect(m_hToolbar, &rcToolbar);
		bounds.top += rcTabCtrl.bottom - rcTabCtrl.top + rcToolbar.bottom - rcToolbar.top;
		GetActiveWebViewController()->put_Bounds(bounds);
	}

	void UpdateToolbarControls()
	{
		auto* sender = GetActiveWebView();

		wil::unique_cotaskmem_string uri;
		sender->get_Source(&uri);
		SetWindowText(m_hEdit, uri.get());

		BOOL canGoBack;
		BOOL canGoForward;
		sender->get_CanGoBack(&canGoBack);
		sender->get_CanGoForward(&canGoForward);
		SendMessage(m_hToolbar, TB_ENABLEBUTTON, ID_GOBACK, canGoBack);
		SendMessage(m_hToolbar, TB_ENABLEBUTTON, ID_GOFORWARD, canGoForward);

		auto* tab = GetActiveTab();
		SendMessage(m_hToolbar, TB_ENABLEBUTTON, ID_RELOAD, tab->m_navigationCompleted);
		SendMessage(m_hToolbar, TB_ENABLEBUTTON, ID_STOP, !tab->m_navigationCompleted);
	}

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
			wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
			wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
			wcex.lpszClassName = L"WinWebWindowClass";
		}
		return RegisterClassExW(&wcex);
	}

	HRESULT OnNewWindowRequested(ICoreWebView2* sender, ICoreWebView2NewWindowRequestedEventArgs* args)
	{
		GetActiveWebViewController()->put_IsVisible(false);
		ICoreWebView2Deferral* deferral;
		args->GetDeferral(&deferral);
		CWebTab* pWebTab{ new CWebTab(this, args, deferral) };
		m_tabs.emplace_back(pWebTab);
		TCITEM tcItem{};
		tcItem.mask = TCIF_TEXT;
		tcItem.pszText = (PWSTR)L"";
		m_activeTab = TabCtrl_GetItemCount(m_hTabCtrl);
		TabCtrl_InsertItem(m_hTabCtrl, m_activeTab, &tcItem);
		TabCtrl_SetCurSel(m_hTabCtrl, m_activeTab);
		return S_OK;
	}

	HRESULT OnWindowCloseRequested(ICoreWebView2* sender, IUnknown* args)
	{
		int i = FindTabIndex(sender);
		if (i < 0)
			return S_OK;
		int ntabs = TabCtrl_GetItemCount(m_hTabCtrl);
		if (ntabs == 1)
		{
			Navigate(L"about:blank");
			return S_OK;
		}
		m_tabs.erase(m_tabs.begin() + i);
		TabCtrl_DeleteItem(m_hTabCtrl, i);
		if (m_activeTab == i)
		{
			TabCtrl_SetCurSel(m_hTabCtrl, i >= ntabs ? ntabs - 1 : i);
		}
		return S_OK;
	}

	HRESULT OnNavigationStarting(ICoreWebView2* sender, ICoreWebView2NavigationStartingEventArgs* args)
	{
		UpdateToolbarControls();
		return S_OK;
	}

	HRESULT OnHistoryChanged(ICoreWebView2* sender, IUnknown* args)
	{
		UpdateToolbarControls();
		return S_OK;
	}
	
	HRESULT OnSourceChanged(ICoreWebView2* sender, ICoreWebView2SourceChangedEventArgs* args)
	{
		UpdateToolbarControls();
		return S_OK;
	}

	HRESULT OnDocumentTitleChanged(ICoreWebView2* sender, IUnknown* args)
	{
		wil::unique_cotaskmem_string title;
		sender->get_DocumentTitle(&title);
		int i = FindTabIndex(sender);
		if (i >= 0)
		{
			TCITEM tcItem{};
			tcItem.mask = TCIF_TEXT;
			tcItem.pszText = title.get();
			TabCtrl_SetItem(m_hTabCtrl, i, &tcItem);
		}
		return S_OK;
	}

	HRESULT OnNavigationCompleted(ICoreWebView2* sender, ICoreWebView2NavigationCompletedEventArgs* args)
	{
		UpdateToolbarControls();
		return S_OK;
	}

	LRESULT OnWndMsg(HWND hWnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
	{
		switch (iMsg)
		{
		case WM_COMMAND:
			switch (wParam)
			{
			case ID_GOBACK:
				GetActiveWebView()->GoBack();
				break;
			case ID_GOFORWARD:
				GetActiveWebView()->GoForward();
				break;
			case ID_RELOAD:
				GetActiveWebView()->Reload();
				break;
			case ID_STOP:
				GetActiveWebView()->Stop();
				break;
			}
			break;
		case WM_NOTIFY:
		{
			NMHDR* pnmhdr = (NMHDR*)lParam;
			if (pnmhdr->code == TCN_SELCHANGE)
			{
				GetActiveWebViewController()->put_IsVisible(false);
				m_activeTab = TabCtrl_GetCurSel(m_hTabCtrl);
				GetActiveWebViewController()->put_IsVisible(true);
				ResizeWebView();
				UpdateToolbarControls();
			}
			break;
		}
		case WM_SIZE:
			ResizeUIControls();
			if (m_activeTab != -1 && GetActiveWebViewController())
			{
				ResizeWebView();
			};
			break;
		case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hWnd, &ps);
			EndPaint(hWnd, &ps);
			return true;
		}
		default:
			return DefWindowProc(hWnd, iMsg, wParam, lParam);
		}
		return 0;
	}

	LRESULT OnEditWndMsg(HWND hWnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
	{
		if (iMsg == WM_KEYDOWN && wParam == VK_RETURN && hWnd == m_hEdit)
		{
			int length = GetWindowTextLength(m_hEdit);
			std::wstring url(length, 0);
			GetWindowText(m_hEdit, const_cast<wchar_t*>(url.data()), length + 1);
			Navigate(url.c_str());
			return 0;
		}
		return CallWindowProc(m_oldEditWndProc, hWnd, iMsg, wParam, lParam);
	}

	static LRESULT CALLBACK EditWndProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
	{
		CWebWindow* pWebWnd = reinterpret_cast<CWebWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
		LRESULT lResult = pWebWnd->OnEditWndMsg(hwnd, iMsg, wParam, lParam);
		return lResult;
	}

	static LRESULT CALLBACK WndProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
	{
		if (iMsg == WM_NCCREATE)
			SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<LPCREATESTRUCT>(lParam)->lpCreateParams));
		CWebWindow* pWebWnd = reinterpret_cast<CWebWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
		LRESULT lResult = pWebWnd->OnWndMsg(hwnd, iMsg, wParam, lParam);
		return lResult;
	}

	HWND m_hWnd = nullptr;
	HWND m_hTabCtrl = nullptr;
	HWND m_hToolbar = nullptr;
	HWND m_hEdit = nullptr;
	HFONT m_hToolbarFont = nullptr;
	HFONT m_hEditFont = nullptr;
	WNDPROC m_oldEditWndProc = nullptr;
	wil::com_ptr<ICoreWebView2Environment> m_webviewEnvironment;
	std::vector<std::unique_ptr<CWebTab>> m_tabs;
	int m_activeTab = -1;
};

