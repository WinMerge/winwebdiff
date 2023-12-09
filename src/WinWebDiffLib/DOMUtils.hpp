#pragma once

#if defined(_M_ARM64) && !defined(RAPIDJSON_ENDIAN)
#define  RAPIDJSON_ENDIAN RAPIDJSON_LITTLEENDIAN
#endif
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>
#include <string>
#include <unordered_map>

using WDocument = rapidjson::GenericDocument<rapidjson::UTF16<>>;
using WValue = rapidjson::GenericValue<rapidjson::UTF16<>>;

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

namespace domutils
{
	bool containsClassName(const WValue& value, const wchar_t* name)
	{
		if (value[L"nodeType"].GetInt() != NodeType::ELEMENT_NODE)
			return false;
		if (!value.HasMember(L"attributes"))
			return false;
		const auto& ary = value[L"attributes"].GetArray();
		for (unsigned int i = 0; i + 1 < ary.Size(); i += 2)
		{
			if (wcscmp(ary[i].GetString(), L"class") == 0 && 
			    wcsstr(ary[i + 1].GetString(), name) != nullptr)
				return true;
		}
		return false;
	}

	const wchar_t* getAttribute(const WValue& node, const wchar_t* name)
	{
		if (!node.HasMember(L"attributes"))
			return nullptr;
		const auto& ary = node[L"attributes"].GetArray();
		for (unsigned i = 0; i < ary.Size(); i += 2)
		{
			if (wcscmp(ary[i].GetString(), name) == 0 && i + 1 < ary.Size())
				return ary[i + 1].GetString();
		}
		return nullptr;
	}

	void setAttribute(WValue& node, const wchar_t* name, const std::wstring& value, WDocument::AllocatorType& allocator)
	{
		if (!node.HasMember(L"attributes"))
			return;
		const auto& ary = node[L"attributes"].GetArray();
		for (unsigned i = 0; i < ary.Size(); i += 2)
		{
			if (wcscmp(ary[i].GetString(), name) == 0 && i + 1 < ary.Size())
			{
				ary[i + 1].SetString(value.c_str(), static_cast<unsigned>(value.length()), allocator);
				break;
			}
		}
	}

	void makeTextNode(WValue& textNode, const std::wstring& text, WDocument::AllocatorType& allocator)
	{
		WValue children;
		WValue textValue(text.c_str(), static_cast<unsigned>(text.size()), allocator);
		children.SetArray();
		textNode.SetObject();
		textNode.AddMember(L"nodeId", -1, allocator);
		textNode.AddMember(L"nodeType", 3, allocator);
		textNode.AddMember(L"nodeValue", textValue, allocator);
		textNode.AddMember(L"children", children, allocator);
	}

	void getFrameIdList(WValue& tree, std::vector<std::wstring>& frameIdList)
	{
		frameIdList.push_back(tree[L"frame"][L"id"].GetString());
		if (tree.HasMember(L"childFrames"))
		{
			for (auto& frame : tree[L"childFrames"].GetArray())
				getFrameIdList(frame, frameIdList);
		}
	}

	void makeNodeIdToNodeMap(WValue& nodeTree, std::unordered_map<int, WValue*>& map)
	{
		map.insert_or_assign(nodeTree[L"nodeId"].GetInt(), &nodeTree);
		if (nodeTree.HasMember(L"children") && nodeTree[L"children"].IsArray())
		{
			for (auto& child : nodeTree[L"children"].GetArray())
			{
				map.insert_or_assign(child[L"nodeId"].GetInt(), &child);
				makeNodeIdToNodeMap(child, map);
			}
		}
		if (nodeTree.HasMember(L"contentDocument"))
		{
			auto& contentDocument = nodeTree[L"contentDocument"];
			map.insert_or_assign(contentDocument[L"nodeId"].GetInt(), &contentDocument);
			makeNodeIdToNodeMap(contentDocument, map);
		}
	}

	std::unordered_map<int, WValue*> makeNodeIdToNodeMap(WValue& nodeTree)
	{
		std::unordered_map<int, WValue*> map;
		makeNodeIdToNodeMap(nodeTree, map);
		return map;
	}

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
					return { pvalue, pparent ? pparent : &nodeTree };
			}
		}
		if (nodeTree.HasMember(L"contentDocument"))
		{
			auto [pvalue, pparent] = findNodeId(nodeTree[L"contentDocument"], nodeId);
			return { pvalue, pparent ? pparent : &nodeTree[L"contentDocument"] };
		}
		return { nullptr, nullptr };
	}
}
