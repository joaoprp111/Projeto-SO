#include <sys/wait.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>

int main(int argc, char const *argv[]) {

    int fd_escrita_canal;

    if ((fd_escrita_canal = open("canalClienteServidor", O_WRONLY)) == -1) {
        perror("Abrir o fifo");
    }

    int i;
    write(fd_escrita_canal, argv[1], strlen(argv[1]));
    write(fd_escrita_canal,"_", 1);

    for(i = 2; i < argc; i++){
	write(fd_escrita_canal, argv[i], strlen(argv[i]));
	write(fd_escrita_canal,"_", 1);
    }

    close(fd_escrita_canal);

    return 0;
}

