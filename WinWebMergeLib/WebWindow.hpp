#pragma once

#include <Windows.h>
#include <CommCtrl.h>
#include <wrl.h>
#include <wil/com.h>
#include <string>
#include "WebView2.h"
#include "resource.h"

using namespace Microsoft::WRL;

class CWebWindow
{
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
		m_hWnd = CreateWindowExW(0, L"WinWebWindowClass", nullptr, WS_CHILD | WS_VISIBLE,
			0, 0, 0, 0, hWndParent, nullptr, hInstance, this);
		if (!m_hWnd)
			return false;
		m_hToolbar = CreateWindowExW(0,
			TOOLBARCLASSNAME, nullptr, WS_CHILD | TBSTYLE_FLAT | TBSTYLE_LIST | WS_VISIBLE,
			0, 0, 0, 0, m_hWnd,
			(HMENU)1, hInstance, nullptr);
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
		m_hEdit = CreateWindowEx(0, TEXT("EDIT"), TEXT(""), WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
			0, 0, 0, 0, m_hToolbar, (HMENU)2, hInstance, NULL);
		SetWindowLongPtr(m_hEdit, GWLP_USERDATA, (LONG_PTR)this);
		m_oldEditWndProc = (WNDPROC)SetWindowLongPtr(m_hEdit, GWLP_WNDPROC, (LONG_PTR)EditWndProc);
		SendMessage(m_hEdit, WM_SETFONT, (WPARAM)m_hEditFont, 0);
		SendMessage(m_hToolbar, TB_ADDBUTTONS, (WPARAM)std::size(tbb), (LPARAM)&tbb);
		SendMessage(m_hToolbar, WM_SETFONT, (WPARAM)m_hToolbarFont, 0);
		InitializeWebView();
		return true;
	}

	bool Destroy()
	{
		if (m_hToolbar)
			DestroyWindow(m_hToolbar);
		if (m_hWnd)
			DestroyWindow(m_hWnd);
		m_hToolbar = nullptr;
		m_hWnd = nullptr;
		return true;
	}

	bool Navigate(const wchar_t* url)
	{
		HRESULT hr = m_webview->Navigate(url);
		if (hr == E_INVALIDARG)
			hr = m_webview->Navigate((L"http://" + std::wstring(url)).c_str());
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
		RECT bounds, rcToolbar, rcStop;
		GetClientRect(m_hWnd, &bounds);
		GetClientRect(m_hToolbar, &rcToolbar);
		MoveWindow(m_hToolbar, bounds.left, bounds.top, bounds.right - bounds.left, rcToolbar.bottom - rcToolbar.top, TRUE);
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

	bool InitializeWebView()
	{
		// <-- WebView2 sample code starts here -->
		// Step 3 - Create a single WebView within the parent window
		// Locate the browser and set up the environment for WebView
		HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(nullptr, nullptr, nullptr,
			Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
				[this](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {

					// Create a CoreWebView2Controller and get the associated CoreWebView2 whose parent is the main window hWnd
					env->CreateCoreWebView2Controller(m_hWnd, Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
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

							// Resize WebView to fit the bounds of the parent window
							RECT bounds, rcToolbar;
							GetClientRect(m_hWnd, &bounds);
							GetClientRect(m_hToolbar, &rcToolbar);
							bounds.top += rcToolbar.bottom - rcToolbar.top;
							m_webviewController->put_Bounds(bounds);

							// Schedule an async task to navigate to Bing
							m_webview->Navigate(L"https://www.google.com/");

							/*
							// Step 4 - Navigation events
							// register an ICoreWebView2NavigationStartingEventHandler to cancel any non-https navigation
							EventRegistrationToken token;
							m_webviewWindow->add_NavigationStarting(Callback<ICoreWebView2NavigationStartingEventHandler>(
								[](ICoreWebView2* webview, ICoreWebView2NavigationStartingEventArgs* args) -> HRESULT {
									PWSTR uri;
									args->get_Uri(&uri);
									std::wstring source(uri);
									if (source.substr(0, 5) != L"https") {
										args->put_Cancel(true);
									}
									CoTaskMemFree(uri);
									return S_OK;
								}).Get(), &token);

							// Step 5 - Scripting
							// Schedule an async task to add initialization script that freezes the Object object
							m_webviewWindow->AddScriptToExecuteOnDocumentCreated(L"Object.freeze(Object);", nullptr);
							// Schedule an async task to get the document URL
							m_webviewWindow->ExecuteScript(L"window.document.URL;", Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
								[](HRESULT errorCode, LPCWSTR resultObjectAsJson) -> HRESULT {
									LPCWSTR URL = resultObjectAsJson;
									//doSomethingWithURL(URL);
									return S_OK;
								}).Get());

							// Step 6 - Communication between host and web content
							// Set an event handler for the host to return received message back to the web content
							m_webviewWindow->add_WebMessageReceived(Callback<ICoreWebView2WebMessageReceivedEventHandler>(
								[](ICoreWebView2* webview, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
									PWSTR message;
									args->TryGetWebMessageAsString(&message);
									// processMessage(&message);
									webview->PostWebMessageAsString(message);
									CoTaskMemFree(message);
									return S_OK;
								}).Get(), &token);

							// Schedule an async task to add initialization script that
							// 1) Add an listener to print message from the host
							// 2) Post document URL to the host
							m_webviewWindow->AddScriptToExecuteOnDocumentCreated(
								L"window.chrome.webview.addEventListener(\'message\', event => alert(event.data));" \
								L"window.chrome.webview.postMessage(window.document.URL);",
								nullptr);*/

							EventRegistrationToken navigationStartingToken{};
							EventRegistrationToken navigationCompletedToken{};
							EventRegistrationToken sourceChangedToken{};
							EventRegistrationToken historyChangedToken{};

							m_webview->add_NavigationStarting(
								Callback<ICoreWebView2NavigationStartingEventHandler>(
									[this](ICoreWebView2* sender, ICoreWebView2NavigationStartingEventArgs* args)
									-> HRESULT {
										SendMessage(m_hToolbar, TB_ENABLEBUTTON, ID_STOP, true);
										return S_OK;
									})
								.Get(), &navigationStartingToken);

							m_webview->add_HistoryChanged(
								Callback<ICoreWebView2HistoryChangedEventHandler>(
									[this](ICoreWebView2* sender, IUnknown* args) -> HRESULT {
										BOOL canGoBack;
										BOOL canGoForward;
										sender->get_CanGoBack(&canGoBack);
										sender->get_CanGoForward(&canGoForward);
										SendMessage(m_hToolbar, TB_ENABLEBUTTON, ID_GOBACK, canGoBack);
										SendMessage(m_hToolbar, TB_ENABLEBUTTON, ID_GOFORWARD, canGoForward);

										return S_OK;
									})
								.Get(), &historyChangedToken);

							m_webview->add_SourceChanged(
								Callback<ICoreWebView2SourceChangedEventHandler>(
									[this](ICoreWebView2* sender, ICoreWebView2SourceChangedEventArgs* args)
									-> HRESULT {
										wil::unique_cotaskmem_string uri;
										sender->get_Source(&uri);
										SetWindowText(m_hEdit, uri.get());

										return S_OK;
									})
								.Get(), &sourceChangedToken);

							m_webview->add_NavigationCompleted(
								Callback<ICoreWebView2NavigationCompletedEventHandler>(
									[this](ICoreWebView2* sender, ICoreWebView2NavigationCompletedEventArgs* args)
									-> HRESULT {
										SendMessage(m_hToolbar, TB_ENABLEBUTTON, ID_STOP, false);
										SendMessage(m_hToolbar, TB_ENABLEBUTTON, ID_RELOAD, true);
										return S_OK;
									})
								.Get(), &navigationCompletedToken);
							return S_OK;
						}).Get());
					return S_OK;
				}).Get());
		return SUCCEEDED(hr);
	}

	LRESULT OnWndMsg(HWND hWnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
	{
		switch (iMsg)
		{
		case WM_COMMAND:
			switch (wParam)
			{
			case ID_GOBACK:
				m_webview->GoBack();
				break;
			case ID_GOFORWARD:
				m_webview->GoForward();
				break;
			case ID_RELOAD:
				m_webview->Reload();
				break;
			case ID_STOP:
				m_webview->Stop();
				break;
			}
			break;
		case WM_SIZE:
			if (m_webviewController != nullptr) {
				RECT bounds, rcToolbar;
				GetClientRect(m_hWnd, &bounds);
				GetClientRect(m_hToolbar, &rcToolbar);
				bounds.top += rcToolbar.bottom - rcToolbar.top;
				m_webviewController->put_Bounds(bounds);
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
		switch (iMsg)
		{
		case WM_KEYDOWN:
			if (wParam == VK_RETURN && hWnd == m_hEdit)
			{
				int length = GetWindowTextLength(m_hEdit);
				std::wstring url(length, 0);
				GetWindowText(m_hEdit, const_cast<wchar_t*>(url.data()), length + 1);
				Navigate(url.c_str());
			}
			break;
		default:
			return CallWindowProc(m_oldEditWndProc, hWnd, iMsg, wParam, lParam);
		}
		return 0;
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

	HWND m_hWnd;
	HWND m_hToolbar;
	HWND m_hEdit;
	HFONT m_hToolbarFont;
	HFONT m_hEditFont;
	wil::com_ptr<ICoreWebView2Controller> m_webviewController;
	wil::com_ptr<ICoreWebView2> m_webview;
	WNDPROC m_oldEditWndProc;
};

