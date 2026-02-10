#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

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
	

	char current_directory[4096];
	getcwd(current_directory, sizeof(current_directory));
	char command[5016];
	snprintf(command, sizeof(command), "xdg-open %s/%s", current_directory, argv[1]);
	system(command);
	return 0;
}
