all: argus.o argusd.o
	mkfifo canalClienteServidor
	mkfifo canalServidorCliente

argus.o: argus.c argus.h
	gcc -g -o argus argus.c

argusd.o: argusd.c argus.h
	gcc -g -o argusd argusd.c


clean:
	rm argus argusd
	rm -rf *.idx
	rm -rf *.o
	rm -rf *.txt
	unlink canalClienteServidor
	unlink canalServidorCliente
