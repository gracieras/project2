#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include "encrypt-module.h"

//Authors - Felipe Bautista Salamanca and Emmanuel Paz

/*
 *Input Buffer Data Structure
 */
char *input_buffer;
int input_buffer_size;
sem_t lock_InputBuffer_sem;
sem_t space_inside_inputBuffer;
int stuff_inside_InBuffer;
int chars_to_count_input;
/*
 *Output Buffer Data Structure 
 */
char *output_buffer;
int output_buffer_size;
sem_t space_inside_outputBuffer;
sem_t lock_OutputBuffer_sem;
int stuff_inside_OutBuffer;
int chars_to_count_output;

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
        sem_wait(&lock_InputBuffer_sem);
        sem_wait(&lock_OutputBuffer_sem);
        if(stuff_inside_OutBuffer == 0 && stuff_inside_InBuffer == 0 && chars_to_count_input == 0 && chars_to_count_output == 0) {
            sem_post(&lock_InputBuffer_sem);
            sem_post(&lock_OutputBuffer_sem);

            break;
        }
        sem_post(&lock_InputBuffer_sem);
        sem_post(&lock_OutputBuffer_sem);
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

/*
The reader thread is responsible for reading from the input file one character at a time and 
placing the characters in the input buffer. It must do so by calling the provided function 
read_input(). Each buffer item corresponds to a character.
*/
void *inputThread(){
    //The index at which the input buffer can write
    int currentIndex = 0;
    char c;
    while((c = read_input()) != EOF){
        //Checks if a reset has occurred
        if(reset_flag == 1)
            sem_wait(&reset_sem);
        //checks if there is space inside input Buffer
        sem_wait(&space_inside_inputBuffer);
        //locks the input buffer
        sem_wait(&lock_InputBuffer_sem);
        //Stores the read character at the read index in the input buffer
        *(input_buffer + (currentIndex % input_buffer_size)) = c;
        //increment the read index
        currentIndex++;
        //increase the size of the input buffer
        stuff_inside_InBuffer++;
        //increments the number of character ready to be counted
        chars_to_count_input++;
        //release lock on input buffer
        sem_post(&lock_InputBuffer_sem);
    }
    read_done_flag = 1;
}

/*
The input counter thread simply counts occurrences of each letter in the input file by looking at 
each character in the input buffer
*/
void *inCounterThread(){
    //The counting index in the input buffer
    int currentIndex = 0;
    while(1){
        //grabs the lock on input buffer
        sem_wait(&lock_InputBuffer_sem);
        //Checks if there are items ready to counted
        if(chars_to_count_input == 0){
            if(read_done_flag == 1){
                sem_post(&lock_InputBuffer_sem);
                break;
            }
            sem_post(&lock_InputBuffer_sem);
        }
        else {
            //Check of the character to be counted is not the ending character
            count_input(*(input_buffer + (currentIndex % input_buffer_size)));
            //increments the counting index
            currentIndex++;
            //decreases the items to be counted 
            chars_to_count_input--;
            //releases the lock on the input buffer
            sem_post(&lock_InputBuffer_sem);
        }
    }
}

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
        sem_wait(&space_inside_outputBuffer);
        //lock both buffers
        sem_wait(&lock_InputBuffer_sem);
        sem_wait(&lock_OutputBuffer_sem);
        if(stuff_inside_InBuffer == 0){
            if(read_done_flag == 1){
                sem_post(&lock_InputBuffer_sem);
                sem_post(&lock_OutputBuffer_sem);
                sem_post(&space_inside_outputBuffer);
                break;
            }
            sem_post(&lock_InputBuffer_sem);
            sem_post(&lock_OutputBuffer_sem);
            sem_post(&space_inside_outputBuffer);
        }
        else {
            //checks if the character ready to be dequeue is not the ending character
            /*if (*(input_buffer + (currentIndex_in % input_buffer_size)) != EOF) {
                *(output_buffer + (currentIndex_out % output_buffer_size)) = encrypt(*(input_buffer + (currentIndex_in % input_buffer_size)));
            } else
                *(output_buffer + (currentIndex_out % output_buffer_size)) = *(input_buffer + (currentIndex_in % input_buffer_size));
            */
            *(output_buffer + (currentIndex_out % output_buffer_size)) = encrypt(*(input_buffer + (currentIndex_in % input_buffer_size)));
            
            //Incrementes Read index in the input buffer
            currentIndex_in++;
            //Increment the Write index in the output buffer
            currentIndex_out++;

            //indicate removal of things in input buffer
            stuff_inside_InBuffer--;
            //indicate stuff inside output buffer
            stuff_inside_OutBuffer++;
            chars_to_count_output++;
            //signal freed space in input buffer
            sem_post(&space_inside_inputBuffer);
            //unlock buffers
            sem_post(&lock_InputBuffer_sem);
            sem_post(&lock_OutputBuffer_sem);
        }
    }
}
/*
The output counter thread simply counts occurrences of each letter in the output file by looking 
at each character in the output buffer. It must call the provided function count_output(). 
*/
void *outCounterThread(){
    //The counting index of the output buffer
    int currentIndex = 0;
    while(1){
        //lock output buffer
        sem_wait(&lock_OutputBuffer_sem);
        //Checks if there are items ready to counted
        if(chars_to_count_output == 0){
            if(read_done_flag == 1){
                sem_post(&lock_OutputBuffer_sem);
                break;
            }
            sem_post(&lock_OutputBuffer_sem);
        }
        else {
            //Check of the character to be counted is not the ending character

            count_output(*(output_buffer + (currentIndex % output_buffer_size)));
            //increments the counting index
            currentIndex++;
            //decreases the items to be counted 
            chars_to_count_output--;
            //unlock output buffer
            sem_post(&lock_OutputBuffer_sem);
        }
    }
}
/*
The writer thread is responsible for writing the encrypted characters in the output buffer to the 
output file. It must do so by calling the provided function write_output(). 
*/
void *writeThread(){
    //The index at which the output buffer can read
    int currentIndex = 0;
    while(1){
        //lock output buffer
        sem_wait(&lock_OutputBuffer_sem);
        //Checks if the output buffer is empty
        if(stuff_inside_OutBuffer == 0){
            if(read_done_flag == 1){
                sem_post(&lock_OutputBuffer_sem);
                break;
            }
            sem_post(&lock_OutputBuffer_sem);
        }
        else {
            write_output(*(output_buffer + (currentIndex % output_buffer_size)));
            //Increments the count index
            currentIndex++;
            //Decreases the number of items to be counted
            stuff_inside_OutBuffer--;
            //indicate space available in output buffer
            sem_post(&space_inside_outputBuffer);
            //release lock on output buffer
            sem_post(&lock_OutputBuffer_sem);
        }
    }
}

int main(int argc, char *argv[]) {
    int input, output;
    if(argc < 4){
        printf("arguments should be: inputfile outputfile logfile\n");
        return 1;
    }

    init(argv[1], argv[2], argv[3]);

    printf("input buffer size? ");
    scanf(" %d", &input);
    printf("output buffer size? ");
    scanf(" %d", &output);
    //flags
    read_done_flag = 0;
    reset_flag = 0;
    //initialize for buffers
    stuff_inside_InBuffer = 0;
    stuff_inside_OutBuffer = 0;
    chars_to_count_input = 0;
    chars_to_count_output = 0;
    //threads
    pthread_t readT;
    pthread_t input_countT;
    pthread_t encryptT;
    pthread_t output_countT;
    pthread_t writeT;
    //buffer sizes
    input_buffer_size = input;
    output_buffer_size = output;
    //init buffers
    input_buffer = (char*) malloc(input * sizeof(char));
    output_buffer = (char*) malloc(input * sizeof(char));
    /*
     * semaphores to make sure we don't exceed space available in buffers
     */
    sem_init(&space_inside_inputBuffer, 0, input);
    sem_init(&space_inside_outputBuffer, 0, output);
    /*
     * locking buffers to avoid race condition
     */
    sem_init(&lock_InputBuffer_sem, 0, 1);
    sem_init(&lock_OutputBuffer_sem, 0 , 1);
    /*
     * if a reset is requested then reading thread should wait
     */
    sem_init(&reset_sem, 0, 0);

    pthread_create(&readT, NULL, inputThread, NULL);
    pthread_create(&input_countT, NULL, inCounterThread, NULL);
    pthread_create(&encryptT, NULL, encryptThread, NULL);
    pthread_create(&output_countT, NULL, outCounterThread, NULL);
    pthread_create(&writeT, NULL, writeThread, NULL);

    pthread_join(readT, NULL);
    pthread_join(input_countT, NULL);
    pthread_join(encryptT, NULL);
    pthread_join(output_countT, NULL);
    pthread_join(writeT, NULL);

    printf("End of file reached.\n");
    log_counts();

    free(input_buffer);
    free(output_buffer);

    return 0;
}