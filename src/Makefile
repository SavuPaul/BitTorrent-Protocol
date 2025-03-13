CFLAGS = -pthread -Wall

build: tema2

tema2: tema2.c helpers.c
	mpicc -o tema2 tema2.c helpers.c $(CFLAGS)

clean:
	rm -rf tema2

