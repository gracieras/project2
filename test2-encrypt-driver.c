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

char *inbuffer;     //char array buffer to hold input
char *outbuffer;    //char array buffer to hold output

sem_t readsem;
sem_t writesem;
sem_t inputLock;
sem_t outputLock;
sem_t reset;

int in,out; //size of in/out buffers
bool isDone;

int inputData;
int outputData;
int inCounter;
int outCounter;
int resetting;

//reset started
void reset_requested() 
{
    //stops the Reader
    resetting = 1;

    while (1)
    {
        sem_wait(&inputLock);
        sem_wait(&outputLock);

        if(inputData == 0 && outputData == 0 && inCounter == 0 && outCounter == 0) 
        {
            break;
        }

        sem_post(&inputLock);
        sem_post(&outputLock);
    }

    sem_post(&inputLock);
    sem_post(&outputLock);

    log_counts();
}

//reset is finished
void reset_finished() 
{
    sem_post(&reset);

    resetting = 0;
}

//thread method to read each character in the input file as they buffer is ready to receive them,
//calls read_input from encrypt-module.h to iterate thorugh file and places each character in the inbuffer.
//signals that characters are ready to be counted
void *readFile()
{
    char c;
    int reader = 0;

    while((c = read_input()) != EOF)
    {
        if(resetting == 1)
        {
            sem_wait(&reset);
        }

        sem_wait(&readsem);
        sem_wait(&inputLock);

        int tempmod = reader % in;

        *(inbuffer + tempmod) = c;
        // *(inbuffer + (reader % in)) = c;

        reader++;
        inputData++;
        inCounter++;
        
        sem_post(&inputLock);
    }
    isDone = true;
}

//thread method to count each character in the inbuffer and add to total count 
//and character counts. after character is counted it signals it is ready to be encrypted
void *countInBuffer()
{
    int countincounter = 0;

    while(1)
    {
        sem_wait(&inputLock);
        
        if(inCounter == 0)
        {
            if(isDone)
            {
                sem_post(&inputLock);
                break;
            }
            sem_post(&inputLock);
        }
        else 
        {
            int tempmod = countincounter % in;
            count_input(*(inbuffer + tempmod));
            // count_input(*(inbuffer + (countincounter % in)));
            
            countincounter++;
            inCounter--;
            
            sem_post(&inputLock);
        }
    }
    
}

//thread method that encrypts characters as they become available in the inbuffer
//writes encrypted character to outbuffer and signals that character is ready to be counter
//signals to reader that encrypted character can be overwritten through readFile()
//signals that encrypted character is ready to be counted
void *encryptFile()
{
    int encryptincounter = 0;
    int encryptoutcounter = 0;

    while(1)
    {
        sem_wait(&inputLock);
        sem_wait(&outputLock);
        sem_wait(&writesem);
        
        if(inputData == 0)
        {
            if(isDone)
            {
                sem_post(&writesem);
                sem_post(&inputLock);
                sem_post(&outputLock);
                break;
            }
            sem_post(&writesem);
            sem_post(&inputLock);
            sem_post(&outputLock);
        }
        else 
        {
            //checks if the character ready to be dequeue is not the ending character
            /*if (*(inbuffer + (currentIndex_in % in)) != EOF) {
                *(outbuffer + (currentIndex_out % out)) = encrypt(*(inbuffer + (currentIndex_in % in)));
            } else
                *(outbuffer + (currentIndex_out % out)) = *(inbuffer + (currentIndex_in % in));
            */

            int tempmodin = encryptincounter % in;
            int tempmodout = encryptoutcounter % out;
            *(outbuffer + tempmodout) = encrypt(*(inbuffer + tempmodin));
            // *(outbuffer + (currentIndex_out % out)) = encrypt(*(inbuffer + (currentIndex_in % in)));
            
            encryptincounter++;
            encryptoutcounter++;
            inputData--;
            outputData++;
            outCounter++;
            
            sem_post(&readsem);
            sem_post(&inputLock);
            sem_post(&outputLock);
        }
    }
    
}

//method thread to count total and count each character in the outbuffer
//once counted, signals that the character is ready to be written to output file
void *countOutBuffer()
{
    int countoutcounter = 0;
    while(1)
    {
        sem_wait(&outputLock);
        
        if(outCounter == 0)
        {
            if(isDone)
            {
                sem_post(&outputLock);
                break;
            }
            sem_post(&outputLock);
        }
        else 
        {
            int tempmod = countoutcounter % out;
            count_output(*(outbuffer + tempmod));
            // count_output(*(outbuffer + (countoutcounter % out)));
            
            countoutcounter++;
            outCounter--;
            
            sem_post(&outputLock);
        }
    }
    
}

//method thread to write character to output file
//once character is written, signals encrypt thread that the output buffer is ready to 
//receive new characters
void *writeFile()
{
    int writer = 0;

    while(1)
    {
        sem_wait(&outputLock);
        
        if(outputData == 0)
        {
            if(isDone)
            {
                sem_post(&outputLock);
                break;
            }
            sem_post(&outputLock);
        }
        else 
        {
            // write_output(*(outbuffer + (currentIndex % out)));
            int tempmod = writer % out;
            write_output(*(outbuffer + tempmod));

            writer++;
            outputData--;
            
            sem_post(&writesem);
            sem_post(&outputLock);
        }
    }
    
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

    // printf("set buffer size success\n");

    //allocate space for inbuffer and outbuffer
    inbuffer = (char*) malloc(in * sizeof(char));
    outbuffer = (char*) malloc(in * sizeof(char));

    // printf("allocate memory success\n");

    isDone = false;
    resetting = 0;
    inputData = 0;
    outputData = 0;
    inCounter = 0;
    outCounter = 0;

    // printf("initialize variables success\n");

    //declare threads
    pthread_t reader;
    pthread_t inputCounter;
    pthread_t encryptor;
    pthread_t outputCounter;
    pthread_t writer;

    // printf("declare pthread success\n");
    
    //initialize semaphores
    sem_init(&readsem, 0, in);
    sem_init(&writesem, 0, out);
    sem_init(&inputLock, 0, 1);
    sem_init(&outputLock, 0 , 1);
    sem_init(&reset, 0, 0);

    // printf("initialize sem success\n");

    //creating threads
    pthread_create(&reader, NULL, readFile, NULL);
    pthread_create(&inputCounter, NULL, countInBuffer, NULL);
    pthread_create(&encryptor, NULL, encryptFile, NULL);
    pthread_create(&outputCounter, NULL, countOutBuffer, NULL);
    pthread_create(&writer, NULL, writeFile, NULL);

    // printf("create pthread success\n");

    //adding threads
    pthread_join(reader, NULL);
    pthread_join(inputCounter, NULL);
    pthread_join(encryptor, NULL);
    pthread_join(outputCounter, NULL);
    pthread_join(writer, NULL);

    // printf("pthread join success\n");

    //outputing data
    log_counts();

    //freeing memory
    free(inbuffer);
    free(outbuffer);

    return 0;
}
