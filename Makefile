
encrypt: encrypt.o encrypt-module.o
		gcc -o encrypt encrypt.c encrypt-module.c -lpthread

encrypt.o: encrypt352.c 
		gcc -Wall -g -c encrypt.c -lpthread

encrypt-module.o: encrypt-module.c encrypt-module.h
		gcc -Wall -g -c encrypt-module.c -lpthread		

clean:
		rm -f encrypt.o encrypt-module.o encrypt output.txt