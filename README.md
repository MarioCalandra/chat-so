Progetto di Sistemi Operativi.

[CLIENT]

Quando il client effettua la connessione al server, quest'ultimo richiede la ricezione di un determinato codice (token). Qualora il client non dovesse comunicare un codice, o dovesse comunicarne uno errato, verrà segnalato un tentativo di connessione da parte di un client sconosciuto e la connessione client-server verrà chiusa.
Per ragioni di mescolamento tra input e output nella stessa schermata, è stato necessario creare un "writer" da cui poter scrivere. Il writer e il client comunicano tramite un'opportuna message queue. Il client funziona tramite due thread: uno è adibito alla ricezione di messaggi provenienti dal writer, che generalmente vengono inoltrati al server; l'altro ha il compito di ricevere e stampare messaggi provenienti dal server. E' stata necessaria la creazione di due funzioni (new_write e new_read) per risolvere alcuni problemi determinati dalla write e dalla read.

