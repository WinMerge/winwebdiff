#pragma once

#include <Windows.h>
#include <CommCtrl.h>
#include <Shlwapi.h>
#include <wrl.h>
#include <wil/com.h>
#include <string>
#include <vector>
#include <memory>
#include <cassert>
#undef max
#undef min
#include <rapidjson/document.h>
#include "WebView2.h"
#include "resource.h"

#pragma comment(lib, "shlwapi.lib")

using namespace Microsoft::WRL;
using WDocument = rapidjson::GenericDocument < rapidjson::UTF16 < >>;

class CWebWindow
{
	enum { MARGIN = 16 };

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
			std::shared_ptr<std::wstring> url2(url ? new std::wstring(url) : nullptr);
			HRESULT hr = m_parent->m_webviewEnvironment->CreateCoreWebView2Controller(m_parent->m_hWebViewParent, Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
				[this, url2, args, deferral](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
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
						m_webview->Navigate(url2->c_str());
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

	bool Create(HINSTANCE hInstance, HWND hWndParent, const wchar_t* url)
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
		m_hWebViewParent = CreateWindowExW(0, L"WebViewParentClass", nullptr,
			WS_CHILD | WS_VISIBLE,
			0, 0, 0, 0, m_hWnd, nullptr, hInstance, this);
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
		InitializeWebView(url);
		return true;
	}

	bool Destroy()
	{
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

	bool GetFitToWindow() const
	{
		return m_fitToWindow;
	}

	void SetFitToWindow(bool fitToWindow)
	{
		m_fitToWindow = fitToWindow;
		ShowScrollBar(m_hWebViewParent, SB_BOTH, !m_fitToWindow);
		ResizeWebView();
		CalcScrollBarRange();
	}

	SIZE GetSize() const
	{
		RECT rc{};
		GetActiveWebViewController()->get_Bounds(&rc);
		return { rc.right - rc.left, rc.bottom - rc.top };
	}

	void SetSize(const SIZE size)
	{
		m_size = size;
		ResizeWebView();
		CalcScrollBarRange();
	}

	double GetZoom() const
	{
		return m_zoom;
	}

	void SetZoom(double zoom)
	{
		double oldZoom = m_zoom;
		m_zoom = zoom;
		if (m_zoom < 0.1)
			m_zoom = 0.1;
		m_nVScrollPos = static_cast<int>(m_nVScrollPos / oldZoom * m_zoom);
		m_nHScrollPos = static_cast<int>(m_nHScrollPos / oldZoom * m_zoom);

		if (m_activeTab >= 0)
		{
			RECT rc;
			GetClientRect(m_hWebViewParent, &rc);
			SIZE size = GetSize();
			if (m_nHScrollPos > static_cast<int>(size.cx - rc.right))
				m_nHScrollPos = size.cx - rc.right;
			if (m_nHScrollPos < 0)
				m_nHScrollPos = 0;
			if (m_nVScrollPos > static_cast<int>(size.cy - rc.bottom))
				m_nVScrollPos = size.cy - rc.bottom;
			if (m_nVScrollPos < 0)
				m_nVScrollPos = 0;
			CalcScrollBarRange();
			InvalidateRect(m_hWebViewParent, NULL, TRUE);
		}
	}

	void SetFocus()
	{
		::SetFocus(m_hWebViewParent);
	}

	bool SaveScreenshot(const wchar_t* filename)
	{
		std::wstring filepath = filename;
		HRESULT hr = GetActiveWebView()->CallDevToolsProtocolMethod(L"Page.getLayoutMetrics", L"{}",
			Callback<ICoreWebView2CallDevToolsProtocolMethodCompletedHandler>(
				[this, filepath](HRESULT errorCode, LPCWSTR returnObjectAsJson) -> HRESULT {

					wil::com_ptr<IStream> stream;
					HRESULT hr = SHCreateStreamOnFileEx(filepath.c_str(), STGM_READWRITE | STGM_CREATE, FILE_ATTRIBUTE_NORMAL, TRUE, nullptr, &stream);
					if (FAILED(hr))
						return hr;

					RECT rcOrg;
					GetActiveWebViewController()->get_Bounds(&rcOrg);
 
					WDocument document;
					document.Parse(returnObjectAsJson);
					int width = document[L"cssContentSize"][L"width"].GetInt();
					int height = document[L"cssContentSize"][L"height"].GetInt();

					RECT rcNew{ 0, 0, width, height };
					GetActiveWebViewController()->put_Bounds(rcNew);

					hr = GetActiveWebView()->CapturePreview(
						COREWEBVIEW2_CAPTURE_PREVIEW_IMAGE_FORMAT_PNG, stream.get(),
						Callback<ICoreWebView2CapturePreviewCompletedHandler>(
							[this, rcOrg](HRESULT errorCode) -> HRESULT {
								GetActiveWebViewController()->put_Bounds(rcOrg);
								return S_OK;
							})
						.Get());
					return S_OK;
				})
			.Get());

		if (FAILED(hr))
			return false;
		return true;
	}

	bool SaveHTML(const wchar_t* filename)
	{
		/*
		HRESULT hr2 = GetActiveWebView()->CallDevToolsProtocolMethod(L"Page.captureSnapshot", L"{\"format\": \"mhtml\"}",
			Callback<ICoreWebView2CallDevToolsProtocolMethodCompletedHandler>(
				[this, filename](HRESULT errorCode, LPCWSTR returnObjectAsJson) -> HRESULT {
					WDocument document;
					document.Parse(returnObjectAsJson);

					wil::unique_file fp;
					_wfopen_s(&fp, (filename + std::wstring(L".mhtml")).c_str(), L"wt,ccs=UTF-8");
					fwprintf(fp.get(), L"%s", document[L"data"].GetString());
					return S_OK;
				})
			.Get());
			*/

		GetActiveWebView()->ExecuteScript(L"document.children[0].innerHTML",
			Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
				[filename](HRESULT errorCode, LPCWSTR resultObjectAsJson) -> HRESULT {
					wil::unique_file fp;
					_wfopen_s(&fp, filename, L"wt,ccs=UTF-8");
					fwprintf(fp.get(), L"%s", resultObjectAsJson);
					return S_OK;
				})
			.Get());
		return true;
	}

private:

	bool InitializeWebView(const wchar_t *url)
	{
		std::shared_ptr<std::wstring> url2(new std::wstring(url));
		HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(nullptr, nullptr, nullptr,
			Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
				[this, url2](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
					m_webviewEnvironment = env;
					m_tabs.emplace_back(new CWebTab(this, url2->c_str()));

					TCITEM tcItem{};
					tcItem.mask = TCIF_TEXT;
					tcItem.pszText = (PWSTR)L"";
					TabCtrl_InsertItem(m_hTabCtrl, 0, &tcItem);
					m_activeTab = 0;
					return S_OK;
				}).Get());
		return SUCCEEDED(hr);
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

	CWebTab* GetActiveTab() const
	{
		assert(m_activeTab >= 0 && m_activeTab < m_tabs.size());
		return m_tabs[m_activeTab].get();
	}

	ICoreWebView2* GetActiveWebView() const
	{
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
		MoveWindow(m_hWebViewParent, 0, rcClient.top + rcToolbar.bottom, bounds.right - bounds.left, bounds.bottom - (rcClient.bottom + rcToolbar.bottom), TRUE);
	}

	void ResizeWebView()
	{
		RECT bounds;
		GetClientRect(m_hWebViewParent, &bounds);
		if (m_fitToWindow)
		{
			GetActiveWebViewController()->put_Bounds(bounds);
		}
		else
		{
			bounds.right = bounds.left + m_size.cx;
			bounds.bottom = bounds.top + m_size.cy;
			GetActiveWebViewController()->put_Bounds(bounds);
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

	void CalcScrollBarRange()
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
			si.nPos = m_nVScrollPos;
			SetScrollInfo(m_hWebViewParent, SB_VERT, &si, TRUE);
			m_nVScrollPos = GetScrollPos(m_hWebViewParent, SB_VERT);

			si.nMin = 0;
			si.nMax = size.cx;
			si.nPage = rc.right;
			si.nPos = m_nHScrollPos;
			SetScrollInfo(m_hWebViewParent, SB_HORZ, &si, TRUE);
			m_nHScrollPos = GetScrollPos(m_hWebViewParent, SB_HORZ);
		}
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
			wcex.hbrBackground = (HBRUSH)(COLOR_3DFACE + 1);
			wcex.lpszClassName = L"WebViewParentClass";
		}
		return RegisterClassExW(&wcex) != 0;
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
		CalcScrollBarRange();
		ScrollWindow(m_hWebViewParent, si.nPos - m_nHScrollPos, 0, NULL, NULL);
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
		CalcScrollBarRange();
		ScrollWindow(m_hWebViewParent, 0, si.nPos - m_nVScrollPos, NULL, NULL);
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
				CalcScrollBarRange();
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

	static LRESULT CALLBACK WebViewParentWndProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
	{
		if (iMsg == WM_NCCREATE)
			SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<LPCREATESTRUCT>(lParam)->lpCreateParams));
		CWebWindow* pWebWnd = reinterpret_cast<CWebWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
		LRESULT lResult = pWebWnd->OnWebViewParentWndMsg(hwnd, iMsg, wParam, lParam);
		return lResult;
	}

	HWND m_hWnd = nullptr;
	HWND m_hTabCtrl = nullptr;
	HWND m_hToolbar = nullptr;
	HWND m_hEdit = nullptr;
	HWND m_hWebViewParent = nullptr;
	HFONT m_hToolbarFont = nullptr;
	HFONT m_hEditFont = nullptr;
	WNDPROC m_oldEditWndProc = nullptr;
	wil::com_ptr<ICoreWebView2Environment> m_webviewEnvironment;
	std::vector<std::unique_ptr<CWebTab>> m_tabs;
	int m_activeTab = -1;
	int m_nVScrollPos = 0;
	int m_nHScrollPos = 0;
	double m_zoom = 1.0;
	SIZE m_size{ 1024, 600 };
	bool m_fitToWindow = true;
};

