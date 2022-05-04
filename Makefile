encrypt:
		gcc test2-encrypt-driver.c encrypt-module.c -lpthread -o encrypt

clean:
		rm -f encrypt-driver.o encrypt-module.o encrypt out.txt log.txt