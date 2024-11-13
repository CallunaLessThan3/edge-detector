CC     = gcc
CFLAGS =
DEPS   = #.h
OBJ    = edge_detector.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

edge_detector: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

clean:
	rm -rf *.o main