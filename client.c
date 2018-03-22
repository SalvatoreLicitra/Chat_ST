#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/signal.h>
#include <inttypes.h>
#include <fcntl.h>

#define SERVICE_DIM 25 //dimensione messaggi di servizio tra client e server
#define USR_DIM 20	//dimensione massima di username/password
#define OBJ_DIM 50	//dimensione massima dell'oggetto
#define TEXT_DIM 256	//dimensione massima del testo del messaggio

typedef struct Messaggio{
	int code; //codice univoco
	char mittente[USR_DIM];	//salvo il mittente del messaggio
	char destinatario[USR_DIM];	//salvo il destinatario del messaggio
	char oggetto[OBJ_DIM];	//oggetto del messaggio
	char testo[TEXT_DIM];	//testo del messaggio
}Messaggio;

//variabili globali
struct sockaddr_in server;//indirizzo del server
char username[USR_DIM];//username del client
int ds_socket;//descittore socket
int flag_socket = 0;//indica se la socket è aperta o meno

//dichiarazione funzioni:
int chat_read();
int chat_write();
int chat_delete();
void leggi_frase(char*dest,int max_dim);
int leggi_numero();
int login();
int nuovo_utente();
int ParseCmdLine(int argc, char *argv[], char **szAddress, char **szPort);

//funzioni gestioni eventi

void gestiscichiusura(){
	printf("\nchiusura client...\n");
	if(flag_socket == 1){
		if(close(ds_socket) == -1){
			printf("\nerrore chiusura socket\n");
			exit(EXIT_FAILURE);
		}
	}
	exit(EXIT_SUCCESS);
}

void gestisci_sigill(){
	printf("\noperazione non consentita, o locazione di memoria non disponibile. CHIUSURA");
	if(flag_socket==1){
		if(close(ds_socket) == -1){
			printf("\nerrore chiusura socket");
			exit(EXIT_FAILURE);
		}
	}
	exit(EXIT_FAILURE);
}

void gestisci_disconnessione(){
	printf("problemi di connessione, annullamento operazione in corso");
	if(flag_socket==1){
		if(close(ds_socket) == -1){
			printf("\nerrore chiusura socket");
			exit(EXIT_FAILURE);
		}
	}
	exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    short int port;
    char *szAddress;
    char *szPort;
    char *endptr;
	struct hostent *he;
	he=NULL;

	//gestione eventi
	struct sigaction sa;
	sa.sa_flags = 0;
	sigfillset(&sa.sa_mask);
	sa.sa_handler = gestiscichiusura;
	if(sigaction(SIGINT,&sa,NULL) == -1){	//<---ctrl + c
		printf("sigaction\n");
		exit(EXIT_FAILURE);
	}
	if(sigaction(SIGQUIT,&sa,NULL) == -1){	//<---ctrl + '\'
		printf("sigaction\n");
		exit(EXIT_FAILURE);
	}
	if(sigaction(SIGHUP,&sa,NULL) == -1){	//<---chiusura terminale
		printf("sigaction\n");
		exit(EXIT_FAILURE);
	}
	if(sigaction(SIGTERM,&sa,NULL) == -1){	//<---terminazione
		printf("sigaction\n");
		exit(EXIT_FAILURE);
	}
	sa.sa_handler=gestisci_sigill;
	if(sigaction(SIGILL,&sa,NULL) == -1){	//<---illegal instruction
		printf("sigaction\n");
		exit(EXIT_FAILURE);
	}
	if(sigaction(SIGSEGV,&sa,NULL) == -1){	//<---segmentation fault
		printf("sigaction\n");
		exit(EXIT_FAILURE);
	}
	sa.sa_handler = gestisci_disconnessione;
	if(sigaction(SIGPIPE,&sa,NULL) == -1){	//<---per la connessione
		printf("sigaction\n");
		exit(EXIT_FAILURE);
	}
	
    ParseCmdLine(argc, argv, &szAddress, &szPort);

    port = strtol(szPort, &endptr, 0);
    if ( *endptr ){
		printf("client: porta non riconosciuta.\n");
		exit(EXIT_FAILURE);
    }
	if ( (ds_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0 )	{
		fprintf(stderr, "client: errore durante la creazione della socket.\n");
		exit(EXIT_FAILURE);
    }

	memset(&server, 0, sizeof(server));
	server.sin_family      = AF_INET;
    server.sin_port        = htons(port);

    if(inet_aton(szAddress, &server.sin_addr) <= 0 ){
		printf("client: indirizzo IP non valido.\nclient: risoluzione nome...");
		if ((he=gethostbyname(szAddress)) == NULL){
			printf("fallita.\n");
  			exit(EXIT_FAILURE);
		}
		printf("riuscita.\n\n");
		server.sin_addr = *((struct in_addr *)he->h_addr);
    }
	printf("inizializzazione completata..\n");
	printf("sessione --> Login/Registrazione\n");
	
	//autenticazione
	while(1){
		printf("Autenticazione: sei un nuovo utente? [0: sì - altro numero: no] =>  ");
		int nuovo;
		nuovo = leggi_numero();
		if(nuovo == 0){ //nuovo utente -> registrazione
			if(nuovo_utente() == -1) printf("impossibile creare nuovo utente, prova con un nuovo username\n");
			else break;
		}
		else{ //devo fare il login
			if(login() == -1) printf("\nlogin fallito, riprovare\n");
			else break;
		}
	}
	printf("\nconnessione riuscita correttamente, ciao %s!\navvio sessione...\n",username);
	//sessione
	while(1){
		int comando;
		printf("\n+------------------------------------------------------+");
		printf("\nscegliere operazione (digita uno dei seguenti comandi)\n -[1] Leggi messaggi\n -[2] Scrivi nuovo messaggio\n -[3] Cancella messaggio\n -[0] Logout\n --> ");
		comando = leggi_numero();
		printf("\n+------------------------------------------------------+\n");
		switch(comando){
			case 1: //leggi messaggi
				chat_read();
				break;
			case 2: //scrivi nuovo nessaggio
				chat_write();
				break;
			case 3: //cancella messaggio
				chat_delete();
				break;
			case 0: //chiudi sessione
				printf("logout in corso... ciao!\n");
				exit(EXIT_SUCCESS);
				break;
			default: //errore input
				printf("comando inserito non valido, riprova\n");
		}
	}
	exit(0);
}

int chat_read(){
	if((ds_socket = socket(AF_INET,SOCK_STREAM,0)) == -1){
		printf("ERRORE: durante la creazione socket (read)\n");
		exit(EXIT_FAILURE);
	}
	flag_socket = 1;
	printf("Lettura...\n");
	int comando = 1;
	if(connect(ds_socket,(struct sockaddr*)&server,sizeof(server)) == -1){
		close(ds_socket);
		printf("ERRORE: durante la connessione\n");
		exit(EXIT_FAILURE);
	}
	if(send(ds_socket,&comando,sizeof(int),0) == -1){
		printf("ERRORE: durante l'invio\n");
		if(close(ds_socket) == -1){
			printf("ERRORE: chiusura socket\n");
			exit(EXIT_FAILURE);
		}
		return -1;
	}
	printf("|**************MESSAGGI**************|\n");
	char service[SERVICE_DIM];
	int i = 0,r=0;
	while(1){
		if(recv(ds_socket,&service,SERVICE_DIM,MSG_WAITALL) == -1){
			printf("\nERRORE: durante la ricezione\n");
			if(close(ds_socket) == -1){
				printf("\nERRORE: chiusura socket\n");
				exit(EXIT_FAILURE);
			}
			return -1;
		}
		if(strcmp(service,"fine") == 0) break;
		int code;
		if(recv(ds_socket,&code,sizeof(int),MSG_WAITALL) == -1){
			printf("\nERRORE: durante la ricezione\n");
			if(close(ds_socket) == -1){
				printf("\nERRORE: chiusura socket\n");
				exit(EXIT_FAILURE);
			}
			return -1;
		}
		if(code != -1) r=1;
		if(i==0 && code == -1) {
			if(r==0)printf("\n\tNon hai messaggi recenti\n\n");
			else printf("\n -Fine messaggi recenti\n\n");
			printf("  **********************************  \n");
			printf(" -Lettura Archivio\n\n[tali messaggi non sono cancellabili]\n");
			i=1;
		}
		char m[USR_DIM];
		if(recv(ds_socket,m,USR_DIM,MSG_WAITALL) == -1){
			printf("\nERRORE: durante la ricezione\n");
			if(close(ds_socket) == -1){
				printf("\nERRORE: chiusura socket\n");
				exit(EXIT_FAILURE);
			}
			return -1;
		}
		char obj[OBJ_DIM];
		if(recv(ds_socket,obj,OBJ_DIM,MSG_WAITALL) == -1){
			printf("\nERRORE: durante la ricezione\n");
			if(close(ds_socket) == -1){
				printf("\nERRORE: chiusura socket\n");
				exit(EXIT_FAILURE);
			}
			return -1;
		}
		char txt[TEXT_DIM];
		if(recv(ds_socket,txt,TEXT_DIM,MSG_WAITALL) == -1){
			printf("\nERRORE: durante la ricezione\n");
			if(close(ds_socket) == -1){
				printf("\nERRORE: chiusura socket\n");
				exit(EXIT_FAILURE);
			}
			return -1;
		}
		printf("\n\t------messaggio------\n\tcode:#%d\n\tmittente: |%s|\n\toggetto: |%s|\n\ttesto: |%s|\n\t---------------------\n",code,m,obj,txt);
	}
	printf("\n\n|         fine dei messaggi          |\n");
	printf("**************************************\n");
	if(close(ds_socket) == -1){
		printf("\nERRORE: chiusura socket\n");
		exit(EXIT_FAILURE);
	}
	flag_socket = 0;
	return 0;
}

int chat_write(){
	if ((ds_socket=socket(AF_INET,SOCK_STREAM,0)) == -1){
		printf("ERRORE: durante la creazione socket (write)\n");
		exit(EXIT_FAILURE);
	}
	flag_socket = 1;
	printf("Nuovo messaggio:\n");
	int comando = 2;
	printf("inserisci il destinatario: ");
	char d[USR_DIM];
	leggi_frase(d,USR_DIM);
	printf("inserisci l'oggetto: ");
	char o[OBJ_DIM];
	leggi_frase(o,OBJ_DIM);
	printf("inserisci il testo del messaggio: ");
	char t[TEXT_DIM];
	leggi_frase(t,TEXT_DIM);
	if(connect(ds_socket,(struct sockaddr*)&server,sizeof(server)) == -1){
		close(ds_socket);
		printf("ERRORE: durante la connessione\n");
		exit(EXIT_FAILURE);
	}
	if(send(ds_socket,&comando,sizeof(int),0) == -1){
		printf("ERRORE: durante l'invio\n");
		if(close(ds_socket) == -1){
			printf("ERRORE: chiusura socket\n");
			exit(EXIT_FAILURE);
		}
		return -1;
	}
	char response[SERVICE_DIM];
	do{
		if(send(ds_socket,d,USR_DIM,0) == -1){
			printf("ERRORE: durante l'invio\n");
			if(close(ds_socket) == -1){
				printf("ERRORE: chiusura socket\n");
				exit(EXIT_FAILURE);
			}
			return -1;
		}
		if(recv(ds_socket,response,SERVICE_DIM,MSG_WAITALL) == -1){
			printf("\nERRORE: durante la ricezione\n");
			if(close(ds_socket) == -1){
				printf("\nERRORE: chiusura socket\n");
				exit(EXIT_FAILURE);
			}
			return -1;
		}
		if(strcmp(response,"ok") == 0){
			printf("preview messaggio\n");
			printf("\n\t------messaggio------\n\tdesinatario: |%s|\n\toggetto: |%s|\n\ttesto: |%s|\n\t---------------------\n",d,o,t);
			break;
		}
		else{
			printf("\nATTENZIONE: %s non è un utente affiliato al servizio",d);
			printf("\ninserisci un destinatario valido: ");
			leggi_frase(d,USR_DIM);
		}
	}while(1);
	printf("vuoi modificare il messaggio inserito? [y/n]  ");
	char ris[SERVICE_DIM];
	leggi_frase(ris,SERVICE_DIM);
	if(strcmp(ris,"y") == 0 || strcmp(ris,"Y") == 0){
		printf("inserisci l'oggetto: ");
		leggi_frase(o,OBJ_DIM);
		printf("inserisci il testo del messaggio: ");
		leggi_frase(t,TEXT_DIM);
		printf("preview messaggio\n");
		printf("\n\t------messaggio------\n\tdesinatario: |%s|\n\toggetto: |%s|\n\ttesto: |%s|\n\t---------------------\n",d,o,t);
	}
	if(send(ds_socket,o,OBJ_DIM,0) == -1){
		printf("ERRORE: durante l'invio\n");
		if(close(ds_socket) == -1){
			printf("ERRORE: chiusura socket\n");
			exit(EXIT_FAILURE);
		}
		return -1;
	}
	if(send(ds_socket,t,TEXT_DIM,0) == -1){
		printf("ERRORE: durante l'invio\n");
		if(close(ds_socket) == -1){
			printf("ERRORE: chiusura socket\n");
			exit(EXIT_FAILURE);
		}
		return -1;
	}
	char response1[SERVICE_DIM];
	if(recv(ds_socket,response1,SERVICE_DIM,0) == -1){
		printf("\nERRORE: durante la ricezione\n");
		if(close(ds_socket) == -1){
			printf("\nERRORE: chiusura socket\n");
			exit(EXIT_FAILURE);
		}
		return -1;
	}
	if(close(ds_socket) == -1){
		printf("\nERRORE: chiusura socket\n");
		exit(EXIT_FAILURE);
	}
	flag_socket = 0;
	if(strcmp(response1,"ok") == 0){
		printf("invio avvenuto correttamente\n");
		return 0;
	}
	printf("spiacenti, si è verificato un errore, riprovare\n");
	return -1; 
}

int chat_delete(){
	if((ds_socket = socket(AF_INET,SOCK_STREAM,0)) == -1){
		printf("ERRORE: durante la creazione socket (login)\n");
		exit(EXIT_FAILURE);
	}
	flag_socket = 1;
	int comando;
	//parte 1: lista messaggi eliminabili (quelli in lista)
	printf("Eliminazione...\n");
	comando = 3;
	if(connect(ds_socket,(struct sockaddr*)&server,sizeof(server)) == -1){
		close(ds_socket);
		printf("ERRORE: durante la connessione\n");
		exit(EXIT_FAILURE);
	}
	//invio comando
	if(send(ds_socket,&comando,sizeof(int),0) == -1){
		printf("ERRORE: durante l'invio\n");
		if(close(ds_socket) == -1){
			printf("ERRORE: chiusura socket\n");
			exit(EXIT_FAILURE);
		}
		return -1;
	}
	int msg_num;
	if(recv(ds_socket,&msg_num,sizeof(int),MSG_WAITALL) == -1){
			printf("\nERRORE: durante la ricezione\n");
			if(close(ds_socket) == -1){
				printf("\nERRORE: chiusura socket\n");
				exit(EXIT_FAILURE);
			}
			return -1;
	}
	int esiste = 0,i,c=0;
	int lista[msg_num];
	for(i=0;i<msg_num;i++) lista[i] = 0;
	printf("|**************MESSAGGI**************|\n");
	char service[SERVICE_DIM];
	while(1){
		if(recv(ds_socket,&service,SERVICE_DIM,MSG_WAITALL) == -1){
			printf("\nERRORE: durante la ricezione\n");
			if(close(ds_socket) == -1){
				printf("\nERRORE: chiusura socket\n");
				exit(EXIT_FAILURE);
			}
			return -1;
		}
		if(strcmp(service,"fine") == 0) break;
		esiste = 1;
		int code;
		if(recv(ds_socket,&code,sizeof(int),MSG_WAITALL) == -1){
			printf("\nERRORE: durante la ricezione\n");
			if(close(ds_socket) == -1){
				printf("\nERRORE: chiusura socket\n");
				exit(EXIT_FAILURE);
			}
			return -1;
		}
		lista[c]=code;
		c++;
		char m[USR_DIM];
		if(recv(ds_socket,m,USR_DIM,MSG_WAITALL) == -1){
			printf("\nERRORE: durante la ricezione\n");
			if(close(ds_socket) == -1){
				printf("\nERRORE: chiusura socket\n");
				exit(EXIT_FAILURE);
			}
			return -1;
		}
		char obj[OBJ_DIM];
		if(recv(ds_socket,obj,OBJ_DIM,MSG_WAITALL) == -1){
			printf("\nERRORE: durante la ricezione\n");
			if(close(ds_socket) == -1){
				printf("\nERRORE: chiusura socket\n");
				exit(EXIT_FAILURE);
			}
			return -1;
		}
		char txt[TEXT_DIM];
		if(recv(ds_socket,txt,TEXT_DIM,MSG_WAITALL) == -1){
			printf("\nERRORE: durante la ricezione\n");
			if(close(ds_socket) == -1){
				printf("\nERRORE: chiusura socket\n");
				exit(EXIT_FAILURE);
			}
			return -1;
		}
		printf("\n\t------messaggio------\n\tcode:#%d\n\tmittente: |%s|\n\toggetto: |%s|\n\ttesto: |%s|\n\t---------------------\n",code,m,obj,txt);
	}
	if(close(ds_socket) == -1){
		printf("\nERRORE: chiusura socket\n");
		exit(EXIT_FAILURE);
	}
	flag_socket = 0;
	if(esiste == 0){
		printf("\n\n|   non hai messaggi cancellabili    |\n");
		printf("**************************************\n");
		return 0;
	}
	printf("\n|                                    |");
	printf("\n**************************************\n");
	printf("inserisci il relativo codice per eliminare un messaggio [ -1 per annullare]\n");
	for(i=0;i<c;i++){
		printf("numero %d - codice [%d]\t",i,lista[i]);
		if((i+1)%3==0) printf("\n");
	}
	int da_eliminare,p=0;
	while(1){
		printf("\ncodice: ");
		da_eliminare = leggi_numero();
		if(da_eliminare == -1){
			printf("operazione annullata\n");
			return 0;
		}
		if(da_eliminare < 0){
			printf("codice non corretto, riprovare");
			continue;
		}
		for(i=0;i<c;i++){
			if(lista[i] == da_eliminare) p=1;
		}
		if(p==1) break;
		printf("codice non corretto, riprovare");
	}
	//parte 2: richiesta eliminazione specifico messaggio
	comando = 27;
	ds_socket = socket(AF_INET,SOCK_STREAM,0);
	if(ds_socket == -1){
		printf("ERRORE: durante la creazione socket (login)\n");
		exit(EXIT_FAILURE);
	}
	if(connect(ds_socket,(struct sockaddr*)&server,sizeof(server)) == -1){
		close(ds_socket);
		printf("ERRORE: durante la connessione\n");
		exit(EXIT_FAILURE);
	}
	flag_socket = 1;
	//invio comando
	if(send(ds_socket,&comando,sizeof(int),0) == -1){
		printf("ERRORE: durante l'invio\n");
		if(close(ds_socket) == -1){
			printf("ERRORE: chiusura socket\n");
			exit(EXIT_FAILURE);
		}
		return -1;
	}
	if(send(ds_socket,&da_eliminare,sizeof(int),0) == -1){
		printf("ERRORE: durante l'invio\n");
		if(close(ds_socket) == -1){
			printf("ERRORE: chiusura socket\n");
			exit(EXIT_FAILURE);
		}
		return -1;
	}
	char response[SERVICE_DIM];
	if(recv(ds_socket,response,SERVICE_DIM,0) == -1){
		printf("\nERRORE: durante la ricezione\n");
		if(close(ds_socket) == -1){
			printf("\nERRORE: chiusura socket\n");
			exit(EXIT_FAILURE);
		}
		return -1;
	}
	if(close(ds_socket) == -1){
		printf("\nERRORE: chiusura socket\n");
		exit(EXIT_FAILURE);
	}
	flag_socket = 0;
	//controllo la risposta del server
	if(strcmp(response,"ok") == 0){
		printf("eliminazione avvenuta correttamente\n");
		return 0;
	}
	printf("spiacente, si è verificato un errore\t-riprovare");
	return -1;
}

//funzione per il login
int login(){
	ds_socket = socket(AF_INET,SOCK_STREAM,0);
	if(ds_socket == -1){
		printf("ERRORE: durante la creazione socket (login)\n");
		exit(EXIT_FAILURE);
	}
	flag_socket = 1;
	int comando = 4;//il login lato server corrisponde al comando 4
	char user[USR_DIM];
	char password[USR_DIM];
	char response[SERVICE_DIM];
	printf("Login: \n");
	printf("inserisci username [max %d caratteri]:  ",USR_DIM);
	leggi_frase(user,USR_DIM);
	printf("inserisci password [max %d caratteri]:  ",USR_DIM);
	leggi_frase(password,USR_DIM);
	
	//provo ad inviare username e password al server
	if(connect(ds_socket,(struct sockaddr*)&server,sizeof(server)) == -1){
		close(ds_socket);
		printf("ERRORE: durante la connessione\n");
		exit(EXIT_FAILURE);
	}
	//invio comando
	if(send(ds_socket,&comando,sizeof(int),0) == -1){
		printf("ERRORE: durante l'invio\n");
		if(close(ds_socket) == -1){
			printf("ERRORE: chiusura socket\n");
			exit(EXIT_FAILURE);
		}
		return -1;
	}
	//invio username
	if(send(ds_socket,user,USR_DIM,0) == -1){
		printf("ERRORE: durante l'invio\n");
		if(close(ds_socket) == -1){
			printf("ERRORE: chiusura socket\n");
			exit(EXIT_FAILURE);
		}
		return -1;
	}
	//invio password
	if(send(ds_socket,password,USR_DIM,0) == -1){
		printf("ERRORE: durante l'invio\n");
		if(close(ds_socket) == -1){
			printf("ERRORE: chiusura socket\n");
			exit(EXIT_FAILURE);
		}
		return -1;
	}
	printf("\nlogin in corso ...");
	//ricezione da server
	if(recv(ds_socket,response,SERVICE_DIM,0) == -1){
		printf("\nERRORE: durante la ricezione\n");
		if(close(ds_socket) == -1){
			printf("\nERRORE: chiusura socket\n");
			exit(EXIT_FAILURE);
		}
		return -1;
	}
	if(close(ds_socket) == -1){
		printf("\nERRORE: chiusura socket\n");
		exit(EXIT_FAILURE);
	}
	flag_socket = 0;
	//controllo la risposta del server
	if(strcmp(response,"ok") == 0){
		strcpy(username,user);
		return 0;
	}
	printf("\nle credenziali sono errate: %s",response);
	return -1;
}

//funzione per la creazione di un nuovo utente
int nuovo_utente(){
	printf("Registrazione...\n");
	ds_socket = socket(AF_INET,SOCK_STREAM,0);
	if(ds_socket == -1){
		printf("ERRORE: durante la creazione del socket");
		exit(EXIT_FAILURE);
	}
	flag_socket = 1;
	int comando = 5;
	char user[USR_DIM];
	char password[USR_DIM];
	char response[SERVICE_DIM];
	printf("inserisci username [max %d caratteri]:  ",USR_DIM);
	leggi_frase(user,USR_DIM);
	printf("inserisci password [max %d caratteri]:  ",USR_DIM);
	leggi_frase(password,USR_DIM);
	
	if(connect(ds_socket,(struct sockaddr*)&server,sizeof(server)) == -1){
		close(ds_socket);
		printf("ERRORE: durante la connessione\n");
		exit(EXIT_FAILURE);
	}
	if(send(ds_socket,&comando,sizeof(int),0) == -1){
		printf("ERRORE: durante l'invio\n");
		if(close(ds_socket) == -1){
			printf("ERRORE: chiusura socket\n");
			exit(EXIT_FAILURE);
		}
		return -1;
	}
	//invio username
	if(send(ds_socket,user,USR_DIM,0) == -1){
		printf("ERRORE: durante l'invio\n");
		if(close(ds_socket) == -1){
			printf("ERRORE: chiusura socket\n");
			exit(EXIT_FAILURE);
		}
		return -1;
	}
	//invio password
	if(send(ds_socket,password,USR_DIM,0) == -1){
		printf("ERRORE: durante l'invio\n");
		if(close(ds_socket) == -1){
			printf("ERRORE: chiusura socket\n");
			exit(EXIT_FAILURE);
		}
		return -1;
	}
	printf("\nregistrazione in corso...\n");
	//ricezione response da server
	if(recv(ds_socket,response,SERVICE_DIM,0) == -1){
		printf("ERRORE: durante la ricezione\n");
		if(close(ds_socket) == -1){
			printf("ERRORE: chiusura socket\n");
			exit(EXIT_FAILURE);
		}
		return -1;
	}
	if(close(ds_socket) == -1){
		printf("\nERRORE: chiusura socket");
		exit(EXIT_FAILURE);
	}
	flag_socket = 0;
	//controllo la risposta del server
	if(strcmp(response,"ok") == 0){
		strcpy(username,user);
		return 0;
	}
	printf("le credenziali sono errate: %s\n",response);
	return -1;
}

//funzione ausiliare per la lettura di una stringa
void leggi_frase(char *dest,int max_dim){
	char prova = getchar();//controllo che non ci sia un carattere newline
	if(prova !='\n') ungetc(prova,stdin);// se è un altro carattere lo rimetto nello stdin
	char p[max_dim];
	fgets(p,max_dim,stdin);
	p[strlen(p)-1]='\0';//tolgo il carattere new-line
	strncpy(dest,p,max_dim);

}

//funzione ausiliare per la lettura di un intero
int leggi_numero(){
	int r = -1;
	char prova = getchar();//controllo che non ci sia un carattere newline
	if(prova !='\n') ungetc(prova,stdin);// se è un altro carattere lo rimetto nello stdin
	char p[256];
	fgets(p,256,stdin);
	p[strlen(p)-1]='\0';//tolgo il carattere new-line
	r = atoi(p);
	return r;
}

//funzione ausiliare per l'inizializzazione del client
int ParseCmdLine(int argc, char *argv[], char **szAddress, char **szPort) {
    int n = 1;
    while(n<argc){
		if (!strncmp(argv[n],"-a",2) || !strncmp(argv[n],"-A",2)){
		    *szAddress = argv[++n];
		}
		else 
			if(!strncmp(argv[n],"-p",2) || !strncmp(argv[n],"-P",2)){
			    *szPort = argv[++n];
			}
			else
				if(!strncmp(argv[n],"-h",2) || !strncmp(argv[n],"-H",2)){
		    		printf("Sintassi:\n\n");
			    	printf("    client -a (indirizzo remoto) -p (porta remota) [-h].\n\n");
			    	exit(EXIT_SUCCESS);
				}
		++n;
    }
	if(argc==1){
	    printf("Sintassi:\n\n");
		printf("    client -a (indirizzo remoto) -p (porta remota) [-h].\n\n");
	    exit(EXIT_SUCCESS);
	}
    return 0;
}

