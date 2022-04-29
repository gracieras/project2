encrypt-driver: encrypt-driver.o encrypt-module.o
		gcc -o encrypt-driver encrypt-driver.c encrypt-module.c -lpthread

encrypt-driver.o: encrypt-driver.c 
		gcc -Wall -g -c encrypt-driver.c -lpthread

encrypt-module.o: encrypt-module.c encrypt-module.h
		gcc -Wall -g -c encrypt-module.c -lpthread		

clean:
		rm -f encrypt-driver.o encrypt-module.o encrypt-driver output.txt