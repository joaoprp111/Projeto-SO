#include <sys/wait.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>

#define SIZE 1024

char* parsing(char *buffer){
	if(buffer){
		buffer = strtok(NULL, "_");
		return buffer;
	}
	else return NULL;
}

int main(int argc, char const *argv[]) {

	/*O servidor quando arranca já deve ter o fifo criado */

	/* Assumindo que o fifo está aberto, agora o servidor lê do fifo e faz parsing dos comandos*/
	char** tarefasExecucao = NULL; /* Array que guarda as strings que representam tarefas em execução */
	int numTarefasExecucao = 0;
	char bufferLeitura[SIZE];
	int fd_leitura_canal; /* Descritor de leitura do fifo */
	int bytesread = 1;
	char* str1 = "À espera que o cliente abra o canal de comunicação para escrever (...) ";
	write(1, str1, strlen(str1));

        if((fd_leitura_canal = open("canalClienteServidor", O_RDONLY)) == -1){
		perror("Erro ao abrir o fifo");
		exit(-1);
	}

	char* outStr = "Já está, servidor iniciado!\n";
	write(1, outStr, strlen(outStr));

	while(bytesread > 0){
		bytesread = read(fd_leitura_canal, bufferLeitura, SIZE);
		char* pointer = bufferLeitura;
		pointer = strtok(pointer, "_");
		while(pointer = parsing(pointer)){
			tarefasExecucao = (char**) realloc(tarefasExecucao, (numTarefasExecucao+1) * sizeof(char*));
               	        tarefasExecucao[numTarefasExecucao++] = strdup(pointer);
                        printf("debug parsing: %s\n", tarefasExecucao[numTarefasExecucao-1]);
		}
	}

	close(fd_leitura_canal);

	exit(0);
}
