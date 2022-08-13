#pragma once

#include "WinWebDiffLib.h"
#include "WebWindow.hpp"
#include "Utils.hpp"
#include "Diff.hpp"
#include <Windows.h>
#include <shellapi.h>
#include <vector>
#include <map>
#include <wil/win32_helpers.h>
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

using WDocument = rapidjson::GenericDocument<rapidjson::UTF16<>>;
using WValue = rapidjson::GenericValue<rapidjson::UTF16<>>;

enum OP_TYPE
{
	OP_NONE = 0, OP_1STONLY, OP_2NDONLY, OP_3RDONLY, OP_DIFF, OP_TRIVIAL
};

struct DiffInfo
{
	DiffInfo(int begin1 = 0, int end1 = 0,
	         int begin2 = 0, int end2 = 0,
	         int begin3 = 0, int end3 = 0)
		: begin{ begin1, begin2, begin3}
		, end{ end1, end2, end3 }
		, op(OP_DIFF)
	{}

	DiffInfo(const DiffInfo& src)
		: nodeIds{ src.nodeIds[0], src.nodeIds[1], src.nodeIds[2] }
		, begin{ src.begin[0], src.begin[1], src.begin[2] }
		, end{ src.end[0], src.end[1], src.end[2] }
		, op(src.op)
	{}
	int nodeIds[3];
	int begin[3];
	int end[3];
	OP_TYPE op;
};

struct TextSegment
{
	int nodeId;
	size_t begin;
	size_t size;
};

struct TextBlocks
{
	void Make(const WValue& nodeTree)
	{
		const int nodeType = nodeTree[L"nodeType"].GetInt();

		if (nodeType == 3 /* TEXT_NODE */)
		{
			std::wstring text = nodeTree[L"nodeValue"].GetString();
			TextSegment seg;
			seg.nodeId = nodeTree[L"nodeId"].GetInt();
			seg.begin = textBlocks.size();
			seg.size = text.size();
			textBlocks += text;
			segments.insert_or_assign(seg.begin, seg);

		}
		if (nodeTree.HasMember(L"children") && nodeTree[L"children"].IsArray())
		{
			const auto* nodeName = nodeTree[L"nodeName"].GetString();
			if (wcscmp(nodeName, L"SCRIPT") != 0 &&
			    wcscmp(nodeName, L"NOSCRIPT") != 0 &&
			    wcscmp(nodeName, L"NOFRAMES") != 0 &&
			    wcscmp(nodeName, L"STYLE") != 0 &&
			    wcscmp(nodeName, L"TITLE") != 0)
			{
				for (const auto& child : nodeTree[L"children"].GetArray())
				{
					Make(child);
				}
			}
		}
		if (nodeTree.HasMember(L"contentDocument"))
		{
			Make(nodeTree[L"contentDocument"]);
		}
	}
	std::wstring textBlocks;
	std::map<size_t, TextSegment> segments;
};

class DataForDiff
{
public:
	DataForDiff(const TextBlocks& textBlocks, const IWebDiffWindow::DiffOptions& diffOptions) :
		m_textBlocks(textBlocks), m_diffOptions(diffOptions)
	{
	}
	~DataForDiff()
	{
	}
	unsigned size() const { return static_cast<unsigned>(m_textBlocks.textBlocks.size() * sizeof(wchar_t)); }
	const char* data() const { return reinterpret_cast<const char*>(m_textBlocks.textBlocks.data()); }
	const char* next(const char* scanline) const
	{
		auto it = m_textBlocks.segments.find(reinterpret_cast<const wchar_t*>(scanline) - m_textBlocks.textBlocks.data());
		if (it != m_textBlocks.segments.end())
			return scanline + it->second.size * sizeof(wchar_t);
		return nullptr;
	}
	bool equals(const char* scanline1, unsigned size1,
		const char* scanline2, unsigned size2) const
	{
		if (size1 != size2)
			return false;
		return memcmp(scanline1, scanline2, size1) == 0;
	}
	unsigned long hash(const char* scanline) const
	{
		unsigned long ha = 5381;
		const char* begin = scanline;
		const char* end = this->next(scanline);

		for (const auto* ptr = begin; ptr < end; ptr++)
		{
			ha += (ha << 5);
			ha ^= *ptr & 0xFF;
		}
		return ha;
	}

private:
	const TextBlocks& m_textBlocks;
	const IWebDiffWindow::DiffOptions& m_diffOptions;
};

namespace Comparer
{
	template<typename Element, typename Comp02Func>
	std::vector<Element> Make3WayLineDiff(const std::vector<Element>& diff10, const std::vector<Element>& diff12,
		Comp02Func cmpfunc)
	{
		std::vector<Element> diff3;

		size_t diff10count = diff10.size();
		size_t diff12count = diff12.size();

		size_t diff10i = 0;
		size_t diff12i = 0;
		size_t diff3i = 0;

		bool firstDiffBlockIsDiff12;

		Element dr3, dr10, dr12, dr10first, dr10last, dr12first, dr12last;

		int linelast0 = 0;
		int linelast1 = 0;
		int linelast2 = 0;

		for (;;)
		{
			if (diff10i >= diff10count && diff12i >= diff12count)
				break;

			/*
			 * merge overlapped diff blocks
			 * diff10 is diff blocks between file1 and file0.
			 * diff12 is diff blocks between file1 and file2.
			 *
			 *                      diff12
			 *                 diff10            diff3
			 *                 |~~~|             |~~~|
			 * firstDiffBlock  |   |             |   |
			 *                 |   | |~~~|       |   |
			 *                 |___| |   |       |   |
			 *                       |   |   ->  |   |
			 *                 |~~~| |___|       |   |
			 * lastDiffBlock   |   |             |   |
			 *                 |___|             |___|
			 */

			if (diff10i >= diff10count && diff12i < diff12count)
			{
				dr12first = diff12.at(diff12i);
				dr12last = dr12first;
				firstDiffBlockIsDiff12 = true;
			}
			else if (diff10i < diff10count && diff12i >= diff12count)
			{
				dr10first = diff10.at(diff10i);
				dr10last = dr10first;
				firstDiffBlockIsDiff12 = false;
			}
			else
			{
				dr10first = diff10.at(diff10i);
				dr12first = diff12.at(diff12i);
				dr10last = dr10first;
				dr12last = dr12first;
				if (dr12first.begin[0] <= dr10first.begin[0])
					firstDiffBlockIsDiff12 = true;
				else
					firstDiffBlockIsDiff12 = false;
			}
			bool lastDiffBlockIsDiff12 = firstDiffBlockIsDiff12;

			size_t diff10itmp = diff10i;
			size_t diff12itmp = diff12i;
			for (;;)
			{
				if (diff10itmp >= diff10count || diff12itmp >= diff12count)
					break;

				dr10 = diff10.at(diff10itmp);
				dr12 = diff12.at(diff12itmp);

				if (dr10.end[0] == dr12.end[0])
				{
					diff10itmp++;
					lastDiffBlockIsDiff12 = true;

					dr10last = dr10;
					dr12last = dr12;
					break;
				}

				if (lastDiffBlockIsDiff12)
				{
					if ((std::max)(dr12.begin[0], dr12.end[0]) < dr10.begin[0])
						break;
				}
				else
				{
					if ((std::max)(dr10.begin[0], dr10.end[0]) < dr12.begin[0])
						break;
				}

				if (dr12.end[0] > dr10.end[0])
				{
					diff10itmp++;
					lastDiffBlockIsDiff12 = true;
				}
				else
				{
					diff12itmp++;
					lastDiffBlockIsDiff12 = false;
				}

				dr10last = dr10;
				dr12last = dr12;
			}

			if (lastDiffBlockIsDiff12)
				diff12itmp++;
			else
				diff10itmp++;

			if (firstDiffBlockIsDiff12)
			{
				dr3.begin[1] = dr12first.begin[0];
				dr3.begin[2] = dr12first.begin[1];
				if (diff10itmp == diff10i)
					dr3.begin[0] = dr3.begin[1] - linelast1 + linelast0;
				else
					dr3.begin[0] = dr3.begin[1] - dr10first.begin[0] + dr10first.begin[1];
			}
			else
			{
				dr3.begin[0] = dr10first.begin[1];
				dr3.begin[1] = dr10first.begin[0];
				if (diff12itmp == diff12i)
					dr3.begin[2] = dr3.begin[1] - linelast1 + linelast2;
				else
					dr3.begin[2] = dr3.begin[1] - dr12first.begin[0] + dr12first.begin[1];
			}

			if (lastDiffBlockIsDiff12)
			{
				dr3.end[1] = dr12last.end[0];
				dr3.end[2] = dr12last.end[1];
				if (diff10itmp == diff10i)
					dr3.end[0] = dr3.end[1] - linelast1 + linelast0;
				else
					dr3.end[0] = dr3.end[1] - dr10last.end[0] + dr10last.end[1];
			}
			else
			{
				dr3.end[0] = dr10last.end[1];
				dr3.end[1] = dr10last.end[0];
				if (diff12itmp == diff12i)
					dr3.end[2] = dr3.end[1] - linelast1 + linelast2;
				else
					dr3.end[2] = dr3.end[1] - dr12last.end[0] + dr12last.end[1];
			}

			linelast0 = dr3.end[0] + 1;
			linelast1 = dr3.end[1] + 1;
			linelast2 = dr3.end[2] + 1;

			if (diff10i == diff10itmp)
				dr3.op = OP_3RDONLY;
			else if (diff12i == diff12itmp)
				dr3.op = OP_1STONLY;
			else
			{
				if (!cmpfunc(dr3))
					dr3.op = OP_DIFF;
				else
					dr3.op = OP_2NDONLY;
			}

			diff3.push_back(dr3);

			diff3i++;
			diff10i = diff10itmp;
			diff12i = diff12itmp;
		}

		for (size_t i = 0; i < diff3i; i++)
		{
			Element& dr3r = diff3.at(i);
			if (i < diff3i - 1)
			{
				Element& dr3next = diff3.at(i + 1);
				for (int j = 0; j < 3; j++)
				{
					if (dr3r.end[j] >= dr3next.begin[j])
						dr3r.end[j] = dr3next.begin[j] - 1;
				}
			}
		}

		return diff3;
	}

	std::vector<DiffInfo> edscriptToDiffInfo(const std::vector<char>& edscript, const TextBlocks& textBlocks0, const TextBlocks& textBlocks1)
	{
		std::vector<DiffInfo> diffInfoList;
		int i0 = 0, i1 = 0;
		auto it0 = textBlocks0.segments.begin();
		auto it1 = textBlocks1.segments.begin();
		for (auto ed : edscript)
		{
			switch (ed)
			{
			case '-':
				diffInfoList.emplace_back(i0, i0, i1, i1 - 1);
				++it0;
				++i0;
				break;
			case '+':
				diffInfoList.emplace_back(i0, i0 - 1, i1, i1);
				++it1;
				++i1;
				break;
			case '!':
				diffInfoList.emplace_back(i0, i0, i1, i1);
				++it0;
				++it1;
				++i0;
				++i1;
				break;
			default:
				++it0;
				++it1;
				++i0;
				++i1;
				break;
			}
		}
		return diffInfoList;
	}

	void setNodeIdInDiffInfoList(std::vector<DiffInfo>& diffInfoList,
		const TextBlocks textBlocks[], int paneCount)
	{
		for (size_t i = 0; i < diffInfoList.size(); ++i)
		{
			for (int pane = 0; pane < paneCount; ++pane)
			{
				auto it = textBlocks[pane].segments.begin();
				std::advance(it, diffInfoList[i].begin[pane]);
				if (diffInfoList[i].end[pane] < diffInfoList[i].begin[pane])
				{
					--it;
					diffInfoList[i].nodeIds[pane] = -it->second.nodeId - 1;
				}
				else
				{
					diffInfoList[i].nodeIds[pane] = it->second.nodeId;
				}
			}
		}
	}

	std::vector<DiffInfo> CompareDocuments(const IWebDiffWindow::DiffOptions& diffOptions,
		const std::vector<WDocument>& documents)
	{
		TextBlocks textBlocks[3];
		textBlocks[0].Make(documents[0][L"root"]);
		textBlocks[1].Make(documents[1][L"root"]);
		DataForDiff data0(textBlocks[0], diffOptions);
		DataForDiff data1(textBlocks[1], diffOptions);
		if (documents.size() < 3)
		{
			Diff<DataForDiff> diff(data0, data1);
			std::vector<char> edscript;

			diff.diff(static_cast<Diff<DataForDiff>::Algorithm>(diffOptions.diffAlgorithm), edscript);
			std::vector<DiffInfo> diffInfoList = edscriptToDiffInfo(edscript, textBlocks[0], textBlocks[1]);
			setNodeIdInDiffInfoList(diffInfoList, textBlocks, 2);
			return diffInfoList;
		}

		textBlocks[2].Make(documents[2][L"root"]);
		DataForDiff data2(textBlocks[2], diffOptions);
		Diff<DataForDiff> diff10(data1, data0);
		Diff<DataForDiff> diff12(data1, data2);
		Diff<DataForDiff> diff20(data2, data0);
		std::vector<char> edscript10, edscript12;
		diff10.diff(static_cast<Diff<DataForDiff>::Algorithm>(diffOptions.diffAlgorithm), edscript10);
		diff12.diff(static_cast<Diff<DataForDiff>::Algorithm>(diffOptions.diffAlgorithm), edscript12);
		std::vector<DiffInfo> diffInfoList10 = edscriptToDiffInfo(edscript10, textBlocks[1], textBlocks[0]);
		std::vector<DiffInfo> diffInfoList12 = edscriptToDiffInfo(edscript12, textBlocks[1], textBlocks[2]);

		auto compfunc02 = [&](const DiffInfo & wd3) {
			auto it0 = textBlocks[0].segments.begin();
			auto it2 = textBlocks[2].segments.begin();
			std::advance(it0, wd3.begin[0]);
			if (it0 == textBlocks[0].segments.end())
				return false;
			std::advance(it2, wd3.begin[2]);
			if (it2 == textBlocks[2].segments.end())
				return false;
			unsigned s0 = static_cast<unsigned>(textBlocks[0].segments[it0->second.begin].size);
			unsigned s2 = static_cast<unsigned>(textBlocks[2].segments[it2->second.begin].size);
			return data2.equals(
				reinterpret_cast<const char *>(textBlocks[0].textBlocks.data()) + it0->second.begin, s0,
				reinterpret_cast<const char *>(textBlocks[2].textBlocks.data()) + it2->second.begin, s2
				);
		};

		std::vector<DiffInfo> diffInfoList = Make3WayLineDiff(diffInfoList10, diffInfoList12, compfunc02);
		setNodeIdInDiffInfoList(diffInfoList, textBlocks, 3);
		return diffInfoList;
	}
}

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
			std::shared_ptr<int> counter(new int{ nPanes });
			for (int i = 0; i < nPanes; ++i)
			{
				std::wstring userDataFolder = GetUserDataFolderPath(i);
				ComPtr<IWebDiffCallback> callback2(callback);
				hr = m_webWindow[i].Create(m_hInstance, m_hWnd, urls[i], userDataFolder.c_str(),
						m_size, m_fitToWindow, m_zoom, m_userAgent, nullptr,
						[this, i, counter, callback2](WebDiffEvent::EVENT_TYPE event)
							{
								WebDiffEvent ev;
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
								else if (event == WebDiffEvent::NavigationCompleted)
								{
									*counter = *counter - 1;
									if (*counter == 0)
										Recompare(callback2.Get());
								}
								for (const auto& listener : m_listeners)
									listener->Invoke(ev);
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
		if (!m_bShowDifferences)
			return m_webWindow[pane].SaveFile(filename, kind, callback);
		ComPtr<IWebDiffCallback> callback2(callback);
		std::wstring sfilename(filename);
		return unhighlightDifferencesLoop(
			Callback<IWebDiffCallback>([this, kind, pane, sfilename, callback2](const WebDiffCallbackResult& result) -> HRESULT
				{
					return m_webWindow[pane].SaveFile(sfilename, kind,
						Callback<IWebDiffCallback>([this, callback2](const WebDiffCallbackResult& result) -> HRESULT
							{
								HRESULT hr = result.errorCode;
								if (SUCCEEDED(hr))
									hr = compare(callback2.Get());
								if (FAILED(hr) && callback2)
									callback2->Invoke({ hr, nullptr });
								return S_OK;
							}).Get());
				}).Get());
	}

	HRESULT SaveFiles(FormatType kind, const wchar_t* filenames[], IWebDiffCallback* callback) override
	{
		auto sfilenames = std::make_shared<std::vector<std::wstring>>();
		for (int pane = 0; pane < m_nPanes; ++pane)
			sfilenames->push_back(filenames[pane]);
		if (!m_bShowDifferences)
			return saveFilesLoop(kind, sfilenames, callback);
		ComPtr<IWebDiffCallback> callback2(callback);
		return unhighlightDifferencesLoop(
			Callback<IWebDiffCallback>([this, kind, sfilenames, callback2](const WebDiffCallbackResult& result) -> HRESULT
				{
					return saveFilesLoop(kind, sfilenames,
						Callback<IWebDiffCallback>([this, sfilenames, callback2](const WebDiffCallbackResult& result) -> HRESULT
							{
								HRESULT hr = result.errorCode;
								if (SUCCEEDED(hr))
									hr = compare(callback2.Get());
								if (FAILED(hr) && callback2)
									callback2->Invoke({ hr, nullptr });
								return S_OK;
							}).Get());
				}).Get());
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

	const DiffOptions& GetDiffOptions() const
	{
		return m_diffOptions;
	}

	void SetDiffOptions(const DiffOptions& diffOptions)
	{
		m_diffOptions = diffOptions;
		Recompare(nullptr);
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
		selectDiff(m_currentDiffIndex, oldDiffIndex);
		return true;
	}

	bool LastDiff() override
	{
		int oldDiffIndex = m_currentDiffIndex;
		m_currentDiffIndex = static_cast<int>(m_diffInfos.size()) - 1;
		if (oldDiffIndex == m_currentDiffIndex)
			return false;
		selectDiff(m_currentDiffIndex, oldDiffIndex);
		return true;
	}

	bool NextDiff() override
	{
		int oldDiffIndex = m_currentDiffIndex;
		++m_currentDiffIndex;
		if (m_currentDiffIndex >= m_diffInfos.size())
			m_currentDiffIndex = static_cast<int>(m_diffInfos.size()) - 1;
		if (oldDiffIndex == m_currentDiffIndex)
			return false;
		selectDiff(m_currentDiffIndex, oldDiffIndex);
		return true;
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
		selectDiff(m_currentDiffIndex, oldDiffIndex);
		return true;
	}

	bool FirstConflict() override
	{
		int oldDiffIndex = m_currentDiffIndex;
		for (size_t i = 0; i < m_diffInfos.size(); ++i)
			if (m_diffInfos[i].op == OP_DIFF)
				m_currentDiffIndex = static_cast<int>(i);
		if (oldDiffIndex == m_currentDiffIndex)
			return false;
		selectDiff(m_currentDiffIndex, oldDiffIndex);
		return true;
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
		selectDiff(m_currentDiffIndex, oldDiffIndex);
		return true;
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
		selectDiff(m_currentDiffIndex, oldDiffIndex);
		return true;
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
		selectDiff(m_currentDiffIndex, oldDiffIndex);
		return true;
	}

	bool SelectDiff(int diffIndex) override
	{
		return selectDiff(diffIndex, m_currentDiffIndex);
	}

	int  GetNextDiffIndex() const override
	{
		if (m_diffInfos.size() == 0 || m_currentDiffIndex >= m_diffInfos.size() - 1)
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

private:

	enum NodeType
	{
		ELEMENT_NODE = 1,
		ATTRIBUTE_NODE = 2,
		TEXT_NODE = 3,
		CDATA_SECTION_NODE = 4,
		PROCESSING_INSTRUCTION_NODE = 7,
		COMMENT_NODE = 8,
		DOCUMENT_NODE = 9,
		DOCUMENT_TYPE_NODE = 10,
		DOCUMENT_FRAGMENT_NODE = 11,
	};

	std::pair<WValue*, WValue*> findNodeId(WValue& nodeTree, int nodeId)
	{
		if (nodeTree[L"nodeId"].GetInt() == nodeId)
		{
			return { &nodeTree, nullptr };
		}
		if (nodeTree.HasMember(L"children") && nodeTree[L"children"].IsArray())
		{
			for (auto& child : nodeTree[L"children"].GetArray())
			{
				auto [pvalue, pparent] = findNodeId(child, nodeId);
				if (pvalue)
					return { pvalue, pparent ? pparent : &nodeTree};
			}
		}
		if (nodeTree.HasMember(L"contentDocument"))
		{
			auto [pvalue, pparent] = findNodeId(nodeTree[L"contentDocument"], nodeId);
			return { pvalue, pparent ? pparent : &nodeTree[L"contentDocument"]};
		}
		return { nullptr, nullptr };
	}

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
							callback2->Invoke({ hr, nullptr });
					}
					if (FAILED(hr) && callback2)
						callback2->Invoke({ hr, nullptr });
					return S_OK;
				}).Get());
		return hr;
	}

	HRESULT compare(IWebDiffCallback* callback)
	{
		ComPtr<IWebDiffCallback> callback2(callback);
		std::shared_ptr<std::vector<std::wstring>> jsons(new std::vector<std::wstring>());
		HRESULT hr = getDocumentsLoop(jsons,
			Callback<IWebDiffCallback>([this, jsons, callback2](const WebDiffCallbackResult& result) -> HRESULT
				{
					HRESULT hr = result.errorCode;
					if (SUCCEEDED(hr))
					{
						std::shared_ptr<std::vector<WDocument>> documents(new std::vector<WDocument>(m_nPanes));
						for (int pane = 0; pane < m_nPanes; ++pane)
						{
							(*documents)[pane].Parse((*jsons)[pane].c_str());
							unhighlightNodes((*documents)[pane], (*documents)[pane][L"root"]);
						}
						m_diffInfos = Comparer::CompareDocuments(m_diffOptions, *documents);
						if (m_currentDiffIndex != -1 && m_currentDiffIndex >= m_diffInfos.size())
							m_currentDiffIndex = static_cast<int>(m_diffInfos.size() - 1);
						if (m_bShowDifferences)
							highlightNodes(m_diffInfos, *documents);
						hr = highlightDocuments(documents, callback2.Get());
					}
					if (FAILED(hr) && callback2)
						callback2->Invoke(result);
					return S_OK;
				}).Get());
		return hr;
	}

	struct ModifiedNode
	{
		int nodeId;
		std::wstring outerHTML;
	};

	std::wstring modifiedNodesToHTMLs(const WValue& tree, std::list<ModifiedNode>& nodes)
	{
		std::wstring html;
		NodeType nodeType = static_cast<NodeType>(tree[L"nodeType"].GetInt());
		switch (nodeType)
		{
		case NodeType::DOCUMENT_TYPE_NODE:
		{
			html += L"<!DOCTYPE ";
			html += tree[L"nodeName"].GetString();
			html += L">";
			break;
		}
		case NodeType::DOCUMENT_NODE:
		{
			if (tree.HasMember(L"children"))
			{
				for (const auto& child : tree[L"children"].GetArray())
					html += modifiedNodesToHTMLs(child, nodes);
			}
			break;
		}
		case NodeType::COMMENT_NODE:
		{
			html += L"<!-- ";
			html += tree[L"nodeValue"].GetString();
			html += L" -->";
			break;
		}
		case NodeType::TEXT_NODE:
		{
			if (tree.HasMember(L"insertedNodes"))
			{
				for (const auto& child : tree[L"insertedNodes"].GetArray())
					html += modifiedNodesToHTMLs(child, nodes);
			}
			html += utils::EncodeHTMLEntities(tree[L"nodeValue"].GetString());
			if (tree.HasMember(L"modified"))
			{
				ModifiedNode node;
				node.nodeId = tree[L"nodeId"].GetInt();
				node.outerHTML = html;
				nodes.emplace_back(std::move(node));
			}
			break;
		}
		case NodeType::ELEMENT_NODE:
		{
			if (tree.HasMember(L"insertedNodes"))
			{
				for (const auto& child : tree[L"insertedNodes"].GetArray())
					html += modifiedNodesToHTMLs(child, nodes);
			}
			html += L'<';
			html += tree[L"nodeName"].GetString();
			if (tree.HasMember(L"attributes"))
			{
				const auto& attributes = tree[L"attributes"].GetArray();
				for (unsigned i = 0; i < attributes.Size(); i += 2)
				{
					html += L" ";
					html += attributes[i].GetString();
					html += L"=\"";
					if (i + 1 < attributes.Size())
						html += utils::EncodeHTMLEntities(attributes[i + 1].GetString());
					html += L"\"";
				}
			}
			html += L'>';
			if (tree.HasMember(L"children"))
			{
				for (const auto& child : tree[L"children"].GetArray())
					html += modifiedNodesToHTMLs(child, nodes);
			}
			if (tree.HasMember(L"contentDocument"))
			{
				for (const auto& child : tree[L"contentDocument"][L"children"].GetArray())
					modifiedNodesToHTMLs(child, nodes);
			}
			if (!utils::IsVoidElement(tree[L"nodeName"].GetString()))
			{
				html += L"</";
				html += tree[L"nodeName"].GetString();
				html += L'>';
			}
			if (tree.HasMember(L"modified"))
			{
				ModifiedNode node;
				node.nodeId = tree[L"nodeId"].GetInt();
				node.outerHTML = html;
				nodes.emplace_back(std::move(node));
			}
			break;
		}
		}
		return html;
	}

	bool isDiffNode(const WValue& value)
	{
		if (value[L"nodeType"].GetInt() != NodeType::ELEMENT_NODE)
			return false;
		if (wcscmp(value[L"nodeName"].GetString(), L"SPAN") != 0)
			return false;
		if (!value.HasMember(L"attributes"))
			return false;
		const auto& ary = value[L"attributes"].GetArray();
		if (ary.Size() < 2 ||
		    wcscmp(ary[0].GetString(), L"class") != 0 ||
		    wcscmp(ary[1].GetString(), L"wwd-diff") != 0)
			return false;
		return true;
	}
	
	void unhighlightNodes(WDocument& doc, WValue& tree)
	{
		NodeType nodeType = static_cast<NodeType>(tree[L"nodeType"].GetInt());
		switch (nodeType)
		{
		case NodeType::DOCUMENT_NODE:
		{
			if (tree.HasMember(L"children"))
			{
				for (auto& child : tree[L"children"].GetArray())
					unhighlightNodes(doc, child);
			}
			break;
		}
		case NodeType::ELEMENT_NODE:
		{
			if (tree.HasMember(L"children"))
			{
				for (auto& child : tree[L"children"].GetArray())
				{
					if (isDiffNode(child))
					{
						const int nodeId = child[L"nodeId"].GetInt();
						if (child[L"children"].GetArray().Size() > 0)
						{
							child.CopyFrom(child[L"children"].GetArray()[0], doc.GetAllocator());
						}
						else
						{
							child[L"nodeType"].SetInt(NodeType::TEXT_NODE);
							child[L"nodeValue"].SetString(L"");
						}
						child[L"nodeId"].SetInt(nodeId);
						child.AddMember(L"modified", true, doc.GetAllocator());
					}
					else
						unhighlightNodes(doc, child);
				}
			}
			if (tree.HasMember(L"contentDocument") && tree[L"contentDocument"].HasMember(L"children"))
			{
				for (auto& child : tree[L"contentDocument"][L"children"].GetArray())
					unhighlightNodes(doc, child);
			}
			break;
		}
		}
	}

	void getDiffNodes(WDocument& doc, WValue& tree, std::map<int, int>& nodes)
	{
		NodeType nodeType = static_cast<NodeType>(tree[L"nodeType"].GetInt());
		switch (nodeType)
		{
		case NodeType::DOCUMENT_NODE:
		{
			if (tree.HasMember(L"children"))
			{
				for (auto& child : tree[L"children"].GetArray())
					getDiffNodes(doc, child, nodes);
			}
			break;
		}
		case NodeType::ELEMENT_NODE:
		{
			if (tree.HasMember(L"children"))
			{
				for (auto& child : tree[L"children"].GetArray())
				{
					if (isDiffNode(child))
					{
						const int nodeId = child[L"nodeId"].GetInt();
						auto attributes = child[L"attributes"].GetArray();
						const wchar_t* buf = attributes[3].GetString();
						const int diffIndex = attributes.Size() > 3 ? _wtoi(attributes[3].GetString()) : -1;
						nodes.insert_or_assign(diffIndex, nodeId);
					}
					else
						getDiffNodes(doc, child, nodes);
				}
			}
			if (tree.HasMember(L"contentDocument") && tree[L"contentDocument"].HasMember(L"children"))
			{
				for (auto& child : tree[L"contentDocument"][L"children"].GetArray())
					getDiffNodes(doc, child, nodes);
			}
			break;
		}
		}
	}

	std::wstring getDiffStyleValue(COLORREF color, COLORREF backcolor) const
	{
		wchar_t styleValue[256];
		if (color == CLR_NONE)
			swprintf_s(styleValue, L"background-color: #%02x%02x%02x",
				GetRValue(backcolor), GetGValue(backcolor), GetBValue(backcolor));
		else
			swprintf_s(styleValue, L"color: #%02x%02x%02x; background-color: #%02x%02x%02x",
				GetRValue(color), GetGValue(color), GetBValue(color),
				GetRValue(backcolor), GetGValue(backcolor), GetBValue(backcolor));
		return styleValue;
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
							callback2->Invoke({ hr, nullptr });
					}
					if (FAILED(hr) && callback2)
						callback2->Invoke({ hr, nullptr });
					return S_OK;
				}).Get());
		return hr;
	}

	void highlightNodes(std::vector<DiffInfo>& diffInfoList, std::vector<WDocument>& documents)
	{
		std::wstring styleValue = getDiffStyleValue(m_colorSettings.clrDiffText, m_colorSettings.clrDiff);
		std::wstring styleSNPValue = getDiffStyleValue(m_colorSettings.clrSNPText, m_colorSettings.clrSNP);
		for (size_t i = 0; i < diffInfoList.size(); ++i)
		{
			const auto& diffInfo = diffInfoList[i];
			for (int pane = 0; pane < m_nPanes; ++pane)
			{
				WValue spanNode, attributes, children, id;
				auto& allocator = documents[pane].GetAllocator();
				id.SetString(std::to_wstring(i).c_str(), allocator);
				attributes.SetArray();
				attributes.PushBack(L"class", allocator);
				attributes.PushBack(L"wwd-diff", allocator);
				attributes.PushBack(L"data-wwdid", allocator);
				attributes.PushBack(id, allocator);
				attributes.PushBack(L"style", allocator);
				if ((pane == 0 && diffInfo.op == OP_3RDONLY) ||
				    (pane == 2 && diffInfo.op == OP_1STONLY))
					attributes.PushBack(WValue(styleSNPValue.c_str(), allocator), allocator);
				else
					attributes.PushBack(WValue(styleValue.c_str(), allocator), allocator);
				spanNode.SetObject();
				spanNode.AddMember(L"nodeName", L"SPAN", allocator);
				spanNode.AddMember(L"attributes", attributes, allocator);
				spanNode.AddMember(L"nodeType", 1, allocator);
				if (diffInfo.nodeIds[pane] >= 0)
				{
					auto [pvalue, pparent] = findNodeId(documents[pane][L"root"], diffInfo.nodeIds[pane]);
					if (pvalue)
					{
						WValue textNode;
						textNode.CopyFrom(*pvalue, allocator);
						textNode.RemoveMember(L"modified");
						const int nodeId = textNode[L"nodeId"].GetInt();
						children.SetArray();
						children.PushBack(textNode, allocator);
						spanNode.AddMember(L"children", children, allocator);
						spanNode.AddMember(L"nodeId", nodeId, allocator);
						spanNode.AddMember(L"modified", true, allocator);
						pvalue->CopyFrom(spanNode, allocator);
					}
				}
				else
				{
					auto [pvalue, pparent] = findNodeId(documents[pane][L"root"], -diffInfo.nodeIds[pane] - 1);
					if (pvalue)
					{
						spanNode.AddMember(L"nodeId", -1, allocator);
						children.SetArray();
						spanNode.AddMember(L"children", children, allocator);
						if (!pvalue->HasMember(L"insertedNodes"))
						{
							WValue insertedNodes;
							insertedNodes.SetArray();
							pvalue->AddMember(L"insertedNodes", insertedNodes, allocator);
						}
						(*pvalue)[L"insertedNodes"].GetArray().PushBack(spanNode, allocator);
						pvalue->AddMember(L"modified", true, allocator);
					}
				}
			}
		}
	}

	HRESULT applyHTMLLoop(
		int pane,
		std::shared_ptr<const std::list<ModifiedNode>> nodes,
		IWebDiffCallback* callback,
		std::list<ModifiedNode>::iterator it)
	{
		if (it == nodes->end())
		{
			if (callback)
				callback->Invoke({ S_OK, nullptr });
			return S_OK;
		}
		ComPtr<IWebDiffCallback> callback2(callback);
		HRESULT hr = m_webWindow[pane].SetOuterHTML(it->nodeId, it->outerHTML,
			Callback<IWebDiffCallback>([this, pane, nodes, it, callback2](const WebDiffCallbackResult& result) -> HRESULT
				{
					HRESULT hr = result.errorCode;
					if (SUCCEEDED(hr))
					{
						std::list<ModifiedNode>::iterator it2(it);
						++it2;
						if (it2 != nodes->end())
							hr = applyHTMLLoop(pane, nodes, callback2.Get(), it2);
						else
							callback2->Invoke({ hr, nullptr });
					}
					if (FAILED(hr) && callback2)
						callback2->Invoke({ hr, nullptr });
					return S_OK;
				}).Get());
		return hr;
	}

	HRESULT applyDOMLoop(std::shared_ptr<std::vector<WDocument>> documents, IWebDiffCallback* callback, int pane = 0)
	{
		ComPtr<IWebDiffCallback> callback2(callback);
		auto nodes = std::make_shared<std::list<ModifiedNode>>();
		modifiedNodesToHTMLs((*documents)[pane][L"root"], *nodes);
		HRESULT hr = applyHTMLLoop(pane, nodes,
			Callback<IWebDiffCallback>([this, documents, callback2, pane](const WebDiffCallbackResult& result) -> HRESULT
				{
					HRESULT hr = result.errorCode;
					if (SUCCEEDED(hr))
					{
						if (pane + 1 < m_nPanes)
							hr = applyDOMLoop(documents, callback2.Get(), pane + 1);
						else if (callback2)
							callback2->Invoke({ hr, nullptr });
					}
					if (FAILED(hr) && callback2)
						callback2->Invoke({ hr, nullptr });
					return hr;
				}).Get(), nodes->begin());
		return hr;
	}

	HRESULT highlightDocuments(std::shared_ptr<std::vector<WDocument>> documents, IWebDiffCallback* callback)
	{
		ComPtr<IWebDiffCallback> callback2(callback);
		applyDOMLoop(documents,
			Callback<IWebDiffCallback>([this, documents, callback2](const WebDiffCallbackResult& result) -> HRESULT
				{
					HRESULT hr = result.errorCode;
					if (SUCCEEDED(hr))
						hr = makeDiffNodeIdArrayLoop(callback2.Get());
					if (FAILED(hr) && callback2)
						callback2->Invoke({ hr, nullptr });
					return S_OK;
				}).Get());
		return S_OK;
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
						unhighlightNodes(doc, doc[L"root"]);
						auto nodes = std::make_shared<std::list<ModifiedNode>>();
						modifiedNodesToHTMLs(doc[L"root"], *nodes);
						hr = applyHTMLLoop(pane, nodes,
							Callback<IWebDiffCallback>([this, pane, callback2](const WebDiffCallbackResult& result) -> HRESULT
								{
									HRESULT hr = result.errorCode;
									if (SUCCEEDED(hr))
									{
										if (pane + 1 < m_nPanes)
											hr = unhighlightDifferencesLoop(callback2.Get(), pane + 1);
										else if (callback2)
											callback2->Invoke({ hr, nullptr });
									}
									if (FAILED(hr) && callback2)
										callback2->Invoke({ hr, nullptr });
									return S_OK;
								}).Get(), nodes->begin());
					}
					if (FAILED(hr) && callback2)
						callback2->Invoke({ hr, nullptr });
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
						std::map<int, int> nodes;
						getDiffNodes(doc, doc[L"root"], nodes);
						for (unsigned i = 0; i < m_diffInfos.size(); ++i)
						{
							if (m_diffInfos[i].nodeIds[pane] != -1)
								m_diffInfos[i].nodeIds[pane] = nodes[i];
						}
						if (pane < m_nPanes - 1)
							hr = makeDiffNodeIdArrayLoop(callback2.Get(), pane + 1);
						else if (callback2)
							callback2->Invoke({ hr, nullptr });
					}
					if (FAILED(hr) && callback2)
						callback2->Invoke({ hr, nullptr });
					return S_OK;
				}).Get());
		return hr;
	}

	bool selectDiff(int diffIndex, int prevDiffIndex)
	{
		if (diffIndex < 0 || diffIndex >= m_diffInfos.size())
			return false;
		std::wstring styleValue = getDiffStyleValue(m_colorSettings.clrDiffText, m_colorSettings.clrDiff);
		std::wstring styleSelValue = getDiffStyleValue(m_colorSettings.clrSelDiffText, m_colorSettings.clrSelDiff);
		std::wstring styleSNPValue = getDiffStyleValue(m_colorSettings.clrSNPText, m_colorSettings.clrSNP);
		std::wstring styleSelSNPValue = getDiffStyleValue(m_colorSettings.clrSelSNPText, m_colorSettings.clrSelSNP);
		for (int pane = 0; pane < m_nPanes; ++pane)
		{
			std::wstring args = L"{ \"nodeId\": " + std::to_wstring(m_diffInfos[diffIndex].nodeIds[pane]) + L" }";
			m_webWindow[pane].CallDevToolsProtocolMethod(L"DOM.scrollIntoViewIfNeeded", args.c_str(), nullptr);
			if (prevDiffIndex != -1 && prevDiffIndex < m_diffInfos.size())
			{
				bool snp = ((pane == 0 && m_diffInfos[prevDiffIndex].op == OP_3RDONLY) ||
					(pane == 2 && m_diffInfos[prevDiffIndex].op == OP_1STONLY));
				args = L"{ \"nodeId\": " + std::to_wstring(m_diffInfos[prevDiffIndex].nodeIds[pane])
				     + L", \"name\": \"style\", \"value\": \"" + (snp ? styleSNPValue : styleValue) + L"\" }";
				m_webWindow[pane].CallDevToolsProtocolMethod(L"DOM.setAttributeValue", args.c_str(), nullptr);
			}
			if (diffIndex < m_diffInfos.size())
			{
				bool snp = ((pane == 0 && m_diffInfos[diffIndex].op == OP_3RDONLY) ||
					(pane == 2 && m_diffInfos[diffIndex].op == OP_1STONLY));
				args = L"{ \"nodeId\": " + std::to_wstring(m_diffInfos[diffIndex].nodeIds[pane])
				     + L", \"name\": \"style\", \"value\": \"" + (snp ? styleSelSNPValue : styleSelValue) + L"\" }";
				m_webWindow[pane].CallDevToolsProtocolMethod(L"DOM.setAttributeValue", args.c_str(), nullptr);
			}
		}
		return true;
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
	DiffOptions m_diffOptions{};
	bool m_bShowDifferences = true;
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
};
