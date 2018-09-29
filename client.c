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

#define MAX_CLIENTS	5
#define BUFLEN 256

int main(int argc, char **argv) {
	if (argc != 3) {
		fprintf(stderr, "Usage: ./client <IP_server> <port_server>\n");
		exit(0);
	}

	// Creare fisier de log:
	char log_name[17];
	strcpy(log_name, "client-");
	sprintf(log_name + 7, "%d", getpid());
	strcat(log_name, ".log");
	FILE *log = fopen(log_name, "w");


	// TCP:
	int sockfd, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    fd_set read_fds;
    fd_set tmp_fds;
    int fdmax, j;
    char buffer[BUFLEN];
	FD_ZERO(&read_fds);
    FD_ZERO(&tmp_fds);
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        fprintf(stderr, "ERROR opening socket");
		exit(0);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[2]));
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    FD_SET(0, &read_fds);
    FD_SET(sockfd, &read_fds);
    fdmax = sockfd;
    if (connect(sockfd,(struct sockaddr*) &serv_addr,sizeof(serv_addr)) < 0) {
        fprintf(stderr, "ERROR connecting");
		exit(0);    
    }

    // UDP:
    int fd_UDP;
	struct sockaddr_in to_station;

   	fd_UDP = socket(PF_INET, SOCK_DGRAM, 0);
    to_station.sin_family = PF_INET;
	to_station.sin_port = htons(atoi(argv[2])); // port
	to_station.sin_addr.s_addr = inet_addr(argv[1]);
	int len = sizeof(struct sockaddr);

	connect(fd_UDP, (struct sockaddr *)&to_station, sizeof(to_station));
	FD_SET(fd_UDP, &read_fds);
	if (fd_UDP > fdmax) {
		fdmax = fd_UDP;
	}

	// Daca am trimis comanda login
    int loginSent = 0;

    // Daca am tirmis comanda logout
    int logoutSent = 0;

    // Daca in momentul de fata sunt logat
    int logged = 0;

    // Daca am trimis comanda quit la server
    int quitSent = 0;

    // Ultimul numar de card pe care am incercat login
    char last_card[7];

    // Daca urmeaza sa trimit confirmarea pentru unlock
    int unlockConf = 0;

    // Daca urmeaza sa trimit confirmarea tarnsferului
    int confirm_transfer = 0;

    // Daca am trimis comanda pentru transfer
    int transfer_sent = 0;

    while(1){
  		// citesc de la tastatura
    	tmp_fds = read_fds;
    	select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
    	// parcurg socketii
    	for (j = 0; j <= fdmax; j++) {
        	if (FD_ISSET(j, &tmp_fds)) {
          		if (j == 0) {
          			// daca citim de la stdin
            		memset(buffer, 0, BUFLEN);
            		fgets(buffer, BUFLEN-1, stdin);
            		fprintf(log, "%s", buffer);
            		char aux[10];
            		sscanf(buffer, "%s ", aux);
            		char buf_aux[BUFLEN];
            		strcpy(buf_aux, buffer);

            		// Daca trebuie sa trimitem parola secreta:
            		if (unlockConf) {
            			char parola[9];
            			strcpy(parola, buffer);
            			memset(buffer,0,BUFLEN);
            			strcpy(buffer, last_card);
            			buffer[strlen(buffer)-1] = 0;
            			strcat(buffer, " ");
            			strcat(buffer, parola);
            			sendto(fd_UDP, buffer, sizeof(buffer), 0,
            				(struct sockaddr *) &to_station,
            				sizeof(struct sockaddr));
            			unlockConf = 0;

            		// Daca trebuie sa trimitem confirmarea transferului
            		} else if (confirm_transfer) {
            			n = send(sockfd, buffer, strlen(buffer), 0);
            			confirm_transfer = 0;

            		// Daca trimitem login
            		} else if (strcmp(aux, "login") == 0) {
            			if (logged) {
            				printf("-2 : Sesiune deja deschisa\n");
            				fprintf(log, "-2 : Sesiune deja deschisa\n");
            				continue;
            			} else {
            				char *tok = strtok(buf_aux, " ");
            				tok = strtok(NULL, " ");
            				if (tok) {
            					strcpy(last_card, tok);
            				}
            				n = send(sockfd, buffer, strlen(buffer), 0);
            				if (confirm_transfer == 0) {
            					loginSent = 1;
            				}
            			}

            		// Daca trimitem logout
            		} else if (strcmp(aux, "logout") == 0) {
            			if (logged == 0) {
            				printf("-1 : Clientul nu este autentificat\n");
            				fprintf(log, "-1 : Clientul nu este autentificat\n");
            				continue;
            			} else {
            				n = send(sockfd, buffer, strlen(buffer), 0);
            				logoutSent = 1;
            			}

            		// Daca trimitem listsold
            		} else if (strcmp(aux, "listsold") == 0) {
            			if (logged == 0) {
            				printf("-1 : Clientul nu este autentificat\n");
            				fprintf(log, "-1 : Clientul nu este autentificat\n");
            				continue;
            			} else {
            				n = send(sockfd, buffer, strlen(buffer), 0);
            			}

            		// Daca trimitem transfer
            		} else if (strcmp(aux, "transfer") == 0) {
            			if (logged == 0) {
            				printf("-1 : Clientul nu este autentificat\n");
            				fprintf(log, "-1 : Clientul nu este autentificat\n");
            				continue;
            			} else {
            				transfer_sent = 1;
            				n = send(sockfd, buffer, strlen(buffer), 0);
            			}

            		// Daca trimitem quit
            		} else if (strcmp(aux, "quit") == 0) {
            			quitSent = 1;
            			n = send(sockfd, buffer, strlen(buffer), 0);

            		// Daca trimitem unlock
            		} else if (strcmp(aux, "unlock") == 0) {
            			if (logged == 0) {
            				buffer[strlen(buffer)-1] = 0;
            				strcat(buffer, " ");
            				strcat(buffer, last_card);

            				// Observam ca spre deosebire de celelalte,
            				// trimiterea se face pe UDP
            				sendto(fd_UDP, buffer, sizeof(buffer), 0,
            					(struct sockaddr *) &to_station,
            					sizeof(struct sockaddr));
            			} else {
            				printf("-2 : Sesiune deja deschisa\n");
            				fprintf(log, "-2 : Sesiune deja deschisa\n");
            			}
            		} else {
            			n = send(sockfd, buffer, strlen(buffer), 0);
            		}

            	// Daca a sosit ceva pe socket-ul UDP
          		} else if (j == fd_UDP) {
          			memset(buffer, 0, BUFLEN);
          			recvfrom(fd_UDP, buffer, BUFLEN, 0,(struct sockaddr *) &to_station, (socklen_t *) &len);
          			printf("%s\n", buffer);
          			fprintf(log,"%s\n", buffer);
          			char *tok = strtok(buffer, " ");
          			tok = strtok(NULL, " ");
          			// Daca se cere parola pentru unlock
          			if (strcmp (tok, "Trimite") == 0) {
          				unlockConf = 1;
          			}
          			// Altfel ignoram

          		// Daca a sosit ceva pe socket-ul TCP
          		} else if (j == sockfd) {
            		memset(buffer, 0, BUFLEN);
            		n = recv(sockfd, buffer, sizeof(buffer), 0);
            		if (n == 0)
              			return 1;
            		else {
            			// Daca am primit QUIT de la server, inchidem
            			if (strcmp(buffer, "QUIT") == 0) {
            				fclose(log);
            				close(sockfd);
            				close(fd_UDP);
            				exit(0);
            			}

            			// Daca am primit confirmare pozitiva la login
              			if (loginSent) {	
              				printf("%s\n", buffer);
              				fprintf(log, "%s\n", buffer);
              				char *tok = strtok(buffer, " ");
              				tok = strtok(NULL, " ");
              				if (strcmp(tok, "Welcome") == 0) {
              					logged = 1;
              					loginSent = 0;
              				}
              				else {
              					loginSent = 0;
              				}

              			// Daca am primit confirmare pozitiva la logout
              			} else if (logoutSent) {
              				printf("%s\n", buffer);
              				fprintf(log, "%s\n", buffer);
              				char *tok = strtok(buffer, " ");
              				tok = strtok(NULL, " ");
              				if (strcmp(tok, "Clientul") == 0) {
              					logged = 0;
              					logoutSent = 0;
              				}

              			// Daca am primit confirmare la quit
              			} else if (quitSent) {
              				char *tok = strtok(buffer, " ");
              				tok = strtok(NULL, " ");
              				if (strcmp(tok, "QUIT") == 0) {
              					fclose(log);
            					close(sockfd);
            					close(fd_UDP);
            					exit(0);
              				}

              			// Daca am trimis cererea de transfer si am primit
              			// Confirmare pozitiva
              			} else if (transfer_sent) {
              				printf("%s\n", buffer);
              				fprintf(log, "%s\n", buffer);
              				char *tok = strtok(buffer, " ");
              				tok = strtok(NULL, " ");
              				if (strcmp (tok, "Transfer") == 0) {
              					confirm_transfer = 1;
              					transfer_sent = 0;
              				}
              			} else {
              				printf("%s\n", buffer);
              				fprintf(log, "%s\n", buffer);
              			}
            		}
          		}
        	}
      	}
    }
    return 0;
}