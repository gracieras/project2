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
<<<<<<< Updated upstream
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

=======
    while((c = read_input()) != EOF){
        //Checks if a reset has occurred
        if(reset_flag == 1)
            sem_wait(&reset_sem);
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
        iCounter++;
        //release lock on input buffer
>>>>>>> Stashed changes
        sem_post(&inputLock);
    }

    // inbuffer[reader % in] = EOF;
    // sem_post(&countinsem);  

    isDone = true;  
    // pthread_exit(0);
}

<<<<<<< Updated upstream
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

=======
/*
The input counter thread simply counts occurrences of each letter in the input file by looking at 
each character in the input buffer
*/
void *inCounterThread(){
    //The counting index in the input buffer
    int currentIndex = 0;
    while(1){
        //grabs the lock on input buffer
        sem_wait(&inputLock);
        //Checks if there are items ready to counted
        if(iCounter == 0){
            if(read_done_flag == 1){
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
            iCounter--;
            //releases the lock on the input buffer
>>>>>>> Stashed changes
            sem_post(&inputLock);
        }
    }
}

<<<<<<< Updated upstream
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
=======
/*
The encryption thread consumes one character at a time from the input buffer, encrypts it, and 
places it in the output buffer. It must do so by calling the provided function encrypt().
*/
void *encryptThread(){
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
            if(read_done_flag == 1){
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
            oCounter++;
            //signal freed space in input buffer
            sem_post(&readsem);
            //unlock buffers
>>>>>>> Stashed changes
            sem_post(&inputLock);
            sem_post(&outputLock);
        }
        
    }
    // pthread_exit(0);
}
<<<<<<< Updated upstream

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

=======
/*
The output counter thread simply counts occurrences of each letter in the output file by looking 
at each character in the output buffer. It must call the provided function count_output(). 
*/
void *outCounterThread(){
    //The counting index of the output buffer
    int currentIndex = 0;
    while(1){
        //lock output buffer
        sem_wait(&outputLock);
        //Checks if there are items ready to counted
        if(oCounter == 0){
            if(read_done_flag == 1){
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
            oCounter--;
            //unlock output buffer
>>>>>>> Stashed changes
            sem_post(&outputLock);
        }
        
    }
    // pthread_exit(0);
}
<<<<<<< Updated upstream

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
=======
/*
The writer thread is responsible for writing the encrypted characters in the output buffer to the 
output file. It must do so by calling the provided function write_output(). 
*/
void *writeThread(){
    //The index at which the output buffer can read
    int currentIndex = 0;
    while(1){
        //lock output buffer
        sem_wait(&outputLock);
        //Checks if the output buffer is empty
        if(outputData == 0){
            if(read_done_flag == 1){
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
>>>>>>> Stashed changes
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