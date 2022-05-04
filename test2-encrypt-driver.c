/*
 * Grace Rasmussen - ger
 * Noah Tang - ntang1
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <semaphore.h>
#include "encrypt-module.h"
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>
#include <stdlib.h>

char *inbuffer;     //int array buffer to hold input
char *outbuffer;    //int array buffer to hold output

sem_t writesem;
sem_t outputLock;
sem_t inputLock;
sem_t readsem;
sem_t reset;

int in,out; //size of in/out buffers
bool isDone;

int inputData;
int inCounter;
int outputData;
int outCounter;
int resetting;

void reset_requested() {
    //stops the Reader
    resetting = 1;
    //waits until it safe to reset
    while (1){
        sem_wait(&inputLock);
        sem_wait(&outputLock);
        if(outputData == 0 && inputData == 0 && inCounter == 0 && outCounter == 0) {
            sem_post(&inputLock);
            sem_post(&outputLock);

            break;
        }
        sem_post(&inputLock);
        sem_post(&outputLock);
    }
    //Logs the current input and output counts
    log_counts();
}

void reset_finished() {
    //signals the reader to continue
    sem_post(&reset);
    resetting = 0;
}

//thread method to read each character in the input file as they buffer is ready to receive them,
//calls read_input from encrypt-module.h to iterate thorugh file and places each character in the inbuffer.
//signals that characters are ready to be counted
void *readFile(){
    //The index at which the input buffer can write
    int currentIndex = 0;
    char c;
    while((c = read_input()) != EOF){
        //Checks if a reset has occurred
        if(resetting == 1)
            sem_wait(&reset);
        //checks if there is space inside input Buffer
        sem_wait(&readsem);
        //locks the input buffer
        sem_wait(&inputLock);
        //Stores the read character at the read index in the input buffer
        *(inbuffer + (currentIndex % in)) = c;
        //increment the read index
        currentIndex++;
        //increase the size of the input buffer
        inputData++;
        //increments the number of character ready to be counted
        inCounter++;
        //release lock on input buffer
        sem_post(&inputLock);
    }
    isDone = true;
}

//thread method to count each character in the inbuffer and add to total count 
//and character counts. after character is counted it signals it is ready to be encrypted
void *countInBuffer(){
    //The counting index in the input buffer
    int currentIndex = 0;
    while(1){
        //grabs the lock on input buffer
        sem_wait(&inputLock);
        //Checks if there are items ready to counted
        if(inCounter == 0){
            if(isDone){
                sem_post(&inputLock);
                break;
            }
            sem_post(&inputLock);
        }
        else {
            //Check of the character to be counted is not the ending character
            count_input(*(inbuffer + (currentIndex % in)));
            //increments the counting index
            currentIndex++;
            //decreases the items to be counted 
            inCounter--;
            //releases the lock on the input buffer
            sem_post(&inputLock);
        }
    }
}

//thread method that encrypts characters as they become available in the inbuffer
//writes encrypted character to outbuffer and signals that character is ready to be counter
//signals to reader that encrypted character can be overwritten through readFile()
//signals that encrypted character is ready to be counted
void *encryptFile(){
    //The index at which the input biffer can read
    int currentIndex_in = 0;
    //The index at which the output buffer can write
    int currentIndex_out = 0;
    while(1){
        //checks if there is space in the output buffer
        sem_wait(&writesem);
        //lock both buffers
        sem_wait(&inputLock);
        sem_wait(&outputLock);
        if(inputData == 0){
            if(isDone){
                sem_post(&inputLock);
                sem_post(&outputLock);
                sem_post(&writesem);
                break;
            }
            sem_post(&inputLock);
            sem_post(&outputLock);
            sem_post(&writesem);
        }
        else {
            //checks if the character ready to be dequeue is not the ending character
            /*if (*(inbuffer + (currentIndex_in % in)) != EOF) {
                *(outbuffer + (currentIndex_out % out)) = encrypt(*(inbuffer + (currentIndex_in % in)));
            } else
                *(outbuffer + (currentIndex_out % out)) = *(inbuffer + (currentIndex_in % in));
            */
            *(outbuffer + (currentIndex_out % out)) = encrypt(*(inbuffer + (currentIndex_in % in)));
            
            //Incrementes Read index in the input buffer
            currentIndex_in++;
            //Increment the Write index in the output buffer
            currentIndex_out++;

            //indicate removal of things in input buffer
            inputData--;
            //indicate stuff inside output buffer
            outputData++;
            outCounter++;
            //signal freed space in input buffer
            sem_post(&readsem);
            //unlock buffers
            sem_post(&inputLock);
            sem_post(&outputLock);
        }
    }
}

//method thread to count total and count each character in the outbuffer
//once counted, signals that the character is ready to be written to output file
void *countOutBuffer(){
    //The counting index of the output buffer
    int currentIndex = 0;
    while(1){
        //lock output buffer
        sem_wait(&outputLock);
        //Checks if there are items ready to counted
        if(outCounter == 0){
            if(isDone){
                sem_post(&outputLock);
                break;
            }
            sem_post(&outputLock);
        }
        else {
            //Check of the character to be counted is not the ending character

            count_output(*(outbuffer + (currentIndex % out)));
            //increments the counting index
            currentIndex++;
            //decreases the items to be counted 
            outCounter--;
            //unlock output buffer
            sem_post(&outputLock);
        }
    }
}

//method thread to write character to output file
//once character is written, signals encrypt thread that the output buffer is ready to 
//receive new characters
void *writeFile(){
    //The index at which the output buffer can read
    int currentIndex = 0;
    while(1){
        //lock output buffer
        sem_wait(&outputLock);
        //Checks if the output buffer is empty
        if(outputData == 0){
            if(isDone){
                sem_post(&outputLock);
                break;
            }
            sem_post(&outputLock);
        }
        else {
            write_output(*(outbuffer + (currentIndex % out)));
            //Increments the count index
            currentIndex++;
            //Decreases the number of items to be counted
            outputData--;
            //indicate space available in output buffer
            sem_post(&writesem);
            //release lock on output buffer
            sem_post(&outputLock);
        }
    }
}

int main(int argc, char *argv[]) {

    char *finput, *foutput, *flog;

    //obtaining file name
    if (argc == 4)
    {
        finput = argv[1];
        foutput = argv[2];
        flog = argv[3];
    }
    else
    {
        printf("please include input file name, output file name, and log file name");
        exit(0);
    }

    //calling init with file names
	init(finput, foutput, flog);

    //prompt user for input buffer size
    printf("What input buffer size to use? ");
    scanf("%d", &in);
    if (in <= 1)
    {
        printf("input buffer needs to be greater than 1");
        exit(0);
    }

    //prompt user for output buffer size
    printf("What output buffer size to use? ");
    scanf("%d", &out);
    if (out <= 1)
    {
        printf("output buffer needs to be greater than 1");
        exit(0);
    }

    //allocate space for inbuffer and outbuffer
    inbuffer = (char*) malloc(in * sizeof(char));
    outbuffer = (char*) malloc(in * sizeof(char));

    isDone = false;
    resetting = 0;
    inputData = 0;
    outputData = 0;
    inCounter = 0;
    outCounter = 0;

    //initialize semaphores
    sem_init(&readsem, 0, in);
    sem_init(&writesem, 0, out);
    sem_init(&inputLock, 0, 1);
    sem_init(&outputLock, 0 , 1);
    sem_init(&reset, 0, 0);

    //declare threads
    pthread_t reader;
    pthread_t inputCounter;
    pthread_t encryptor;
    pthread_t outputCounter;
    pthread_t writer;

    //creating threads
    pthread_create(&reader, NULL, readFile, NULL);
    pthread_create(&inputCounter, NULL, countInBuffer, NULL);
    pthread_create(&encryptor, NULL, encryptFile, NULL);
    pthread_create(&outputCounter, NULL, countOutBuffer, NULL);
    pthread_create(&writer, NULL, writeFile, NULL);

    pthread_join(reader, NULL);
    pthread_join(inputCounter, NULL);
    pthread_join(encryptor, NULL);
    pthread_join(outputCounter, NULL);
    pthread_join(writer, NULL);

    printf("End of file reached.\n");
    log_counts();

    free(inbuffer);
    free(outbuffer);

    return 0;
}
