#include <sys/wait.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <signal.h>
#include <assert.h>
typedef void (*sighandler_t)(int);

#define SIZE 512

typedef struct tarefa{
        char* comando;
        int estado; // 1 terminada normalmente, 0 em execução, 2 terminada com kill, -1 terminado por erro dos execs
        int pid;
        int *pidfilhos;
        int nFilhos;
} *Tarefa;

Tarefa *tarefas = NULL;
int tempoExecucaoMax = 100; // em segundos
int tempoInatividadeMax = 100;
int numTarefas = 0;
int returnStatus = -1;
int pidInatividade;

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

void terminarTarefa(int pos){
        int fd = open("canalServidorCliente", O_WRONLY);

        printf("Pid: %d\n", tarefas[pos]->pid);

        if(pos < numTarefas && pos >= 0 && tarefas[pos]->estado == 0){
                kill(tarefas[pos]->pid, SIGUSR1);
                char* str = "Tarefa terminada com sucesso!\n";
                write(fd, str, strlen(str)+1);
        }
        else{
                char* str = "Essa tarefa não está em execução.\n";
                write(fd, str, strlen(str)+1);
        }

        close(fd);
}

void listarTarefasExecucao(int num){

        int fd_escrita_canal = open("canalServidorCliente", O_WRONLY);

        int i = 0;
        int existeTExec = 0;
        char outputfinal[256];
        printf("n tarefas %d\n", num);
        for(i = 0; i < num; i++){
                printf("estado %d\n", tarefas[i]->estado);
                if(tarefas[i]->estado == 0){
                        existeTExec = 1;
                        char* n = NULL;
                        n = itoa(i+1, n);
                        char* output = strdup("#");
                        strcat(output, n);
                        strcat(output, ": ");
                        strcat(output, tarefas[i]->comando);
                        strcat(output, "\n");
                        write(fd_escrita_canal, output, strlen(output)+1);
                }
        }

        if(!existeTExec){
                char* out = "Não há tarefas em execução!\n";
                write(fd_escrita_canal, out, strlen(out)+1);
        }

        close(fd_escrita_canal);
}

void matarProcessos(){
        int i;
        for(i = 0; i < tarefas[numTarefas]->nFilhos; i++){
                kill((tarefas[numTarefas]->pidfilhos)[i], SIGTERM);
        }
        kill(getpid(), SIGTERM);
}

void sig_alrm_handler(int signum){
    printf("\n[DEBUG] ATINGIU LIMITE DE EXECUÇÃO OU INATIVIDADE\n");
    int pidRecetorAlarme = getpid();
    int j, encontrado = 0;
    if(pidRecetorAlarme == pidInatividade) encontrado = 1;

    if(!encontrado){
        printf("Execução\n");
        returnStatus = 2;
    }
    else{
        printf("Inatividade\n");
        returnStatus = 4;
    }
    matarProcessos();
    /*printf("\n[DEBUG] Terminou com kill!\n");
    for(int i = 0; i < numTarefas; i++) printf("Estado de terminação: %d\n", tarefas[i]->terminada);*/
}

void sig_usr1_handler(int signum){
        returnStatus = 3;
        if(signum == SIGUSR1) matarProcessos();
}

void sig_term_handler(int signum){
        printf("%d\n", returnStatus);
        _exit(returnStatus);
}

int executar(char** comandos, int numComandos, int fd_log){
        signal(SIGCHLD, SIG_DFL);
        signal(SIGALRM, sig_alrm_handler);
        signal(SIGUSR1, sig_usr1_handler);
        char buffer[256];
        int i, pid, bytes, j = 0;
        int status[numComandos];
        int p[numComandos-1][2];
        int p2[numComandos-1][2];
        tarefas[numTarefas]->pidfilhos = (int*) malloc(((numComandos * 2)-1) * sizeof(int));
        assert(numTarefas+1 > 0);

        alarm(tempoExecucaoMax);

        for(i = 0; i < numComandos; i++){
                //Primeiro comando
                if(i == 0){
                        if(pipe(p[i]) == -1){
                                perror("pipe");
                                return -1;
                        }
                        if(pipe(p2[i]) == -1){
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
                                        tarefas[numTarefas]->nFilhos += 1;
                                        (tarefas[numTarefas]->pidfilhos)[j++] = pid;
                        }

                        switch(pid = fork()){
                                case -1:
                                        perror("fork");
                                        return -1;
                                case 0:
                                        pidInatividade = getpid();
                                        alarm(tempoInatividadeMax);
                                        dup2(p[i][0], 0); //ta a ler do p[i]
                                        close(p[i][0]); 
                                        close(p2[i][0]);
                                        dup2(p2[i][1], 1); // ta a escrever pro p2[i]
                                        close(p2[i][1]);
                                        while((bytes = read(0, buffer, 256)) > 0){
                                            alarm(0);
                                            write(1, buffer, bytes);
                                        }
                                        _exit(0);
                                default:
                                        close(p2[i][1]);
                                        close(p[i][0]);
                                        tarefas[numTarefas]->nFilhos += 1;
                                        (tarefas[numTarefas]->pidfilhos)[j++] = pid;
                        }

                }
                //Ultimo comando
                else if(i == numComandos-1){
                        switch(pid = fork()){
                                case -1:
                                        perror("fork");
                                        return (-1);
                                case 0:
                                        dup2(p2[i-1][0], 0);
                                        close(p2[i-1][0]);

                                        dup2(fd_log, 1);
                                        exec_command(comandos[i], numComandos);
                                        _exit(-1);
                                default:
                                        close(p2[i-1][0]);
                                        tarefas[numTarefas]->nFilhos += 1;
                                        (tarefas[numTarefas]->pidfilhos)[j++] = pid;
                        }
                }
                //Comandos intermedios
                else{
                        if(pipe(p[i]) != 0){
                                perror("pipe");
                                return (-1);
                        }
                        if(pipe(p2[i]) == -1){
                                perror("pipe");
                                return -1;
                        }
                        switch(pid = fork()){
                                case -1:
                                        perror("fork");
                                        return (-1);
                                case 0:
                                        dup2(p2[i-1][0], 0);
                                        close(p2[i-1][0]);

                                        close(p[i][0]);
                                        dup2(p[i][1], 1);
                                        close(p[i][1]);

                                        exec_command(comandos[i], numComandos);
                                        _exit(-1);
                                default:
                                        close(p[i][1]);
                                        close(p2[i-1][0]);
                                        tarefas[numTarefas]->nFilhos += 1;
                                        (tarefas[numTarefas]->pidfilhos)[j++] = pid;
                        }

                        switch(pid = fork()){
                                case -1:
                                        perror("fork");
                                        return (-1);
                                case 0:
                                        pidInatividade = getpid();
                                        alarm(tempoInatividadeMax);
                                        dup2(p[i][0], 0);
                                        close(p[i][0]);

                                        dup2(p2[i][1], 1);
                                        close(p2[i][1]);
                                        close(p2[i][0]);

                                        while((bytes = read(0, buffer, 256)) > 0){
                                            alarm(0);
                                            write(1, buffer, bytes);
                                        }
                                        _exit(0);
                                default:
                                        tarefas[numTarefas]->nFilhos += 1;
                                        (tarefas[numTarefas]->pidfilhos)[j++] = pid;
                        }
                }
        }

        //printf("\n[DEBUG] Terminou com sucesso!\n");

        //assert(numComandos == tarefas[numTarefas]->nFilhos);

        for(i = 0; i < (numComandos*2) - 1; i++){
                //printf("Num comandos: %d\n", numComandos);
                //printf("Num filhos: %d\n", tarefas[numTarefas]->nFilhos);
                //printf("Filho %d: %d\n", i, (tarefas[pos]->pidfilhos)[i]);
                wait(&status[i]);
                printf("status do filho: %d\n", WEXITSTATUS(status[i]));
        }

        alarm(0);

        for(i = 0; i < (numComandos*2) - 1; i++){
                if(WEXITSTATUS(status[i]) == 2) return 2;
                else if(WEXITSTATUS(status[i]) == 3) return 3;
                else if(WEXITSTATUS(status[i]) == 4) return 4;
                else if (WEXITSTATUS(status[i] == 255)) return -1;
        }

        assert(tarefas[numTarefas]->nFilhos > 0);

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
                        comandos[i][tam-1] = '\0';//printf("Num comandos: %d\n", numComandos);
                //printf("Num filhos: %d\n", tarefas[numTarefas]->nFilhos);
                        //printf("\nNovo resultado intermédios-> |%s|\n", comandos[i]);
                }
        }

        return comandos;
}

void tarefasTerminadas(){
    int i, fd = open("canalServidorCliente", O_WRONLY), count = 0;
    for(i = 0; i < numTarefas; i++){
        if(tarefas[i]->estado > 0){
            count++;
            write(fd, "#", strlen("#"));
            char* n = NULL;
            n = itoa(i+1, n);
            write(fd, n, strlen(n)); write(fd, ", ", strlen(", "));
            if(tarefas[i]->estado == 1) write(fd, "concluida: ", strlen("concluida: "));
            else if(tarefas[i]->estado == 2) write(fd, "max execução: ", strlen("max execução: "));
            else if(tarefas[i]->estado == 3) write(fd, "terminada pelo utilizador: ", strlen("terminada pelo utilizador: "));
            else if(tarefas[i]->estado == 4) write(fd, "max inactividade: ", strlen("max inactividade: "));
            write(fd, tarefas[i]->comando, strlen(tarefas[i]->comando));
            write(fd, "\n", 1);
        }
    }
    if(count == 0) write(fd, "Ainda não há tarefas terminadas!\n", strlen("Ainda não há tarefas terminadas!\n"));
    close(fd);
}

void alterarTempoInatividade(int tempo){
        int fd = open("canalServidorCliente", O_WRONLY);

        tempoInatividadeMax = tempo;
        char* str = "Tempo de inatividade entre pipes anónimos alterado com sucesso!\n";
        write(fd, str, strlen(str)+1);

        close(fd);
}

void alterarTempoMaxExec(int tempo){
        int fd = open("canalServidorCliente", O_WRONLY);

        tempoExecucaoMax = tempo;
        char* str = "Tempo de execução alterado com sucesso!\n";
        write(fd, str, strlen(str)+1);

        close(fd);
}

int procuraTarefaComPid(int pid){
        int i, encontrado = 0, res = -1;
        for(i = 0; i < numTarefas && !encontrado; i++){
                if(tarefas[i]->pid == pid){
                        res = i;
                        encontrado = 1;
                }
        }

        return res;
}

void sig_chld_handler(int signum){
        int status, pid;
        pid = wait(&status);
        int pos = procuraTarefaComPid(pid);
        assert(pos >= 0);
        if(WEXITSTATUS(status) == 1) tarefas[pos]->estado = 1;
        else if (WEXITSTATUS(status) == 2) tarefas[pos]->estado = 2;
        else if (WEXITSTATUS(status) == 3) tarefas[pos]->estado = 3;
        else if (WEXITSTATUS(status) == 4) tarefas[pos]->estado = 4;
        else tarefas[pos]->estado = -1;
}

void sig_int_handler(int signum){
        int i;
        for(i = 0; i < numTarefas; i++){
                free(tarefas[i]->comando);
                free(tarefas[i]);
        }
        free(tarefas);
        exit(0);
}

void sig_pipe_handler(int signum){
        printf("\nSIGPIPE\n");
}

int main(int argc, char *argv[]) {

        signal(SIGINT, sig_int_handler);
        signal(SIGPIPE, sig_pipe_handler);
        signal(SIGCHLD, sig_chld_handler);

        /*O servidor quando arranca já deve ter o fifo criado */

        /* Assumindo que o fifo está aberto, agora o servidor lê do fifo e faz parsing dos comandos*/
        int fd_leitura_canal = -1, fd_escrita_canal = -1; /* Descritor de leitura do fifo */
        int fd_log = open("log.txt", O_CREAT | O_RDWR | O_TRUNC, 0660);
        if(fd_log < 0){
                perror("log");
                return -1;
        }

        while((fd_leitura_canal = open("canalClienteServidor", O_RDONLY)) >= 0){
                char bufferLeitura[SIZE];
                char** comandos = NULL;
                int numComandos = 0;
                int bytesread = 0;

                bytesread = read(fd_leitura_canal, bufferLeitura, SIZE);

                //if(bytesread > 0) write(1, bufferLeitura, bytesread);

                char* copia = strdup(bufferLeitura);
                comandos = parsing(copia, &numComandos);
                free(copia);

                //for(int i = 0; i < numComandos; i++) printf("comandos[%d]: %s\n", i, comandos[i]);

                if(strcmp(comandos[0],"-e") == 0){

                        tarefas = (Tarefa*) realloc(tarefas, (numTarefas+1)*sizeof(Tarefa));
                        tarefas[numTarefas] = (Tarefa) malloc(sizeof(struct tarefa));
                        tarefas[numTarefas]->comando = strdup(bufferLeitura+3);  /*Para remover a flag e o espaço */
                        //printf("%s\n", tarefas[numTarefas]->comando);
                        tarefas[numTarefas]->pid = 0;
                        tarefas[numTarefas]->pidfilhos = NULL;
                        tarefas[numTarefas]->nFilhos = 0;
                        tarefas[numTarefas]->estado = 0; //em execução

                        char* n = NULL;
                        n = itoa(numTarefas+1, n);
                        char output[14] = "nova tarefa #";
                        strcat(output, n);
                        strcat(output, "\n");
                        fd_escrita_canal = open("canalServidorCliente", O_WRONLY);
                        write(fd_escrita_canal, output, strlen(output)+1);
                        close(fd_escrita_canal);

                        int pid;

                        pid = fork();
                        if(pid < 0) {perror("fork"); return -1;}
                        if(pid == 0){
                                signal(SIGTERM, sig_term_handler);
                                int ret = 0;
                                ret = executar(comandos+1, numComandos-1, fd_log);
                                if(!ret) _exit(1); //terminou com sucesso
                                else if(ret == 2) _exit(2);
                                else if(ret == 3) _exit(3);
                                else if(ret == 4) _exit(4);
                                else _exit(-1);
                        }

                        tarefas[numTarefas]->pid = pid;
                        numTarefas++;
                }
                else if(strcmp(comandos[0], "-l") == 0) listarTarefasExecucao(numTarefas);
                else if(strcmp(comandos[0], "-m") == 0) alterarTempoMaxExec(atoi(comandos[1]));
                else if(strcmp(comandos[0], "-t") == 0) terminarTarefa(atoi(comandos[1])-1);
                else if(strcmp(comandos[0], "-i") == 0) alterarTempoInatividade(atoi(comandos[1]));
                else if(strcmp(comandos[0], "-r") == 0) tarefasTerminadas();
                /*else if(strcmp(comandos[0], "-h") == 0) ajuda();
                else{
                        char* mensagem = "Flag inválida!\n";
                        write(1, mensagem, strlen(mensagem)+1);
                }*/

                /*for(int i = 0; i < numComandos; i++) free(comandos[i]);*/

                close(fd_leitura_canal);
        }

        return 0;
}