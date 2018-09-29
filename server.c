#include <fcntl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

// Fiecare intrare este de forma structurii de mai jos
typedef struct {
	char nume[13];
	char prenume[13];
	char nr_card[7];
	char pin[5];
	char parola[9];
	double sold;
	unsigned char bloc;
} intrare;

// Pentru transfer, dupa ce se face cererea de transfer
typedef struct {
	int to;
	double sum;
} for_transfer;

#define MAX_CLIENTS	5
#define BUFLEN 256

int login(char *buffer, int N, intrare *users, int *session, int *attempts, int client) {
	char *tok;
	// In tok voi avea numele cardului
	tok = strtok(NULL, " ");

	// Nu am primit card
	if (tok == NULL) {
		return -4;
	}

	int i;
	for (i = 0; i < N; ++i) {
		if (strcmp(users[i].nr_card, tok) == 0) {
			// Daca exista deja o sesiune deschisa
			if (session[i] != 0) {
				return -2;
			}
			// Daca cardul e blocat
			if (users[i].bloc != 0) {
				return -5;
			}

			// In tok am pinul
			tok = strtok(NULL, " ");
			// Nu am introdus parola:
			if (tok == NULL) {
				// Incrementez numarul de incercari esuate
				if (++attempts[i] >= 3) {
					users[i].bloc = 1;
					return -5;
				} else {
					return -3;
				}
			}

			// Verific pinul introdus
			if (strcmp(tok, users[i].pin) == 0) {
				attempts[i] = 0;
				session[i] = client;
				return i;
			}
			else {
				// Pin gresit:
				if (++attempts[i] >= 3) {
					users[i].bloc = 1;
					return -5;
				} else {
					return -3;
				}
			}
		}
	}
	// Nu s-a introdus un card valid
	return -4;
}

int cerere_transfer(char *buffer, int N, intrare *users, int client, for_transfer *coada_transfer) {
	char *tok = strtok(NULL, " ");
	// Daca nu s-a primit niciun cont
	if (tok == NULL) {
		return -4;
	}
	int i;
	for (i = 0; i < N; ++i) {
		if (strcmp(tok, users[i].nr_card) == 0) {
			// Trec mai departe la verificarea sumei
			tok = strtok(NULL, " ");
			// Daca nu s-a dat o suma
			if (tok == NULL) {
				return -6;
			}

			double suma = atof(tok);
			// Daca suma e prea mare
			if (users[client].sold < suma) {
				return -8;
			}
			
			// Adaugam la coada de asteptare pentru confirmare:
			coada_transfer[client].to = i;
			coada_transfer[client].sum = suma;
			return i;
		}
	}

	//Nu exista contul respectiv
	return -4;
}

void transfer(int index, for_transfer *coada_transfer, intrare* users) {
	// Odata confirmat, transferam banii, cu ajutorul cozii de asteptare
	users[index].sold -= coada_transfer[index].sum;
	users[coada_transfer[index].to].sold += coada_transfer[index].sum;
	coada_transfer[index].sum = 0;
	coada_transfer[index].to = -1;
}

int unlock_req(char *buffer, int N, intrare *users, int *deblocare) {
	char *tok;
	tok = strtok(NULL, " ");
	// Nu am primit card
	if (tok == NULL) {
		return -4;
	}
	int i;
	for (i = 0; i < N; ++i) {
		if (strcmp(users[i].nr_card, tok) == 0) {

			// Daca exista deja alt client care incearca sa deblocheze
			if (deblocare[i]) {
				return -7;
			}

			// Daca avem un card blocat
			if (users[i].bloc) {
				deblocare[i] = 1;
				return i;
			} else {
				// Daca avem un card care nu este blocat
				return -6;
			}
		}
	}
	return -4;
}

int unlock(char *buffer, char *cont, int N, intrare *users, int *deblocare) {
	int i;
	char cont1[7];
	strcpy(cont1, cont);
	char *tok = strtok(NULL, " ");
	if (tok == NULL) {
		return -7;
	}
	for (i = 0; i < N; ++i) {
		if (strcmp(cont, users[i].nr_card) == 0) {
			deblocare[i] = 0;

			// Daca parola este cea corecta
			if (strcmp(tok, users[i].parola) == 0) {
				users[i].bloc = 0;
				return i;
			} else {
				return -7;
			}
		}
	}
	return -7;
}

int main(int argc, char **argv) {
	if (argc != 3) {
		fprintf(stderr, "Usage: ./server <port_server> <users_data_file>\n");
		exit(0);
	}


	//Citirea fisierului users_data_file:
	FILE *in = fopen(argv[2], "r");
	int N;
	fscanf(in, "%d", &N);	
	int i;
	intrare *users = (intrare*) calloc(N, sizeof(intrare));
	for (i = 0; i < N; ++i) {
		fscanf(in, "%s %s %s %s %s %lf", users[i].nume, users[i].prenume, users[i].nr_card, users[i].pin, users[i].parola, &users[i].sold);
	}
	fclose(in);

	// Daca pe cont (pozitie) avem sesiune activa sau nu
	int *session = (int*) calloc(N, sizeof(int));

	// Numarul de incercari de login pe cont
	int *attempts = (int*) calloc(N, sizeof(int));

	// Coada de asteptare a confirmarii de transfer
	for_transfer *coada_transfer = 
		(for_transfer *) malloc(N * sizeof(for_transfer));
	// O vom initializa in felul urmator
	for (i = 0; i < N; ++i) {
		// Destinatarul = -1 (nu exista)
		coada_transfer[i].to = -1;

		// Suma este vida
		coada_transfer[i].sum = 0;
	}

	// Daca pe cont avem sau nu o sesiune de unlock activa
	int *deblocare = (int*) calloc(N, sizeof(int));


	int sockfd, newsockfd, portno, clilen;
    char buffer[BUFLEN];
    struct sockaddr_in serv_addr, cli_addr;
    int n, j;

    fd_set read_fds;	// multimea de citire folosita in select()
    fd_set tmp_fds;		// multime folosita temporar 
    int fdmax;			// valoare maxima file descriptor din multimea read_fds
	// golim multimea de descriptori de citire (read_fds) si multimea tmp_fds 
    FD_ZERO(&read_fds);
    FD_ZERO(&tmp_fds);
    
    // Socket-ul TCP:
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
    	fprintf(stderr, "ERROR opening socket");
		exit(1);
    }
     
    portno = atoi(argv[1]);

    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;	// foloseste adresa IP a masinii
    serv_addr.sin_port = htons(portno);

    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr_in)) < 0) {
    	fprintf(stderr, "ERROR on binding");
		exit(1);
    }


    listen(sockfd, MAX_CLIENTS);

    // adaugam noul file descriptor (socketul pe care se asculta conexiuni) in multimea read_fds
    FD_SET(sockfd, &read_fds);

    FD_SET(fileno(stdin), &read_fds);
    fdmax = sockfd;


    // Socket-ul UDP:
    struct sockaddr_in my_sockaddr, from_client;
    my_sockaddr.sin_family = PF_INET;
	my_sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	my_sockaddr.sin_port = htons(atoi(argv[1]));
    
    int fd_udp;
    fd_udp = socket(PF_INET, SOCK_DGRAM, 0);
    bind(fd_udp, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr));
    // adaug socketul UDP la multime
    FD_SET(fd_udp, &read_fds);
    if (fd_udp > fdmax) {
    	fdmax = fd_udp;
    }

    // main loop
	while (1) {
		tmp_fds = read_fds; 
		if (select(fdmax + 1, &tmp_fds, NULL, NULL, NULL) == -1) {
			fprintf(stderr, "ERROR in select");
			exit(0);
		}
		for(i = 0; i <= fdmax; ++i) {
			if (FD_ISSET(i, &tmp_fds)) {			
				if (i == fileno(stdin)) {
					memset(buffer, 0, BUFLEN);
					fscanf(stdin, "%s", buffer);

					// Daca serverul a primit quit de la tastatura
					if (strcmp(buffer, "quit") == 0) {
						memset(buffer, 0, BUFLEN);
						strcpy(buffer, "QUIT");
						int i2;
						// va inchide mai intai toti clientii
						for (i2 = 0; i2 <= fdmax; ++i2) {
							if (i2 != sockfd) {
								send(i2, buffer, strlen(buffer), 0);
							}
						}
						exit(0);
					} else {
						// Pentru orice altceva va afisa eroare
						printf("-10 :  Eroare la apel nume-functie\n");
					}
				} else if (i == fd_udp) {
					// Daca am primit ceva pe socket-ul UDP:
					memset(buffer, 0, BUFLEN);
					int len = sizeof(struct sockaddr);
					if ((n = recvfrom(fd_udp, buffer, BUFLEN, 0,
						(struct sockaddr *) &from_client,
						(socklen_t *) &len)) <= 0) {

						if (n == 0) {
							//conexiunea s-a inchis
							printf("Socket %d hung up\n", i);
						} else if (n < 0) {
							fprintf(stderr, "ERROR in recv");
							exit(0);
						} 
					} else {
						buffer[strlen(buffer) - 1] = 0;
						int len = sizeof(struct sockaddr);
						char *tok = strtok(buffer, " ");
						if (strcmp(tok, "unlock") == 0) {
							// Apelez unlock_req si verific conditiile
							int ret = unlock_req(buffer, N, users, deblocare);
							switch (ret) {
								case -4 :
									memset(buffer, 0, BUFLEN);
            						strcpy(buffer,
            							"UNLOCK> -4 :  Numar card inexistent");
            						sendto(fd_udp, buffer, strlen(buffer), 0,
            							(struct sockaddr *) &from_client,
            							sizeof(struct sockaddr));
            						break;
            					case -6 :
            						memset(buffer, 0, BUFLEN);
            						strcpy(buffer,
            							"UNLOCK> -6 : Operatie esuata");
            						sendto(fd_udp, buffer, strlen(buffer), 0,
            							(struct sockaddr *) &from_client,
            							sizeof(struct sockaddr));
            						break;
            					case -7 :
            						memset(buffer, 0, BUFLEN);
            						strcpy(buffer,
            							"UNLOCK> -7 : Deblocare esuata");
            						sendto(fd_udp, buffer, strlen(buffer), 0,
            							(struct sockaddr *) &from_client,
            							sizeof(struct sockaddr));
            						break;
            					default :
            						memset(buffer, 0, BUFLEN);
            						strcpy(buffer,
            							"UNLOCK> Trimite parola secreta");
            						sendto(fd_udp, buffer, strlen(buffer), 0,
            							(struct sockaddr *) &from_client,
            							sizeof(struct sockaddr));
							}
						} else {
							int len = sizeof(struct sockaddr);
							// Verific daca parola primita se potriveste
							int ret = unlock(buffer, tok, N, users, deblocare);
							switch (ret) {
								case -7 :
									memset(buffer, 0, BUFLEN);
            						strcpy(buffer,
            							"UNLOCK> -7 : Deblocare esuata");
            						sendto(fd_udp, buffer, strlen(buffer), 0,
            							(struct sockaddr *) &from_client,
            							sizeof(struct sockaddr));
            						break;
            					default :
            						memset(buffer, 0, BUFLEN);
            						strcpy(buffer,
            							"UNLOCK> Card deblocat");
            						sendto(fd_udp, buffer, strlen(buffer), 0,
            							(struct sockaddr *) &from_client,
            							sizeof(struct sockaddr));
							}
						}

					}
				} else if (i == sockfd) {
					// a venit ceva pe socketul inactiv(cel cu listen)
					// deci avem o noua conexiune
					// actiunea serverului: accept()
					clilen = sizeof(cli_addr);
					if ((newsockfd = accept(sockfd, 
						(struct sockaddr *)&cli_addr, &clilen)) == -1) {
						fprintf(stderr, "ERROR in accept");
						exit(1);
					}  else {
						// adaug noul socket intors de accept() la multime
						//descriptorilor de citire
						FD_SET(newsockfd, &read_fds);
						if (newsockfd > fdmax) { 
							fdmax = newsockfd;
						}
					}
					printf("Conexiune de la %s, port %d, socket_client %d\n ", 
						inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port),
						newsockfd);
				} else {
					// am primit date pe unul din socketii cu care vorbesc cu clientii
					// actiunea serverului: recv()
					memset(buffer, 0, BUFLEN);
					
					if ((n = recv(i, buffer, sizeof(buffer), 0)) <= 0) {
						if (n == 0) {
							//conexiunea s-a inchis
							printf("Socket %d hung up\n", i);
						} else if (n < 0) {
							fprintf(stderr, "ERROR in recv");
							exit(0);
						}
						close(i);
						// scoatem din multimea de citire socketul pe care 
						FD_CLR(i, &read_fds);
					} 
					
					else {
            			buffer[strlen(buffer) - 1] = 0; //elimin '\n'
            			int i2;

            			// Verific daca se primeste sau nu o confirmare pentru
            			// transfer bancar:
            			int confirm_transfer = 0;
            			for (i2 = 0; i2 < N; ++i2) {
            				if (session[i2] == i * 130) {
            					printf("buf = %s\n",buffer);
            					confirm_transfer = 1;
            					if (buffer != NULL) {
            						if (buffer[0] == 'y') {
            							transfer(i2, coada_transfer, users);
            							session[i2] /= 130;
            							memset(buffer, 0, BUFLEN);
            							strcpy(buffer,
            								"IBANK> Transfer realizat cu succes");
            							send(i, buffer, strlen(buffer), 0);
            						} else {
            							coada_transfer[i2].to = -1;
            							coada_transfer[i2].sum = 0;
            							session[i2] /= 130;
            							memset(buffer, 0, BUFLEN);
            							strcpy(buffer,
            								"IBANK> -9 Operatie anulata");
            							send(i, buffer, strlen(buffer), 0);
            						}
            					} else {
            						coada_transfer[i2].to = -1;
            						coada_transfer[i2].sum = 0;
            						session[i2] /= 130;
            						memset(buffer, 0, BUFLEN);
            						strcpy(buffer,
            							"IBANK> -9 Operatie anulata");
            						send(i, buffer, strlen(buffer), 0);
            					}
            					break;
            				}
            			}

            			char *tok;
            			tok = strtok(buffer, " ");
            			if (confirm_transfer) {
            				printf("CONTINUE i= %d\n",i);
            				continue;
            			}
            			//Daca nu se primeste o confirmare, ci o alta comanda:
            			if (strcmp(tok, "login") == 0) {
            				int ret = login(buffer, N, users,
            					session, attempts, i);
            				int i2;
            				switch (ret) {
            					case -4 :
            						memset(buffer, 0, BUFLEN);
            						strcpy(buffer,
            							"IBANK> -4 : Numar card inexistent");
            						send(i, buffer, strlen(buffer), 0);
            						break;
            					case -5 :
            						memset(buffer, 0, BUFLEN);
            						strcpy(buffer,
            							"IBANK> -5 Card blocat");
            						send(i, buffer, strlen(buffer), 0);
            						break;
            					case -3 :
            						memset(buffer, 0, BUFLEN);
            						strcpy(buffer,
            							"IBANK> -3 Pin gresit");
            						send(i, buffer, strlen(buffer), 0);
            						break;
            					case -2 :
            						memset(buffer, 0, BUFLEN);
            						strcpy(buffer,
            							"IBANK> -2 Sesiune deja deschisa");
            						send(i, buffer, strlen(buffer), 0);
            						break;
            					default :
            						memset(buffer, 0, BUFLEN);
            						strcpy(buffer, "IBANK> Welcome ");
            						strcat(buffer, users[ret].nume);
            						strcat(buffer, " ");
            						strcat(buffer, users[ret].prenume);
            						send(i, buffer, strlen(buffer), 0);
            						break;
            				}
            			} else if (strcmp(tok, "logout") == 0) {
            				int i2;
            				for (i2 = 0; i2 < N; ++i2) {
            					if (session[i2] == i) {
            						session[i2] = 0;
            						memset(buffer, 0, BUFLEN);
            						strcpy(buffer,
            							"IBANK> Clientul a fost deconectat");
            						send(i, buffer, strlen(buffer), 0);
            						break;
            					}
            				}
            			} else if (strcmp(tok, "listsold") == 0) {
            				int i2;
            				for (i2 = 0; i2 < N; ++i2) {
            					if (session[i2] == i) {
            						memset(buffer, 0, BUFLEN);
            						sprintf(buffer, "IBANK> %0.2f",
            							users[i2].sold);
            						send(i, buffer, strlen(buffer), 0);
            						break;
            					}
            				}
            			} else if (strcmp(tok, "transfer") == 0) {
            				int i2;
            				for (i2 = 0; i2 < N; ++i2) {
            					if (session[i2] == i) {
            						int ret = cerere_transfer(buffer, N, users,
            							i2, coada_transfer);
            						switch (ret) {
            							case -8 :
            								memset(buffer, 0, BUFLEN);
            								strcpy(buffer,
            									"IBANK> -8 Fonduri insuficiente");
            								send(i, buffer, strlen(buffer), 0);
            								break;
            							case -6 :
            								memset(buffer, 0, BUFLEN);
            								strcpy(buffer,
            									"IBANK> -6 Operatie esuata");
            								send(i, buffer, strlen(buffer), 0);
            								break;
            							case -4 :
            								memset(buffer, 0, BUFLEN);
            								strcpy(buffer,
            									"IBANK> -4 Numar card inexistent");
            								send(i, buffer, strlen(buffer), 0);
            								break;
            							default :
            								memset(buffer, 0, BUFLEN);
            								sprintf (buffer,
            									"IBANK> Transfer %0.2f catre %s %s? [y/n]",
            									coada_transfer[i2].sum,
            									users[coada_transfer[i2].to].nume,
            									users[coada_transfer[i2].to].prenume);
            								send(i, buffer, strlen(buffer), 0);
            								session[i2] *= 130;
            								break;
            						}
            					}
            				}
            			} else if (strcmp(tok, "quit") == 0) {
            				int i2;
            				for (i2 = 0; i2 < N; ++i2) {
            					session[i2] = 0;
            					memset(buffer, 0, BUFLEN);
            					strcpy(buffer, "IBANK> QUIT");
            					send(i, buffer, strlen(buffer), 0);
            					break;
            				}
            			} else {
            				memset(buffer, 0, BUFLEN);
            				strcpy(buffer, "IBANK> -6 Operatie esuata");
            				send(i, buffer, strlen(buffer), 0);
            			}	
            		}
				} 
			}
		}
    }


    close(sockfd);
   

	return 0;
}