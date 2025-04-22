#pragma once

#include "WinWebDiffLib.h"
#include "WebWindow.hpp"
#include "DiffHighlighter.hpp"
#include "DiffLocation.hpp"
#include <shellapi.h>
#include <wil/win32_helpers.h>

class CWebDiffWindow : public IWebDiffWindow
{
public:
	CWebDiffWindow()
	{
	}

	bool Create(HINSTANCE hInstance, HWND hWndParent, int nID, const RECT& rc)
	{
		m_hInstance = hInstance;
		MyRegisterClass(hInstance);
		m_hWnd = CreateWindowExW(0, L"WinWebDiffWindowClass", nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
			rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, hWndParent, reinterpret_cast<HMENU>((intptr_t)nID), hInstance, this);
		return m_hWnd ? true : false;
	}

	bool Destroy()
	{
		BOOL bSucceeded = true;
		if (m_hWnd)
			bSucceeded = DestroyWindow(m_hWnd);
		m_hWnd = nullptr;
		return !!bSucceeded;
	}

	bool IsWebView2Installed() const override
	{
		wil::unique_cotaskmem_string version_info;
		HRESULT hr = GetAvailableCoreWebView2BrowserVersionString(nullptr, &version_info);
		return (hr == S_OK && version_info != nullptr);
	}

	bool DownloadWebView2() const override
	{
		return ShellExecute(0, 0, L"https://go.microsoft.com/fwlink/p/?LinkId=2124703", 0, 0, SW_SHOW);
	}

	void AddEventListener(IWebDiffEventHandler* handler) override
	{
		m_listeners.push_back(handler);
	}

	void SetUserDataFolderType(UserdataFolderType userDataFolderType, bool perPane) override
	{
		m_userDataFolderType = userDataFolderType;
		m_bUserDataFolderPerPane = perPane;
	}

	HRESULT New(int nUrls, IWebDiffCallback* callback) override
	{
		const wchar_t* urls[3] = { L"about:blank", L"about:blank", L"about:blank" };
		return Open(nUrls, urls, callback);
	}

	HRESULT Open(const wchar_t* url1, const wchar_t* url2, IWebDiffCallback* callback) override
	{
		const wchar_t* urls[3] = { url1, url2, nullptr };
		return Open(2, urls, callback);
	}

	HRESULT Open(const wchar_t* url1, const wchar_t* url2, const wchar_t* url3, IWebDiffCallback* callback) override
	{
		const wchar_t* urls[3] = { url1, url2, url3 };
		return Open(3, urls, callback);
	}

	HRESULT Open(int nPanes, const wchar_t* urls[3], IWebDiffCallback* callback)
	{
		HRESULT hr = S_OK;
		m_nPanes = nPanes;
		if (m_hWnd)
		{
			Close();
			for (int i = 0; i < nPanes; ++i)
			{
				std::wstring userDataFolder = GetUserDataFolderPath(i);
				ComPtr<IWebDiffCallback> callback2(callback);
				hr = m_webWindow[i].Create(this, m_hInstance, m_hWnd, urls[i], userDataFolder.c_str(),
						m_size, m_fitToWindow, m_zoom, m_userAgent, nullptr,
						[this, i, callback2](WebDiffEvent::EVENT_TYPE event, IUnknown* sender, IUnknown* args)
							{
								WebDiffEvent ev{};
								ev.type = event;
								ev.pane = i;
								if (event == WebDiffEvent::SourceChanged)
								{
									m_webWindow[ev.pane].SetZoom(m_zoom);
								}
								else if (event == WebDiffEvent::ZoomFactorChanged)
								{
									m_zoom = m_webWindow[ev.pane].GetZoom();
									for (int pane = 0; pane < m_nPanes; ++pane)
									{
										if (pane != ev.pane)
											m_webWindow[pane].SetZoom(m_zoom);
									}
								}
								else if (event == WebDiffEvent::HSCROLL)
								{
									for (int pane = 0; pane < m_nPanes; ++pane)
									{
										if (pane != ev.pane)
											m_webWindow[pane].SetHScrollPos(m_webWindow[ev.pane].GetHScrollPos());
									}
								}
								else if (event == WebDiffEvent::VSCROLL)
								{
									for (int pane = 0; pane < m_nPanes; ++pane)
									{
										if (pane != ev.pane)
											m_webWindow[pane].SetVScrollPos(m_webWindow[ev.pane].GetVScrollPos());
									}
								}
								else if (event == WebDiffEvent::NavigationStarting)
								{
									m_documentLoaded[ev.pane] = false;
									m_urlChanged[ev.pane] = true;
									SetCompareState(NOT_COMPARED);
								}
								else if (event == WebDiffEvent::FrameNavigationStarting)
								{
								}
								else if (event == WebDiffEvent::NavigationCompleted)
								{
									addEventListener(sender, ev.pane, nullptr);
									m_documentLoaded[ev.pane] = true;
									if ((std::count(m_documentLoaded, m_documentLoaded + m_nPanes, true) == m_nPanes) &&
									    (std::count(m_urlChanged, m_urlChanged + m_nPanes, true) == m_nPanes))
									{
										std::fill_n(m_urlChanged, m_nPanes, false);
										Recompare(callback2.Get());
									}
								}
								else if (event == WebDiffEvent::FrameNavigationCompleted)
								{
									addEventListener(sender,ev.pane, nullptr);
								}
								else if (event == WebDiffEvent::GoBacked)
								{
									if (m_bSynchronizeEvents && GetSyncEventFlag(EVENT_GOBACKFORWARD))
									{
										for (int pane = 0; pane < m_nPanes; ++pane)
										{
											if (ev.pane == pane)
												continue;
											m_webWindow[pane].GoBack();
										}
									}
								}
								else if (event == WebDiffEvent::GoForwarded)
								{
									if (m_bSynchronizeEvents && GetSyncEventFlag(EVENT_GOBACKFORWARD))
									{
										for (int pane = 0; pane < m_nPanes; ++pane)
										{
											if (ev.pane == pane)
												continue;
											m_webWindow[pane].GoForward();
										}
									}
								}
								else if (event == WebDiffEvent::WebMessageReceived || event == WebDiffEvent::FrameWebMessageReceived)
								{
									std::wstring msg = m_webWindow[i].GetWebMessage();
#ifdef _DEBUG
									wchar_t buf[4096];
									wsprintfW(buf, L"WebMessageReceived(pane:%d): %s\n", ev.pane, msg.c_str());
									OutputDebugString(buf);
#endif
									WDocument doc;
									doc.Parse(msg.c_str());
									std::wstring event = doc.HasMember(L"event") ? doc[L"event"].GetString() : L"";
									std::wstring window = doc.HasMember(L"window") ? doc[L"window"].GetString() : L"";
									if (event == L"dblclick")
									{
										const int diffIndex = doc[L"wwdid"].GetInt();
										if (diffIndex > -1)
											SelectDiff(diffIndex);
									}
									else if (event == L"scroll")
									{
										if (m_bSynchronizeEvents && GetSyncEventFlag(EVENT_SCROLL))
											syncEvent(ev.pane, msg);
										for (int pane = 0; pane < m_nPanes; ++pane)
											m_webWindow[pane].PostWebMessageAsJsonInAllFrames(L"{\"event\": \"diffRects\"}");
									}
									else if (event == L"click")
									{
										if (m_bSynchronizeEvents && GetSyncEventFlag(EVENT_CLICK))
											syncEvent(ev.pane, msg);
									}
									else if (event == L"input")
									{
										if (m_bSynchronizeEvents && GetSyncEventFlag(EVENT_INPUT))
											syncEvent(ev.pane, msg);
									}
									else if (event == L"diffRects")
									{
										m_diffLocation[ev.pane].read(doc);
									}
								}
								RaiseEvent(ev);
							});
			}
			std::vector<RECT> rects = CalcChildWebWindowRect(m_hWnd, m_nPanes, m_bHorizontalSplit);
			for (int i = 0; i < m_nPanes; ++i)
				m_webWindow[i].SetWindowRect(rects[i]);
		}
		return hr;
	}

	void Close() override
	{
		for (int i = 0; i < m_nPanes; ++i)
			m_webWindow[i].Destroy();
	}

	void NewTab(int pane, const wchar_t *url, IWebDiffCallback* callback) override
	{
		if (pane < 0 || pane >= m_nPanes || !m_hWnd)
			return;
		m_webWindow[pane].NewTab(url, m_zoom, m_userAgent, callback);
	}

	void CloseActiveTab(int pane) override
	{
		if (pane < 0 || pane >= m_nPanes || !m_hWnd)
			return;
		m_webWindow[pane].CloseActiveTab();
	}

	HRESULT Reload(int pane) override
	{
		if (pane < 0 || pane >= m_nPanes || !m_hWnd)
			return E_INVALIDARG;
		return m_webWindow[pane].Reload();
	}

	HRESULT ReloadAll() override
	{
		HRESULT hr = S_OK;
		for (int pane = 0; pane < m_nPanes; ++pane)
			hr = m_webWindow[pane].Reload();
		return hr;
	}

	HRESULT Recompare(IWebDiffCallback* callback) override
	{
		return compare(callback);
	}

	HRESULT SaveFile(int pane, FormatType kind, const wchar_t* filename, IWebDiffCallback* callback) override
	{
		if (pane < 0 || pane >= m_nPanes)
			return false;
		ComPtr<IWebDiffCallback> callback2(callback);
		std::wstring sfilename(filename);
		return unhighlightDifferencesLoop(
			Callback<IWebDiffCallback>([this, kind, pane, sfilename, callback2](const WebDiffCallbackResult& result) -> HRESULT
				{
					HRESULT hr = result.errorCode;
					if (SUCCEEDED(hr))
					{
						hr = m_webWindow[pane].SaveFile(sfilename, kind,
							Callback<IWebDiffCallback>([this, callback2](const WebDiffCallbackResult& result) -> HRESULT
								{
									HRESULT hr = result.errorCode;
									if (SUCCEEDED(hr))
										hr = compare(callback2.Get());
									if (FAILED(hr) && callback2)
										return callback2->Invoke({ hr, nullptr });
									return S_OK;
								}).Get());
					}
					if (FAILED(hr) && callback2)
						return callback2->Invoke({ hr, nullptr });
					return S_OK;
				}).Get());
	}

	HRESULT SaveFiles(FormatType kind, const wchar_t* filenames[], IWebDiffCallback* callback) override
	{
		auto sfilenames = std::make_shared<std::vector<std::wstring>>();
		for (int pane = 0; pane < m_nPanes; ++pane)
			sfilenames->push_back(filenames[pane]);
		ComPtr<IWebDiffCallback> callback2(callback);
		return unhighlightDifferencesLoop(
			Callback<IWebDiffCallback>([this, kind, sfilenames, callback2](const WebDiffCallbackResult& result) -> HRESULT
				{
					HRESULT hr = result.errorCode;
					if (SUCCEEDED(hr))
					{
						hr = saveFilesLoop(kind, sfilenames,
							Callback<IWebDiffCallback>([this, sfilenames, callback2](const WebDiffCallbackResult& result) -> HRESULT
								{
									HRESULT hr = result.errorCode;
									if (SUCCEEDED(hr))
										hr = compare(callback2.Get());
									if (FAILED(hr) && callback2)
										return callback2->Invoke({ hr, nullptr });
									return S_OK;
								}).Get());
					}
					if (FAILED(hr) && callback2)
						return callback2->Invoke({ hr, nullptr });
					return S_OK;
				}).Get());
	}

	HRESULT SaveDiffFiles(FormatType kind, const wchar_t* filenames[], IWebDiffCallback* callback) override
	{
		auto sfilenames = std::make_shared<std::vector<std::wstring>>();
		for (int pane = 0; pane < m_nPanes; ++pane)
			sfilenames->push_back(filenames[pane]);
		ComPtr<IWebDiffCallback> callback2(callback);
		HRESULT hr = saveFilesLoop(kind, sfilenames,
			Callback<IWebDiffCallback>([this, sfilenames, callback2](const WebDiffCallbackResult& result) -> HRESULT
				{
					if (callback2)
						return callback2->Invoke({ result.errorCode, nullptr });
					return S_OK;
				}).Get());
		return hr;
	}

	HRESULT ClearBrowsingData(int pane, BrowsingDataType datakinds) override
	{
		int spane = pane, epane = pane;
		if (pane < 0 || pane >= m_nPanes)
		{
			spane = 0;
			epane = m_nPanes - 1;
		}
		HRESULT hr = S_OK;
		for (int i = spane; i <= epane; ++i)
		{
			if (FAILED(hr = m_webWindow[i].ClearBrowsingData(datakinds)))
				return hr;
		}
		return hr;
	}

	const wchar_t* GetCurrentUrl(int pane) override
	{
		if (pane < 0 || pane >= m_nPanes)
			return L"";
		return m_webWindow[pane].GetCurrentUrl();
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

	int  GetActivePane() const override
	{
		if (!m_hWnd)
			return -1;
		for (int i = 0; i < m_nPanes; ++i)
			if (m_webWindow[i].IsFocused())
				return i;
		return -1;
	}

	void SetActivePane(int pane) override
	{
		m_webWindow[pane].SetFocus();
	}

	bool GetHorizontalSplit() const override
	{
		return m_bHorizontalSplit;
	}

	void SetHorizontalSplit(bool horizontalSplit) override
	{
		if (!m_hWnd)
			return;
		m_bHorizontalSplit = horizontalSplit;
		std::vector<RECT> rects = CalcChildWebWindowRect(m_hWnd, m_nPanes, m_bHorizontalSplit);
		for (int i = 0; i < m_nPanes; ++i)
			m_webWindow[i].SetWindowRect(rects[i]);
	}

	void GetDiffColorSettings(IWebDiffWindow::ColorSettings& settings) const override
	{
		settings = m_colorSettings;
	}

	void SetDiffColorSettings(const IWebDiffWindow::ColorSettings& settings) override
	{
		m_colorSettings = settings;
	}

	double GetZoom() const override
	{
		return m_zoom;
	}

	void SetZoom(double zoom) override
	{
		if (zoom == m_zoom)
			return;
		m_zoom = std::clamp(zoom, 0.25, 5.0);
		for (int pane = 0; pane < m_nPanes; ++pane)
			m_webWindow[pane].SetZoom(m_zoom);
	}

	const wchar_t *GetUserAgent() const override
	{
		if (m_nPanes == 0)
			return L"";
		return m_userAgent.c_str();
	}

	void SetUserAgent(const wchar_t* userAgent) override
	{
		m_userAgent = userAgent;
		for (int pane = 0; pane < m_nPanes; ++pane)
			m_webWindow[pane].SetUserAgent(m_userAgent);
	}

	bool GetFitToWindow() const override
	{
		return m_fitToWindow;
	}

	void SetFitToWindow(bool fitToWindow) override
	{
		m_fitToWindow = fitToWindow;
		for (int pane = 0; pane < m_nPanes; ++pane)
			m_webWindow[pane].SetFitToWindow(fitToWindow);
	}

	SIZE GetSize() const override
	{
		return m_size;
	}

	void SetSize(const SIZE size) override
	{
		m_size = size;
		for (int pane = 0; pane < m_nPanes; ++pane)
			m_webWindow[pane].SetSize(size);
	}

	bool GetShowDifferences() const override
	{
		return m_bShowDifferences;
	}

	void SetShowDifferences(bool visible) override
	{
		if (visible == m_bShowDifferences)
			return;
		m_bShowDifferences = visible;
		Recompare(nullptr);
	}

	bool GetShowWordDifferences() const override
	{
		return m_bShowWordDifferences;
	}

	void SetShowWordDifferences(bool visible) override
	{
		if (visible == m_bShowWordDifferences)
			return;
		m_bShowWordDifferences = visible;
		Recompare(nullptr);
	}

	const DiffOptions& GetDiffOptions() const override
	{
		return m_diffOptions;
	}

	void SetDiffOptions(const DiffOptions& diffOptions) override
	{
		m_diffOptions = diffOptions;
	}

	bool GetSyncEvents() const
	{
		return m_bSynchronizeEvents;
	}

	void SetSyncEvents(bool syncEvents)
	{
		m_bSynchronizeEvents = syncEvents;
	}

	unsigned GetSyncEventFlags() const
	{
		return m_eventSyncFlags;
	}

	void SetSyncEventFlags(unsigned flags)
	{
		m_eventSyncFlags = flags;
	}

	bool GetSyncEventFlag(EventType event) const
	{
		return (m_eventSyncFlags & event) != 0;
	}

	void SetSyncEventFlag(EventType event, bool flag)
	{
		if (flag)
			m_eventSyncFlags = m_eventSyncFlags | static_cast<unsigned>(event);
		else
			m_eventSyncFlags = m_eventSyncFlags & ~static_cast<unsigned>(event);
	}

	CompareState GetCompareState() const
	{
		return m_compareState;
	}

	void SetCompareState(CompareState compareState)
	{
		CompareState oldCompareState = m_compareState;
		m_compareState = compareState;
		if (m_compareState != oldCompareState)
		{
			WebDiffEvent ev{ WebDiffEvent::CompareStateChanged, -1 };
			RaiseEvent(ev);
			if (compareState == CompareState::COMPARED || compareState == CompareState::NOT_COMPARED)
			{
				if (ev.pane >= 0)
					m_diffLocation[ev.pane].clear();
			}
			if (compareState == CompareState::COMPARED)
			{
				for (int pane = 0; pane < m_nPanes; ++pane)
					m_webWindow[pane].PostWebMessageAsJsonInAllFrames(L"{\"event\": \"diffRects\"}");
			}
		}
	}

	LogCallback GetLogCallback() const
	{
		return m_logCallback;
	}

	void SetLogCallback(LogCallback logCallback)
	{
		m_logCallback = logCallback;
	}

	int  GetDiffCount() const override
	{
		return static_cast<int>(m_diffInfos.size());
	}

	int  GetConflictCount() const override
	{
		int conflictCount = 0;
		for (size_t i = 0; i < m_diffInfos.size(); ++i)
			if (m_diffInfos[i].op == OP_DIFF)
				++conflictCount;
		return conflictCount;
	}

	int  GetCurrentDiffIndex() const override
	{
		return m_currentDiffIndex;
	}

	bool FirstDiff() override
	{
		int oldDiffIndex = m_currentDiffIndex;
		if (m_diffInfos.size() == 0)
			m_currentDiffIndex = -1;
		else
			m_currentDiffIndex = 0;
		if (oldDiffIndex == m_currentDiffIndex)
			return false;
		return SUCCEEDED(selectDiff(m_currentDiffIndex, nullptr));
	}

	bool LastDiff() override
	{
		int oldDiffIndex = m_currentDiffIndex;
		m_currentDiffIndex = static_cast<int>(m_diffInfos.size()) - 1;
		if (oldDiffIndex == m_currentDiffIndex)
			return false;
		return SUCCEEDED(selectDiff(m_currentDiffIndex, nullptr));
	}

	bool NextDiff() override
	{
		int oldDiffIndex = m_currentDiffIndex;
		++m_currentDiffIndex;
		if (static_cast<size_t>(m_currentDiffIndex) >= m_diffInfos.size())
			m_currentDiffIndex = static_cast<int>(m_diffInfos.size()) - 1;
		if (oldDiffIndex == m_currentDiffIndex)
			return false;
		return SUCCEEDED(selectDiff(m_currentDiffIndex, nullptr));
	}
	
	bool PrevDiff() override
	{
		int oldDiffIndex = m_currentDiffIndex;
		if (m_diffInfos.size() == 0)
			m_currentDiffIndex = -1;
		else
		{
			--m_currentDiffIndex;
			if (m_currentDiffIndex < 0)
				m_currentDiffIndex = 0;
		}
		if (oldDiffIndex == m_currentDiffIndex)
			return false;
		return SUCCEEDED(selectDiff(m_currentDiffIndex, nullptr));
	}

	bool FirstConflict() override
	{
		int oldDiffIndex = m_currentDiffIndex;
		for (size_t i = 0; i < m_diffInfos.size(); ++i)
			if (m_diffInfos[i].op == OP_DIFF)
				m_currentDiffIndex = static_cast<int>(i);
		if (oldDiffIndex == m_currentDiffIndex)
			return false;
		return SUCCEEDED(selectDiff(m_currentDiffIndex, nullptr));
	}

	bool LastConflict() override
	{
		int oldDiffIndex = m_currentDiffIndex;
		for (int i = static_cast<int>(m_diffInfos.size() - 1); i >= 0; --i)
		{
			if (m_diffInfos[i].op == OP_DIFF)
			{
				m_currentDiffIndex = i;
				break;
			}
		}
		if (oldDiffIndex == m_currentDiffIndex)
			return false;
		return SUCCEEDED(selectDiff(m_currentDiffIndex, nullptr));
	}

	bool NextConflict() override
	{
		int oldDiffIndex = m_currentDiffIndex;
		for (size_t i = m_currentDiffIndex + 1; i < m_diffInfos.size(); ++i)
		{
			if (m_diffInfos[i].op == OP_DIFF)
			{
				m_currentDiffIndex = static_cast<int>(i);
				break;
			}
		}
		if (oldDiffIndex == m_currentDiffIndex)
			return false;
		return SUCCEEDED(selectDiff(m_currentDiffIndex, nullptr));
	}

	bool PrevConflict()  override
	{
		int oldDiffIndex = m_currentDiffIndex;
		for (int i = m_currentDiffIndex - 1; i >= 0; --i)
		{
			if (m_diffInfos[i].op == OP_DIFF)
			{
				m_currentDiffIndex = i;
				break;
			}
		}
		if (oldDiffIndex == m_currentDiffIndex)
			return false;
		return SUCCEEDED(selectDiff(m_currentDiffIndex, nullptr));
	}

	bool SelectDiff(int diffIndex) override
	{
		if (diffIndex < 0 || static_cast<size_t>(diffIndex) >= m_diffInfos.size())
			return false;
		m_currentDiffIndex = diffIndex;
		return SUCCEEDED(selectDiff(diffIndex, nullptr));
	}

	int  GetNextDiffIndex() const override
	{
		if (m_diffInfos.size() == 0 || m_currentDiffIndex >= static_cast<int>(m_diffInfos.size() - 1))
			return -1;
		return m_currentDiffIndex + 1;
	}

	int  GetPrevDiffIndex() const override
	{
		if (m_diffInfos.size() == 0 || m_currentDiffIndex <= 0)
			return -1;
		return m_currentDiffIndex - 1;
	}

	int  GetNextConflictIndex() const override
	{
		for (size_t i = m_currentDiffIndex + 1; i < m_diffInfos.size(); ++i)
			if (m_diffInfos[i].op == OP_DIFF)
				return static_cast<int>(i);
		return -1;
	}

	int  GetPrevConflictIndex() const override
	{
		for (int i = static_cast<int>(m_currentDiffIndex - 1); i >= 0; --i)
			if (m_diffInfos[i].op == OP_DIFF)
				return i;
		return -1;
	}

	HWND GetHWND() const override
	{
		return m_hWnd;
	}

	HWND GetPaneHWND(int pane) const override
	{
		if (pane < 0 || pane >= m_nPanes)
			return nullptr;
		return m_webWindow[pane].GetHWND();
	}

	bool Copy() override
	{
		return execCommand(L"copy");
	}

	bool Cut() override
	{
		return execCommand(L"cut");
	}

	bool Delete() override
	{
		return execCommand(L"delete");
	}

	bool Paste() override
	{
		return execCommand(L"paste");
	}

	bool SelectAll() override
	{
		return execCommand(L"selectall");
	}

	bool Undo() override
	{
		return execCommand(L"undo");
	}

	bool Redo() override
	{
		return execCommand(L"redo");
	}

	bool CanUndo() override
	{
		return true;
	}

	bool CanRedo() override
	{
		return true;
	}

	void RaiseEvent(const WebDiffEvent& e) override
	{
		for (const auto& listener : m_listeners)
			listener->Invoke(e);
	}

	std::vector<DiffLocation::DiffRect> GetDiffRectArray(int pane)
	{
		return m_diffLocation[pane].getDiffRectArray();
	}

	std::vector<DiffLocation::ContainerRect> GetContainerRectArray(int pane)
	{
		return m_diffLocation[pane].getContainerRectArray();
	}

	DiffLocation::Rect GetVisibleAreaRect(int pane)
	{
		return m_diffLocation[pane].getVisibleAreaRect();
	}

	void ScrollTo(int pane, float scrollX, float scrollY)
	{
		std::wstring json = L"{\"event\": \"scrollTo\", \"window\": \"\", \"scrollX\": " + std::to_wstring(scrollX) + L", \"scrollY\": " + std::to_wstring(scrollY) + L"}";
		m_webWindow[pane].PostWebMessageAsJsonInAllFrames(json.c_str());
	}

private:

	HRESULT getDocumentsLoop(std::shared_ptr<std::vector<std::wstring>> jsons, IWebDiffCallback* callback, int pane = 0)
	{
		static const wchar_t* method = L"DOM.getDocument";
		static const wchar_t* params = L"{ \"depth\": -1, \"pierce\": true }";
		ComPtr<IWebDiffCallback> callback2(callback);
		HRESULT hr = m_webWindow[pane].CallDevToolsProtocolMethod(method, params,
			Callback<IWebDiffCallback>([this, pane, jsons, callback2](const WebDiffCallbackResult& result) -> HRESULT
				{
					HRESULT hr = result.errorCode;
					if (SUCCEEDED(hr))
					{
						jsons->push_back(result.returnObjectAsJson);
						if (pane + 1 < m_nPanes)
							hr = getDocumentsLoop(jsons, callback2.Get(), pane + 1);
						else if (callback2)
							return callback2->Invoke({ hr, nullptr });
					}
					if (FAILED(hr) && callback2)
						return callback2->Invoke({ hr, nullptr });
					return S_OK;
				}).Get());
		return hr;
	}

	HRESULT addEventListener(IUnknown* sender, int pane, IWebDiffCallback* callback)
	{
		return m_webWindow[pane].ExecuteScript(sender, GetScriptOnLoad(), callback);
	}

	HRESULT syncEvent(int srcPane, const std::wstring& json)
	{
		for (int pane = 0; pane < m_nPanes; ++pane)
		{
			if (pane == srcPane)
				continue;
			m_webWindow[pane].PostWebMessageAsJsonInAllFrames(json.c_str());
		}
		return S_OK;
	}

	HRESULT compare(IWebDiffCallback* callback)
	{
		SetCompareState(COMPARING);
		ComPtr<IWebDiffCallback> callback2(callback);
		std::shared_ptr<std::vector<std::wstring>> jsons(new std::vector<std::wstring>());
		HRESULT hr = getDocumentsLoop(jsons,
			Callback<IWebDiffCallback>([this, jsons, callback2](const WebDiffCallbackResult& result) -> HRESULT
				{
					HRESULT hr = result.errorCode;
					if (SUCCEEDED(hr))
					{
						std::vector<TextSegments> textSegments(m_nPanes);
						std::shared_ptr<std::vector<WDocument>> documents(new std::vector<WDocument>(m_nPanes));
						for (int pane = 0; pane < m_nPanes; ++pane)
						{
							(*documents)[pane].Parse((*jsons)[pane].c_str());
#ifdef _DEBUG
							WStringBuffer buffer;
							WPrettyWriter writer(buffer);
							(*documents)[pane].Accept(writer);
							WriteToTextFile((L"c:\\tmp\\file" + std::to_wstring(pane) + L".json"),
								buffer.GetString());
#endif
							Highlighter::unhighlightNodes((*documents)[pane][L"root"], (*documents)[pane].GetAllocator());
							textSegments[pane].Make((*documents)[pane][L"root"]);
						}
						m_diffInfos = Comparer::compare(m_diffOptions, textSegments);
						Comparer::setNodeIdInDiffInfoList(m_diffInfos, textSegments);
						if (m_currentDiffIndex != -1 && static_cast<size_t>(m_currentDiffIndex) >= m_diffInfos.size())
							m_currentDiffIndex = static_cast<int>(m_diffInfos.size() - 1);
						if (m_bShowDifferences)
						{
							Highlighter highlighter(*documents.get(), m_diffInfos, m_colorSettings, m_diffOptions, m_bShowWordDifferences, m_currentDiffIndex);
							highlighter.highlightNodes();
#ifdef _DEBUG
							for (int pane = 0; pane < m_nPanes; ++pane)
							{
								WStringBuffer buffer;
								WPrettyWriter writer(buffer);
								(*documents)[pane].Accept(writer);
								WriteToTextFile((L"c:\\tmp\\file" + std::to_wstring(pane) + L"_1.json"),
									buffer.GetString());
							}
#endif
						}
						hr = highlightDocuments(documents,
							Callback<IWebDiffCallback>([this, callback2](const WebDiffCallbackResult& result) -> HRESULT
								{
									HRESULT hr = result.errorCode;
									if (SUCCEEDED(hr))
									{
										hr = setStyleSheetLoop(Highlighter::getStyleSheetText(m_currentDiffIndex, m_colorSettings).c_str(),
											Callback<IWebDiffCallback>([this, callback2](const WebDiffCallbackResult& result) -> HRESULT
												{
													HRESULT hr = result.errorCode;
													SetCompareState(FAILED(hr) ? NOT_COMPARED : COMPARED);
													if (callback2)
														return callback2->Invoke({ hr, nullptr });
													return S_OK;
												}).Get());
									}
									if (FAILED(hr))
										SetCompareState(NOT_COMPARED);
									if (FAILED(hr) && callback2)
										return callback2->Invoke({ hr, nullptr });
									return S_OK;
								}).Get());
					}
					if (FAILED(hr))
						SetCompareState(NOT_COMPARED);
					if (FAILED(hr) && callback2)
						return callback2->Invoke({ hr, nullptr });
					return S_OK;
				}).Get());
		if (FAILED(hr))
			SetCompareState(NOT_COMPARED);
		return hr;
	}

	HRESULT saveFilesLoop(FormatType kind, std::shared_ptr<std::vector<std::wstring>> filenames, IWebDiffCallback* callback, int pane = 0)
	{
		ComPtr<IWebDiffCallback> callback2(callback);
		HRESULT hr = m_webWindow[pane].SaveFile((*filenames)[pane].c_str(), kind,
			Callback<IWebDiffCallback>([this, kind, filenames, callback2, pane](const WebDiffCallbackResult& result) -> HRESULT
				{
					HRESULT hr = result.errorCode;
					if (SUCCEEDED(hr))
					{
						if (pane + 1 < m_nPanes)
							hr = saveFilesLoop(kind, filenames, callback2.Get(), pane + 1);
						else if (callback2)
							return callback2->Invoke({ hr, nullptr });
					}
					if (FAILED(hr) && callback2)
						return callback2->Invoke({ hr, nullptr });
					return S_OK;
				}).Get());
		return hr;
	}

	HRESULT applyHTMLLoop(
		int pane,
		std::shared_ptr<const std::list<ModifiedNode>> nodes,
		IWebDiffCallback* callback)
	{
		size_t size = nodes->size();
		if (size == 0)
		{
			if (callback)
				callback->Invoke({ S_OK, nullptr });
			return S_OK;
		}
		ComPtr<IWebDiffCallback> callback2(callback);
		auto count = std::make_shared<size_t>();
		for (auto it = nodes->rbegin(); it != nodes->rend(); ++it)
		{
			HRESULT hr = m_webWindow[pane].SetOuterHTML(it->nodeId, it->outerHTML,
				Callback<IWebDiffCallback>([this, count, size, callback2](const WebDiffCallbackResult& result)->HRESULT
				{
					(*count)++;
					if (*count == size && callback2)
						callback2->Invoke({ result.errorCode, nullptr });
					return S_OK;
				}).Get());
			if (FAILED(hr))
			{
				(*count)++;
				if (*count == size && callback2)
					callback2->Invoke({ hr, nullptr });
			}
		}
		return S_OK;
	}

	HRESULT applyDOMLoop(std::shared_ptr<std::vector<WDocument>> documents, IWebDiffCallback* callback)
	{
		ComPtr<IWebDiffCallback> callback2(callback);
		auto count = std::make_shared<int>();
		for (int pane = 0; pane < m_nPanes; ++pane)
		{
			auto nodes = std::make_shared<std::list<ModifiedNode>>();
			Highlighter::modifiedNodesToHTMLs((*documents)[pane][L"root"], *nodes);
			HRESULT hr = applyHTMLLoop(pane, nodes,
				Callback<IWebDiffCallback>([this, callback2, count](const WebDiffCallbackResult& result) -> HRESULT
					{
						(*count)++;
						if (*count == m_nPanes && callback2)
							callback2->Invoke({ result.errorCode, nullptr });
						return S_OK;
					}).Get());
			if (FAILED(hr))
			{
				(*count)++;
				if (*count == m_nPanes && callback2)
					callback2->Invoke({ hr, nullptr });
			}
		}
		return S_OK;
	}

	HRESULT setStyleSheetLoop(const std::wstring& styles, IWebDiffCallback* callback, int pane = 0)
	{
		ComPtr<IWebDiffCallback> callback2(callback);
		HRESULT hr = m_webWindow[pane].SetStyleSheetInAllFrames(styles,
			Callback<IWebDiffCallback>([this, pane, styles, callback2](const WebDiffCallbackResult& result) -> HRESULT
				{
					HRESULT hr = result.errorCode;
					if (SUCCEEDED(hr))
					{
						if (pane + 1 < m_nPanes)
							hr = setStyleSheetLoop(styles, callback2.Get(), pane + 1);
						else if (callback2)
							return callback2->Invoke({ hr, nullptr });
					}
					if (FAILED(hr) && callback2)
						return callback2->Invoke({ hr, nullptr });
					return S_OK;
				}).Get());
		return hr;
	}

	HRESULT highlightDocuments(std::shared_ptr<std::vector<WDocument>> documents, IWebDiffCallback* callback)
	{
		ComPtr<IWebDiffCallback> callback2(callback);
		HRESULT hr = applyDOMLoop(documents,
			Callback<IWebDiffCallback>([this, documents, callback2](const WebDiffCallbackResult& result) -> HRESULT
				{
					HRESULT hr = result.errorCode;
					if (SUCCEEDED(hr))
						hr = makeDiffNodeIdArrayLoop(callback2.Get());
					if (FAILED(hr) && callback2)
						return callback2->Invoke({ hr, nullptr });
					return S_OK;
				}).Get());
		return hr;
	}

	HRESULT unhighlightDifferencesLoop(IWebDiffCallback* callback, int pane = 0)
	{
		static const wchar_t* method = L"DOM.getDocument";
		static const wchar_t* params = L"{ \"depth\": -1, \"pierce\": true }";
		ComPtr<IWebDiffCallback> callback2(callback);
		HRESULT hr = m_webWindow[pane].CallDevToolsProtocolMethod(method, params,
			Callback<IWebDiffCallback>([this, pane, callback2](const WebDiffCallbackResult& result) -> HRESULT
				{
					HRESULT hr = result.errorCode;
					if (SUCCEEDED(hr))
					{
						WDocument doc;
						doc.Parse(result.returnObjectAsJson);
						Highlighter::unhighlightNodes(doc[L"root"], doc.GetAllocator());
						auto nodes = std::make_shared<std::list<ModifiedNode>>();
						Highlighter::modifiedNodesToHTMLs(doc[L"root"], *nodes);
						hr = applyHTMLLoop(pane, nodes,
							Callback<IWebDiffCallback>([this, pane, callback2](const WebDiffCallbackResult& result) -> HRESULT
								{
									HRESULT hr = result.errorCode;
									if (SUCCEEDED(hr))
									{
										if (pane + 1 < m_nPanes)
											hr = unhighlightDifferencesLoop(callback2.Get(), pane + 1);
										else if (callback2)
											return callback2->Invoke({ hr, nullptr });
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
		return hr;
	}

	HRESULT makeDiffNodeIdArrayLoop(IWebDiffCallback* callback, int pane = 0)
	{
		static const wchar_t* method = L"DOM.getDocument";
		static const wchar_t* params = L"{ \"depth\": -1, \"pierce\": true }";
		ComPtr<IWebDiffCallback> callback2(callback);
		HRESULT hr = m_webWindow[pane].CallDevToolsProtocolMethod(method, params,
			Callback<IWebDiffCallback>([this, pane, callback2](const WebDiffCallbackResult& result) -> HRESULT
				{
					HRESULT hr = result.errorCode;
					if (SUCCEEDED(hr))
					{
						WDocument doc;
						doc.Parse(result.returnObjectAsJson);
#ifdef _DEBUG
						WStringBuffer buffer;
						WPrettyWriter writer(buffer);
						doc.Accept(writer);
						WriteToTextFile((L"c:\\tmp\\file" + std::to_wstring(pane) + L"_2.json"),
							buffer.GetString());
#endif

						std::map<int, int> nodes;
						Highlighter::getDiffNodes(doc[L"root"], nodes);
						for (unsigned i = 0; i < m_diffInfos.size(); ++i)
						{
							if (m_diffInfos[i].nodeIds[pane] != -1)
								m_diffInfos[i].nodeIds[pane] = nodes[i];
						}
						if (pane < m_nPanes - 1)
							hr = makeDiffNodeIdArrayLoop(callback2.Get(), pane + 1);
						else if (callback2)
							return callback2->Invoke({ hr, nullptr });
					}
					if (FAILED(hr) && callback2)
						return callback2->Invoke({ hr, nullptr });
					return S_OK;
				}).Get());
		return hr;
	}

	HRESULT scrollIntoViewIfNeededLoop(int diffIndex, IWebDiffCallback* callback, int pane = 0)
	{
		ComPtr<IWebDiffCallback> callback2(callback);
		std::wstring args = L"{ \"nodeId\": " + std::to_wstring(m_diffInfos[diffIndex].nodeIds[pane]) + L" }";
		HRESULT hr = m_webWindow[pane].CallDevToolsProtocolMethod(L"DOM.scrollIntoViewIfNeeded", args.c_str(),
			Callback<IWebDiffCallback>([this, diffIndex, pane, callback2](const WebDiffCallbackResult& result) -> HRESULT
				{
					HRESULT hr = S_OK; // result.errorCode;
					m_webWindow[pane].ShowToolTip(FAILED(result.errorCode), 5000);
					if (SUCCEEDED(hr))
					{
						if (pane + 1 < m_nPanes)
							hr = scrollIntoViewIfNeededLoop(diffIndex, callback2.Get(), pane + 1);
						else if (callback2)
							return callback2->Invoke({ hr, nullptr });
					}
					if (FAILED(hr) && callback2)
						return callback2->Invoke(result);
					return S_OK;
				}).Get());
		return hr;
	}

	HRESULT selectDiff(int diffIndex, IWebDiffCallback* callback)
	{
		if (diffIndex < 0 || static_cast<size_t>(diffIndex) >= m_diffInfos.size())
			return false;
		ComPtr<IWebDiffCallback> callback2(callback);
		std::shared_ptr<std::vector<std::wstring>> jsons(new std::vector<std::wstring>());
		HRESULT hr = setStyleSheetLoop(Highlighter::getStyleSheetText(diffIndex, m_colorSettings).c_str(),
			Callback<IWebDiffCallback>([this, diffIndex, callback2](const WebDiffCallbackResult& result) -> HRESULT
				{
					HRESULT hr = result.errorCode;
					if (SUCCEEDED(hr))
					{
						hr = scrollIntoViewIfNeededLoop(diffIndex,
							Callback<IWebDiffCallback>([this, callback2](const WebDiffCallbackResult& result) -> HRESULT
								{
									HRESULT hr = result.errorCode;
									if (SUCCEEDED(hr))
									{
										WebDiffEvent ev{ WebDiffEvent::DiffSelected, -1 };
										RaiseEvent(ev);
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
		return hr;
	}

	std::wstring getFromClipboard() const
	{
		std::wstring text;
		if (OpenClipboard(m_hWnd))
		{
			HGLOBAL hData = GetClipboardData(CF_UNICODETEXT);
			if (hData != nullptr)
			{
				LPWSTR pszData = (LPWSTR) GlobalLock(hData);
				if (pszData != nullptr)
				{
					text = pszData;
					GlobalUnlock(hData);
				}
			}
			CloseClipboard();
		}
		return text;
	}

	bool execCommand(const wchar_t *command)
	{
		HWND hwndFocus = GetFocus();
		if (!hwndFocus)
			return false;
		std::wstring cmd = command;
		wchar_t classNameBuf[256]{};
		GetClassName(hwndFocus, classNameBuf, sizeof(classNameBuf) / sizeof(wchar_t));
		std::wstring className = classNameBuf;
		if (className == L"Edit")
		{
			UINT msg = 0;
			WPARAM wParam = 0;
			LPARAM lParam = 0;
			if (cmd == L"copy") { msg = WM_COPY; }
			else if (cmd == L"cut") { msg = WM_CUT; }
			else if (cmd == L"paste") { msg = WM_PASTE; }
			else if (cmd == L"selectall") { msg = EM_SETSEL; wParam = 0; lParam = -1; }
			else if (cmd == L"undo") { msg = WM_UNDO; }
			SendMessage(hwndFocus, msg, wParam, lParam);
			return true;
		}
		int pane = GetActivePane();
		if (pane < 0)
			return false;
		std::wstring script;
		if (cmd != L"paste")
			script = L"document.execCommand(\"" + cmd + L"\")";
		else
		{
			std::wstring text = utils::Quote(getFromClipboard());
			script = L"document.execCommand(\"insertText\", false, " + text + L")";
		}
		return SUCCEEDED(m_webWindow[pane].ExecuteScript(script.c_str(), nullptr));
	}

	std::wstring GetUserDataFolderPath(int pane)
	{
		std::wstring path;
		
		if (m_userDataFolderType == UserdataFolderType::APPDATA)
			path = wil::ExpandEnvironmentStringsW(L"%APPDATA%\\WinMerge\\WinWebDiff\\").get();
		else
			path = wil::GetModuleFileNameW(GetModuleHandle(nullptr)).get() + std::wstring(L".WebView2");
		if (m_bUserDataFolderPerPane)
			path += L"\\" + std::to_wstring(pane + 1);
		return path;
	}

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
				int cx = 0; //GetSystemMetrics(SM_CXVSCROLL);
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
				int cy = 0; // GetSystemMetrics(SM_CXVSCROLL);
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
		m_nDraggingSplitter = i;
		SetCapture(m_hWnd);
	}

	void OnLButtonUp(UINT nFlags, int x, int y)
	{
		if (m_nDraggingSplitter == -1)
			return;
		ReleaseCapture();
		MoveSplitter(x, y);
		m_nDraggingSplitter = -1;
	}

	void OnMouseMove(UINT nFlags, int x, int y)
	{
		if (m_nPanes < 2)
			return;
		if (m_nDraggingSplitter == -1)
			return;
		MoveSplitter(x, y);
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
			if ((HWND)wParam == m_hWnd)
			{
				SetCursor(::LoadCursor(nullptr, !m_bHorizontalSplit ? IDC_SIZEWE : IDC_SIZENS));
				return TRUE;
			}
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
		CWebDiffWindow* pWebWnd = reinterpret_cast<CWebDiffWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
		LRESULT lResult = pWebWnd->OnWndMsg(hwnd, iMsg, wParam, lParam);
		return lResult;
	}

	ATOM MyRegisterClass(HINSTANCE hInstance)
	{
		WNDCLASSEXW wcex = { 0 };
		wcex.cbSize = sizeof(WNDCLASSEX);
		wcex.style = CS_HREDRAW | CS_VREDRAW;
		wcex.lpfnWndProc = (WNDPROC)CWebDiffWindow::WndProc;
		wcex.cbClsExtra = 0;
		wcex.cbWndExtra = 0;
		wcex.hInstance = hInstance;
		wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
		wcex.hbrBackground = (HBRUSH)(COLOR_3DFACE + 1);
		wcex.lpszClassName = L"WinWebDiffWindowClass";
		return RegisterClassExW(&wcex);
	}

	static HRESULT WriteToTextFile(const std::wstring& path, const std::wstring& data)
	{
		wil::unique_file fp;
		_wfopen_s(&fp, path.c_str(), L"wt,ccs=UTF-8");
		if (fp)
			fwprintf(fp.get(), L"%s", data.c_str());
		return fp != nullptr ? S_OK : (GetLastError() == 0 ? E_FAIL : HRESULT_FROM_WIN32(GetLastError()));
	}

	static wchar_t* GetScriptOnLoad()
	{
		LPVOID pData = nullptr;
		HMODULE hModule = GetModuleHandle(L"WinWebDiffLib.dll");
		HRSRC hResource = FindResource(hModule, MAKEINTRESOURCE(IDR_SCRIPT), RT_RCDATA);
		if (hResource) {
			HGLOBAL hLoadedResource = LoadResource(hModule, hResource);
			if (hLoadedResource) {
				pData = LockResource(hLoadedResource);
				FreeResource(hLoadedResource);
			}
		}
		return reinterpret_cast<wchar_t*>(pData) + 1/*bom*/;
	}

	int m_nPanes = 0;
	HWND m_hWnd = nullptr;
	HINSTANCE m_hInstance = nullptr;
	CWebWindow m_webWindow[3];
	int m_nDraggingSplitter = -1;
	bool m_bHorizontalSplit = false;
	bool m_bDragging = false;
	POINT m_ptOrg{};
	POINT m_ptPrev{};
	SIZE m_size{ 1024, 600 };
	bool m_fitToWindow = true;
	double m_zoom = 1.0;
	std::wstring m_userAgent = L"";
	UserdataFolderType m_userDataFolderType = UserdataFolderType::APPDATA;
	bool m_bUserDataFolderPerPane = true;
	std::vector<ComPtr<IWebDiffEventHandler>> m_listeners;
	int m_currentDiffIndex = -1;
	std::vector<DiffInfo> m_diffInfos;
	DiffLocation m_diffLocation[3];
	DiffOptions m_diffOptions{};
	bool m_bShowDifferences = true;
	bool m_bShowWordDifferences = true;
	bool m_bSynchronizeEvents = true;
	bool m_documentLoaded[3] = { false, false, false };
	bool m_urlChanged[3] = { false, false, false };
	unsigned m_eventSyncFlags = EVENT_SCROLL | EVENT_CLICK | EVENT_INPUT | EVENT_GOBACKFORWARD;
	CompareState m_compareState = NOT_COMPARED;
	IWebDiffWindow::ColorSettings m_colorSettings = {
		RGB(239, 203,   5), RGB(192, 192, 192), RGB(0, 0, 0),
		RGB(239, 119, 116), RGB(240, 192, 192), RGB(0, 0, 0),
		RGB(251, 242, 191), RGB(233, 233, 233), RGB(0, 0, 0),
		RGB(228, 155,  82), RGB(192, 192, 192), RGB(0, 0, 0),
		RGB(248, 112,  78), RGB(252, 181, 163), RGB(0, 0, 0),
		RGB(251, 250, 223), RGB(233, 233, 233), RGB(0, 0, 0),
		RGB(239, 183, 180), RGB(240, 224, 224), RGB(0, 0, 0),
		RGB(241, 226, 173), RGB(255, 170, 130), RGB(0, 0, 0),
		RGB(255, 160, 160), RGB(200, 129, 108), RGB(0, 0, 0),
	};
	LogCallback m_logCallback;
};
