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
			containerRects.push_back(rect);
		}
		m_diffRects[pane].insert_or_assign(window, diffRects);
		m_containerRects[pane].insert_or_assign(window, containerRects);
	}

	void clear()
	{
		for (int pane = 0; pane < 3; ++pane)
		{
			m_diffRects[pane].clear();
			m_containerRects[pane].clear();
			m_diffRectsSerialized[pane].clear();
		}
	}

	IWebDiffWindow::DiffRect* GetDiffRectArray(int pane, int& count)
	{
		count = static_cast<int>(m_diffRectsSerialized[pane].size());
		return m_diffRectsSerialized[pane].data();
	}

	std::map<std::wstring, std::vector<IWebDiffWindow::DiffRect>> m_diffRects[3];
	std::map<std::wstring, std::vector<IWebDiffWindow::ContainerRect>> m_containerRects[3];
	std::vector<IWebDiffWindow::DiffRect> m_diffRectsSerialized[3];
};
