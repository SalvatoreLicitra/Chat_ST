#include <sys/socket.h>
#include <arpa/inet.h> 
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/signal.h>
#include <fcntl.h>
#include <inttypes.h>

#define SERVICE_DIM 25 //dimensione messaggi di servizio tra client e server

#define USR_DIM 20	//dimensione massima di username/password
#define OBJ_DIM 50	//dimensione massima dell'oggetto
#define TEXT_DIM 256	//dimensione massima del testo del messaggio
#define SIZE_MESSAGGIO USR_DIM+USR_DIM+OBJ_DIM+TEXT_DIM+sizeof(int)	//dimensione massima di un messaggio


//strutture di un messaggio
typedef struct Messaggio{
	int  codice;
	char mittente[USR_DIM];	//salvo il mittente del messaggio
	char destinatario[USR_DIM];	//salvo il destinatario del messaggio
	char oggetto[OBJ_DIM];	//oggetto del messaggio
	char testo[TEXT_DIM];	//testo del messaggio
	struct Messaggio *next; //puntatore al prossimo messaggio
}Messaggio;

int file_utenti;//descrittore pubblico del file contenete username e password
Messaggio *root;//puntatore al primo elemento della lista
int file_messaggi;//descrittore pubblico file messaggi statici
int last_code;//codice da assegnare al prossimo messaggio
int list_s;//descrittore socket
int conn_s;//descrittore socket client
int msg_num;//numero di messaggi in lista
int flag_client;//client_sock aperto/chiuso
char user[USR_DIM];//username corrente

//gestione eventi

void gestiscichiusura(){
	printf("\nchiusura server in corso...\n");
	gestioneSalvataggio();
	//salvo backup file
	if(system("cp utenti_password utenti_password_bk") == -1){
		close(list_s);
		fprintf(stderr,"\nerrore durante creazione backup file utenti\n");
		exit(EXIT_FAILURE);
	}
	if(system("cp messaggi_statici messaggi_statici_bk") == -1){
		close(list_s);
		fprintf(stderr,"\nerrore durante creazione backup file utenti\n");
		exit(EXIT_FAILURE);
	}
	if(close(file_utenti)==-1) printf("\nATTENZIONE non è stato possibile eliminare il descrittore del file utenti");
	if(close(file_messaggi)==-1) printf("\nATTENZIONE non è stato possibile eliminare il descrittore del file messaggi");
	if(root!=NULL){
		Messaggio *ap = root;
		Messaggio *del;
		do{
			del = ap;
			ap=(Messaggio*)ap->next;
			del = NULL;
			free(del);
		}while(ap!=NULL);
	}
	if(close(list_s)==-1)	printf("\nATTENZIONE non è stato possibile chiudere il listening socket"); 
	if(flag_client!=0) {if(close(conn_s)==-1)	printf("\nATTENZIONE non è stato possibile chiudere il socket del client");}
	printf("\nchiusura server COMPLETATA\n");
	exit(0);
}

void gestisci_sigill(){
	printf("operazione non consentita, o locazine di memoria non accessibile\n");
	exit(EXIT_SUCCESS);
	raise(SIGINT);
}

void gestisci_disconnessione(){
	printf("errore di connessione");
	exit(EXIT_SUCCESS);
}

void gestisci_timeout(){
	printf( "TIMEOUT connessione. Scollegamento cliente...");
	flag_client=1;
}

//funzione ausiliaria
int ParseCmdLine(int argc, char *argv[], char **szPort){
    int n = 1;
    while(n<argc){
		if(!strncmp(argv[n],"-p",2) || !strncmp(argv[n],"-P",2)) 
			*szPort = argv[++n];
		else 
			if (!strncmp(argv[n],"-h",2) || !strncmp(argv[n],"-H",2)){
			    printf("Sintassi:\n\n");
	    		printf("    server -p (porta) [-h]\n\n");
			    exit(EXIT_SUCCESS);
			}
		++n;
    }
	if(argc==1){
	    printf("Sintassi:\n\n");
		printf("    server -p (porta) [-h]\n\n");
	    exit(EXIT_SUCCESS);
	}
    return 0;
}

//funzione ausiliaria per check user e passwd
int controllo_dati(char*user,char*pass){
	int file_index = 0;
	int end = lseek(file_utenti,0,2);//fine del file
	lseek(file_utenti,0,0);//mi posiziono all'inizio del file
	while(file_index<end){
		char user_check[USR_DIM];
		if(read(file_utenti,user_check,USR_DIM) == -1){
			fprintf(stderr,"server: errore chiamata read utente\n");
			return -1;
		}
		int corretto = strcmp(user_check,user);//controllo se mi trovo nella riga giusta
		if(corretto == 0){
			//controllo password
			char pass_check[USR_DIM];
			if(read(file_utenti,pass_check,USR_DIM) == -1){
				fprintf(stderr,"server: errore chiamata read password\n");
				return -1;
			}
			corretto = strcmp(pass_check,pass);
			if (corretto == 0){
				printf("utente : [%s] login completato correttemente\n",user);
				return 0;
			}
			else{
				printf("password errata");
				return 2;
			}
		}
		else{
			lseek(file_utenti,USR_DIM,1);//sposto il file-pointer di altri usr_dim dalla posizione attuale
		}
		file_index +=2*USR_DIM;//incremento l'indice
	}
	printf("utente non trovato\n");
	return 1;
}

//funzione ausiliaria per check user
int check_username(char *user){
	int file_index = 0;
	int end = lseek(file_utenti,0,2);
	lseek(file_utenti,0,0);
	while(file_index<end){
		char user_check[USR_DIM];
		if(read(file_utenti,user_check,USR_DIM) == -1){
			fprintf(stderr,"server: errore chiamata read utente\n");
			return -1;
		}
		int trovato = strcmp(user_check,user);
		if(trovato == 0){
			return 1;
		}
		lseek(file_utenti,USR_DIM,1);
		file_index+=2*USR_DIM;
	}
	return 0;			
}

//salva i messaggi contenuti nella lista su file
int gestioneSalvataggio(){
	Messaggio *t = root;
	if(root == NULL) {
		return 0;
	}
	while(t!=NULL){
		char messaggio[SIZE_MESSAGGIO-sizeof(int)];
		strncpy(&messaggio[0],t->mittente,USR_DIM);
		strncpy(&messaggio[USR_DIM],t->destinatario,USR_DIM);
		strncpy(&messaggio[2*USR_DIM],t->oggetto,OBJ_DIM);
		strncpy(&messaggio[2*USR_DIM+OBJ_DIM],t->testo,TEXT_DIM);
		if(write(file_messaggi,messaggio,SIZE_MESSAGGIO-sizeof(int)) ==  -1){
			fprintf(stderr,"server: errore durante la scrittura su file messaggi_statici\n");
			exit(EXIT_FAILURE);
		}
		t = t->next;
	}
	free(t);
	return 0;
}

int chat_create_account(){
	printf("REGISTRAZIONE\n");
	char user_pass[USR_DIM*2];
	int client_sock = conn_s;
	/*ricevo il nome*/
	if(recv(client_sock,&user_pass[0],USR_DIM,MSG_WAITALL) == -1){
		fprintf(stderr,"server: errore durante la recv del username");
		return -1;
	}
	if(recv(client_sock,&user_pass[USR_DIM],USR_DIM,MSG_WAITALL) == -1){
		fprintf(stderr,"server: errore durante la recv della password");
		return -1;
	}
	int check = check_username(&user_pass[0]);
	
	switch(check){
		case 0: //nome utente disponibile
			if(write(file_utenti,user_pass,USR_DIM*2) ==  -1){
				fprintf(stderr,"server: errore durante la scrittura su file\n");
				exit(EXIT_FAILURE);
			}
			if(send(client_sock,"ok",SERVICE_DIM,0) == -1){
				fprintf(stderr,"server: errore durante l'invio\n");
				return -1;
			}
			printf("utente: [%s] password:[%s] correttamente registrato\n",&user_pass[0],&user_pass[USR_DIM]);
			strncpy(user,user_pass,USR_DIM);
			return 0;
		case 1: //username già esistente
			if(send(client_sock,"user",SERVICE_DIM,0) == -1){
				fprintf(stderr,"server: errore durante l'invio\n");
				return -1;
			}
			printf("utente già esistente\n");
			return -1;
		default: //errore
			if(send(client_sock,"errore",SERVICE_DIM,0) == -1){
				fprintf(stderr,"server: errore durante l'invio\n");
				return -1;
			}
			printf("errore non calcolato\n");
			return -1;
	}
}

int chat_login(){
	printf("LOGIN\n");
	int client_sock = conn_s;
	if(recv(client_sock,user,USR_DIM,MSG_WAITALL) == -1){
		fprintf(stderr,"server: errore durante la recv\n");
		return -1;
	}
	char pass[USR_DIM];
	if(recv(client_sock,pass,USR_DIM,MSG_WAITALL) == -1){
		fprintf(stderr,"server: errore durante la recv\n");
		return -1;
	}
	int check = controllo_dati(user,pass);//funzione che controlla la combinazione username-password
	switch(check){
		case 0: //corretto
			if(send(client_sock,"ok",SERVICE_DIM,0) ==-1){
				fprintf(stderr,"server: errore durante la send\n");
				return -1;
			}
			return 0;
		case 1: //l'username è sbagliato
			if(send(client_sock,"user",SERVICE_DIM,0) ==-1){
				fprintf(stderr,"server: errore durante la send\n");
				return -1;
			}
			return 1;
		case 2: //la password è sbagliata
			if(send(client_sock,"password",SERVICE_DIM,0) ==-1){
				fprintf(stderr,"server: errore durante la send\n");
				return -1;
			}
			return 2;
		default://errore
			if(send(client_sock,"errore",SERVICE_DIM,0) ==-1){
				fprintf(stderr,"server: errore durante la send\n");
				return -1;
			}
			fprintf(stderr,"ERRORE: imprevisto durante l'autenticazione\n");
			return 1;
	}
		
}

int chat_read(){
	printf("LETTURA\n");
	int client_sock = conn_s;
	Messaggio*tmp = root;
	if(root!=NULL){
		while(tmp->next!= NULL){
			if(strcmp(user,tmp->destinatario) == 0){
				//allora invio
				//invio un messaggio di servizio che annuncia l'invio di dati
				if(send(client_sock,"messaggio",SERVICE_DIM,0) == -1){
					fprintf(stderr,"server: errore durante l'invio - service");
					return -1;
				}
				//codice
				if(send(client_sock,&tmp->codice,sizeof(int),0) == -1){
					fprintf(stderr,"server: errore durante l'invio - code");
					return -1;
				}
				//mittente
				if(send(client_sock,tmp->mittente,USR_DIM,0) == -1){
					fprintf(stderr,"server: errore durante l'invio - mitt");
					return -1;
				}
				//oggetto
				if(send(client_sock,tmp->oggetto,OBJ_DIM,0) == -1){
					fprintf(stderr,"server: errore durante l'invio - obj");
					return -1;
				}
				//testo
				if(send(client_sock,tmp->testo,TEXT_DIM,0) == -1){
					fprintf(stderr,"server: errore durante l'invio - text");
					return -1;
				}
			}
			if(tmp->next != NULL)tmp =(Messaggio*)tmp->next;
		}
		if(strcmp(user,tmp->destinatario) == 0){
			if(send(client_sock,"messaggio",SERVICE_DIM,0) == -1){
				fprintf(stderr,"server: errore durante l'invio - service");
				return -1;
			}
			if(send(client_sock,&tmp->codice,sizeof(int),0) == -1){
				fprintf(stderr,"server: errore durante l'invio -codice");
				return -1;
			}
			if(send(client_sock,tmp->mittente,USR_DIM,0) == -1){
				fprintf(stderr,"server: errore durante l'invio - mitt");
				return -1;
			}
			if(send(client_sock,tmp->oggetto,OBJ_DIM,0) == -1){
				fprintf(stderr,"server: errore durante l'invio - obj");
				return -1;
			}
			if(send(client_sock,tmp->testo,TEXT_DIM,0) == -1){
				fprintf(stderr,"server: errore durante l'invio - text");
				return -1;
			}
		}
	}
	int file_index = 0;
	int end = lseek(file_messaggi,0,2);
	lseek(file_messaggi,0,0);
	while(file_index<end){
		char mitt[USR_DIM],dest[USR_DIM];
		if(read(file_messaggi,mitt,USR_DIM) == -1){
			fprintf(stderr,"server: errore chiamata read utente\n");
			return -1;
		}
		if(read(file_messaggi,dest,USR_DIM) == -1){
			fprintf(stderr,"server: errore chiamata read utente\n");
			return -1;
		}
		if(strcmp(user,dest) == 0){//invio se user == destinatario
			//invio un messaggio di servizio che annuncia l'invio di dati
			if(send(client_sock,"messaggio",SERVICE_DIM,0) == -1){
				fprintf(stderr,"server: errore durante l'invio");
				return -1;
			}
			//codice(i messaggi salvati statici non hanno codice, metto -1)
			int co = -1;
			if(send(client_sock,&co,sizeof(int),0) == -1){
				fprintf(stderr,"server: errore durante l'invio");
				return -1;
			}
			if(send(client_sock,mitt,USR_DIM,0) == -1){
				return -1;
			}
			char obj[USR_DIM];
			if(read(file_messaggi,obj,OBJ_DIM) == -1){
				fprintf(stderr,"server: errore chiamata read utente\n");
				return -1;
			}
			if(send(client_sock,obj,OBJ_DIM,0) == -1){
				fprintf(stderr,"server: errore durante l'invio");
				return -1;
			}
			char text[TEXT_DIM];
			if(read(file_messaggi,text,TEXT_DIM) == -1){
				fprintf(stderr,"server: errore chiamata read utente\n");
				return -1;
			}
			if(send(client_sock,text,TEXT_DIM,0) == -1){
				fprintf(stderr,"server: errore durante l'invio");
				return -1;
			}
		}
		//se ho scartato il messaggio non ho letto obj e text, devo quindi spostarmi al nuovo
		else lseek(file_messaggi,OBJ_DIM + TEXT_DIM,1);
		file_index += SIZE_MESSAGGIO - sizeof(int);
	}
	//invio un messaggio di servizio che annuncia la fine dei dati
	if(send(client_sock,"fine",SERVICE_DIM,0) == -1){
		fprintf(stderr,"server: errore durante l'invio");
		return -1;
	}
	return 0;
}

int chat_write(){
	int client_sock = conn_s;
	printf("Nuovo Messaggio\n");
	Messaggio *mess = malloc(sizeof(Messaggio));
	//mittente
	strcpy(mess->mittente,user);
	//destinatario, è ritenuto valiso solo un destinatario affiliato al servizio
	int valido = 0;
	while(valido != 1){
		if(recv(client_sock,mess->destinatario,USR_DIM,MSG_WAITALL) == -1){
			free(mess);
			fprintf(stderr,"server: errore durante la recv destinatario");
			return -1;
		}
		valido = check_username(mess->destinatario);
		if(valido==1){
			if(send(client_sock,"ok",SERVICE_DIM,0) == -1){
				fprintf(stderr,"server: errore durante l'invio conferma\n");
			}
			break;
		}
		else{
			if(send(client_sock,"no",SERVICE_DIM,0) == -1){
				fprintf(stderr,"server: errore durante l'invio conferma\n");
			}
		}
	}
	//oggetto
	if(recv(client_sock,mess->oggetto,OBJ_DIM,MSG_WAITALL) == -1){
		free(mess);
		fprintf(stderr,"server: errore durante la recv dell'oggetto");
		return -1;
	}
	//testo
	if(recv(client_sock,mess->testo,TEXT_DIM,MSG_WAITALL) == -1){
		free(mess);
		fprintf(stderr,"server: errore durante la recv del testo");
		return -1;
	}
	//codice
	mess->codice = last_code;
	last_code++;
	mess->next = NULL;
	//il messaggio è completo
	if(root == NULL){
		root = mess;
	}
	else{
		Messaggio *tmp = root;
		mess->next = (Messaggio*)tmp;
		root = (Messaggio*)mess;
	}
	printf("----messaggio----\ncode:#|%d|\nmittente:|%s|\ndestinatario:|%s|\noggetto:|%s|\ntesto:|%s|\n",mess->codice,mess->mittente,mess->destinatario,mess->oggetto,mess->testo);
	printf("inviato... ");
	if(send(client_sock,"ok",SERVICE_DIM,0) == -1){
		fprintf(stderr,"server: errore durante l'invio conferma\n");
	}
	msg_num++;
	return 0;
}

int chat_delete(){
	int client_sock = conn_s;
	printf("ELIMINAZIONE MESSAGGIO\n");
	int code;//codice messaggio da eliminare
	if(recv(client_sock,&code,sizeof(int),MSG_WAITALL) == -1){
		fprintf(stderr,"server: errore nella recv\n");
		return -1;
	}
	printf("eliminazione messaggio codice #%d\n",code);
	Messaggio *pre;
	Messaggio *tmp = root;
	int i = 0;
	while(tmp->next != NULL){
		if(tmp->codice == code) break;
		pre = tmp;
		tmp = (Messaggio*)tmp->next;
		i++;
	}
	//se sono uscito dal while perchè ho letto tutti i messaggi
	if(tmp->next == NULL && tmp->codice!=code){
		printf("il messaggio non è stato trovato, puoi aver sbagliato codice, oppure è già stato trasferito nella zona statica!\n");
		if(send(client_sock,"no",SERVICE_DIM,0) == -1){
			fprintf(stderr,"server: errore durante l'invio conferma\n");
			return -1;
		}
		return 0;
	}
	else{
		if(i==0){
			if(tmp->next == NULL){	root = NULL;}
			else{ root = (Messaggio*)tmp->next;}
		}
		else{pre->next = (Messaggio*)tmp->next;}
	}
	free(tmp);
	if(send(client_sock,"ok",SERVICE_DIM,0) == -1){
		fprintf(stderr,"server: errore durante l'invio conferma\n");
		return -1;
	}
	msg_num--;
	printf("eliminazione completata");
	return 0;
}

int selected_read(){
	int client_sock = conn_s;
	printf("LETTURA cancellabili\n");
	if(send(client_sock,&msg_num,sizeof(int),0) == -1){
			fprintf(stderr,"server: errore durante l'invio");
			return -1;
	}
	Messaggio*tmp = root;
	if(root == NULL){
		//se non ci sono messaggi esco subito
		if(send(client_sock,"fine",SERVICE_DIM,0) == -1){
			fprintf(stderr,"server: errore durante l'invio");
			return -1;
		}
		return 0;
	}
	int i = 0;
		while(tmp->next!= NULL){
			if(strcmp(user,tmp->destinatario) == 0){
				//allora invio
				//invio un messaggio di servizio che annuncia l'invio di dati
				if(send(client_sock,"messaggio",SERVICE_DIM,0) == -1){
					fprintf(stderr,"server: errore durante l'invio - service %d",i);
					return -1;
				}
				//codice
				if(send(client_sock,&tmp->codice,sizeof(int),0) == -1){
					fprintf(stderr,"server: errore durante l'invio - code");
					return -1;
				}
				//mittente
				if(send(client_sock,tmp->mittente,USR_DIM,0) == -1){
					fprintf(stderr,"server: errore durante l'invio - mitt");
					return -1;
				}
				//oggetto
				if(send(client_sock,tmp->oggetto,OBJ_DIM,0) == -1){
					fprintf(stderr,"server: errore durante l'invio - obj");
					return -1;
				}
				//testo
				if(send(client_sock,tmp->testo,TEXT_DIM,0) == -1){
					fprintf(stderr,"server: errore durante l'invio - text");
					return -1;
				}
			}
			if(tmp->next != NULL)tmp =(Messaggio*)tmp->next;
			i++;
		}
		if(strcmp(user,tmp->destinatario) == 0){
			if(send(client_sock,"messaggio",SERVICE_DIM,0) == -1){
				fprintf(stderr,"server: errore durante l'invio - service %d",i);
				return -1;
			}
			if(send(client_sock,&tmp->codice,sizeof(int),0) == -1){
				fprintf(stderr,"server: errore durante l'invio - code");
				return -1;
			}
			if(send(client_sock,tmp->mittente,USR_DIM,0) == -1){
				fprintf(stderr,"server: errore durante l'invio - mitt");
				return -1;
			}
			if(send(client_sock,tmp->oggetto,OBJ_DIM,0) == -1){
				fprintf(stderr,"server: errore durante l'invio - obj");
				return -1;
			}
			if(send(client_sock,tmp->testo,TEXT_DIM,0) == -1){
				fprintf(stderr,"server: errore durante l'invio - text");
				return -1;
			}
		}
	//invio un messaggio di servizio che annuncia la fine dei dati
	if(send(client_sock,"fine",SERVICE_DIM,0) == -1){
		fprintf(stderr,"server: errore durante l'invio");
		return -1;
	}
	return 0;
}

int main(int argc, char *argv[]){
	printf("SERVER chat\n\nSetup in corso...\n\n");
	//gestione segnali
	struct sigaction sa;
	sa.sa_flags=0;
	sigfillset(&sa.sa_mask);
	sa.sa_handler=gestiscichiusura;
	if(sigaction(SIGINT,&sa,NULL) ==-1){
		printf("sigaction\n");
		exit(EXIT_FAILURE);
	}
	if(sigaction(SIGQUIT,&sa,NULL) ==-1){
		printf("sigaction\n");
		exit(EXIT_FAILURE);
	}	
	if(sigaction(SIGHUP,&sa,NULL) ==-1){
		printf("sigaction\n");
		exit(EXIT_FAILURE);
	}
	if(sigaction(SIGTERM,&sa,NULL) ==-1){
		printf("sigaction\n");
		exit(EXIT_FAILURE);
	}
	sa.sa_handler=gestisci_sigill;
	if(sigaction(SIGILL,&sa,NULL) ==-1){
		printf("sigaction\n");
		exit(EXIT_FAILURE);
	}
	if(sigaction(SIGSEGV,&sa,NULL) ==-1){
		printf("sigaction\n");
		exit(EXIT_FAILURE);
	}
	sa.sa_handler=gestisci_timeout;
	if(sigaction(SIGALRM,&sa,NULL) ==-1){
		printf("sigaction\n");
		exit(EXIT_FAILURE);
	}
	sa.sa_handler = gestisci_disconnessione;
	if(sigaction(SIGPIPE,&sa,NULL) ==-1){
		printf("sigaction\n");
		exit(EXIT_FAILURE);
	}
	
    int sin_size, proc_id;
    pthread_t tid;
    short int port;
    struct sockaddr_in servaddr;
    struct sockaddr_in their_addr;
    char *endptr;
	void *status;
	
	ParseCmdLine(argc, argv, &endptr);
	
	port = strtol(endptr, &endptr, 0);
	if ( *endptr ){
	    fprintf(stderr, "server: porta non riconosciuta.\n");
	    exit(EXIT_FAILURE);
	}    
	printf("\t-server in ascolto sulla porta %d.\n",port);
	printf("\t-creazione socket\n"); 

    if ((list_s = socket(AF_INET, SOCK_STREAM, 0)) < 0 ){
		fprintf(stderr, "server: errore nella creazione della socket.\n");
		exit(EXIT_FAILURE);
    }
	printf("\t-settaggio sockaddr_in\n");

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family      = AF_INET;
    servaddr.sin_port        = htons(port);
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);

	printf("\t-binding\n");
    if (bind(list_s, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0 ){
		close(list_s);
		fprintf(stderr, "server: errore durante la bind.\n");
		exit(EXIT_FAILURE);
    }
    listen(list_s, 1000);
    
	//STO COMINCIANDO AD INIZIALIZZARE STRUTTURE DATI
	
	/*APRO IN LETTURA IL FILE CONTENETE I DATI UTENTI*/ 
	
	file_utenti=open("utenti_password",O_RDWR|O_CREAT|O_APPEND,0666);
	if(file_utenti == -1){
		close(list_s);
		fprintf(stderr,"errore apertura/creazione file utenti\n");
		exit(EXIT_FAILURE);
	}
	/*CREO UNA COPIA DI BACKUP DEL FILE UTENTI PER PREVENIRE CORRUZIONI*/
	if(system("cp utenti_password utenti_password_bk") == -1){
		close(list_s);
		fprintf(stderr,"errore durante creazione backup file utenti\n");
		exit(EXIT_FAILURE);
	}
	
	/*CREO IL FILE PER I MESSAGI STATICI*/
	file_messaggi=open("messaggi_statici",O_RDWR|O_CREAT|O_APPEND,0666);//ad ogni sessione server ne creo uno nuovo
	if(file_messaggi == -1){
		close(list_s);
		fprintf(stderr,"errore apertura/creazione file messaggi\n");
		exit(EXIT_FAILURE);
	}
	/*CREO UNA COPIA DI BACKUP DEL FILE MESSAGGI PER PREVENIRE CORRUZIONI*/
	if(system("cp messaggi_statici messaggi_statici_bk") == -1){
		close(list_s);
		fprintf(stderr,"errore durante creazione backup file utenti\n");
		exit(EXIT_FAILURE);
	}
	//inizializzo root
	root = NULL;
	msg_num = 0;
	
	printf("\nsetup:  COMPLETATO\n\n");
	printf("\tin attesa dei client...\t\t[CTRL+C per chiudere il server]\n");

	sin_size = sizeof(struct sockaddr_in);
    while ( 1 ){		
		while((conn_s = accept(list_s, (struct sockaddr *)&their_addr, &sin_size) ) < 0 );
		alarm(30);
		flag_client = 1;
		printf("server: connessione da %s\n", inet_ntoa(their_addr.sin_addr));
		int comando;
		if(recv(conn_s,&comando,sizeof(int),MSG_WAITALL) == -1){
			fprintf(stderr,"server: errore ricezione comando, termino la connessione con il client");
			alarm(0);
			if (close(conn_s) == -1){
				raise(SIGINT);
			}
			flag_client = 0;
			continue;
		}
		printf("comando letto: ");
		switch(comando){
			case 1: //LETTURA
				chat_read();
				break;
			case 2: //SCRITTURA
				chat_write();
				break;
			case 3: //STAMPA MESSAGGI ELIMINABILI
				selected_read();
				break;
			case 27: //ELIMINAZIONE
				chat_delete();
				break;
			case 4: //LOG-IN
				chat_login();
				break;
			case 5: //REGISTRAZIONE
				chat_create_account();
				break;
			default:
				printf("[%d] non è un comando riconosciuto, il client verrà disconnesso\n",comando);
		}
		if(flag_client != 0){
			if ( close(conn_s) < 0 ){
				fprintf(stderr, "server: errore durante la close.\n");
				exit(EXIT_FAILURE);
			}
		}
		alarm(0);
		flag_client = 0;
	}
}

