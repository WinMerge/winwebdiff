#include "framework.h"
#include "WinWebDiff.h"
#include "../WinWebDiffLib/WinWebDiffLib.h"
#include <combaseapi.h>
#include <shellapi.h>
#include <CommCtrl.h>
#include <string>
#include <vector>
#include <filesystem>
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

HINSTANCE m_hInstance;
HINSTANCE hInstDLL;
HWND m_hWnd;
WCHAR m_szTitle[256] = L"WinWebDiff";
WCHAR m_szWindowClass[256] = L"WINWEBDIFF";
IWebDiffWindow* m_pWebDiffWindow = nullptr;

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
				m_pWebDiffWindow->Open(cmdline.sUrls[0].c_str(), cmdline.sUrls[1].c_str());
			else
				m_pWebDiffWindow->Open(cmdline.sUrls[0].c_str(), cmdline.sUrls[1].c_str(), cmdline.sUrls[2].c_str());
		}
		else
			m_pWebDiffWindow->New(2);
	}

	MSG msg;
	while (GetMessage(&msg, nullptr, 0, 0))
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
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

bool CompareFiles(const std::wstring& filename1, const std::wstring& filename2, const std::wstring& options)
{
	std::wstring cmdline = L"\"C:\\Program Files\\WinMerge\\WinMergeU.exe\" /ub \"" + filename1 + L"\" \"" + filename2 + L"\"";
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

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_CREATE:
		m_pWebDiffWindow = WinWebDiff_CreateWindow(hInstDLL, hWnd);
		break;
	case WM_SIZE:
	{
		RECT rc;
		GetClientRect(hWnd, &rc);
		m_pWebDiffWindow->SetWindowRect(rc);
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
				m_pWebDiffWindow->New(2);
				break;
			case IDM_FILE_NEW_TAB:
			{
				int nActivePane = m_pWebDiffWindow->GetActivePane();
				m_pWebDiffWindow->NewTab(nActivePane < 0 ? 0 : nActivePane, L"about:blank");
				break;
			}
			case IDM_FILE_CLOSE_TAB:
			{
				int nActivePane = m_pWebDiffWindow->GetActivePane();
				m_pWebDiffWindow->CloseActiveTab(nActivePane < 0 ? 0 : nActivePane);
				break;
			}
			case IDM_EXIT:
				DestroyWindow(hWnd);
				break;
			case IDM_VIEW_SIZE_FIT_TO_WINDOW:
				m_pWebDiffWindow->SetFitToWindow(true);
				break;
			case IDM_VIEW_SIZE_1024x600:
				m_pWebDiffWindow->SetFitToWindow(false);
				m_pWebDiffWindow->SetSize({ 1024, 600 });
				break;
			case IDM_VIEW_SIZE_1280x800:
				m_pWebDiffWindow->SetFitToWindow(false);
				m_pWebDiffWindow->SetSize({ 1280, 800 });
				break;
			case IDM_VIEW_SIZE_1440x900:
				m_pWebDiffWindow->SetFitToWindow(false);
				m_pWebDiffWindow->SetSize({ 1440, 900});
				break;
			case IDM_VIEW_SPLITHORIZONTALLY:
				m_pWebDiffWindow->SetHorizontalSplit(!m_pWebDiffWindow->GetHorizontalSplit());
				break;
			case IDM_COMPARE_SCREENSHOTS:
			{
				std::vector<std::wstring> filenames = { L"c:\\tmp\\p0.png", L"c:\\tmp\\p1.png" };
				const wchar_t* pfilenames[3] = { filenames[0].c_str(), filenames[1].c_str() };
				m_pWebDiffWindow->SaveScreenshots(pfilenames,
					Callback<IWebDiffCallback>([filenames](HRESULT hr) -> HRESULT
						{
							CompareFiles(filenames[0].c_str(), filenames[1].c_str(), L"");
							return S_OK;
						})
					.Get());
				break;
			}
			case IDM_COMPARE_HTML:
			{
				std::vector<std::wstring> filenames = { L"c:\\tmp\\p0.html", L"c:\\tmp\\p1.html" };
				const wchar_t* pfilenames[3] = { filenames[0].c_str(), filenames[1].c_str() };
				m_pWebDiffWindow->SaveHTMLs(pfilenames,
					Callback<IWebDiffCallback>([filenames](HRESULT hr) -> HRESULT
						{
							CompareFiles(filenames[0].c_str(), filenames[1].c_str(), L"/unpacker PrettifyHTML");
							return S_OK;
						})
					.Get());
				break;
			}
			case IDM_COMPARE_RESOURCE_TREE:
			{
				std::vector<std::wstring> dirnames = { L"c:\\tmp\\dir1\\", L"c:\\tmp\\dir2\\" };
				const wchar_t* pdirnames[3] = { dirnames[0].c_str(), dirnames[1].c_str() };
				m_pWebDiffWindow->SaveResourceTrees(pdirnames,
					Callback<IWebDiffCallback>([dirnames](HRESULT hr) -> HRESULT
						{
							CompareFiles(dirnames[0].c_str(), dirnames[1].c_str(), L"/r");
							return S_OK;
						})
					.Get());
				break;
			}
			default:
				return DefWindowProc(hWnd, message, wParam, lParam);
			}
		}
		break;
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
