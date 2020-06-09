#include <sys/wait.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {

    int fd_escrita_canal;

    if ((fd_escrita_canal = open("canalClienteServidor", O_WRONLY)) == -1) {
        perror("Abrir o fifo");
    }

    char str[512];
    strcpy(str, argv[1]);
    for(int i = 2; i < argc; i++){
	    strcat(str, " "); strcat(str, argv[i]); 
    }

    write(fd_escrita_canal, str, strlen(str)+1);

    close(fd_escrita_canal);

    return 0;
}

