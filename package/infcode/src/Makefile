# build testio executable when user executes "make"

infcode: infcode.o
	$(CC) $(LDFLAGS) infcode.o -o infcode

infcode.o: infcode.c
	$(CC) $(CFLAGS) -c infcode.c

# remove object files and executable when user executes "make clean"
clean:
	rm *.o infcode
