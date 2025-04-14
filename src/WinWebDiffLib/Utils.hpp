#pragma once

#include <vector>
#include <cstdlib>
#include <string>
#include <windows.h>

namespace utils
{
	int cmp(const void* a, const void* b)
	{
		const wchar_t* const* pa = reinterpret_cast<const wchar_t* const*>(a);
		const wchar_t* const* pb = reinterpret_cast<const wchar_t* const*>(b);
		return wcscmp(*pa, *pb);
	}

	bool IsVoidElement(const wchar_t* name)
	{
		static const wchar_t* voidElements[] =
		{
			L"AREA",
			L"BASE",
			L"BR",
			L"COL",
			L"EMBED",
			L"HR",
			L"IMG",
			L"INPUT",
			L"LINK",
			L"META",
			L"SOURCE",
			L"TRACK",
			L"WBR",
		};
		return bsearch(&name, voidElements,
			sizeof(voidElements) / sizeof(voidElements[0]),
			sizeof(voidElements[0]), cmp);
	}

	bool IsInlineElement(const wchar_t* name)
	{
		static const wchar_t* inlineElements[] =
		{
			L"A",
			L"ABBR",
			L"ACRONYM",
			L"AUDIO",
			L"B",
			L"BDI",
			L"BDO",
			L"BIG",
			L"BR",
			L"BUTTON",
			L"CANVAS",
			L"CITE",
			L"CODE",
			L"DATA",
			L"DATALIST",
			L"DEL",
			L"DFN",
			L"EM",
			L"EMBED",
			L"I",
			L"IFRAME",
			L"IMG",
			L"INPUT",
			L"INS",
			L"KBD",
			L"LABEL",
			L"MAP",
			L"MARK",
			L"METER",
			L"NOSCRIPT",
			L"OBJECT",
			L"OUTPUT",
			L"PICTURE",
			L"PROGRESS",
			L"Q",
			L"RUBY",
			L"S",
			L"SAMP",
			L"SCRIPT",
			L"SELECT",
			L"SLOT",
			L"SMALL",
			L"SPAN",
			L"STRONG",
			L"SUB",
			L"SUP",
			L"SVG",
			L"TEMPLATE",
			L"TEXTAREA",
			L"TIME",
			L"TT",
			L"U",
			L"VAR",
			L"VIDEO",
			L"WBR",
		};
		return bsearch(&name, inlineElements,
			sizeof(inlineElements) / sizeof(inlineElements[0]),
			sizeof(inlineElements[0]), cmp);
	}

	std::wstring trim_ws(const std::wstring& str)
	{
		if (str.empty())
			return str;

		std::wstring result(str);
		std::wstring::iterator it = result.begin();
		while (it != result.end() && *it < 0x100 && isspace(*it))
			++it;

		if (it != result.begin())
			result.erase(result.begin(), it);

		if (result.empty())
			return result;

		it = result.end() - 1;
		while (it != result.begin() && *it < 0x100 && iswspace(*it))
			--it;

		if (it != result.end() - 1)
			result.erase(it + 1, result.end());
		return result;
	}

	std::wstring EncodeHTMLEntities(const std::wstring& text)
	{
		std::wstring result;
		for (auto c : text)
		{
			switch (c)
			{
			case '<':  result += L"&lt;"; break;
			case '>':  result += L"&gt;"; break;
			case '"':  result += L"&quot;"; break;
			default:   result += c; break;
			}
		}
		return result;
	}

	std::wstring Escape(const std::wstring& text)
	{
		std::wstring result;
		for (auto c : text)
		{
			switch (c)
			{
			case '*':  result += L"%2A"; break;
			case '?':  result += L"%3F"; break;
			case ':':  result += L"%3A"; break;
			case '/':  result += L"%2F"; break;
			case '\\': result += L"%5C"; break;
			default:   result += c; break;
			}

		}
		return result;
	}

	std::wstring Quote(const std::wstring& text)
	{
		std::wstring ret;
		ret += L"\"";
		for (auto c : text)
		{
			switch (c)
			{
			case '\a': ret += L"\\a"; break;
			case '\b': ret += L"\\b"; break;
			case '\f': ret += L"\\f"; break;
			case '\r': break;
			case '\n': ret += L"\\n"; break;
			case '\t': ret += L"\\t"; break;
			case '\v': ret += L"\\v"; break;
			case '\"': ret += L"\\\""; break;
			case '\\': ret += L"\\\\"; break;
			default:
				if (c < 0x20 || c == 0x7F)
				{
					wchar_t buf[5];
					swprintf(buf, 5, L"\\x%02X", c);
					ret += buf;
				}
				else
				{
					ret += c;
				}
				break;
			}
		}
		ret += L"\"";
		return ret;
	}

	std::vector<BYTE> DecodeBase64(const std::wstring& base64)
	{
		std::vector<BYTE> data;
		DWORD cbBinary = 0;
		if (CryptStringToBinary(base64.c_str(), static_cast<DWORD>(base64.size()), CRYPT_STRING_BASE64_ANY, nullptr, &cbBinary, nullptr, nullptr))
		{
			data.resize(cbBinary);
			CryptStringToBinary(base64.c_str(), static_cast<DWORD>(base64.size()), CRYPT_STRING_BASE64_ANY, data.data(), &cbBinary, nullptr, nullptr);
		}
		return data;
	}
}
