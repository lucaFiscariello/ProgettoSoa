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
int NUM_BLOCK ;
int DIM_BLOCK =60;

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
	NUM_BLOCK= atoi(argv[4]);

	for(int i =3; i< NUM_BLOCK-1; i++){
		char* data = malloc(sizeof(char)*DIM_BLOCK);
		get_data(i,data,DIM_BLOCK);
		printf("BLOCK[%d] = %s\n",i,data);
	}

	return 0;

}

