#include "argus.h"
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

/**
 * Como estrutura principal global, para armazenar os comandos de cada tarefa, pids de processos filhos, do próprio processo e do seu estado,
 * foi criada esta struct para depois construir um array de structs deste tipo, em que a primeira tarefa corresponde à struct da posição 0
 * do array, a segunda à posição 1 e assim sucessivamente.
 */
typedef struct tarefa{
        char* comando;
        int estado; /* 1 terminada normalmente, 0 em execução, 2 terminada com kill, 3 terminação forçada pelo utilizador, 4 terminada
        por tempo de inatividade entre pipes anónimos */
        int pid;
        int *pidfilhos;
        int nFilhos;
} *Tarefa;

Tarefa *tarefas = NULL; //Array com a informação de cada tarefa
int tempoExecucaoMax = 100; // em segundos
int tempoInatividadeMax = 100; // em segundos
int numTarefas = 0;
int returnStatus = -1; //Variável global que guarda o estado de retorno de um processo, depois de ter terminado
int pidInatividade; //Pid dos processos que ativam o alarme de inatividade

/**
 * Função auxiliar que transforma um inteiro em string
 */
char* itoa(int num, char *str){ 
    int i = 0;  
  
    if(num == 0){
        str = (char*) realloc(str, (i+1) * sizeof(char));
        str[i++] = '0';
    }

    while (num != 0){ 
        int rem = num % 10; 
        str = (char*) realloc(str, (i+1) * sizeof(char));
        str[i++] = (rem > 9)? (rem-10) + 'a' : rem + '0'; 
        num = num/10; 
    }
    
    str = (char*) realloc(str, (i+1) * sizeof(char));
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

/**
 * Função auxiliar que verifica se há um pipe no comando do input
 */
int existePipe(char* token){
        int res = 0, i = 0;

        while(token[i] && !res){
                if(token[i] == '|') res = 1;
                i++;
        }

        return res;
}

/**
 * Execução de um comando
 */
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

/**
 * Terminar a tarefa que está na posição pos do array (exemplo: tarefa 1 -> pos 0)
 */
void terminarTarefa(int pos){
        int fd = open("canalServidorCliente", O_WRONLY);

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


/**
 * Mostra todas as tarefas que estão a ser executadas
 */
void listarTarefasExecucao(int num){

        int fd_escrita_canal = open("canalServidorCliente", O_WRONLY);

        int i = 0;
        int existeTExec = 0;
        char outputfinal[SIZE];
        for(i = 0; i < num; i++){
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



/**
 * Para todos os processos
 */
void matarProcessos(){
        int i;
        for(i = 0; i < tarefas[numTarefas]->nFilhos; i++){
                kill((tarefas[numTarefas]->pidfilhos)[i], SIGTERM);
        }
        kill(getpid(), SIGTERM);
}


/**
 * Recebe uma terminação forçada por excesso de tempo e altera a variável de retorno de estado
 e mata os processos em execução
 */
void sig_alrm_handler(int signum){
    int pidRecetorAlarme = getpid();
    int j, encontrado = 0;
    if(pidRecetorAlarme == pidInatividade) encontrado = 1;

    if(!encontrado){
        returnStatus = 2;
    }
    else{
        returnStatus = 4;
    }
    matarProcessos();
}

/**
 * Mata o processo quando o tempo limite de inatividade é ultrapassado
 */
void sig_usr1_handler(int signum){
        returnStatus = 3;
        if(signum == SIGUSR1) matarProcessos();
}

/**
 * Mata o processo e retorna o sinal pretendido
 */
void sig_term_handler(int signum){
        _exit(returnStatus);
}


/**
 * Função principal que executa tarefas e cria filhos que conta o tempo de inatividade.
 O tempo de execução é contado através do pai vezes filhos específicos
 */
int executar(char** comandos, int numComandos){

        if(signal(SIGCHLD, SIG_DFL) == SIG_ERR){
            perror("signal");
            return -1;
        }
        if(signal(SIGALRM, sig_alrm_handler) == SIG_ERR){
            perror("signal");
            return -1;
        }
        if(signal(SIGUSR1, sig_usr1_handler) == SIG_ERR){
            perror("signal");
            return -1;
        }

        char buffer[SIZE];
        int i, pid, bytes, j = 0;
        int status[numComandos];
        int p[numComandos-1][2];
        int p2[numComandos-1][2];
        tarefas[numTarefas]->pidfilhos = (int*) malloc(((numComandos * 2)-1) * sizeof(int));
        assert(numTarefas+1 > 0);

        int fd_log = open("log.txt", O_CREAT | O_RDWR | O_APPEND, 0660);
        if(fd_log < 0){
            perror("open");
            return -1;
        }
        int inicio = lseek(fd_log, 0, SEEK_END); //Pos onde vamos começar a escrever o op
        close(fd_log);

        alarm(tempoExecucaoMax);

        for(i = 0; i < numComandos; i++){
                //Primeiro comando
                if(i == 0 && numComandos > 1){
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

                                        int fd = open("log.txt", O_CREAT | O_RDWR | O_APPEND, 0660);
                                        if(fd < 0){
                                            perror("log");
                                            return -1;
                                        }
                                        dup2(fd, 1);
                                        close(fd);

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

        for(i = 0; i < (numComandos*2) - 1; i++) wait(&status[i]);

        alarm(0);

        for(i = 0; i < (numComandos*2) - 1; i++){
                if(WEXITSTATUS(status[i]) == 2) return 2;
                else if(WEXITSTATUS(status[i]) == 3) return 3;
                else if(WEXITSTATUS(status[i]) == 4) return 4;
                else if (WEXITSTATUS(status[i] == 255)) return -1;
        }

        char* n = itoa(numTarefas+1, NULL);

        int fd_idx = open("log.idx", O_CREAT | O_RDWR | O_APPEND, 0660);
        if(fd_idx < 0){
            perror("open");
            return -1;
        }

        write(fd_idx, n, strlen(n));
        write(fd_idx, "-", 1);
        char* in = itoa(inicio, NULL);
        write(fd_idx, in, strlen(in));
        write(fd_idx, " ", 1);

        fd_log = open("log.txt", O_CREAT | O_RDWR | O_APPEND, 0660);
        if(fd_log < 0){
            perror("open");
            return -1;
        }

        int fim = lseek(fd_log, 0, SEEK_END);
        close(fd_log);

        char* f = itoa(fim, NULL);
        write(fd_idx, f, strlen(f));
        write(fd_idx, "\n", 1);

        close(fd_idx);

        assert(tarefas[numTarefas]->nFilhos > 0);

        return 0;
}


/**
 * Efetua o parsing
 */
char** parsing(char* bufferLeitura, int *numComandos){
        
        char** comandos = NULL;

        comandos = (char**) realloc(comandos, (*numComandos+1) * sizeof(char*));
        (*numComandos)++;

        int pipe = existePipe(bufferLeitura);

        char* token = strtok(bufferLeitura, " ");
        comandos[0] = strdup(token);

        int tam = 0;

        while((token = strtok(NULL, "|")) != NULL){

                if(*numComandos == 1 && pipe){
                        tam = strlen(token);
                        token[tam-1] = '\0';
                }

                char output[SIZE] = "nova tarefa #";
                comandos = (char**) realloc(comandos, (*numComandos+1) * sizeof(char*));
                comandos[*numComandos] = strdup(token);
                (*numComandos)++;
        }

        if(pipe){
                //ultimo comando
                comandos[*numComandos-1] = comandos[*numComandos-1] + 1;

                //intermédios
                for(int i = 2; i < (*numComandos-1); i++){
                        comandos[i] = comandos[i]+1;
                        tam = strlen(comandos[i]);
                        comandos[i][tam-1] = '\0';
                }
        }

        return comandos;
}


/**
 * Histórico de tarefas terminadas
 */
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


/**
 * Altera o tempo de inatividade
 */
void 
void alterarTempoInatividade(int tempo){
        int fd = open("canalServidorCliente", O_WRONLY);

        tempoInatividadeMax = tempo;
        char* str = "Tempo de inatividade entre pipes anónimos alterado com sucesso!\n";
        write(fd, str, strlen(str)+1);

        close(fd);
}

/**
 * Altera o limite máximo de tempo de execução
 */
void alterarTempoMaxExec(int tempo){
        int fd = open("canalServidorCliente", O_WRONLY);

        tempoExecucaoMax = tempo;
        char* str = "Tempo de execução alterado com sucesso!\n";
        write(fd, str, strlen(str)+1);

        close(fd);
}


/**
 * Procura uma tarefa específica 
 */
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

/**
 * Serve para alterar o estado com que o filho terminou
 */
void sig_chld_handler(int signum){
        int status, pid;
        pid = wait(&status);
        int pos = procuraTarefaComPid(pid);
        if(WEXITSTATUS(status) == 1) tarefas[pos]->estado = 1;
        else if (WEXITSTATUS(status) == 2) tarefas[pos]->estado = 2;
        else if (WEXITSTATUS(status) == 3) tarefas[pos]->estado = 3;
        else if (WEXITSTATUS(status) == 4) tarefas[pos]->estado = 4;
        else tarefas[pos]->estado = -1;
}

/**
 * Quando o utilizador clica no CTRL + C a estrutura é liberte em memória e termina o programa
 */
void sig_int_handler(int signum){
        int i;
        for(i = 0; i < numTarefas; i++){
                free(tarefas[i]->comando);
                free(tarefas[i]);
        }
        free(tarefas);
        write(1, "Servidor terminado!\n", strlen("Servidor terminado!\n")+1);
        exit(0);
}


/**
 * Mostra as opçãos disponíveis do programa
 */
void ajuda(){
    int fd = open("canalServidorCliente", O_WRONLY);
    write(fd, "tempo-inactividade segs\n", strlen("tempo-inactividade segs\n"));
    write(fd, "tempo-execucao segs\n", strlen("tempo-execucao segs\n"));
    write(fd, "executar p1 | p2 ... | pn\n", strlen("executar p1 | p2 ... | pn\n"));
    write(fd, "listar\n", strlen("listar\n"));
    write(fd, "terminar n\n", strlen("terminar n\n"));
    write(fd, "historico\n", strlen("historico\n"));
    close(fd);
}



/**
 * Altera o limite máximo de tempo de execução
 */
void procurarLinha(int pos, int *inicio, int *fim){

    char c = '\0', i;
    char buffer[SIZE];
    int fd_idx = open("log.idx", O_RDONLY);
    int encontrar = 0;

    while(!encontrar){
        i = 0;
        bzero(buffer, SIZE);
        while(c != '-'){
            read(fd_idx, &c, 1);
            buffer[i++] = c;
        }
        buffer[i] = '\0';
        int numEncontrado = atoi(buffer);
        if(numEncontrado == pos) encontrar = 1;
        else{
            //mudar de linha
            c = '\0';
            while(c != '\n') read(fd_idx, &c, 1);
        }
    }
    c = '\0';
    char bufferNovo[SIZE];
    int posb = 0;
    while(c != '\n'){
        read(fd_idx, &c, 1);
        bufferNovo[posb++] = c;
    }
    bufferNovo[posb-1] = '\0';

    //parsing
    char* p = bufferNovo;
    if((p = strtok(p, " ")) != NULL){
        *inicio = atoi(p);
        p = strtok(NULL, " ");
        *fim = atoi(p);
    }
}


/**
 * Indica o output de uma tarefa específica
 */
void output(int pos){

    if(numTarefas == 0){
        int fd_escrita_canal = open("canalServidorCliente", O_WRONLY);
        write(fd_escrita_canal, "Não existem outputs!\n", strlen("Não existem outputs!\n")+1);
        close(fd_escrita_canal);
    }
    else if(tarefas[pos-1]->estado != 1 || pos > numTarefas){
        int fd_escrita_canal = open("canalServidorCliente", O_WRONLY);
        write(fd_escrita_canal, "Não existe output para essa tarefa!\n", strlen("Não existe output para essa tarefa!\n")+1);
        close(fd_escrita_canal);
    }
    else{
        int inicio = 0, fim = 0;
        procurarLinha(pos, &inicio, &fim);

        if(inicio == fim){
            int fd_escrita_canal = open("canalServidorCliente", O_WRONLY);
            write(fd_escrita_canal, "Esta tarefa não tem output!\n", strlen("Esta tarefa não tem output!\n")+1);
            close(fd_escrita_canal);
            return;
        }

        int fd = open("log.txt", O_RDONLY);
        int posLog = lseek(fd, 0, SEEK_CUR);
        int size = fim - inicio;
        lseek(fd, inicio, SEEK_SET);

        char buffer[SIZE];
        read(fd, buffer, size);
        lseek(fd, posLog, SEEK_SET);
        close(fd);

        int fd_escrita_canal = open("canalServidorCliente", O_WRONLY);
        write(fd_escrita_canal, buffer, size);
        close(fd_escrita_canal);
    }
}
    


int main(int argc, char *argv[]) {

        if(signal(SIGINT, sig_int_handler) == SIG_ERR){
            perror("signal");
            return -1;    
        }
        if(signal(SIGCHLD, sig_chld_handler) == SIG_ERR){
            perror("signal");
            return -1;
        }

        /*O servidor quando arranca já deve ter o fifo criado */

        /* Assumindo que o fifo está aberto, agora o servidor lê do fifo e faz parsing dos comandos*/
        int fd_leitura_canal = -1, fd_escrita_canal = -1;
    
        while((fd_leitura_canal = open("canalClienteServidor", O_RDONLY)) >= 0){
                char bufferLeitura[SIZE];
                char** comandos = NULL;
                int numComandos = 0;
                int bytesread = 0;

                bytesread = read(fd_leitura_canal, bufferLeitura, SIZE);

                char* copia = strdup(bufferLeitura);
                comandos = parsing(copia, &numComandos);
                free(copia);
                
                if((strcmp(comandos[0],"-e") == 0) || (strcmp(comandos[0], "executar") == 0)){

                        tarefas = (Tarefa*) realloc(tarefas, (numTarefas+1)*sizeof(Tarefa));
                        tarefas[numTarefas] = (Tarefa) malloc(sizeof(struct tarefa));
                        if(strcmp(comandos[0],"-e") == 0) tarefas[numTarefas]->comando = strdup(bufferLeitura+3);  /*Para remover a flag e o espaço */
                        else tarefas[numTarefas]->comando = strdup(bufferLeitura+strlen("executar "));

                        tarefas[numTarefas]->pid = 0;
                        tarefas[numTarefas]->pidfilhos = NULL;
                        tarefas[numTarefas]->nFilhos = 0;
                        tarefas[numTarefas]->estado = 0; //em execução

                        char* n = NULL;
                        n = itoa(numTarefas+1, n);
                        char output[SIZE] = "nova tarefa #";
                        strcat(output, n);
                        strcat(output, "\n");
                        strcat(output, "\0");
                        fd_escrita_canal = open("canalServidorCliente", O_WRONLY);
                        write(fd_escrita_canal, output, strlen(output)+1);
                        close(fd_escrita_canal);

                        int pid;

                        pid = fork();
                        if(pid < 0) {perror("fork"); return -1;}
                        if(pid == 0){
                                signal(SIGTERM, sig_term_handler);
                                int ret = 0;
                                ret = executar(comandos+1, numComandos-1);
                                if(!ret) _exit(1); //terminou com sucesso
                                else if(ret == 2) _exit(2);
                                else if(ret == 3) _exit(3);
                                else if(ret == 4) _exit(4);
                                else _exit(-1);
                        }

                        tarefas[numTarefas]->pid = pid;
                        numTarefas++;
                }
                else if((strcmp(comandos[0], "-l") == 0 )|| (strcmp(comandos[0], "listar") == 0)) listarTarefasExecucao(numTarefas);
                else if((strcmp(comandos[0], "-m") == 0 )|| (strcmp(comandos[0], "tempo-execucao") == 0)) alterarTempoMaxExec(atoi(comandos[1]));
                else if((strcmp(comandos[0], "-t") == 0 )|| (strcmp(comandos[0], "terminar") == 0)) terminarTarefa(atoi(comandos[1])-1);
                else if((strcmp(comandos[0], "-i") == 0 )|| (strcmp(comandos[0], "tempo-inactividade") == 0)) alterarTempoInatividade(atoi(comandos[1]));
                else if((strcmp(comandos[0], "-r") == 0 )|| (strcmp(comandos[0], "historico") == 0)) tarefasTerminadas();
                else if((strcmp(comandos[0], "-h") == 0 )|| (strcmp(comandos[0], "ajuda") == 0)) ajuda();
                else if((strcmp(comandos[0], "-o") == 0 )|| (strcmp(comandos[0], "output") == 0)) output(atoi(comandos[1]));
                else{
                        fd_escrita_canal = open("canalServidorCliente", O_WRONLY);
        
                        char* mensagem = "Flag inválida!\n";
                        write(fd_escrita_canal, mensagem, strlen(mensagem)+1);

                        close(fd_escrita_canal);
                }

                close(fd_leitura_canal);
        }

        return 0;
}