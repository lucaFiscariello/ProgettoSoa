/**
 * In questo file è contenuto il codice utente che testa le system call in maniera più elaborata considerando la concorrenza tra thread.
 * In particolare vengono lanciati un certo numero di thread che eseguono in concorrenza letture, scritture, invalidazioni.
 * Il test può essere suddiviso logicamente in 3 fasi:
 * 	- inizializzazione: vengono invalidati tutti i blocchi del device per creare un ambiente di esecuzione riproducibile
 *	- esecuzione: si lanciano le operazioni le scrittura,lettura e invalidazione in maniera concorrente
 *	- check: al termine delle operazioni concorrenti si verifica , usando appropriate strutture dati, se è avvenuta correttamente anche solo una lettura 
 *	  di un blocco che è stato invalidato ma mai sovrascritto. Se si verifica uno scenario di questo tipo si è verificato un errore.
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <pthread.h>

#define NUM_WRITER 5
#define NUM_READER 20
#define NUM_INVALIDATOR 5
#define NUM_OP_W 10 		//Numero di scritture tentate da ogni writer
#define NUM_OP_R 5		//Numero di letture tentate da ogni reader
#define NUM_OP_I 10		//Numero di invalidazioni tentate da ogni invalidatore
#define DIM_MESSAGE 100

#define set_color_red() (printf("\033[0;31m"))
#define set_color_green() (printf("\033[0;32m"))
#define set_color_yellow() (printf("\033[0;33m"))
#define set_color_default() (printf("\033[0m"))

//Aggiunge l'id di un blocco alla struttura id_blocks_read[] in maniera thread safe
#define add_id_block_read(block_id) \
	pthread_mutex_lock(&lock_read);\
	id_blocks_read[current_pos_br++]=block_id;\
	pthread_mutex_unlock(&lock_read);

//Aggiunge l'id di un blocco alla struttura id_blocks_write[] in maniera thread safe
#define add_id_block_write(block_id) \
	pthread_mutex_lock(&lock_write);\
	id_blocks_write[current_pos_bw++]=block_id;\
	pthread_mutex_unlock(&lock_write);

//Aggiunge l'id di un blocco alla struttura id_blocks_invalidate[] in maniera thread safe
#define add_id_block_invalidate(block_id) \
	pthread_mutex_lock(&lock_invalidate);\
	id_blocks_invalidate[current_pos_bi++]=block_id;\
	pthread_mutex_unlock(&lock_invalidate);

//Spiazzamento delle system call nella system call table
int INVALIDATE ; 
int PUT ;
int GET ;
int NUM_BLOCK ;

//Barriera per sincronizzare i thread e lanciarli tutti nello stesso momento
pthread_barrier_t barrier;
unsigned count;

//Strutture dati per controllare la buona riuscita del test
int id_blocks_read[NUM_OP_R*NUM_READER];
int id_blocks_invalidate[NUM_OP_I*NUM_INVALIDATOR];
int id_blocks_write[NUM_OP_W*NUM_WRITER];


//indici che tengono traccia della prima posizione libera in cui scrivere in id_blocks_read[], id_blocks_write[], id_blocks_invalidate[]
int current_pos_br=0;
int current_pos_bw=0;
int current_pos_bi=0;

//Lock per accedere in maniera safe alle strutture dai id_blocks_read[], id_blocks_write[], id_blocks_invalidate[]
pthread_mutex_t lock_read;
pthread_mutex_t lock_write;
pthread_mutex_t lock_invalidate;


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
 * Funzione eseguita dei thread scrittori. Ogni thread prova ad eseguire "NUM_OP_W" scritture. 
 */
void *thread_put(void *vargp)
{
    int *myid = (int *)vargp;
	int ret;

	pthread_barrier_wait(&barrier);

	for(int i=0;i<NUM_OP_W;i++){

		char *buffer = (char*)calloc(DIM_MESSAGE ,sizeof(char));
		sprintf(buffer, "Blocco scritto da thread con id  %d : numero blocco %d/5",*myid, i);
		ret = put_data(buffer,strlen(buffer));

		if(ret>0)
			add_id_block_write(ret);

		set_color_yellow();
		printf("%s. Scritto blocco con id: %d.\n",buffer,ret);
		set_color_default();
		
		free(buffer);

	}

    return NULL;
}

/**
 * Funzione eseguita dai thread lettori.Ogni thread esegue "NUM_OP_R" letture.
*/
void *thread_get(void *vargp)
{

    int *myid = (int *)vargp;
	int ret;

	pthread_barrier_wait(&barrier);

	for(int i=0;i<NUM_OP_R;i++){

		char *buffer = (char*)calloc(DIM_MESSAGE , sizeof(char));
		int offset = rand() % NUM_BLOCK + 1;
		ret = get_data(offset,buffer,DIM_MESSAGE);
		
		if(ret>0)
			add_id_block_read(offset);

		set_color_green();
		printf("Thread con id %d legge blocco a offset %d: %s\n",*myid,offset,buffer);
		set_color_default();
		
		free(buffer);

	}

    return NULL;
}


/**
 * Funzione eseguita dai thread invalidatori.Ogni thread esegue "NUM_OP_I" invalidazioni.
*/
void *thread_invalidate(void *vargp)
{
	int *myid = (int *)vargp;
	int ret;

	pthread_barrier_wait(&barrier);

	for(int i =0; i<NUM_OP_I; i++){
		int offset = rand() % (NUM_BLOCK-3) + 3;
		
		
		ret = invalidate_data(offset);

		if(ret>=0){
		   add_id_block_invalidate(offset);
		   set_color_red();
		   printf("Thread con id %d invalida blocco a offset %d\n",*myid,offset);
		   set_color_default();
		}
		
	
	}

    return NULL;
}

//Cerca id di un blocco nella struttura id_blocks_write
int search_into_bw(int id){

	for(int i =0; i<current_pos_bw;i++){
		if(id_blocks_write[i]==id)
			return 1;
	}
}

//Cerca id di un blocco nella struttura id_blocks_invalidate
int search_into_bi(int id){

	for(int i =0; i<current_pos_bi;i++){
		if(id_blocks_invalidate[i]==id)
			return 1;
	}
}

//Verifica se il test è andato a buon fine
int check_test(){

	for(int i =0; i<current_pos_br;i++){
		int id_block = id_blocks_read[i];

		if(search_into_bi(id_block)&&!search_into_bw(id_block))
			return -1;

	}

	return 1;
}

//Invalida tutti i blocchi del device per creare un ambiente di test riproducibile
void inizialize_test(){

	for(int i =3; i<NUM_BLOCK;i++){
		invalidate_data(i);
	}


}

int main(int argc, char** argv){
    
	PUT = atoi(argv[1]);
	GET = atoi(argv[2]);
	INVALIDATE = atoi(argv[3]);
	NUM_BLOCK = atoi(argv[4]);

	count = NUM_READER +  NUM_WRITER + NUM_INVALIDATOR;

	pthread_barrier_init(&barrier, NULL, count);
	//inizialize_test();

	//Strutture per assegnare id ai thread mandati in esecuzione
 	pthread_t thread_id_w[NUM_WRITER];
	pthread_t thread_id_r[NUM_READER];
	pthread_t thread_id_i[NUM_INVALIDATOR];

	//Creo scrittori
	for(int i=0; i< NUM_WRITER; i++){
		pthread_create(&thread_id_w[i], NULL, thread_put, (void*)&thread_id_w[i]);
	}

	//Creo lettori
	for(int i=0; i< NUM_READER; i++){
		pthread_create(&thread_id_r[i], NULL, thread_get, (void*)&thread_id_r[i]);
	}

	//Creo invalidatori
	for(int i=0; i< NUM_INVALIDATOR; i++){
		pthread_create(&thread_id_i[i], NULL, thread_invalidate, (void*)&thread_id_i[i]);
	}
	

	//Attendo fine esecuzione di tutti i thread
	for(int i=0; i< NUM_WRITER; i++){pthread_join(thread_id_w[i], NULL);}
	for(int i=0; i< NUM_READER; i++){pthread_join(thread_id_r[i], NULL);}
	for(int i=0; i< NUM_INVALIDATOR; i++){pthread_join(thread_id_i[i], NULL);}
	
	if(check_test()>0){
		printf("Test andato a buon fine!\n");
	}else{
		printf("Si è verificata un incongruenza! E' stato possibile leggere un blocco invalidato o mai scritto da nessuno scrittore\n");
	}

	return 0;

}

