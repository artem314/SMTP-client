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
SSL *ssl; /*Socket secured*/
SSL_CTX *ctx;

char text[512];
char RecvBuf[1024];

char codeBuf[3];
int sock;

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

int read_socket(SOCKET sock)
{
	int size_recv, total_size = 0;
	char chunk[BUFSIZ];
	char recvbuf[BUFSIZ] = "";

	while (1)
	{
		memset(chunk, 0, BUFSIZ);  //clear the variable
		if ((size_recv = recv(sock, chunk, BUFSIZ, 0)) < 0)
		{
			break;
		}
		else
		{
			total_size += size_recv;
			StringCchCat(recvbuf, BUFSIZ, chunk);
		}
	}
	_tcscpy_s(RecvBuf, recvbuf);

	return total_size;
}

int send_SSLsocket(const char *buf)
{
	int all = strlen(buf);

	int sended = 0;
	while (all > 0)
	{
		sended = SSL_write(ssl, buf, strlen(buf));
		if (sended < 0)
			return SOCKET_ERROR;
		buf += sended;
		all -= sended;
	}
	return all;
}

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

void read_SSLsocket()
{
	int size_recv, total_size = 0;
	char chunk[BUFSIZ];
	char recvbuf[BUFSIZ] = "";
	while (1)
	{
		memset(chunk, 0, BUFSIZ);  //clear the variable
		if ((size_recv = SSL_read(ssl, chunk, BUFSIZ)) < 0)
		{
			break;
		}
		else if (size_recv == 0) break;
		else
		{
			total_size += size_recv;
			StringCchCat(recvbuf, BUFSIZ, chunk);
		}
	}

	//recvdData = recvbuf;
	_tcscpy_s(RecvBuf, recvbuf);
	for (int i = 0; i < 3; i++)
		codeBuf[i] = RecvBuf[i];
}


int create_socket(int port, const char *hostname) {
	int sockfd;
	char *tmp_ptr = NULL;
	struct hostent *host;
	struct sockaddr_in dest_addr;

	if ((host = gethostbyname(hostname)) == NULL) {
		printf("Error: Cannot resolve hostname .\n");
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
		printf("Error: Cannot connect to host %s [%s] on port .\n", hostname, tmp_ptr, port);
	}

	return sockfd;
}

int send_socket(SOCKET sock,const char* buf)
{
	int all = strlen(buf);
	DWORD dwCount;

	int sended = 0;
	while (all > 0)
	{
		sended = send(sock, buf, all, 0);
		if (sended < 0)
			return SOCKET_ERROR;
		buf += sended;
		all -= sended;
	}
	return all;
}

int main(int argc, char* argv[])
{


	setlocale(LC_ALL, "");
	WSAStartup(MAKEWORD(2, 2), &ws);

	const char hostname[] = "smtp.gmail.com";

	int sock = create_socket(587, hostname);
	if(sock!=0)
		printf("Successfully made the TCP connection to:%s\n", hostname);

	DWORD timeout = 1000 * 0.3;
	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

	OpenSSL_add_all_algorithms();

	ERR_load_crypto_strings();
	SSL_load_error_strings();

	if (SSL_library_init() < 0)
		printf("Could not initialize the OpenSSL library !\n");

	const SSL_METHOD *method = TLSv1_2_client_method();

	if ((ctx = SSL_CTX_new(method)) == NULL)
		printf("Unable to create a new SSL context structure.\n");


	StringCchCopyA(text, _countof(text), "EHLO localhost\r\n");
	sendn(sock, text, sizeof(text));
	recvn(sock, text, sizeof(text));
	printf("%s\n", text);
	printf("ehlo---------------------------------\n");


	/*std::string tls = "STARTTLS";

	tls = base64_encode(reinterpret_cast<const unsigned char*>(tls.c_str()), tls.size());
	tls = tls + "\r\n";

	sendn(sock, tls.c_str(), tls.size());
	recvn(sock, text, sizeof(text));
	printf("%s\n", text);
	printf("starttls---------------------------------\n");*/
	
	int sock1 = create_socket(587, hostname);
	if (sock1 != 0)
		printf("Successfully made the TCP connection to:%s\n", hostname);


	setsockopt(sock1, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

	ssl = SSL_new(ctx);

	SSL_set_fd(ssl, sock1);

	if (SSL_connect(ssl) != 1)
		printf("Error: Could not build a SSL session to:%s\n", hostname);
	else
		printf("Successfully enabled SSL/TLS session to: %s\n", hostname);

	//memset(&text[0], 0, sizeof(text));
	//StringCbCopyA(text, sizeof(text), "AUTH LOGIN\r\n");
	//memset(&text[0], 0, sizeof(text));
	//sendn(sock, text, sizeof(text));
	////recvn(sock, text, sizeof(text));
	send_SSLsocket("AUTH LOGIN\r\n");
	//read_socket(sock);
	read_SSLsocket();
	printf("%s\n", RecvBuf);
	printf("AUTH---------------------------------\n");


	std::string login64 = "artem3331114@gmail.com";
	login64 = base64_encode(reinterpret_cast<const unsigned char*>(login64.c_str()), login64.length());
	login64 = login64 + "\r\n";
	send_SSLsocket(login64.c_str());
	read_SSLsocket();
	printf("%s\n", RecvBuf);
	printf("login---------------------------------\n");


	std::string pass64 = "smjgzicjouudmlka";
	//std::string pass64 = "ljnfbgfyfz2226";
	pass64 = base64_encode(reinterpret_cast<const unsigned char*>(pass64.c_str()), pass64.length());
	pass64 = pass64 + "\r\n";
	pass64.c_str();

	send_SSLsocket(pass64.c_str());
	
	read_SSLsocket();
	printf("%s\n", RecvBuf);
	printf("pass---------------------------------\n");
	//printf_s("%s", tls);


	std::string from = "artem3331114@gmail.com";
	from = base64_encode(reinterpret_cast<const unsigned char*>(from.c_str()), from.length());
	from = "MAIL FROM: <" +from + ">\r\n";
	send_SSLsocket(from.c_str());
	read_SSLsocket();
	printf("%s\n", RecvBuf);


	std::string to = "artem_pochta_314@mail.ru";
	//to = base64_encode(reinterpret_cast<const unsigned char*>(to.c_str()), to.length());
	to = "RCPT TO: <" + to + ">\r\n";
	send_SSLsocket(to.c_str());
	read_SSLsocket();
	printf("%s\n", RecvBuf);

	StringCbCopyA(text, sizeof(text), "DATA\r\n");
	send_SSLsocket("DATA\r\n");
	read_SSLsocket();
	printf("%s\n", RecvBuf);

	StringCbCopyA(text, sizeof(text), "SUBJECT: test\r\n");
	send_SSLsocket("SUBJECT: test\r\n");
	read_SSLsocket();
	printf("%s\n", RecvBuf);

	StringCbCopyA(text, sizeof(text), "Hi!\nIt is a message for you\r\n");
	send_SSLsocket("Hi!\nIt is a сообщение for you\r\n");
	read_SSLsocket();
	printf("%s\n", RecvBuf);

	StringCbCopyA(text, sizeof(text), "\r\n.\r\n");
	send_SSLsocket("\r\n.\r\n");
	read_SSLsocket();
	//printf("%s\n", codeBuf);

	StringCbCopyA(text, sizeof(text), "QUIT");
	send_SSLsocket("QUIT");
	read_SSLsocket();
	printf("%s\n", RecvBuf);


	closesocket(sock);
	WSACleanup();

	//memset(&text[0], 0, sizeof(text));
	//StringCbCopyA(text, sizeof(text), "STARTTLS\r\n");
	//sendn(sock, text, sizeof(text));
	//recvn(sock, text, sizeof(text));
	//printf("%s\n", text);
	//printf("starttls---------------------------------\n");


}