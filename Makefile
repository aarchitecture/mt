CC = c99

mt: mt.c
	$(CC) -o $@ $^

test: mt
	./test.sh

.PHONY: clean test
clean:
	rm -f mt
