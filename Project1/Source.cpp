#define WIN32_LEAN_AND_MEAN
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <iostream>
#include <tchar.h>
#include <strsafe.h>
#include <WindowsX.h>
#include <CommCtrl.h>
#include <winsock2.h>
#include <process.h>

#include <openssl/ssl.h>
#include <openssl/err.h>
#pragma comment(lib, "ssleay32.lib")
#pragma comment(lib, "libeay32.lib")

#pragma comment(lib, "ws2_32.lib")

WSADATA ws;
SOCKET s;

char text[130];
char *recvdData;

SSL *ssl; /*Socket secured*/
SSL_CTX *ctx;

char codeBuf[3];
char RecvBuf[512];
int sock;

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

int send_SSLsocket(const char *buf,int num)
{
	int size = num;
	int sended = 0;
	while (size > 0)
	{
		sended = SSL_write(ssl, buf, size);
		if (sended < 0)
			return SOCKET_ERROR;
		buf += sended;
		size -= sended;
	}
	return size;
}

//int SSL_read_all(char* buf, int num)
//{
//	int size = num;
//	while (size > 0)
//	{
//		int r = SSL_read(ssl, buf, size);
//		if (r <= 0)
//			return SOCKET_ERROR;
//		buf += r;
//		size -= r;
//	}
//
//	for (int i = 0; i < 3; i++)
//		codeBuf[i] = buf[i];
//
//	return num;
//}

//void read_SSLsocket()
//{
//
//	int size_recv, total_size = 0;
//	char chunk[BUFSIZ];
//	char recvbuf[BUFSIZ] = "";
//
//	while (1)
//	{
//		memset(chunk, 0, BUFSIZ);  //clear the variable
//		if ((size_recv = SSL_read(ssl, chunk, BUFSIZ)) < 0)
//		{
//			break;
//		}
//		else if (size_recv == 0) break;
//		else
//		{
//			total_size += size_recv;
//			StringCchCat(recvbuf, BUFSIZ, chunk);
//		}
//	}
//
//	//recvdData = recvbuf;
//	_tcscpy_s(RecvBuf, recvbuf);
//	for (int i = 0; i < 3; i++)
//		codeBuf[i] = RecvBuf[i];
//
//}

int SSL_read_all(void* buf, int num)
{
	char* ptr = reinterpret_cast<char*>(buf);
	int read_bytes = 0;
	while (read_bytes < num)
	{
		int r = SSL_read(ssl, ptr + read_bytes, num - read_bytes);
		if (r <= 0)
			return r;
		read_bytes += r;
	}
	return read_bytes;
}

int create_socket() {
	int sockfd;
	char hostname[256] = "smtp.gmail.com";
	char    portnum[6] = "465";
	char      proto[6] = "";
	char      *tmp_ptr = NULL;
	int           port ;
	struct hostent *host;
	struct sockaddr_in dest_addr;


	port = atoi(portnum);

	if ((host = gethostbyname(hostname)) == NULL) {
		printf( "Error: Cannot resolve hostname .\n");
		abort();
	}

	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	dest_addr.sin_family = AF_INET;
	dest_addr.sin_port = htons(port);
	dest_addr.sin_addr.s_addr = *(long*)(host->h_addr);
memset(&(dest_addr.sin_zero), '\0', 8);

	tmp_ptr = inet_ntoa(dest_addr.sin_addr);

	if (connect(sockfd, (struct sockaddr *) &dest_addr,
		sizeof(struct sockaddr)) == -1) {
		printf("Error: Cannot connect to host %s [%s] on port .\n",hostname, tmp_ptr, port);
	}

	return sockfd;
}

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

int main()
{
	/*HANDLE hp;
	hp = (HANDLE)_beginthreadex(NULL, 0, SendMail, NULL, 0, NULL);

	WaitForSingleObject(hp, INFINITE);
*/

	sockaddr_in addr;
	HOSTENT *d_addr;

	setlocale(LC_ALL, "");
	WSAStartup(MAKEWORD(2, 2), &ws);
	s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

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

	//DWORD timeout = 1000 * 0.3;
	//setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

	StringCbCopyA(text, sizeof(text), "EHLO smtp.gmail.com\r\n");
	sendn(s, text, sizeof(text));
	recvn(s, text, sizeof(text));
	printf("%s\n", text);
	printf("ehlo---------------------------------\n");

	memset(&text[0], 0, sizeof(text));
	StringCbCopyA(text, sizeof(text), "STARTTLS\r\n");
	sendn(s, text, sizeof(text));
	recvn(s, text, sizeof(text));
	printf("%s\n", text);
	printf("starttls---------------------------------\n");


	OpenSSL_add_all_algorithms();
	ERR_load_BIO_strings();
	ERR_load_crypto_strings();
	SSL_load_error_strings();


	if (SSL_library_init() < 0)
		printf("Could not initialize the OpenSSL library !\n");

	const SSL_METHOD *method = TLSv1_2_client_method();

	if ((ctx = SSL_CTX_new(method)) == NULL)
		printf("Unable to create a new SSL context structure.\n");

	//SSL_CTX_set_options(ctx, );
	
	ssl = SSL_new(ctx);
	int sock = create_socket();

	if (sock != 0)
		printf("Successfully made the TCP connection to:.\n", "smtp.gmail.com");

	SSL_set_fd(ssl, sock); //Associate socket to SLL
	//SSL_CTX_set_default_verify_paths(ctx);

	if (SSL_connect(ssl) != 1)
		printf("Error: Could not build a SSL session to: .\n");
	else
		printf("Successfully enabled SSL/TLS session to: .\n");

	int ret = 0;

	memset(&text[0], 0, sizeof(text));
	StringCchCopyA(text, _countof(text), "EHLO smtp.gmail.com\r\n");
	ret = send_SSLsocket(text, sizeof(text));
	memset(&text[0], 0, sizeof(text));
	SSL_read_all(text,sizeof(text));
	printf("%c%c%c\n", text[0],text[1],text[2]);
	printf("1---------------------------------\n");

	memset(&text[0], 0, sizeof(text));
	StringCchCopyA(text, _countof(text), "AUTH LOGIN\r\n");
	memset(&text[0], 0, sizeof(text));
	ret = send_SSLsocket(text, sizeof(text));
	SSL_read_all(text, sizeof(text));
	printf("%s\n", text);
	printf("3---------------------------------\n");

	std::string login64 = "artem3331114@gmail.com";

	login64 = base64_encode(reinterpret_cast<const unsigned char*>(login64.c_str()), login64.length());
	//StringCchCopyA(text, login64.length(), login64.c_str());
	
	//Authstring = Authstring + login64+"\r\n";

	//memcpy(text, login64.c_str(), login64.length());
	
	//StringCbCopyA(text, login64.size(), login64.c_str());
	login64 = login64 + "\r\n";
	ret = send_SSLsocket(login64.c_str(), login64.size());
	memset(&text[0], 0, sizeof(text));
	SSL_read_all(text, sizeof(text));
	printf("%s\n", text);
	printf("4---------------------------------\n");


	std::string pass64 = "ljnfbgfyfz2226";
	pass64 = base64_encode(reinterpret_cast<const unsigned char*>(login64.c_str()), login64.length());
	pass64 = pass64 + "\r\n";
	pass64.c_str();

	ret = send_SSLsocket(pass64.c_str(), pass64.size());
	memset(&text[0], 0, sizeof(text));
	SSL_read_all(text, sizeof(text));
	printf("%s\n", text);
	printf("5---------------------------------\n");

	////StringCchCopyA(text, _countof(text), "ljnfbgfyfz2226\r\n");
	//ret = send_SSLsocket("ljnfbgfyfz2226\r\n");
	//SSL_read_all(text, sizeof(text));
	//printf("%s\n", codeBuf);

	//StringCchCopyA(text, _countof(text), "MAIL FROM: artem3331114@gmail.com\r\n");
	//ret = send_SSLsocket("MAIL FROM: artem3331114@gmail.com\r\n");
	//SSL_read_all(text, sizeof(text));
	//printf("%s\n", codeBuf);

	//StringCchCopyA(text, _countof(text), "RCPT TO: artem_pochta_314@mail.ru\r\n");
	//ret = send_SSLsocket("RCPT TO : artem_pochta_314@mail.ru\r\n");
	//SSL_read_all(text, sizeof(text));
	//printf("%s\n", codeBuf);

	//StringCchCopyA(text, _countof(text), "DATA\r\n");
	//ret = send_SSLsocket("DATA\r\n");
	//SSL_read_all(text, sizeof(text));
	//printf("%s\n", codeBuf);

	//StringCchCopyA(text, _countof(text), "SUBJECT: test\r\n");
	//ret = send_SSLsocket("SUBJECT: test\r\n");
	////read_SSLsocket();
	////printf("%s\n", codeBuf);

	//StringCchCopyA(text, _countof(text), "Hi!\nIt is a message for you\r\n");
	//ret = send_SSLsocket("Hi!\nIt is a message for you\r\n");
	////read_SSLsocket();
	////printf("%s\n", codeBuf);

	//StringCchCopyA(text, _countof(text), "\r\n.\r\n");
	//ret = send_SSLsocket("\r\n.\r\n");
	////read_SSLsocket();
	////printf("%s\n", codeBuf);

	//StringCchCopyA(text, _countof(text), "QUIT");
	//ret = send_SSLsocket("QUIT");
	//read_SSLsocket();
	//printf("%s\n", codeBuf);

	if (ret == SOCKET_ERROR)
	{
		printf(TEXT("error\n"));
	}
	else if (ret == 0)
		printf(TEXT("ok\n"));


	closesocket(s);
	WSACleanup();

	//BIO_printf(outbio, "Finished SSL/TLS connection with server: %s.\n", dest_url);

	printf("that`s all");

	return 0;
}