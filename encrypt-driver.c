#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <semaphore.h>
#include "encrypt-module.h"
#include <time.h>
#include <stdlib.h>

int *inbuffer;     //int array buffer to hold input
int *outbuffer;    //int array buffer to hold output

sem_t encryptinsem;
sem_t encryptoutsem;
sem_t countinsem;
sem_t countoutsem;
sem_t readsem;
sem_t writesem;

int resetting;

//counters for read/write and counters for buffers
int reader, incounter, encryptincounter, encryptoutcounter, outcounter, writer;

int in,out; //size of in/out buffers

void reset_requested() 
{
    resetting = 1;
    sem_wait(&countinsem);
    sem_wait(&countoutsem);
	log_counts();
    reset_finished();
}

void reset_finished() 
{
	resetting = 0;
    sem_post(&countinsem);
    sem_post(&countoutsem);
}

//thread method to read each character in the input file as they buffer is ready to receive them,
//calls read_input from encrypt-module.h to iterate thorugh file and places each character in the inbuffer.
//signals that characters are ready to be counted
void *readFile(void *param) {

    int c;
    reader = 0;
    while ((c = read_input()) != EOF) {
        while(resetting == 1) {
            reset_requested();
        }
        sem_wait(&readsem);
        inbuffer[reader] = c;
        sem_post(&countinsem);
        reader = (reader + 1) % in;
    }   
    inbuffer[reader] = EOF;
    sem_post(&countinsem);    
    pthread_exit(0);
}

//thread method to count each character in the inbuffer and add to total count 
//and character counts. after character is counted it signals it is ready to be encrypted
void *countInBuffer(void *param) {

    incounter = 0;
    while (1) {
        while(resetting == 1) {
            reset_requested();
        }
        sem_wait(&countinsem);
        
        if(inbuffer[incounter] == EOF) {
            sem_post(&encryptinsem);
            break;
        }
        
        count_input(inbuffer[incounter]);
        sem_post(&encryptinsem);
        incounter = (incounter + 1) % in;
    }
    pthread_exit(0);
}

//thread method that encrypts characters as they become available in the inbuffer
//writes encrypted character to outbuffer and signals that character is ready to be counter
//signals to reader that encrypted character can be overwritten through readFile()
//signals that encrypted character is ready to be counted
void *encryptFile(void *param) {

    encryptincounter = 0;
    encryptoutcounter = 0;
    while(1) {
        while(resetting == 1) {
            reset_requested();
        }
        sem_wait(&encryptinsem);

        if(inbuffer[encryptincounter] == EOF) {
            outbuffer[encryptoutcounter] = EOF;
            sem_post(&countoutsem); //potential error
            break;
        }
        sem_wait(&encryptoutsem);
        outbuffer[encryptoutcounter] = encrypt(inbuffer[encryptincounter]);
        sem_post(&readsem);
        sem_post(&countoutsem);

        encryptincounter = (encryptincounter + 1) % in;
        encryptoutcounter = (encryptoutcounter + 1) % out;
    }
    pthread_exit(0);
}

//method thread to count total and count each character in the outbuffer
//once counted, signals that the character is ready to be written to output file
void *countOutBuffer(void *param) {

    outcounter = 0;
    while(1) {
        while(resetting == 1) {
            reset_requested();
        }
        sem_wait(&countoutsem);
        
        if(outbuffer[outcounter] == EOF) {
            sem_post(&writesem);
            break;
        }   
        count_output(outbuffer[outcounter]);
        sem_post(&writesem);
        outcounter = (outcounter + 1) % out;
    }
    pthread_exit(0);
}

//method thread to write character to output file
//once character is written, signals encrypt thread that the output buffer is ready to 
//receive new characters
void *writeFile(void *param) {

    writer = 0;
    while(1) {
        while(resetting == 1) {
            reset_requested();

        }
        sem_wait(&writesem);
		
        if(outbuffer[writer] == EOF) {
            break;
        }
        write_output(outbuffer[writer]);
        sem_post(&encryptoutsem);
        writer = (writer + 1) % out;
    }
    pthread_exit(0);
}

int main(int argc, char *argv[]) 
{
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
    printf("please give input buffer size.");
    scanf("%d", &in);
    if (in <= 1)
    {
        printf("input buffer needs to be greater than 1");
        exit(0);
    }

    //prompt user for output buffer size
    printf("please give output buffer size.");
    scanf("%d", &out);
    if (out <= 1)
    {
        printf("output buffer needs to be greater than 1");
        exit(0);
    }

    inbuffer = malloc(sizeof(char) * in);
    outbuffer = malloc(sizeof(char) * out);

    reader = 0;
    incounter = 0;
    encryptincounter = 0;
    encryptoutcounter = 0;
    outcounter = 0;
    writer = 0;
    resetting = 0;

    //creating threads
	pthread_t reader;
	pthread_t inputCounter;
	pthread_t encryptor;
	pthread_t outputCounter;
	pthread_t writer;
	pthread_attr_t attr;

    // sem_init()
    // sem_t encryptinsem;
    // sem_t encryptoutsem;
    // sem_t countinsem;
    // sem_t countoutsem;
    // sem_t readsem;
    // sem_t writesem;

    sem_init(&encryptinsem, 0, 0);
    sem_init(&encryptoutsem, 0, 0);
    sem_init(&countinsem, 0, 0);
    sem_init(&countoutsem, 0, 0);
    sem_init(&readsem, 0, 0);
    sem_init(&writesem, 0, 0);

    pthread_attr_init(&attr);
	pthread_create(&reader, &attr, &readFile, 0);
    pthread_create(&inputCounter, &attr, &countInBuffer, 0);
    pthread_create(&encryptor, &attr, &encryptFile, 0);
    pthread_create(&outputCounter, &attr, &countOutBuffer, 0);
    pthread_create(&writer, &attr, &writeFile, 0);

	pthread_join(reader, NULL);
    pthread_join(inputCounter, NULL);
    pthread_join(encryptor, NULL);
    pthread_join(outputCounter, NULL);
    pthread_join(writer, NULL);

	sem_destroy(&readsem);
    sem_destroy(&countinsem);
    sem_destroy(&encryptinsem);
    sem_destroy(&encryptoutsem);
    sem_destroy(&countoutsem);
    sem_destroy(&writesem);

    //log character counts
    // char c;
	// while ((c = read_input()) != EOF) 
    // { 
	// 	count_input(c); 
	// 	c = encrypt(c); 
	// 	count_output(c); 
	// 	write_output(c); 
	// } 
	// printf("End of file reached.\n"); 
	log_counts();

    //freeing memory
	free(inbuffer);
    free(outbuffer);
}