#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>


#define INVALIDATE 156 //this depends on what the kernel tells you when mounting the vtpmo module
#define PUT 174
#define GET 177

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
        

	char* testo = "prova testo s1";
	char* testo2 = "prova testo s2";

	char* testoletto = malloc(sizeof(char)*15);

	int offset = put_data(testo,15);
	int offset2 = put_data(testo2,15);
	invalidate_data(offset);

	get_data(offset2,testoletto,15);

	printf("letto %s",testoletto);


	return 0;

}

