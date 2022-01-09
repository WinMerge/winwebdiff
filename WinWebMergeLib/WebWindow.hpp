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
		CWebTab(CWebWindow *parent)
			: m_parent(parent)
		{
			InitializeWebView();
		}

		bool InitializeWebView()
		{
			// Create a CoreWebView2Controller and get the associated CoreWebView2 whose parent is the main window hWnd
			HRESULT hr = m_parent->m_webviewEnvironment->CreateCoreWebView2Controller(m_parent->m_hWnd, Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
				[this](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
					if (controller != nullptr) {
						m_webviewController = controller;
						m_webviewController->get_CoreWebView2(&m_webview);
					}

					// Add a few settings for the webview
					// The demo step is redundant since the values are the default settings
					ICoreWebView2Settings* Settings;
					m_webview->get_Settings(&Settings);
					Settings->put_IsScriptEnabled(TRUE);
					Settings->put_AreDefaultScriptDialogsEnabled(TRUE);
					Settings->put_IsWebMessageEnabled(TRUE);

					m_parent->ResizeWebView();

					// Schedule an async task to navigate to Bing
					m_webview->Navigate(L"https://www.google.com/");

					EventRegistrationToken navigationStartingToken{};
					EventRegistrationToken navigationCompletedToken{};
					EventRegistrationToken sourceChangedToken{};
					EventRegistrationToken historyChangedToken{};

					m_webview->add_NavigationStarting(
						Callback<ICoreWebView2NavigationStartingEventHandler>(
							[this](ICoreWebView2* sender, ICoreWebView2NavigationStartingEventArgs* args)
							-> HRESULT {
								return m_parent->OnNavigationStarting(sender, args);
							})
						.Get(), &navigationStartingToken);

					m_webview->add_HistoryChanged(
						Callback<ICoreWebView2HistoryChangedEventHandler>(
							[this](ICoreWebView2* sender, IUnknown* args) -> HRESULT {
								return m_parent->OnHistoryChanged(sender, args);
							})
						.Get(), &historyChangedToken);

					m_webview->add_SourceChanged(
						Callback<ICoreWebView2SourceChangedEventHandler>(
							[this](ICoreWebView2* sender, ICoreWebView2SourceChangedEventArgs* args)
							-> HRESULT {
								return m_parent->OnSourceChanged(sender, args);
							})
						.Get(), &sourceChangedToken);

					m_webview->add_NavigationCompleted(
						Callback<ICoreWebView2NavigationCompletedEventHandler>(
							[this](ICoreWebView2* sender, ICoreWebView2NavigationCompletedEventArgs* args)
							-> HRESULT {
								return m_parent->OnNavigationCompleted(sender, args);
							})
						.Get(), &navigationCompletedToken);
					return S_OK;
				}).Get());
			return SUCCEEDED(hr);
		}

		wil::com_ptr<ICoreWebView2Controller> m_webviewController;
		wil::com_ptr<ICoreWebView2> m_webview;
		CWebWindow* m_parent;
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
					m_tabs.emplace_back(new CWebTab(this));

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
		if (m_hWnd)
			DestroyWindow(m_hWnd);
		m_hToolbar = nullptr;
		m_hWnd = nullptr;
		DeleteObject(m_hToolbarFont);
		DeleteObject(m_hEditFont);
		return true;
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
		SetToolbarRect();
	}

	void SetFocus()
	{
		::SetFocus(m_hWnd);
	}

private:

	void SetToolbarRect()
	{
		RECT bounds, rcToolbar, rcStop, rcTabCtrl{};
		GetClientRect(m_hWnd, &bounds);
		GetClientRect(m_hToolbar, &rcToolbar);
		TabCtrl_AdjustRect(m_hTabCtrl, false, &rcTabCtrl);
		const int tabCtrlHeight = 32; // rcTabCtrl.bottom - rcTabCtrl.top;
		const int toolbarHeight = rcToolbar.bottom - rcToolbar.top;
		MoveWindow(m_hTabCtrl, bounds.left, bounds.top, bounds.right - bounds.left, tabCtrlHeight, TRUE);
		MoveWindow(m_hToolbar, bounds.left, bounds.top + tabCtrlHeight, bounds.right - bounds.left, rcToolbar.bottom - rcToolbar.top, TRUE);
		SendMessage(m_hToolbar, TB_GETRECT, ID_STOP, (LPARAM)&rcStop);
		MoveWindow(m_hEdit, rcStop.right + 4, rcStop.top + 2, bounds.right - 4 - (rcStop.right + 4), rcStop.bottom - rcStop.top - 2 * 2, TRUE);
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

	HRESULT OnNavigationStarting(ICoreWebView2* sender, ICoreWebView2NavigationStartingEventArgs* args)
	{
		SendMessage(m_hToolbar, TB_ENABLEBUTTON, ID_STOP, true);
		return S_OK;
	}

	HRESULT OnHistoryChanged(ICoreWebView2* sender, IUnknown* args)
	{
		BOOL canGoBack;
		BOOL canGoForward;
		sender->get_CanGoBack(&canGoBack);
		sender->get_CanGoForward(&canGoForward);
		SendMessage(m_hToolbar, TB_ENABLEBUTTON, ID_GOBACK, canGoBack);
		SendMessage(m_hToolbar, TB_ENABLEBUTTON, ID_GOFORWARD, canGoForward);
		return S_OK;
	}
	
	HRESULT OnSourceChanged(ICoreWebView2* sender, ICoreWebView2SourceChangedEventArgs* args)
	{
		wil::unique_cotaskmem_string uri;
		sender->get_Source(&uri);
		SetWindowText(m_hEdit, uri.get());
		return S_OK;
	}

	HRESULT OnNavigationCompleted(ICoreWebView2* sender, ICoreWebView2NavigationCompletedEventArgs* args)
	{
		SendMessage(m_hToolbar, TB_ENABLEBUTTON, ID_STOP, false);
		SendMessage(m_hToolbar, TB_ENABLEBUTTON, ID_RELOAD, true);
		return S_OK;
	}

	void ResizeWebView()
	{
		RECT bounds, rcToolbar, rcTabCtrl;
		GetClientRect(m_hWnd, &bounds);
		GetClientRect(m_hToolbar, &rcToolbar);
		GetClientRect(m_hTabCtrl, &rcTabCtrl);
		bounds.top += rcTabCtrl.bottom - rcTabCtrl.top + rcToolbar.bottom - rcToolbar.top;
		GetActiveWebViewController()->put_Bounds(bounds);
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
		case WM_SIZE:
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

