#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include "parser.h"
#include <sys/stat.h>

// Jiajie Ni
// Gabriel Sánchez Losa

void manejador_padre(int sig){
	if (sig == SIGINT) {
		printf("\n");
		fprintf(stderr, "msh> ");
	}
}

void manejador_hijo(int sig){
	if (sig == SIGINT) {
		printf("\n");
		exit(0);
	}
}

int main()
{
	char buf [1024];
	tline *line;
	int i,j;
	pid_t pid;
	int status;
	int pipe1[2];
	int octal;

	signal(SIGINT, manejador_padre);

	printf("msh> ");
	while (fgets(buf, 1024, stdin)){
		line = tokenize(buf);
		signal(SIGINT, manejador_padre);
		if (line->ncommands == 0) {
                        printf("msh> ");
			continue;
		}
		if(strcmp(line->commands[0].argv[0], "exit") == 0){
			break;
		}

		//MANDATO UMASK

		if ((strcmp(line->commands[0].argv[0], "umask")==0)&&(line->commands[0].argv[1]!=NULL)) {

                        if (atoi(line->commands[0].argv[1]) <= 777 && atoi(line->commands[0].argv[1]) >= 0) {
                                sscanf(line->commands[0].argv[1], "%o", &octal);
                                umask(octal);
                        }
                        else
                                fprintf(stderr, "%s: %s: Opción inválida\n", line->commands[0].argv[0], line->commands[0].argv[1]);

                } else if ((strcmp(line->commands[0].argv[0], "umask")==0)&&(line->commands[0].argv[1] == NULL)) {
                        fprintf(stderr, "Error: %s requiere de argumentos\n", line->commands[0].argv[0]);
		}

		//MANDATO CD

		if (strcmp(line->commands[0].argv[0],"cd") == 0){ //función de string.h que compara strings
			char *dir;
                	char buffer[512];
			if(line->ncommands > 2) {
				fprintf(stderr,"Uso: %s directorio\n", line->commands[0].argv[0]);
				continue;
			 }
			if (line->ncommands == 1) {
				dir = getenv("HOME");
				if(dir == NULL) {
		 	 		fprintf(stderr,"No existe la variable $HOME\n");
				}
			}
			else {
				dir = line->commands[0].argv[1];
			}
			// Comprobar si es un directorio
			if (chdir(dir) != 0) {
				fprintf(stderr,"Error al cambiar de directorio: %s\n", strerror(errno));
			}
			chdir(line->commands[0].argv[1]);
			printf("El directorio actual es: %s\n", getcwd(buffer,-1));
			printf("msh> ");
			continue;
		}

		//UN SOLO MANDATO
		else if (line->ncommands == 1){

			pid = fork();

			if (pid < 0){ //Error en el fork
				fprintf(stderr, "Falló el fork().\n%s\n", strerror(errno));
				exit(-1);
			}

			else if (pid == 0){ //Proceso hijo
				if (line->background) {
					signal(SIGINT,SIG_IGN);
				}
				else {
					signal(SIGINT, manejador_hijo);
				}
		        	if (line->redirect_input != NULL) { //Redirección de entrada
                			int fichero;
                			fichero = open(line->redirect_input, O_RDONLY); //redirect_input guarda la ruta del fichero
                			if (fichero == -1) { //Error al abrir el fichero
                        			fprintf(stderr, "%s: Error. El fichero de redirección de entrada no existe o no se pudo abrir %s\n", line->redirect_input,strerror(errno));
                    			}
                    			else {
                        			dup2(fichero, STDIN_FILENO);
                        			close(fichero);
                    			}
				}
				if (line->redirect_output != NULL) { //Redirección de salida

                			int fichero2;
                			fichero2 = open(line->redirect_output, O_WRONLY | O_CREAT| O_TRUNC, 0666);
                			if (fichero2 == -1) {
                        			fprintf(stderr, "%s: Error. Fallo al crear el fichero de redirección de salida: %s\n", line->redirect_output,strerror(errno));
                			}
                			else {
                        			dup2(fichero2, STDOUT_FILENO);
                        			close(fichero2);
                			}
                		}
				if (line->redirect_error != NULL) { //Redirección de error
                        			int fichero3;
                        			fichero3 = open(line->redirect_error, O_WRONLY | O_CREAT | O_TRUNC, 0666);
                        			if (fichero3 == -1) {
                                			fprintf(stderr, "%s: Error: Fallo al crear el fichero de redirección de errores: %s\n", line->redirect_error,strerror(errno));
                        			}
                        			else {
                                			dup2(fichero3, STDERR_FILENO);
                                			close(fichero3);
                        			}
                		}
				execvp(line->commands[0].filename,line->commands[0].argv);
				fprintf(stderr, "%s: Error al ejecutar el mandato %s\n", line->commands[0].argv[0],strerror(errno));
				exit(1);
			}
			else { //Proceso padre
				signal(SIGINT,SIG_IGN);
				if (line->background)
					waitpid(pid, NULL, WNOHANG);
				else {
					wait (&status);
					if (WIFEXITED(status) != 0)
						if (WEXITSTATUS(status) != 0)
							fprintf(stderr, "%s: Error. El mandato no se ejecutó correctamente\n",line->commands[0].argv[0]);
				}
			}

		signal(SIGINT,manejador_padre);

		}
		else if (line->ncommands == 2){ //Dos mandatos
			pipe(pipe1);
			pid = fork();

			if (pid < 0){ //Error en el fork
                                fprintf(stderr, "Falló el fork().\n%s\n", strerror(errno));
                                exit(-1);
                        }

                        else if (pid == 0){ //Proceso hijo 1

				close(pipe1[0]);
				dup2(pipe1[1], STDOUT_FILENO);
				close(pipe1[1]);

				if (line->background) {
                                        signal(SIGINT,SIG_IGN);
                                }
                                else {
                                        signal(SIGINT, manejador_hijo);
                                }

				if (line->redirect_input != NULL) { //Redirección de entrada
                                        int fichero;
                                        fichero = open(line->redirect_input, O_RDONLY); //redirect_input guarda la ruta del fichero
                                        if (fichero == -1) { //Error al abrir el fichero
                                                fprintf(stderr, "%s: Error. El fichero de redirección de entrada no existe o no se pudo abrir %s\n", line->redirect_input,strerror(errno));
                                        }
                                        else {
                                                dup2(fichero, STDIN_FILENO);
                                                close(fichero);
                                        }
                                }

                                execvp(line->commands[0].filename,line->commands[0].argv);
                                fprintf(stderr, "%s: Error al ejecutar el mandato %s\n", line->commands[0].argv[0],strerror(errno));
                                exit(1);
                        }
                        else { //Proceso padre

				signal(SIGINT,SIG_IGN);

				close(pipe1[1]);
				pid=fork();

				if(pid < 0){
					fprintf(stderr, "Falló el fork().\n%s\n", strerror(errno));
                                	exit(-1);
				}

				else if (pid == 0){ //Proceso hijo 2

					if (line->background) {
                                        	signal(SIGINT,SIG_IGN);
                                	}
                                	else {
                                        	signal(SIGINT, manejador_hijo);
                                	}

                                	dup2(pipe1[0], STDIN_FILENO);
                                	close(pipe1[0]);

					if (line->redirect_output != NULL) { //Redirección de salida

                                       		int fichero2;
                                        	fichero2 = open(line->redirect_output, O_WRONLY | O_CREAT| O_TRUNC, 0666);
                                        	if (fichero2 == -1) {
                                                	fprintf(stderr, "%s: Error. Fallo al crear el fichero de redirección de salida: %s\n", line->redirect_output,strerror(errno));
                                        	}
                                        	else {
                                                	dup2(fichero2, 1);
                                                	close(fichero2);
                                        	}
                                	}
					if (line->redirect_error != NULL) { //Redirección de error
                                                int fichero3;
                                                fichero3 = open(line->redirect_error, O_WRONLY | O_CREAT | O_TRUNC, 0666);
                                                if (fichero3 == -1) {
                                                        fprintf(stderr, "%s: Error: Fallo al crear el fichero de redirección de errores: %s\n", line->redirect_error,strerror(errno));
                                                }
                                                else {
                                                        dup2(fichero3, STDERR_FILENO);
                                                        close(fichero3);
                                                }
                                	}
                                	execvp(line->commands[1].filename,line->commands[1].argv);
                                	fprintf(stderr, "%s: Error al ejecutar el mandato\n", line->commands[0].argv[0]);
                                	exit(1);
                        	}
				else { //Proceso padre

					signal(SIGINT,SIG_IGN);

					if (line->background) {
                                        	waitpid(pid, NULL, WNOHANG);
						waitpid(pid, NULL, WNOHANG);
					}
                                	else {
                                		wait (&status);
						wait (&status);
                                			if (WIFEXITED(status) != 0)
                                        			if (WEXITSTATUS(status) != 0)
                                                			printf("%s: Error. El mandato no se ejecutó correctamente\n",line->commands[0].argv[0]);
					}
				}
                        }

		signal(SIGINT,manejador_padre);

		}
		else { //Más de 2 mandatos

			int pidHijos[line->ncommands];
			int pipes[line->ncommands-1][2];

			for (i=0; i<line->ncommands-1; i++) {
				pipe(pipes[i]);
				if (pipe(pipes[i]) < 0)
					fprintf(stderr, "Falló el pipe().\n%s\n", strerror(errno));
			}
			pid=fork();
			for(i=0; i<line->ncommands; i++){
				if (pid < 0){ //Error en el fork
                                	fprintf(stderr, "Falló el fork().\n%s\n", strerror(errno));
                                	exit(-1);
                        	}
				else if (pid == 0){ //Proceso hijo
					if(i==0){ //Primer proceso

						close(pipes[i][0]);
                                                dup2(pipes[i][1], STDOUT_FILENO);
                                                close(pipes[i][1]);

						if (line->background) {
        	                                        signal(SIGINT,SIG_IGN);
	                                        }
                	                        else {
                        	                        signal(SIGINT, manejador_hijo);
                                	        }

						if (line->redirect_input != NULL) { //Redirección de entrada
                                        		int fichero;
                                        		fichero = open(line->redirect_input, O_RDONLY); //redirect_input guarda la ruta del fichero
                                        		if (fichero == -1) { //Error al abrir el fichero
                                                		fprintf(stderr, "%s: Error. El fichero de redirección de entrada no existe o no se pudo abrir %s\n", line->redirect_input,strerror(errno));
                                        		}
                                        		else {
                                                		dup2(fichero, STDIN_FILENO);
                                                		close(fichero);
                                        		}
                                		}

						for(j=1; j<line->ncommands-1; j++){
							close(pipes[j][0]);
							close(pipes[j][1]);
						}
                                                execvp(line->commands[i].filename,line->commands[i].argv);
                                                fprintf(stderr, "%s: Error al ejecutar el mandato %s\n", line->commands[i].argv[i],strerror(errno));
                                                exit(1);
					}
					else if (i==line->ncommands - 1){ //Último proceso

						close(pipes[i-1][1]);
						dup2(pipes[i-1][0], STDIN_FILENO);
                                        	close(pipes[i-1][0]);

						if (line->background) {
                                                	signal(SIGINT,SIG_IGN);
                                        	}
                                       		else {
                                                	signal(SIGINT, manejador_hijo);
                                        	}

						if (line->redirect_output != NULL) { //Redirección de salida

                                            		int fichero2;
                                                	fichero2 = open(line->redirect_output, O_WRONLY | O_CREAT| O_TRUNC, 0666);

	                                                if (fichero2 == -1) {
        	                                                fprintf(stderr, "%s: Error. Fallo al crear el fichero de redirección de salida: %s\n", line->redirect_output,strerror(errno));
				 			}
                	                                else {
                        	                                dup2(fichero2, 1);
                                	                        close(fichero2);
                                        	        }
                                       		 }
                                        	if (line->redirect_error != NULL) { //Redirección de error

                                                	int fichero3;
                                                	fichero3 = open(line->redirect_error, O_WRONLY | O_CREAT | O_TRUNC, 0666);

                                                	if (fichero3 == -1) {
                                                        	fprintf(stderr, "%s: Error: Fallo al crear el fichero de redirección de errores: %s\n", line->redirect_error,strerror(errno));
							}
                                                	else {
                                                        	dup2(fichero3, STDERR_FILENO);
                                                        	close(fichero3);
                                                	}
                                        	}
                                        	execvp(line->commands[i].filename,line->commands[i].argv);
                                        	fprintf(stderr, "%s: Error al ejecutar el mandato %s\n", line->commands[i].argv[i],strerror(errno));
                                        	exit(1);
					}
					else { //Proceso intermedio

						if (line->background) {
                                                	signal(SIGINT,SIG_IGN);
                                        	}
                                        	else {
                                                	signal(SIGINT, manejador_hijo);
                                        	}

						dup2(pipes[i-1][0], STDIN_FILENO);
                                		close(pipes[i-1][0]);

                                		dup2(pipes[i][1], STDOUT_FILENO);
                                		close(pipes[i][1]);

						for(j=i+1; j<line->ncommands; j++){
							close(pipes[j][0]);
							close(pipes[j][1]);
						}
                                		execvp(line->commands[i].filename,line->commands[i].argv);
                                		fprintf(stderr, "%s: Error al ejecutar el mandato %s\n", line->commands[0].argv[0],strerror(errno));
		                                exit(1);
					}
                        	}
				else {//Proceso padre
					pidHijos[i]=pid;
					close(pipes[i][1]);
					pid=fork();
					if (pid != 0)
						close(pipes[i][0]);
				}
			}

			if (pid == 0)
				exit(0);
			for(i=0; i<line->ncommands; i++){
				if (line->background)
					waitpid(pidHijos[i], NULL, WNOHANG);
				else
					waitpid(pidHijos[i], NULL, 0);
			}
		}
		printf("msh> ");
	}
	return 0;
}
