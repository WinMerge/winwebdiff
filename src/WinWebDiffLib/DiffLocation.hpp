#include "WinWebDiffLib.h"
#include <vector>
#include <map>
#include <string>
#include <rapidjson/document.h>

using WDocument = rapidjson::GenericDocument<rapidjson::UTF16<>>;
using WValue = rapidjson::GenericValue<rapidjson::UTF16<>>;

class DiffLocation
{
public:
	void read(int pane, const WDocument& doc)
	{
		std::wstring window = doc[L"window"].GetString();
		std::vector<IWebDiffWindow::ContainerRect> containerRects;
		std::vector<IWebDiffWindow::DiffRect> diffRects;
		for (const auto& value : doc[L"diffRects"].GetArray())
		{
			IWebDiffWindow::DiffRect rect;
			rect.id = value[L"id"].GetInt();
			rect.containerId = value[L"containerId"].GetInt();
			rect.left = value[L"left"].GetFloat();
			rect.top = value[L"top"].GetFloat();
			rect.width = value[L"width"].GetFloat();
			rect.height = value[L"height"].GetFloat();
			diffRects.push_back(rect);
		}
		for (const auto& value : doc[L"containerRects"].GetArray())
		{
			IWebDiffWindow::ContainerRect rect;
			rect.id = value[L"id"].GetInt();
			rect.containerId = value[L"containerId"].GetInt();
			rect.left = value[L"left"].GetFloat();
			rect.top = value[L"top"].GetFloat();
			rect.width = value[L"width"].GetFloat();
			rect.height = value[L"height"].GetFloat();
			rect.scrollLeft = value[L"scrollLeft"].GetFloat();
			rect.scrollTop = value[L"scrollTop"].GetFloat();
			rect.scrollWidth = value[L"scrollWidth"].GetFloat();
			rect.scrollHeight = value[L"scrollHeight"].GetFloat();
			rect.clientWidth = value[L"clientWidth"].GetFloat();
			rect.clientHeight = value[L"clientHeight"].GetFloat();
			containerRects.push_back(rect);
		}
		m_diffRects[pane].insert_or_assign(window, diffRects);
		m_containerRects[pane].insert_or_assign(window, containerRects);
		if (window == L"")
		{
			m_scrollX[pane] = doc[L"scrollX"].GetFloat();
			m_scrollY[pane] = doc[L"scrollY"].GetFloat();
			m_innerWidth[pane] = doc[L"innerWidth"].GetFloat();
			m_innerHeight[pane] = doc[L"innerHeight"].GetFloat();
		}
	}

	void clear()
	{
		for (int pane = 0; pane < 3; ++pane)
		{
			m_diffRects[pane].clear();
			m_containerRects[pane].clear();
			m_diffRectsSerialized[pane].clear();
			m_containerRectsSerialized[pane].clear();
			m_scrollX[pane] = 0.0f;
			m_scrollY[pane] = 0.0f;
			m_innerWidth[pane] = 0.0f;
			m_innerHeight[pane] = 0.0f;
		}
	}

	IWebDiffWindow::DiffRect* GetDiffRectArray(int pane, int& count)
	{
		m_diffRectsSerialized[pane].clear();
		for (const auto& pair: m_diffRects[pane])
		{
			const auto& key = pair.first;
			for (const auto& diffRect : pair.second)
			{
				IWebDiffWindow::DiffRect rect;
				rect.id = diffRect.id;
				rect.containerId = diffRect.containerId;
				rect.left = diffRect.left + m_scrollX[pane];
				rect.top = diffRect.top + m_scrollY[pane]; 
				rect.width = diffRect.width;
				rect.height = diffRect.height;
				m_diffRectsSerialized[pane].push_back(rect);
			}
		}
		count = static_cast<int>(m_diffRectsSerialized[pane].size());
		return m_diffRectsSerialized[pane].data();
	}

	IWebDiffWindow::ContainerRect* GetContainerRectArray(int pane, int& count)
	{
		m_containerRectsSerialized[pane].clear();
		for (const auto& pair: m_containerRects[pane])
		{
			const auto& key = pair.first;
			for (const auto& containerRect : pair.second)
			{
				IWebDiffWindow::ContainerRect rect;
				rect.id = containerRect.id;
				rect.containerId = containerRect.containerId;
				rect.left = containerRect.left + m_scrollX[pane];
				rect.top = containerRect.top + m_scrollY[pane];
				rect.width = containerRect.width;
				rect.height = containerRect.height;
				rect.scrollLeft = containerRect.scrollLeft;
				rect.scrollTop = containerRect.scrollTop;
				rect.scrollWidth = containerRect.scrollWidth;
				rect.scrollHeight = containerRect.scrollHeight;
				rect.clientWidth = containerRect.clientWidth;
				rect.clientHeight = containerRect.clientHeight;
				m_containerRectsSerialized[pane].push_back(rect);
			}
		}
		count = static_cast<int>(m_containerRectsSerialized[pane].size());
		return m_containerRectsSerialized[pane].data();
	}

	IWebDiffWindow::Rect GetVisibleAreaRect(int pane)
	{
		return { m_scrollX[pane], m_scrollY[pane], m_innerWidth[pane], m_innerHeight[pane] };
	}

	std::map<std::wstring, std::vector<IWebDiffWindow::DiffRect>> m_diffRects[3];
	std::map<std::wstring, std::vector<IWebDiffWindow::ContainerRect>> m_containerRects[3];
	std::vector<IWebDiffWindow::DiffRect> m_diffRectsSerialized[3];
	std::vector<IWebDiffWindow::ContainerRect> m_containerRectsSerialized[3];
	float m_scrollX[3] = { 0.0f, 0.0f, 0.0f };
	float m_scrollY[3] = { 0.0f, 0.0f, 0.0f };
	float m_innerWidth[3] = { 0.0f, 0.0f, 0.0f };
	float m_innerHeight[3] = { 0.0f, 0.0f, 0.0f };
};
