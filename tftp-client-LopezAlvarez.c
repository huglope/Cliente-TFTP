// Practica Tema 7: Lopez Alvarez, Hugo
// @author Hugo Lopez Alvarez
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>


int main(int argc, char *argv[]){
	// Se declaran las variables que se van a utilizar
	int sock, verbose = 0, rec, leng, num_bloque;
	char buffer[516], *nombre_archivo;
	struct sockaddr_in dirsock, cliente;
	struct servent *tftp;
	socklen_t tamservidor = sizeof(struct sockaddr);
	FILE *archivo;
	size_t num_bytes;

	// Se comprueba que el comando tiene la estructura correcta
	if (argc!=4 && argc!=5){
		fprintf(stderr,"La estructura del comando debe ser: tftp-client ip_servidor {-r|-w} archivo [-v]\n");
		exit(-1);
	}
	
	// Comprueba y transforma la direccion IP del servidor
	if (inet_aton(argv[1], &dirsock.sin_addr) == 0){
		fprintf(stderr, "La ip_servidor tiene un valor erroneo\n");
		exit(-1);
	}

	// Se almacena el nomobre del archivo sobre el que se van a realizar las operaciones
	nombre_archivo = argv[3];

	// Se comprueba Ã±a estructura del comando
	if (argc == 5 && strcmp(argv[4], "-v") == 0) verbose = 1;
	else if (argc == 5){
		fprintf(stderr,"La estructura del comando debe ser: tftp-client ip_servidor {-r|-w} archivo [-v]\n");
                exit(-1);
        }

	// Se abre un socket para la comunicaciÃ³n
	if((sock = socket (AF_INET, SOCK_DGRAM, 0)) < 0){
		perror("socket()");
		exit(EXIT_FAILURE);
	}

	// Se obtiene el puerto de tftp mediante su nombre
	if((tftp = getservbyname("tftp", "udp")) == NULL){
		perror("getServByName()");
		exit(EXIT_FAILURE);
	}

	// Se almacenan la informaciÃn del servidor
	dirsock.sin_port = tftp->s_port;
	dirsock.sin_family = AF_INET;

	// Se rellenan los campos de la estructura con la informacion que necesita el cliente
	cliente.sin_family = AF_INET;
	cliente.sin_port = htons(0);
	cliente.sin_addr.s_addr = INADDR_ANY;

	// Se enlaza el socket con cualquier IP
	if(bind(sock, (struct sockaddr *) &cliente, sizeof(cliente)) < 0){
		perror("bind()");
		exit(EXIT_FAILURE);
	}

	//Se comprueba que tipo de operaciÃ³n quiere realizar
	if (strcmp(argv[2], "-r") == 0){
		// Se preparan las variables para la operaciÃn de lectura
		num_bloque = 1;
		buffer[0]=0;
		buffer[1]=1;
		strcpy(&buffer[2], nombre_archivo);
		strcpy(&buffer[2+1+strlen(nombre_archivo)], "octet");
		leng = 2 + strlen(nombre_archivo) + 1 + strlen("octet") + 1;

		// Se envia la solicitud de empezar a recibir los datos del archivo especificado
		if(sendto(sock, buffer, leng, 0, (struct sockaddr *) &dirsock, sizeof(struct sockaddr_in)) < 0){
			perror("sendto()");
			exit(EXIT_FAILURE);
		}
		else if(verbose) printf ("Enviada solicitud de lectura de \"%s\" a servidor tftp (%s)\n", nombre_archivo, argv[1]); 

		// Se abre el archivo en el que se van a escribir los datos recibidos del servidor
		archivo = fopen(nombre_archivo, "w");
		
		do{

			// Se recibe un mensaje del servidor
			if((rec = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr *) &dirsock, &tamservidor)) < 0){
				perror("recvfrom()");
				exit(EXIT_FAILURE);
			}
			else if(verbose) printf("Recibido bloque del servidor tftp.\n");

			// Se comprueba si se ha detectado algun error
			if (buffer[1] == 5){
				printf("errcode = ");
			        printf("%u%u\n",buffer[2], buffer[3]);
				fprintf(stderr, "%s\n", &buffer[4]);
				exit(-1);
			}

			if(verbose && num_bloque == 1) printf("Es el primer bloque (numero de bloque 1)\n");
			else if(verbose) printf("Es el bloque numero %i.\n", num_bloque);

			// Se comprueba que el numero de bloque es el correcto
			if(num_bloque == ((unsigned char)buffer[2]*256 + (unsigned char) buffer[3])){
				num_bloque++;
				fwrite(&buffer[4], rec - 4, 1, archivo);
			}
			// Si no es correcto se devuelve un error
			else{
				perror("num_bloque");
				exit(-1);
			}
		
			// Se cambia el opcode del mensaje que se va a enviar para que corresponda con el ACK del bloque recibido
			buffer[1] = 4;
			
			// Se envia el ACK
			if(sendto(sock, buffer, 4, 0, (struct sockaddr *) &dirsock, sizeof(struct sockaddr_in)) < 0){
				perror("sendtoi_ack()");
				exit(EXIT_FAILURE);
			}
			else if (verbose) printf("Enviamos ACK del bloque %i.\n\n", num_bloque - 1);

		}
		// Mientras los bytes recibidos sean 516 (612 de data) se siguen recibiendo informacion del servidor
		while(rec == 516);
		// Se cierra el fichero en el que se ha escrito la informacion recibida del servidor
		if(fclose(archivo) != 0){
			perror("fclose()");
			exit(EXIT_FAILURE);
		}
		else if(verbose) printf("El bloque %i era el ultimo: cerramos el fichero\n", num_bloque - 1);

	}

	// Operacioon de escritura en el servidor
	else if (strcmp(argv[2], "-w") == 0){
		// Se modifican las variables para la operacion de escritura
		num_bloque = 0;
		buffer[0]=0;
                buffer[1]=2;
                strcpy(&buffer[2], nombre_archivo);
                strcpy(&buffer[2+1+strlen(nombre_archivo)], "octet");


                leng = 2 + strlen(nombre_archivo) + 1 + strlen("octet") + 1;

		// Se envia al servidor la solicitud de escritura sobre el fichero especificado
                if(sendto(sock, buffer, leng, 0, (struct sockaddr *) &dirsock, sizeof(struct sockaddr_in)) < 0){
                        perror("sendto()");
                        exit(EXIT_FAILURE);
                }
                else if(verbose) printf ("Enviada solicitud de escritura de \"%s\" a servidor tftp (%s)\n", nombre_archivo, argv[1]);

		// Se abre el fichero que va a ser leido para enviar al servidor
                archivo = fopen(nombre_archivo, "r");

                do{
			// Se recibe el ACK del servidor, pero puseto que se puede recibir un error, los bytes que vamos a recibir son 516
                        if((rec = recvfrom(sock, buffer, 516, 0, (struct sockaddr *) &dirsock, &tamservidor)) < 0){
                                perror("recvfrom()");
                                exit(EXIT_FAILURE);
                        }
                        else if(verbose) printf("Recibido ACK del servidor tftp.\n");

			// Se comprueba si se ha recibido un error
                        if (buffer[1] == 5){
                                printf("errcode = ");
                                printf("%u%u\n", buffer[2],  buffer[3]);
                                fprintf(stderr, "%s\n", &buffer[4]);
                                exit(-1);
                        }


			// Se comprueba que el paquete recibido es un ACK
			if(buffer[1] != 4){
				fprintf(stderr, "El paquete recibido no ha sido un ACK\n");
				exit(-1);
			}
			else{
				// Se comprueba que el numero de bloque del ACK recibido es correcto
				if(num_bloque == ((unsigned char)buffer[2]*256 + (unsigned char) buffer[3])){
					num_bloque++;
				}
				else{
					fprintf(stderr, "El ACK recibido no es correcto\n");
					exit(-1);
				}
                        }

			//Se aumenta el numero de bloque que se va a enviar al servidor
			if(255 == (unsigned char) buffer[3]){
			       	buffer[2] = buffer[2] + 1;
				buffer[3] = 0;
			}
			else buffer[3] = buffer[3] +1;

			// Se prepara el buffer para enviar un mensaje con Data
			buffer[1] = 3;
			num_bytes = fread(&buffer[4], 1, 512, archivo);

			// Se envia el mensaje al servidor con la informacion
                        if(sendto(sock, buffer, num_bytes +4, 0, (struct sockaddr *) &dirsock, sizeof(struct sockaddr_in)) < 0){
                                perror("sendtoi_ack()");
                                exit(EXIT_FAILURE);
                        }
                        else if (verbose) printf("Enviamos ACK del bloque %i.\n\n", num_bloque - 1);

                }
		// Se repite la operacion hasta que la informacion enviada es menor de 512
                while(num_bytes >= 512);
		// Se recibe el ultimo ACK del servidor
                if((rec = recvfrom(sock, buffer, 4, 0, (struct sockaddr *) &dirsock, &tamservidor)) < 0){
        	        perror("recvfrom()");
	                exit(EXIT_FAILURE);
                        }
                else if(verbose) printf("Recibido ACK del servidor tftp.\n");

		// Se cierra el fichero del que se ha extraido la informacion que se ha enviado al servidor
                if(fclose(archivo) != 0){
                        perror("fclose()");
                        exit(EXIT_FAILURE);
                }
                else if(verbose) printf("El bloque %i era el ultimo: cerramos el fichero\n", num_bloque);
	}

	else{
		fprintf(stderr, "La estructura del comando debe ser: tftp-client ip_servidor {-r|-w} archivo [-v]\n");
		exit(-1);
	}

	// Se cierra el socket
	close(sock);
	return 0;
}
