/* pcthreads.c */
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdint.h>
#include <semaphore.h>

#define MAXTHREAD 10
#define RANGE 1
#define ALLDONE 2

/* gcc -o pcthreads pcthreads.c -lpthread */

struct msg{
    int iSender; //sender of message (0... number of threads)
    int type;    //its type
    int value1;  //first value
    int value2;  // second value
};

//global variable
struct msg **mailboxes;
pthread_t **threads;
sem_t **senderSem;
sem_t **receSem;

void SendMsg(int iTo, struct msg *pMsg){
    sem_wait(senderSem[iTo]); //wait for mailbox to be available
    mailboxes[iTo] = pMsg;
    sem_post(receSem[iTo]); // release the mailbox semaphore

}

void RecvMsg(int iFrom, struct msg *pMsg){
    sem_wait(receSem[iFrom]); //wait for message
    *pMsg = *mailboxes[iFrom]; 
    sem_post(senderSem[iFrom]); //release the mailbox semaphore
}

void *threadFunction(void *arg){
    int threadId = (int)(intptr_t) arg;
    
    struct msg *message;

    message = (struct msg *)malloc(sizeof(struct msg));

    RecvMsg(threadId, message); //Wait for a RANGE message
    if(message->type != RANGE){
        printf("Thread got wrong type of message.\n");
    }

    message->iSender = threadId;

    int sum = 0;
    for(int i = message->value1; i <= message->value2; i++){
        sum += i;
    }

    message->type = ALLDONE;
    message->value1 = sum;

    SendMsg(0, message); //Send the ALLDONE message

    pthread_exit(NULL);
}

int main(int argc, char * argv[]){
    if(argc != 3){
        printf("Usage: %s <numOfThreads>, <Number to add to>\n", argv[0]);
        exit(1);
    } 

    int numThreads = atoi(argv[1]);
    int upperLim = atoi(argv[2]);

    if(numThreads < 1 || numThreads > MAXTHREAD){
        printf("Number of threads must be between 1 and %d.\n", MAXTHREAD);
        exit(1);
    }

    if(upperLim < 1){
        printf("The upper limit must be positive.");
        exit(1);
    }
    //mem alloc
    mailboxes = (struct msg **)malloc((numThreads + 1) * sizeof(struct msg *));
    struct msg *received = (struct msg *)malloc(sizeof(struct msg));
    senderSem = (sem_t **)malloc((numThreads + 1) * sizeof(sem_t *));
    receSem = (sem_t **)malloc((numThreads + 1) * sizeof(sem_t * ));
    threads = (pthread_t **)malloc(numThreads * sizeof(pthread_t *));


    //inti semaphores and make the mailboxes:
    for(int i = 0; i <= numThreads; i++){
        senderSem[i] = (sem_t *)malloc(sizeof(sem_t));
        receSem[i] = (sem_t*)malloc(sizeof(sem_t));

        sem_init(senderSem[i], 0, 1);
        sem_init(receSem[i], 0, 0);
    }

   //make the messages to send.
    
    struct msg **parentMessage;
    parentMessage = (struct msg **)malloc(numThreads * sizeof(struct msg *));
    int range = upperLim / numThreads;
    int temp = 0;

    for(int i = 0; i < numThreads; i++){
        parentMessage[i] = (struct msg *)malloc(sizeof(struct msg));
        parentMessage[i]-> iSender = 0;
        parentMessage[i]-> type = RANGE;
        parentMessage[i]-> value1 = temp + 1;
        temp += range;
        parentMessage[i]-> value2 = (i == numThreads - 1) ? upperLim : temp;

        SendMsg(i + 1, parentMessage[i]); //Send RANGE to child threads.
    }

    //Make the threads:
    for(int i = 0; i < numThreads; i++){
        threads[i] = (pthread_t *)malloc(sizeof(pthread_t));
        pthread_create(threads[i], NULL, &threadFunction, (void *)(intptr_t)(i + 1));
    }

    int total = 0;
    for(int i = 1; i <= numThreads; i++){
        RecvMsg(0, received); //receive ALLDONE
        total += received->value1;
    }

    printf("The total for 1 to %d using %d threads is %d.\n", upperLim, numThreads, total);

    //clean up
    for(int i = 0; i < numThreads; i++){
        free(parentMessage[i]);
        free(threads[i]);    
    }

    for(int i = 0; i <= numThreads; i++){
        sem_destroy(senderSem[i]);
        sem_destroy(receSem[i]);
    }

    free(received);
    free(senderSem);
    free(receSem);
    free(threads);
    free(mailboxes);
    free(parentMessage);

    return 0;
}
