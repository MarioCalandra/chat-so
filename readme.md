# PROGETTO DI SISTEMI OPERATIVI

Per ragioni di mescolamento tra input e output nella stessa schermata, è stato necessario creare un writer da cui poter scrivere. Il writer e il client comunicano tramite un'opportuna *message queue*. E' composto da un semplice algoritmo capace di inoltrare quanto viene scritto al client.

## CLIENT

Il client funziona tramite due thread: uno è adibito alla ricezione di messaggi provenienti dal writer, che generalmente vengono inoltrati al server; l'altro ha il compito di ricevere e stampare messaggi provenienti dal server. E' stata necessaria la creazione di due funzioni (`new_write` e `new_read`) per risolvere alcuni problemi determinati dalla `write` e dalla `read`.
All'esecuzione, bisognerà specificare il proprio *nickname* come primo argomento. 

## SERVER

Il server è capace di gestire più client contemporaneamente grazie all'uso della funzione `select`: l'algoritmo è stato studiato, preso dal *Beginning Linux* e successivamente modificato opportunamente. Esso è capace di comunicare con i client in maniera estremamente intuitiva tramite opportune funzioni da me realizzate (vedi `sendClientMessage`, `sendAllMessage`, `sendToOtherClients`, `sendRoomMessage`).
Ad ogni client viene associato un *file descriptor* che ne rappresenta l'ID di riferimento; in questo modo risulta più semplice ed immediato il riconoscimento di ogni client. Ognuno di questi è caratterizzato da una opportuna *struct* in cui sono memorizzate informazioni di vario tipo.
Quando un client si connette, il server valuterà se il *nickname* associato sia registrato o meno:

* Se il nickname è presente in database, il server chiederà la password per effettuare il login;
* Se il nickname non è presente in database, il server richiederà la registrazione.

Una volta effettuato il login, all'utente sarà mostrata una panoramica delle *stanze* disponibili.

## INFORMAZIONI

Le informazioni di *utenti* e *stanze* sono salvate all'interno di due file `.csv`. Il server ne gestisce il salvataggio e il caricamento tramite opportune funzioni (vedi `loadRoomData`, `loadAllRoomData`, `saveRoomData`, `deleteRoomData`, `loadUserData`, `saveUserData`, `deleteUserData`). Nel file `users.csv` ad ogni entry corrisponde un nome, la password e il rank (variabile da 1 a 3). Nel file `rooms.csv` ad ogni entry corrisponde il nome della stanza, la password (*public* se pubblica), il nome dell'utente moderatore e il numero massimo di persone ospitabili. Nei file `log.txt` e `ban.txt` sono salvati rispettivamente i log del server, ossia i *messaggi* inviati dai client e le *azioni* commesse da ognuno di essi, e gli indirizzi IP bannati.

## COMANDI

Per interagire con il server, è indispensabile che l'utente faccia uso di determinati *comandi* da scrivere direttamente nel writer preceduti dal simbolo `/`. Di seguito viene mostrata una lista dei comandi disponibili, con breve spiegazione annessa.

- */help*: mostra una panoramica dei comandi *usufruibili* dall'utente
- */register*: permette ad un utente non registrato di registrarsi
- */login*: necessario per effettuare il login
- */setpassword*: serve a cambiare la propria password
- */pm*: permette una comunicazione tra due utenti tramite *Private Message*
- */clear*: esegue *clear* nel client per pulire la shell (operazione effettuata tramite una `fork` e una `execlp` nel client)
- */exit*: disconnette il client dal server
- */refresh*: aggiorna la panoramica delle stanze
- */join*: permette di accedere ad una stanza
- */leave*: fa abbandonare la stanza in cui ci si trova
- */list*: mostra la lista di utenti all'interno della propria stanza
- */makeroom*: permette di creare una stanza (solo gli *utenti avanzati* possono farlo)

Di seguito i comandi usufruibili solo dal *moderatore* di una stanza (o da un *admin*):

- */removeroom*: rimuove la stanza inserita
- */kickr*: permette di kickare un utente dalla propria stanza
- */setrpassword*: serve a cambiare la password della stanza

Di seguito i comandi usufruibili solo da un *admin*:

- */kick*: kicka un utente dal server
- */broadcast*: permette di inviare un messaggio in broadcast
- */banip*: banna un IP permanentemente dal server
- */setrank*: permette di settare il rank di un utente
- */remove*: rimuove dal database un utente

All'interno del codice sono stati definiti un comando e una password segreta che permettono all'utente di accedere con permessi admin. Di default, comando e password sono */admin* e *admin*.

## SICUREZZA

Quando il client effettua la connessione al server, quest'ultimo richiede la ricezione di un determinato codice (*token*). Qualora il client non dovesse comunicare un codice, o dovesse comunicarne uno errato, verrà segnalato un *tentativo di connessione da parte di un client sconosciuto* e la connessione client-server verrà chiusa. Per prevenire tentativi di Code Injection, si è cercato quanto il più possibile di evitare il salvataggio di dati contenenti il simbolo `,`. Non è possibile una connessione da parte di due o più client aventi lo stesso IP.
