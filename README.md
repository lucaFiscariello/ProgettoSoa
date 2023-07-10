# ProgettoSoa

## Introduzione
Il progetto prevede la realizzazione di 3 system call file system dipendent e un driver che è invece supportato dal VFS. Le operazioni da implementare dovranno  leggere e scrivere blocchi di dati su un block device. Nello specifico il block device utilizzato è un file all'interno del quale verrà montato un file system contenente un unico file. Il driver implementato offre le funzionalità per leggere il contenuto dell'unico file, mentre le system call permettono di modificare il contenuto dei blocchi che mantengono a tutti gli effetti il body dell'unico file del file system montato sul device.

## Architettura
Per il raggiungimento di tali obiettivi è stata pensata un architettura del modulo composta da 3 elementi implementati in altrettante directory :

1. Sezione dedica all'implementazione delle system call -> [message-service/system-call](https://github.com/lucaFiscariello/ProgettoSoa/tree/987e2bddcca109c31a2627071eac5f593a7441e8/message-service/system-call)
2. Sezione dedicata all'implementazione del file system e del driver -> [message-service/singlefile-FS](https://github.com/lucaFiscariello/ProgettoSoa/tree/987e2bddcca109c31a2627071eac5f593a7441e8/message-service/singlefile-FS)
3. Sezione dedicata all'implementazione di una libreria di supporto -> [message-service/core-RCU](https://github.com/lucaFiscariello/ProgettoSoa/tree/987e2bddcca109c31a2627071eac5f593a7441e8/message-service/core-RCU)

L'interazioni tra i vari componenti del modulo è descritto dall'immagine seguente. Sia le system call , sia le funzioni implementate dal driver si appoggiano
alla libreria di supporto *RCU-core* che offre una serie di API utili a leggere, scrivere e invalidare blocchi nel file device.

![soaDiagramma drawio (8)](https://github.com/lucaFiscariello/ProgettoSoa/assets/80633764/9e7246f7-6e6b-4338-b824-71a2fa69d46c)

In particolare *RCU-core* può essere visto come l'insieme di altre 3 componenti che hanno responsabilità differenti. Tali componenti sono :

1. [meta_block](https://github.com/lucaFiscariello/ProgettoSoa/blob/987e2bddcca109c31a2627071eac5f593a7441e8/message-service/core-RCU/meta_block.c) : Ai fini di questo progetto il meta blocco è un blocco del device dedicato a mantenere una serie di dati e metadati utili alla gestione della concorrenza tra thread e la buona riuscita delle operazioni di scrittura lettura e invalidazione. 
Questo modulo si occupa di gestire tutte le operazioni connesse a tale blocco, in particolare le operazioni di scrittura, lettura e inizializzazione.
2. [block_read_write](https://github.com/lucaFiscariello/ProgettoSoa/blob/987e2bddcca109c31a2627071eac5f593a7441e8/message-service/core-RCU/block_read_write.c) : Questo componente offre invece api di basso livello che permettono di scrivere e leggere un blocco dal device. E' possibile definire queste api
"primitive" dal momento che si occupano solo di leggere e scrivere blocchi di dati senza preoccuparsi di aggiornare metadati o strutture dati usate in modo condiviso nel modulo.
3. [rcu](https://github.com/lucaFiscariello/ProgettoSoa/blob/987e2bddcca109c31a2627071eac5f593a7441e8/message-service/core-RCU/rcu.c) : Questo componente si occupa invece di gestire tutti i metadati che permettono alle operazioni di invalidazione, lettura e scrittura di andare a buon fine in contesto
di concorrenza. L'idea è che tale componente durante le operazioni di scrittura, lettura e invalidazione crei e popoli i blocchi con dati e metadati e poi sfrutti le api primitive offerte da block_read_write per concretizzare l'operazione sul device.

Complessivamente l'architettura che si viene a creare con le funzioni implementate da ogni componente è la seguente:

![soaDiagramma drawio (12)](https://github.com/lucaFiscariello/ProgettoSoa/assets/80633764/d858bfc1-bdbf-455b-98e1-29d2664107da)


## Device file
Affinchè i servizi offerti dal driver e dalle system call funzionino correttamente è necessario che il device venga montato. In questo modo sarà possibile leggere e scrivere informazioni nei suoi blocchi . In particolare i blocchi mantenuti dal device file sono organizzati nel seguente modo:

1. Il primo blocco ospita il superblocco
2. Il secondo blocco ospita l'inode
3. Il terzo blocco ospita il meta blocco
4. I restanti blocchi ospitano il contenuto dell'unico file del file system


### Metablocco
Il metablocco è implementato come una [struct](https://github.com/lucaFiscariello/ProgettoSoa/blob/987e2bddcca109c31a2627071eac5f593a7441e8/message-service/core-RCU/lib/include/meta_block.h#LL41C1-L69C5). 
Di seguito sono riportati i campi più importanti:

- **firstBlock** : l'id del primo blocco valido in ordine temporale. Supponendo che sul device avvengano una serie di scritture e invalidazioni, il campo firstBlock mantiene l'id del primo blocco valido che è stato scritto temporalmente prima degli altri.Si tratta di un campo dinamico che deve essere opportunamente aggiornato in caso di invalidazione. Il campo è utilizzato dai lettori che vogliono leggere tutti i blocchi in ordine di consegna. L'operazione di lettura di tutti i blocchi verrà dettagliata successivamente.
- **lastWriteBlock**: l'id dell'ultimo blocco scritto nel device. Il campo è utilizzato dagli scrittori e dagli invalidatori per aggiornare i metadati dei blocchi scritti/invalidati. La sua funzionalità verrà dettagliata in seguito.
- **bitMap**: bitMap utilizzata per capire se un blocco è valido o è stato invalidato. Se il bit corrispondente a quel blocco all'interno della bit map è 1, allora il blocco non è valido, altrimenti se è 0 è valido.
E' importante osservare come questa bitmap viene utilizzata *esclusivamente* per capire quali blocchi sono non validi nel momento in cui si effettua il montaggio del device.
Nelle operazioni di read effettuate dai thread lettori questa bit map non è presa in considerazione in quanto potrebbe essere modificata concorrentemtne da uno scrittore e causare inconsistenze.
- **headInvalidBlock**: puntatore ad una linked list che mantiene tutti i nodi non validi. Questa struttura dati è stata creata per rendere la complessità della operazioni di scrittura tempo costante e non lineare. La sua utilità verrà spiegata in dettaglio successivamente.

### Blocco
Un blocco che mantiene una parte del body dell'unico file del file system è modellato dalla struct [block](https://github.com/lucaFiscariello/ProgettoSoa/blob/987e2bddcca109c31a2627071eac5f593a7441e8/message-service/core-RCU/lib/include/meta_block.h#LL75C1-L88C3).
I campi da cui è composta sono :
- **validity** : intero che permette di capire se un blocco è valido o meno. Per verificare se un blocco è valido o meno si consulterà questo campo e non la bitmap. Questo perchè il campo validity può essere acceduto atomicamente al contenuto di tutto il blocco, evitando in questo modo inconsistenze.
- **next_block** : id del blocco valido scritto temporalmente dopo il blocco corrente.
- **pred_block** : id del blocco valido scritto temporalente prima del blocco corrente.
- **data** : dati del blocco

L'idea è quella di sfruttare i metadati di ogni blocco per creare una lista doppiamente collegata di tutti i nodi validi. In questo modo l'operazione di lettura di tutti i blocchi seguendo l'ordine di consegna diventa banale, e consiste solo nello scorrere la lista collegata. Tale lista deve essere doppiamente collegata a causa delle operazioni di invalidazioni. E' importante osservare come i blocchi vengono acceduti in lettura esclusivamente usando *buffer head* e non è presente una duplicazione del contenuto dei blocchi in ram.

## Block_read_write
In questa sezione verrà chiarito come sono state implementate le operazioni primitive di lettura e scrittura nel sotto-modulo [block_read_write](https://github.com/lucaFiscariello/ProgettoSoa/blob/987e2bddcca109c31a2627071eac5f593a7441e8/message-service/core-RCU/block_read_write.c). Le operazioni di lettura e scrittura si basano sul meccanismo del read copy update. I lettori possono effettuare operazioni di lettura in concorrenza senza problemi, mentre è possibile avere attivo un solo scrittore per volta il quale dovrà acquisire un lock. In particolare gli scrittori per srivere un nuovo blocco devono creare un nuovo buffer contenente i dati e i metadati del blocco e in maniera atomica faranno puntare questi dati dal buffer head. L'immagine seguente mostra un esempio di come avviene un operazione di scrittura di un nuovo blocco. In giallo è mostrato il nuovo blocco, in rosso un blocco non valido e in verde i blocchi validi mantenuti in memoria. 

![soaDiagramma drawio (9)](https://github.com/lucaFiscariello/ProgettoSoa/assets/80633764/173708a9-9eab-4800-a737-387946f32b69)


#### Read
L'operazione di [read primitiva](https://github.com/lucaFiscariello/ProgettoSoa/blob/987e2bddcca109c31a2627071eac5f593a7441e8/message-service/core-RCU/block_read_write.c#LL23C1-L53C2) permette di leggere sia i dati che i metadati di un blocco nel device ad un offset dato. La read è implementata in modo che i lettori possano concorrere tra di loro senza problemi, eventuali scrittori vengono invece notificati. Di seguito è riportato il codice del lettore il quale sfrutta alcune API linux rcu:
- **rcu_lock_read()** per avvisare eventuali scrittori che un lettore sta avviando una lettura. Tale API non blocca altri lettori.
- **__atomic_load()** è una semplice assegnazione. Assegna il contenuto di *bh->b_data* in *temp*.In questo pezzo di codice *bh* è il riferimento al buffer head. Tale assegnazione sfrutta però direttive del compilatore in modo da garantire un accesso atomico ai dati.
- **rcu_unlock_read()** libera eventuali scrittori in attesa.
Quando il lettore ha acquisito in modo atomico il riferimento al buffer contenenti i dati di interesse può copiarlo in un buffer locale.

```c 
 
        epoch = rcu_lock_read();
    
        __atomic_load(&bh->b_data, &temp, __ATOMIC_SEQ_CST);
        memcpy( block,temp, DIM_BLOCK);

        rcu_unlock_read(epoch); 

 
```


#### Write
L'operazione di [scrittura primitiva](https://github.com/lucaFiscariello/ProgettoSoa/blob/987e2bddcca109c31a2627071eac5f593a7441e8/message-service/core-RCU/block_read_write.c#LL64C1-L93C2) permette di scrivere un blocco di dati e metadati in blocco ad offset dato. Utilizza le api duali rispetto ai lettori. Di seguito è riportato un pezzo di codice che sfrutta :
- **rcu_synchronize()** attende che scada il greece period.
Una volta scaduto il greece period il thread scrittore ha la certezza che il buffer puntato da temp non venga utilizzato da nessun lettore. Pertanto può essere deallocato. Tutto il meccanismo è basato su creazione di nuovi buffer e swap atomico dei puntatori.

```c 

        update_epoch();

        temp = bh->b_data;
        __atomic_load(&block, &bh->b_data, __ATOMIC_SEQ_CST);
        
        rcu_synchronize();
        kfree(temp);
  
```

## Rcu
In questa sezione verrà invece chiarita l'implementazione del sotto-modulo [rcu](https://github.com/lucaFiscariello/ProgettoSoa/blob/987e2bddcca109c31a2627071eac5f593a7441e8/message-service/core-RCU/rcu.c). Le funzioni che vengono implementate al suo interno vengono direttamente chiamate delle system call e dal driver.

#### Write_rcu
[La funzione di scrittura](https://github.com/lucaFiscariello/ProgettoSoa/blob/987e2bddcca109c31a2627071eac5f593a7441e8/message-service/core-RCU/rcu.c#LL26C1-L76C2) permette di effettuare la scrittura di un buffer di dati in un blocco libero del device driver.
Concettualmente l'operazione di scrittura può essere suddivisa nei seguenti passaggi:
- acquisizione lock in scrittura.
- creazione nuovo blocco : la creazione del nuovo blocco consiste nell'allocare una struttura di tipo block e compilarla con dati e metadati. In particolare i dati sono passati come parametro della funzione, nei metadati è memorizzato l'offset del blocco scritto temporalmente prima del blocco corrente. Quest'ultima informazione è letta in un campo del metablocco.
- aggiornamento blocco precedente: poichè è implementata una lista doppiamente collegata bisogna aggiornare il riferimento al next del blocco scritto temporalmente prima.
- scrittura del blocco sul device : tale scrittura sfrutta l'api primitiva offerta dal sotto-modulo block_read_write.

Per l'individuazione della posizione in cui scrivere il blocco vengono utilizzate due informazioni combinate nella funzione [get_next_free_block()](https://github.com/lucaFiscariello/ProgettoSoa/blob/987e2bddcca109c31a2627071eac5f593a7441e8/message-service/core-RCU/block_read_write.c#LL104C1-L134C1). La funzione leggerà prima il campo *next_free_block* del metablocco che indica quale è la prossima posizione libera in cui poter scrivere un valore. Se questo campo restituisce un valore non accettabile allora si pescherà una posizione libera dalla linked list che memorizza tutti i blocchi non validi. In particolare la linked list è utilizzata come uno stack, per cui si legge solo il valore in testa. Questo garantisce che le operazioni di scrittura abbiano una complessità costante. Di seguito è riportata un immagine che mostra come sono aggiornati i metadati del blocco in seguito ad un operazione di scrittura. In verde sono mostrati i blocchi validi e in rosso il blocco non valido.

![soaDiagramma drawio (11)](https://github.com/lucaFiscariello/ProgettoSoa/assets/80633764/c71909c1-b7ca-43a8-8050-a42a130ab4f8)


#### Read_rcu
[La funzione di lettura](https://github.com/lucaFiscariello/ProgettoSoa/blob/987e2bddcca109c31a2627071eac5f593a7441e8/message-service/core-RCU/rcu.c#LL148C1-L159C2) permette di effettuare una lettura di un blocco ad un dato offset. Durante le operazioni di lettura non c'è necessità di aggiornare
nessuna struttura dati. Pertanto l'operazione di lettura è molto semplice e può essere suddivisa nei seguenti passaggi:
- check per verificare velidità dell'offset. Si verifica che sia un valore positivo minore nel numero di blocchi totali del device
- lettura del blocco ad un dato offset tramite api primitiva offerta dal block_read_write.
- check per verificare la validità del blocco. Se il blocco non è valido è restituito ENODATA.

#### Invalidate_rcu
[L'operazione di invalidazine](https://github.com/lucaFiscariello/ProgettoSoa/blob/987e2bddcca109c31a2627071eac5f593a7441e8/message-service/core-RCU/rcu.c#LL89C1-L146C1) permette di cacellare logicamente un blocco dal device. La cancellazione concettualmente è attuata in due passaggi: il primo consiste nel modificare il campo validity del blocco alzandolo a 1 e rendendolo di fatto un blocco non valido (si aggiorna anche la bitmap nel metablocco). Il secondo consiste nell'aggiornare i riferimeni dei blocchi all'interno della lista doppiamente collegata. L'aggiornamento dei riferimenti fa in modo che il nodo scritto temporalmente prima del nodo che è stato invalidato punti al successivo. L'immagine mostra un esempio di invalidazione e di come vengono aggiornati i metadati mantenuti dai blocchi. In verde sono mostrati i blocchi validi e in rosso quelli non validi.
 
![soaDiagramma drawio (10)](https://github.com/lucaFiscariello/ProgettoSoa/assets/80633764/bc2bf451-3a4b-4252-965f-4862555defd8)

## System call
Grazie all'architettura implementata il codice delle system call diventa molto snello. Le 3 system call richieste non faranno altro che copiare i dati da e verso gli utenti utilizzando la *copyfromuser()* e la *copytouser()* e chiamare una delle tre funzioni offerte dal sotto-module rcu. In particolare il mapping tra system call e funzioni rcu è il seguente:

| System call  | rcu  |
| ------------- | ------------- |
| [put_data](https://github.com/lucaFiscariello/ProgettoSoa/blob/987e2bddcca109c31a2627071eac5f593a7441e8/message-service/system-call/system-call.c#LL39C1-L54C2)  | write_rcu  |
| [get_data](https://github.com/lucaFiscariello/ProgettoSoa/blob/987e2bddcca109c31a2627071eac5f593a7441e8/message-service/system-call/system-call.c#LL56C1-L72C2)  | read_rcu  |
| [invalidate_data](https://github.com/lucaFiscariello/ProgettoSoa/blob/987e2bddcca109c31a2627071eac5f593a7441e8/message-service/system-call/system-call.c#LL74C1-L79C2)  | invalidate_rcu  |

## Driver
Per l'implementazione del driver vale un discorso analogo. Sono state implementate le funzioni di [read](https://github.com/lucaFiscariello/ProgettoSoa/blob/987e2bddcca109c31a2627071eac5f593a7441e8/message-service/singlefile-FS/file.c#LL24C1-L105C2),
[open](https://github.com/lucaFiscariello/ProgettoSoa/blob/987e2bddcca109c31a2627071eac5f593a7441e8/message-service/singlefile-FS/file.c#LL164C1-L177C2) e 
[release](https://github.com/lucaFiscariello/ProgettoSoa/blob/987e2bddcca109c31a2627071eac5f593a7441e8/message-service/singlefile-FS/file.c#LL179C1-L187C2). Tali funzioni permettono di accedere al contenuto dell'unico file del file system montato sul device.
In particolare per l'implementazione della read , che permette la lettura di tutti i blocchi in ordine di consegna, vengono sfruttati i metadati memorizzati all'interno di ogni blocco. Come specificato in precedenza i metadati di ogni blocco permettono di creare una lista doppiamente collegata. Il lettore pertanto non dovrà far altro che leggere l'id del primo blocco valido dal metablocco e scorrere la lista di tutti i blocchi, concatenando ad ogni passo il contenuto di un blocco in un buffer. Nel buffer finale verranno mantenuti solo dati utili, tutti i metadati non verranno presi in considerazione. Il pezzo di codice che effettua lo scorrimento dei blocchi è confinato da una *rcu_read_lock()* e una *rcu_read_unlock()*. 

## In sintesi
La soluzione proposta ha le seguenti caratteristiche:
- I dati dei blocchi del device sono acceduti esclusivamente tramite buffer head, non si mantengono altre strutture dati in memoria.
- L'unica struttura dati aggiuntiva è una linked list dei nodi non validi. La struttura è acceduta da uno scrittore alla volta e da nessun lettore, per cui non si verificano mai problemi di concorrenza.
- Le operazioni di lettura, scrittura e invalidazione hanno tutte complessità costante.
- La concorrenza tra lettori e scrittore è gestita tramime rcu implementata utilizzando wait queue e operazioni atomiche.


Alcune limitazioni:
- la soluzione proposta può gestire fino ad un massimo di 32000 blocchi a causa della dimensione della bitmap. Tuttavia è possibile facilmente estendere il codice e utilizzare altri blocchi del device per estendere la bitmap. Un blocco di metadati per gestire  32000 blocchi sembra un compromesso accettabile.

## Esecuzione codice
> **Warning**
> Se si esegue il codice all'interno di una macchina virtuale, assicurarsi di non lanciare il codice in una cartella condivisa, altrimenti il make test potrebbe fallire.

Il [codice utente](https://github.com/lucaFiscariello/ProgettoSoa/blob/987e2bddcca109c31a2627071eac5f593a7441e8/message-service/system-call/user/user.c) implementa una semplice chiamata alle system call senza considerare la concorrenza tra thread. E' stato implementato anche un [test](https://github.com/lucaFiscariello/ProgettoSoa/blob/987e2bddcca109c31a2627071eac5f593a7441e8/message-service/system-call/user/test.c) che lancia un certo numero di scrittori, lettori e invalidatori in concorrenza. Il test va a buon fine se non si verificano inconsistenze. Un inconsistenza si verifica ad esempio se un lettore riesce a leggere il contenuto di un blocco che è stato invalidato. 

#### Codice utente
Nel make file sono state attuate tutte le configurazioni del caso. Lanciando i seguenti comandi il codice dovrebbe girare senza problemi.
```c 

 make                   #per compilare
 make mount-all         #crea il file system e lo monta, monta tutti i moduli necessari
 make user              #esegue il codice utente
 cat mount/the-file     #per osservare il contenuto del file
  
```
Se si vogliono eseguire ulteriori prove è possibile modificare 
[questo codice](https://github.com/lucaFiscariello/ProgettoSoa/blob/987e2bddcca109c31a2627071eac5f593a7441e8/message-service/system-call/user/user.c) ed eseguire i comandi seguenti:

```c 

 make                   #ricompila
 make user              #esegue il codice utente
  
```


Volendo è possibile visionare il contenuto di tutto il device tramite il seguente comando:
```c 

 make print-blocks
  
```

#### Test
Dopo aver eseguito il codice seguente compariranno sulla console dei messaggi che comunicheranno se il test è andato a buon fine oppure no.

```c 

 make test              #esegue il test
 make umount-all        #smonta tutti i moduli e il file system
  
```
