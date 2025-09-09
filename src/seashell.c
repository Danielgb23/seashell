# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include "md4c.h"

# include <ncurses.h>


int read_file(const char *filename, char * * out){
	// open file
	FILE* fp;
	fp=fopen(filename, "r");
	if (fp == NULL) {
	    perror("no such file.");
	    return 0;
	}
	
	// find end of file string
	fseek(fp, 0, SEEK_END);
	long size = ftell(fp);
	rewind(fp);
		
	//allocate memory
	char *buffer = malloc (size + 1);
	if (!buffer) {
       		 perror("Can't allocate memory for file");
        	fclose(fp);
        	return 0;
    	}

	// Read file
    	fread(buffer, 1, size, fp);
    	buffer[size] = '\0'; // Null terminate

	fclose(fp);
	*out = buffer;
	return size;
}

// Md4c 

// ANSI colors
#define ANSI_RESET   "\033[0m"
#define ANSI_BOLD    "\033[1m"
#define ANSI_UNDER   "\033[4m"
#define ANSI_ITALIC   "\033[3m"

#define ANSI_CYAN    "\033[36m"
#define ANSI_YELLOW  "\033[33m"
#define ANSI_BLACK  "\033[30m"
#define ANSI_RED  "\033[31m"
#define ANSI_GREEN  "\033[32m"
#define ANSI_BLUE  "\033[34m"
#define ANSI_MAGENTA  "\033[35m"
#define ANSI_WHITE  "\033[37m"

#define ANSI_BG_BLUE "\033[43m"

static int current_block = 0;
static int in_link = 0;
static int in_em = 0;   // italics
static int in_strong = 0; // bold
static int in_under = 0; // under
static int in_code = 0; // under

static int enter_block(MD_BLOCKTYPE type, void *detail, void *userdata) {
    current_block = type;
    if (type == MD_BLOCK_H) printf("\n");
    return 0;
}

static int leave_block(MD_BLOCKTYPE type, void *detail, void *userdata) {
    if (type == MD_BLOCK_H || type == MD_BLOCK_P) printf("\n");
    current_block = 0;
    return 0;
}

static int enter_span(MD_SPANTYPE type, void *detail, void *userdata) {
    if (type == MD_SPAN_A) { in_link = 1; }
    if (type == MD_SPAN_EM) { in_em = 1; printf(ANSI_ITALIC); }
    if (type == MD_SPAN_STRONG) { in_strong = 1; printf(ANSI_BOLD); }
    if (type == MD_SPAN_U) { in_under = 1; printf(ANSI_UNDER); }
    if (type == MD_SPAN_CODE) { in_code = 1; printf(ANSI_BG_BLUE ANSI_ITALIC); }
    return 0;
}

static int leave_span(MD_SPANTYPE type, void *detail, void *userdata) {
    if (type == MD_SPAN_A) { in_link = 0; }
    if (type == MD_SPAN_EM) { in_em = 0; printf(ANSI_RESET); }
    if (type == MD_SPAN_STRONG) { in_strong = 0; printf(ANSI_RESET); }
    if (type == MD_SPAN_U) { in_under = 0; printf(ANSI_RESET); }
    if (type == MD_SPAN_CODE) { in_code = 0; printf(ANSI_RESET); }
    return 0;
}

static int text(MD_TEXTTYPE type, const MD_CHAR *text, MD_SIZE size, void *userdata) {
    if (current_block == MD_BLOCK_H) {
        printf(ANSI_BOLD ANSI_RED "%.*s" ANSI_RESET, (int)size, text);
    } else if (in_link) {
        printf(ANSI_YELLOW ANSI_UNDER "%.*s" ANSI_RESET, (int)size, text);
    } else {
        printf("%.*s", (int)size, text);
    }
    return 0;
}
int main(int argc, const char * argv[]){

	// raw markdown
	char * raw;
	int size_raw;

	int ch; //ncurses char
	int x = 0, y = 0;  // cursor position
		
	if (argc != 2){	
		printf("help\n");
		return 0;
	}

	initscr();              // Start ncurses
	cbreak();               // Disable line buffering
	noecho();               // Don't echo typed chars
	keypad(stdscr, TRUE);   // Enable arrow keys
	start_color();          // Enable colors
	
	init_pair(1, COLOR_CYAN, COLOR_BLACK);   // foreground cyan, bg black
	attron(COLOR_PAIR(1));
	printw("Heading\n");
	attroff(COLOR_PAIR(1));

	//
	printw("Hello, Markdown!\n");
    	refresh();
    	getch();                // Wait for key press
				//
	curs_set(1);
	while((ch = getch()) != 'q') {
    	switch(ch) {
        		case KEY_UP:
			case 'k':
				if (y > 0) y--;
				break;
        		case KEY_DOWN:
        		case 'j':
				if (y < LINES - 1) y++;
				break;
        		case KEY_LEFT:
        		case 'h':
				if (x > 0) x--;
				break;
        		case KEY_RIGHT:
        		case 'l':
				if (x < COLS - 1) x++;
				break;
        		case '\n':
				break;
			default:
				//mvaddch(y, x, ch); // type a character at cursor
				//if (x < COLS - 1) x++;
    		}
	move(y, x);   // move hardware cursor
    	refresh();
	}

    	endwin();               // Cleanup
				//
	size_raw = read_file(argv[1], &raw);	
	if(size_raw == 0){
		return 1;
	}

	//printf("%s", raw);

	MD_PARSER parser = {
        0,
        MD_FLAG_COLLAPSEWHITESPACE | MD_FLAG_UNDERLINE, // flags
        enter_block,
        leave_block,
        enter_span,
        leave_span,
        text,
        NULL,   // debug callback
        NULL    // userdata
    };
	// Parse Markdown
	md_parse(raw, size_raw, &parser, NULL);
	free(raw);
}

