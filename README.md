# Event Loop in C
This is a multithreaded server event-loop based application written in C. It uses the POSIX threads (pthreads) library to handle multiple client connections concurrently.

## Concepts

### Sockets

A socket is one endpoint of a two-way communication link between two programs running on the network. In this application, we use the `socket` function to create a socket.

### Threads

A thread is the smallest unit of processing that can be performed in an OS. In this application, we use the pthreads library to create multiple threads - each handling a separate client connection.


### Thread Pool

A thread pool is a group of pre-instantiated, idle threads which stand ready to be given work. These are preferred over instantiating new threads for each task when there is a large number of short tasks to be done rather than a small number of long ones. This prevents having to incur the overhead of creating a thread many times.

### Functioning
The server application works as follows:

* The server creates a socket and binds it to a specific port number. Clients can connect to the server using this port number.

* The server listens for incoming client connections. When a client connects, the server accepts the connection, creates a new thread, and adds it to the thread pool.

* Each thread in the thread pool waits for data from its client. When data arrives, the thread reads the data and processes it.

* The server continues to accept new connections and process data from existing connections until it is stopped.

### Functions

* `main`: The main function of the application. It creates the server socket and starts the event loop.

* `initialize_thread_pool`: Initializes the thread pool with a specified capacity.

* `cleanup_thread_pool`: Cleans up the resources used by the thread pool.

* `handle_new_connections`: Accepts new client connections, creates new threads for each connection, and adds them to the thread pool.

* `event_loop`: The main event loop of the server. It uses the select function to monitor the server socket and all client sockets for activity.

* `handle_client`: The function that each thread runs. It reads data from the client and processes it.

## Building and Running

To run the server application, use the following command:
```bash
./spawn_server.sh
```
The server will start and listen for client connections on port 3000.

To create client connections, use the following command:
```bash
./test_server.sh
```


