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

sem_t readsem;
sem_t countinsem;
sem_t encryptinsem;
sem_t encryptoutsem;
sem_t countoutsem;
sem_t writesem;

int resetting;

//counters for read/write and counters for buffers
int reader, incounter, encryptincounter, encryptoutcounter, outcounter, writer;

int in,out; //size of in/out buffers

// //circular buffer stuff for creating the buffer later
// uint8_t * ibuffer;
// uint8_t * obuffer;

// //input buffer
// cbuf_handle_t ime;

// //output buffer
// cbuf_handle_t ome;

//TODO
void reset_requested() 
{
    for (int i = 0; i < get_input_total_count(); i++)
    {
        inbuffer[i] = 0;
    }
    for (int i = 0; i < get_output_total_count(); i++)
    {
        outbuffer[i] = 0;
    }
	log_counts();
    reset_finished();
}
//TODO
void reset_finished() 
{
	resetting = 0;
}

//thread method to read each character in the input file as they buffer is ready to receive them,
//calls read_input from encrypt-module.h to iterate thorugh file and places each character in the inbuffer.
//signals that characters are ready to be counted
void *readFile(void *param) {

    int c;
    reader = 0;
    while ((c = read_input()) != EOF) {
        srand(time(0));   //random seed for this thread
        int r = rand();
        if (r % 2 == 0)
        {
            resetting = 1;
        }
        while(resetting == 1) {
            reset_requested();
            //TODO have a way to set resetting to 1
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
        srand(time(0));   //random seed for this thread
        int r = rand();
        if (r % 2 == 0)
        {
            resetting = 1;
        }
        while(resetting == 1) {
            reset_requested();
            //TODO have a way to set resetting to 1
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
        srand(time(0));   //random seed for this thread
        int r = rand();
        if (r % 2 == 0)
        {
            resetting = 1;
        }
        while(resetting == 1) {
            reset_requested();
            //TODO have a way to set resetting to 1
        }
        sem_wait(&encryptinsem);

        if(inbuffer[encryptincounter] == EOF) {
            outbuffer[encryptoutcounter] = EOF;
            sem_post(&countoutsem); //potential error
            break;
        }
        sem_wait(&encryptoutsem);
        outbuffer[encryptoutcounter] = caesar_encrypt(inbuffer[encryptincounter]);
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
        srand(time(0));   //random seed for this thread
        int r = rand();
        if (r % 2 == 0)
        {
            resetting = 1;
        }
        while(resetting == 1) {
            reset_requested();
            //TODO have a way to set resetting to 1
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
        srand(time(0));   //random seed for this thread
        int r = rand();
        if (r % 2 == 0)
        {
            resetting = 1;
        }
        while(resetting == 1) {
            reset_requested();
            //TODO have a way to set resetting to 1

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
    //obtaining file name
    char *finput, *foutput, *flog;
    srand(time(0));   //random seed
    if (argc == 3)
    {
        finput = argv[1];
        foutput = argv[2];
        flog = argv[3];
        // finput = fopen(argv[1], "r");
        // foutput = fopen(argv[2], "r");
        // flog = fopen(argv[3], "r");
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
    // int ibuffersize; //max size
    // scanf("%d", &ibuffersize);
    scanf("%d", &in);
    // if (ibuffersize <= 1)
    if (in <= 1)
    {
        printf("input buffer needs to be greater than 1");
        exit(0);
    }

    // ibuffer  = malloc(ibuffersize * sizeof(uint8_t));
    // ime = circular_buf_init(ibuffer, ibuffersize); //creating input circular buffer

    //prompt user for output buffer size
    printf("please give output buffer size.");
    // int obuffersize; //max size
    // scanf("%d", &obuffersize);
    scanf("%d", &out);
    if (out <= 1)
    {
        printf("output buffer needs to be greater than 1");
        exit(0);
    }

    // obuffer  = malloc(obuffersize * sizeof(uint8_t));
    // ome = circular_buf_init(obuffer, obuffersize); //creating output circular buffer

    //creating threads
	pthread_t reader;
	pthread_t inputCounter;
	pthread_t encryptor;
	pthread_t outputCounter;
	pthread_t writer;
	pthread_attr_t attr;

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
    char c;
	while ((c = read_input()) != EOF) 
    { 
		count_input(c); 
		c = encrypt(c); 
		count_output(c); 
		write_output(c); 
	} 
	printf("End of file reached.\n"); 
	log_counts();

    //freeing memory
	free(inbuffer);
    free(outbuffer);
    // free(ibuffer);
    // free(obuffer);
    // circular_buf_free(ime);
    // circular_buf_free(ome);
    fclose(finput);
    fclose(foutput);
    fclose(flog);
}

// typedef struct circular_buf_t circular_buf_t;
// typedef circular_buf_t* cbuf_handle_t;

// /// Pass in a storage buffer and size 
// /// Returns a circular buffer handle
// cbuf_handle_t circular_buf_init(uint8_t* buffer, size_t size);

// /// Free a circular buffer structure.
// /// Does not free data buffer; owner is responsible for that
// void circular_buf_free(cbuf_handle_t me);

// /// Reset the circular buffer to empty, head == tail
// void circular_buf_reset(cbuf_handle_t me);

// /// Put version 1 continues to add data if the buffer is full
// /// Old data is overwritten
// void circular_buf_put(cbuf_handle_t me, uint8_t data);

// /// Retrieve a value from the buffer
// /// Returns 0 on success, -1 if the buffer is empty
// int circular_buf_get(cbuf_handle_t me, uint8_t * data);

// /// Returns true if the buffer is empty
// bool circular_buf_empty(cbuf_handle_t me);

// /// Returns true if the buffer is full
// bool circular_buf_full(cbuf_handle_t me);

// /// Returns the maximum capacity of the buffer
// size_t circular_buf_capacity(cbuf_handle_t me);

// /// Returns the current number of elements in the buffer
// size_t circular_buf_size(cbuf_handle_t me);

// // The hidden definition of our circular buffer structure
// struct circular_buf_t {
// 	uint8_t * buffer;
// 	size_t head;
// 	size_t tail;
// 	size_t max; //of the buffer
// 	bool full;
// };

// // // User provides struct
// // void circular_buf_init(circular_buf_t* me, uint8_t* buffer, size_t size);

// // // Return a concrete struct
// // circular_buf_t circular_buf_init(uint8_t* buffer, size_t size);

// // // Return a pointer to a struct instance - preferred approach
// // cbuf_handle_t circular_buf_init(uint8_t* buffer, size_t size);

// cbuf_handle_t circular_buf_init(uint8_t* buffer, size_t size)
// {
// 	assert(buffer && size);

// 	cbuf_handle_t cbuf = malloc(sizeof(circular_buf_t));
// 	assert(cbuf);

// 	cbuf->buffer = buffer;
// 	cbuf->max = size;
// 	circular_buf_reset(cbuf);

// 	assert(circular_buf_empty(cbuf));

// 	return cbuf;
// }

// void circular_buf_reset(cbuf_handle_t me)
// {
//     assert(me);

//     me->head = 0;
//     me->tail = 0;
//     me->full = false;
// }

// void circular_buf_free(cbuf_handle_t me)
// {
// 	assert(me);
// 	free(me);
// }

// bool circular_buf_full(cbuf_handle_t me)
// {
// 	assert(me);

// 	return me->full;
// }

// bool circular_buf_empty(cbuf_handle_t me)
// {
// 	assert(me);

// 	return (!me->full && (me->head == me->tail));
// }

// size_t circular_buf_capacity(cbuf_handle_t me)
// {
// 	assert(me);

// 	return me->max;
// }

// size_t circular_buf_size(cbuf_handle_t me)
// {
// 	assert(me);

// 	size_t size = me->max;

// 	if(!me->full)
// 	{
// 		if(me->head >= me->tail)
// 		{
// 			size = (me->head - me->tail);
// 		}
// 		else
// 		{
// 			size = (me->max + me->head - me->tail);
// 		}
// 	}

// 	return size;
// }

// static void advance_pointer(cbuf_handle_t me)
// {
// 	assert(me);

// 	if(me->full)
//    	{
// 		if (++(me->tail) == me->max) 
// 		{ 
// 			me->tail = 0;
// 		}
// 	}

// 	if (++(me->head) == me->max) 
// 	{ 
// 		me->head = 0;
// 	}
// 	me->full = (me->head == me->tail);
// }

// static void retreat_pointer(cbuf_handle_t me)
// {
// 	assert(me);

// 	me->full = false;
// 	if (++(me->tail) == me->max) 
// 	{ 
// 		me->tail = 0;
// 	}
// }

// void circular_buf_put(cbuf_handle_t me, uint8_t data)
// {
// 	assert(me && me->buffer);

//     me->buffer[me->head] = data;

//     advance_pointer(me);
// }

// int circular_buf_get(cbuf_handle_t me, uint8_t * data)
// {
//     assert(me && data && me->buffer);

//     int r = -1;

//     if(!circular_buf_empty(me))
//     {
//         *data = me->buffer[me->tail];
//         retreat_pointer(me);

//         r = 0;
//     }

//     return r;
// }