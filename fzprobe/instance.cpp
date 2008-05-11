#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <time.h>
#include <stdarg.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include "log.h"

#include "instance.h"

#define BUFFERLEN 100

extern bool quit;

#define max(x, y) ((x) > (y) ? (x) : (y))

void CInstance::Close()
{
	if (datasock != -1)
	{
		shutdown(datasock, SHUT_RDWR);
		close(datasock);
		datasock = -1;
	}

	if (sock != -1)
	{
		shutdown(sock, SHUT_RDWR);
		close(sock);
		sock = -1;
	}
	printf("Connection closed");

	delete this;
	pthread_exit(0);
}

bool CInstance::Send(const char* str)
{ 
	printf(str);
	if (write(sock, str, strlen(str)) != strlen(str))
	{
		printf("Failed to write to socket: %d", errno);
		Close();
		return false;
	}
	if (write(sock, "\r\n", 2) != 2)
	{
		printf("Failed to write to socket: %d", errno);
		Close();
		return false;
	}

	return true;
}

void CInstance::DataSocketEvent()
{
	int arg;
	socklen_t len = sizeof(arg);
	if (getsockopt(datasock, SOL_SOCKET, SO_ERROR, &arg, &len) != 0)
	{
		Send("550 getsockopt failed.");
		Close();
	}
	if (arg == EINPROGRESS)
		return;

	if (arg)
	{
		Send("503 Failure of data connection.");
		Close();
	}

	char buffer[100];
	sprintf(buffer, "%d", data);
	int buflen = strlen(buffer) + 1;
	memcpy(buffer + buflen, &addr.sin_addr, 4);
	buflen += 4;
	if (write(datasock, buffer, buflen) != buflen)
	{
		Send("504 Connection closed.");
		Close();
	}

	shutdown(datasock, SHUT_RDWR);
	close(datasock);
	datasock = -1;

	Send("200 Successful");
}

void CInstance::Main()
{
	if (!Send("220 FZ router and firewall tester ready"))
		return;

	int starttime;
	char buffer[BUFFERLEN];
	int pos = 0;
	int totalstart = time(0);

	bool gotIP = false;
	bool gotPort = false;
	bool gotVersion = false;
	bool gotPreport = false;
	bool gotList = false;
	
	for (;;)
	{
		if ((time(0) - totalstart) > TOTALTIME)
		{
			Send("500 You're too slow");
			Close();
		}

		if (!pos)
			starttime = time(0);

		if ((time(0) - starttime) > TIMEOUT)
		{
			Send("500 You're too slow");
			Close();
		}
		
		timeval timeout = {0};
		timeout.tv_sec = TIMEOUT;
		
		int n = sock;
		fd_set rd;
		fd_set wrt;
		FD_ZERO(&rd);
		FD_ZERO(&wrt);
		FD_SET(sock, &rd);
		if (datasock != -1)
		{
			FD_SET(datasock, &wrt);
			n = max(n, datasock);
		}
		int res = select(n + 1, &rd, &wrt, 0, &timeout);
		if (res == -1)
		{
			if (errno == EINTR)
				continue;
			if (!quit)
				printf("Select failed");
			break;
		}
		else if (res == 0)
		{
			printf("Timeout");
			break;
		}

		if (datasock != -1 && FD_ISSET(datasock, &wrt))
		{
			DataSocketEvent();
			if (!FD_ISSET(sock, &rd))
				continue;
		}

		if (!FD_ISSET(sock, &rd))
		{
			printf("socket not ready");
			Close();
		}

		int len = read(sock, buffer + pos, BUFFERLEN - pos);
		if (len == -1)
		{
			if (errno = EAGAIN)
				continue;
			printf("recv failed");
			Close();
		}
		else if (!len)
		{
			Close();
		}

		for (int i = pos; i < pos + (len - 2); i++)
		{
			if (buffer[i] == '\r' || buffer[i] == '\n')
			{
				printf("Invalid command");
				Close();
			}
		}
		pos += len;
		
		if (buffer[pos - 1] != '\n' || buffer[pos - 2] != '\r')
		{
			if (pos >= (BUFFERLEN - 1))
			{
				printf("Command too long");
				Close();
			}
			continue;
		}

		buffer[pos - 2] = 0;
		printf("Command: %s", buffer);

		if (pos > 5 && !memcmp(buffer, "USER ", 5))
		{
			Send("331 Give any password.");
		}
		else if (pos > 5 && !memcmp(buffer, "PASS ", 5))
		{
			Send("230 logged on.");
		}
		else if (pos > 8 && !memcmp(buffer, "VERSION ", 8))
		{
			if (gotVersion)
			{
				Send("550 don't repeat yourself");
				Close();
			}

			gotVersion = true;

			Send("200 Please perform your test");
		}
		else if (pos > 5 && !memcmp(buffer, "IP ", 3))
		{
			if (gotIP)
			{
				Send("550 don't repeat yourself");
				Close();
			}
			gotIP = true;
			
			// Get real IP address
			unsigned int ip = addr.sin_addr.s_addr;
			int written = snprintf(clientip, 16, "%d.%d.%d.%d", ip % 256, (ip >> 8) % 256, (ip >> 16) % 256, ip >> 24);
			if (written < 0 || written >= 16)
			{
				Send("550 Internal probe error.");
				Close();
			}

			// Convert to our special hex format
			char realHex[100];
			for (char *p = clientip, *q = realHex; ; p++, q++)
			{
				if (!*p)
				{
					*q = 0;
					break;
				}
				
				if (*p == '.')
					*q = '-';
				else
					*q = *p - '0' + 'a';
			}
			buffer[pos - 2] = 0;

			char* incIP = buffer + 3;
			char* incHex;
			if ((incHex = strchr(incIP, ' ')))
			{
				*incHex = 0;
				incHex++;
			}
			else
			{
				Send("500 Syntax error");
				Close();
			}
			if (!strcmp(incHex, realHex))
			{
				if (!strcmp(incIP, clientip))
					Send("200 OK");
				else
					Send("501 Mismatch. Router tainted the address.");
			}
			else
			{
				char hexifiedIP[100];
				for (char *p = incIP, *q = hexifiedIP; ; p++, q++)
				{
					if (!*p)
					{
						*q = 0;
						break;
					}
					
					if (*p == '.')
						*q = '-';
					else
						*q = *p - '0' + 'a';
				}
				if (!strcmp(incHex, hexifiedIP))
				{
					sprintf(buffer, "510 Mismatch. Your IP is %s, %s", clientip, realHex);
					Send(buffer);
				}
				else
				{
					sprintf(buffer, "511 Mismatch. Tainted by router or firewall. Your IP is: %s %s", clientip, realHex);
					Send(buffer);
				}
			}
		}
		else if (pos > 5 && !memcmp(buffer, "PREP ", 5))
		{
			if (gotPreport)
			{
				Send("550 Don't repeat yourself");
				Close();
			}
			gotPreport = true;
			int port = 0;
			for (char* p = buffer + 5; *p; p++)
			{
				if (*p < '0' || *p > '9')
				{
					Send("550 Syntax error");
					Close();
				}
				port = port * 10 + *p - '0';
			}
			if (port < 1 || port > 65535)
			{
				Send("550 Syntax error, port out of range");
				Close();
			}
			dataport = port;
			data = rand() + 1;
			char buffer[100];
			sprintf(buffer, "200 Using port %d, data token %d", port, data);
			Send(buffer);
		}
		else if (pos > 5 && !memcmp(buffer, "PORT ", 5))
		{
			if (!gotIP || !gotPreport)
			{
				Send("503 Bad sequence of commands");
				Close();
			}
			if (gotPort)
			{
				Send("550 Don't repeat yourself");
				Close();
			}
			gotPort = true;

			char* strport = 0;
			unsigned char dotcount = 0;
			for (char* p = buffer + 5; *p; p++)
			{
				if ((*p < '0' || *p > '9') && *p != ',')
				{
					Send("550 Syntax error");
					Close();
				}
				if (*p == ',')
				{
					if (*p == *(p - 1))
					{
						Send("550 Syntax error");
						Close();
					}
					dotcount++;
					if (dotcount == 4)
					{
						*p = 0;
						strport = p + 1;
					}
					else
						*p = '.';
				}
			}
			if (dotcount != 5)
			{
				Send("550 Syntax error");
				Close();
			}

			if (strcmp(clientip, buffer + 5))
			{
				Send("501 IP Mismatch. Tainted by router or firewall.");
				Close();
			}
			
			int port = 0;
			for (char* p = strport; *p; p++)
			{
				if (*p == '.')
					port *= 256;
				else
					port = port - (port % 256) + (port % 256) * 10 + *p - '0';
			}
			if (port != dataport)
			{
				Send("502 Port mismatch. Tainted by router or firewall.");
				Close();
			}

			Send("200 PORT command successful");
		}
		else if (pos > 4 && !memcmp(buffer, "LIST", 4))
		{
			if (!gotPort)
			{
				Send("503 Bad sequence of commands");
				Close();
			}
			if (gotList)
			{
				Send("550 Don't repeat yourself");
				Close();
			}
			gotList = true;

			struct sockaddr_in connectaddr;
			connectaddr.sin_family = AF_INET;
			if (inet_aton(clientip, &connectaddr.sin_addr) == 0)
			{
				printf(clientip);
				Send("501 Invalid IP-address");
				Close();
			}
			connectaddr.sin_port = htons(dataport);
			
			datasock = socket(AF_INET, SOCK_STREAM, 0);
			if (datasock == -1)
			{
				Send("502 Failed to create socket");
				Close();
			}
			if (fcntl(datasock, F_SETFL, fcntl(sock, F_GETFL) | O_NONBLOCK) < 0)
			{
				Send("502 Failed to create socket");
				Close();
			}

			if (connect(datasock, (struct sockaddr *)&connectaddr, sizeof(connectaddr)) == -1 && errno != EINPROGRESS)
			{
				Send("503 Failed to establish data connection.");
				Close();
			}
			
			Send("150 opening data connection");
		}
		else if (pos > 4 && !memcmp(buffer, "PASV", 4))
		{
			Send("557 PASV not permitted, you have malicious router or firewall");
			Close();
		}
		else if (pos > 4 && !memcmp(buffer, "QUIT", 4))
		{
			Send("200 goodbye!");
			Close();
		}
		else
		{
			Send("500 invalid command, goodbye!");
			Close();	
		}

		pos = 0;

		
	}

	Close();
	
	return;
}

CInstance::CInstance(int id, int fd, sockaddr_in addr)
	: id(id), sock(fd), datasock(-1), addr(addr)
{
}

CInstance::~CInstance()
{
}

bool CInstance::Run()
{
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	pthread_t id;
	pthread_create(&id, &attr, ThreadProc, this);
}

void* CInstance::ThreadProc(void* data)
{
	((CInstance*)data)->Main(); 
	return 0;
}

void CInstance::printf(const char* fmt, ...)
{
	va_list ap;

	char* buff = new char[strlen(fmt) + 10];
	sprintf(buff, "% 4d: %s\n", id, fmt);

	va_start(ap, fmt);
	log.Log(buff, ap);
	va_end(ap);
	delete [] buff;
}

