#include <sys/wait.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>

int main(int argc, char const *argv[]) {

    int fd_fifo;

    if ((fd_fifo = open("fifo_servidor", O_WRONLY)) == -1) {
        perror("Open");
    }
    else {
        printf("opened FIFO for writing\n");
    }

    int i;
    write(fd_fifo, argv[1], strlen(argv[1]));
    write(fd_fifo,"_", 1);

    for(i = 2; i < argc; i++){
	write(fd_fifo, argv[i], strlen(argv[i]));
	write(fd_fifo,"_", 1);
    }

    close(fd_fifo);

    return 0;
}

