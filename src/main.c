#define _POSIX_C_SOURCE 199309L
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <time.h>

#include <pthread.h>

#include <sys/wait.h>

#include <sys/inotify.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define DEFAULT_PORT 9090

//this not mine 
#define EVENT_SIZE  ( sizeof (struct inotify_event) )
#define EVENT_BUF_LEN     ( 1024 * ( EVENT_SIZE + 16 ) )

static char *inject_script = "\n<link rel=\"icon\" href=\"data:,\">"
			     "\n<script data-live-viewer>\n"
			     "window.addEventListener('load', function() {\n"
			     "  const es = new EventSource('/events');\n"
			     "  es.onmessage = function(e) {\n"
			     "    if (e.data === 'reload') location.reload();\n"
			     "  };\n"
			     "});\n"
			     "</script>\n";

typedef struct {
	char *directory_path;
	char *filename;
	int read_flag;
	int running;
} program_ctx;

int socket_init(struct sockaddr_in *address);
void socket_loop(int socketfd, char *filename, int *read_flag);
char *read_file(const char *filename, size_t *file_size);
void open_browser(void);
char *current_directory(char *buffer, size_t size);
void *inotify_watcher(void *arg);
struct inotify_event *inotify_wait(char* buffer, struct inotify_event *event, int len);


char *current_directory(char *buffer, size_t size){
	if(getcwd(buffer, size) == NULL) return NULL;
	return buffer;
}

struct inotify_event *inotify_wait(char* buffer, struct inotify_event *event, int len){
	char *i = event == NULL?buffer:(char*)event + sizeof(struct inotify_event) + event->len;

	if(i >= (buffer + len)){
		return NULL;
	}

	return (struct inotify_event *)i;
}

void *inotify_watcher(void *arg){
	int inotifyfd = inotify_init();
	program_ctx *context = (program_ctx*)arg;
	char *directory_path = (char*)context->directory_path;
	char *filename = (char*)context->filename;
	struct inotify_event *event = NULL;
	int mask = IN_CLOSE_WRITE;

	printf("inotify thread started... working directory is: %s \n", directory_path);

	if(inotifyfd < 0){
		printf("inotify failure \n");
		close(inotifyfd);
		return NULL;
	}
	int inotifywd = inotify_add_watch(inotifyfd, directory_path, mask);
	if(inotifywd < 0){
		printf("inotify watch desrciptor failure \n");
		return NULL;
	}

	char event_buffer[EVENT_BUF_LEN];
	int len;
	while(context->running){
		len = read(inotifyfd, event_buffer, sizeof(event_buffer));
		if(len == -1){
			printf("cant read from queue \n");
			return NULL;
		}

		event = NULL;
		while((event = inotify_wait(event_buffer, event, len)) != NULL){
			if(event->len > 0 && strcmp(event->name, filename) == 0){
				if((event->mask & IN_CLOSE_WRITE)){
						__atomic_store_n(&context->read_flag, 1, __ATOMIC_SEQ_CST);
						printf("file was modified brudda \n");
				}
			}
		}
	}
	printf("exited \n");
	inotify_rm_watch(inotifyfd, inotifywd);
	close(inotifyfd);
	return NULL;
}


int main(int argc, char *argv[]){
	if(argc < 2){
		printf("Usage: $live-viewer <HTML FILE NAME> \n");
		printf("       $live-viewer -help for available commands \n\n");
		return 1;
	}

	if(strcmp("-help", argv[1]) == 0){
		printf("FLAGS:       -help              <lists all commands> \n");
		printf("             -port PORT_NUMBER  <changes port> \n\n");
		printf("COMMANDS:    stop               <stops the process> \n");
		return 1;
	}
	program_ctx context = {0};

	char current_d_buffer[4096];
	char *directory = current_directory(current_d_buffer, sizeof(current_d_buffer)); 
	context.directory_path = directory;
	context.filename = argv[1];

	if(directory == NULL){
		printf("cant find directory \n");
		return -1;
	}

	struct sockaddr_in address;
	int socketfd = socket_init(&address);
	if(socketfd < 0){
		printf("sokcet failure \n");
		close(socketfd);
		return -1;
	};
	printf("SERVER OPEN AT: 127.0.0.1:%d\n", DEFAULT_PORT);

	//open_browser();

	context.running = 1;
	pthread_t inotify_thread;
	pthread_create(&inotify_thread, NULL, inotify_watcher, &context);

	while(1){
		socket_loop(socketfd, argv[1], &context.read_flag);
	}
	context.running = 0;

	close(socketfd);
	pthread_join(inotify_thread, NULL);
	return 0;
}

int socket_init(struct sockaddr_in *address){
	int socketfd = socket(AF_INET, SOCK_STREAM, 0);
	if(socketfd == -1){
		return -1;
	}

	int option = 1;
	if(setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option)) == -1 ){
		perror("reuse address failed? \n");
		close(socketfd);
		return -1;
	}


	address->sin_family = AF_INET;
	address->sin_port = htons(DEFAULT_PORT);
	address->sin_addr.s_addr = 0;

	if(bind(socketfd, (struct sockaddr*)address, sizeof(*address)) == -1){
		close(socketfd);
		return -1;
	}

	if(listen(socketfd, 16) == -1){
		close(socketfd);
		return -1;
	}

	return socketfd;
}

void socket_loop(int socketfd, char *filename, int *read_flag){
		int clientfd = accept(socketfd, NULL, NULL);

		char request[4096];
		int receive_size = recv(clientfd, request, sizeof(request) - 1, 0);
		if(receive_size <= 0){
			close(clientfd);
			return;
		}
		request[receive_size] = '\0';
		printf("%s \n", request);


		if(strstr(request, "GET /events")){
			const char *header = "HTTP/1.1 200 OK\r\n"
					     "Content-Type: text/event-stream\r\n"
					     "Cache-Control: no-cache\r\n"
					     "Connection: keep-alive\r\n\r\n";

			send(clientfd, header, strlen(header), 0);
			
			const char *initial = ": connected\n\n";
			send(clientfd, initial, strlen(initial), 0);

			while(1){
				if(__atomic_exchange_n(read_flag, 0, __ATOMIC_SEQ_CST) == 1){
					const char *message = "data: reload\n\n";
					if(send(clientfd, message, strlen(message), 0) <= 0){
						printf("cleint dc during reload \n");
						break;
					};
					printf("gonna reload \n");
					break;
				}
				struct timespec ts = {0, 20 * 1000 * 1000};
				nanosleep(&ts, NULL);
			}
			close(clientfd);
			return;
		}


		if(strstr(request, "GET / ")){
			size_t size;
			char *html = read_file(filename, &size);
			if(!html){
				close(clientfd);
				return;
			}

			size_t inject_len = strlen(inject_script);
			size_t out_len = size + inject_len;
			char *out = malloc(out_len + 1);
			if(!out){
				free(html);
				close(clientfd);
				return;
			}

			memcpy(out, html, size);
			memcpy(out + size, inject_script, inject_len);
			out[out_len] = '\0';
			free(html);

			if(clientfd >= 0){
				char header_buffer[512];
				int header_len = snprintf(header_buffer, sizeof(header_buffer), 
							  "HTTP/1.1 200 OK\r\n"
							  "Content-Type: text/html\r\n"
							  "Content-Length: %zu\r\n""\r\n", out_len);

				send(clientfd, header_buffer, header_len, 0);
				send(clientfd, out, out_len, 0);
			}
			free(out);
			close(clientfd);
			return;
		}

		if(strstr(request, "GET /") && !strstr(request, "GET /events") && !strstr(request, "GET / ")){
			char filepath[512];
			sscanf(request, "GET /%511s", filepath);

			size_t size;
			char *data = read_file(filepath, &size);
			if(data){
				char header[256];
				snprintf(header, sizeof(header), "HTTP/1.1 200 OK\r\n"
						                 "Content-Length: %zu\r\n\r\n", size);
				send(clientfd, header, strlen(header), 0);
				send(clientfd, data, size, 0);
				free(data);
			}else{
				const char *notfound= "HTTP/1.1 404 Not Found\r\n\r\n";
				send(clientfd, notfound, strlen(notfound), 0);
			}
		}
		close(clientfd);
}

char *read_file(const char *filename, size_t *file_size){
	FILE *file = fopen(filename, "rb");
	if(!file){
		return NULL;
	}

	fseek(file, 0, SEEK_END);
	long size = ftell(file);
	rewind(file);

	char *data = malloc(size + 1);
	if(!data){
		fclose(file);
		return NULL;
	}

	fread(data, 1, size, file);
	data[size] = '\0';
	fclose(file);

	*file_size = size;
	return data;
}

void open_browser(void){
	pid_t pid = fork();

	if(pid == -1){
		perror("forking failed \n");
		return;
	}

	if(pid == 0){
		char URL[256];
		snprintf(URL, sizeof(URL), "http://127.0.0.1:%d", DEFAULT_PORT);
		execlp("xdg-open", "xdg-open", URL, (char*)NULL);

		printf("child exiting \n");
		_exit(1);
	}

	//collect this damn orphan so he dont litter >:(
	if(waitpid(pid, NULL, 0) < 0){
		perror("i just cant man \n");
		return;
	}
	
	return;
}
