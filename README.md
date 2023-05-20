# ProgettoSoa
## Architettura
Il progetto prevede la realizzazione di 3 system call file system dipendent e un dirver che è invece supportato dal VFS. Per il raaggiungimento di tale obiettivi
è stata pensata un architettura del modulo che permettesse di organizzare il codice in maniera più chiara possibile. Dal punto di vista architetturale il modulo si
compone di 3 elementi che corrispondono ad altrettante directory :

1. Sezione dedica all'implementazione delle system call -> [message-service/system-call](https://github.com/lucaFiscariello/ProgettoSoa/tree/0024a5ef8ca604a3026442dd7e17773451ac1bc9/message-service/system-call)
2. Sezione dedicata all'implementazione del driver -> [message-service/singlefile-FS](https://github.com/lucaFiscariello/ProgettoSoa/tree/0024a5ef8ca604a3026442dd7e17773451ac1bc9/message-service/singlefile-FS)
3. Sezione dedicata all'implemntazione di una libreria di supporto -> [message-service/core-RCU](https://github.com/lucaFiscariello/ProgettoSoa/tree/0024a5ef8ca604a3026442dd7e17773451ac1bc9/message-service/core-RCU)

L'interazioni tra i vari componenti del modulo è descritto dall'immagine seguente. Sia le system call , sia le funzioni implementate dal drive si appoggiano
sulla libreria di supporto "core-RCU" che offre una serie di api utili a leggere, scrivere e invalidari blocchi nel file device.

![soaDiagramma drawio](https://github.com/lucaFiscariello/ProgettoSoa/assets/80633764/0f393e0f-8c02-4589-8558-57f4bb3695af)

In particolare "core-RCU" può essere visto come l'insieme di altre 3 componenti che hanno responsabilità differenti. Tali componenti sono :

1. [meta_block](https://github.com/lucaFiscariello/ProgettoSoa/blob/0024a5ef8ca604a3026442dd7e17773451ac1bc9/message-service/core-RCU/meta_block.c) : Ai fini di questo progetto il meta blocco è un blocco del device dedicato a mantenere una serie di dati e metadati utili alla gestione la concorrenza tra thread e la buona riuscita delle operazioni di scrittura lettura e invalidazione. 
Questo modulo si occupa di gestire tutte le operazioni connesse a tale blocco, in particolare le operazioni di scrittura, lettura e inizializzazione.
2. [block_read_write](https://github.com/lucaFiscariello/ProgettoSoa/blob/0024a5ef8ca604a3026442dd7e17773451ac1bc9/message-service/core-RCU/block_read_write.c) : Questo componente offre invece api di basso livello che permettono di scrivere e leggere un blocco dal device. E' possibile definire queste api
"primitive" dal momento che si occupano solo di leggere e scrivere blocchi di dati senza preoccuparsi di aggiornare metadati o strutture dati usate in modo condivisa nel modulo.
3. [rcu](https://github.com/lucaFiscariello/ProgettoSoa/blob/0024a5ef8ca604a3026442dd7e17773451ac1bc9/message-service/core-RCU/rcu.c) : Questo componente si occupa invece di gestire tutti i metadati che permettono alle operazioni di invalidazione, lettura e scrittura di andare a buon fine in contesto
di concorrenza. L'idea è che tale componente crei e popoli con i metadati tutti i blocchi che devono essere scritti in memoria e poi sfrutti le api primitive offerte
da block_read_write per concretizzare l'operazione sul device.

Complessivamente l'architettura che si viene a creare con le funzioni implementate da ogni componente è il seguente:

![soaDiagramma drawio (1)](https://github.com/lucaFiscariello/ProgettoSoa/assets/80633764/71b2b630-e8d6-4476-9b6c-72eee38f6028)

## Device file
Affinchè le funzionalità offerte dal driver e le system call funzionino correttamente è necessario che sul device file venga montato un file system. 
In particolare il device file è organizzato nel seguente modo:

1. Il primo blocco ospita il superblocco
2. Il secondo blocco ospita l'inode
3. Il terzo blocco ospita il meta blocco
4. I restanti blocchi ospitano il contenuto dell'unico file del file system

### Metablocco
Il metablocco è implementato come una [struct](https://github.com/lucaFiscariello/ProgettoSoa/blob/0024a5ef8ca604a3026442dd7e17773451ac1bc9/message-service/core-RCU/lib/include/meta_block.h#LL41C1-L69C5). 
Di seguito sono riportati i più importanti:

- firstBlock : l'id del primo blocco valido in ordine temporale. Supponendo che sul device avvengano una serie di scritture e invalidazioni, il campo firstBlock mantiene l'id del primo blocco valido che è stato scritto temporalmente prima degli altri.
si tratta di un campo dinamico che deve essere opportunamente aggiornato in caso di invalidazione. Il campo è utilizzato dai lettori che vogliono leggere tutti i blocchi
in ordine di consegna. L'operazione di lettura di tutti i blocchi verrà dettagliata successivamente.
- lastWriteBlock: l'id dell'ultimo blocco scritto nel device. Il campo è utilizzato dagli scrittori e dagli invalidatori per aggiornare i metadati dei blocchi scritti/invalidati. La sua funzionalità verrà dettagliata in seguito.
- bitMap: bitMap utilizzata per capire se un blocco è valido o è stato invalidato. Se il bit corrispondente a quel blocco all'interno della bit map è 1, allora il blocco non è valido, altrimenti se è 0 è valido.
E' importante osservare come questa bitmap viene utilizzata *esclusivamente* per capire quali blocchi sono non validi nel momento in cui si effettua il montaggio del device.
Nelle operazioni di read effettuate dai thread lettori questa bit map non è presa in considerazione in quanto potrebbe essere modificata concorrentemtne da uno scrittore 
e causare inconsistenze.
- headInvalidBlock: puntatore ad una linked list che mantiene tutti i nodi non validi. Questa struttura dati è stata creata per rendere la complessità della operazioni di scrittura 
tempo costante e non lineare. La sua utilità verrà spiegata in dettaglio successivamente.

### Blocco
Un blocco che mantiene il body dell'unico file del file system è modellato dalla struct [block](https://github.com/lucaFiscariello/ProgettoSoa/blob/0024a5ef8ca604a3026442dd7e17773451ac1bc9/message-service/core-RCU/lib/include/meta_block.h#LL75C1-L88C3).
I campi da cui è composta sono :
- validity : intero che permette di capire se un blocco è valido o meno. Un thread lettore a livello kernel dopo aver effettuato l'operazione di lettura e prima di rilasciare il 
buffer letto a livello user, controllerà la validità del blocco. Se il blocco è valido lo restituisce, altrimenti verrà restituito ENODATA. Questa soluzione sicuramente 
risulta essere poco efficiente dal momento che se un blocco è non valido comunque esso è letto, tuttavia è comunque una soluzione che garantisce atomicità e non crea 
inconsistenza. L'altra soluzione poteva essere quella di controllare il valore della bitmap, ma si sarebbe dovuto gestire anche il fatto che la stessa informazione è duplicata in due posizioni diverse.
- next_block : id del blocco valido scritto temporalmente dopo il blocco corrente.
- pred_block : id del blocco valido scritto temporalente prima del blocco corrente.

L'idea è quella di sfruttare i metadati di ogni blocco per creare una lista doppiamente collegata di tutti i nodi validi. In questo modo l'operazione di lettura di 
tutti i blocchi seguendo l'ordine di consegna diventa banale, e consiste solo nello scorrere la lista collegata. Tale lista deve essere doppiamente collegata a causa 
delle operazioni di invalidazioni.

## Block_read_write
In questa sezione verrà chiarito come sono state implementate le operazioni primitive di lettura e scrittura. Le operazioni di lettura e scrittura si basano 
sulle api linux basate su rcu. Le operazioni di lettura e scrittura utilizzano quindi il meccanismo del read copy update. Pertanto i lettori possono effettuare operazioni 
di lettura in concorrenza senza problemi, mentre è possibile avere attivo un solo scrittore per volta il quale dovrà acquisire un lock.

#### Read
Di seguito è riportato il codice del lettore il quale sfrutta:
- rcu_read_lock() per avvisare eventuali scrittori che un lettore sta avviando una lettura. Tale API non blocca altri lettori.
- rcu_dereference() è una semplice assegnazione. Assegna il contenuto di bh->b_data in temp. Tale assegnazione sfrutta però direttive e ottimizzazioni del 
compilatore in modo da garantire un ordinamento degli accessi in memoria tra scrittori e lettori.
- rcu_read_unlock() libera eventuali scrittori in attesa.
Quando il lettore ha acquisito in modo atomico il riferimento al buffer contenenti i dati di interesse può copiarlo in un buffer locale.

```c 
 
 rcu_read_lock();
 temp = (void*) rcu_dereference(bh->b_data);
 memcpy( block,temp, DIM_BLOCK);
 rcu_read_unlock(); 
 
```

Il codice completo della lettura è presente [qui](https://github.com/lucaFiscariello/ProgettoSoa/blob/0024a5ef8ca604a3026442dd7e17773451ac1bc9/message-service/core-RCU/block_read_write.c#LL23C1-L53C2).

#### Write
L'operazione di scrittura primitiva utilizza le api duali rispetto ai lettori. Di seguito è riportato un pezzo di codice che sfrutta :
- rcu_assign_pointer() che è simile a rcu_dereference(). La documentazione del kernel linux consiglia tuttavia di utilizzare rcu_assign_pointer per i lettori e rcu_dereference per gli scrittori.
- synchronize_rcu() attende che scada il greece period.
Una volta scaduto il greece period il thread scrittore ha la certezza che il buffer puntato da temp non venga utilizzato da nessun lettore. Pertanto può essere deallocato.

```c 

  temp = bh->b_data;
  rcu_assign_pointer(bh->b_data,(void *) block);
  synchronize_rcu();
  kfree(temp);
  
```

Il codice completo della scrittura è presente [qui](https://github.com/lucaFiscariello/ProgettoSoa/blob/0024a5ef8ca604a3026442dd7e17773451ac1bc9/message-service/core-RCU/block_read_write.c#LL135C1-L165C1).

#### Read_all_block
Per la lettura di tutti i blocchi in ordine di consegna vengono sfruttati i metadati memorizzati all'interno di ogni blocco. Come specificato in precedenza i metadati
di ogni blocco permettono di creare una lista doppiamente collegata. Il lettore pertanto non dovrà fare altro che leggere l'id del primo blocco valido dal metablocco
e scorrere la lista di tutti i blocchi, concatenando ad ogni passo il contenuto di un blocco in un buffer. Nel buffer finale verranno mantenuti solo dati utili, tutti i metadati
non verranno presi in considerazione. Il pezzo di codice che effettua lo scorrimento dei blocchi è confinato da una rcu_read_lock() e una rcu_read_unlock().
Il codice completo della lettura è presente [qui](https://github.com/lucaFiscariello/ProgettoSoa/blob/0024a5ef8ca604a3026442dd7e17773451ac1bc9/message-service/core-RCU/block_read_write.c#LL64C1-L126C1).





