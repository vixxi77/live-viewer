#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <limits.h>

int main(int argc, char *argv[]){
	if(argc < 2){
		printf("Usage: $live-viewer <HTML FILE NAME> \n");
		return 1;
	}

	char current_directory[4096];
	getcwd(current_directory, sizeof(current_directory));
	char command[5016];
	snprintf(command, sizeof(command), "xdg-open %s/%s", current_directory, argv[1]);
	system(command);
	return 0;
}
