#include <arpa/inet.h>
#include <pthread.h>
#include <errno.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#define BUFFER_SIZE 65536

FILE *log_file = NULL;

typedef struct {
    char ** keys;
    char ** values;
    int num_headers;
}
Header;

typedef struct {
    char ** keys;
    char ** values;
    int num_params;
}
QueryParameters;

typedef struct {
    int id;
    char * method;
    char * path;
    char * path_without_query;
    char * version;
    QueryParameters * query_parameters;
    Header * headers;
    char * body;
}
HttpRequest;

typedef struct {
    int status_code;
    char * status_message;
    Header * headers;
    char * body;
}
HttpResponse;



void free_query_parameters(QueryParameters *query_parameters){
    if (query_parameters -> num_params != 0){
        for (int i = 0; i< query_parameters -> num_params ; i ++){
            free(query_parameters -> keys[i]);
            free(query_parameters -> values[i]);
        }

        free(query_parameters -> keys);
        free(query_parameters -> values);
    }

    free(query_parameters);
}

void free_headers(Header *header){
    for (int i = 0; i< header -> num_headers ; i ++){
        free(header -> keys[i]);
        free(header -> values[i]);
    }

    free(header -> keys);
    free(header -> values);

    free(header);
}

void free_request(HttpRequest * http_request) {
    free(http_request -> method);
    free(http_request -> path);
    free(http_request -> version);
    free(http_request -> path_without_query);
    free(http_request -> body);
    free_headers(http_request -> headers);
    free_query_parameters(http_request -> query_parameters);

    free(http_request);
}

void free_response(HttpResponse *http_response){
    free(http_response -> status_message);
    free(http_response -> body);
    free_headers(http_response -> headers);

    free(http_response);
}

void parse_request(char * request, HttpRequest * http_request);

void parse_first_line(char * first_line, int first_line_length, HttpRequest * http_request);

char * parse_path(QueryParameters * query_parameters, char * path);

void parse_header(char * header, int header_length, Header * http_header);

HttpResponse * response(int status_code, char * status_message, char * body);

void add_header(HttpResponse * http_response, char * key, char * value);

char * serialize_response(HttpResponse * http_response);

char * get_param(HttpRequest * http_request, char * key);

void parse_request(char * request, HttpRequest * http_request) {
    char * first_line_end = strstr(request, "\r\n");
    int first_line_length = first_line_end - request;
    char * first_line = malloc(first_line_length + 1); // free: done!
    strncpy(first_line, request, first_line_length);
    first_line[first_line_length] = '\0';


    parse_first_line(first_line, first_line_length, http_request);
    free(first_line);

    char * header_end = strstr(first_line_end + 2, "\r\n\r\n");
    int header_length = header_end - first_line_end - 2;

    char * header = malloc(header_length + 1);  // free done

    strncpy(header, first_line_end + 2, header_length);

    header[header_length] = '\0';

    http_request -> headers = malloc(sizeof(Header));   /// pending

    parse_header(header, header_length, http_request -> headers);
    free(header);


    int body_length = strlen(request) - (header_end - request) - 4;
    char * body = malloc(body_length + 1);
    strncpy(body, header_end + 4, body_length);
    body[body_length] = '\0';
    http_request -> body = body;
}

void parse_first_line(char * first_line, int first_line_length, HttpRequest * http_request) {
    char * method = strtok(first_line, " ");
    char * path = strtok(NULL, " ");
    char * version = strtok(NULL, " ");
    http_request -> method = malloc(strlen(method) + 1); // free: done!
    strcpy(http_request -> method, method);
    http_request -> path = malloc(strlen(path) + 1); // free: done!
    strcpy(http_request -> path, path);
    http_request -> version = malloc(strlen(version) + 1); // free: done!
    strcpy(http_request -> version, version);
    http_request -> query_parameters = malloc(sizeof(QueryParameters));  /// pending
    http_request -> query_parameters -> num_params = 0;
    http_request -> path_without_query = parse_path(http_request -> query_parameters, path);
}

char * parse_path(QueryParameters * query_parameters, char * path) {
    char * query_start = strstr(path, "?");
    char * path_without_query;
    if (query_start != NULL) {
        int path_without_query_length = query_start - path;
        path_without_query = malloc(path_without_query_length + 1);   /// pending
        strncpy(path_without_query, path, path_without_query_length);
        path_without_query[path_without_query_length] = '\0';

        char * query = query_start + 1;
        char * key = strtok(query, "=");
        char * value = strtok(NULL, "&");

        query_parameters -> keys = malloc(sizeof(char * ));  /// pending
        query_parameters -> values = malloc(sizeof(char * ));

        query_parameters -> keys[0] = malloc(sizeof(char) * strlen(key) + 1);
        query_parameters -> values[0] = malloc(sizeof(char) * strlen(value) + 1);
        strcpy(query_parameters -> keys[0] , key);
        strcpy(query_parameters -> values[0] , value);

        query_parameters -> num_params = 1;
        while (value != NULL) {
            key = strtok(NULL, "=");
            value = strtok(NULL, "&");

            if (key != NULL && value != NULL) {
                query_parameters -> keys = realloc(query_parameters -> keys, sizeof(char * ) * (query_parameters -> num_params + 1));
                query_parameters -> values = realloc(query_parameters -> values, sizeof(char * ) * (query_parameters -> num_params + 1));
                
                int idx = query_parameters -> num_params;
                query_parameters -> keys[idx] = malloc(sizeof(char) * strlen(key) + 1);
                query_parameters -> values[idx] = malloc(sizeof(char) * strlen(value) + 1);
                strcpy(query_parameters -> keys[idx] , key);
                strcpy(query_parameters -> values[idx], value);
                
                query_parameters -> num_params++;
            }
        }
    }
    else{
        path_without_query = malloc(strlen(path) + 1);   /// pending
        strcpy(path_without_query, path);
    }

    return path_without_query;
}

void parse_header(char * header, int header_length, Header * http_header) {
    // char * header_copy = malloc(header_length + 1);
    // strncpy(header_copy, header, header_length);
    // header_copy[header_length] = '\0';
    char * key = strtok(header, ":");
    char * value = strtok(NULL, "\r");
    http_header -> keys = malloc(sizeof(char * ));
    http_header -> values = malloc(sizeof(char * ));

    
    http_header -> keys[0] = malloc(sizeof(char) * strlen(key) + 1);
    http_header -> values[0] = malloc(sizeof(char) * strlen(value) + 1);
    strcpy(http_header -> keys[0] , key);
    strcpy(http_header -> values[0] , value + 1);

    http_header -> num_headers = 1;
    while (value != NULL) {
        key = strtok(NULL, ":");
        value = strtok(NULL, "\r");
        if (key != NULL && value != NULL) {
            key = key + 1;
            value = value + 1;
            http_header -> keys = realloc(http_header -> keys, sizeof(char * ) * (http_header -> num_headers + 1));
            http_header -> values = realloc(http_header -> values, sizeof(char * ) * (http_header -> num_headers + 1));
                            
            int idx = http_header -> num_headers;
            http_header -> keys[idx] = malloc(sizeof(char) * strlen(key) + 1);
            http_header -> values[idx] = malloc(sizeof(char) * strlen(value) + 1);
            strcpy(http_header -> keys[idx] , key);
            strcpy(http_header -> values[idx], value);
        
            http_header -> num_headers++;
        }
    }
}

HttpResponse * response(int status_code, char * status_message, char * body) {
    HttpResponse * http_response = malloc(sizeof(HttpResponse));
    http_response -> status_code = status_code;

    http_response -> status_message = malloc(10);
    strcpy(http_response -> status_message, status_message);
    http_response -> body = body;
    http_response -> headers = malloc(sizeof(Header));
    http_response -> headers -> num_headers = 0;
    return http_response;
}

void add_header(HttpResponse * http_response, char * key, char * value) {
    if (http_response -> headers -> num_headers == 0){
        http_response -> headers -> keys = malloc(sizeof(char * ));
        http_response -> headers -> values = malloc(sizeof(char * ));
    }
    else{
        http_response -> headers -> keys = realloc(http_response -> headers -> keys, sizeof(char * ) * (http_response -> headers -> num_headers + 1));
        http_response -> headers -> values = realloc(http_response -> headers -> values, sizeof(char * ) * (http_response -> headers -> num_headers + 1));
    }

    int idx = http_response -> headers -> num_headers;
    http_response -> headers -> keys[idx] = malloc(sizeof(char) * strlen(key) + 1);
    http_response -> headers -> values[idx] = malloc(sizeof(char) * strlen(value) + 1);
    strcpy(http_response -> headers -> keys[idx] , key);
    strcpy(http_response -> headers -> values[idx], value);

    http_response -> headers -> num_headers++;
}

char * serialize_response(HttpResponse * http_response) {
    char * status_line = malloc(100);
    sprintf(status_line, "HTTP/1.1 %d %s\r\n", http_response -> status_code, http_response -> status_message);
    char * headers = malloc(1000);
    strcpy(headers, "");
    char * header = malloc(100);
    for (int i = 0; i < http_response -> headers -> num_headers; i++) {
        sprintf(header, "%s: %s\r\n", http_response -> headers -> keys[i], http_response -> headers -> values[i]);
        strcat(headers, header);
    }
    char * body = malloc(1000);
    strcpy(body, "");
    if (http_response -> body != NULL) {
        sprintf(body, "\r\n%s", http_response -> body);
    }
    char * response = malloc(strlen(status_line) + strlen(headers) + strlen(body) + 1);
    strcpy(response, status_line);
    strcat(response, headers);
    strcat(response, body);
    free(status_line);
    free(header);
    free(headers);
    free(body);
    return response;
}

char * get_param(HttpRequest * http_request, char * key) {
    for (int i = 0; i < http_request -> query_parameters -> num_params; i++) {
        if (strcmp(http_request -> query_parameters -> keys[i], key) == 0) {
            return http_request -> query_parameters -> values[i];
        }
    }
    return NULL;
}

typedef struct {
    void * ( * worker)(void * );
    void * args;
}
Task;

typedef struct {
    Task ** tasks;
    int size;
    int start;
    int end;
    pthread_mutex_t lock;
    sem_t semaphore;
}
TaskQueue;

typedef struct {
    TaskQueue * taskQueue;
    int numThreads;
    int open;
    pthread_t * workerThreads;
}
ThreadPool;

void createTaskQueue(TaskQueue * taskQueue, int size);

int enqueueTaskQueue(TaskQueue * taskQueue, Task * task);

Task * dequeueTaskQueue(TaskQueue * taskQueue);

void createThreadPool(ThreadPool * threadPool, TaskQueue * taskQueue, int( * priority)(void * ), int numThreads, int queueSize);

void addTaskThreadPool(ThreadPool * threadPool, void * ( * function_worker)(void * ), void * args);

void * worker(void * args);

void createTaskQueue(TaskQueue * taskQueue, int size) {
    taskQueue -> tasks = (Task ** ) malloc(sizeof(Task * ) * size);
    for (size_t i = 0; i < size; i++)
        taskQueue -> tasks[i] = NULL;
    taskQueue -> start = 0;
    taskQueue -> end = 0;
    taskQueue -> size = size;
    sem_init( & taskQueue -> semaphore, 1, 0);
    // initial lock 
    pthread_mutex_init( & taskQueue -> lock, NULL);
}

int enqueueTaskQueue(TaskQueue * taskQueue, Task * task) {
    pthread_mutex_lock( & taskQueue -> lock);
    if (taskQueue -> start == (taskQueue -> end + 1) % taskQueue -> size) {
        pthread_mutex_unlock( & taskQueue -> lock);
        return 0;
    }
    taskQueue -> tasks[taskQueue -> end] = task;
    taskQueue -> end = (taskQueue -> end + 1) % taskQueue -> size;
    sem_post( & taskQueue -> semaphore);
    pthread_mutex_unlock( & taskQueue -> lock);
    return 1;
}

Task * dequeueTaskQueue(TaskQueue * taskQueue) {
    sem_wait( & taskQueue -> semaphore);
    pthread_mutex_lock( & taskQueue -> lock);
    Task * task = taskQueue -> tasks[taskQueue -> start];
    taskQueue -> tasks[taskQueue -> start] = NULL;
    taskQueue -> start = (taskQueue -> start + 1) % taskQueue -> size;
    pthread_mutex_unlock( & taskQueue -> lock);
    return task;
}

void createThreadPool(ThreadPool * threadPool, TaskQueue * taskQueue, int( * priority)(void * ), int numThreads, int queueSize) {
    threadPool -> taskQueue = taskQueue;
    threadPool -> numThreads = numThreads;
    threadPool -> open = 1;
    threadPool -> workerThreads = (pthread_t * ) malloc(sizeof(pthread_t) * numThreads);
    for (size_t i = 0; i < numThreads; i++)
        pthread_create( & threadPool -> workerThreads[i], NULL, worker, threadPool);
}

void addTaskThreadPool(ThreadPool * threadPool, void * ( * function_worker)(void * ), void * args) {
    Task * task = (Task * ) malloc(sizeof(Task));
    task -> worker = function_worker;
    task -> args = args;
    enqueueTaskQueue(threadPool -> taskQueue, task);
}

void free_task(Task * task) {
    free(task -> args);
    free(task);
}

void * worker(void * args) {
    ThreadPool * threadPool = (ThreadPool * ) args;
    while (threadPool -> open) {
        Task * task = dequeueTaskQueue(threadPool -> taskQueue);
        printf("Dequeued");
        // if (task != NULL) {
            task -> worker(task -> args);
            // free_task(task);
        // }
    }
    return NULL;
}

typedef struct {
    char * path;
    HttpResponse * ( * handler_function)(HttpRequest * );
}
Handler;

typedef struct {
    int port;
    ThreadPool * threadPool;
    Handler * handlers;
    int num_handlers;
    int open;
}
WebServer;

void create_web_server(WebServer * webServer, int port, int num_threads, int queue_size, int max_num_handlers) {
    webServer -> port = port;
    TaskQueue * taskQueue = (TaskQueue * ) malloc(sizeof(TaskQueue));
    createTaskQueue(taskQueue, queue_size);
    webServer -> threadPool = (ThreadPool * ) malloc(sizeof(ThreadPool));
    createThreadPool(webServer -> threadPool, taskQueue, NULL, num_threads, queue_size);
    webServer -> handlers = (Handler * ) malloc(sizeof(Handler) * max_num_handlers);
    webServer -> num_handlers = 0;
    webServer -> open = 1;
}

void add_new_handler(WebServer * webServer, char * path, HttpResponse * ( * handler_function)(HttpRequest * )) {
    Handler * handler = & webServer -> handlers[webServer -> num_handlers++];
    handler -> path = path;
    handler -> handler_function = handler_function;
}

typedef struct {
    int sockfd;
    struct sockaddr_in host_addr;
    int host_addrlen;
}
ConnectionDescriptor;

ConnectionDescriptor * start_web_server(WebServer * webServer) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("Cannot initialize socket.\n");
        exit(1);
    }
    printf("Socket successfully initialized\n");

    struct sockaddr_in host_addr;
    int host_addrlen = sizeof(host_addr);

    host_addr.sin_family = AF_INET;
    host_addr.sin_port = htons(webServer -> port);
    host_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sockfd, (struct sockaddr * ) & host_addr, host_addrlen) != 0) {
        perror("Cannot bind socket to address.\n");
        exit(1);
    }
    printf("Socket successfully binded\n");

    if (listen(sockfd, SOMAXCONN) != 0) {
        perror("Cannot listen for connections.\n");
        exit(1);
    }
    printf("Listening for connections\n");

    ConnectionDescriptor * connectionDescriptor = (ConnectionDescriptor * ) malloc(sizeof(ConnectionDescriptor));
    connectionDescriptor -> sockfd = sockfd;
    connectionDescriptor -> host_addr = host_addr;
    connectionDescriptor -> host_addrlen = host_addrlen;
    return connectionDescriptor;
}

typedef struct {
    int id;
    int newsockfd;
    WebServer * webServer;
}
HandleRequestArgs;

void * handle_request(void * args) {
    HandleRequestArgs * handleRequestArgs = (HandleRequestArgs * ) args;
    int id = handleRequestArgs -> id;
    int newsockfd = handleRequestArgs -> newsockfd;
    WebServer * webServer = handleRequestArgs -> webServer;

    struct timeval time_thread_dedicated;
    gettimeofday(&time_thread_dedicated, NULL);
    printf("Id: %d, Time thread dedicated: %ld\n", id, time_thread_dedicated.tv_sec * 1000000 + time_thread_dedicated.tv_usec);
    fprintf(log_file, "Id: %d, Time thread dedicated: %ld\n", id, time_thread_dedicated.tv_sec * 1000000 + time_thread_dedicated.tv_usec);
    fflush(log_file);

    char * buffer = malloc(BUFFER_SIZE);

    int valread = read(newsockfd, buffer, BUFFER_SIZE);
    if (valread < 0) {
        perror("Cannot read from socket.\n");
        return NULL;
    }

    HttpRequest * http_request = malloc(sizeof(HttpRequest));
    parse_request(buffer, http_request);
    http_request -> id = id;

    Handler * handler = NULL;
    for (int i = 0; i < webServer -> num_handlers; i++) {
        if (strncmp(webServer -> handlers[i].path, http_request -> path_without_query, strlen(webServer -> handlers[i].path)) == 0) {
            if (handler == NULL || strlen(webServer -> handlers[i].path) > strlen(handler -> path)) {
                handler = & webServer -> handlers[i];
            }
        }
    }


    HttpResponse * http_response = NULL;

    if (handler != NULL) {
        http_response = handler -> handler_function(http_request);
    } else {
        http_response = response(404, "Not Found", "");
    }

    free_request(http_request);

    char * response_string = serialize_response(http_response);

    struct timeval time_process_finished;
    gettimeofday(&time_process_finished, NULL);
    printf("Id: %d, Time thread dedicated: %ld\n", id, time_process_finished.tv_sec * 1000000 + time_process_finished.tv_usec);
    fprintf(log_file, "Id: %d, Time thread dedicated: %ld\n", id, time_process_finished.tv_sec * 1000000 + time_process_finished.tv_usec);
    fflush(log_file);

    int valwrite = write(newsockfd, response_string, strlen(response_string));
    if (valwrite < 0) {
        perror("Cannot write to socket.\n");
        return NULL;
    }

    free_response(http_response);
    close(newsockfd);
    free(buffer);
    free(response_string);
    // free(handleRequestArgs);
    return NULL;
}

void accept_connections(WebServer * webServer, ConnectionDescriptor * connectionDescriptor) {
    int id_counter = 0;

    int sockfd = connectionDescriptor -> sockfd;
    struct sockaddr_in host_addr = connectionDescriptor -> host_addr;
    int host_addrlen = connectionDescriptor -> host_addrlen;

    while (1) {
        int newsockfd = accept(sockfd, (struct sockaddr * ) & host_addr, (socklen_t * ) & host_addrlen);
        if (newsockfd < 0) {
            perror("Cannot accept connection.\n");
            continue;
        }
        printf("Connection accepted\n");

        int id = id_counter++;

        struct timeval time_accepted;
        gettimeofday(&time_accepted, NULL);
        printf("Id: %d, Time accepted: %ld\n", id, time_accepted.tv_sec * 1000000 + time_accepted.tv_usec);
        fprintf(log_file, "Id: %d, Time accepted: %ld\n", id, time_accepted.tv_sec * 1000000 + time_accepted.tv_usec);
        fflush(log_file);

        HandleRequestArgs * handleRequestArgs = (HandleRequestArgs * ) malloc(sizeof(HandleRequestArgs));
        handleRequestArgs -> id = id;
        handleRequestArgs -> newsockfd = newsockfd;
        handleRequestArgs -> webServer = webServer;
        addTaskThreadPool(webServer -> threadPool, handle_request, handleRequestArgs);

        if (!webServer -> open)
            break;
    }
}

HttpResponse * ping_handler(HttpRequest * http_request) {
    char * pong_string = malloc(10 * sizeof(char));
    strcpy(pong_string, "pong");
    HttpResponse * http_response = response(200, "OK", pong_string);
    add_header(http_response, "Content-Type", "text/plain");
    return http_response;
}

HttpResponse * add_handler(HttpRequest * http_request) {
    char * a = get_param(http_request, "a");
    char * b = get_param(http_request, "b");
    int a_int = atoi(a);
    int b_int = atoi(b);
    int sum = a_int + b_int;
    char * sum_string = malloc(10);
    sprintf(sum_string, "%d", sum);
    HttpResponse * http_response = response(200, "OK" , sum_string);
    add_header(http_response, "Content-Type", "text/plain");
    return http_response;
}

HttpResponse * sub_handler(HttpRequest * http_request) {
    char * a = get_param(http_request, "a");
    char * b = get_param(http_request, "b");
    int a_int = atoi(a);
    int b_int = atoi(b);
    int sub = a_int - b_int;
    char * sub_string = malloc(10);
    sprintf(sub_string, "%d", sub);
    HttpResponse * http_response = response(200, "OK", sub_string);
    add_header(http_response, "Content-Type", "text/plain");
    return http_response;
}

HttpResponse * mult_handler(HttpRequest * http_request) {
    char * a = get_param(http_request, "a");
    char * b = get_param(http_request, "b");
    int a_int = atoi(a);
    int b_int = atoi(b);
    int mult = a_int * b_int;
    char * mult_string = malloc(10);
    sprintf(mult_string, "%d", mult);
    HttpResponse * http_response = response(200, "OK", mult_string);
    add_header(http_response, "Content-Type", "text/plain");
    return http_response;
}

HttpResponse * div_handler(HttpRequest * http_request) {
    char * a = get_param(http_request, "a");
    char * b = get_param(http_request, "b");
    int a_int = atoi(a);
    int b_int = atoi(b);
    if (b_int == 0) {
        char * cannot_string = malloc(50* sizeof(char));
        strcpy(cannot_string, "Cannot divide by 0");
        HttpResponse * http_response = response(400, "Bad Request", cannot_string);
        add_header(http_response, "Content-Type", "text/plain");
        return http_response;
    }
    int div = a_int / b_int;
    char * div_string = malloc(10);
    sprintf(div_string, "%d", div);
    HttpResponse * http_response = response(200, "OK", div_string);
    add_header(http_response, "Content-Type", "text/plain");
    return http_response;
}

int main() {
    log_file = fopen("log.txt", "w");
    if (log_file == NULL) {
        printf("Error opening file!\n");
        exit(1);
    }

    WebServer * webServer = (WebServer * ) malloc(sizeof(WebServer));
    create_web_server(webServer, 8080, 2, 1024, 10);
    add_new_handler(webServer, "/ping", ping_handler);
    add_new_handler(webServer, "/add", add_handler);
    add_new_handler(webServer, "/sub", sub_handler);
    add_new_handler(webServer, "/mult", mult_handler);
    add_new_handler(webServer, "/div", div_handler);
    ConnectionDescriptor * connectionDescriptor = start_web_server(webServer);
    accept_connections(webServer, connectionDescriptor);
    return 0;
}
