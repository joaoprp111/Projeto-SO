#include <sys/wait.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    if (argc == 1){
	//chamar o argus
    }
    else {
        int fd_escrita_canal, fd_leitura_canal;

        if ((fd_escrita_canal = open("canalClienteServidor", O_WRONLY)) == -1) {
            perror("Abrir o fifo");
            return -1;
        }

        if((fd_leitura_canal = open("canalServidorCliente", O_RDONLY)) == -1){
            perror("Abrir o fifo");
            return -1;
        }

        char str[512];
        strcpy(str, argv[1]);
        for(int i = 2; i < argc; i++){
	      strcat(str, " "); strcat(str, argv[i]); 
        }

        write(fd_escrita_canal, str, strlen(str)+1);

        char buffer[256];
        int bytesread = read(fd_leitura_canal, buffer, 256);
        if(bytesread > 0) write(1, buffer, bytesread);

        close(fd_escrita_canal);
        close(fd_leitura_canal);
    }
    return 0;
}

