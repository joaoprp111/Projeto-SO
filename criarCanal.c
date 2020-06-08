#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>

int main(int argc, char const *argv[]) {

	if(mkfifo("canalClienteServidor", 0666) == -1){
		perror("Criar fifo");
		exit(-1);
	}
	
	char* mensagem = "Canal criado com sucesso!\n";
	write(1, mensagem, strlen(mensagem)+1);

	exit(0);
}
