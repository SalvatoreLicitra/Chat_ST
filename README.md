# Chat_ST
[IMPORTANT: the application use the system call POSIX, so it run only on posix systems]

Sequential messaging service developed as a project of the course of Operating Systems (2016/17)

Specifics:
Implementation of a message exchange service supported through a sequential server. The service must accept messages from clients and store them. The client application must provide a user with the following functions:

  1. Read all messages received by the user.
  2. Send a new message to any of the users of the system.
  3. Delete messages received by the user.

The service can only be used by authorized users 
A message must contain at least the Recipient, Subject, and Text fields.

INSTRUCTION:
  
  How to run the server: "./server -p [PORT NUMBER]"
  How to run the client: "./client -a [SERVER ADDRESS] -p[PORT NUMBER]"
  

****************************************************************************************************************
[IMPORTANTE: l'applicazione utilizza le system call di sistema POSIX, quindi è utilizzabile solo sui sistemi posix]

Servizio di messaggistica sequenziale sviluppato come progetto del corso di Sistemi operativi (2016/17)

Specifiche:
Realizzazione di un servizio di scambio messaggi supportato tramite un server sequenziale.
Il servizio deve accettare messaggi provenienti da client ed archiviarli.
L'applicazione client deve fornire ad un utente le seguenti funzioni:
  1. Lettura tutti i messaggi spediti all'utente.
  2. Spedizione di un nuovo messaggio a uno qualunque degli utenti del sistema.
  3. Cancellare dei messaggi ricevuti dall'utente.

Si precisa che il servizio potrà essere utilizzato solo da utenti autorizzati (deve essere quindi previsto un meccanismo di autenticazione).
Un messaggio deve contenere almeno i campi Destinatario, Oggetto e Testo.

ISTRUZIONI:

  Per far partire il server: "./server -p [PORT NUMBER]"
  Per far partire il client: "./client -a [SERVER ADDRESS] -p [PORT NUMBER]"
