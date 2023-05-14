/**
 * In questo file è contenuto il codice utente che testa le system call in maniera più elaborata considerando la concorrenza tra thread.
 * In particolare vengono lanciati un certo numero di thread che eseguono in concorrenza:
 * 	-operazioni di scrittura
 * 	-operazioni di lettura
 * 	-operazioni di invalidazione
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include <fcntl.h>

#define NUM_WRITER 10
#define NUM_READER 20
#define NUM_INVALIDATOR 5
#define DIM_MESSAGE 41

#define set_color_red() (printf("\033[0;31m"))
#define set_color_green() (printf("\033[0;32m"))
#define set_color_yellow() (printf("\033[0;33m"))
#define set_color_default() (printf("\033[0m"))

int INVALIDATE ; 
int PUT ;
int GET ;

//Barriera per sincronizzare i thread e lanciarli tutti nello stesso momento
pthread_barrier_t barrier;
unsigned count;

int invalidate_data(int offset){
	return syscall(INVALIDATE,offset);
}

int put_data(char * source, size_t size){
	return syscall(PUT,source,size);
}

int get_data(int offset, char * source, size_t size){
	return syscall(GET,offset,source,size);
}

/**
 * Funzione eseguita dei thread scrittori. Ogni thread prova ad eseguire 5 scritture. 
 */
void *thread_put(void *vargp)
{
    int *myid = (int *)vargp;
	int ret;

	pthread_barrier_wait(&barrier);

	for(int i=0;i<5;i++){

		char *buffer = (char*)malloc(DIM_MESSAGE * sizeof(char));
		sprintf(buffer, "Blocco scritto da thread con id %d : numero blocco %d/5", *myid, i);
		ret = put_data(buffer,strlen(buffer));

		set_color_yellow();
		printf("%s , offset %d\n",buffer,ret);
		set_color_default();

	}

    return NULL;
}

/**
 * Funzione eseguita dai thread lettori.Ogni thread esegue 5 letture.
*/
void *thread_get(void *vargp)
{

    int *myid = (int *)vargp;
	int ret;

	pthread_barrier_wait(&barrier);

	for(int i=0;i<5;i++){

		char *buffer = (char*)malloc(DIM_MESSAGE * sizeof(char));
		int offset = i+(*myid);
		ret = get_data(offset,buffer,DIM_MESSAGE+11);

		set_color_green();
		printf("Thread con id %d legge blocco a offset %d: %s\n",*myid,offset,buffer);
		set_color_default();

	}

    return NULL;
}


/**
 * Funzione eseguita dai thread invalidatori.Ogni thread esegue 10 invalidazioni.
*/
void *thread_invalidate(void *vargp)
{
	int *myid = (int *)vargp;

	pthread_barrier_wait(&barrier);

	for(int i =0; i<10; i++){
		int offset = *myid+i;
		invalidate_data(offset);

		set_color_red();
		printf("Thread con id %d invalida blocco a offset %d\n",*myid,offset);
		set_color_default();
	
	}

    return NULL;
}

int main(int argc, char** argv){
    
	PUT = atoi(argv[1]);
	GET = atoi(argv[2]);
	INVALIDATE = atoi(argv[3]);

	count = NUM_READER +  NUM_WRITER + NUM_INVALIDATOR;

	int id_writer[NUM_WRITER];
	int id_reader[NUM_READER];
	int id_inv[NUM_INVALIDATOR];

	pthread_barrier_init(&barrier, NULL, count);

 	pthread_t thread_id_w;
	pthread_t thread_id_r;
	pthread_t thread_id_i;

	//Creo scrittori
	for(int i=0; i< NUM_WRITER; i++){
		id_writer[i]=NUM_WRITER+i;
		pthread_create(&thread_id_w, NULL, thread_put, (void*)&id_writer[i]);
	}

	//Creo lettori
	for(int i=0; i< NUM_READER; i++){
		id_reader[i]=NUM_READER+i;
		pthread_create(&thread_id_r, NULL, thread_get, (void*)&id_reader[i]);
	}

	//Creo invalidatori
	for(int i=0; i< NUM_INVALIDATOR; i++){
		id_inv[i]=NUM_INVALIDATOR+i;
		pthread_create(&thread_id_i, NULL, thread_invalidate, (void*)&id_inv[i]);
	}
	
	pthread_exit(NULL);

	return 0;

}

