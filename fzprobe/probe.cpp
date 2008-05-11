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
#include <signal.h>
#include <stdlib.h>
#include "permissions.h"
#include "log.h"
#include "instance.h"

// Number of currently active connections
static int active = 0;

static unsigned int idcounter = 0;

int create_socket(unsigned int port)
{
	log.Log("Creating socket on port %d\n", port);
	int sock = socket(PF_INET, SOCK_STREAM, 0);
	if (sock == -1)
	{
		log.Log("Error creating socket: %d\n", errno);
		return -1;
	}

	struct sockaddr_in addr = {0};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = INADDR_ANY;
	
	if (bind(sock, (sockaddr *)&addr, sizeof(addr)) < 0)
	{
		close(sock);
		log.Log("Error binding socket: %d\n", errno);
		return -1;
	}

	if (listen(sock, 5) < 0)
	{
		close(sock);
		log.Log("Error listening: %d\n", errno);
		return -1;
	}

	if (fcntl(sock, F_SETFL, fcntl(sock, F_GETFL) | O_NONBLOCK) < 0)
	{
		close(sock);
		log.Log("fcntl failed: %d\n", errno);
		return -1;
	}

	return sock;
}

bool accept_and_spawn(int sock)
{
	sockaddr_in addr;
	socklen_t len = sizeof(addr);
	int fd = accept(sock, (sockaddr*)&addr, &len);
	if (fd == -1)
	{
		log.Log("Error accepting: %d\n", errno);
		return false;
	}
	else
	{
		unsigned int ip = addr.sin_addr.s_addr;
		char realIP[100];
		sprintf(realIP, "%d.%d.%d.%d", ip % 256, (ip >> 8) % 256, (ip >> 16) % 256, ip >> 24);

		log.Log("% 4d: Successful accept from %s\n", idcounter, realIP);
	}

	CInstance *instance = new CInstance(idcounter, fd, addr);
	++idcounter %= 10000;

	if (!instance->Run())
		return false;
	
	return true;
}

bool quit = false;

void sigterm(int signal)
{
	quit = true;	
}

void sigusr1(int signal)
{
	log.Reopen();
}

int main()
{
	log.Init();

	// Opening logfile
	if (!log.Open())
	{
		log.Log("Failed to open logfile, error %d\n", errno);
		return 1;
	}

	signal(SIGTERM, sigterm);
	signal(SIGINT, sigterm);
	signal(SIGUSR1, sigusr1);

	// Creating socket
	int sock = create_socket(is_root() ? 21 : 14148);
	if (sock == -1)
		return 1;

	time_t cur_time = time(0);
	struct tm *cur_time_tm = gmtime(&cur_time);
	char buffer[50];
	strftime(buffer, 50, "fzprobe started %F %H:%M\n", cur_time_tm);
	log.Log(buffer);

	if (is_root() && !drop_root())
	{
		close(sock);
		return 1;
	}

	srand(time(0));

	while (!quit)
	{
		fd_set set;
		FD_ZERO(&set);
		FD_SET(sock, &set);
		timeval tv = {0};
		tv.tv_sec = 1;
		if (select(sock + 1, &set, 0, 0, &tv) == -1)
		{
			if (errno == EINTR)
				continue;
			if (!quit)
				log.Log("select failed: %d\n", errno);
			break;
		}

		if (FD_ISSET(sock, &set))
			accept_and_spawn(sock);
	}

	shutdown(sock, SHUT_RDWR);
	close(sock);

	log.Log("Goodbye\n");
	
	return 0;
}

