OBJS = lab08.o 
PROG = lab08

%.o : %.c
	gcc -c -g -o $@ $<

$(PROG) : $(OBJS)
	gcc -g -o $@ $^

clean:
	rm -rf $(OBJS) $(PROG)
