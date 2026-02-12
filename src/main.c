#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <sys/wait.h>

#include <sys/inotify.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define DEFAULT_PORT 9090

//static char *inject_script = "\n<script data-live-viewer>\n"" console.log(\"yooo\");\n""</script>\n";

int socket_init(struct sockaddr_in *address);
void socket_loop(int socketfd, char *filename);
char *read_file(const char *filename, size_t *file_size);
void open_browser(void);


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
	printf("here before server creation \n");

	struct sockaddr_in address;
	int socketfd = socket_init(&address);
	if(socketfd == -1){
		printf("sokcet failure");
		close(socketfd);
		return -1;
	};
	printf("SERVER OPEN AT: 127.0.0.1:%d\n", DEFAULT_PORT);

	/*
	char current_directory[4096];
	getcwd(current_directory, sizeof(current_directory));
	//snprintf(command, sizeof(command), "xdg-open %s/%s", current_directory, argv[1]);
	*/
	open_browser();

	while(1){
		socket_loop(socketfd, argv[1]);
	}

	close(socketfd);
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
		return -1;
	}


	address->sin_family = AF_INET;
	address->sin_port = htons(DEFAULT_PORT);
	address->sin_addr.s_addr = 0;

	if(bind(socketfd, (struct sockaddr*)address, sizeof(*address)) == -1){
		close(socketfd);
		return -1;
	}

	if(listen(socketfd, 1) == -1){
		close(socketfd);
		return -1;
	}

	return socketfd;
}

void socket_loop(int socketfd, char *filename){
		int clientfd = accept(socketfd, NULL, NULL);

		char request[4096];
		recv(clientfd, request, sizeof(request) - 1, 0);

		if(strstr(request, "GET / ") || strstr(request, "GET /index")){
			size_t size;
			char *html = read_file(filename, &size);
			if(!html){
				close(clientfd);
				return;
			}

			if(clientfd >= 0){
				char header_buffer[512];
				int header_len = snprintf(header_buffer, sizeof(header_buffer), 
							  "HTTP/1.1 200 OK\r\n"
							  "Content-Type: text/html\r\n"
							  "Content-Length: %zu\r\n""\r\n", size);

				send(clientfd, header_buffer, header_len, 0);
				send(clientfd, html, size, 0);

			}
			free(html);
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
