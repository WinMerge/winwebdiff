#pragma once

#undef max
#undef min
#include <string>
#include <vector>
#include <filesystem>
#include <memory>
#include <cassert>
#include <functional>
#include <WebView2.h>
#include <WebView2EnvironmentOptions.h>
#include <CommCtrl.h>
#include <Shlwapi.h>
#include <WinInet.h>
#include <wrl.h>
#include <wil/com.h>
#include <wincrypt.h>
#include "WinWebDiffLib.h"
#include "Utils.hpp"
#include "DOMUtils.hpp"
#include "resource.h"

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "crypt32.lib")

using namespace Microsoft::WRL;
using WDocument = rapidjson::GenericDocument<rapidjson::UTF16<>>;
using WValue = rapidjson::GenericValue<rapidjson::UTF16<>>;
using WStringBuffer = rapidjson::GenericStringBuffer<rapidjson::UTF16<>>;
using WPrettyWriter = rapidjson::PrettyWriter<WStringBuffer, rapidjson::UTF16<>>;

class CWebWindow
{
	class CWebTab
	{
		friend CWebWindow;
	public:
		CWebTab(CWebWindow* parent, const wchar_t* url, double zoom, const std::wstring& userAgent, IWebDiffCallback* callback)
			: m_parent(parent)
		{
			InitializeWebView(url, zoom, userAgent, nullptr, nullptr, callback);
		}

		CWebTab(CWebWindow* parent, double zoom, const std::wstring& userAgent, ICoreWebView2NewWindowRequestedEventArgs* args = nullptr, ICoreWebView2Deferral* deferral = nullptr, IWebDiffCallback* callback = nullptr)
			: m_parent(parent)
		{
			InitializeWebView(nullptr, zoom, userAgent, args, deferral, callback);
		}

		bool InitializeWebView(const wchar_t* url, double zoom, const std::wstring& userAgent, ICoreWebView2NewWindowRequestedEventArgs* args, ICoreWebView2Deferral* deferral, IWebDiffCallback* callback)
		{
			std::shared_ptr<std::wstring> url2(url ? new std::wstring(url) : nullptr);
			ComPtr<IWebDiffCallback> callback2(callback);
			HRESULT hr = m_parent->m_webviewEnvironment->CreateCoreWebView2Controller(m_parent->m_hWebViewParent, Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
				[this, url2, zoom, userAgent, args, deferral, callback2](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
					if (FAILED(result))
					{
						m_parent->SetToolTipText(L"Failed to create WebView2 controller");
						m_parent->ShowToolTip(true);
					}

					if (controller != nullptr) {
						m_webviewController = controller;
						m_webviewController->get_CoreWebView2(&m_webview);
					}

					if (!m_webview)
					{
						if (callback2)
							return callback2->Invoke({ result, nullptr });
						return result;
					}

					m_webviewController->put_ZoomFactor(zoom);

					m_webviewController->add_AcceleratorKeyPressed(
						Callback<ICoreWebView2AcceleratorKeyPressedEventHandler>(
							[this](ICoreWebView2Controller* sender, ICoreWebView2AcceleratorKeyPressedEventArgs* args) {
								return m_parent->OnAcceleratorKeyPressed(sender, args);
							}).Get(), nullptr);

					m_webviewController->add_ZoomFactorChanged(
						Callback<ICoreWebView2ZoomFactorChangedEventHandler>(
							[this](ICoreWebView2Controller* sender, IUnknown* args) -> HRESULT {
								m_navigationCompleted = true;
								return m_parent->OnZoomFactorChanged(sender, args);
							}).Get(), nullptr);

					wil::com_ptr_t<ICoreWebView2Settings> settings;
					m_webview->get_Settings(&settings);
					if (settings)
					{
						settings->put_IsScriptEnabled(TRUE);
						settings->put_AreDefaultScriptDialogsEnabled(TRUE);
						settings->put_IsWebMessageEnabled(TRUE);
						settings->put_AreDevToolsEnabled(TRUE);
						auto settings2 = settings.try_query<ICoreWebView2Settings2>();
						if (settings2)
							settings2->put_UserAgent(userAgent.c_str());
					}

					m_webview->add_NewWindowRequested(
						Callback<ICoreWebView2NewWindowRequestedEventHandler>(
							[this](ICoreWebView2* sender, ICoreWebView2NewWindowRequestedEventArgs* args) {
								return m_parent->OnNewWindowRequested(sender, args);
							}).Get(), nullptr);

					m_webview->add_WindowCloseRequested(
						Callback<ICoreWebView2WindowCloseRequestedEventHandler>(
							[this](ICoreWebView2* sender, IUnknown* args) {
								return m_parent->OnWindowCloseRequested(sender, args);
							}).Get(), nullptr);

					m_webview->add_NavigationStarting(
						Callback<ICoreWebView2NavigationStartingEventHandler>(
							[this](ICoreWebView2* sender, ICoreWebView2NavigationStartingEventArgs* args) -> HRESULT {
								m_navigationCompleted = false;
								return m_parent->OnNavigationStarting(sender, args);
							}).Get(), nullptr);

					m_webview->add_HistoryChanged(
						Callback<ICoreWebView2HistoryChangedEventHandler>(
							[this](ICoreWebView2* sender, IUnknown* args) -> HRESULT {
								return m_parent->OnHistoryChanged(sender, args);
							}).Get(), nullptr);

					m_webview->add_SourceChanged(
						Callback<ICoreWebView2SourceChangedEventHandler>(
							[this](ICoreWebView2* sender, ICoreWebView2SourceChangedEventArgs* args) -> HRESULT {
								return m_parent->OnSourceChanged(sender, args);
							}).Get(), nullptr);

					m_webview->add_DocumentTitleChanged(
						Callback<ICoreWebView2DocumentTitleChangedEventHandler>(
							[this](ICoreWebView2* sender, IUnknown* args) -> HRESULT {
								return m_parent->OnDocumentTitleChanged(sender, args);
							}).Get(), nullptr);

					m_webview->add_NavigationCompleted(
						Callback<ICoreWebView2NavigationCompletedEventHandler>(
							[this](ICoreWebView2* sender, ICoreWebView2NavigationCompletedEventArgs* args) -> HRESULT {
								m_navigationCompleted = true;
								return m_parent->OnNavigationCompleted(sender, args);
							}).Get(), nullptr);

					m_webview->add_WebMessageReceived(
						Microsoft::WRL::Callback<ICoreWebView2WebMessageReceivedEventHandler>(
							[this](ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args)
							{
								return m_parent->OnWebMessageReceived(sender, args);
							}).Get(), nullptr);

					m_webview->CallDevToolsProtocolMethod(L"Page.enable", L"{}", nullptr);
					m_webview->CallDevToolsProtocolMethod(L"DOM.enable", L"{}", nullptr);
					m_webview->CallDevToolsProtocolMethod(L"CSS.enable", L"{}", nullptr);
					m_webview->CallDevToolsProtocolMethod(L"Overlay.enable", L"{}", nullptr);
					/*
					m_webview->CallDevToolsProtocolMethod(L"Network.enable", L"{}", nullptr);
					m_webview->CallDevToolsProtocolMethod(L"Log.enable", L"{}", nullptr);

					wil::com_ptr<ICoreWebView2DevToolsProtocolEventReceiver> receiver;
					for (const auto* event : { L"Network.requestWillBeSent", L"Network.responseReceived", L"Log.entryAdded"})
					{
						std::wstring event2 = event;
						m_webview->GetDevToolsProtocolEventReceiver(event, &receiver);
						receiver->add_DevToolsProtocolEventReceived(
							Callback<ICoreWebView2DevToolsProtocolEventReceivedEventHandler>(
								[this, event2](ICoreWebView2* sender, ICoreWebView2DevToolsProtocolEventReceivedEventArgs* args) -> HRESULT {
									return m_parent->OnDevToolsProtocolEventReceived(sender, args, event2);
								}).Get(), nullptr);
					}
					*/

					wil::com_ptr<ICoreWebView2_4> webview2_4 = m_webview.try_query<ICoreWebView2_4>();
					if (webview2_4)
					{
						webview2_4->add_FrameCreated(
							Callback<ICoreWebView2FrameCreatedEventHandler>(
								[this](ICoreWebView2* sender, ICoreWebView2FrameCreatedEventArgs* args) -> HRESULT
								{
									wil::com_ptr<ICoreWebView2Frame> webviewFrame;
									args->get_Frame(&webviewFrame);
									auto webviewFrame2 = webviewFrame.try_query<ICoreWebView2Frame2>();

									m_frames.emplace_back(webviewFrame);

									if (webviewFrame2)
									{
										webviewFrame2->add_WebMessageReceived(
											Callback<ICoreWebView2FrameWebMessageReceivedEventHandler>(
												[this](ICoreWebView2Frame* sender, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT
												{
													return m_parent->OnFrameWebMessageReceived(sender, args);
												}).Get(), nullptr);
										webviewFrame2->add_NavigationStarting(
											Callback<ICoreWebView2FrameNavigationStartingEventHandler>(
												[this](ICoreWebView2Frame* sender, ICoreWebView2NavigationStartingEventArgs* args) -> HRESULT {
													return m_parent->OnFrameNavigationStarting(sender, args);
												}).Get(), nullptr);

										webviewFrame2->add_NavigationCompleted(
											Callback<ICoreWebView2FrameNavigationCompletedEventHandler>(
												[this](ICoreWebView2Frame* sender, ICoreWebView2NavigationCompletedEventArgs* args) -> HRESULT {
													return m_parent->OnFrameNavigationCompleted(sender, args);
												}).Get(), nullptr);

									}

									webviewFrame->add_Destroyed(
										Callback<ICoreWebView2FrameDestroyedEventHandler>(
											[this](ICoreWebView2Frame* sender, IUnknown* args) -> HRESULT
											{
												auto frame =
													std::find(m_frames.begin(), m_frames.end(), sender);
												if (frame != m_frames.end())
												{
													m_frames.erase(frame);
												}
												return S_OK;
											}).Get(), nullptr);
									return S_OK;
								}).Get(), nullptr);
					}

					if (args && deferral)
					{
						args->put_NewWindow(m_webview.get());
						args->put_Handled(true);
						deferral->Complete();
						deferral->Release();
					}
					else
					{
						m_webview->Navigate(url2->c_str());
					}

					m_parent->SetActiveTab(this);

					if (callback2)
						return callback2->Invoke({ S_OK, nullptr });
					return S_OK;
				}).Get());
			return SUCCEEDED(hr);
		}

	private:
		wil::com_ptr<ICoreWebView2Controller> m_webviewController;
		wil::com_ptr<ICoreWebView2> m_webview;
		std::vector<wil::com_ptr<ICoreWebView2Frame>> m_frames;
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

	HRESULT Create(HINSTANCE hInstance, HWND hWndParent, const wchar_t* url, const wchar_t* userDataFolder,
		const SIZE& size, bool fitToWindow, double zoom, std::wstring& userAgent,
		IWebDiffCallback* callback, std::function<void(WebDiffEvent::EVENT_TYPE, IUnknown*, IUnknown*)> eventHandler)
	{
		m_fitToWindow = fitToWindow;
		m_size = size;
		m_eventHandler = eventHandler;
		MyRegisterClass(hInstance);
		m_hWnd = CreateWindowExW(0, L"WinWebWindowClass", nullptr,
			WS_CHILD | WS_VISIBLE,
			0, 0, 0, 0, hWndParent, nullptr, hInstance, this);
		if (!m_hWnd)
			return HRESULT_FROM_WIN32(GetLastError());
		m_hTabCtrl = CreateWindowEx(0, WC_TABCONTROL, nullptr,
			WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | TCS_FLATBUTTONS | TCS_FOCUSNEVER,
			0, 0, 4, 4, m_hWnd, (HMENU)1, hInstance, nullptr);
		m_hToolbar = CreateWindowExW(0, TOOLBARCLASSNAME, nullptr,
			WS_CHILD | TBSTYLE_FLAT | TBSTYLE_LIST | CCS_NOMOVEY | WS_VISIBLE,
			0, 0, 0, 0, m_hWnd, (HMENU)2, hInstance, nullptr);
		m_hWebViewParent = CreateWindowExW(0, L"WebViewParentClass", nullptr,
			WS_CHILD | WS_VISIBLE,
			0, 0, 0, 0, m_hWnd, nullptr, hInstance, this);
		m_hToolTip = CreateWindowEx(WS_EX_TOPMOST, TOOLTIPS_CLASS, nullptr,
			WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
			CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
			m_hWebViewParent, nullptr, hInstance, nullptr);
		bool bWin10orGreater = IsWin10OrGreater();
		HDC hDC = GetDC(m_hWnd);
		LOGFONT lfToolbar{};
		lfToolbar.lfHeight = MulDiv(-14, GetDeviceCaps(hDC, LOGPIXELSX), 72);
		lfToolbar.lfWeight = FW_NORMAL;
		lfToolbar.lfCharSet = DEFAULT_CHARSET;
		lfToolbar.lfOutPrecision = OUT_TT_ONLY_PRECIS;
		lfToolbar.lfQuality = PROOF_QUALITY;
		lfToolbar.lfPitchAndFamily = VARIABLE_PITCH | FF_DECORATIVE;
		wcscpy_s(lfToolbar.lfFaceName, bWin10orGreater ? L"Segoe MDL2 Assets" : L"Segoe UI Symbol");
		NONCLIENTMETRICS info{ sizeof(info) };
		SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(info), &info, 0);
		LOGFONT lfEdit = info.lfCaptionFont;
		ReleaseDC(m_hWnd, hDC);
		m_hToolbarFont = CreateFontIndirect(&lfToolbar);
		m_hEditFont = CreateFontIndirect(&lfEdit);
		SendMessage(m_hToolbar, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);
		TBBUTTON tbb[] = {
			{I_IMAGENONE, ID_GOBACK,    TBSTATE_ENABLED, BTNS_BUTTON | BTNS_AUTOSIZE, {}, 0, (INT_PTR)(bWin10orGreater ? L"\uE0A6" : L"\u25C0")},
			{I_IMAGENONE, ID_GOFORWARD, TBSTATE_ENABLED, BTNS_BUTTON | BTNS_AUTOSIZE, {}, 0, (INT_PTR)(bWin10orGreater ? L"\uE0AB" : L"\u25B6")},
			{I_IMAGENONE, ID_RELOAD,    TBSTATE_ENABLED, BTNS_BUTTON | BTNS_AUTOSIZE, {}, 0, (INT_PTR)(bWin10orGreater ? L"\uE149" : L"\u21BB")},
			{I_IMAGENONE, ID_STOP,      TBSTATE_ENABLED, BTNS_BUTTON | BTNS_AUTOSIZE, {}, 0, (INT_PTR)(bWin10orGreater ? L"\uE106" : L"\u2715")},
		};
		m_hEdit = CreateWindowEx(0, TEXT("EDIT"), TEXT(""),
			WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
			0, 0, 0, 0, m_hToolbar, (HMENU)3, hInstance, nullptr);
		SetWindowLongPtr(m_hTabCtrl, GWLP_USERDATA, (LONG_PTR)this);
		m_oldTabCtrlWndProc = (WNDPROC)SetWindowLongPtr(m_hTabCtrl, GWLP_WNDPROC, (LONG_PTR)TabCtrlWndProc);
		SetWindowLongPtr(m_hEdit, GWLP_USERDATA, (LONG_PTR)this);
		m_oldEditWndProc = (WNDPROC)SetWindowLongPtr(m_hEdit, GWLP_WNDPROC, (LONG_PTR)EditWndProc);
		SendMessage(m_hTabCtrl, WM_SETFONT, (WPARAM)m_hEditFont, 0);
		SendMessage(m_hEdit, WM_SETFONT, (WPARAM)m_hEditFont, 0);
		SendMessage(m_hToolbar, TB_ADDBUTTONS, (WPARAM)std::size(tbb), (LPARAM)&tbb);
		SendMessage(m_hToolbar, WM_SETFONT, (WPARAM)m_hToolbarFont, 0);

		m_toolItem.cbSize = sizeof(TOOLINFO);
		m_toolItem.uFlags = TTF_IDISHWND | TTF_TRACK | TTF_ABSOLUTE;
		m_toolItem.hwnd = m_hWebViewParent;
		m_toolItem.hinst = hInstance;
		m_toolItem.lpszText = const_cast<WCHAR*>(m_toolTipText.c_str());
		m_toolItem.uId = reinterpret_cast<UINT_PTR>(m_hWebViewParent);
		m_toolItem.rect = { 0, 0, 300, 100 };
		SendMessage(m_hToolTip, TTM_ADDTOOL, 0, (LPARAM)(LPTOOLINFO)&m_toolItem);

		return InitializeWebView(url, zoom, userAgent, userDataFolder, callback);
	}

	bool Destroy()
	{
		if (m_hToolTip)
			DestroyWindow(m_hToolTip);
		if (m_hWebViewParent)
			DestroyWindow(m_hWebViewParent);
		if (m_hToolbar)
			DestroyWindow(m_hToolbar);
		if (m_hTabCtrl)
			DestroyWindow(m_hTabCtrl);
		if (m_hWnd)
			DestroyWindow(m_hWnd);
		if (m_hToolbarFont)
			DeleteObject(m_hToolbarFont);
		if (m_hEditFont)
			DeleteObject(m_hEditFont);
		m_hWebViewParent = nullptr;
		m_hToolbar = nullptr;
		m_hTabCtrl = nullptr;
		m_hWnd = nullptr;
		m_hToolbarFont = nullptr;
		m_hEditFont = nullptr;
		m_tabs.clear();
		m_activeTab = -1;
		return true;
	}

	void NewTab(const wchar_t* url, double zoom, const std::wstring& userAgent, IWebDiffCallback* callback)
	{
		m_tabs.emplace_back(new CWebTab(this, url, zoom, userAgent, callback));

		TCITEM tcItem{};
		tcItem.mask = TCIF_TEXT;
		tcItem.pszText = (PWSTR)L"";
		int tabIndex = static_cast<int>(m_tabs.size()) - 1;
		TabCtrl_InsertItem(m_hTabCtrl, tabIndex, &tcItem);
	}

	HRESULT Navigate(const std::wstring& url)
	{
		if (!GetActiveWebView())
			return E_FAIL;
		HRESULT hr = GetActiveWebView()->Navigate(url.c_str());
		if (hr == E_INVALIDARG)
		{
			std::wstring url2 = (url.find(' ') == std::wstring::npos && url.find('.') != std::wstring::npos) ?
				L"http://" + url : L"https://www.google.com/search?q=" + url;
			hr = GetActiveWebView()->Navigate(url2.c_str());
		}
		return hr;
	}

	HRESULT GoBack()
	{
		if (!GetActiveWebView())
			return E_FAIL;
		return GetActiveWebView()->GoBack();
	}

	HRESULT GoForward()
	{
		if (!GetActiveWebView())
			return E_FAIL;
		return GetActiveWebView()->GoForward();
	}

	HRESULT Reload()
	{
		if (!GetActiveWebView())
			return E_FAIL;
		return GetActiveWebView()->Reload();
	}

	const wchar_t *GetCurrentUrl()
	{
		if (!GetActiveWebView())
			return L"";
		wil::unique_cotaskmem_string uri;
		GetActiveWebView()->get_Source(&uri);
		m_currentUrl = uri.get();
		return m_currentUrl.c_str();
	}

	void CloseActiveTab()
	{
		if (!GetActiveWebView())
			return;
		CloseTab(m_activeTab);
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

	bool GetFitToWindow() const
	{
		return m_fitToWindow;
	}

	void SetFitToWindow(bool fitToWindow)
	{
		m_fitToWindow = fitToWindow;
		ShowScrollbar();
		ResizeWebView();
		CalcScrollBarRange(m_nHScrollPos, m_nVScrollPos);
	}

	SIZE GetSize() const
	{
		RECT rc{};
		auto* pWebViewController = GetActiveWebViewController();
		if (pWebViewController)
			pWebViewController->get_Bounds(&rc);
		return { rc.right - rc.left, rc.bottom - rc.top };
	}

	void SetSize(const SIZE size)
	{
		m_size = size;
		if (m_fitToWindow)
			return;
		ShowScrollbar();
		ResizeWebView();
		CalcScrollBarRange(m_nHScrollPos, m_nVScrollPos);
	}

	double GetZoom() const
	{
		double zoom = 1.0;
		auto* pWebViewController = GetActiveWebViewController();
		if (pWebViewController)
			pWebViewController->get_ZoomFactor(&zoom);
		return zoom;
	}

	void SetZoom(double zoom)
	{
		auto* pWebViewController = GetActiveWebViewController();
		if (!pWebViewController)
			return;
		pWebViewController->put_ZoomFactor(std::clamp(zoom, 0.25, 5.0));
	}

	std::wstring GetUserAgent() const
	{
		if (!GetActiveWebView())
			return L"";
		wil::com_ptr_t<ICoreWebView2Settings> settings;
		wil::unique_cotaskmem_string userAgent;
		GetActiveWebView()->get_Settings(&settings);
		auto settings2 = settings.try_query<ICoreWebView2Settings2>();
		if (settings2)
			settings2->get_UserAgent(&userAgent);
		return userAgent.get();
	}

	void SetUserAgent(const std::wstring& userAgent)
	{
		if (!GetActiveWebView())
			return;
		wil::com_ptr_t<ICoreWebView2Settings> settings;
		GetActiveWebView()->get_Settings(&settings);
		auto settings2 = settings.try_query<ICoreWebView2Settings2>();
		if (settings2)
			settings2->put_UserAgent(userAgent.c_str());
	}

	bool IsFocused() const
	{
		HWND hwndCurrent = GetFocus();
		return m_hWnd == hwndCurrent || IsChild(m_hWnd, hwndCurrent);
	}

	int GetHScrollPos() const
	{
		return m_nHScrollPos;
	}

	void SetHScrollPos(int pos)
	{
		ScrollWindow(m_hWebViewParent, m_nHScrollPos - pos, 0, nullptr, nullptr);
		SetScrollPos(m_hWebViewParent, SB_HORZ, pos, TRUE);
		m_nHScrollPos = pos;
	}

	int GetVScrollPos() const
	{
		return m_nVScrollPos;
	}

	void SetVScrollPos(int pos)
	{
		ScrollWindow(m_hWebViewParent, 0, m_nVScrollPos - pos, nullptr, nullptr);
		SetScrollPos(m_hWebViewParent, SB_VERT, pos, TRUE);
		m_nVScrollPos = pos;
	}

	void SetFocus()
	{
		auto* pWebViewController = GetActiveWebViewController();
		if (!pWebViewController)
			return;
		pWebViewController->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
	}

	HRESULT SaveFile(const std::wstring& filename, IWebDiffWindow::FormatType kind, IWebDiffCallback* callback)
	{
		switch (kind)
		{
		case IWebDiffWindow::SCREENSHOT:
			return SaveScreenshot(filename, false, callback);
		case IWebDiffWindow::FULLSIZE_SCREENSHOT:
			return SaveScreenshot(filename, true, callback);
		case IWebDiffWindow::HTML:
			return SaveHTML(filename, callback);
		case IWebDiffWindow::TEXT:
			return SaveText(filename, callback);
		case IWebDiffWindow::RESOURCETREE:
			return SaveResourceTree(filename, callback);
		case IWebDiffWindow::PDF:
			return SavePDF(filename, callback);
		default:
			return E_INVALIDARG;
		}
	}

	HRESULT SaveScreenshot(const std::wstring& filename, bool fullSize, IWebDiffCallback* callback)
	{
		if (!GetActiveWebView())
			return E_FAIL;
		ComPtr<IWebDiffCallback> callback2(callback);
		HRESULT hr = CallDevToolsProtocolMethod(L"Page.getLayoutMetrics", L"{}",
			Callback<IWebDiffCallback>(
				[this, filename, fullSize, callback2](const WebDiffCallbackResult& result) -> HRESULT {
					HRESULT hr = result.errorCode;
					if (SUCCEEDED(hr))
					{
						wil::com_ptr<IStream> stream;
						hr = SHCreateStreamOnFileEx(filename.c_str(), STGM_READWRITE | STGM_CREATE, FILE_ATTRIBUTE_NORMAL, TRUE, nullptr, &stream);
						if (SUCCEEDED(hr))
						{
							RECT rcOrg;
							GetActiveWebViewController()->get_Bounds(&rcOrg);

							WDocument document;
							document.Parse(result.returnObjectAsJson);
							UINT dpi = MyGetDpiForWindow(m_hWnd);
							int width = document[L"cssContentSize"][L"width"].GetInt()
								* dpi / 96;
							int height = document[L"cssContentSize"][L"height"].GetInt()
								* dpi / 96;

							if (fullSize)
							{
								RECT rcNew{ 0, 0, width, height };
								GetActiveWebViewController()->put_Bounds(rcNew);
							}

							hr = GetActiveWebView()->CapturePreview(
								COREWEBVIEW2_CAPTURE_PREVIEW_IMAGE_FORMAT_PNG, stream.get(),
								Callback<ICoreWebView2CapturePreviewCompletedHandler>(
									[this, rcOrg, fullSize, callback2, stream](HRESULT errorCode) -> HRESULT {
										stream->Commit(STGC_DEFAULT);
										if (fullSize)
											GetActiveWebViewController()->put_Bounds(rcOrg);
										if (callback2)
											return callback2->Invoke({ errorCode, nullptr });
										return S_OK;
									}).Get());
						}
					}
					if (FAILED(hr) && callback2)
						return callback2->Invoke({ hr, nullptr });
					return S_OK;
				}).Get());
		if (FAILED(hr) && callback2)
			return callback2->Invoke({ hr, nullptr });
		return hr;
	}

	HRESULT SaveText(FILE *fp, const WValue& value, size_t& textLength)
	{
		const int nodeType = value[L"nodeType"].GetInt();

		if (nodeType == 3 /* TEXT_NODE */)
		{
			std::wstring text = value[L"nodeValue"].GetString();
			text =
				((text.length() > 0 && iswspace(text.front())) ? L" " : L"") + 
				utils::trim_ws(text) +
				((text.length() > 0 && iswspace(text.back())) ? L" " : L"");
			if (fwprintf(fp, L"%s", text.c_str()) < 0)
				return HRESULT_FROM_WIN32(GetLastError());
			textLength += text.length();
		}
		else if (nodeType == 1 /* ELEMENT_NODE */)
		{
			const wchar_t* nodeName = value[L"nodeName"].GetString();
			if (wcscmp(nodeName, L"INPUT") == 0)
			{
				const wchar_t* type = domutils::getAttribute(value, L"type");
				if (!type || wcscmp(type, L"hidden") != 0)
				{
					const wchar_t *inputValue = domutils::getAttribute(value, L"value");
					if (inputValue)
					{
						std::wstring text = inputValue;
						text =
							((text.length() > 0 && iswspace(text.front())) ? L" " : L"") +
							utils::trim_ws(text) +
							((text.length() > 0 && iswspace(text.back())) ? L" " : L"");
						if (fwprintf(fp, L"%s", text.c_str()) < 0)
							return HRESULT_FROM_WIN32(GetLastError());
						textLength += text.length();
					}
				}
			}
		}
		if (value.HasMember(L"children") && value[L"children"].IsArray())
		{
			const auto* nodeName = value[L"nodeName"].GetString();
			const bool fInline = utils::IsInlineElement(nodeName);
			if (wcscmp(nodeName, L"SCRIPT") != 0 && wcscmp(nodeName, L"STYLE") != 0)
			{
				if (nodeType == 1)
				{
					if ((!fInline && textLength > 0) || wcscmp(nodeName, L"BR") == 0 || wcscmp(nodeName, L"HR") == 0)
					{
						fwprintf(fp, L"\n");
						textLength = 0;
					}
				}
				for (const auto& child : value[L"children"].GetArray())
				{
					HRESULT hr = SaveText(fp, child, textLength);
					if (FAILED(hr))
						return hr;
				}
			}
		}
		if (value.HasMember(L"contentDocument"))
		{
			HRESULT hr = SaveText(fp, value[L"contentDocument"], textLength);
			if (FAILED(hr))
				return hr;
		}
		return S_OK;
	}

	HRESULT SaveText(const std::wstring& filename, IWebDiffCallback* callback)
	{
		if (!GetActiveWebView())
			return E_FAIL;
		ComPtr<IWebDiffCallback> callback2(callback);
		std::wstring params = L"{ \"depth\": -1, \"pierce\": true }";
		HRESULT hr = CallDevToolsProtocolMethod(L"DOM.getDocument", params.c_str(),
			Callback<IWebDiffCallback>(
				[this, filename, callback2](const WebDiffCallbackResult& result) -> HRESULT {
					HRESULT hr = result.errorCode;
					if (SUCCEEDED(hr))
					{
						WDocument document;
						document.Parse(result.returnObjectAsJson);
						wil::unique_file fp;
						_wfopen_s(&fp, filename.c_str(), L"at,ccs=UTF-8");
						size_t textLength = 0;
						hr = SaveText(fp.get(), document[L"root"], textLength);
					}
					if (callback2)
						return callback2->Invoke({ hr, nullptr });
					return S_OK;
				}).Get());
		if (FAILED(hr) && callback2)
			return callback2->Invoke({ hr, nullptr });
		return hr;
	}

	HRESULT SaveHTML(const std::wstring& filename, IWebDiffCallback* callback)
	{
		if (!GetActiveWebView())
			return E_FAIL;
		const wchar_t *script = L"document.documentElement.outerHTML";
		ComPtr<IWebDiffCallback> callback2(callback);
		HRESULT hr = GetActiveWebView()->ExecuteScript(script,
			Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
				[filename, callback2](HRESULT errorCode, LPCWSTR resultObjectAsJson) -> HRESULT {
					if (SUCCEEDED(errorCode))
					{
						WDocument document;
						document.Parse(resultObjectAsJson);
						errorCode = WriteToTextFile(filename, document.GetString());
					}
					if (callback2)
						return callback2->Invoke({ errorCode, resultObjectAsJson });
					return S_OK;
				}).Get());
		if (FAILED(hr) && callback2)
			return callback2->Invoke({ hr, nullptr });
		return hr;
	}

	HRESULT SetOuterHTML(int nodeId, const std::wstring& html, IWebDiffCallback* callback)
	{
		if (!GetActiveWebView())
			return E_FAIL;
		ComPtr<IWebDiffCallback> callback2(callback);
		std::wstring params = L"{ \"nodeId\": " + std::to_wstring(nodeId)
			+ L", \"outerHTML\":" + utils::Quote(html) + L" }";
		HRESULT hr = CallDevToolsProtocolMethod(L"DOM.setOuterHTML", params.c_str(),
			Callback<IWebDiffCallback>(
				[this, callback2](const WebDiffCallbackResult& result) -> HRESULT {
					HRESULT hr = result.errorCode;
					if (callback2)
						return callback2->Invoke({ hr, nullptr });
					return S_OK;
				}).Get());
		return hr;
	}

	HRESULT SaveFrameHTML(const std::wstring& frameId, const std::wstring& dirname, IWebDiffCallback* callback)
	{
		if (!GetActiveWebView())
			return E_FAIL;
		ComPtr<IWebDiffCallback> callback2(callback);
		std::wstring params = L"{ \"frameId\": \"" + frameId + L"\" }";
		HRESULT hr = CallDevToolsProtocolMethod(L"DOM.getFrameOwner", params.c_str(),
			Callback<IWebDiffCallback>(
				[this, dirname, callback2](const WebDiffCallbackResult& result) -> HRESULT {
					HRESULT hr = result.errorCode;
					if (SUCCEEDED(hr))
					{
						WDocument document;
						document.Parse(result.returnObjectAsJson);
						const int backendNodeId = document[L"backendNodeId"].GetInt();
						const std::wstring params = L"{ \"backendNodeId\": " + std::to_wstring(backendNodeId) + L"}";
						hr = CallDevToolsProtocolMethod(L"DOM.describeNode", params.c_str(),
							Callback<IWebDiffCallback>(
								[this, dirname, callback2](const WebDiffCallbackResult& result) -> HRESULT {
									HRESULT hr = result.errorCode;
									if (SUCCEEDED(hr))
									{
										WDocument document;
										document.Parse(result.returnObjectAsJson);
										int backendNodeId = document[L"node"][L"contentDocument"][L"backendNodeId"].GetInt();
										std::wstring params = L"{ \"backendNodeId\": " + std::to_wstring(backendNodeId) + L"}";
										hr = CallDevToolsProtocolMethod(L"DOM.getOuterHTML", params.c_str(),
											Callback<IWebDiffCallback>(
												[this, dirname, callback2](const WebDiffCallbackResult& result) -> HRESULT {
													HRESULT hr = result.errorCode;
													if (SUCCEEDED(hr))
													{
														WDocument document;
														document.Parse(result.returnObjectAsJson);
														std::filesystem::path path = dirname;
														path /= L"[Source].html";
														if (FAILED(hr = WriteToTextFile(path, document[L"outerHTML"].GetString())))
															WriteToErrorLog(dirname, path, hr);
													}
													if (callback2)
														return callback2->Invoke({ hr, nullptr });
													return S_OK;
												}).Get());
									}
									if (FAILED(hr) && callback2)
										return callback2->Invoke({ hr, nullptr });
									return S_OK;
								}).Get());
					}
					if (FAILED(hr) && callback2)
						return callback2->Invoke({ hr, nullptr });
					return S_OK;
				}).Get());
		if (FAILED(hr) && callback2)
			return callback2->Invoke({ hr, nullptr });
		return hr;
	}

	HRESULT SaveResourceContent(const std::wstring& frameId, const WValue& resource, const std::wstring& dirname, IWebDiffCallback* callback)
	{
		if (!GetActiveWebView())
			return E_FAIL;
		std::wstring url(resource[L"url"].GetString());
		std::wstring mimeType(resource[L"mimeType"].GetString());
		int64_t lastModified = resource.HasMember(L"lastModified") ? resource[L"lastModified"].GetInt64() : 0;
		std::wstring args = L"{ \"frameId\": \"" + frameId + L"\", \"url\": \"" + url + L"\" }";
		ComPtr<IWebDiffCallback> callback2(callback);
		HRESULT hr = CallDevToolsProtocolMethod(L"Page.getResourceContent", args.c_str(),
			Callback<IWebDiffCallback>(
				[this, dirname, url, mimeType, lastModified, callback2](const WebDiffCallbackResult& result) -> HRESULT {
					std::filesystem::path path(dirname);
					if (SUCCEEDED(result.errorCode))
					{
						std::wstring filename;
						if (url.compare(0, 5, L"data:") == 0)
							filename = L"[InlineData]";
						else
							filename = std::filesystem::path(url).filename();
						size_t pos = filename.find('?');
						if (pos != std::wstring::npos)
						{
							if (pos > 0)
								filename = filename.substr(0, pos);
							else
								filename = filename.substr(pos + 1);
						}

						if (filename.length() > 64)
						{
							std::filesystem::path orgfilename = filename;
							filename = orgfilename.stem().wstring().substr(0, 61) + L"\u2026" + orgfilename.extension().wstring();
						}
						path /= utils::Escape(filename);
						if (!path.has_extension())
						{
							if (mimeType == L"image/png")
								path += L".png";
							else if (mimeType == L"image/gif")
								path += L".gif";
							else if (mimeType == L"image/jpeg" || mimeType == L"image/jpg")
								path += L".jpg";
							else if (mimeType == L"image/svg" || mimeType == L"image/svg+xml")
								path += L".svg";
						}

						if (std::filesystem::exists(path))
							path = RenameFile(path);

						WDocument document;
						document.Parse(result.returnObjectAsJson);
						HRESULT errorCode;
						if (document[L"base64Encoded"].GetBool())
						{
							if (FAILED(errorCode = WriteToBinaryFile(path, document[L"content"].GetString())))
								WriteToErrorLog(dirname, url, errorCode);
						}
						else
						{
							if (FAILED(errorCode = WriteToTextFile(path, document[L"content"].GetString())))
								WriteToErrorLog(dirname, url, errorCode);
						}
						if (lastModified > 0)
							SetLastModifed(path, lastModified);
					}
					else
					{
						WriteToErrorLog(dirname, url, result.errorCode);
					}
					if (callback2)
						return callback2->Invoke({ result.errorCode, result.returnObjectAsJson });
					return S_OK;
				}).Get());
		if (FAILED(hr) && callback2)
			return callback2->Invoke({ hr, nullptr });
		return hr;
	}

	uint32_t GetResourceTreeItemCount(const WValue& frameTree)
	{
		uint32_t count = 0;
		if (frameTree.HasMember(L"childFrames") && frameTree[L"childFrames"].IsArray())
		{
			for (const auto& frame : frameTree[L"childFrames"].GetArray())
				count += GetResourceTreeItemCount(frame);
		}
		if (frameTree.HasMember(L"frame"))
		{
			count += frameTree[L"resources"].GetArray().Size();
			++count;
		}
		return count;
	}

	HRESULT SaveResourceTree(bool root, const std::wstring& dirname, const WValue& frameTree, IWebDiffCallback* callback)
	{
		if (frameTree.HasMember(L"childFrames"))
		{
			int i = 1;
			size_t siz = frameTree[L"childFrames"].GetArray().Size();
			int n = ([](size_t siz) { int n = 1; while (siz /= 10) ++n; return n; })(siz);
			for (const auto& frame : frameTree[L"childFrames"].GetArray())
			{
				wchar_t buf[32];
				swprintf_s(buf, L"%0*d", n, i);
				std::wstring frameId = buf;
				SaveResourceTree(false, (std::filesystem::path(dirname) / (L"Frame" + frameId)), frame, callback);
				++i;
			}
		}
		if (frameTree.HasMember(L"frame"))
		{
			if (!std::filesystem::exists(dirname))
				std::filesystem::create_directories(dirname);
			std::wstring frameId = frameTree[L"frame"][L"id"].GetString();
			for (const auto& resource : frameTree[L"resources"].GetArray())
			{
				std::filesystem::path path = dirname;
				path /= resource[L"type"].GetString();
				if (!std::filesystem::exists(path))
					std::filesystem::create_directories(path);
				SaveResourceContent(frameId, resource, path, callback);
			}
			if (root)
				SaveHTML(std::filesystem::path(dirname) / L"[Source].html", callback);
			else
				SaveFrameHTML(frameId, dirname, callback);
		}
		return S_OK;
	}

	HRESULT SaveResourceTree(const std::wstring& dirname, IWebDiffCallback* callback)
	{
		if (!GetActiveWebView())
			return E_FAIL;
		ComPtr<IWebDiffCallback> callback2(callback);
		HRESULT hr = CallDevToolsProtocolMethod(L"Page.getResourceTree", L"{}",
			Callback<IWebDiffCallback>(
				[this, dirname, callback2](const WebDiffCallbackResult& result) -> HRESULT {
					WDocument document;
					document.Parse(result.returnObjectAsJson);
					const WValue& tree = document[L"frameTree"];

					std::filesystem::path path(dirname);
					path /= L"[ResourceTree].json";
					WStringBuffer buffer;
					WPrettyWriter writer(buffer);
					document.Accept(writer);
					HRESULT errorCode;
					if (FAILED(errorCode = WriteToTextFile(path, buffer.GetString())))
						WriteToErrorLog(dirname, L"[ResourceTree].json", errorCode);

					uint32_t size = GetResourceTreeItemCount(tree);
					std::shared_ptr<unsigned> counter(new unsigned{ size });
					SaveResourceTree(true, dirname, tree,
						Callback<IWebDiffCallback>(
							[this, counter, callback2](const WebDiffCallbackResult& result) -> HRESULT {
								--*counter;
								if (*counter == 0)
									return callback2->Invoke(result);
								return S_OK;
							}).Get());
					return S_OK;
				}).Get());
		if (FAILED(hr) && callback2)
			return callback2->Invoke({ hr, nullptr });
		return hr;
	}

	HRESULT SavePDF(const std::wstring& filename, IWebDiffCallback* callback)
	{
		if (!GetActiveWebView())
			return E_FAIL;
		ComPtr<IWebDiffCallback> callback2(callback);
		wil::com_ptr<ICoreWebView2_7> webview2_7 = GetActiveTab()->m_webview.try_query<ICoreWebView2_7>();
		if (!webview2_7)
			return E_NOINTERFACE;
		wil::com_ptr<ICoreWebView2PrintSettings> printSettings;
		wil::com_ptr<ICoreWebView2Environment6> webviewEnvironment6 = m_webviewEnvironment.try_query<ICoreWebView2Environment6>();
		if (webviewEnvironment6)
		{
			webviewEnvironment6->CreatePrintSettings(&printSettings);
			printSettings->put_ShouldPrintBackgrounds(true);
		}
		HRESULT hr = webview2_7->PrintToPdf(filename.c_str(), printSettings.get(),
			Callback<ICoreWebView2PrintToPdfCompletedHandler>(
				[callback2](HRESULT errorCode, BOOL isSucessful) -> HRESULT {
					if (callback2)
						return callback2->Invoke({ errorCode, nullptr});
					return S_OK;
				}).Get());
		if (FAILED(hr) && callback2)
			return callback2->Invoke({ hr, nullptr });
		return hr;
	}

	HRESULT CallDevToolsProtocolMethod(const wchar_t* methodName, const wchar_t* params, IWebDiffCallback *callback, bool showError = true)
	{
		if (!GetActiveWebView())
			return E_FAIL;
		ComPtr<IWebDiffCallback> callback2(callback);
		std::shared_ptr<std::wstring> msg;
		if (showError)
		{
			msg = std::make_shared<std::wstring>();
			*msg += L"Error when calling ";
			*msg += methodName;
			*msg += L"(";
			*msg += params;
			*msg += L")\n";
		}
		HRESULT hr = GetActiveWebView()->CallDevToolsProtocolMethod(methodName, params,
			Callback<ICoreWebView2CallDevToolsProtocolMethodCompletedHandler>(
				[this, callback2, msg](HRESULT errorCode, LPCWSTR returnObjectAsJson) -> HRESULT {
					if (FAILED(errorCode))
					{
						SetToolTipText(*msg + returnObjectAsJson);
						ShowToolTip(true, TOOLTIP_TIMEOUT);
					}
					if (callback2)
						return callback2->Invoke({ errorCode, returnObjectAsJson });
					return S_OK;
				}).Get());
		return hr;
	}

	HRESULT ExecuteScript(const wchar_t* script, IWebDiffCallback *callback)
	{
		if (!GetActiveWebView())
			return E_FAIL;
		ComPtr<IWebDiffCallback> callback2(callback);
		HRESULT hr = GetActiveWebView()->ExecuteScript(script,
			Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
				[this, callback2](HRESULT errorCode, LPCWSTR resultObjectAsJson) -> HRESULT {
					if (FAILED(errorCode))
					{
						SetToolTipText(resultObjectAsJson);
						ShowToolTip(true, TOOLTIP_TIMEOUT);
					}
					if (callback2)
						return callback2->Invoke({ errorCode, resultObjectAsJson });
					return S_OK;
				}).Get());
		return hr;
	}

	HRESULT ExecuteScript(IUnknown* target, const wchar_t* script, IWebDiffCallback *callback)
	{
		if (!GetActiveWebView())
			return E_FAIL;
		ComPtr<IWebDiffCallback> callback2(callback);
		wil::com_ptr<ICoreWebView2Frame2> frame;
		target->QueryInterface<ICoreWebView2Frame2>(&frame);
		if (frame)
			return frame->ExecuteScript(script,
				Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
					[this, callback2](HRESULT errorCode, LPCWSTR resultObjectAsJson) -> HRESULT {
						if (FAILED(errorCode))
						{
							SetToolTipText(resultObjectAsJson);
							ShowToolTip(true, TOOLTIP_TIMEOUT);
						}
						if (callback2)
							return callback2->Invoke({ errorCode, resultObjectAsJson });
						return S_OK;
					}).Get());
		wil::com_ptr<ICoreWebView2> webview;
		target->QueryInterface<ICoreWebView2>(&webview);
		if (webview)
			return webview->ExecuteScript(script,
				Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
					[this, callback2](HRESULT errorCode, LPCWSTR resultObjectAsJson) -> HRESULT {
						if (FAILED(errorCode))
						{
							SetToolTipText(resultObjectAsJson);
							ShowToolTip(true, TOOLTIP_TIMEOUT);
						}
						if (callback2)
							return callback2->Invoke({ errorCode, resultObjectAsJson });
						return S_OK;
					}).Get());
		return E_FAIL;
	}

	HRESULT executeScriptLoop(std::shared_ptr<std::wstring> script, IWebDiffCallback *callback,
		std::vector<wil::com_ptr<ICoreWebView2Frame>>::iterator it)
	{
		if (!GetActiveTab())
			return E_FAIL;
		if (it == GetActiveTab()->m_frames.end())
		{
			if (callback)
				return callback->Invoke({ S_OK, nullptr });
			return S_OK;
		}
		ComPtr<IWebDiffCallback> callback2(callback);
		if (*it == nullptr)
			return E_FAIL;
		wil::com_ptr<ICoreWebView2Frame2> frame2 =
			it->try_query<ICoreWebView2Frame2>();
		if (!frame2)
			return E_FAIL;
		HRESULT hr = frame2->ExecuteScript(script->c_str(),
			Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
				[this, it, callback2, script](HRESULT errorCode, LPCWSTR resultObjectAsJson) -> HRESULT {
					HRESULT hr = errorCode;
					if (SUCCEEDED(hr))
					{
						std::vector<wil::com_ptr<ICoreWebView2Frame>>::iterator it2(it);
						++it2;
						if (it2 != GetActiveTab()->m_frames.end())
							hr = executeScriptLoop(script, callback2.Get(), it2);
						else if (callback2)
							return callback2->Invoke({ hr, nullptr });
					}
					if (FAILED(hr) && callback2)
						return callback2->Invoke({ errorCode, resultObjectAsJson });
					return S_OK;
				}).Get());
		return hr;
	}

	HRESULT ExecuteScriptInAllFrames(const wchar_t* script, IWebDiffCallback *callback)
	{
		if (!GetActiveWebView())
			return E_FAIL;
		ComPtr<IWebDiffCallback> callback2(callback);
		auto script2 = std::make_shared<std::wstring>(script);
		HRESULT hr = GetActiveWebView()->ExecuteScript(script,
			Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
				[this, callback2, script2](HRESULT errorCode, LPCWSTR resultObjectAsJson) -> HRESULT {
					HRESULT hr = errorCode;
					if (SUCCEEDED(hr))
						hr = executeScriptLoop(script2, callback2.Get(), GetActiveTab()->m_frames.begin());
					if (FAILED(hr) && callback2)
						return callback2->Invoke({ errorCode, nullptr });
					return S_OK;
				}).Get());
		return hr;
	}

	HRESULT PostWebMessageAsJsonInAllFrames(const wchar_t* json)
	{
		if (!GetActiveWebView())
			return E_FAIL;
		auto script2 = std::make_shared<std::wstring>(json);
		HRESULT hr = GetActiveWebView()->PostWebMessageAsJson(json);
		if (FAILED(hr))
			return hr;
		for (auto frame : GetActiveTab()->m_frames)
		{
			auto frame3 = frame.try_query<ICoreWebView2Frame3>();
			if (frame3)
			{
				hr = frame3->PostWebMessageAsJson(json);
				if (FAILED(hr))
					return hr;
			}
		}
		return S_OK;
	}

	HRESULT setStyleSheetText(const std::wstring& styleSheetId, const std::wstring& styles, IWebDiffCallback* callback)
	{
		static const wchar_t* method = L"CSS.setStyleSheetText";
		std::wstring params = L"{ \"styleSheetId\": \"" + styleSheetId + L"\", "
			L"\"text\": " + utils::Quote(styles) + L" }";
		ComPtr<IWebDiffCallback> callback2(callback);
		HRESULT hr = CallDevToolsProtocolMethod(method, params.c_str(),
			Callback<IWebDiffCallback>([this,  callback2](const WebDiffCallbackResult& result) -> HRESULT
				{
					HRESULT hr = result.errorCode;
					if (callback2)
						return callback2->Invoke({ hr, nullptr });
					return S_OK;
				}).Get());
		return hr;
	}

	HRESULT setFrameStyleSheet(const std::wstring& frameId, const std::wstring& styles, IWebDiffCallback* callback)
	{
		static const wchar_t* method = L"CSS.createStyleSheet";
		std::wstring params = L"{ \"frameId\": \"" + frameId + L"\" }";
		ComPtr<IWebDiffCallback> callback2(callback);
		HRESULT hr = CallDevToolsProtocolMethod(method, params.c_str(),
			Callback<IWebDiffCallback>([this, styles, callback2](const WebDiffCallbackResult& result) -> HRESULT
				{
					HRESULT hr = result.errorCode;
					if (SUCCEEDED(hr))
					{
						WDocument document;
						document.Parse(result.returnObjectAsJson);
						std::wstring styleSheetId = document[L"styleSheetId"].GetString();
						hr = setStyleSheetText(styleSheetId, styles, callback2.Get());
					}
					if (FAILED(hr) && callback2)
						return callback2->Invoke({ hr, nullptr });
					return S_OK;
				}).Get());
		return hr;
	}

	HRESULT setFrameStyleSheetLoop(
		std::shared_ptr<std::vector<std::wstring>> frameIdList,
		std::shared_ptr<const std::wstring> styles,
		IWebDiffCallback* callback,
		std::vector<std::wstring>::iterator it)
	{
		if (it == frameIdList->end())
		{
			if (callback)
				callback->Invoke({ S_OK, nullptr });
			return S_OK;
		}
		ComPtr<IWebDiffCallback> callback2(callback);
		HRESULT hr = setFrameStyleSheet(*it, *styles,
			Callback<IWebDiffCallback>([this, frameIdList, styles, it, callback2](const WebDiffCallbackResult& result) -> HRESULT
				{
					HRESULT hr = S_OK; // result.errorCode;
					if (SUCCEEDED(hr))
					{
						std::vector<std::wstring>::iterator it2(it);
						++it2;
						if (it2 != frameIdList->end())
							hr = setFrameStyleSheetLoop(frameIdList, styles, callback2.Get(), it2);
						else if (callback2)
							return callback2->Invoke({ hr, nullptr });
					}
					if (FAILED(hr) && callback2)
						return callback2->Invoke({ hr, nullptr });
					return S_OK;
				}).Get());
		return hr;
	}

	HRESULT SetStyleSheetInAllFrames(const std::wstring& styles, IWebDiffCallback* callback)
	{
		static const wchar_t* method = L"Page.getFrameTree";
		static const wchar_t* params = L"{}";
		ComPtr<IWebDiffCallback> callback2(callback);
		auto styles2 = std::make_shared<std::wstring>(styles);
		HRESULT hr = CallDevToolsProtocolMethod(method, params,
			Callback<IWebDiffCallback>([this, styles2, callback2](const WebDiffCallbackResult& result) -> HRESULT
				{
					HRESULT hr = result.errorCode;
					if (SUCCEEDED(hr))
					{
						WDocument document;
						document.Parse(result.returnObjectAsJson);
						std::shared_ptr<std::vector<std::wstring>> frameIdList(new std::vector<std::wstring>());
						domutils::getFrameIdList(document[L"frameTree"], *frameIdList);
						hr = setFrameStyleSheetLoop(frameIdList, styles2,
							callback2.Get(), frameIdList->begin());
					}
					if (FAILED(hr) && callback2)
						return callback2->Invoke({ hr, nullptr });
					return S_OK;
				}).Get());
		return hr;
	}

	HRESULT ClearBrowsingData(IWebDiffWindow::BrowsingDataType dataKinds)
	{
		if (!GetActiveWebView())
			return E_FAIL;
		auto webView2_13 = GetActiveTab()->m_webview.try_query<ICoreWebView2_13>();
		if (!webView2_13)
			return E_FAIL;
		wil::com_ptr<ICoreWebView2Profile> webView2Profile;
		webView2_13->get_Profile(&webView2Profile);
		if (!webView2Profile)
			return E_FAIL;
		auto webView2Profile2 = webView2Profile.try_query<ICoreWebView2Profile2>();
		return webView2Profile2->ClearBrowsingData(static_cast<COREWEBVIEW2_BROWSING_DATA_KINDS>(dataKinds), nullptr);
	}

	HRESULT SetToolTipText(const std::wstring& text)
	{
		if (!m_hToolTip)
			return E_FAIL;
		m_toolTipText = text;
		m_toolItem.lpszText = const_cast<WCHAR *>(m_toolTipText.c_str());
		SendMessage(m_hToolTip, TTM_SETTOOLINFO, (WPARAM)TRUE, (LPARAM)&m_toolItem);
		return S_OK;
	}

	HRESULT ShowToolTip(bool show, int timeoutms = -1)
	{
		m_showToolTip = show;
		if (!m_hToolTip)
			return E_FAIL;
		if (show)
		{
			RECT rc{};
			GetClientRect(m_hWebViewParent, &rc);
			POINT pt{rc.left, rc.top};
			ClientToScreen(m_hWebViewParent, &pt);
			SendMessage(m_hToolTip, TTM_SETMAXTIPWIDTH, 0, rc.right - rc.left);
			SendMessage(m_hToolTip, TTM_TRACKACTIVATE, (WPARAM)TRUE, (LPARAM)&m_toolItem);
			SendMessage(m_hToolTip, TTM_TRACKPOSITION, 0, (LPARAM)MAKELONG(pt.x + 10, pt.y + 10));
			if (timeoutms != -1)
				SetTimer(m_hWnd, ID_TOOLTIP_TIMER, timeoutms, nullptr);
		}
		else
		{
			SendMessage(m_hToolTip, TTM_TRACKACTIVATE, (WPARAM)FALSE, (LPARAM)&m_toolItem);
		}
		return S_OK;
	}

	std::wstring GetWebMessage() const
	{
		return m_webmessage;
	}

private:

	HRESULT InitializeWebView(const wchar_t* url, double zoom, const std::wstring& userAgent, const wchar_t* userDataFolder, IWebDiffCallback* callback)
	{
		std::shared_ptr<std::wstring> url2(new std::wstring(url));
		HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(nullptr, userDataFolder, nullptr,
			Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
				[this, url2, zoom, userAgent, callback](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
					m_webviewEnvironment = env;
					m_tabs.emplace_back(new CWebTab(this, url2->c_str(), zoom, userAgent, callback));

					TCITEM tcItem{};
					tcItem.mask = TCIF_TEXT;
					tcItem.pszText = (PWSTR)L"";
					m_activeTab = static_cast<int>(m_tabs.size()) - 1;
					TabCtrl_InsertItem(m_hTabCtrl, m_activeTab, &tcItem);
					TabCtrl_SetCurSel(m_hTabCtrl, m_activeTab);
					return S_OK;
				}).Get());
		return hr;
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

	void SetActiveTab(const CWebTab *ptab)
	{
		int tabIndex = FindTabIndex(ptab->m_webview.get());
		if (tabIndex >= 0)
			SetActiveTab(tabIndex);
	}

	void SetActiveTab(int tabIndex)
	{
		if (tabIndex < 0 || tabIndex >= static_cast<int>(m_tabs.size()))
			return;
		if (!GetActiveWebViewController())
			return;
		CalcScrollBarRange(0, 0);
		GetActiveWebViewController()->put_IsVisible(false);
		int oldActiveTab = TabCtrl_GetCurSel(m_hTabCtrl);
		if (oldActiveTab != tabIndex)
			TabCtrl_SetCurSel(m_hTabCtrl, tabIndex);
		m_activeTab = tabIndex;
		GetActiveWebViewController()->put_IsVisible(true);
		ShowScrollbar();
		ResizeWebView();
		CalcScrollBarRange(m_nHScrollPos, m_nVScrollPos);
		UpdateToolbarControls();
		m_eventHandler(WebDiffEvent::TabChanged, nullptr, nullptr);
	}

	CWebTab* GetActiveTab() const
	{
		assert(m_activeTab >= 0 && m_activeTab < static_cast<int>(m_tabs.size()));
		return m_tabs[m_activeTab].get();
	}

	HRESULT CloseTab(int tab)
	{
		int ntabs = static_cast<int>(m_tabs.size());
		if (m_tabs.size() == 1)
			return Navigate(L"about:blank");
		if (m_activeTab == tab)
			SetActiveTab(tab == ntabs - 1 ? ntabs - 2 : tab + 1);
		m_tabs.erase(m_tabs.begin() + tab);
		TabCtrl_DeleteItem(m_hTabCtrl, tab);
		ntabs = static_cast<int>(m_tabs.size());
		if (m_activeTab >= ntabs)
			m_activeTab = ntabs - 1;
		return S_OK;
	}

	ICoreWebView2* GetActiveWebView() const
	{
		if (m_activeTab < 0)
			return nullptr;
		return GetActiveTab()->m_webview.get();
	}

	ICoreWebView2Controller* GetActiveWebViewController() const
	{
		return GetActiveTab()->m_webviewController.get();
	}

	void ResizeUIControls()
	{
		if (!m_hWnd)
			return;
		RECT bounds{}, rcToolbar{}, rcStop{}, rcClient{};
		GetClientRect(m_hWnd, &bounds);
		rcClient = bounds;
		TabCtrl_AdjustRect(m_hTabCtrl, false, &rcClient);
		MoveWindow(m_hTabCtrl, bounds.left, bounds.top, bounds.right - bounds.left, rcClient.top, TRUE);
		GetClientRect(m_hTabCtrl, &rcClient);
		TabCtrl_AdjustRect(m_hTabCtrl, false, &rcClient);
		GetClientRect(m_hToolbar, &rcToolbar);
		MoveWindow(m_hToolbar, rcClient.left, rcClient.top, rcClient.right - rcClient.left, rcToolbar.bottom - rcToolbar.top, TRUE);
		SendMessage(m_hToolbar, TB_GETRECT, ID_STOP, (LPARAM)&rcStop);
		MoveWindow(m_hEdit, rcStop.right + 4, rcStop.top + 1, rcClient.right - 4 - (rcStop.right + 4), rcStop.bottom - rcStop.top - 2 * 1, TRUE);
		RECT rcWebViewParent{ 0, rcClient.top + rcToolbar.bottom, bounds.right, bounds.bottom };
		MoveWindow(m_hWebViewParent, rcWebViewParent.left, rcWebViewParent.top, rcWebViewParent.right - rcWebViewParent.left, rcWebViewParent.bottom - rcWebViewParent.top, TRUE);
	}

	void ShowScrollbar()
	{
		if (m_fitToWindow)
		{
			ShowScrollBar(m_hWebViewParent, SB_BOTH, false);
		}
		else
		{
			ShowScrollBar(m_hWebViewParent, SB_BOTH, true);
			RECT rc{};
			GetClientRect(m_hWebViewParent, &rc);
			if (m_size.cx < rc.right - rc.left)
				ShowScrollBar(m_hWebViewParent, SB_HORZ, false);
			if (m_size.cy < rc.bottom - rc.top)
				ShowScrollBar(m_hWebViewParent, SB_VERT, false);
		}
	}

	void ResizeWebView()
	{
		auto* pWebViewController = GetActiveWebViewController();
		if (!pWebViewController)
			return;
		RECT bounds;
		GetClientRect(m_hWebViewParent, &bounds);
		if (m_fitToWindow)
		{
			pWebViewController->put_Bounds(bounds);
		}
		else
		{
			if (m_size.cx < bounds.right - bounds.left)
				bounds.left += (bounds.right - bounds.left - m_size.cx) / 2;
			if (m_size.cy < bounds.bottom - bounds.top)
				bounds.top += (bounds.bottom - bounds.top - m_size.cy) / 2;
			bounds.right = bounds.left + m_size.cx;
			bounds.bottom = bounds.top + m_size.cy;
			pWebViewController->put_Bounds(bounds);
		}
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

	void CalcScrollBarRange(int nHScrollPos, int nVScrollPos)
	{
		if (!m_fitToWindow && m_activeTab >= 0)
		{
			RECT rc;
			GetClientRect(m_hWebViewParent, &rc);
			SCROLLINFO si{ sizeof(SCROLLINFO) };

			SIZE size = GetSize();

			si.fMask = SIF_POS | SIF_RANGE | SIF_PAGE | SIF_DISABLENOSCROLL;
			si.nMin = 0;
			si.nMax = size.cy;
			si.nPage = rc.bottom;
			si.nPos = nVScrollPos;
			SetScrollInfo(m_hWebViewParent, SB_VERT, &si, TRUE);
			m_nVScrollPos = GetScrollPos(m_hWebViewParent, SB_VERT);

			si.nMin = 0;
			si.nMax = size.cx;
			si.nPage = rc.right;
			si.nPos = nHScrollPos;
			SetScrollInfo(m_hWebViewParent, SB_HORZ, &si, TRUE);
			m_nHScrollPos = GetScrollPos(m_hWebViewParent, SB_HORZ);
		}
	}

	static std::filesystem::path RenameFile(const std::filesystem::path& path)
	{
		int i = 1;
		std::filesystem::path path2 = path;
		std::wstring orgstem = path2.stem();
		std::wstring orgext = path2.extension();
		while (std::filesystem::exists(path2))
			path2.replace_filename(orgstem + L"(" + std::to_wstring(i++) + L")" + orgext);
		return path2;
	}

	static void WriteToErrorLog(const std::wstring& dirname, const std::wstring& url, HRESULT hr)
	{
		std::wstring msg;
		LPTSTR errorText = nullptr;
		FormatMessage(
			FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS,
			nullptr, hr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&errorText, 0, nullptr);
		if (errorText)
		{
			msg = errorText;
			LocalFree(errorText);
		}
		wil::unique_file fp;
		std::filesystem::path path(dirname);
		path /= L"[Error].log";
		_wfopen_s(&fp, path.c_str(), L"at,ccs=UTF-8");
		fwprintf(fp.get(), L"url=%s hr=%08x: %s", url.c_str(), hr, msg.c_str());
	}

	static HRESULT WriteToTextFile(const std::wstring& path, const std::wstring& data)
	{
		wil::unique_file fp;
		_wfopen_s(&fp, path.c_str(), L"wt,ccs=UTF-8");
		if (fp)
			fwprintf(fp.get(), L"%s", data.c_str());
		return fp != nullptr ? S_OK : (GetLastError() == 0 ? E_FAIL : HRESULT_FROM_WIN32(GetLastError()));
	}

	static HRESULT WriteToBinaryFile(const std::wstring& path, const std::wstring& base64)
	{
		wil::unique_file fp;
		std::vector<BYTE> data = utils::DecodeBase64(base64);
		_wfopen_s(&fp, path.c_str(), L"wb");
		if (fp)
			fwrite(data.data(), data.size(), 1, fp.get());
		return fp != nullptr ? S_OK : (GetLastError() == 0 ? E_FAIL : HRESULT_FROM_WIN32(GetLastError()));
	}

	static void TimetToFileTime(time_t t, LPFILETIME pft)
	{
		ULARGE_INTEGER time_value{};
		time_value.QuadPart = (t * 10000000LL) + 116444736000000000LL;
		pft->dwLowDateTime = time_value.LowPart;
		pft->dwHighDateTime = time_value.HighPart;
	}

	static bool SetLastModifed(const std::wstring& path, int64_t lastModified)
	{
		if (lastModified == 0)
			return false;
		wil::unique_hfile file(CreateFile(path.c_str(),
			GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL, nullptr));
		if (!file)
			return false;
		FILETIME ft{};
		TimetToFileTime(lastModified, &ft);
		return SetFileTime(file.get(), nullptr, nullptr, &ft);
	}

	bool MyRegisterClass(HINSTANCE hInstance)
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
		ATOM atom = RegisterClassExW(&wcex);
		if (atom == 0)
			return false;
		if (!GetClassInfoEx(hInstance, L"WebViewParentClass", &wcex))
		{
			wcex.cbSize = sizeof(WNDCLASSEX);
			wcex.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
			wcex.lpfnWndProc = (WNDPROC)WebViewParentWndProc;
			wcex.cbClsExtra = 0;
			wcex.cbWndExtra = 0;
			wcex.hInstance = hInstance;
			wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
			wcex.hbrBackground = CreateSolidBrush(RGB(206, 215, 230));
			wcex.lpszClassName = L"WebViewParentClass";
		}
		return RegisterClassExW(&wcex) != 0;
	}

	HRESULT OnAcceleratorKeyPressed(ICoreWebView2Controller* sender, ICoreWebView2AcceleratorKeyPressedEventArgs* args)
	{
		COREWEBVIEW2_KEY_EVENT_KIND kind{};
		UINT virtualKey = 0;
		int lParam = 0;
		args->get_KeyEventKind(&kind);
		args->get_VirtualKey(&virtualKey);
		args->get_KeyEventLParam(&lParam);
		if (kind == COREWEBVIEW2_KEY_EVENT_KIND_KEY_DOWN || kind == COREWEBVIEW2_KEY_EVENT_KIND_SYSTEM_KEY_DOWN)
		{
			bool handled = false;
			short vkmenu = GetAsyncKeyState(VK_MENU);
			short vkctrl = GetAsyncKeyState(VK_CONTROL);
			if (virtualKey == VK_F6 ||
			    virtualKey == VK_ESCAPE ||
			    (vkctrl && virtualKey == 'O') ||
			    (vkctrl && virtualKey == 'J') ||
			    (vkctrl && virtualKey == 'W') ||
			    (vkmenu && virtualKey == VK_UP) ||
			    (vkmenu && virtualKey == VK_DOWN))
			{
				PostMessage(GetParent(m_hWnd), WM_KEYDOWN, virtualKey, lParam);
				handled = true;
			}
			else if (virtualKey == VK_F12)
			{
				GetActiveWebView()->OpenDevToolsWindow();
			}
			else if ((vkmenu && virtualKey == 'D') || (vkctrl && virtualKey == 'L'))
			{
				::SendMessage(m_hEdit, EM_SETSEL, 0, -1);
				::SetFocus(m_hEdit);
				handled = true;
			}
			else if (vkmenu && 'A' <= virtualKey && virtualKey <= 'Z')
			{
				PostMessage(m_hWnd, WM_SYSKEYDOWN, virtualKey, lParam);
				handled = true;
			}
			else if (vkctrl && (virtualKey == 'K' || virtualKey == 'E'))
			{
				SetWindowText(m_hEdit, L"");
				::SetFocus(m_hEdit);
				handled = true;
			}
			if (handled)
				args->put_Handled(handled);
		}
		return S_OK;
	}

	HRESULT OnZoomFactorChanged(ICoreWebView2Controller* sender, IUnknown* args)
	{
		m_eventHandler(WebDiffEvent::ZoomFactorChanged, sender, args);
		return S_OK;
	}

	HRESULT OnNewWindowRequested(ICoreWebView2* sender, ICoreWebView2NewWindowRequestedEventArgs* args)
	{
		CalcScrollBarRange(0, 0);

		GetActiveWebViewController()->put_IsVisible(false);
		ICoreWebView2Deferral* deferral;
		args->GetDeferral(&deferral);
		CWebTab* pWebTab{ new CWebTab(this, GetZoom(), GetUserAgent(), args, deferral)};
		m_tabs.emplace_back(pWebTab);
		TCITEM tcItem{};
		tcItem.mask = TCIF_TEXT;
		tcItem.pszText = (PWSTR)L"";
		m_activeTab = TabCtrl_GetItemCount(m_hTabCtrl);
		TabCtrl_InsertItem(m_hTabCtrl, m_activeTab, &tcItem);
		TabCtrl_SetCurSel(m_hTabCtrl, m_activeTab);

		m_eventHandler(WebDiffEvent::NewWindowRequested, sender, args);

		return S_OK;
	}

	HRESULT OnWindowCloseRequested(ICoreWebView2* sender, IUnknown* args)
	{
		int i = FindTabIndex(sender);
		if (i < 0)
			return S_OK;
		m_eventHandler(WebDiffEvent::WindowCloseRequested, sender, args);
		return CloseTab(i);
	}

	HRESULT OnNavigationStarting(ICoreWebView2* sender, ICoreWebView2NavigationStartingEventArgs* args)
	{
		UpdateToolbarControls();
		m_eventHandler(WebDiffEvent::NavigationStarting, sender, args);
		return S_OK;
	}

	HRESULT OnFrameNavigationStarting(ICoreWebView2Frame* sender, ICoreWebView2NavigationStartingEventArgs* args)
	{
		m_eventHandler(WebDiffEvent::FrameNavigationStarting, sender, args);
		return S_OK;
	}

	HRESULT OnHistoryChanged(ICoreWebView2* sender, IUnknown* args)
	{
		UpdateToolbarControls();
		m_eventHandler(WebDiffEvent::HistoryChanged, sender, args);
		return S_OK;
	}
	
	HRESULT OnSourceChanged(ICoreWebView2* sender, ICoreWebView2SourceChangedEventArgs* args)
	{
		UpdateToolbarControls();
		m_eventHandler(WebDiffEvent::SourceChanged, sender, args);
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
		m_eventHandler(WebDiffEvent::DocumentTitleChanged, sender, args);
		return S_OK;
	}

	HRESULT OnNavigationCompleted(ICoreWebView2* sender, ICoreWebView2NavigationCompletedEventArgs* args)
	{
		UpdateToolbarControls();
		m_eventHandler(WebDiffEvent::NavigationCompleted, sender, args);
		return S_OK;
	}

	HRESULT OnFrameNavigationCompleted(ICoreWebView2Frame* sender, ICoreWebView2NavigationCompletedEventArgs* args)
	{
		m_eventHandler(WebDiffEvent::FrameNavigationCompleted, sender, args);
		return S_OK;
	}

	HRESULT OnFrameWebMessageReceived(ICoreWebView2Frame* sender, ICoreWebView2WebMessageReceivedEventArgs* args)
	{
		wil::unique_cotaskmem_string messageRaw;
		args->TryGetWebMessageAsString(&messageRaw);
		m_webmessage = messageRaw.get();
		m_eventHandler(WebDiffEvent::FrameWebMessageReceived, sender, args);
		return S_OK;
	}

	HRESULT OnWebMessageReceived(ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args)
	{
		wil::unique_cotaskmem_string messageRaw;
		args->TryGetWebMessageAsString(&messageRaw);
		m_webmessage = messageRaw.get();
		m_eventHandler(WebDiffEvent::WebMessageReceived, sender, args);
		return S_OK;
	}

	HRESULT OnDevToolsProtocolEventReceived(ICoreWebView2* sender, ICoreWebView2DevToolsProtocolEventReceivedEventArgs* args, const std::wstring& event)
	{
		wil::unique_cotaskmem_string parameterObjectAsJson;
		args->get_ParameterObjectAsJson(&parameterObjectAsJson);
		return S_OK;
	}

	void OnHScroll(UINT nSBCode, UINT nPos)
	{
		SCROLLINFO si{ sizeof SCROLLINFO, SIF_POS | SIF_RANGE | SIF_PAGE | SIF_TRACKPOS };
		GetScrollInfo(m_hWebViewParent, SB_HORZ, &si);
		switch (nSBCode) {
		case SB_LINEUP:
			--m_nHScrollPos;
			break;
		case SB_LINEDOWN:
			++m_nHScrollPos;
			break;
		case SB_PAGEUP:
			m_nHScrollPos -= si.nPage;
			break;
		case SB_PAGEDOWN:
			m_nHScrollPos += si.nPage;
			break;
		case SB_THUMBTRACK:
			m_nHScrollPos = nPos;
			break;
		default: break;
		}
		CalcScrollBarRange(m_nHScrollPos, m_nVScrollPos);
		ScrollWindow(m_hWebViewParent, si.nPos - m_nHScrollPos, 0, NULL, NULL);
		m_eventHandler(WebDiffEvent::HSCROLL, nullptr, nullptr);
	}

	void OnVScroll(UINT nSBCode, UINT nPos)
	{
		SCROLLINFO si{ sizeof SCROLLINFO, SIF_POS | SIF_RANGE | SIF_PAGE | SIF_TRACKPOS };
		GetScrollInfo(m_hWebViewParent, SB_VERT, &si);
		switch (nSBCode) {
		case SB_LINEUP:
			--m_nVScrollPos;
			break;
		case SB_LINEDOWN:
			++m_nVScrollPos;
			break;
		case SB_PAGEUP:
			m_nVScrollPos -= si.nPage;
			break;
		case SB_PAGEDOWN:
			m_nVScrollPos += si.nPage;
			break;
		case SB_THUMBTRACK:
			m_nVScrollPos = nPos;
			break;
		default: break;
		}
		CalcScrollBarRange(m_nHScrollPos, m_nVScrollPos);
		ScrollWindow(m_hWebViewParent, 0, si.nPos - m_nVScrollPos, NULL, NULL);
		m_eventHandler(WebDiffEvent::VSCROLL, nullptr, nullptr);
	}

	LRESULT OnWndMsg(HWND hWnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
	{
		switch (iMsg)
		{
		case WM_COMMAND:
			switch (wParam)
			{
			case ID_GOBACK:
				if (GetActiveWebView())
				{
					GetActiveWebView()->GoBack();
					m_eventHandler(WebDiffEvent::GoBacked, nullptr, nullptr);
				}
				break;
			case ID_GOFORWARD:
				if (GetActiveWebView())
				{
					GetActiveWebView()->GoForward();
					m_eventHandler(WebDiffEvent::GoForwarded, nullptr, nullptr);
				}
				break;
			case ID_RELOAD:
				if (GetActiveWebView())
					GetActiveWebView()->Reload();
				break;
			case ID_STOP:
				if (GetActiveWebView())
					GetActiveWebView()->Stop();
				break;
			}
			break;
		case WM_NOTIFY:
		{
			NMHDR* pnmhdr = reinterpret_cast<NMHDR*>(lParam);
			if (pnmhdr->code == TCN_SELCHANGE)
				SetActiveTab(TabCtrl_GetCurSel(m_hTabCtrl));
			break;
		}
		case WM_SIZE:
			ResizeUIControls();
			if (m_activeTab != -1 && GetActiveWebViewController())
			{
				if (!m_fitToWindow)
					ScrollWindow(m_hWebViewParent, m_nHScrollPos, m_nVScrollPos, nullptr, nullptr);
				ResizeWebView();
				CalcScrollBarRange(m_nHScrollPos, m_nVScrollPos);
				if (!m_fitToWindow)
					ScrollWindow(m_hWebViewParent, -m_nHScrollPos, -m_nVScrollPos, nullptr, nullptr);
				if (m_showToolTip)
					ShowToolTip(TRUE);
			};
			break;
		case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hWnd, &ps);
			EndPaint(hWnd, &ps);
			return true;
		}
		case WM_TIMER:
		{
			if (wParam == ID_TOOLTIP_TIMER)
				ShowToolTip(false);
			break;
		}
		case WM_DESTROY:
			Destroy();
			break;
		default:
			return DefWindowProc(hWnd, iMsg, wParam, lParam);
		}
		return 0;
	}

	LRESULT OnWebViewParentWndMsg(HWND hWnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
	{
		switch (iMsg)
		{
		case WM_VSCROLL:
			OnVScroll((UINT)(LOWORD(wParam) & 0xff), (int)(unsigned short)HIWORD(wParam) | ((LOWORD(wParam) & 0xff00) << 8)); // See 'case WM_VSCROLL:' in CImgMergeWindow::ChildWndProc() 
			break;
		case WM_HSCROLL:
			OnHScroll((UINT)(LOWORD(wParam) & 0xff), (int)(unsigned short)HIWORD(wParam) | ((LOWORD(wParam) & 0xff00) << 8)); // See 'case WM_HSCROLL:' in CImgMergeWindow::ChildWndProc() 
			break;
		case WM_MOUSEWHEEL:
			break;
			/*
		case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hWnd, &ps);
			EndPaint(hWnd, &ps);
			return true;
		}*/
		default:
			return DefWindowProc(hWnd, iMsg, wParam, lParam);
		}
		return 0;
	}

	LRESULT OnTabCtrlWndMsg(HWND hWnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
	{
		if (iMsg == WM_MBUTTONDOWN)
		{
			TCHITTESTINFO ti{ { LOWORD(lParam), HIWORD(lParam) }, };
			int index = TabCtrl_HitTest(hWnd, &ti);
			if (index >= 0)
				CloseTab(index);
			return 0;
		}
		return CallWindowProc(m_oldTabCtrlWndProc, hWnd, iMsg, wParam, lParam);
	}

	LRESULT OnEditWndMsg(HWND hWnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
	{
		if (hWnd == m_hEdit)
		{
			if (iMsg == WM_CHAR && wParam == VK_RETURN)
			{
				int length = GetWindowTextLength(m_hEdit);
				std::wstring url(length, 0);
				GetWindowText(m_hEdit, const_cast<wchar_t*>(url.data()), length + 1);
				SetFocus();
				Navigate(url);
				return 0;
			}
			else if (iMsg == WM_KEYDOWN || iMsg == WM_SYSKEYDOWN)
			{
				short vkctrl = GetAsyncKeyState(VK_CONTROL);
				short vkmenu = GetAsyncKeyState(VK_MENU);
				if ((vkctrl && (wParam == 'E' || wParam == 'K')) || (vkmenu && wParam == 'D'))
				{
					SendMessage(m_hEdit, EM_SETSEL, 0, -1);
					return 0;
				}
			}
		}
		return CallWindowProc(m_oldEditWndProc, hWnd, iMsg, wParam, lParam);
	}

	static LRESULT CALLBACK WndProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
	{
		if (iMsg == WM_NCCREATE)
			SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<LPCREATESTRUCT>(lParam)->lpCreateParams));
		CWebWindow* pWebWnd = reinterpret_cast<CWebWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
		LRESULT lResult = pWebWnd->OnWndMsg(hwnd, iMsg, wParam, lParam);
		return lResult;
	}

	static LRESULT CALLBACK WebViewParentWndProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
	{
		if (iMsg == WM_NCCREATE)
			SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<LPCREATESTRUCT>(lParam)->lpCreateParams));
		CWebWindow* pWebWnd = reinterpret_cast<CWebWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
		LRESULT lResult = pWebWnd->OnWebViewParentWndMsg(hwnd, iMsg, wParam, lParam);
		return lResult;
	}

	static LRESULT CALLBACK TabCtrlWndProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
	{
		CWebWindow* pWebWnd = reinterpret_cast<CWebWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
		LRESULT lResult = pWebWnd->OnTabCtrlWndMsg(hwnd, iMsg, wParam, lParam);
		return lResult;
	}

	static LRESULT CALLBACK EditWndProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
	{
		CWebWindow* pWebWnd = reinterpret_cast<CWebWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
		LRESULT lResult = pWebWnd->OnEditWndMsg(hwnd, iMsg, wParam, lParam);
		return lResult;
	}

	static UINT MyGetDpiForWindow(HWND hWnd)
	{
		if (GetDpiForWindowFunc)
			return GetDpiForWindowFunc(hWnd);
		return GetDeviceCaps(GetDC(nullptr), LOGPIXELSX);
	}

	bool IsWin10OrGreater()
	{
		return GetDpiForWindowFunc != nullptr;
	}

	const int ID_TOOLTIP_TIMER = 1;
	const int TOOLTIP_TIMEOUT = 5000;
	HWND m_hWnd = nullptr;
	HWND m_hTabCtrl = nullptr;
	HWND m_hToolbar = nullptr;
	HWND m_hEdit = nullptr;
	HWND m_hToolTip = nullptr;
	HWND m_hWebViewParent = nullptr;
	HFONT m_hToolbarFont = nullptr;
	HFONT m_hEditFont = nullptr;
	WNDPROC m_oldTabCtrlWndProc = nullptr;
	WNDPROC m_oldEditWndProc = nullptr;
	TOOLINFO m_toolItem{};
	wil::com_ptr<ICoreWebView2Environment> m_webviewEnvironment;
	std::vector<std::unique_ptr<CWebTab>> m_tabs;
	int m_activeTab = -1;
	int m_nVScrollPos = 0;
	int m_nHScrollPos = 0;
	SIZE m_size{ 1024, 600 };
	bool m_fitToWindow = true;
	std::function<void(WebDiffEvent::EVENT_TYPE, IUnknown*, IUnknown*)> m_eventHandler;
	std::wstring m_currentUrl;
	std::wstring m_toolTipText = L"test";
	bool m_showToolTip = false;
	std::wstring m_webmessage;
	inline static const auto GetDpiForWindowFunc = []() {
		HMODULE hUser32 = GetModuleHandle(L"user32.dll");
		return reinterpret_cast<decltype(&::GetDpiForWindow)>(
			::GetProcAddress(hUser32, "GetDpiForWindow"));
	}();
};

