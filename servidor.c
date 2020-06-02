#include <sys/wait.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>

#define MAX_ARG_SIZE 1024

char* parsing(char *buffer){
	if(buffer){
		buffer = strtok(NULL, "_");
		return buffer;
	}
	else return NULL;
}

int main(int argc, char const *argv[]) {
	// int *execucao (dinamico)
    char buffer[MAX_ARG_SIZE];
    int fd_fifo, fd_log;
    int bytes_read;

    if (mkfifo("fifo_servidor", 0666) == -1)
        perror("Mkfifo");

    if ((fd_log = open("log.txt", O_CREAT | O_TRUNC | O_WRONLY, 0666)) == -1) {
        perror("Open log.txt");
        return -1;
    }
    else
        printf("[DEBUG] opened file for writing.\n");

    while (1) {
        bzero(buffer, 1024);

        if ((fd_fifo = open("fifo_servidor", O_RDONLY)) == -1)
            perror("Open FIFO");
        else
            printf("[DEBUG] opened FIFO for reading\n");

        bytes_read = read(fd_fifo, buffer, MAX_ARG_SIZE);
        write(1, buffer, bytes_read);
        write(1, "\n", 1);
        printf("[DEBUG] wrote %s to stdout\n", buffer);

	char* p = buffer;
	p = strtok(p, "_");
	printf("Instrução: %s\n", p);
	char* tarefa;
	while(tarefa = parsing(p)){
		//meter tarefa no array
		printf("%s\n", tarefa);
	}

        if (bytes_read == 0)
            printf("[DEBUG] EOF\n");

        close(fd_fifo);
    }

    close(fd_log);

    return 0;
}
