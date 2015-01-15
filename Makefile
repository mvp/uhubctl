CFLAGS = -g -O0
LDFLAGS = -lusb-1.0
PROGRAM = uhubctl

$(PROGRAM): $(PROGRAM).o
	cc $(CFLAGS) $@.c -o $@ $(LDFLAGS)

clean:
	rm -rf $(PROGRAM).o $(PROGRAM).dSYM $(PROGRAM)
