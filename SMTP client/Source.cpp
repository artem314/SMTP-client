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
	// инициализируем сокеты
	if (!WSAStartup(0x101, &ws))
	{
		printf("Error in WSAStartup(...)\n");
		return 1;
	}

	// создаем сокет
	s = socket(AF_INET, SOCK_STREAM, 0);
	if (s == INVALID_SOCKET)
	{
		printf("Error in socket(...)\n");
		return 1;
	}

	// получаем адрес сервера
	d_addr = gethostbyname("smtp.gmail.com");
	if (d_addr == NULL)
	{
		printf("Error in gethostbyname(...)\n");
		return 1;
	};

	// заполняем параметры адреса
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = *((unsigned long *)d_addr->h_addr);
	addr.sin_port = htons(25);

	// устанавливаем соединение
	if (SOCKET_ERROR == (connect(s, (sockaddr *)&addr,
		sizeof(addr))))
	{
		printf("Error in connect(...)\n");
		return 1;
	}

	// ждем ответ от сервера
	recv(s, text, sizeof(text), 0);
	printf("recv - %s", text);

	// приветствуем сервер
	strcpy(text, "HELO smtp.gmail.com\r\n");
	send(s, text, strlen(text), 0);
	printf("send - %s", text);

	// ждем подтверждение от сервера
	recv(s, text, sizeof(text), 0);
	printf("recv - %s", text);

	// начинаем отправлять конверт состоящий из полей
	// MAIL FROM: и RCPT TO: После каждого поля ждем
	// подтверждение

	// сообщаем отправителя
	strcpy(text, "MAIL FROM: [email]sender@mail.ru[/email] ");
	send(s, text, strlen(text), 0);
	printf("send - %s", text);

	// ждем подтверждение от сервера
	recv(s, text, sizeof(text), 0);
	printf("recv - %s", text);

	// сообщаем получателя
	strcpy(text, "RCPT TO: [email]receiver@mtu-net.ru[/email] ");
	send(s, text, strlen(text), 0);
	printf("send - %s", text);

	// ждем подтверждение от сервера
	recv(s, text, sizeof(text), 0);
	printf("recv - %s", text);

	// подаем команду, что готовы начать передачу письма
	strcpy(text, "DATA\r\n");
	send(s, text, strlen(text), 0);
	printf("send - %s", text);

	// ждем подтверждение от сервера
	recv(s, text, sizeof(text), 0);
	printf("recv - %s", text);

	// передаем заголовок

	// от кого письмо
	strcpy(text, "FROM: [email]sender@mail.ru[/email] ");
	send(s, text, strlen(text), 0);
	printf("send - %s", text);

	// кому письмо
	strcpy(text, "TO: [email]artem3331114@gmail.com[/email] ");
	send(s, text, strlen(text), 0);
	printf("send - %s", text);

	// тема письма
	strcpy(text, "SUBJECT: test\r\n");
	send(s, text, strlen(text), 0);
	printf("send - %s", text);

	// текст письма
	strcpy(text, "Hi!\nIt is a message for you\n");
	send(s, text, strlen(text), 0);
	printf("send - %s", text);

	// говорим, что закончили
	strcpy(text, "\r\n.\r\n");
	send(s, text, strlen(text), 0);
	printf("send - %s", text);
	recv(s, text, sizeof(text), 0);
	printf("recv - %s", text);

	// прощаемся с сервером
	strcpy(text, "QUIT");
	send(s, text, strlen(text), 0);
	printf("send - %s", text);

	// закрываем сокет
	closesocket(s);

	return 0;
}