all:	main.c
		gcc -o main main.c -lpthread

clean:
		rm main *.o
