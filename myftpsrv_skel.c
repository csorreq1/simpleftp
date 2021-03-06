#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include <unistd.h>
#include <err.h>

#include <netinet/in.h>

#define BUFSIZE 512
#define CMDSIZE 4
#define PARSIZE 100

#define MSG_220 "220 srvFtp version 1.0\r\n"
#define MSG_331 "331 Password required for %s\r\n"
#define MSG_230 "230 User %s logged in\r\n"
#define MSG_530 "530 Login incorrect\r\n"
#define MSG_221 "221 Goodbye\r\n"
#define MSG_550 "550 %s: no such file or directory\r\n"
#define MSG_299 "299 File %s size %ld bytes\r\n"
#define MSG_226 "226 Transfer complete\r\n"

/**
 * function: receive the commands from the client
 * sd: socket descriptor
 * operation: \0 if you want to know the operation received
 *            OP if you want to check an especific operation
 *            ex: recv_cmd(sd, "USER", param)
 * param: parameters for the operation involve
 * return: only usefull if you want to check an operation
 *         ex: for login you need the seq USER PASS
 *             you can check if you receive first USER
 *             and then check if you receive PASS
 **/
bool recv_cmd(int sd, char *operation, char *param) {
    char buffer[BUFSIZE], *token;
    int recv_s;

    // receive the command in the buffer and check for errors

recv_s = recv(sd, buffer, BUFSIZE, 0);
    if (recv_s < 0) {
        warn("error receiving data");
    }
	if (recv_s == 0){
        errx(1, "connection closed by host");
    }

    // expunge the terminator characters from the buffer
    buffer[strcspn(buffer, "\r\n")] = 0;

    // complex parsing of the buffer
    // extract command receive in operation if not set \0
    // extract parameters of the operation in param if it needed
    token = strtok(buffer, " ");
    if (token == NULL || strlen(token) < 4) {
        warn("not valid ftp command");
        return false;
    } else {
        if (operation[0] == '\0') strcpy(operation, token);
        if (strcmp(operation, token)) {
            warn("abnormal client flow: did not send %s command", operation);
            return false;
        }
        token = strtok(NULL, " ");
        if (token != NULL) strcpy(param, token);
    }
    return true;
}

/**
 * function: send answer to the client
 * sd: file descriptor
 * message: formatting string in printf format
 * ...: variable arguments for economics of formats
 * return: true if not problem arise or else
 * notes: the MSG_x have preformated for these use
 **/
bool send_ans(int sd, char *message, ...){
    char buffer[BUFSIZE];

    va_list args;
    va_start(args, message);

    vsprintf(buffer, message, args);
    va_end(args);
    // send answer preformated and check errors

if(send(sd, buffer, sizeof(buffer), 0) == -1) {
          printf("No se pudo enviar el mensaje");
          return false;
          } 
          else {
	        return true;
	    }


}

/**
 * function: RETR operation
 * sd: socket descriptor
 * file_path: name of the RETR file
 **/

void retr(int sd, char *file_path) {
    FILE *file;    
    int bread;
    long fsize;
    char buffer[BUFSIZE];
	
    file = fopen(file_path, "r");
	

    // check if file exists if not inform error to client
	if (file == NULL) {
	        send_ans(sd, MSG_550, file_path);
	        return;
	    } 


    // send a success message with the file length

    // important delay for avoid problems with buffer size
    sleep(1);
	
	fseek(file, 0L, SEEK_END);
	    fsize = ftell(file);
        send_ans(sd, MSG_299, file_path, fsize);
	    rewind(file);

    // send the file
	while (1){
         bread = fread(buffer, 1, BUFSIZE, file);

         if (bread > 0) {
	            send (sd, buffer, bread, 0);
	            sleep(1);
         } if (bread < BUFSIZE){
             break;
	    }

    }

    // close the file
	fclose(file);

    // send a completed transfer message
	send_ans(sd, MSG_226);
}
/**
 * funcion: check valid credentials in ftpusers file
 * user: login user name
 * pass: user password
 * return: true if found or false if not
 **/
bool check_credentials(char *user, char *pass) {
    FILE *file;
    char *path = "./ftpusers", *line = NULL, cred[100];
    size_t len = 0;
    bool found = false;

    // make the credential string
    strcpy(cred,user);
    strcat(cred,":");
    strcat(cred, pass);
    strcat(cred, "\n");

    // check if ftpusers file it's present
      file = fopen (path, "r");
    if (file==NULL){
        printf("No se puede abrir el archivo\n");
    }

    // search for credential string
    line = (char*)malloc(100*sizeof(char));

    while(!feof(file)){
        fgets(line,100, file);
        if(strcmp(cred,line)==0){
            found = true;

        }
    }

    // close file and release any pointes if necessary
    fclose(file);
    free(line);
    
    // return search status
     return found;
}

/**
 * function: login process management
 * sd: socket descriptor
 * return: true if login is succesfully, false if not
 **/
bool authenticate(int sd) {
    char user[PARSIZE], pass[PARSIZE];

    // wait to receive USER action
	recv_cmd(sd, "USER", user);

    // ask for password
	send_ans(sd, MSG_331, user);

    // wait to receive PASS action
	 recv_cmd(sd, "PASS", pass);

    // if credentials don't check denied login
	if(!check_credentials(user, pass)) {
	      send_ans(sd, MSG_530);
	      return false;
      }

    // confirm login
	send_ans(sd, MSG_230, user);
	return true;
}

/**
 *  function: execute all commands (RETR|QUIT)
 *  sd: socket descriptor
 **/

void operate(int sd) {
    char op[CMDSIZE], param[PARSIZE];

    while (true) {
        op[0] = param[0] = '\0';
        // check for commands send by the client if not inform and exit
	if(!recv_cmd(sd, op, param)) {
	            exit(1);
	        }

        if (strcmp(op, "RETR") == 0) {
            retr(sd, param);
        } else if (strcmp(op, "QUIT") == 0) {
            // send goodbye and close connection

		send_ans(sd, MSG_221);


            break;
        } else {
            // invalid command
            // furute use
        }
    }
}

/**
 * Run with
 *         ./mysrv <SERVER_PORT>
 **/
int main (int argc, char *argv[]) {

    // arguments checking
    if(argc =! 2){
      printf("Arguments Error\n" );
      exit(1);
    }
    // reserve sockets and variables space
    int socket_descriptor, newsockfd;
    struct sockaddr_in servaddr, clieaddr;
    int puerto = atoi (argv[1]);
    socklen_t clienSize; 

    // create server socket and check errors
     socket_descriptor = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_descriptor == -1){
      perror("Falla socket");
	    exit (1);
    }

    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(puerto);
    servaddr.sin_addr.s_addr = INADDR_ANY;
    
    // bind master socket and check errors
     if (bind(socket_descriptor, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0){
        printf("binding failed");
    }

    // make it listen
    if (listen (socket_descriptor, 5) < 0){
        printf("Listen error");
        exit(1);
    }

    // main loop
    while (true) {
        // accept connectiones sequentially and check errors
         clienSize = sizeof(clieaddr);

        newsockfd = accept(socket_descriptor, (struct sockaddr *) &clieaddr, &clienSize);
        if (newsockfd < 0){
            printf("Error on Accept");
        }

        // send hello
        send_ans(newsockfd, MSG_220);


        // operate only if authenticate is true
     if(authenticate(newsockfd) != true){
            close(newsockfd);
        }else{
            operate(newsockfd);
        }
       
    }

    // close server socket
      close(socket_descriptor);


    return 0;
}
