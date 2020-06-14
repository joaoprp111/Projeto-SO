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
            char* copia = strdup(buffer);
            printf("%d\n",bytesread);
            printf("buffer: %s\n",buffer);
            printf("copia: %s\n",copia);
            if ((fd_escrita_canal = open("canalClienteServidor", O_WRONLY)) == -1) {
                perror("Abrir o fifo");
                return -1;
            }
            write(fd_escrita_canal,copia,bytesread-1);
            free(copia);
            close(fd_escrita_canal);

            char buffer2[512];
            if((fd_leitura_canal = open("canalServidorCliente", O_RDONLY)) == -1){
                perror("Abrir o fifo");
                return -1;
            }
            char* copia2;
            while((bytesread = read(fd_leitura_canal, buffer2, 512)) > 0){ 
                copia2 = strdup(buffer2);
                write(1, copia2, bytesread);
            }
            free(copia2);
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