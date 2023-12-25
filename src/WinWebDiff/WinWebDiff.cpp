#include "framework.h"
#include "WinWebDiff.h"
#include "../WinWebDiffLib/WinWebDiffLib.h"
#include <combaseapi.h>
#include <shellapi.h>
#include <CommCtrl.h>
#include <commdlg.h>
#include <Shlwapi.h>
#include <string>
#include <vector>
#include <list>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <wrl.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shlwapi.lib")

#if defined _M_IX86
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_IA64
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='ia64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif

using namespace Microsoft::WRL;

class TempFile
{
public:
	~TempFile()
	{
		Delete();
	}

	std::wstring Create(const wchar_t *ext)
	{
		wchar_t buf[MAX_PATH];
		GetTempFileName(std::filesystem::temp_directory_path().wstring().c_str(), L"WD", 0, buf);
		std::wstring temp = buf;
		if (temp.empty())
			return _T("");
		if (ext && !std::wstring(ext).empty())
		{
			std::wstring newfile = temp + ext;
			std::filesystem::rename(temp, newfile);
			temp = newfile;
		}
		m_path = temp;
		return temp;
	}

	bool Delete()
	{
		bool success = true;
		if (!m_path.empty())
			success = !!DeleteFile(m_path.c_str());
		if (success)
			m_path = _T("");
		return success;
	}

	std::wstring m_path;
};

class TempFolder
{
public:
	~TempFolder()
	{
		Delete();
	}

	std::wstring Create()
	{
		TempFile tmpfile;
		std::wstring temp = tmpfile.Create(nullptr);
		if (temp.empty())
			return L"";
		temp += L".dir";
		if (!std::filesystem::create_directory(temp))
			return L"";
		m_path = temp;
		return temp;
	}

	bool Delete()
	{
		bool success = true;
		if (!m_path.empty())
			success = std::filesystem::remove_all(m_path);
		if (success)
			m_path = _T("");
		return success;
	}

	std::wstring m_path;
};

struct CmdLineInfo
{
	explicit CmdLineInfo(const wchar_t* cmdline) : nUrls(0)
	{
		if (cmdline[0] == 0)
			return;

		int argc;
		wchar_t** argv = CommandLineToArgvW(cmdline, &argc);
		for (int i = 0; i < argc; ++i)
		{
			if (argv[i][0] != '-' && argv[i][0] != '/' && nUrls < 3)
			{
				sUrls[nUrls] = argv[i];
				++nUrls;
			}
		}
	}

	std::wstring sUrls[3];
	int nUrls;
};

static HRESULT OnWebDiffEvent(const WebDiffEvent& e);

struct CallbackImpl: public IWebDiffEventHandler
{
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override { return E_NOTIMPL; }
	ULONG STDMETHODCALLTYPE AddRef(void) override { return ++m_nRef; }
	ULONG STDMETHODCALLTYPE Release(void) override { if (--m_nRef == 0) { delete this; return 0; } return m_nRef; }
	HRESULT STDMETHODCALLTYPE Invoke(const WebDiffEvent& event) { return OnWebDiffEvent(event); }
	int m_nRef = 0;
};

HINSTANCE m_hInstance;
HINSTANCE hInstDLL;
HWND m_hWnd;
HWND m_hwndWebToolWindow;
WCHAR m_szTitle[256] = L"WinWebDiff";
WCHAR m_szWindowClass[256] = L"WINWEBDIFF";
IWebDiffWindow* m_pWebDiffWindow = nullptr;
IWebToolWindow *m_pWebToolWindow = nullptr;
std::list<TempFile> m_tempFiles;
std::list<TempFolder> m_tempFolders;
CallbackImpl m_callback;

ATOM				MyRegisterClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK	About(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
					 _In_opt_ HINSTANCE hPrevInstance,
					 _In_ LPWSTR	lpCmdLine,
					 _In_ int	   nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	InitCommonControls();
	
	HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

	MyRegisterClass(hInstance);
	hInstDLL = GetModuleHandleW(L"WinWebDiffLib.dll");

	if (!InitInstance (hInstance, nCmdShow))
	{
		return FALSE;
	}

	HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_WINWEBDIFF));

	CmdLineInfo cmdline(lpCmdLine);

	if (!m_pWebDiffWindow->IsWebView2Installed())
	{
		if (IDYES == MessageBox(nullptr, L"WebView2 runtime is not installed. Do you want to download it?", L"WinWebDiff", MB_YESNO))
		{
			m_pWebDiffWindow->DownloadWebView2();
		}
	}
	else
	{
		if (cmdline.nUrls > 0)
		{
			if (cmdline.nUrls <= 2)
				m_pWebDiffWindow->Open(cmdline.sUrls[0].c_str(), cmdline.sUrls[1].c_str(), nullptr);
			else
				m_pWebDiffWindow->Open(cmdline.sUrls[0].c_str(), cmdline.sUrls[1].c_str(), cmdline.sUrls[2].c_str(), nullptr);
		}
		else
			m_pWebDiffWindow->New(2, nullptr);
	}

	MSG msg;
	while (GetMessage(&msg, nullptr, 0, 0))
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg) && m_hwndWebToolWindow == 0 || !IsDialogMessage(m_hwndWebToolWindow, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	if (SUCCEEDED(hr))
		CoUninitialize();

	return (int) msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEXW wcex{ sizeof(WNDCLASSEX) };

	wcex.style          = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc    = WndProc;
	wcex.cbClsExtra     = 0;
	wcex.cbWndExtra     = 0;
	wcex.hInstance      = hInstance;
	wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_WINWEBDIFF));
	wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_WINWEBDIFF);
	wcex.lpszClassName  = m_szWindowClass;
	wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassExW(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   m_hInstance = hInstance; // Store instance handle in our global variable

   m_hWnd = CreateWindowW(m_szWindowClass, m_szTitle, WS_OVERLAPPEDWINDOW,
	  CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

   if (!m_hWnd)
   {
	  return FALSE;
   }

   ShowWindow(m_hWnd, nCmdShow);
   UpdateWindow(m_hWnd);

   return TRUE;
}

std::wstring GetLastErrorString()
{
	wchar_t buf[256];
	FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		buf, (sizeof(buf) / sizeof(wchar_t)), NULL);
	return buf;
}

bool CompareFiles(const std::vector<std::wstring>& filenames, const std::wstring& options)
{
	std::wstring cmdline = L"\"C:\\Program Files\\WinMerge\\WinMergeU.exe\" /ub ";
	for (const auto& filename : filenames)
		cmdline += L"\"" + filename + L"\" ";
	if (!options.empty())
		cmdline += L" " + options;
	STARTUPINFO stInfo = { sizeof(STARTUPINFO) };
	stInfo.dwFlags = STARTF_USESHOWWINDOW;
	stInfo.wShowWindow = SW_SHOW;
	PROCESS_INFORMATION processInfo;
	bool retVal = !!CreateProcess(nullptr, (LPTSTR)cmdline.c_str(),
		nullptr, nullptr, FALSE, CREATE_DEFAULT_ERROR_MODE, nullptr, nullptr,
		&stInfo, &processInfo);
	if (!retVal)
	{
		std::wstring msg = L"Failed to launch WinMerge: " + GetLastErrorString() + L"\nCommand line: " + cmdline;
		MessageBox(nullptr, msg.c_str(), L"WinWebDiff", MB_ICONEXCLAMATION | MB_OK);
		return false;
	}
	CloseHandle(processInfo.hThread);
	CloseHandle(processInfo.hProcess);
	return true;
}

std::string ToUTF8(const wchar_t* text)
{
	int size = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, NULL, NULL);
	std::string utf8(size, 0);
	WideCharToMultiByte(CP_UTF8, 0, text, -1, utf8.data(), static_cast<int>(utf8.size()), NULL, NULL);
	utf8.resize(strlen(utf8.c_str()));
	return utf8;
}

void AppMsgBox(const std::wstring& text, int types)
{
	PostMessage(m_hWnd, WM_USER + 100, reinterpret_cast<WPARAM>(new std::wstring(text)), types);
}

bool GenerateHTMLReport(const wchar_t* filename)
{
	const int BUFFER_SIZE = 4096;
	wchar_t tmp[BUFFER_SIZE];
	wchar_t rptdir_full[BUFFER_SIZE];
	std::wstring rptdir;
	std::wstring rptfilepath[3];
	std::wstring difffilename[3];
	std::vector<std::string> rptfilepath_utf8, difffilename_utf8;
	wcscpy_s(rptdir_full, filename);
	PathRemoveExtensionW(rptdir_full);
	PathAddExtensionW(rptdir_full, L".files");
	rptdir = PathFindFileName(rptdir_full);
	CreateDirectoryW(rptdir_full, nullptr);
	const wchar_t* pfilenames[3]{};
	std::vector<std::wstring> filenames;
	for (int i = 0; i < m_pWebDiffWindow->GetPaneCount(); ++i)
	{
		rptfilepath[i] = m_pWebDiffWindow->GetCurrentUrl(i);
		rptfilepath_utf8.push_back(ToUTF8(rptfilepath[i].c_str()));
		wsprintfW(tmp, L"%d.pdf", i + 1);
		difffilename[i] = rptdir + L"/" + tmp;
		difffilename_utf8.push_back(ToUTF8(difffilename[i].c_str()));
		filenames.push_back(std::wstring(rptdir_full) + L"/" + tmp);
		pfilenames[i] = filenames[i].c_str();
	}
	std::ofstream fout;
	try
	{
		fout.open(filename, std::ios::out | std::ios::trunc);
		fout <<
			"<!DOCTYPE html>" << std::endl <<
			"<html>" << std::endl <<
			"<head>" << std::endl <<
			"<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\">" << std::endl <<
			"<title>WinMerge Webpage Compare Report</title>" << std::endl <<
			"<style type=\"text/css\">" << std::endl <<
			"table { table-layout: fixed; width: 100%; border-collapse: collapse; }" << std::endl <<
			"th,td { border: solid 1px black; }" << std::endl <<
			"embed { width: 100%; height: calc(100vh - 56px) }" << std::endl <<
			".title { color: white; background-color: blue; vertical-align: top; }" << std::endl <<
			"</style>" << std::endl <<
			"</head>" << std::endl <<
			"<body>" << std::endl <<
			"<table>" << std::endl <<
			"<tr>" << std::endl;
		for (int i = 0; i < m_pWebDiffWindow->GetPaneCount(); ++i)
			fout << "<th class=\"title\">" << rptfilepath_utf8[i] << "</th>" << std::endl;
		fout << 
			"</tr>" << std::endl <<
			"<tr>" << std::endl;
		for (int i = 0; i < m_pWebDiffWindow->GetPaneCount(); ++i)
			fout << "<td><embed type=\"application/pdf\" src=\"" << difffilename_utf8[i] <<
			"\" title=\"" << difffilename_utf8[i] << "\"></td>" << std::endl;
		fout <<
			"</tr>" << std::endl <<
			"</table>" << std::endl <<
			"</body>" << std::endl <<
			"</html>" << std::endl;
	}
	catch (...)
	{
		return false;
	}
	m_pWebDiffWindow->SaveDiffFiles(IWebDiffWindow::PDF, pfilenames,
		Callback<IWebDiffCallback>([](const WebDiffCallbackResult& result) -> HRESULT
			{
				if (SUCCEEDED(result.errorCode))
					AppMsgBox(L"The report has been created successfully.", MB_OK | MB_ICONINFORMATION);
				else
					AppMsgBox(L"Failed to create the report", MB_OK | MB_ICONWARNING);
				return S_OK;
			})
		.Get());
	return true;
}

void UpdateMenuState(HWND hWnd)
{
	HMENU hMenu = GetMenu(hWnd);
	CheckMenuItem(hMenu, IDM_VIEW_VIEWDIFFERENCES,    m_pWebDiffWindow->GetShowDifferences() ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_VIEW_SPLITHORIZONTALLY,  m_pWebDiffWindow->GetHorizontalSplit() ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuRadioItem(hMenu, IDM_VIEW_DIFF_ALGORITHM_MYERS, IDM_VIEW_DIFF_ALGORITHM_NONE,
		m_pWebDiffWindow->GetDiffOptions().diffAlgorithm + IDM_VIEW_DIFF_ALGORITHM_MYERS, MF_BYCOMMAND);
	CheckMenuItem(hMenu, IDM_SYNC_ENABLED, m_pWebDiffWindow->GetSyncEvents() ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_SYNC_SCROLL,    m_pWebDiffWindow->GetSyncEventFlag(IWebDiffWindow::EVENT_SCROLL) ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_SYNC_CLICK,  m_pWebDiffWindow->GetSyncEventFlag(IWebDiffWindow::EVENT_CLICK) ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_SYNC_INPUT,  m_pWebDiffWindow->GetSyncEventFlag(IWebDiffWindow::EVENT_INPUT) ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_SYNC_GOBACKFORWARD,  m_pWebDiffWindow->GetSyncEventFlag(IWebDiffWindow::EVENT_GOBACKFORWARD) ? MF_CHECKED : MF_UNCHECKED);
	m_pWebToolWindow->Sync();
}

HRESULT OnWebDiffEvent(const WebDiffEvent& e)
{
	if (WebDiffEvent::CompareScreenshotsSelected <= e.type && e.type <= WebDiffEvent::CompareResourceTreesSelected)
		PostMessage(m_hWnd, WM_COMMAND, IDM_COMPARE_SCREENSHOTS + e.type - WebDiffEvent::CompareScreenshotsSelected, 0);
	return S_OK;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_CREATE:
		m_pWebDiffWindow = WinWebDiff_CreateWindow(hInstDLL, hWnd);
		m_pWebToolWindow = WinWebDiff_CreateToolWindow(hInstDLL, hWnd, m_pWebDiffWindow);
		m_hwndWebToolWindow = m_pWebToolWindow->GetHWND();
		m_callback.AddRef();
		m_pWebDiffWindow->AddEventListener(&m_callback);
		UpdateMenuState(hWnd);
		break;
	case WM_SIZE:
	{
		RECT rc, rcToolWindow;
		GetClientRect(hWnd, &rc);
		GetClientRect(m_hwndWebToolWindow, &rcToolWindow);
		rc.right -= rcToolWindow.right;
		m_pWebDiffWindow->SetWindowRect(rc);
		MoveWindow(m_hwndWebToolWindow, rc.right, 0, rcToolWindow.right, rc.bottom, TRUE);
		break;
	}
	case WM_COMMAND:
	{
		int wmId = LOWORD(wParam);
		switch (wmId)
		{
		case IDM_ABOUT:
			DialogBox(m_hInstance, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
			break;
		case IDM_FILE_NEW:
			m_pWebDiffWindow->New(2, nullptr);
			break;
		case IDM_FILE_NEW3:
			m_pWebDiffWindow->New(3, nullptr);
			break;
		case IDM_FILE_NEW_TAB:
		{
			int nActivePane = m_pWebDiffWindow->GetActivePane();
			m_pWebDiffWindow->NewTab(nActivePane < 0 ? 0 : nActivePane, L"about:blank", nullptr);
			break;
		}
		case IDM_FILE_CLOSE_TAB:
		{
			int nActivePane = m_pWebDiffWindow->GetActivePane();
			m_pWebDiffWindow->CloseActiveTab(nActivePane < 0 ? 0 : nActivePane);
			break;
		}
		case IDM_FILE_RELOAD:
			m_pWebDiffWindow->ReloadAll();
			break;
		case IDM_FILE_GENERATE_REPORT:
		{
			wchar_t szFileName[MAX_PATH] = {0}, szFile[MAX_PATH] = {0};
			OPENFILENAMEW ofn = {0};
			ofn.lStructSize = sizeof(OPENFILENAME);
			ofn.hwndOwner = hWnd;
			ofn.lpstrFilter = L"HTML file(*.html)\0*.html\0\0";
			ofn.lpstrFile = szFileName;
			ofn.lpstrFileTitle = szFile;
			ofn.nMaxFile = MAX_PATH;
			ofn.nMaxFileTitle = sizeof(szFile);
			ofn.Flags = OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY;
			ofn.lpstrTitle = L"Generate HTML Report File";
			ofn.lpstrDefExt = L"html";
			if (GetSaveFileNameW(&ofn) != 0)
			{
				GenerateHTMLReport(ofn.lpstrFile);
			}
			break;
		}
		case IDM_EXIT:
			DestroyWindow(hWnd);
			break;
		case IDM_VIEW_VIEWDIFFERENCES:
			m_pWebDiffWindow->SetShowDifferences(!m_pWebDiffWindow->GetShowDifferences());
			UpdateMenuState(hWnd);
			break;
		case IDM_VIEW_SIZE_FIT_TO_WINDOW:
			m_pWebDiffWindow->SetFitToWindow(true);
			m_pWebToolWindow->Sync();
			break;
		case IDM_VIEW_SIZE_320x512:
			m_pWebDiffWindow->SetFitToWindow(false);
			m_pWebDiffWindow->SetSize({ 320, 512 });
			m_pWebToolWindow->Sync();
			break;
		case IDM_VIEW_SIZE_375x600:
			m_pWebDiffWindow->SetFitToWindow(false);
			m_pWebDiffWindow->SetSize({ 375, 600 });
			m_pWebToolWindow->Sync();
			break;
		case IDM_VIEW_SIZE_1024x640:
			m_pWebDiffWindow->SetFitToWindow(false);
			m_pWebDiffWindow->SetSize({ 1024, 640 });
			m_pWebToolWindow->Sync();
			break;
		case IDM_VIEW_SIZE_1280x800:
			m_pWebDiffWindow->SetFitToWindow(false);
			m_pWebDiffWindow->SetSize({ 1280, 800 });
			m_pWebToolWindow->Sync();
			break;
		case IDM_VIEW_SIZE_1440x900:
			m_pWebDiffWindow->SetFitToWindow(false);
			m_pWebDiffWindow->SetSize({ 1440, 900});
			m_pWebToolWindow->Sync();
			break;
		case IDM_VIEW_SPLITHORIZONTALLY:
			m_pWebDiffWindow->SetHorizontalSplit(!m_pWebDiffWindow->GetHorizontalSplit());
			UpdateMenuState(m_hWnd);
			break;
		case IDM_VIEW_DIFF_ALGORITHM_MYERS:
		case IDM_VIEW_DIFF_ALGORITHM_MINIMAL:
		case IDM_VIEW_DIFF_ALGORITHM_PATIENCE:
		case IDM_VIEW_DIFF_ALGORITHM_HISTOGRAM:
		case IDM_VIEW_DIFF_ALGORITHM_NONE:
		{
			IWebDiffWindow::DiffOptions diffOptions = m_pWebDiffWindow->GetDiffOptions();
			diffOptions.diffAlgorithm = wmId - IDM_VIEW_DIFF_ALGORITHM_MYERS;
			m_pWebDiffWindow->SetDiffOptions(diffOptions);
			UpdateMenuState(hWnd);
			break;
		}
		case IDM_COMPARE_RECOMPARE:
			m_pWebDiffWindow->Recompare(nullptr);
			break;
		case IDM_COMPARE_SCREENSHOTS:
		case IDM_COMPARE_FULLSIZE_SCREENSHOTS:
		{
			const wchar_t* pfilenames[3]{};
			std::vector<std::wstring> filenames;
			for (int pane = 0; pane < m_pWebDiffWindow->GetPaneCount(); pane++)
			{
				m_tempFiles.emplace_back();
				m_tempFiles.back().Create(L".png");
				filenames.push_back(m_tempFiles.back().m_path);
				pfilenames[pane] = filenames[pane].c_str();
			}
			m_pWebDiffWindow->SaveFiles(
				(wmId == IDM_COMPARE_FULLSIZE_SCREENSHOTS) ? IWebDiffWindow::FULLSIZE_SCREENSHOT : IWebDiffWindow::SCREENSHOT,
				pfilenames,
				Callback<IWebDiffCallback>([filenames](const WebDiffCallbackResult& result) -> HRESULT
					{
						CompareFiles(filenames, L"");
						return S_OK;
					})
				.Get());
			break;
		}
		case IDM_COMPARE_HTML:
		{
			const wchar_t* pfilenames[3]{};
			std::vector<std::wstring> filenames;
			for (int pane = 0; pane < m_pWebDiffWindow->GetPaneCount(); pane++)
			{
				m_tempFiles.emplace_back();
				m_tempFiles.back().Create(L".html");
				filenames.push_back(m_tempFiles.back().m_path);
				pfilenames[pane] = filenames[pane].c_str();
			}
			m_pWebDiffWindow->SaveFiles(IWebDiffWindow::HTML, pfilenames,
				Callback<IWebDiffCallback>([filenames](const WebDiffCallbackResult& result) -> HRESULT
					{
						CompareFiles(filenames, L"/unpacker PrettifyHTML");
						return S_OK;
					})
				.Get());
			break;
		}
		case IDM_COMPARE_TEXT:
		{
			const wchar_t* pfilenames[3]{};
			std::vector<std::wstring> filenames;
			for (int pane = 0; pane < m_pWebDiffWindow->GetPaneCount(); pane++)
			{
				m_tempFiles.emplace_back();
				m_tempFiles.back().Create(L".txt");
				filenames.push_back(m_tempFiles.back().m_path);
				pfilenames[pane] = filenames[pane].c_str();
			}
			m_pWebDiffWindow->SaveFiles(IWebDiffWindow::TEXT, pfilenames,
				Callback<IWebDiffCallback>([filenames](const WebDiffCallbackResult& result) -> HRESULT
					{
						CompareFiles(filenames, L"");
						return S_OK;
					})
				.Get());
			break;
		}
		case IDM_COMPARE_RESOURCE_TREE:
		{
			const wchar_t* pdirnames[3]{};
			std::vector<std::wstring> dirnames;
			for (int pane = 0; pane < m_pWebDiffWindow->GetPaneCount(); pane++)
			{
				m_tempFolders.emplace_back();
				m_tempFolders.back().Create();
				dirnames.push_back(m_tempFolders.back().m_path);
				pdirnames[pane] = dirnames[pane].c_str();
			}
			m_pWebDiffWindow->SaveFiles(IWebDiffWindow::RESOURCETREE, pdirnames,
				Callback<IWebDiffCallback>([dirnames](const WebDiffCallbackResult& result) -> HRESULT
					{
						CompareFiles(dirnames, L"/r");
						return S_OK;
					})
				.Get());
		break;
		}
		case IDM_COMPARE_NEXTDIFFERENCE:
			m_pWebDiffWindow->NextDiff();
			break;
		case IDM_COMPARE_PREVIOUSDIFFERENCE:
			m_pWebDiffWindow->PrevDiff();
			break;
		case IDM_COMPARE_FIRSTDIFFERENCE:
			m_pWebDiffWindow->FirstDiff();
			break;
		case IDM_COMPARE_LASTDIFFERENCE:
			m_pWebDiffWindow->LastDiff();
			break;
		case IDM_COMPARE_NEXTCONFLICT:
			m_pWebDiffWindow->NextConflict();
			break;
		case IDM_COMPARE_PREVIOUSCONFLICT:
			m_pWebDiffWindow->PrevConflict();
			break;
		case IDM_SYNC_ENABLED:
			m_pWebDiffWindow->SetSyncEvents(!m_pWebDiffWindow->GetSyncEvents());
			UpdateMenuState(m_hWnd);
			break;
		case IDM_SYNC_SCROLL:
			m_pWebDiffWindow->SetSyncEventFlag(IWebDiffWindow::EVENT_SCROLL,
				!m_pWebDiffWindow->GetSyncEventFlag(IWebDiffWindow::EVENT_SCROLL));
			UpdateMenuState(m_hWnd);
			break;
		case IDM_SYNC_CLICK:
			m_pWebDiffWindow->SetSyncEventFlag(IWebDiffWindow::EVENT_CLICK,
				!m_pWebDiffWindow->GetSyncEventFlag(IWebDiffWindow::EVENT_CLICK));
			UpdateMenuState(m_hWnd);
			break;
		case IDM_SYNC_INPUT:
			m_pWebDiffWindow->SetSyncEventFlag(IWebDiffWindow::EVENT_INPUT,
				!m_pWebDiffWindow->GetSyncEventFlag(IWebDiffWindow::EVENT_INPUT));
			UpdateMenuState(m_hWnd);
			break;
		case IDM_SYNC_GOBACKFORWARD:
			m_pWebDiffWindow->SetSyncEventFlag(IWebDiffWindow::EVENT_GOBACKFORWARD,
				!m_pWebDiffWindow->GetSyncEventFlag(IWebDiffWindow::EVENT_GOBACKFORWARD));
			UpdateMenuState(m_hWnd);
			break;
		case IDM_CLEAR_DISK_CACHE:
			m_pWebDiffWindow->ClearBrowsingData(-1, IWebDiffWindow::BrowsingDataType::DISK_CACHE);
			break;
		case IDM_CLEAR_COOKIES:
			m_pWebDiffWindow->ClearBrowsingData(-1, IWebDiffWindow::BrowsingDataType::COOKIES);
			break;
		case IDM_CLEAR_BROWSING_HISTORY:
			m_pWebDiffWindow->ClearBrowsingData(-1, IWebDiffWindow::BrowsingDataType::BROWSING_HISTORY);
			break;
		case IDM_CLEAR_ALL_PROFILE:
			m_pWebDiffWindow->ClearBrowsingData(-1, IWebDiffWindow::BrowsingDataType::ALL_PROFILE);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
		break;
	}
	case WM_USER + 100:
	{
		std::wstring* ptext = reinterpret_cast<std::wstring*>(wParam);
		MessageBoxW(m_hWnd, ptext->c_str(), L"WinWebDiff", static_cast<int>(lParam));
		delete ptext;
		break;
	}
	case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hWnd, &ps);
			EndPaint(hWnd, &ps);
		}
		break;
	case WM_DESTROY:
		WinWebDiff_DestroyWindow(m_pWebDiffWindow);
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}
