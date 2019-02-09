all: bin shell

bin:
	mkdir bin

shell: ./src/shell.c
	gcc ./src/shell.c -o ./bin/shell
