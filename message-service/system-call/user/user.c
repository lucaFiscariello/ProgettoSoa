/**
 * In questo file è contenuto il codice utente che testa le system call in maniera molto elementare senza considerare la concorrenza tra thread.
 * Test più elaborati sono stati implementati nel file "test.c".
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include <fcntl.h>


int INVALIDATE ; 
int PUT ;
int GET ;

int invalidate_data(int offset){
	return syscall(INVALIDATE,offset);
}

int put_data(char * source, size_t size){
	return syscall(PUT,source,size);
}

int get_data(int offset, char * source, size_t size){
	return syscall(GET,offset,source,size);
}

int main(int argc, char** argv){
    
	PUT = atoi(argv[1]);
	GET = atoi(argv[2]);
	INVALIDATE = atoi(argv[3]);

	char* block_1 = "Questo è il contenuto del primo blocco scritto per primo.";
	char* block_2 = "Questo è il contenuto del secondo blocco scritto per secondo.";
	char* block_3 = "Questo è il contenuto del terzo blocco scritto dopo l'invalidazione del primo blocco.";
	char* block_4 = "Questo è il contenuto del quarto blocco scritto per quarto.";

	char* block_read_1 =  malloc(sizeof(char)*strlen(block_1));
	char* block_read_2 =  malloc(sizeof(char)*strlen(block_2));

	int offset_1 = put_data(block_1,strlen(block_1));
	printf("Scrittura blocco ad offset %d\n",offset_1);

	int offset_2 = put_data(block_2,strlen(block_2));
	printf("Scrittura blocco ad offset %d\n",offset_2);

	invalidate_data(offset_1);
	printf("Invalidazione blocco ad offset %d\n",offset_1);

	int offset_3 = put_data(block_3,strlen(block_3));
	printf("Scrittura blocco ad offset %d\n",offset_3);

	int offset_4 = put_data(block_4,strlen(block_4));
	printf("Scrittura blocco ad offset %d\n",offset_4);

	get_data(offset_1,block_read_1,strlen(block_1));
    	get_data(offset_2,block_read_2,strlen(block_2));

	printf("Letto il blocco ad offset %d non valido : %s\n",offset_1,block_read_1);
	printf("Letto il blocco ad offset %d valido: %s\n",offset_2,block_read_2);

	return 0;

}

