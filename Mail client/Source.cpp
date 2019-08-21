#define WIN32_LEAN_AND_MEAN
//#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

#include <Windows.h>

#include <WindowsX.h>
#include <CommCtrl.h>
#include <tchar.h>
#include <stdio.h>
#include <stdlib.h> 
#include <process.h>
#include <Commdlg.h>
#include "shlwapi.h"
#include <list>
#include <winsock2.h>
#include <iostream>
#include "psapi.h"
#include "strsafe.h"
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "afxres.h"
#include "resource.h"

#pragma comment(lib, "ssleay32.lib")
#pragma comment(lib, "libeay32.lib")
#pragma comment(lib,"Shlwapi.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment (lib, "comctl32")

#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#define WM_ADDITEM  WM_USER+1

#define ID_FROM 2001
#define ID_TO 2002
#define ID_SUBJECT 2003
#define ID_DATA 2004
#define ID_SEND 2005
#define ID_FILE 2006
#define ID_RECIPIENTS 2007
#define ID_ADDRCPT 2008
#define ID_FILES 2009
#define IDC_STATIC 2010

WSADATA ws;
SSL_CTX *ctx;

CRITICAL_SECTION c_section;

TCHAR szNewReciverBuffer[256] = TEXT("");
BOOL selector = TRUE;
char text[512];

TCHAR szFile[MAX_PATH] = { 0 };

char WarnText[256];
char codeBuf[4];

constexpr char hostname[] = "smtp.gmail.com";

std::string file;
static const std::string base64_chars =
"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyz"
"0123456789+/";

static inline bool is_base64(unsigned char c) {
	return (isalnum(c) || (c == '+') || (c == '/'));
}

std::string base64_encode(unsigned char const* bytes_to_encode, unsigned int in_len) {
	std::string ret;
	int i = 0;
	int j = 0;
	unsigned char char_array_3[3];
	unsigned char char_array_4[4];

	while (in_len--) {
		char_array_3[i++] = *(bytes_to_encode++);
		if (i == 3) {
			char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
			char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
			char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
			char_array_4[3] = char_array_3[2] & 0x3f;

			for (i = 0; (i < 4); i++)
				ret += base64_chars[char_array_4[i]];
			i = 0;
		}
	}

	if (i)
	{
		for (j = i; j < 3; j++)
			char_array_3[j] = '\0';

		char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
		char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
		char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
		char_array_4[3] = char_array_3[2] & 0x3f;

		for (j = 0; (j < i + 1); j++)
			ret += base64_chars[char_array_4[j]];

		while ((i++ < 3))
			ret += '=';
	}
	return ret;
}

MSG msg;
HWND hwnd = NULL;
BOOL bRet;
HWND hDlg = NULL;

struct FileStruct {
	char fileName[260];
	char *fileContent;
	LARGE_INTEGER size;
};

std::list<FileStruct> files;
std::list<std::string> recipients;

LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
BOOL OnCreate(HWND hwnd, LPCREATESTRUCT lpCreateStruct);
void OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify);

INT_PTR  CALLBACK DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

BOOL Dialog_OnInitDialog(HWND hwnd, HWND hwndFocus, LPARAM lParam);

void Dialog_OnClose(HWND hwnd);

void Dialog_OnCommand(HWND hWnd, int id, HWND hwndCtl, UINT codeNotify);

void OnAddItem(HWND hwnd, WPARAM wParam);

int create_socket(int port, const char *hostname) {
	int sockfd = 0;
	struct hostent *host;
	struct sockaddr_in dest_addr;

	if ((host = gethostbyname(hostname)) == NULL)
	{
		MessageBox(NULL, TEXT("Cannot resolve hostname\n(Problem with Internet Connection)"), TEXT("Error"), MB_ICONEXCLAMATION | MB_OK);
		return -1;
	}
	sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	dest_addr.sin_family = AF_INET;
	dest_addr.sin_port = htons(port);
	dest_addr.sin_addr.s_addr = *(long*)(host->h_addr);
	memset(&(dest_addr.sin_zero), '\0', 8);

	if (connect(sockfd, (struct sockaddr *) &dest_addr,
		sizeof(struct sockaddr)) == -1)
	{
		StringCchPrintfA(WarnText, _countof(WarnText), "Cannot connect to host %s:%s", hostname, port);
		MessageBox(NULL, WarnText, TEXT("Error"), MB_ICONEXCLAMATION | MB_OK);
		return -1;
	}
	return sockfd;
}
int send_SSLsocket(SSL *ssl, const char *buf)
{
	int all = strlen(buf);

	int sended = 0;
	while (all > 0)
	{
		sended = SSL_write(ssl, buf, all);
		if (sended < 0)
			return SOCKET_ERROR;
		buf += sended;
		all -= sended;
	}
	return all;
}

bool checkstr(const char *buf, int len)
{
	if (len > 2 && buf[len - 1] == '\n' && buf[len - 2] == '\r')
	{
		for (size_t i = len - 3; i > 0; i--)
		{
			if (isdigit(static_cast<unsigned char>(buf[i])) && (buf[i - 3] == '\n' || i - 3 == -1))
			{
				if (buf[i + 1] == ' ')
					return true;
			}
		}
		return false;
	}
	else
		return false;
}

void read_SSLsocket(SSL *ssl)
{
	char ptr[BUFSIZ];
	int read_bytes = 0;
	int r = 0;
	while (1)
	{
		if (read_bytes >= 3)
		{
			if (checkstr(ptr, read_bytes))
				break;
			else
			{
				r = SSL_read(ssl, ptr + read_bytes, BUFSIZ - read_bytes);
				if (r <= 0)
					break;
			}
		}
		else
		{
			r = SSL_read(ssl, ptr + read_bytes, BUFSIZ - read_bytes);
			if (r <= 0)
				break;
		}
		read_bytes += r;
	}

	memcpy(codeBuf, ptr, 3);
	codeBuf[3] = '\0';
}

int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE, LPTSTR lpszCmdLine, int nCmdShow)
{
	if (WSAStartup(0x101, &ws) || ws.wVersion != 0x101)
	{
		MessageBox(NULL, TEXT("WinSock Lib initialization failed"), NULL, MB_OK | MB_ICONERROR);
		return -1;
	}

	setlocale(LC_ALL, "");

	WNDCLASSEX wcex;
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
	wcex.lpfnWndProc = WindowProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
	wcex.lpszMenuName = NULL;
	wcex.lpszClassName = TEXT("Windowslass");;
	wcex.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

	if (0 == RegisterClassEx(&wcex)) // регистрирует класс
	{
		MessageBox(NULL, TEXT("The window class failed to register!"), TEXT("Error"), MB_ICONEXCLAMATION | MB_OK);
		return -1;
	}

	LoadLibrary(TEXT("ComCtl32.dll"));

	hwnd = CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("Windowslass"), TEXT("MAIL"), WS_OVERLAPPEDWINDOW,
		400, 100, 600, 650, NULL, NULL, hInstance, NULL);

	if (NULL == hwnd)
	{
		MessageBox(NULL, TEXT("Problem creating the window"), TEXT("Error"), MB_ICONEXCLAMATION | MB_OK);
		return -1;
	}

	//SSL init
	OpenSSL_add_all_algorithms();

	ERR_load_crypto_strings();
	SSL_load_error_strings();

	if (SSL_library_init() < 0)
	{
		MessageBox(NULL, TEXT("Could not initialize the OpenSSL library"), TEXT("Error"), MB_ICONEXCLAMATION | MB_OK);
		return -1;
	}

	const SSL_METHOD *method = TLSv1_2_client_method();

	if ((ctx = SSL_CTX_new(method)) == NULL)
	{
		MessageBox(NULL, TEXT("Unable to create a new SSL context structure"), TEXT("Error"), MB_ICONEXCLAMATION | MB_OK);
		return -1;
	}
	//SSL init

	ShowWindow(hwnd, nCmdShow);

	for (;;)
	{
		bRet = GetMessage(&msg, NULL, 0, 0);

		if (bRet == -1)
		{
		}
		else if (FALSE == bRet)
		{
			break;
		}
		else
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	WSACleanup();
	return (int)msg.wParam;
}

BOOL OnCreate(HWND hwnd, LPCREATESTRUCT lpCreateStruct)
{
	RECT rect = { 0 };
	GetWindowRect(hwnd, &rect);
	LONG width = rect.right - rect.left;
	LONG height = rect.bottom - rect.top;

	CreateWindowEx(0, TEXT("Static"), TEXT("FROM:"),
		WS_CHILD | WS_VISIBLE, 10, 10, 100, 20, hwnd, (HMENU)IDC_STATIC, NULL, NULL);

	CreateWindowEx(0, TEXT("Edit"), TEXT("Sender"),
		WS_CHILD | WS_VISIBLE |
		ES_LEFT | WS_BORDER, 20, 40, width - 70, 30, hwnd, (HMENU)ID_FROM, lpCreateStruct->hInstance, NULL);

	CreateWindowEx(0, TEXT("Static"), TEXT("TO:"),
		WS_CHILD | WS_VISIBLE, 10, 80, 100, 20, hwnd, (HMENU)IDC_STATIC, NULL, NULL);

	HWND cbmbx = CreateWindowEx(0, TEXT("COMBOBOX"), TEXT("RECIPIENTS"),
		WS_CHILD | WS_VISIBLE | WS_VSCROLL | CBS_DROPDOWNLIST, 20, 110, width / 2, 200,
		hwnd, (HMENU)ID_RECIPIENTS, lpCreateStruct->hInstance, NULL);

	CreateWindowEx(0, TEXT("Button"), TEXT("Add Recipient"),
		WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, width / 2 + 40, 110, width / 3, 30,
		hwnd, (HMENU)ID_ADDRCPT, lpCreateStruct->hInstance, NULL);

	CreateWindowEx(0, TEXT("Static"), TEXT("SUBJECT:"),
		WS_CHILD | WS_VISIBLE, 10, 150, 100, 20, hwnd, (HMENU)IDC_STATIC, NULL, NULL);

	CreateWindowEx(0, TEXT("Edit"), TEXT("Subject"),
		WS_CHILD | WS_VISIBLE |
		ES_LEFT | WS_BORDER, 20, 180, width - 70, 30, hwnd, (HMENU)ID_SUBJECT, lpCreateStruct->hInstance, NULL);

	CreateWindowEx(0, TEXT("Edit"), TEXT("Text...\r\n\r\n\r\n\r\nSent From Mail Client"),
		WS_CHILD | WS_VISIBLE | WS_VSCROLL |
		ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | WS_BORDER, 20, 220, width - 40, 200, hwnd, (HMENU)ID_DATA, lpCreateStruct->hInstance, NULL);

	CreateWindowEx(0, TEXT("Button"), TEXT("Send"),
		WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 20, 440, width / 2, 40,
		hwnd, (HMENU)ID_SEND, lpCreateStruct->hInstance, NULL);

	CreateWindowEx(0, TEXT("Static"), TEXT("Insert Files:"),
		WS_CHILD | WS_VISIBLE, 10, 490, 100, 20, hwnd, (HMENU)IDC_STATIC, NULL, NULL);

	CreateWindowEx(0, TEXT("COMBOBOX"), TEXT("FILES"),
		WS_CHILD | WS_VISIBLE | WS_VSCROLL | CBS_DROPDOWNLIST, 20, 520, width / 2, 200,
		hwnd, (HMENU)ID_FILES, lpCreateStruct->hInstance, NULL);

	CreateWindowEx(0, TEXT("Button"), TEXT("Add File"),
		WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, width / 2 + 40, 520, width / 3, 30,
		hwnd, (HMENU)ID_FILE, lpCreateStruct->hInstance, NULL);

	InitializeCriticalSection(&c_section);

	return TRUE;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		HANDLE_MSG(hwnd, WM_CREATE, OnCreate);
		HANDLE_MSG(hwnd, WM_COMMAND, OnCommand);
	case WM_CLOSE:
	{
		WSACleanup();
		DestroyWindow(hwnd);
	}
	break;
	case WM_DESTROY:
	{
		for (FileStruct fs : files)
		{
			if (fs.fileContent != NULL)
				delete fs.fileContent;
		}

		SSL_CTX_free(ctx);
		DeleteCriticalSection(&c_section);
		PostQuitMessage(0);
	}
	break;

	case WM_ADDITEM:
	{
		OnAddItem(hwnd, wParam);
	}
	return 0;


	case WM_KEYDOWN:
	{
		if (wParam == VK_DELETE)
		{
			if (selector)
			{
				std::list<std::string>::iterator it = recipients.begin();

				HWND hwndCtl = GetDlgItem(hwnd, ID_RECIPIENTS);
				int iItem = ComboBox_GetCurSel(hwndCtl);
				if (iItem != -1)
				{
					advance(it, iItem);
					recipients.erase(it);
					ComboBox_DeleteString(hwndCtl, iItem);
					if (!recipients.empty())
					{
						ComboBox_SetCurSel(hwndCtl, recipients.size() - 1);
					}
				} // if
			}
			else
			{
				std::list<FileStruct>::iterator it = files.begin();

				HWND hwndCtl = GetDlgItem(hwnd, ID_FILES);
				int iItem = ComboBox_GetCurSel(hwndCtl);
				if (iItem != -1)
				{
					advance(it, iItem);

					if (*it->fileContent != NULL)
						delete[] it->fileContent;
					files.erase(it);
					ComboBox_DeleteString(hwndCtl, iItem);
					if (!files.empty())
					{
						ComboBox_SetCurSel(hwndCtl, files.size() - 1);
					}
				} // if
			}

			InvalidateRect(hwnd, NULL, TRUE);
			UpdateWindow(hwnd);
		}
	}
	return 0;

	}

	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

std::string EncodeField(HWND hwndctl, const std::string &param)
{
	int length = SendMessageW(hwndctl, WM_GETTEXTLENGTH, 0, 0);
	std::wstring message;
	message.resize(length + 1);
	SendMessageW(hwndctl, WM_GETTEXT, message.length(), (LPARAM)message.c_str());

	length = WideCharToMultiByte(CP_UTF8, 0, message.c_str(), message.length(), nullptr, 0, nullptr, nullptr);

	std::string texto;
	texto.resize(length + 1);

	length = WideCharToMultiByte(CP_UTF8, 0, message.c_str(), message.length(), &texto[0], texto.length(), nullptr, nullptr);
	texto.resize(length + 1);
	texto = texto;
	return param + ":=?UTF-8?b?" + base64_encode(reinterpret_cast<const unsigned char*>(texto.c_str()), texto.length()) + "?=";
}

unsigned _stdcall SendMail(void *lpparameter)
{
	SSL *ssl;
	ssl = SSL_new(ctx);

	int sock = create_socket(465, hostname);
	if (sock == -1)
	{
		return -1;
	}

	SSL_set_fd(ssl, sock);

	if (SSL_connect(ssl) != 1)
	{
		MessageBox(NULL, "Could not build a SSL session", "Error", MB_ICONEXCLAMATION | MB_OK);
		return -1;
	}

	read_SSLsocket(ssl);
	send_SSLsocket(ssl, "EHLO smtpclient\r\n");
	read_SSLsocket(ssl);

	send_SSLsocket(ssl, "AUTH LOGIN\r\n");
	read_SSLsocket(ssl);

	std::string login64 = "";

	login64 = base64_encode(reinterpret_cast<const unsigned char*>(login64.c_str()), login64.length());
	login64 = login64 + "\r\n";
	send_SSLsocket(ssl, login64.c_str());
	read_SSLsocket(ssl);

	std::string pass64 = "";

	pass64 = base64_encode(reinterpret_cast<const unsigned char*>(pass64.c_str()), pass64.length());
	pass64 = pass64 + "\r\n";

	send_SSLsocket(ssl, pass64.c_str());
	read_SSLsocket(ssl);

	std::string from = "";
	from = base64_encode(reinterpret_cast<const unsigned char*>(from.c_str()), from.length());
	from = "MAIL FROM: <" + from + ">\r\n";
	send_SSLsocket(ssl, from.c_str());
	read_SSLsocket(ssl);

	int wrongRcpt = 0;
	std::string To = "TO:";
	int i = 1;
	for (std::string rcpt : recipients)
	{
		StringCchPrintfA(text, _countof(text), "RCPT TO: <%s>\r\n", rcpt.c_str());
		send_SSLsocket(ssl, text);
		read_SSLsocket(ssl);

		if (strcmp(codeBuf, "553") == 0)
		{
			wrongRcpt++;
			StringCchPrintfA(text, _countof(text), "Wrong email address: %s", rcpt.c_str());
			MessageBox(NULL, text, "RCPT Error", MB_ICONEXCLAMATION | MB_OK);

			if (recipients.size() == wrongRcpt)
			{
				send_SSLsocket(ssl, "QUIT\r\n");
				return -1;
			}
		}
		else
		{
			To += rcpt + ",";
		}

	}

	To.erase(To.length() - 1, 1);

	send_SSLsocket(ssl, "DATA\r\n");
	read_SSLsocket(ssl);

	if (strcmp(codeBuf, "354") == 0)
	{
		HWND hwndctl = GetDlgItem(hwnd, ID_FROM);
		std::string from = EncodeField(hwndctl, "FROM");
		from += "\r\n";
		send_SSLsocket(ssl, from.c_str());

		To += "\r\n";
		send_SSLsocket(ssl, To.c_str());

		hwndctl = GetDlgItem(hwnd, ID_SUBJECT);
		std::string subject = EncodeField(hwndctl, "SUBJECT");
		subject += "\r\n";
		send_SSLsocket(ssl, subject.c_str());

		int length = 0;
		LPSTR LpMessage;

		hwndctl = GetDlgItem(hwnd, ID_DATA);
		length = SendMessageA(hwndctl, WM_GETTEXTLENGTH, 0, 0);
		LpMessage = (LPSTR)malloc(length + 1);
		SendMessageA(hwndctl, WM_GETTEXT, length + 1, (LPARAM)(LpMessage));

		if (!files.empty())
		{
			std::string MIMEHEADER = "MIME-Version: 1.0\r\nContent-Type:multipart/mixed;boundary=frontier\r\n\r\n";
			MIMEHEADER += "--frontier\r\nContent-Disposition:inline\r\nContent-Type:text/plain\r\r\n\r\n";
			send_SSLsocket(ssl, MIMEHEADER.c_str());
			send_SSLsocket(ssl, LpMessage);

			for (FileStruct file : files)
			{
				send_SSLsocket(ssl, "\r\n\r\n--frontier\r\n");
				std::string MIMEFILE = "Content-Type:application/octet-stream;\r\nContent-Transfer-Encoding:base64\r\nContent-Disposition:attachment;filename=\"";
				MIMEFILE+= file.fileName;
				MIMEFILE += "\";\r\n\r\n";
				send_SSLsocket(ssl, MIMEFILE.c_str());
				std::string filetosend = base64_encode(reinterpret_cast<const unsigned char*>(file.fileContent), file.size.QuadPart);
				send_SSLsocket(ssl, filetosend.c_str());
			}
			send_SSLsocket(ssl, "\r\n--frontier--");
		}
		else
		{
			std::string MIMEHEADER = "MIME-Version:1.0\r\nContent-Type:text/plain\r\nContent-Disposition:inline\r\n";
			send_SSLsocket(ssl, MIMEHEADER.c_str());
			send_SSLsocket(ssl, LpMessage);
		}

		send_SSLsocket(ssl, "\r\n.\r\n");
		read_SSLsocket(ssl);
		if (strcmp(codeBuf, "250") == 0)
		{
			MessageBox(NULL, "EMail successfuly send", "Success", MB_ICONINFORMATION | MB_OK);
		}
		else
			MessageBox(NULL, "Failure", "Error", MB_ICONEXCLAMATION | MB_OK);

		send_SSLsocket(ssl, "QUIT\r\n");
	}
	else
	{
		send_SSLsocket(ssl, "QUIT\r\n");
		MessageBox(NULL, "Failure", "Error", MB_ICONEXCLAMATION | MB_OK);
	}

	closesocket(sock);
	SSL_shutdown(ssl);
	return 0;
}

unsigned _stdcall LoadFileAsync(void *lpparameter)
{
	TCHAR *filename = (TCHAR *)lpparameter;

	FileStruct file = { 0 };

	HANDLE hExistingFile = CreateFile((TCHAR *)filename, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);

	if (INVALID_HANDLE_VALUE == hExistingFile)
	{
		return FALSE;
	} // if

	LARGE_INTEGER size;
	BOOL bRet = GetFileSizeEx(hExistingFile, &size);

	if ((FALSE != bRet) && (size.LowPart > 0))
	{
		file.fileContent = new char[size.QuadPart];
		bRet = ReadFile(hExistingFile, file.fileContent, size.QuadPart, NULL, NULL);

		if (FALSE == bRet)
		{
			delete[] file.fileContent;
		} // if
		else
		{

			ULARGE_INTEGER u_size = { 0 };

			for (FileStruct file : files)
			{
				u_size.QuadPart += file.size.QuadPart;
			}

			if (u_size.QuadPart + size.QuadPart >= 0x2000000ULL)
			{
				MessageBox(NULL, TEXT("Files size too large\ntotal file size should be less than 20 Mb"), TEXT("Error"), MB_ICONEXCLAMATION | MB_OK);
				delete[] file.fileContent;
				return 0;
			}

			PathStripPath(filename);

			int length = strlen(filename) + 1;
			length = MultiByteToWideChar(CP_ACP, 0, filename, length, nullptr, 0);

			std::wstring message;
			message.resize(length);

			length = MultiByteToWideChar(CP_ACP, 0, filename, length, &message[0], message.length());
			message.resize(length + 1);

			length = WideCharToMultiByte(CP_UTF8, 0, message.c_str(), message.length(), nullptr, 0, nullptr, nullptr);

			std::string texto;
			texto.resize(length + 1);

			length = WideCharToMultiByte(CP_UTF8, 0, message.c_str(), message.length(), &texto[0], texto.length(), nullptr, nullptr);
			texto.resize(length + 1);

			StringCchCopyA(file.fileName, texto.length(), texto.c_str());

			file.size = size;
			EnterCriticalSection(&c_section);
			files.push_back(file);

			HWND hwndCtl = GetDlgItem(hwnd, ID_FILES);
			int iItem = ComboBox_AddItemData(hwndCtl, filename);
			ComboBox_SetCurSel(hwndCtl, iItem);
			LeaveCriticalSection(&c_section);
		}

	} // if

	CloseHandle(hExistingFile);
	return 0;
}

void OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
{
	switch (id)
	{

	case ID_RECIPIENTS:
	{
		if (codeNotify == CBN_SELCHANGE || codeNotify == CBN_DROPDOWN)
		{
			SetFocus(hwnd);
			selector = TRUE;
		}

	}
	break;

	case ID_FILES:
	{
		if (codeNotify == CBN_SELCHANGE || codeNotify == CBN_DROPDOWN)
		{
			SetFocus(hwnd);
			selector = FALSE;
		}
	}
	break;

	case ID_SEND:
	{
		int a = recipients.size();
		if (recipients.size() != 0)
		{
			HANDLE sendThread = (HANDLE)_beginthreadex(NULL, 0, SendMail, NULL, 0, NULL);
			CloseHandle(sendThread);
		}
		else
		{
			MessageBox(NULL, TEXT("At least one recipient is required"), TEXT("Error"), MB_ICONEXCLAMATION | MB_OK);
		}
	}
	break;

	case ID_FILE:
	{
		OPENFILENAME ofn = { sizeof(OPENFILENAME) };
		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = hwnd;
		ofn.lpstrFile = szFile;
		ofn.nMaxFile = sizeof(szFile);
		ofn.lpstrFilter = _T("All\0");
		ofn.nFilterIndex = 1;
		ofn.lpstrFileTitle = NULL;
		ofn.nMaxFileTitle = 0;
		ofn.lpstrInitialDir = NULL;
		ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

		if (GetOpenFileName(&ofn) != FALSE)
		{
			HANDLE hp = (HANDLE)_beginthreadex(NULL, 0, LoadFileAsync, (void *)&szFile, 0, NULL);
			CloseHandle(hp);
		}

		ZeroMemory(&ofn, sizeof(ofn));

	}
	break;

	case ID_ADDRCPT:
	{
		HINSTANCE hInstance = GetWindowInstance(hwnd);

		int nDlgResult = DialogBox(hInstance, MAKEINTRESOURCE(IDD_DIALOG1), hwnd, DialogProc);
		if (IDOK == nDlgResult)
		{
			SendMessage(hwnd, WM_ADDITEM, 0, 0);
		}
	}
	break;
	}//switch
}

INT_PTR CALLBACK  DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_INITDIALOG:
	{
		BOOL bRet = HANDLE_WM_INITDIALOG(hwndDlg, wParam, lParam, Dialog_OnInitDialog);
		return SetDlgMsgResult(hwndDlg, uMsg, bRet);
	}
	case WM_CLOSE:
		HANDLE_WM_CLOSE(hwndDlg, wParam, lParam, Dialog_OnClose);
		return TRUE;
	case WM_COMMAND:
		HANDLE_WM_COMMAND(hwndDlg, wParam, lParam, Dialog_OnCommand);
		return TRUE;
	} // switch

	return FALSE;
}

BOOL Dialog_OnInitDialog(HWND hwnd, HWND hwndFocus, LPARAM lParam)
{
	HWND hwndEdit = GetDlgItem(hwnd, IDC_EDIT1);
	Edit_LimitText(hwndEdit, _countof(szNewReciverBuffer) - 1);
	return TRUE;
}

void Dialog_OnClose(HWND hwnd)
{
	if (hwnd == hDlg)
	{
		DestroyWindow(hwnd);
	}
	else
	{
		EndDialog(hwnd, IDCLOSE);
	}
}

void Dialog_OnCommand(HWND hWnd, int id, HWND hwndCtl, UINT codeNotify)
{

	switch (id)
	{
	case IDOK:
	{
		int cch = GetDlgItemText(hWnd, IDC_EDIT1, szNewReciverBuffer, _countof(szNewReciverBuffer));

		if (0 == cch) // в редактируемого поле нет текста
		{
			HWND hwndEdit = GetDlgItem(hWnd, IDC_EDIT1);
			EDITBALLOONTIP ebt = { sizeof(EDITBALLOONTIP) };
			ebt.pszTitle = L"Empty";
			ebt.pszText = L"Input new RCPT";
			ebt.ttiIcon = TTI_WARNING;
			Edit_ShowBalloonTip(hwndEdit, &ebt);
		} // if
		else if (hWnd == hDlg)
		{
			SetDlgItemText(hWnd, IDC_EDIT1, NULL);
			SendMessage(hwnd, WM_ADDITEM, 0, 0);
		} // if
		else
		{
			EndDialog(hWnd, IDOK);
		} // else
	} // if
	break;

	case IDCANCEL:
		if (hWnd == hDlg)
		{
			DestroyWindow(hWnd);
		} // if
		else
		{
			EndDialog(hWnd, IDCANCEL);
		} // else
		break;
	} // switch
} // Dialog_OnCommand

void OnAddItem(HWND hwnd, WPARAM wParam)
{
	HWND hwndCtl = GetDlgItem(hwnd, ID_RECIPIENTS);
	int iItem = ComboBox_AddItemData(hwndCtl, szNewReciverBuffer);
	recipients.push_back(szNewReciverBuffer);
	ComboBox_SetCurSel(hwndCtl, iItem);
} // OnAddItem
