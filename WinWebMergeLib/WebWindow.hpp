#pragma once

#include <Windows.h>
#include <wrl.h>
#include <wil/com.h>
#include <string>
#include "WebView2.h"

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
		if (m_hWnd)
			InitializeWebView();
		return m_hWnd ? true : false;
	}

	bool Destroy()
	{
		if (m_hWnd)
			DestroyWindow(m_hWnd);
		m_hWnd = nullptr;
		return true;
	}

	bool Navigate(const wchar_t* url)
	{
		return SUCCEEDED(m_webview->Navigate(url));
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
	}

	void SetFocus()
	{
		::SetFocus(m_hWnd);
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
							RECT bounds;
							GetClientRect(m_hWnd, &bounds);
							m_webviewController->put_Bounds(bounds);

							// Schedule an async task to navigate to Bing
							m_webview->Navigate(L"https://www.bing.com/");

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
		case WM_SIZE:
			if (m_webviewController != nullptr) {
				RECT bounds;
				GetClientRect(hWnd, &bounds);
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

	static LRESULT CALLBACK WndProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
	{
		if (iMsg == WM_NCCREATE)
			SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<LPCREATESTRUCT>(lParam)->lpCreateParams));
		CWebWindow* pWebWnd = reinterpret_cast<CWebWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
		LRESULT lResult = pWebWnd->OnWndMsg(hwnd, iMsg, wParam, lParam);
		return lResult;
	}

	HWND m_hWnd;
	wil::com_ptr<ICoreWebView2Controller> m_webviewController;
	wil::com_ptr<ICoreWebView2> m_webview;
};

