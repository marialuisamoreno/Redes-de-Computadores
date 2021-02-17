/* Maria Luisa Santos Moreno Sanches - 111859 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>

#define MAX_REQ_LEN 1000
#define FBLOCK_SIZE 4000

// methods
#define REQ_GET "GET"
#define REQ_CREATE "CREATE"
#define REQ_REMOVE "REMOVE"
#define REQ_APPEND "APPEND"

#define COD_OK0 "OK-0 method OK\n"
#define COD_OK1_FILE "OK-1 File open OK\n"
#define COD_OK2_CREATE "OK-2 File created OK\n"
#define COD_OK3_REMOVE "OK-3 File removed OK\n"
#define COD_OK4_APPEND "OK-4 File edited OK\n"

#define COD_ERROR_0_METHOD "ERROR-0 Method not supported\n"
#define COD_ERROR_1_FILE "ERROR-1 File could not be opened\n"
#define COD_ERROR_2_FILE "ERROR-2 File could not be created\n"
#define COD_ERROR_3_FILE "ERROR-3 File could not be removed\n"

struct req_t {
    char method[128];
    char filename[356];
    char content[356];
};

typedef struct req_t req_t;

typedef struct {
    int sc;
    char *request;
    
    int cliente;
} arguments;

void get_request(req_t *r, char *rstr){
    bzero(r, sizeof(req_t));
    sscanf(rstr, "%s %s %[^NULL]", r->method, r->filename, r->content);
}

void send_file (int sockfd, req_t r){
    int fd, nr;
    unsigned char fbuff[FBLOCK_SIZE];
    fd = open(r.filename, O_RDONLY, S_IRUSR);
    if (fd == -1){
        // nao foi possivel abrir o arquivo
        perror("open");
        send(sockfd, COD_ERROR_1_FILE, strlen(COD_ERROR_1_FILE), 0);
        return;
    }
    send(sockfd, COD_OK1_FILE, strlen(COD_OK1_FILE), 0);
    
    do{
        bzero(fbuff, FBLOCK_SIZE);
        nr = read(fd, fbuff, FBLOCK_SIZE);
        if (nr > 0){
            send(sockfd, fbuff, nr, 0);
        }
    }while(nr > 0);
    close(fd);
    return;
}

void create_file (int sockfd, req_t r){
    int fd;
    fd = open(r.filename, O_RDWR | O_CREAT, S_IRUSR | S_IRGRP | S_IROTH);
    if (fd == -1){
        // nao foi possivel criar o arquivo
        perror("create");
        send(sockfd, COD_ERROR_2_FILE, strlen(COD_ERROR_2_FILE), 0);
        return;
    }
    send(sockfd, COD_OK2_CREATE, strlen(COD_OK2_CREATE), 0);

    close(fd);
    return;
}

void remove_file (int sockfd, req_t r){
    int fd;
    fd = remove(r.filename);
    if (fd == -1){
        // nao foi possivel remover o arquivo
        perror("remove");
        send(sockfd, COD_ERROR_3_FILE, strlen(COD_ERROR_3_FILE), 0);
        return;
    }
    send(sockfd, COD_OK3_REMOVE, strlen(COD_OK3_REMOVE), 0);

    close(fd);
    return;
}

void append_file (int sockfd, req_t r){
    int fd;
    fd = open(r.filename, O_WRONLY | O_APPEND);
    if (fd == -1){
        // nao foi possivel abrir o arquivo para append
        perror("append");
        send(sockfd, COD_ERROR_1_FILE, strlen(COD_ERROR_1_FILE), 0);
        return;
    }
    lseek(fd, 0, SEEK_SET);
    write(fd, r.content, strlen(r.content));
    send(sockfd, COD_OK4_APPEND, strlen(COD_OK4_APPEND), 0);
    close(fd);
    return;
}

void proc_request (int sockfd, req_t r){
    // operação GET
    if(strcmp(r.method, REQ_GET) == 0){
        send(sockfd, COD_OK0, strlen(COD_OK0), 0);
        send_file(sockfd, r);
    }
    // operação CREATE
    else if (strcmp(r.method, REQ_CREATE) == 0){
        send(sockfd, COD_OK0, strlen(COD_OK0), 0);
        create_file(sockfd, r);
    }
    // operação REMOVE
    else if (strcmp(r.method, REQ_REMOVE) == 0){
        send(sockfd, COD_OK0, strlen(COD_OK0), 0);
        remove_file(sockfd, r);
    }
    // operação APPEND
    else if (strcmp(r.method, REQ_APPEND) == 0){
        send(sockfd, COD_OK0, strlen(COD_OK0), 0);
        append_file(sockfd, r);
    }
    // operação não encontrada
    else{
        send(sockfd, COD_ERROR_0_METHOD, strlen(COD_ERROR_0_METHOD), 0);
    }
    return;
}

void* separa_thread (void* _arg){
    arguments arg = *((arguments *)(_arg));
    int sc = arg.sc;
    char *request = arg.request;
    int cliente = arg.cliente;
    req_t req;

    // recv
    int nr;
    nr = recv(sc, request, MAX_REQ_LEN, 0);
    if (nr > 0){
        // recebi dados
        get_request(&req, request);
        printf("method: %s\nfilename: %s\nclient: %d\n\n", req.method, req.filename, cliente);
        proc_request(sc, req);
    }

    // send
    close(sc);
    pthread_exit(NULL);
}

int main (int argc, char **argv){
    // compile: gcc -pthread -Wall -o server server.c
    // ./server <porta>
    if(argc != 2){
        printf("Use: %s <porta>\n", argv[0]);
        return 0;
    }

    // socket (man socket)
    int sl = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(sl == -1){
        perror("socket");
        return 0;
    }

    struct sockaddr_in saddr;
    saddr.sin_port = htons(atoi(argv[1]));
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = INADDR_ANY;

    // bind
    if (bind(sl, (struct sockaddr *)&saddr, sizeof(struct sockaddr_in)) == -1) {
        perror("bind");
        return 0;
    }

    // listen
    if (listen(sl, 1000) == -1) {
        perror("listen");
        return 0;
    }

    struct sockaddr_in caddr;
    int addr_len;
    int sc;
    char request[MAX_REQ_LEN];

    while(1){
        addr_len = sizeof(struct sockaddr_in);

        // accept
        sc = accept(sl, (struct sockaddr *)&caddr, (socklen_t *)&addr_len);
        if(sc == -1){
            perror("accept");
            // tento uma nova conexão
            continue;
        }
        printf("Conectado com cliente %s: %d\n", inet_ntoa(caddr.sin_addr), ntohs(caddr.sin_port));

        bzero(request, MAX_REQ_LEN);

        // criação da Thread
        pthread_t thread_id;
        arguments arg;
        arg.sc = sc;
        arg.request = request;
        arg.cliente = ntohs(caddr.sin_port);

        pthread_create(&thread_id, NULL, separa_thread, (void *) &arg); 
        //pthread_join(thread_id, NULL); 
    }

    close(sl);
    return 0;
}