#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>

#define MAX_THREADS 5

typedef struct
{
	pthread_t thread;
	int is_active;
	int client_fd;
	struct sockaddr_in client_addr;
	int client_addr_len;
} ThreadInfo;

void *handle_client(void *arg)
{
	printf("Handling client\n");
	ThreadInfo *thread_info = (ThreadInfo *)arg;
	int client_fd = thread_info->client_fd;

	int is_connected = 1;

	while (is_connected)
	{
		char request[1024];
		printf("Waiting for a request...\n");
		int byte_read = read(client_fd, request, sizeof(request) - 1);

		printf("byte_read: %d\n", byte_read);
		if (byte_read <= 0)
		{
			is_connected = 0;
			break;
		}
		request[byte_read] = '\0';

		printf("Request: %s\n", request);

		char response[1024] = "+PONG\r\n";
		write(client_fd, response, strlen(response));
	}

	printf("Client disconnected\n");
	close(client_fd);
	return NULL;
}

void *event_loop(void *arg)
{
	int server_fd = *((int *)arg);
	fd_set read_fds;
	int max_fd = server_fd;

	ThreadInfo thread_info[MAX_THREADS] = {0};

	while (1)
	{
		FD_ZERO(&read_fds);
		FD_SET(server_fd, &read_fds);

		for (int i = 0; i < MAX_THREADS; i++)
		{
			if (thread_info[i].is_active)
			{
				FD_SET(thread_info[i].client_fd, &read_fds);
				if (thread_info[i].client_fd > max_fd)
				{
					max_fd = thread_info[i].client_fd;
				}
			}
		}

		int activity = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
		if ((activity < 0) && (errno != EINTR))
		{
			perror("select");
			exit(EXIT_FAILURE);
		}

		if (FD_ISSET(server_fd, &read_fds))
		{
			socklen_t addr_len = sizeof(thread_info[0].client_addr);
			int client_fd = accept(server_fd, (struct sockaddr *)&thread_info[0].client_addr, &addr_len);
			if (client_fd < 0)
			{
				perror("accept");
				exit(EXIT_FAILURE);
			}

			for (int i = 0; i < MAX_THREADS; i++)
			{
				if (!thread_info[i].is_active)
				{
					thread_info[i].is_active = 1;
					thread_info[i].client_fd = client_fd;
					thread_info[i].client_addr = thread_info[0].client_addr;
					thread_info[i].client_addr_len = sizeof(thread_info[0].client_addr);
					printf("New connection, assigned to thread %d\n", i);
					pthread_create(&thread_info[i].thread, NULL, handle_client, &thread_info[i]);
					break;
				}
			}
		}

		for (int i = 0; i < MAX_THREADS; i++)
		{
			if (thread_info[i].is_active && FD_ISSET(thread_info[i].client_fd, &read_fds))
			{
				pthread_join(thread_info[i].thread, NULL); // Wait for thread to finish
				printf("Handling client in thread %d completed\n", i);
				thread_info[i].is_active = 0;
				thread_info[i].client_fd = 0;
			}
		}
	}
}

int main()
{
	setbuf(stdout, NULL);

	printf("Logs from your program will appear here!\n");

	int server_fd;
	struct sockaddr_in serv_addr = {
		.sin_family = AF_INET,
		.sin_port = htons(3000),
		.sin_addr = {htonl(INADDR_ANY)},
	};

	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd == -1)
	{
		perror("socket");
		exit(EXIT_FAILURE);
	}

	int reuse = 1;
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) < 0)
	{
		perror("setsockopt");
		exit(EXIT_FAILURE);
	}

	if (bind(server_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) != 0)
	{
		perror("bind");
		exit(EXIT_FAILURE);
	}

	int connection_backlog = 5;
	if (listen(server_fd, connection_backlog) != 0)
	{
		perror("listen");
		exit(EXIT_FAILURE);
	}

	pthread_t event_loop_thread;
	pthread_create(&event_loop_thread, NULL, event_loop, &server_fd);

	pthread_join(event_loop_thread, NULL);

	close(server_fd);

	return 0;
}
