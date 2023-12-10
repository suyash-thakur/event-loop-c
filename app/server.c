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
	int client_fd;
} Task;

typedef struct
{
	pthread_t thread;
	int is_active;
	int server_fd;
	struct sockaddr_in client_addr;
	int client_addr_len;
} ThreadInfo;

void *handle_client(void *arg)
{
	Task *task = (Task *)arg;
	int client_fd = task->client_fd;

	free(task);

	int is_connected = 1;

	while (is_connected)
	{
		char request[1024];
		int byte_read = read(client_fd, request, 1024);

		if (byte_read <= 0)
		{
			is_connected = 0;
			break;
		}

		printf("Request: %s\n", request);

		char response[1024] = "+PONG\r\n";
		write(client_fd, response, strlen(response));
	}

	printf("Client disconnected\n");
	close(client_fd);
	return NULL;
}

void *thread_function(void *arg)
{
	ThreadInfo *thread_info = (ThreadInfo *)arg;

	while (thread_info->is_active)
	{
		Task *task = (Task *)malloc(sizeof(Task));

		task->client_fd = accept(thread_info->server_fd, (struct sockaddr *)&thread_info->client_addr, &thread_info->client_addr_len);

		if (task->client_fd == -1)
		{
			printf("Accept failed: %s \n", strerror(errno));
			return 1;
		}

		handle_client(task);
	}

	return NULL;
}

int main()
{
	// Disable output buffering
	setbuf(stdout, NULL);

	// You can use print statements as follows for debugging, they'll be visible when running tests.
	printf("Logs from your program will appear here!\n");

	// Uncomment this block to pass the first stage
	//
	int server_fd, client_addr_len;
	struct sockaddr_in client_addr;

	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd == -1)
	{
		printf("Socket creation failed: %s...\n", strerror(errno));
		return 1;
	}

	// Since the tester restarts your program quite often, setting REUSE_PORT
	// ensures that we don't run into 'Address already in use' errors
	int reuse = 1;
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) < 0)
	{
		printf("SO_REUSEPORT failed: %s \n", strerror(errno));
		return 1;
	}

	struct sockaddr_in serv_addr = {
		.sin_family = AF_INET,
		.sin_port = htons(3000),
		.sin_addr = {htonl(INADDR_ANY)},
	};

	if (bind(server_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) != 0)
	{
		printf("Bind failed: %s \n", strerror(errno));
		return 1;
	}

	int connection_backlog = 5;
	if (listen(server_fd, connection_backlog) != 0)
	{
		printf("Listen failed: %s \n", strerror(errno));
		return 1;
	}

	ThreadInfo thread_info[MAX_THREADS];

	for (int i = 0; i < MAX_THREADS; i++)
	{
		thread_info[i].is_active = 1;
		thread_info[i].server_fd = server_fd;
		thread_info[i].client_addr_len = sizeof(thread_info[i].client_addr);

		pthread_create(&thread_info[i].thread, NULL, thread_function, &thread_info[i]);
	}

	while (1)
	{
		int clint_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
		if (clint_fd == -1)
		{
			printf("Accept failed: %s \n", strerror(errno));
			return 1;
		}

		Task *task = (Task *)malloc(sizeof(Task));
		task->client_fd = clint_fd;

		for (int i = 0; i < MAX_THREADS; i++)
		{
			if (thread_info[i].is_active == 1)
			{
				thread_info[i].is_active = 1;
				thread_info[i].client_addr = client_addr;
				thread_info[i].client_addr_len = client_addr_len;
				pthread_create(&thread_info[i].thread, NULL, thread_function, &thread_info[i]);
				break;
			}
		}
	}
}
