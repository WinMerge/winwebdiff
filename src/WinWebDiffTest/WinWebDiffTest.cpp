#include "pch.h"
#include "CppUnitTest.h"
#include <Windows.h>
#include "../WinWebDiffLib/DiffHighlighter.hpp"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace WinWebDiffTest
{
const wchar_t* json1 = LR"(
{
    "root": {
        "backendNodeId": 2,
        "baseURL": "file:///C:/tmp/test1.html",
        "childNodeCount": 2,
        "children": [
            {
                "backendNodeId": 4,
                "localName": "",
                "nodeId": 2,
                "nodeName": "html",
                "nodeType": 10,
                "nodeValue": "",
                "parentId": 1,
                "publicId": "",
                "systemId": ""
            },
            {
                "attributes": [],
                "backendNodeId": 3,
                "childNodeCount": 2,
                "children": [
                    {
                        "attributes": [],
                        "backendNodeId": 5,
                        "childNodeCount": 0,
                        "children": [],
                        "localName": "head",
                        "nodeId": 4,
                        "nodeName": "HEAD",
                        "nodeType": 1,
                        "nodeValue": "",
                        "parentId": 3
                    },
                    {
                        "attributes": [],
                        "backendNodeId": 6,
                        "childNodeCount": 1,
                        "children": [
                            {
                                "attributes": [],
                                "backendNodeId": 7,
                                "childNodeCount": 1,
                                "children": [
                                    {
                                        "backendNodeId": 8,
                                        "localName": "",
                                        "nodeId": 7,
                                        "nodeName": "#text",
                                        "nodeType": 3,
                                        "nodeValue": "ab",
                                        "parentId": 6
                                    }
                                ],
                                "localName": "span",
                                "nodeId": 6,
                                "nodeName": "SPAN",
                                "nodeType": 1,
                                "nodeValue": "",
                                "parentId": 5
                            }
                        ],
                        "localName": "body",
                        "nodeId": 5,
                        "nodeName": "BODY",
                        "nodeType": 1,
                        "nodeValue": "",
                        "parentId": 3
                    }
                ],
                "frameId": "D3F2CA8045992D833A965DD36B9FA279",
                "localName": "html",
                "nodeId": 3,
                "nodeName": "HTML",
                "nodeType": 1,
                "nodeValue": "",
                "parentId": 1
            }
        ],
        "compatibilityMode": "NoQuirksMode",
        "documentURL": "file:///C:/tmp/test1.html",
        "localName": "",
        "nodeId": 1,
        "nodeName": "#document",
        "nodeType": 9,
        "nodeValue": "",
        "xmlVersion": ""
    }
}
)";

const wchar_t* json2 = LR"(
{
    "root": {
        "backendNodeId": 2,
        "baseURL": "file:///C:/tmp/test2.html",
        "childNodeCount": 2,
        "children": [
            {
                "backendNodeId": 3,
                "localName": "",
                "nodeId": 2,
                "nodeName": "html",
                "nodeType": 10,
                "nodeValue": "",
                "parentId": 1,
                "publicId": "",
                "systemId": ""
            },
            {
                "attributes": [],
                "backendNodeId": 4,
                "childNodeCount": 2,
                "children": [
                    {
                        "attributes": [],
                        "backendNodeId": 5,
                        "childNodeCount": 0,
                        "children": [],
                        "localName": "head",
                        "nodeId": 4,
                        "nodeName": "HEAD",
                        "nodeType": 1,
                        "nodeValue": "",
                        "parentId": 3
                    },
                    {
                        "attributes": [],
                        "backendNodeId": 6,
                        "childNodeCount": 1,
                        "children": [
                            {
                                "attributes": [],
                                "backendNodeId": 7,
                                "childNodeCount": 3,
                                "children": [
                                    {
                                        "attributes": [],
                                        "backendNodeId": 8,
                                        "childNodeCount": 1,
                                        "children": [
                                            {
                                                "backendNodeId": 9,
                                                "localName": "",
                                                "nodeId": 8,
                                                "nodeName": "#text",
                                                "nodeType": 3,
                                                "nodeValue": "01",
                                                "parentId": 7
                                            }
                                        ],
                                        "localName": "span",
                                        "nodeId": 7,
                                        "nodeName": "SPAN",
                                        "nodeType": 1,
                                        "nodeValue": "",
                                        "parentId": 6
                                    },
                                    {
                                        "backendNodeId": 10,
                                        "localName": "",
                                        "nodeId": 9,
                                        "nodeName": "#text",
                                        "nodeType": 3,
                                        "nodeValue": "ab",
                                        "parentId": 6
                                    },
                                    {
                                        "attributes": [],
                                        "backendNodeId": 11,
                                        "childNodeCount": 1,
                                        "children": [
                                            {
                                                "backendNodeId": 12,
                                                "localName": "",
                                                "nodeId": 11,
                                                "nodeName": "#text",
                                                "nodeType": 3,
                                                "nodeValue": "cd",
                                                "parentId": 10
                                            }
                                        ],
                                        "localName": "span",
                                        "nodeId": 10,
                                        "nodeName": "SPAN",
                                        "nodeType": 1,
                                        "nodeValue": "",
                                        "parentId": 6
                                    }
                                ],
                                "localName": "span",
                                "nodeId": 6,
                                "nodeName": "SPAN",
                                "nodeType": 1,
                                "nodeValue": "",
                                "parentId": 5
                            }
                        ],
                        "localName": "body",
                        "nodeId": 5,
                        "nodeName": "BODY",
                        "nodeType": 1,
                        "nodeValue": "",
                        "parentId": 3
                    }
                ],
                "frameId": "B85A70A8A7153996F3C4849515CA3EF7",
                "localName": "html",
                "nodeId": 3,
                "nodeName": "HTML",
                "nodeType": 1,
                "nodeValue": "",
                "parentId": 1
            }
        ],
        "compatibilityMode": "NoQuirksMode",
        "documentURL": "file:///C:/tmp/test2.html",
        "localName": "",
        "nodeId": 1,
        "nodeName": "#document",
        "nodeType": 9,
        "nodeValue": "",
        "xmlVersion": ""
    }
}
)";

	TEST_CLASS(WinWebDiffTest)
	{
	public:
		
		TEST_METHOD(TestMethod1)
		{
            std::vector<WDocument> documents(2);
			std::vector<TextBlocks> textBlocks(2);
            IWebDiffWindow::DiffOptions diffOptions{};
            IWebDiffWindow::ColorSettings colorSettings{};
            documents[0].Parse(json1);
            documents[1].Parse(json2);
			textBlocks[0].Make(documents[0][L"root"]);
			textBlocks[1].Make(documents[1][L"root"]);
			std::vector<DiffInfo> diffInfos = Comparer::compare(diffOptions, textBlocks);
			Comparer::setNodeIdInDiffInfoList(diffInfos, textBlocks);
			Highlighter highlighter(documents, diffInfos, colorSettings, diffOptions, true, 0);
			highlighter.highlightNodes();
		}
	};
}
