#include <sys/wait.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>

int main (int argc, char* argv[]) {

	int fd_escrita_canal, fd_leitura_canal;

    if ((fd_escrita_canal = open("canalClienteServidor", O_WRONLY)) == -1) {
        perror("Abrir o fifo");
        return -1;
    }

    if((fd_leitura_canal = open("canalServidorCliente", O_RDONLY)) == -1){
        perror("Abrir o fifo");
        return -1;
    }
    char buffer[256];
    int bytesread;
    bytesread = read(1,buffer,256);
    write(fd_escrita_canal,buffer,strlen(buffer) + 1);
    close(fd_escrita_canal);
    close(fd_leitura_canal);
    return 0;
}