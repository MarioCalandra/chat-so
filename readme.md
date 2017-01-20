#Progetto di Sistemi Operativi

###CLIENT

Quando il client effettua la connessione al server, quest'ultimo richiede la ricezione di un determinato codice (*token*). Qualora il client non dovesse comunicare un codice, o dovesse comunicarne uno errato, verrà segnalato un *tentativo di connessione da parte di un client sconosciuto* e la connessione client-server verrà chiusa.
Per ragioni di mescolamento tra input e output nella stessa schermata, è stato necessario creare un writer da cui poter scrivere. Il writer e il client comunicano tramite un'opportuna *message queue*. Il client funziona tramite due thread: uno è adibito alla ricezione di messaggi provenienti dal writer, che generalmente vengono inoltrati al server; l'altro ha il compito di ricevere e stampare messaggi provenienti dal server. E' stata necessaria la creazione di due funzioni (`new_write` e `new_read`) per risolvere alcuni problemi determinati dalla `write` e dalla `read`.
All'esecuzione, bisognerà specificare il proprio *nickname* come primo argomento. 

###WRITER

E' composto da un semplice algoritmo capace di inoltrare quanto viene scritto al client.

###SERVER

Il server è capace di gestire più client contemporaneamente grazie all'uso della funzione `select`: l'algoritmo è stato studiato, preso dal *Beginning Linux* e successivamente modificato opportunamente. Esso è capace di comunicare con i client in maniera estremamente intuitiva tramite opportune funzioni da me realizzate (vedi `sendClientMessage`, `sendAllMessage`, `sendToOtherClients`, `sendRoomMessage`).
Ad ogni client viene associato un *file descriptor* che ne rappresenta l'ID di riferimento; in questo modo risulta più semplice ed immediato il riconoscimento di ogni client. Ognuno di questi è caratterizzato da una opportuna *struct* in cui sono memorizzate informazioni di vario tipo.
Quando un client si connette, il server valuterà se il *nickname* associato sia registrato o meno:

* Se il nickname è presente in database, il server chiederà la password per effettuare il login;
* Se il nickname non è presente in database, il server richiederà la registrazione.

Una volta effettuato il login, all'utente sarà mostrata una panoramica delle *stanze* disponibili.

###COMANDI

Per interagire con il server, è indispensabile che l'utente faccia uso di determinati *comandi* da scrivere direttamente nel writer. Ognuno di essi ha la particolarità di essere preceduto dal simbolo `/` per far capire al server di avere a che fare con un comando e non con un semplice testo. Di seguito viene mostrata una lista dei comandi disponibili, con breve spiegazione annessa.

/help
: Mostra la lista dei comandi, mostrando solo quelli usufruibili dall'utente



