#include <vector>
#include <map>
#include <string>
#include <rapidjson/document.h>

using WDocument = rapidjson::GenericDocument<rapidjson::UTF16<>>;
using WValue = rapidjson::GenericValue<rapidjson::UTF16<>>;

class DiffLocation
{
public:
	struct Rect
	{
		float left;
		float top;
		float width;
		float height;
	};
	struct ContainerRect : public Rect
	{
		int id;
		int containerId;
		float scrollLeft;
		float scrollTop;
		float scrollWidth;
		float scrollHeight;
		float clientWidth;
		float clientHeight;
	};
	struct DiffRect : public Rect
	{
		int id;
		int containerId;
	};

	void read(const WDocument& doc)
	{
		std::wstring window = doc[L"window"].GetString();
		std::vector<ContainerRect> containerRects;
		std::vector<DiffRect> diffRects;
		for (const auto& value : doc[L"diffRects"].GetArray())
		{
			DiffRect rect;
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
			ContainerRect rect;
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
		for (auto it = doc[L"frameRects"].MemberBegin(); it != doc[L"frameRects"].MemberEnd(); ++it)
		{
			Rect rect;
			rect.left = it->value[L"left"].GetFloat();
			rect.top = it->value[L"top"].GetFloat();
			rect.width = it->value[L"width"].GetFloat();
			rect.height = it->value[L"height"].GetFloat();
			m_frameRects.insert_or_assign(it->name.GetString(), rect);
		}
		m_diffRects.insert_or_assign(window, diffRects);
		m_containerRects.insert_or_assign(window, containerRects);
		if (window == L"")
		{
			m_scrollX = doc[L"scrollX"].GetFloat();
			m_scrollY = doc[L"scrollY"].GetFloat();
			m_clientWidth = doc[L"clientWidth"].GetFloat();
			m_clientHeight = doc[L"clientHeight"].GetFloat();
		}
	}

	void clear()
	{
		m_diffRects.clear();
		m_containerRects.clear();
		m_scrollX = 0.0f;
		m_scrollY = 0.0f;
		m_clientWidth = 0.0f;
		m_clientHeight = 0.0f;
	}

	void calcGlobalPosition(Rect& rect, std::wstring window)
	{
		while (!window.empty())
		{
			rect.left += m_frameRects[window].left;
			rect.top += m_frameRects[window].top;
			size_t lastOpeningBracketPos = window.find_last_of('[');
			if (lastOpeningBracketPos != std::string::npos) {
				window = window.substr(0, lastOpeningBracketPos);
			}
		}
	}

	std::vector<DiffRect> getDiffRectArray()
	{
		std::vector<DiffRect> diffRectsSerialized;
		for (const auto& pair: m_diffRects)
		{
			const auto& window = pair.first;
			for (const auto& diffRect : pair.second)
			{
				DiffRect rect;
				rect.id = diffRect.id;
				rect.containerId = diffRect.containerId;
				rect.left = diffRect.left;
				rect.top = diffRect.top; 
				rect.width = diffRect.width;
				rect.height = diffRect.height;
				for (int containerId = diffRect.containerId; containerId > 0; )
				{
					const ContainerRect& containerRect = m_containerRects[window][containerId];
					if (containerRect.id == 0 && (containerRect.width == 0 || containerRect.height == 0))
						break;
					clip(rect, containerRect);
					containerId = containerRect.containerId;
				}
				if (rect.left != -99999.9f || rect.top != -99999.9f)
				{
					rect.left += m_scrollX;
					rect.top += m_scrollY;
				}
				if (!window.empty())
				{
					calcGlobalPosition(rect, window);
					Rect rcFrame = m_frameRects[window];
					rcFrame.left += m_scrollX;
					rcFrame.top += m_scrollY;
					clip(rect, rcFrame);
				}
				diffRectsSerialized.push_back(rect);
			}
		}
		return diffRectsSerialized;
	}

	std::vector<ContainerRect> getContainerRectArray()
	{
		std::vector<ContainerRect> containerRectsSerialized;
		for (const auto& pair: m_containerRects)
		{
			const auto& window = pair.first;
			for (const auto& containerRect : pair.second)
			{
				ContainerRect rect;
				rect.id = containerRect.id;
				rect.containerId = containerRect.containerId;
				rect.left = containerRect.left + m_scrollX;
				rect.top = containerRect.top + m_scrollY;
				rect.width = containerRect.width;
				rect.height = containerRect.height;
				rect.scrollLeft = containerRect.scrollLeft;
				rect.scrollTop = containerRect.scrollTop;
				rect.scrollWidth = containerRect.scrollWidth;
				rect.scrollHeight = containerRect.scrollHeight;
				rect.clientWidth = containerRect.clientWidth;
				rect.clientHeight = containerRect.clientHeight;
				if (!window.empty())
				{
					calcGlobalPosition(rect, window);
					Rect rcFrame = m_frameRects[window];
					rcFrame.left += m_scrollX;
					rcFrame.top += m_scrollY;
					clip(rect, rcFrame);
				}
				containerRectsSerialized.push_back(rect);
			}
		}
		return containerRectsSerialized;
	}

	Rect getVisibleAreaRect()
	{
		return { m_scrollX, m_scrollY, m_clientWidth, m_clientHeight };
	}

private:
	bool clip(Rect& rect, const Rect& containerRect)
	{
		if (rect.left + rect.width < containerRect.left ||
			rect.top + rect.height < containerRect.top ||
			rect.left > containerRect.left + containerRect.width ||
			rect.top > containerRect.top + containerRect.height)
		{
			rect.left = rect.top = -99999.9f;
			rect.width = rect.height = 0.0f;
			return true;
		}
		if (rect.left < containerRect.left) {
			rect.width -= containerRect.left - rect.left;
			rect.left = containerRect.left;
		}
		if (rect.top < containerRect.top) {
			rect.height -= containerRect.top - rect.top;
			rect.top = containerRect.top;
		}
		if (rect.left + rect.width > containerRect.left + containerRect.width) {
			rect.width = containerRect.left + containerRect.width - rect.left;
		}
		if (rect.top + rect.height > containerRect.top + containerRect.height) {
			rect.height = containerRect.top + containerRect.height - rect.top;
		}
		return false;
	}

	std::map<std::wstring, std::vector<DiffRect>> m_diffRects;
	std::map<std::wstring, std::vector<ContainerRect>> m_containerRects;
	std::map<std::wstring, Rect> m_frameRects;
	float m_scrollX = 0.0f;
	float m_scrollY = 0.0f;
	float m_clientWidth = 0.0f;
	float m_clientHeight = 0.0f;
};
