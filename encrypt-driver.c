#include <stdio.h>
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
    int ibuffer;
    scanf("%d", &ibuffer);
    if (ibuffer <= 1)
    {
        printf("input buffer needs to be greater than 1");
        exit();
    }

    printf("please give output buffer size.");
    int obuffer;
    scanf("%d", &obuffer);
    if (obuffer <= 1)
    {
        printf("output buffer needs to be greater than 1");
        exit();
    }

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
