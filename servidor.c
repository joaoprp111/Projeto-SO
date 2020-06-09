#include <sys/wait.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>

#define SIZE 1024

int parsing(char *p, char*** array, int n){
        char* aux = p;
        while(aux){
                aux = strtok(NULL, "_");
                *array = (char**) realloc(*array, (n+1) * sizeof(char*));
                *array[n++] = strdup(aux);
        }

        return n;
}

int copia(char buffer[], char bufferDest[], int i){
        int j = 0;
        while(buffer[j]){
                bufferDest[i] = buffer[j];
                i++;
                j++;
        }
        bufferDest[i] = '\0';
        return i;
}

int main(int argc, char *argv[]) {


        /*O servidor quando arranca já deve ter o fifo criado */

        /* Assumindo que o fifo está aberto, agora o servidor lê do fifo e faz parsing dos comandos*/
        /*char** tarefasExecucao = NULL;  Array que guarda as strings que representam tarefas em execução */
        int fd_leitura_canal = -1; /* Descritor de leitura do fifo */

        while((fd_leitura_canal = open("canalClienteServidor", O_RDONLY)) > 0){

                char bufferLeitura[SIZE];
                char bufferAux[SIZE];
                int bytesread = 0;
		int k = 0;

                bytesread = read(fd_leitura_canal, bufferLeitura, SIZE);

        	if(bytesread > 0) write(1, bufferLeitura, bytesread);

                printf("\nBuffer: %s\n", bufferLeitura);

		//printf("%d\n",k);
		/*numTarefasExecucao = parsing(bufferLeitura,&array,numTarefasExecucao);
                int i;
                printf("Flag: %s\n", array[0]);
                for(i = 1; i < numTarefasExecucao-1; i++) printf("%s\n", array[i]);*/

                close(fd_leitura_canal);
        }

        exit(0);
}
