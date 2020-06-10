#include <sys/wait.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <signal.h>

#define SIZE 512

typedef struct tarefa{
        char* comando;
        int terminada; // 1 terminada normalmente, 0 em execução, 2 terminada com kill
        int pid;
        int* pidfilhos;
        int nFilhos;
} *Tarefa;

Tarefa *tarefas = NULL;
int tempoExecucaoMax = 5; // em segundos
int numTarefas = 0;
 
void swap(char a, char b){
        char aux = b;
        b = a;
        a = aux;
}

char* itoa(int num, char *str){ 
    int i = 0;  
  
    // Process individual digits 
    while (num != 0){ 
        int rem = num % 10; 
        str = (char*) realloc(str, (i+1) * sizeof(char*));
        str[i++] = (rem > 9)? (rem-10) + 'a' : rem + '0'; 
        num = num/10; 
    } 
  
    str[i] = '\0';

    int start = 0; 
    int end = i -1; 
    while (start < end){ 
        char aux = str[start];
        str[start] = str[end];
        str[end] = aux;
        start++; 
        end--; 
    }  
  
    return str; 
} 

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

void listarTarefasExecucao(int fd_escrita_canal){
        int i = 0;
        int existeTExec = 0;
        for(i = 0; i < numTarefas; i++){
                if(tarefas[i]->terminada == 0){
                        existeTExec = 1;
                        char* n = NULL;
                        n = itoa(i+1, n);
                        char output[50] = "#";
                        strcat(output, n);
                        strcat(output, ": ");
                        strcat(output, tarefas[i]->comando);
                        strcat(output, "\n");
                        write(fd_escrita_canal, output, 1 + strlen(n) + 2 + strlen(tarefas[i]->comando) + 1 + 1);
                }
        }
        if(!existeTExec){
                char* out = "Não há tarefas em execução!\n";
                write(fd_escrita_canal, out, strlen(out)+1);
        }
}

int executar(char** comandos, int numComandos, int fd_log, int pos){
        int i, pid;
        int status[numComandos];
        int p[numComandos-1][2];
        tarefas[pos]->pidfilhos = (int*) malloc(numComandos * sizeof(int));
        tarefas[pos]->nFilhos = numComandos;

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
                                        (tarefas[pos]->pidfilhos)[i] = pid;
                        }
                }
                //Ultimo comando
                if(i == numComandos-1){
                        switch(pid = fork()){
                                case -1:
                                        perror("fork");
                                        return (-1);
                                case 0:
                                        dup2(p[i-1][0], 0);
                                        close(p[i-1][0]);
                                        dup2(fd_log, 1);
                                        exec_command(comandos[i], numComandos);
                                        _exit(-1);
                                default:
                                        close(p[i-1][0]);
                                        (tarefas[pos]->pidfilhos)[i] = pid;
                        }
                }
                //Comandos intermedios
                else{
                        if(pipe(p[i]) != 0){
                                perror("pipe");
                                return (-1);
                        }
                        switch(pid = fork()){
                                case -1:
                                        perror("fork");
                                        return (-1);
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
                                        (tarefas[pos]->pidfilhos)[i] = pid;

                        }
                }
        }

        for(i = 0; i < numComandos; i++){
                //printf("Filho %d: %d\n", i, (tarefas[pos]->pidfilhos)[i]);
                wait(&status[i]);
        }

        tarefas[numTarefas]->terminada = 1;
        //printf("\n[DEBUG] Terminou com sucesso!\n");

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

void matarProcessos(){
        int i = 0;
        for(i = 0; i < numTarefas; i++){
                if((tarefas[i]->terminada) == 0){
                        //matar os processos
                        //marcar o campo terminada com 2 (significa que foi terminada por um kill)
                        kill(tarefas[i]->pid, SIGKILL);
                        for(int j = 0; j < tarefas[i]->nFilhos; j++) kill((tarefas[i]->pidfilhos)[j], SIGKILL);
                        tarefas[i]->terminada = 2;
                }
        }
}

void sig_alrm_handler(int signum){
    //printf("\n[DEBUG] ATINGIU LIMITE DE EXECUÇÃO\n");
    matarProcessos();
    //printf("\n[DEBUG] Terminou com kill!\n");
    //for(int i = 0; i < numTarefas; i++) printf("Estado de terminação: %d\n", tarefas[i]->terminada);
}

void sig_chld_handler(int signum){
        wait(NULL);
}

int main(int argc, char *argv[]) {


        signal(SIGALRM, sig_alrm_handler);
        signal(SIGCHLD, sig_chld_handler);

        /*O servidor quando arranca já deve ter o fifo criado */

        /* Assumindo que o fifo está aberto, agora o servidor lê do fifo e faz parsing dos comandos*/
        int fd_leitura_canal = -1, fd_escrita_canal = -1; /* Descritor de leitura do fifo */
        int fd_log = open("log.txt", O_CREAT | O_RDWR | O_TRUNC, 0660);
        if(fd_log < 0){
                perror("log");
                return -1;
        }

        while(((fd_leitura_canal = open("canalClienteServidor", O_RDONLY)) > 0) && ((fd_escrita_canal = open("canalServidorCliente", O_WRONLY)) > 0)){

                char bufferLeitura[SIZE];
                char** comandos = NULL;
                int numComandos = 0;
                int bytesread = 0;

                bytesread = read(fd_leitura_canal, bufferLeitura, SIZE);

        	//if(bytesread > 0) write(1, bufferLeitura, bytesread);

                comandos = parsing(bufferLeitura, &numComandos);

                //for(int i = 0; i < numComandos; i++) printf("comandos[%d]: %s\n", i, comandos[i]);

                if(strcmp(comandos[0],"-e") == 0){

                        tarefas = (Tarefa*) realloc(tarefas, (numTarefas+1)*sizeof(Tarefa));
                        tarefas[numTarefas] = (Tarefa) malloc(sizeof(struct tarefa));
                        tarefas[numTarefas]->comando = strdup(bufferLeitura+3);  /*Para remover a flag e o espaço */
                        tarefas[numTarefas]->terminada = 0;
                        tarefas[numTarefas]->nFilhos = 0;

                        char* n = NULL;
                        n = itoa(numTarefas+1, n);
                        char output[14] = "nova tarefa #";
                        strcat(output, n);
                        strcat(output, "\n");
                        write(fd_escrita_canal, output, strlen(output)+1);

                        int pid;

                        pid = fork();
                        switch(pid){
                                case 0:
                                        //signal(SIGCHLD, SIG_DFL); Usa o handler default
                                        executar(comandos+1, numComandos-1, fd_log, numTarefas);
                                        _exit(0);
                                case -1:
                                        perror("fork");
                                        return -1;
                                default:
                                        tarefas[numTarefas]->pid = pid;
                                        //printf("[DEBUG] pidExecucao: %d\n", tarefas[numTarefas]->pid);
                                        break;
                        }

                        numTarefas++;

                        pid = fork();
                        if(pid == 0){
                                alarm(tempoExecucaoMax);
                                pause();
                                _exit(0);
                        }
                        else if (pid < 0){
                                perror("fork");
                                return -1;
                        }
                        //wait(&status);
                }
                else if(strcmp(comandos[0], "-l") == 0) listarTarefasExecucao(fd_escrita_canal);
                /*else if(strcmp(comandos[0], "-i") == 0) alterarTempoInatividade();
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
                close(fd_escrita_canal);
        }

        return 0;
}
