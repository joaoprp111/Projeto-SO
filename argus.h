#ifndef ARGUS_H
#define ARGUS_H

#define SIZE 256

/* Argus */
void tratarInputShell(int fd_escrita_canal);
void receberOutputShell(int fd_leitura_canal);
int argusShell(int fd_escrita_canal, int fd_leitura_canal);

void tratarInputLinhaComandos(int fd_escrita_canal, int argc, char* argv[]);
void receberOutputLinhaComandos(int fd_leitura_canal);
int argusLinhaComandos(int fd_escrita_canal, int fd_leitura_canal, int argc, char* argv[]);

/* Argusd */
int executar(char** comandos, int numComandos);
void matarProcessos();
char** parsing(char* bufferLeitura, int *numComandos);
void ajuda();
int procuraTarefaComPid(int pid);
void alterarTempoMaxExec(int tempo);
void alterarTempoInatividade(int tempo);
void tarefasTerminadas();
void listarTarefasExecucao(int num);
void terminarTarefa(int pos);

#endif