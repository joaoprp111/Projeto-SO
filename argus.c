#include <sys/wait.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    int fd_escrita_canal, fd_leitura_canal;
    if (argc == 1){
        while (1){
            write(1,"argus$ ",strlen("argus$ "));

            char buffer[256];
            int bytesread;
            bytesread = read(0,buffer,256);
            if ((fd_escrita_canal = open("canalClienteServidor", O_WRONLY)) == -1) {
                perror("Abrir o fifo");
                return -1;
            }
            write(fd_escrita_canal,buffer,strlen(buffer) + 1);
            close(fd_escrita_canal);

            char buffer2[512];
            if((fd_leitura_canal = open("canalServidorCliente", O_RDONLY)) == -1){
                perror("Abrir o fifo");
                return -1;
            }
            while((bytesread = read(fd_leitura_canal, buffer2, 512)) > 0) write(1, buffer2, bytesread);
            close(fd_leitura_canal);

        }
    }
    else {

        if ((fd_escrita_canal = open("canalClienteServidor", O_WRONLY)) == -1) {
            perror("Abrir o fifo");
            return -1;
        }

        char str[512];
        strcpy(str, argv[1]);
        for(int i = 2; i < argc; i++){
          strcat(str, " "); strcat(str, argv[i]); 
        }

        write(fd_escrita_canal, str, strlen(str)+1);

        close(fd_escrita_canal);

        if((fd_leitura_canal = open("canalServidorCliente", O_RDONLY)) == -1){
            perror("Abrir o fifo");
            return -1;
        }

        char buffer[512];
        int bytesread;
        while((bytesread = read(fd_leitura_canal, buffer, 256)) > 0) write(1, buffer, bytesread);

        close(fd_leitura_canal);
    }
    return 0;
}