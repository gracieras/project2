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

sem_t readsem; //space_inside_inputBuffer
sem_t writesem; //space_inside_outputBuffer
sem_t inputLock;
sem_t outputLock;
sem_t reset;

int inputData; //stuff_inside_InBuffer;
int outputData; //stuff_inside_OutBuffer;
int iCounter; //chars_to_count_input;
int oCounter; //chars_to_count_output;
int resetting;

bool isDone;

//counters for read/write and counters for buffers
int reader, incounter, encryptincounter, encryptoutcounter, outcounter, writer;

int in,out; //size of in/out buffers

void reset_requested() 
{
    resetting = 1;

    while (1)
    {
        sem_wait(&inputLock);
        sem_wait(&outputLock);

        if (inputData == 0 && outputData == 0 && iCounter == 0 && oCounter == 0)
        {
            sem_post(&inputLock);
            sem_post(&outputLock);
            break;
        }

        sem_post(&inputLock);
        sem_post(&outputLock);
    }
	log_counts();

    // printf("Total input count with the current key is %d\n", get_input_total_count());
    // printf("A:%d B:%d C:%d D:%d E:%d F:%d G:%d H:%d I:%d J:%d K:%d L:%d M:%d N:%d O:%d P:%d Q:%d R:%d S:%d T:%d U:%d V:%d W:%d X:%d Y:%d Z:%d\n", get_input_count('a'), get_input_count('b'), get_input_count('c'), get_input_count('d'), get_input_count('e'), get_input_count('f'), get_input_count('g'), get_input_count('h'), get_input_count('i'), get_input_count('j'), get_input_count('k'), get_input_count('l'), get_input_count('m'), get_input_count('n'), get_input_count('o'), get_input_count('p'), get_input_count('q'), get_input_count('r'), get_input_count('s'), get_input_count('t'), get_input_count('u'), get_input_count('v'), get_input_count('w'), get_input_count('x'), get_input_count('y'), get_input_count('z'));
    // printf("Total output count with the current key is %d\n", get_output_total_count());
    // printf("A:%d B:%d C:%d D:%d E:%d F:%d G:%d H:%d I:%d J:%d K:%d L:%d M:%d N:%d O:%d P:%d Q:%d R:%d S:%d T:%d U:%d V:%d W:%d X:%d Y:%d Z:%d\n", get_output_count('a'), get_output_count('b'), get_output_count('c'), get_output_count('d'), get_output_count('e'), get_output_count('f'), get_output_count('g'), get_output_count('h'), get_output_count('i'), get_output_count('j'), get_output_count('k'), get_output_count('l'), get_output_count('m'), get_output_count('n'), get_output_count('o'), get_output_count('p'), get_output_count('q'), get_output_count('r'), get_output_count('s'), get_output_count('t'), get_output_count('u'), get_output_count('v'), get_output_count('w'), get_output_count('x'), get_output_count('y'), get_output_count('z'));
}

void reset_finished() 
{
	resetting = 0;
    sem_post(&reset);
}

//thread method to read each character in the input file as they buffer is ready to receive them,
//calls read_input from encrypt-module.h to iterate thorugh file and places each character in the inbuffer.
//signals that characters are ready to be counted
void *readFile() 
{
    char c;
    reader = 0;
    while ((c = read_input()) != EOF) 
    {
        if(resetting == 1) 
        {
            sem_wait(&reset);
        }

        sem_wait(&readsem);
        sem_wait(&inputLock);

        inbuffer[reader % in] = c;

        // *(input_buffer + (currentIndex % input_buffer_size)) = c;

        reader++;
        inputData++;
        iCounter++;

        sem_post(&inputLock);
    }

    // inbuffer[reader % in] = EOF;
    // sem_post(&countinsem);  

    isDone = true;  
    // pthread_exit(0);
}
//thread method to count each character in the inbuffer and add to total count 
//and character counts. after character is counted it signals it is ready to be encrypted
void *countInBuffer(void *param) 
{
    incounter = 0;

    while (1) 
    {
        sem_wait(&inputLock);
        
        if(iCounter == 0 && isDone) 
        {
            sem_post(&inputLock);
            break;
        }
        
        count_input(inbuffer[incounter % in]);

        incounter++;
        iCounter--;

        sem_post(&inputLock);
    }
    // pthread_exit(0);
}

//thread method that encrypts characters as they become available in the inbuffer
//writes encrypted character to outbuffer and signals that character is ready to be counter
//signals to reader that encrypted character can be overwritten through readFile()
//signals that encrypted character is ready to be counted
void *encryptFile(void *param) 
{
    encryptincounter = 0;
    encryptoutcounter = 0;

    while(1) 
    {
        sem_wait(&writesem);
        sem_wait(&inputLock);
        sem_wait(&outputLock);

        if(iCounter == 0 && isDone) 
        {
            // outbuffer[encryptoutcounter] = EOF;
            sem_post(&writesem);
            sem_post(&inputLock);
            sem_post(&outputLock);
            break;
        }

        // sem_wait(&encryptoutsem);
        outbuffer[encryptoutcounter % out] = encrypt(inbuffer[encryptincounter % in]);

        inputData--;
        outputData++;
        oCounter++;

        encryptincounter++;
        encryptoutcounter++;
        
        sem_post(&readsem);
        sem_post(&inputLock);
        sem_post(&outputLock);
    }
    // pthread_exit(0);
}

//method thread to count total and count each character in the outbuffer
//once counted, signals that the character is ready to be written to output file
void *countOutBuffer(void *param) 
{
    outcounter = 0;

    while(1) 
    {
        sem_wait(&outputLock);
        
        if(oCounter == 0 && isDone) 
        {
            sem_post(&outputLock);
            break;
        }

        count_output(outbuffer[outcounter % out]);
        
        outcounter++;
        oCounter--;

        sem_post(&outputLock);
    }
    // pthread_exit(0);
}

//method thread to write character to output file
//once character is written, signals encrypt thread that the output buffer is ready to 
//receive new characters
void *writeFile(void *param) 
{
    writer = 0;

    while(1) 
    {
        sem_wait(&outputLock);
		
        if(oCounter == 0 && isDone) 
        {
            sem_post(&outputLock);
            break;
        }

        write_output(outbuffer[writer % out]);

        writer++;
        outputData--;

        sem_post(&writesem);
        sem_post(&outputLock);
    }
    // pthread_exit(0);
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

    int insize;
    int outsize;

    //prompt user for input buffer size
    printf("please give input buffer size. ");
    scanf("%d", &insize);
    // printf("\n");
    if (insize <= 1)
    {
        printf("input buffer needs to be greater than 1\n");
        exit(0);
    }

    //prompt user for output buffer size
    printf("please give output buffer size. ");
    scanf("%d", &outsize);
    // printf("\n");
    if (outsize <= 1)
    {
        printf("output buffer needs to be greater than 1\n");
        exit(0);
    }

    inbuffer = (char*) malloc(sizeof(char) * in);
    outbuffer = (char*) malloc(sizeof(char) * out);

    printf("allocate memory success\n");

    reader = 0;
    incounter = 0;
    encryptincounter = 0;
    encryptoutcounter = 0;
    outcounter = 0;
    writer = 0;
    resetting = 0;
    inputData = 0;
    outputData = 0;
    iCounter = 0;
    oCounter = 0;
    isDone = false;

    printf("initialize variables success\n");
    
    pthread_t reader;
	pthread_t inputCounter;
	pthread_t encryptor;
	pthread_t outputCounter;
	pthread_t writer;

    printf("declare pthread success\n");

    in = insize;
    out = outsize;

    printf("setting size success\n");
    
    //initialize semaphores
    sem_init(&readsem, 0, in);
    sem_init(&writesem, 0, out);
    sem_init(&inputLock, 0, 1);
    sem_init(&outputLock, 0, 1);
    sem_init(&reset, 0, 0);

    printf("initialize sem success\n");

	pthread_create(&reader, NULL, readFile, NULL);

    printf("test1");

    pthread_create(&inputCounter, NULL, countInBuffer, NULL);
    
    printf("test2");
    
    pthread_create(&encryptor, NULL, encryptFile, NULL);
    
    printf("test3");
    
    pthread_create(&outputCounter, NULL, countOutBuffer, NULL);
    
    printf("test4");
    
    pthread_create(&writer, NULL, writeFile, NULL);
    
    printf("test5");
    

    printf("create pthread success\n");

	pthread_join(reader, NULL);
    pthread_join(inputCounter, NULL);
    pthread_join(encryptor, NULL);
    pthread_join(outputCounter, NULL);
    pthread_join(writer, NULL);

    printf("pthread join success\n");

	sem_destroy(&readsem);
    sem_destroy(&writesem);
    sem_destroy(&inputLock);
    sem_destroy(&outputLock);
    sem_destroy(&reset);

    printf("sem destroy success\n");
	
	log_counts();
    printf("End of file reached."); 
    printf("\n");

    //freeing memory
	free(inbuffer);
    free(outbuffer);
}