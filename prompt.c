#include <stdio.h>
#include <stdlib.h>

#include <editline/readline.h>
#include <editline/history.h>

int main(int argc, char** argv) {
	
	// Print version and exit information
	puts("Lispy version 0.0.0.0.1");
	puts("Press Ctrl+C to exit\n");
	
	// Endlessly prompt for input and reply back
	while (1) {
		char* input = readline("lsp> ");
		add_history(input);
		printf("No you're a %s\n", input);
		free(input);
	}
	
	return 0;
	
}
