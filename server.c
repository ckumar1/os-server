#include "cs537.h"
#include "request.h"



typedef struct {
 int fd; // stores the fd values
 long sizeOfRequest; //total buffer size
 int portNbr;
 char* filename;
} request;

pthread_t **cid;
request **buffer;
int fillPtr, max, numOfFull, usedPtr, algorithm;

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t fill = PTHREAD_COND_INITIALIZER;
pthread_cond_t empty = PTHREAD_COND_INITIALIZER;

int requestSizeCompare(const void *request1, const void *request2) {
    return ((*(request **)request1)->sizeOfRequest - (*(request **)request2)->sizeOfRequest);
}

int requestedFileNameCompare(const void *request1, const void *request2) {
    char* filename1 = (*(request **)request1)->filename;
    char* filename2 = (*(request **)request2)->filename;
    return (sizeof(filename1) - sizeof(filename2));
}
// 
// server.c: A very, very simple web server
//
// To run:
//  server <portnum (above 2000)>
//
// Repeatedly handles HTTP requests sent to this port number.
// Most of the work is done within routines written in request.c
//

// CS537: Parse the new arguments too
void getargs(int *portNbr, int *sizeOfBuffers, int *threads, int *alg, int argc, char *argv[])
{
    if (argc != 5) {
            fprintf(stderr, "Usage: %s <portNbr> <sizeOfBuffers> <threads> <schedalg>\n", argv[0]);
            exit(1);
        }

    *portNbr = atoi(argv[1]);
    *sizeOfBuffers = atoi(argv[3]);
    *threads = atoi(argv[2]);
    
    if(strcasecmp(argv[4], "FIFO") == 0) {
        *alg = -2;
    } else if(strcasecmp(argv[4], "SFF") == 0) {
        *alg = -1;
    } else if(strcasecmp(argv[4], "SFNF") == 0) {
        *alg = -3;
    }
}




void *consumer(void *arg) {    
    while(1) {
        pthread_mutex_lock(&lock);
        while(numOfFull == 0) {
            pthread_cond_wait(&fill, &lock);
        }

        request* req;
        
        if(algorithm == -2) { //FIFO
            req = (request*)buffer[usedPtr];
            usedPtr = (usedPtr+1) % max;
        } else if(algorithm == -1) {  //SFF
            req = (request *)buffer[0];
            buffer[0] = buffer[fillPtr - 1];
            if(fillPtr > 1) {
                qsort(buffer, fillPtr, sizeof(*buffer), requestSizeCompare);
            }
            fillPtr--;
        } else if(algorithm == -3) {  //SFNF
            req = (request *)buffer[0];
            buffer[0] = buffer[fillPtr - 1];
            if(fillPtr > 1) {
                qsort(buffer, fillPtr, sizeof(*buffer), requestedFileNameCompare);
            }
            fillPtr--;
        }
        
        numOfFull--;

        pthread_cond_signal(&empty);
        pthread_mutex_unlock(&lock);

        requestHandle(req->fd);
        Close(req->fd);
    }
}

int main(int argc, char *argv[])
{
    int listenfd, connfd, _portNbr, _threads, _sizeOfBuffers, clientlen;

    struct sockaddr_in clientaddr;

    getargs(&_portNbr, &_sizeOfBuffers, &_threads, &algorithm, argc, argv);
    
    //maximum of _sizeOfBuffers
    max = _sizeOfBuffers;
    numOfFull = 0;
    fillPtr = 0;
    usedPtr = 0;

    // allocate the space  to _sizeOfBuffers for FIFO and SFF
    buffer = malloc(_sizeOfBuffers * sizeof(request));


    // 
    // CS537: Create some _threads...
    //
    cid = malloc(_threads*sizeof(*cid));
     
    int i;
    for(i = 0; i< _threads; i++) {
        cid[i] = malloc(sizeof(pthread_t));
        pthread_create(cid[i], NULL, consumer, NULL);
      }


    listenfd = Open_listenfd(_portNbr);
    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *) &clientlen);


        // 
        // CS537: In general, don't handle the request in the main thread.
        // Save the relevant info in a buffer and have one of the worker threads 
        // do the work. However, for SFF, you may have to do a little work
        // here (e.g., a stat() on the filename) ...
        // 

        pthread_mutex_lock(&lock);
        while(numOfFull == max) {
            pthread_cond_wait(&empty, &lock);
        }

        request *req = malloc(sizeof(request)); 
        req->fd = connfd;
        req->portNbr = _portNbr;
        req->sizeOfRequest = sizeOfFileRequested(connfd);
        req->filename = requestedFileName(connfd);
        //printf("%s\n", req->filename);
        
        if(algorithm == -1) { //SFF            
            buffer[fillPtr] = req;
            fillPtr++;
            if(fillPtr > 1) {
                qsort(buffer, fillPtr, sizeof(*buffer),requestSizeCompare);
            }
        } else if(algorithm == -3) { //SFNF
            buffer[fillPtr] = req;
            fillPtr++;
            if(fillPtr > 1) {
                qsort(buffer, fillPtr, sizeof(*buffer),requestedFileNameCompare);
            }
        }else if(algorithm == -2){ //FIFO
            buffer[fillPtr] = req;
            fillPtr = (fillPtr+1) % max;   

        }

       
        numOfFull++;// it start from 0
        

        pthread_cond_signal(&fill);
        pthread_mutex_unlock(&lock);

        // moved to consumer
       // requestHandle(connfd);
    } // end of while loop

} // end of main
