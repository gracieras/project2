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

/*
 *Input Buffer Data Structure
 */
char *inbuffer;
int in;
sem_t inputLock;
sem_t readsem;
int inputData;
int iCounter;
/*
 *Output Buffer Data Structure 
 */
char *outbuffer;
int out;
sem_t writesem;
sem_t outputLock;
int outputData;
int oCounter;

//Reset variables
sem_t reset_sem;
int reset_flag;

//flags finished
int read_done_flag;

/*
When this method get called by the random reset Thread, it stop the reader thread from reading any more input and make 
sure it is safe to reset after all the items in the buffer have been counted and process
*/

void reset_requested() {
    //stops the Reader
    reset_flag = 1;
    //waits until it safe to reset
    while (1){
        sem_wait(&inputLock);
        sem_wait(&outputLock);
        if(outputData == 0 && inputData == 0 && iCounter == 0 && oCounter == 0) {
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
/*
Resumes the reader thread
*/
void reset_finished() {
    //signals the reader to continue
    sem_post(&reset_sem);
    reset_flag = 0;
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

        // inbuffer[reader % in] = c;
        int tempmod = reader % in;
        *(inbuffer + tempmod) = c;
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
void *countInBuffer() 
{
    incounter = 0;

    while (1) 
    {
        sem_wait(&inputLock);
        
        if(iCounter == 0) 
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
            // count_input(inbuffer[incounter % in]);
            int tempmod = incounter % in;
            count_input(*(inbuffer + tempmod));

            incounter++;
            iCounter--;

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
    encryptincounter = 0;
    encryptoutcounter = 0;

    while(1) 
    {
        sem_wait(&writesem);
        sem_wait(&inputLock);
        sem_wait(&outputLock);

        if(iCounter == 0) 
        {
            if (isDone)
            {
                // outbuffer[encryptoutcounter] = EOF;
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
            int tempmodin = encryptincounter % in;
            int tempmodout = encryptoutcounter % out;
            *(outbuffer + tempmodout) = encrypt(*(inbuffer + tempmodin));
            // outbuffer[encryptoutcounter % out] = encrypt(inbuffer[encryptincounter % in]);

            inputData--;
            outputData++;
            oCounter++;

            encryptincounter++;
            encryptoutcounter++;
            
            sem_post(&readsem);
            sem_post(&inputLock);
            sem_post(&outputLock);
        }
        
    }
    // pthread_exit(0);
}

//method thread to count total and count each character in the outbuffer
//once counted, signals that the character is ready to be written to output file
void *countOutBuffer() 
{
    outcounter = 0;

    while(1) 
    {
        sem_wait(&outputLock);
        
        if(oCounter == 0) 
        {
            if(isDone)
            {
                sem_post(&outputLock);
                break;
            }
            
        }
        else
        {
            int tempmod = outcounter % out;
            count_output(*(outbuffer + tempmod));

            outcounter++;
            oCounter--;

            sem_post(&outputLock);
        }
        
    }
    // pthread_exit(0);
}

//method thread to write character to output file
//once character is written, signals encrypt thread that the output buffer is ready to 
//receive new characters
void *writeFile() 
{
    writer = 0;

    while(1) 
    {
        sem_wait(&outputLock);
		
        if(oCounter == 0) 
        {
            if(isDone)
            {
                sem_post(&outputLock);
                break;
            }
        }
        else
        {
            int tempmod = writer % out;
            write_output(*(outbuffer + tempmod));
            // write_output(outbuffer[writer % out]);

            writer++;
            outputData--;

            sem_post(&writesem);
            sem_post(&outputLock);
        }
    }
}

int main(int argc, char *argv[]) {
    int input, output;
    
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
    scanf("%d", &input);
    if (input <= 1)
    {
        printf("input buffer needs to be greater than 1");
        exit(0);
    }

    //prompt user for output buffer size
    printf("What output buffer size to use? ");
    scanf("%d", &output);
    if (output <= 1)
    {
        printf("output buffer needs to be greater than 1");
        exit(0);
    }

    //flags
    read_done_flag = 0;
    reset_flag = 0;
    //initialize for buffers
    inputData = 0;
    outputData = 0;
    iCounter = 0;
    oCounter = 0;
    //threads
    pthread_t readT;
    pthread_t input_countT;
    pthread_t encryptT;
    pthread_t output_countT;
    pthread_t writeT;
    //buffer sizes
    in = input;
    out = output;
    //init buffers
    inbuffer = (char*) malloc(input * sizeof(char));
    outbuffer = (char*) malloc(input * sizeof(char));

    sem_init(&readsem, 0, input);
    sem_init(&writesem, 0, output);
    sem_init(&inputLock, 0, 1);
    sem_init(&outputLock, 0 , 1);
    sem_init(&reset_sem, 0, 0);

    pthread_create(&readT, NULL, readFile, NULL);
    pthread_create(&input_countT, NULL, countInBuffer, NULL);
    pthread_create(&encryptT, NULL, encryptFile, NULL);
    pthread_create(&output_countT, NULL, countOutBuffer, NULL);
    pthread_create(&writeT, NULL, writeFile, NULL);

    pthread_join(readT, NULL);
    pthread_join(input_countT, NULL);
    pthread_join(encryptT, NULL);
    pthread_join(output_countT, NULL);
    pthread_join(writeT, NULL);

    printf("End of file reached.\n");
    log_counts();

    free(inbuffer);
    free(outbuffer);

    return 0;
}