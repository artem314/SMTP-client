#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <string>
#include <Process.h>

#include <WS2tcpip.h>
#include <winsock.h>
#include <stdio.h>
#include <conio.h>

#include <vector>
#pragma comment(lib, "Ws2_32.lib")

int sendn(SOCKET s, const char *buf, int len);
int recvn(SOCKET s, char *buf, int len);

int sendn(SOCKET s, const char *buf, int len)
{
	int size = len;
	while (size > 0)
	{
		int n = send(s, buf, size, 0);
		if (n <= 0)
		{
			if (WSAGetLastError() == WSAEINTR)
			{
				continue;
			}
			return SOCKET_ERROR;
		}
		buf += n;
		size -= n;
	}
	return len;
}
int recvn(SOCKET s, char *buf, int len)
{
	int size = len;
	while (size > 0)
	{
		int n = recv(s, buf, size, 0);
		if (n <= 0)
		{
			if (WSAGetLastError() == WSAEINTR)
			{
				continue;
			}
			return SOCKET_ERROR;
		}
		buf += n;
		size -= n;
	}
	return len;
}

SOCKET s;
WSADATA ws;
struct sockaddr_in addr;
hostent *d_addr;
char text[512];

int main(int argc, char* argv[])
{
	WIN32_FIND_DATA fd;
	HANDLE hFind = FindFirstFile("1.jpg", &fd);

	HANDLE hFile = CreateFile(fd.cFileName, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

	if (hFile != INVALID_HANDLE_VALUE)
	{
		ULARGE_INTEGER size = { fd.nFileSizeLow + fd.nFileSizeHigh };
		char *buffer = new char[size.QuadPart];

		DWORD read;

		ReadFile(hFile, buffer, size.QuadPart, &read, NULL);

		delete[] buffer;
		CloseHandle(hFile);
		FindClose(hFind);
	}
	//setlocale(LC_ALL, "");
	//WSAStartup(MAKEWORD(2, 2), &ws);
	//s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	//	d_addr = gethostbyname("aspmx.l.google.com");
	//	addr.sin_family = AF_INET;
	//	addr.sin_addr.s_addr = *((unsigned long *)d_addr->h_addr);
	//	addr.sin_port = htons(25);
	//	if (SOCKET_ERROR == (connect(s, (sockaddr *)&addr,
	//		sizeof(addr))))
	//	{
	//		printf("Error in connect(...)\n");
	//		return 1;
	//	}
	//	recv(s, text, sizeof(text), 0);
	//	printf("recv - %s", text);
	//	strcpy(text, "EHLO aspmx.l.google.com\r\n");
	//	sendn(s, text, strlen(text));
	//	recv(s, text, sizeof(text), 0);
	//	printf("recv - %s", text);
	//	/*strcpy(text, "AUTH login\r\n");
	//	sendn(s, text, strlen(text));
	//	recv(s, text, sizeof(text), 0);
	//	printf("recv - %s", text);
	//	strcpy(text, "artem3331114@gmail.com\r\n");
	//	sendn(s, text, strlen(text));
	//	recv(s, text, sizeof(text), 0);
	//	printf("recv - %s", text);
	//	strcpy(text, "ljnfbgfyfz2226\r\n");
	//	sendn(s, text, strlen(text));
	//	recv(s, text, sizeof(text), 0);
	//	printf("recv - %s", text);*/
	//	strcpy(text, "MAIL FROM: artem3331114@gmail.com\r\n");
	//	send(s, text, strlen(text),0);
	//	recv(s, text, sizeof(text), 0);
	//	printf("recv - %s", text);
	//	strcpy(text, "RCPT TO: artem_pochta_314@mail.ru\r\n");
	//	sendn(s, text, strlen(text));
	//	recv(s, text, sizeof(text), 0);
	//	printf("recv - %s", text);
	//	strcpy(text, "DATA\r\n");
	//	sendn(s, text, strlen(text));
	//	recv(s, text, sizeof(text), 0);
	//	strcpy(text, "FROM: artem3331114@gmail.com\r\n");
	//	sendn(s, text, strlen(text));
	//	strcpy(text, "TO: artem_pochta_314@mail.ru\r\n");
	//	sendn(s, text, strlen(text));
	//	strcpy(text, "SUBJECT: тема\r\n");
	//	sendn(s, text, strlen(text));
	//	strcpy(text, "сообщение\n");
	//	sendn(s, text, strlen(text));
	//	strcpy(text, "\r\n.\r\n");
	//	sendn(s, text, strlen(text));
	//	recv(s, text, sizeof(text), 0);
	//	strcpy(text, "QUIT");
	//	sendn(s, text, strlen(text));
	//	closesocket(s);
		return 0;
}

	

