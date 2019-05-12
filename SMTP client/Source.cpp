#include <Windows.h>
#include <WindowsX.h>
#include <CommCtrl.h>
#include <tchar.h>
#include <stdio.h>
#include <stdlib.h> 
#include <process.h>

#include "psapi.h"
#include "strsafe.h"

#include <stdio.h>
#pragma comment(lib, "ws2_32.lib")

WSADATA ws;
SOCKET s;
struct sockaddr_in addr;
hostent *d_addr;
char text[1024];

int main(int argc, char *argv[])
{
	// �������������� ������
	if (!WSAStartup(0x101, &ws))
	{
		printf("Error in WSAStartup(...)\n");
		return 1;
	}

	// ������� �����
	s = socket(AF_INET, SOCK_STREAM, 0);
	if (s == INVALID_SOCKET)
	{
		printf("Error in socket(...)\n");
		return 1;
	}

	// �������� ����� �������
	d_addr = gethostbyname("smtp.gmail.com");
	if (d_addr == NULL)
	{
		printf("Error in gethostbyname(...)\n");
		return 1;
	};

	// ��������� ��������� ������
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = *((unsigned long *)d_addr->h_addr);
	addr.sin_port = htons(25);

	// ������������� ����������
	if (SOCKET_ERROR == (connect(s, (sockaddr *)&addr,
		sizeof(addr))))
	{
		printf("Error in connect(...)\n");
		return 1;
	}

	// ���� ����� �� �������
	recv(s, text, sizeof(text), 0);
	printf("recv - %s", text);

	// ������������ ������
	strcpy(text, "HELO smtp.gmail.com\r\n");
	send(s, text, strlen(text), 0);
	printf("send - %s", text);

	// ���� ������������� �� �������
	recv(s, text, sizeof(text), 0);
	printf("recv - %s", text);

	// �������� ���������� ������� ��������� �� �����
	// MAIL FROM: � RCPT TO: ����� ������� ���� ����
	// �������������

	// �������� �����������
	strcpy(text, "MAIL FROM: [email]sender@mail.ru[/email] ");
	send(s, text, strlen(text), 0);
	printf("send - %s", text);

	// ���� ������������� �� �������
	recv(s, text, sizeof(text), 0);
	printf("recv - %s", text);

	// �������� ����������
	strcpy(text, "RCPT TO: [email]receiver@mtu-net.ru[/email] ");
	send(s, text, strlen(text), 0);
	printf("send - %s", text);

	// ���� ������������� �� �������
	recv(s, text, sizeof(text), 0);
	printf("recv - %s", text);

	// ������ �������, ��� ������ ������ �������� ������
	strcpy(text, "DATA\r\n");
	send(s, text, strlen(text), 0);
	printf("send - %s", text);

	// ���� ������������� �� �������
	recv(s, text, sizeof(text), 0);
	printf("recv - %s", text);

	// �������� ���������

	// �� ���� ������
	strcpy(text, "FROM: [email]sender@mail.ru[/email] ");
	send(s, text, strlen(text), 0);
	printf("send - %s", text);

	// ���� ������
	strcpy(text, "TO: [email]artem3331114@gmail.com[/email] ");
	send(s, text, strlen(text), 0);
	printf("send - %s", text);

	// ���� ������
	strcpy(text, "SUBJECT: test\r\n");
	send(s, text, strlen(text), 0);
	printf("send - %s", text);

	// ����� ������
	strcpy(text, "Hi!\nIt is a message for you\n");
	send(s, text, strlen(text), 0);
	printf("send - %s", text);

	// �������, ��� ���������
	strcpy(text, "\r\n.\r\n");
	send(s, text, strlen(text), 0);
	printf("send - %s", text);
	recv(s, text, sizeof(text), 0);
	printf("recv - %s", text);

	// ��������� � ��������
	strcpy(text, "QUIT");
	send(s, text, strlen(text), 0);
	printf("send - %s", text);

	// ��������� �����
	closesocket(s);

	return 0;
}