#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>

#define MAX_THREADS 5

typedef struct
{
	pthread_t thread;
	int client_fd;
	struct sockaddr_in client_addr;
} ThreadInfo;

typedef struct
{
	ThreadInfo *threads;
	int count;
	int capacity;
	pthread_mutex_t mutex;
} ThreadPool;

void *handle_client(void *arg);
void *event_loop(void *arg);
void initialize_thread_pool(ThreadPool *pool, int capacity);
void cleanup_thread_pool(ThreadPool *pool);
void handle_new_connections(int server_fd, ThreadPool *thread_pool);
fd_set master_set;
int max_fd;

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

	max_fd = server_fd;

	int connection_backlog = 5;
	if (listen(server_fd, connection_backlog) != 0)
	{
		perror("listen");
		exit(EXIT_FAILURE);
	}

	// Create a thread pool and start the event loop
	pthread_t event_loop_thread;
	pthread_create(&event_loop_thread, NULL, event_loop, &server_fd);

	// Wait for the event loop thread to complete
	pthread_join(event_loop_thread, NULL);

	// Close the server socket
	close(server_fd);

	return 0;
}

void *handle_client(void *arg)
{
	ThreadInfo *thread_info = (ThreadInfo *)arg;
	int client_fd = thread_info->client_fd;

	char buffer[1024];
	int bytes_read;

	while (1)
	{

		bytes_read = recv(client_fd, buffer, sizeof(buffer), 0);
		printf("Received %d bytes\n", bytes_read);
		if (bytes_read <= 0)
		{
			// Connection closed or error
			break;
		}

		char *response = "+OK\r\n";

		send(client_fd, response, strlen(response), 0);
	}
	printf("Closing connection\n");
	close(client_fd);
	return NULL;
}

void *event_loop(void *arg)
{
	int server_fd = *((int *)arg);
	ThreadPool thread_pool;
	initialize_thread_pool(&thread_pool, MAX_THREADS);

	FD_ZERO(&master_set);
	FD_SET(server_fd, &master_set);

	while (1)
	{
		fd_set read_fds = master_set;

		int activity = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
		if (activity == -1)
		{
			perror("select");
			exit(EXIT_FAILURE);
		}

		// Handle new connections
		if (FD_ISSET(server_fd, &read_fds))
		{
			handle_new_connections(server_fd, &thread_pool);
		}

		// Handle data on existing connections
		for (int i = 0; i < thread_pool.count; i++)
		{
			int client_fd = thread_pool.threads[i].client_fd;
			if (FD_ISSET(client_fd, &read_fds))
			{
				// Create a new thread to handle the client
				pthread_create(&thread_pool.threads[i].thread, NULL, handle_client, &thread_pool.threads[i]);
			}
		}

		// Clean up finished threads
		for (int i = 0; i < thread_pool.count; i++)
		{
			pthread_join(thread_pool.threads[i].thread, NULL);
		}

		// Separate loop for cleaning up closed connections
		for (int i = 0; i < thread_pool.count; i++)
		{
			int client_fd = thread_pool.threads[i].client_fd;
			if (FD_ISSET(client_fd, &read_fds))
			{
				close(client_fd);
				FD_CLR(client_fd, &master_set);
			}
		}
	}

	cleanup_thread_pool(&thread_pool);
	return NULL;
}

void initialize_thread_pool(ThreadPool *pool, int capacity)
{
	printf("Initializing thread pool with capacity %d\n", capacity);
	pool->threads = (ThreadInfo *)malloc(sizeof(ThreadInfo) * capacity);
	pool->count = 0;
	pool->capacity = capacity;
	pthread_mutex_init(&pool->mutex, NULL);
}

void cleanup_thread_pool(ThreadPool *pool)
{
	free(pool->threads);
	pthread_mutex_destroy(&pool->mutex);
}

void handle_new_connections(int server_fd, ThreadPool *thread_pool)
{
	int client_fd = accept(server_fd, NULL, NULL);
	if (client_fd > 0)
	{
		// Add the new client socket to the master set
		FD_SET(client_fd, &master_set);

		// Update max_fd if needed
		if (client_fd > max_fd)
		{
			max_fd = client_fd;
		}

		// Add the new client socket to the thread pool
		pthread_mutex_lock(&thread_pool->mutex);
		thread_pool->threads[thread_pool->count].client_fd = client_fd;
		pthread_mutex_unlock(&thread_pool->mutex);

		// Increment the thread pool count
		thread_pool->count++;
	}
}