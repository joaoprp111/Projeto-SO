#include <sys/wait.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>

#define SIZE 512

int existePipe(char* token){
        int res = 0, i = 0;

        while(token[i] && !res){
                if(token[i] == '|') res = 1;
                i++;
        }

        return res;
}

int exec_command(char* comandos, int n){

        char* args[n];
        char* aux;
        int exec_ret = 0;
        int i = 0;

        aux = strtok(comandos," ");

        while(aux){
                args[i] = aux;
                aux = strtok(NULL, " ");
                i++;
        }

        args[i] = NULL;

        exec_ret = execvp(args[0], args);

        return exec_ret;
}

int executar(char** comandos, int numComandos, int fd_log){
        int i, pid;
        int status[numComandos];
        int p[numComandos-1][2];

        for(i = 0; i < numComandos; i++){
                //Primeiro comando
                if(i == 0){
                        if(pipe(p[i]) == -1){
                                perror("pipe");
                                return -1;
                        }
                        switch(pid = fork()){
                                case -1:
                                        perror("fork");
                                        return -1;
                                case 0:
                                        close(p[i][0]);
                                        dup2(p[i][1], 1);
                                        close(p[i][1]);
                                        exec_command(comandos[i], numComandos);
                                        _exit(-1);
                                default:
                                        close(p[i][1]);
                        }
                }
                //Ultimo comando
                if(i == numComandos-1){
                        switch(pid = fork()){
                                case -1:
                                        perror("fork");
                                        return -1;
                                case 0:
                                        dup2(p[i-1][0], 0);
                                        close(p[i-1][0]);
                                        dup2(fd_log, 1);
                                        exec_command(comandos[i], numComandos);
                                        _exit(-1);
                                default:
                                        close(p[i-1][0]);
                        }
                }
                //Comandos intermedios
                else{
                        if(pipe(p[i]) != 0){
                                perror("pipe");
                                return -1;
                        }
                        switch(pid = fork()){
                                case -1:
                                        perror("fork");
                                        return -1;
                                case 0:
                                        close(p[i][0]);
                                        dup2(p[i][1], 1);
                                        close(p[i][1]);

                                        dup2(p[i-1][0], 0);
                                        close(p[i-1][0]);

                                        exec_command(comandos[i], numComandos);
                                        _exit(-1);
                                default:
                                        close(p[i][1]);
                                        close(p[i-1][0]);

                        }
                }
        }

        for(i = 0; i < numComandos; i++) wait(&status[i]);

        return 0;
}

char** parsing(char* bufferLeitura, int *numComandos){
        
        char** comandos = NULL;

        comandos = (char**) realloc(comandos, (*numComandos+1) * sizeof(char*));
        (*numComandos)++;

        int pipe = existePipe(bufferLeitura);

        char* token = strtok(bufferLeitura, " ");
        comandos[0] = strdup(token);
        //printf("\nFlag-> |%s|\n", comandos[0]);

        int tam = 0;

        while((token = strtok(NULL, "|")) != NULL){

                if(*numComandos == 1 && pipe){
                        tam = strlen(token);
                        token[tam-1] = '\0';
                }

                comandos = (char**) realloc(comandos, (*numComandos+1) * sizeof(char*));
                comandos[*numComandos] = strdup(token);
                //printf("\n|%s|\n", comandos[*numComandos]);
                (*numComandos)++;
        }

        if(pipe){
                //ultimo comando
                comandos[*numComandos-1] = comandos[*numComandos-1] + 1;
                //printf("\nNovo resultado último-> |%s|\n", comandos[*numComandos-1]);

                //intermédios
                for(int i = 2; i < (*numComandos-1); i++){
                        comandos[i] = comandos[i]+1;
                        tam = strlen(comandos[i]);
                        comandos[i][tam-1] = '\0';
                        //printf("\nNovo resultado intermédios-> |%s|\n", comandos[i]);
                }
        }

        return comandos;
}

int main(int argc, char *argv[]) {


        /*O servidor quando arranca já deve ter o fifo criado */

        /* Assumindo que o fifo está aberto, agora o servidor lê do fifo e faz parsing dos comandos*/
        /*char** tarefasExecucao = NULL;  Array que guarda as strings que representam tarefas em execução */
        int fd_leitura_canal = -1; /* Descritor de leitura do fifo */
        int fd_log = open("log.txt", O_CREAT | O_RDWR | O_TRUNC, 0660);
        if(fd_log < 0){
                perror("log");
                return -1;
        }

        while((fd_leitura_canal = open("canalClienteServidor", O_RDONLY)) > 0){

                char bufferLeitura[SIZE];
                char** comandos = NULL;
                int numComandos = 0;
                int bytesread = 0;

                bytesread = read(fd_leitura_canal, bufferLeitura, SIZE);

        	if(bytesread > 0) write(1, bufferLeitura, bytesread);

                comandos = parsing(bufferLeitura, &numComandos);

                for(int i = 0; i < numComandos; i++) printf("comandos[%d]: %s\n", i, comandos[i]);

                if(strcmp(comandos[0],"-e") == 0) executar(comandos+1, numComandos-1, fd_log);
                /*else if(strcmp(comandos[0], "-l") == 0) listar();
                else if(strcmp(comandos[0], "-i") == 0) alterarTempoInatividade();
                else if(strcmp(comandos[0], "-m") == 0) alterarTempoMaxExec();
                else if(strcmp(comandos[0], "-t") == 0) terminarTarefa();
                else if(strcmp(comandos[0], "-r") == 0) tarefasTerminadas();
                else if(strcmp(comandos[0], "-h") == 0) ajuda();
                else{
                        char* mensagem = "Flag inválida!\n";
                        write(1, mensagem, strlen(mensagem)+1);
                }*/

                /*for(int i = 0; i < numComandos; i++) free(comandos[i]);*/

                close(fd_leitura_canal);
        }

        exit(0);
}
