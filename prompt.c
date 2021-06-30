#include <stdio.h>

// Declare a buffer for user input
static char input[2048];

int main(int argc, char** argv) {
	
	// Print version and exit information
	puts("Lispy version 0.0.0.0.1");
	puts("Press Ctrl+C to exit\n");
	
	// Endlessly prompt for input and reply back
	while (1) {
		fputs("lsp>", stdout);
		fgets(input, 2048, stdin);
		printf("No you're a %s", input);
	}
	
	return 0;
	
}
