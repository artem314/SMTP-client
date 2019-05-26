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

#include <vector>

#include <winsock2.h>
#include <iostream>
#include "psapi.h"
#include "strsafe.h"
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "afxres.h"
#pragma comment(lib, "ssleay32.lib")
#pragma comment(lib, "libeay32.lib")

#pragma comment(lib, "ws2_32.lib")

#define ID_FROM 2001
#define ID_TO 2002
#define ID_SUBJECT 2003
#define ID_DATA 2004
#define ID_SEND 2005
#define ID_FILE 2006
#define ID_RECIPIENTS 2007
#define ID_ADDRCPT 2008

#define STD_LNG 512

WSADATA ws;
SSL_CTX *ctx;

CRITICAL_SECTION c_section;

TCHAR szBuffer[100] = TEXT("");

char text[256];
char Data[512];

char WarnText[256];
char RecvBuf[512];

char codeBuf[4];

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
HWND hFindDlg = NULL;
HACCEL hAccel = NULL;

OVERLAPPED _oRead = { 0 }, _oWrite = { 0 };
//char *lpszBufferText;

struct FILEstruct {
	char fileName[260];
	char *fileContent;
	LARGE_INTEGER size;
};

std::vector<FILEstruct> files;
std::vector<std::string> recipients;

LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void OnIdle(HWND hwnd);
BOOL PreTranslateMessage(LPMSG lpMsg);
BOOL OnCreate(HWND hwnd, LPCREATESTRUCT lpCreateStruct);
void OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify);

INT_PTR  CALLBACK DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

BOOL Dialog_OnInitDialog(HWND hwnd, HWND hwndFocus, LPARAM lParam);

void Dialog_OnClose(HWND hwnd);

void Dialog_OnCommand(HWND hWnd, int id, HWND hwndCtl, UINT codeNotify);

void OnAddItem(HWND hwnd, WPARAM wParam);

int create_socket(int port, const char *hostname) {
	int sockfd;
	char *tmp_ptr = NULL;
	struct hostent *host;
	struct sockaddr_in dest_addr;

	if ((host = gethostbyname(hostname)) == NULL) 
	{
		MessageBox(NULL, TEXT("Не удалось обнаружить по доменному имени"), TEXT("Ошибка"), MB_ICONEXCLAMATION | MB_OK);
		//printf("Error: Cannot resolve hostname .\n");
		return -1;
	}

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	dest_addr.sin_family = AF_INET;
	dest_addr.sin_port = htons(port);
	dest_addr.sin_addr.s_addr = *(long*)(host->h_addr);
	memset(&(dest_addr.sin_zero), '\0', 8);

	tmp_ptr = inet_ntoa(dest_addr.sin_addr);

	if (connect(sockfd, (struct sockaddr *) &dest_addr,
		sizeof(struct sockaddr)) == -1) 
	{
		StringCchPrintfA(WarnText, _countof(WarnText), "Не удалось присоединиться к хосту %s с портом %s", hostname, port);
		MessageBox(NULL, WarnText, TEXT("Ошибка"), MB_ICONEXCLAMATION | MB_OK);
		//printf("Error: Cannot connect to host %s [%s] on port .\n", hostname, tmp_ptr, port);
	}

	return sockfd;
}
int send_SSLsocket(SSL *ssl,const char *buf)
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

bool checkstr(const char *buf,int len)
{
	if (len > 2 && buf[len - 1] == '\n' && buf[len - 2] == '\r')
	{
		for (size_t i = len - 3; i > 0; i--)
		{
			if (buf[i] == '\n')
			{
				//isalpha()
				char bufNumb[4];
				memcpy(bufNumb, &buf[i+1], 3);
				bufNumb[3] = '\0';
				if (atoi(bufNumb) > 100 && buf[i + 4] == ' ' )
					return true;
			}
		}

		return false;
	}
	else
		return false;

}

//void read_SSLsocket(SSL *ssl,bool multilineResponse)
//{
//	int size_recv, total_size = 0;
//	char chunk[BUFSIZ];
//	char recvbuf[BUFSIZ] = "";
//	while (1)
//	{
//		memset(chunk, 0, BUFSIZ);  //clear the variable
//		if (multilineResponse)
//		{
//			if (total_size >= 2)
//			{
//				if (recvbuf[total_size - 1] == '\n' && recvbuf[total_size - 2] == '\r'
//					&&	checkstr(recvbuf, total_size))
//				{
//					break;
//				}
//			}
//			else 
//			{
//				if ((size_recv = SSL_read(ssl, chunk, BUFSIZ - total_size)) < 0)
//				{
//					break;
//				}
//				else if (size_recv == 0) break;
//				else
//				{
//					total_size += size_recv;
//					StringCchCatA(recvbuf, BUFSIZ, chunk);
//				}
//			}
//		}
//		else
//		{
//			if (total_size >= 2)
//			{
//				if (recvbuf[total_size - 1] == '\n'&& recvbuf[total_size - 2] == '\r')
//					break;
//			}
//			else
//			{
//				if ((size_recv = SSL_read(ssl, chunk, BUFSIZ - total_size)) < 0)
//				{
//					break;
//				}
//				else if (size_recv == 0)
//					break;
//				else
//				{
//					total_size += size_recv;
//					StringCchCatA(recvbuf, BUFSIZ, chunk);
//				}
//			}
//		}
//	}
//
//	memcpy(codeBuf, recvbuf, 3);
//	codeBuf[3] = '\0';
//}

void read_SSLsocket(SSL *ssl, bool multilineResponse)
{
	char ptr[BUFSIZ];
	int read_bytes = 0;
	int r = 0;
	while (1)
	{
		if (read_bytes >= 3)
		{
			if (multilineResponse)
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
				if (ptr[read_bytes - 1] == '\n'&& ptr[read_bytes - 2] == '\r')
					break;
				else
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
		MessageBox(NULL, TEXT("Ошибка инициализации библиотеки WinSock"), NULL, MB_OK | MB_ICONERROR);
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
		MessageBox(NULL, TEXT("Не удалось зарегестрировать класс!"), TEXT("Ошибка"), MB_ICONEXCLAMATION | MB_OK);
		return -1;
	}

	LoadLibrary(TEXT("ComCtl32.dll"));

	hwnd = CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("Windowslass"), TEXT("MAIL"), WS_OVERLAPPEDWINDOW,
		400, 100, 600, 550, NULL, NULL, hInstance, NULL);

	if (NULL == hwnd)
	{
		MessageBox(NULL, TEXT("Не удалось создать окно!"), TEXT("Ошибка"), MB_ICONEXCLAMATION | MB_OK);
		return -1;
	}

	//инициализация SSL
	OpenSSL_add_all_algorithms();

	ERR_load_crypto_strings();
	SSL_load_error_strings();

	if (SSL_library_init() < 0)
	{
		printf("Could not initialize the OpenSSL library !\n");
		return -1;
	}
		
	const SSL_METHOD *method = TLSv1_2_client_method();

	if ((ctx = SSL_CTX_new(method)) == NULL)
	{
		printf("Unable to create a new SSL context structure.\n");
		return -1;
	}
	//инициализация SSL

	ShowWindow(hwnd, nCmdShow);

	for (;;)
	{
		//while (!PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE))
		//{
		//	OnIdle(hwnd);
		//} // while
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

BOOL TryFinishIo(LPOVERLAPPED lpOverlapped)
{
	if (NULL != lpOverlapped->hEvent)
	{
		// определяем состояние операции ввода/вывода
		DWORD dwResult = WaitForSingleObject(lpOverlapped->hEvent, 0);

		if (WAIT_OBJECT_0 == dwResult) // операция завершена
		{
			CloseHandle(lpOverlapped->hEvent), lpOverlapped->hEvent = NULL;
			return TRUE;
		} // if
	} // if

	return FALSE;
} // TryFinishIo

//void OnIdle(HWND hwnd)
//{
//	if (NULL != lpszBufferText)
//	{
//		if (TryFinishIo(&_oRead) != FALSE) // асинхронное чтение данных из файла завершено
//		{
//			if (ERROR_SUCCESS == _oRead.Internal) // чтение завершено успешно
//			{
//				FILEstruct filesnd = { 0 };
//				filesnd.fileContent = new char[_oRead.InternalHigh];
//				filesnd.size.QuadPart = _oRead.InternalHigh;
//				memcpy(filesnd.fileContent, lpszBufferText, _oRead.InternalHigh);
//				files.push_back(filesnd);
//				
//				MessageBox(hwnd, "ЗАГРУЖЕНО", "", MB_ICONEXCLAMATION | MB_OK);
//			} // if
//
//			// освобождаем выделенную память
//			delete[] lpszBufferText, lpszBufferText = NULL;
//		} // if
//		
//	} // if
//} // OnIdle

BOOL PreTranslateMessage(LPMSG lpMsg)
{
	BOOL bRet = TRUE;

	if (!TranslateAccelerator(hwnd, hAccel, lpMsg))
	{
		bRet = IsDialogMessage(hDlg, lpMsg);

		if (FALSE == bRet)
			bRet = IsDialogMessage(hFindDlg, lpMsg);
	}

	return bRet;
}

BOOL OnCreate(HWND hwnd, LPCREATESTRUCT lpCreateStruct)
{
	RECT rect = { 0 };
	GetWindowRect(hwnd, &rect);
	LONG width = rect.right - rect.left;
	LONG height = rect.bottom - rect.top;

	CreateWindowEx(0, TEXT("Edit"), NULL,
		WS_CHILD | WS_VISIBLE  |
		ES_LEFT  |   WS_BORDER  , 10, 10, width - 40, 40, hwnd, (HMENU)ID_FROM, lpCreateStruct->hInstance, NULL);

	//CreateWindowEx(0, TEXT("Edit"), TEXT("artem_pochta_314@mail.ru"),
	//	WS_CHILD | WS_VISIBLE |  
	//	ES_LEFT  | WS_BORDER, 10, 60, width - 40, 40, hwnd, (HMENU)ID_TO, lpCreateStruct->hInstance, NULL);


	HWND cbmbx = CreateWindowEx(0, TEXT("COMBOBOX"), TEXT("Получатели"),
		WS_CHILD | WS_VISIBLE  | WS_VSCROLL | CBS_DROPDOWNLIST , 10, 60, width /2 , 200,
		hwnd, (HMENU)ID_RECIPIENTS, lpCreateStruct->hInstance, NULL);

	ComboBox_AddString(cbmbx, "artem_pochta_314@mail.ru");
	ComboBox_SetCurSel(cbmbx, 0);

	CreateWindowEx(0, TEXT("Button"), TEXT("Добавить"),
		WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, width / 2 + 40, 60, width /3	, 40,
		hwnd, (HMENU)ID_ADDRCPT, lpCreateStruct->hInstance, NULL);

	CreateWindowEx(0, TEXT("Edit"), TEXT("Subject по русски"),
		WS_CHILD | WS_VISIBLE |  
		ES_LEFT  | WS_BORDER, 10, 110, width - 40, 40, hwnd, (HMENU)ID_SUBJECT, lpCreateStruct->hInstance, NULL);

	CreateWindowEx(0, TEXT("Edit"), TEXT("TEXT по русски"),
		WS_CHILD | WS_VISIBLE | WS_VSCROLL |
		ES_LEFT  | ES_MULTILINE | ES_AUTOVSCROLL | WS_BORDER, 10, 160, width - 40, 200, hwnd, (HMENU)ID_DATA, lpCreateStruct->hInstance, NULL);

	
	CreateWindowEx(0, TEXT("Button"), TEXT("Отправить"),
		WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 10, 370, width /2, 40,
		hwnd, (HMENU)ID_SEND, lpCreateStruct->hInstance, NULL);

	CreateWindowEx(0, TEXT("Button"), TEXT("ФАЙЛ"),
		WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 10, 420, width / 2, 40,
		hwnd, (HMENU)ID_FILE, lpCreateStruct->hInstance, NULL);

	InitializeCriticalSection(&c_section);

	return TRUE;
}

void DrawWindow()
{

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
		for (size_t i = 0; i < files.size(); i++)
		{
			if (files[i].fileContent != NULL)
				delete[] files[i].fileContent;
		}

		DeleteCriticalSection(&c_section);
		PostQuitMessage(0);
	}
	break;
	case WM_PAINT:
	{
		DrawWindow();
	}
	break;

	case WM_ADDITEM:
	{
		OnAddItem(hwnd, wParam);
	}
	return 0;

	}

	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

unsigned _stdcall SendMail(void *lpparameter)
{

	SSL *ssl; 
	ssl = SSL_new(ctx);

	constexpr char hostname[] = "smtp.gmail.com";
	int sock = create_socket(465, hostname);
	if (sock == FALSE)
	{
		return -1;
		printf("Не удалось подключиться к серверу to:%s\n", hostname);
	}
	else
		
		
	SSL_set_fd(ssl, sock);

	if (SSL_connect(ssl) != 1)
	{
		printf("Error: Could not build a SSL session to:%s\n", hostname);
		return -1;
	}
		
	send_SSLsocket(ssl,"EHLO smtpclient\r\n");
	read_SSLsocket(ssl,true);
	printf("%s\n", codeBuf);
	printf("ehlo---------------------------------\n");

	send_SSLsocket(ssl, "AUTH LOGIN\r\n");
	read_SSLsocket(ssl,false);
	printf("%s\n", codeBuf);

	std::string login64 = "artem3331114@gmail.com";

	//std::string login64 = "artem_korshunov_2012@mail.ru";

	login64 = base64_encode(reinterpret_cast<const unsigned char*>(login64.c_str()), login64.length());
	login64 = login64 + "\r\n";
	send_SSLsocket(ssl, login64.c_str());
	read_SSLsocket(ssl, false);
	printf("%s\n", codeBuf);


	std::string pass64 = "smjgzicjouudmlka";

	pass64 = base64_encode(reinterpret_cast<const unsigned char*>(pass64.c_str()), pass64.length());
	pass64 = pass64 + "\r\n";

	send_SSLsocket(ssl, pass64.c_str());
	read_SSLsocket(ssl, false);
	printf("%s\n", codeBuf);

	std::string from = "artem_korshunov_2012@mail.com";
	from = base64_encode(reinterpret_cast<const unsigned char*>(from.c_str()), from.length());
	from = "MAIL FROM: <" + from + ">\r\n";
	send_SSLsocket(ssl, from.c_str());
	read_SSLsocket(ssl, false);
	printf("%s\n", codeBuf);

	char dlgtext[256];

	GetDlgItemTextA(hwnd, ID_TO, dlgtext, _countof(dlgtext));
	StringCchPrintfA(text, _countof(text), "RCPT TO: <%s>\r\n", dlgtext);
	send_SSLsocket(ssl, text);
	read_SSLsocket(ssl, false);
	printf("%s\n", codeBuf);

	send_SSLsocket(ssl, "DATA\r\n");
	read_SSLsocket(ssl, false);
	printf("%s\n", codeBuf);
	

	//send_SSLsocket(ssl, "TO: artem_korshunov_2012@mail.com");

	//std::string from1 = "RANDOM PAL";
	//StringCchPrintfA(text, _countof(text), "FROM: %s <artem_@korshunov_2012@mail.ru>\r\n", from1.c_str());
	//send_SSLsocket(ssl, text);


	GetDlgItemTextA(hwnd, ID_SUBJECT, dlgtext, _countof(dlgtext));
	StringCchPrintfA(text, _countof(text), "SUBJECT: %s\r\n", (LPSTR)dlgtext);
	send_SSLsocket(ssl, text);
	

	HWND hwndctl = GetDlgItem(hwnd, ID_DATA);
	int length = SendMessageA(hwndctl, WM_GETTEXTLENGTH, 0, 0);
	LPSTR LpMessage = (LPSTR)malloc(length + 1);
	SendMessageA(hwndctl, WM_GETTEXT, length + 1, (LPARAM)(LpMessage));
	
	//memcpy(Data, &LpMessage, length);

	//StringCchCatA(LpMessage, length, "\r\n");
	send_SSLsocket(ssl, LpMessage); //ДЛЯ СООБЩЕНИЙ
	
	if (files.size() != 0)
	{
		for (size_t i = 0; i < files.size(); i++)
		{
			//TO-DO ДОБАВИТЬ MIME
			std::string filetosend = base64_encode(reinterpret_cast<const unsigned char*>(files[i].fileContent), files[i].size.QuadPart);
			send_SSLsocket(ssl, filetosend.c_str());
			send_SSLsocket(ssl, "--frontier");
		}
		//send_SSLsocket(ssl, "\n");
		//send_SSLsocket(ssl, "--frontier--");
	}

	send_SSLsocket(ssl, "\r\n.\r\n");
	read_SSLsocket(ssl, false);
	
	MessageBox(NULL, codeBuf, "", MB_ICONEXCLAMATION | MB_OK);

	int ret = strcmp(codeBuf, "250");
	if (ret == 0)
	{
		MessageBox(NULL, "ОТПРАВЛЕНО", "", MB_ICONEXCLAMATION | MB_OK);
	}
	else 
		MessageBox(NULL, "ОШИБКА ОТПРАВКИ", "", MB_ICONEXCLAMATION | MB_OK);

	send_SSLsocket(ssl, "QUIT");

	closesocket(sock);
	SSL_shutdown(ssl);

	return 0;
}

unsigned _stdcall LoadFileAsync(void *lpparameter)
{
	char *filename = (char *)lpparameter;

	FILEstruct file = { 0 };

	HANDLE hExistingFile = CreateFile((char*)filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);

	if (INVALID_HANDLE_VALUE == hExistingFile) // не удалось открыть файл
	{
		return FALSE;
	} // if

	LARGE_INTEGER size;
	BOOL bRet = GetFileSizeEx(hExistingFile, &size);

	if ((FALSE != bRet) && (size.LowPart > 0))
	{
		file.fileContent = new char[size.QuadPart];

		// начинаем асинхронное чтение данных из файла
		bRet = ReadFile(hExistingFile, file.fileContent, size.QuadPart, NULL, NULL);

		if (FALSE == bRet) // возникла ошибка
		{
			// освобождаем выделенную память
			delete[] file.fileContent;
		} // if
		else
		{
			StringCchCopyA(file.fileName, _countof(file.fileName),(char*) filename);
			file.size = size;
			EnterCriticalSection(&c_section);
			files.push_back(file);
			LeaveCriticalSection(&c_section);
		}
		
	} // if

	return 0;
}

BOOL ReadAsync(HANDLE hFile, char *lpBuffer, DWORD dwOffset, DWORD dwSize, LPOVERLAPPED lpOverlapped)
{
	ZeroMemory(lpOverlapped, sizeof(OVERLAPPED));

	lpOverlapped->Offset = dwOffset;
	lpOverlapped->hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

	// начинаем асинхронную операцию чтения данных из файла
	BOOL bRet = ReadFile(hFile, lpBuffer, dwSize, NULL, lpOverlapped);

	if (FALSE == bRet && ERROR_IO_PENDING != GetLastError())
	{
		CloseHandle(lpOverlapped->hEvent), lpOverlapped->hEvent = NULL;
		return FALSE;
	} // if

	return TRUE;
} // ReadAsync

//BOOL OpenFileAsync(const char *szFileName)
//{
//	// открываем существующий файл для чтения и записи
//	HANDLE hExistingFile = CreateFile(szFileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
//
//	if (INVALID_HANDLE_VALUE == hExistingFile) // не удалось открыть файл
//	{
//		return FALSE;
//	} // if
//
//	HANDLE hFile = hExistingFile;
//	// определяем размер файла 
//	LARGE_INTEGER size;
//	BOOL bRet = GetFileSizeEx(hFile, &size);
//
//	if ((FALSE != bRet) && (size.LowPart > 0))
//	{
//		// выделяем память для буфера, в который будет считываться данные из файла
//		lpszBufferText = new char[size.QuadPart];
//
//		// начинаем асинхронное чтение данных из файла
//		bRet = ReadAsync(hFile, lpszBufferText, 0, size.LowPart, &_oRead);
//
//		if (FALSE == bRet) // возникла ошибка
//		{
//			// освобождаем выделенную память
//			delete[] lpszBufferText, lpszBufferText = NULL;
//		} // if
//	} // if
//
//	return bRet;
//} // OpenFileAsync

void OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
{

	switch (id)
	{
		case ID_SEND :
		{
			HANDLE sendThread = (HANDLE)_beginthreadex(NULL, 0, SendMail, NULL, 0, NULL);
			CloseHandle(sendThread);
		}
		break;

		case ID_FILE :
		{
			files.capacity();

			OPENFILENAME ofn;       
			char szFile[260] = { 0 };       

			// Initialize OPENFILENAME
			ZeroMemory(&ofn, sizeof(ofn));
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
				
				HANDLE hp = (HANDLE)_beginthreadex(NULL, 0, LoadFileAsync,(char *)(szFile), 0, NULL);
				//WaitForSingleObject(hp, INFINITE);
				CloseHandle(hp);

				//if (OpenFileAsync(szFile) != FALSE) // открываем файл
				//{
				//	
				//}
			}
						
			/*WIN32_FIND_DATA fd;
			HANDLE hFind = FindFirstFile("1.jpg", &fd);

			HANDLE hFile = CreateFile(fd.cFileName, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

			ULARGE_INTEGER size = { fd.nFileSizeLow + fd.nFileSizeHigh };
			char *buffer = new char[size.QuadPart];

			ReadFile(hFile, buffer, size.QuadPart, NULL, NULL);
	

			file = base64_encode(reinterpret_cast<const unsigned char*>(buffer), size.QuadPart);

			delete[] buffer;
			CloseHandle(hFile);
			FindClose(hFind);*/
		}
		break;

		case ID_ADDRCPT: //добавить  запись
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
	//  дескриптор окна редактируемого поля
	HWND hwndEdit = GetDlgItem(hwnd, IDC_EDIT1);
	//  максимальная длина текста в редактируемом поле
	Edit_LimitText(hwndEdit, _countof(szBuffer) - 1);
	return TRUE;
}

void Dialog_OnClose(HWND hwnd)
{
	if (hwnd == hDlg)
	{
		// уничтожаем немодальное диалоговое окно
		DestroyWindow(hwnd);
	}
	else
	{
		// завершаем работу модального диалогового окна
		EndDialog(hwnd, IDCLOSE);
	}
}

void Dialog_OnCommand(HWND hWnd, int id, HWND hwndCtl, UINT codeNotify)
{

	switch (id)
	{
	case IDOK:
	{
		// получает содержимое редактируемого поля
		int cch = GetDlgItemText(hWnd, IDC_EDIT1, szBuffer, _countof(szBuffer));

		if (0 == cch) // в редактируемого поле нет текста
		{
			// получает дескриптор окна редактируемого поля
			HWND hwndEdit = GetDlgItem(hWnd, IDC_EDIT1);

			EDITBALLOONTIP ebt = { sizeof(EDITBALLOONTIP) };

			ebt.pszTitle = L"Пусто";
			ebt.pszText = L"Укажите название новой записи";
			ebt.ttiIcon = TTI_WARNING;
			Edit_ShowBalloonTip(hwndEdit, &ebt);
		} // if
		else if (hWnd == hDlg)
		{
			// очищает редактируемое поле
			SetDlgItemText(hWnd, IDC_EDIT1, NULL);
			// отправляет окну-владельцу сообщение о том, что нужно добавить запись
			SendMessage(hwnd, WM_ADDITEM, 0, 0);
		} // if
		else
		{
			// завершает работу модального диалогового окна
			EndDialog(hWnd, IDOK);
		} // else
	} // if
	break;

	case IDCANCEL: // нажата кнопка "Отмена"
		if (hWnd == hDlg)
		{
			// уничтожает немодальное диалоговое окно
			DestroyWindow(hWnd);
		} // if
		else
		{
			// завершает работу модального диалогового окна
			EndDialog(hWnd, IDCANCEL);
		} // else
		break;
	} // switch
} // Dialog_OnCommand

void OnAddItem(HWND hwnd, WPARAM wParam)
{
	HWND hwndCtl = GetDlgItem(hwnd, ID_RECIPIENTS);
	// добавляем новый элемент в список
	int iItem = ComboBox_AddItemData(hwndCtl, szBuffer);
	// выделяем новый элемент
	ComboBox_SetCurSel(hwndCtl, iItem);
} // OnAddItem

//MIME-Version:1.0
//Content-Type:text/html
//Content-Disposition:inline
//<html>
//<body>
//<strong><i></i></strong>
//</body>
//</html>
//
//
//для файлов
//
//MIME - Version: 1.0
//Content - Type : application / octet - stream
//Content - Transfer - Encoding : base64
//Content - Disposition : attachment; filename = 1.jpeg;



// общее
//MIME - Version:1.0
//Content - Type : multipart / mixed; boundary = frontier
//
//--frontier
//Content - Type: text / html
//
//<html>
//<body>
//<strong><i>< / i>< / strong>
//< / body>
//< / html>
//
//--frontier
//Content - Type : application / octet - stream
//Content - Transfer - Encoding : base64
//Content - Disposition : attachment; filename = 1.jpg;
