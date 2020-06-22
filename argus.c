#include "argus.h"
#include <sys/wait.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>

/* ---------------------------------------------------------------------------------
         Antes do argus receber comandos, o servidor deve estar aberto
         assim como os canais de comunicação (fifos) devem estar criados
         tudo a partir do makefile
------------------------------------------------------------------------------------- */

int main(int argc, char *argv[]) {
    int fd_escrita_canal, fd_leitura_canal;

    /* Se o argc for 1, lançamos a shell personalizada argus$ e fica sempre aberta até receber um SIGINT originado pelo utilizador */
    if (argc == 1)
        argusShell(fd_escrita_canal, fd_leitura_canal);

    /* Senão recebe-se o comando e envia-se para o servidor, este programa cliente termina mal receba um output do servidor */
    else
        argusLinhaComandos(fd_escrita_canal, fd_leitura_canal, argc, argv);

    return 0;
}

/**
 * Enviar os inputs da shell para o servidor
 */
void tratarInputShell(int fd_escrita_canal){
    write(1,"argus$ ",strlen("argus$ "));

    char buffer[256]; char c;
    int bytesread = 0;
    while(read(0,&c,1) && c != '\n'){
            if(c != '\"') buffer[bytesread++] = c; 
    }
    buffer[bytesread++] = '\0';
            
    write(fd_escrita_canal, buffer, bytesread);
}

/**
 * Receber feedback do servidor, e dar print no "stdout" da shell argus
 */
void receberOutputShell(int fd_leitura_canal){
    int bytesread = 0;

    char buffer2[512];

    while((bytesread = read(fd_leitura_canal, buffer2, 512)) > 0)
                write(1, buffer2, bytesread);
}

/**
 * Criação da shell
 */
int argusShell(int fd_escrita_canal, int fd_leitura_canal){
    while (1){

            if ((fd_escrita_canal = open("canalClienteServidor", O_WRONLY)) == -1) {
                perror("Abrir o fifo");
                return -1;
            }

            tratarInputShell(fd_escrita_canal);
            
            close(fd_escrita_canal);

            if((fd_leitura_canal = open("canalServidorCliente", O_RDONLY)) == -1){
                perror("Abrir o fifo");
                return -1;
            }
            
            receberOutputShell(fd_leitura_canal);
            
            close(fd_leitura_canal);
        }
}

/**
 * Envia os argumentos do executável separados por espaço para os servidor
 */
void tratarInputLinhaComandos(int fd_escrita_canal, int argc, char* argv[]){

    char str[512];
    strcpy(str, argv[1]);
    for(int i = 2; i < argc; i++){
        strcat(str, " "); strcat(str, argv[i]); 
    }

    write(fd_escrita_canal, str, strlen(str)+1);
}

/**
 *  Recebe o feedback do servidor através do fifo e apresenta no ecrã ao cliente
 */
void receberOutputLinhaComandos(int fd_leitura_canal){

    char buffer[512];
    int bytesread;
    while((bytesread = read(fd_leitura_canal, buffer, 256)) > 0) write(1, buffer, bytesread);
}


/**
 * Esta função envia o input da linha de comandos para o seridor, e recebe um feedback dele
 */
int argusLinhaComandos(int fd_escrita_canal, int fd_leitura_canal, int argc, char* argv[]){
        
        if ((fd_escrita_canal = open("canalClienteServidor", O_WRONLY)) == -1) {
            perror("Abrir o fifo");
            return -1;
        }

        tratarInputLinhaComandos(fd_escrita_canal, argc, argv);

        close(fd_escrita_canal);

        if((fd_leitura_canal = open("canalServidorCliente", O_RDONLY)) == -1){
            perror("Abrir o fifo");
            return -1;
        }

        receberOutputLinhaComandos(fd_leitura_canal);

        close(fd_leitura_canal);
}