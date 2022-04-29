#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "encrypt-module.h"

void reset_requested() 
{
	log_counts();
}

void reset_finished() 
{
	
}

int main(int argc, char *argv[]) 
{
    char c;
    FILE *finput, *foutput, *flog;
    if (argc == 3)
    {
        finput = fopen(argv[1], "w");
        foutput = fopen(argv[2], "w");
        flog = fopen(argv[3], "w");
    }
    else
    {
        printf("please include input file name, output name, and log filename");
        exit();
    }

	init(finput, foutput, flog); 

    printf("please give input buffer size.");
    int ibuffersize;
    scanf("%d", &ibuffersize);
    if (ibuffersize <= 1)
    {
        printf("input buffer needs to be greater than 1");
        exit();
    }

    printf("please give output buffer size.");
    int obuffersize;
    scanf("%d", &obuffersize);
    if (obuffersize <= 1)
    {
        printf("output buffer needs to be greater than 1");
        exit();
    }
    
    //circular buffer stuff
    uint8_t * ibuffer  = malloc(ibuffersize * sizeof(uint8_t));
    
    //input buffer
    cbuf_handle_t ime = circular_buf_init(ibuffer, ibuffersize);

    uint8_t * obuffer  = malloc(obuffersize * sizeof(uint8_t));

    //output buffer
    cbuf_handle_t ome = circular_buf_init(obuffer, obuffersize);



	char c;
	while ((c = read_input()) != EOF) { 
		count_input(c); 
		c = encrypt(c); 
		count_output(c); 
		write_output(c); 
	} 
	printf("End of file reached.\n"); 
	log_counts();
}

typedef struct circular_buf_t circular_buf_t;
typedef circular_buf_t* cbuf_handle_t;

/// Pass in a storage buffer and size 
/// Returns a circular buffer handle
cbuf_handle_t circular_buf_init(uint8_t* buffer, size_t size);

/// Free a circular buffer structure.
/// Does not free data buffer; owner is responsible for that
void circular_buf_free(cbuf_handle_t me);

/// Reset the circular buffer to empty, head == tail
void circular_buf_reset(cbuf_handle_t me);

/// Put version 1 continues to add data if the buffer is full
/// Old data is overwritten
void circular_buf_put(cbuf_handle_t me, uint8_t data);

/// Put Version 2 rejects new data if the buffer is full
/// Returns 0 on success, -1 if buffer is full
int circular_buf_put2(cbuf_handle_t me, uint8_t data);

/// Retrieve a value from the buffer
/// Returns 0 on success, -1 if the buffer is empty
int circular_buf_get(cbuf_handle_t me, uint8_t * data);

/// Returns true if the buffer is empty
bool circular_buf_empty(cbuf_handle_t me);

/// Returns true if the buffer is full
bool circular_buf_full(cbuf_handle_t me);

/// Returns the maximum capacity of the buffer
size_t circular_buf_capacity(cbuf_handle_t me);

/// Returns the current number of elements in the buffer
size_t circular_buf_size(cbuf_handle_t me);

// The hidden definition of our circular buffer structure
struct circular_buf_t {
	uint8_t * buffer;
	size_t head;
	size_t tail;
	size_t max; //of the buffer
	bool full;
};

// // User provides struct
// void circular_buf_init(circular_buf_t* me, uint8_t* buffer, size_t size);

// // Return a concrete struct
// circular_buf_t circular_buf_init(uint8_t* buffer, size_t size);

// // Return a pointer to a struct instance - preferred approach
// cbuf_handle_t circular_buf_init(uint8_t* buffer, size_t size);

cbuf_handle_t circular_buf_init(uint8_t* buffer, size_t size)
{
	assert(buffer && size);

	cbuf_handle_t cbuf = malloc(sizeof(circular_buf_t));
	assert(cbuf);

	cbuf->buffer = buffer;
	cbuf->max = size;
	circular_buf_reset(cbuf);

	assert(circular_buf_empty(cbuf));

	return cbuf;
}

void circular_buf_reset(cbuf_handle_t me)
{
    assert(me);

    me->head = 0;
    me->tail = 0;
    me->full = false;
}

void circular_buf_free(cbuf_handle_t me)
{
	assert(me);
	free(me);
}

bool circular_buf_full(cbuf_handle_t me)
{
	assert(me);

	return me->full;
}

bool circular_buf_empty(cbuf_handle_t me)
{
	assert(me);

	return (!me->full && (me->head == me->tail));
}

size_t circular_buf_capacity(cbuf_handle_t me)
{
	assert(me);

	return me->max;
}

size_t circular_buf_size(cbuf_handle_t me)
{
	assert(me);

	size_t size = me->max;

	if(!me->full)
	{
		if(me->head >= me->tail)
		{
			size = (me->head - me->tail);
		}
		else
		{
			size = (me->max + me->head - me->tail);
		}
	}

	return size;
}

static void advance_pointer(cbuf_handle_t me)
{
	assert(me);

	if(me->full)
   	{
		if (++(me->tail) == me->max) 
		{ 
			me->tail = 0;
		}
	}

	if (++(me->head) == me->max) 
	{ 
		me->head = 0;
	}
	me->full = (me->head == me->tail);
}

static void retreat_pointer(cbuf_handle_t me)
{
	assert(me);

	me->full = false;
	if (++(me->tail) == me->max) 
	{ 
		me->tail = 0;
	}
}

void circular_buf_put(cbuf_handle_t me, uint8_t data)
{
	assert(me && me->buffer);

    me->buffer[me->head] = data;

    advance_pointer(me);
}

int circular_buf_get(cbuf_handle_t me, uint8_t * data)
{
    assert(me && data && me->buffer);

    int r = -1;

    if(!circular_buf_empty(me))
    {
        *data = me->buffer[me->tail];
        retreat_pointer(me);

        r = 0;
    }

    return r;
}