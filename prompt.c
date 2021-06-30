#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32 // Windows requires no libedit, instead we provide fake substitutes
#include <string.h>

static char buffer[2048];

char* readline(char* prompt) {
	// Output prompt
	fputs(prompt, stdout);
	
	// Read the user input into the buffer
	fgets(buffer, 2048, stdin);
	
	// Copy the buffer and null-terminate it to make it into a string
	char* cpy = malloc(strlen(buffer)+1);
	strcpy(cpy, buffer);
	cpy[strlen(cpy)-1] = '\0';
	
	return cpy;
}

void add_history(char* unused) {}

#else // Linux & Mac require libedit for line edition and history on input
#include <editline/readline.h>
#include <editline/history.h>
#endif


int main(int argc, char** argv) {
	
	// Print version and exit information
	puts("Lsp version 0.0.0.0.1");
	puts("Ctrl+C to exit\n");
	
	// Endlessly prompt for input and reply back
	while (1) {
		char* input = readline("lsp> ");
		add_history(input);
		printf("Simon says: \"%s\"\n", input);
		free(input);
	}
	
	return 0;
	
}
