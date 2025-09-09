# include <sys/stat.h>
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include "md4c.h"

# include <ncurses.h>

#define MAX_FILE_SIZE (256 * 1024 * 1024)  // 256 MB
					   //
int read_file(const char *filename, char * * out){

	// File size limit
	struct stat st;
    	if (stat(filename, &st) != 0) return 0;

    	if (st.st_size > MAX_FILE_SIZE) {
        fprintf(stderr, "File too large (%ld bytes)\n", st.st_size);
        return 0;
    }


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
# define CODE_COLOR 2
# define TITLE_COLOR 3
# define LINK_COLOR 3

static int current_block = 0;
static int in_link = 0;
static int in_em = 0;   // italics
static int in_strong = 0; // bold
static int in_under = 0; // under
static int in_code = 0; // under

static int enter_block(MD_BLOCKTYPE type, void *detail, void *userdata) {
    current_block = type;
    if (type == MD_BLOCK_H) printw("\n");
    return 0;
}

static int leave_block(MD_BLOCKTYPE type, void *detail, void *userdata) {
    if (type == MD_BLOCK_H || type == MD_BLOCK_P) printw("\n");
    current_block = 0;
    return 0;
}

static int enter_span(MD_SPANTYPE type, void *detail, void *userdata) {
    if (type == MD_SPAN_A) { in_link = 1; }
    if (type == MD_SPAN_EM) { in_em = 1; attron(A_ITALIC); }
    if (type == MD_SPAN_STRONG) { in_strong = 1; attron( A_BOLD); }
    if (type == MD_SPAN_U) { in_under = 1; attron(A_UNDERLINE); }
    if (type == MD_SPAN_CODE) { in_code = 1; attron(COLOR_PAIR(CODE_COLOR)| A_ITALIC); }
    return 0;
}

static int leave_span(MD_SPANTYPE type, void *detail, void *userdata) {
    if (type == MD_SPAN_A) { in_link = 0; }
    if (type == MD_SPAN_EM) { in_em = 0; attroff( A_ITALIC);}
    if (type == MD_SPAN_STRONG) { in_strong = 0; attroff( A_BOLD); }
    if (type == MD_SPAN_U) { in_under = 0; attroff( A_UNDERLINE); }
    if (type == MD_SPAN_CODE) { in_code = 0; attroff( COLOR_PAIR(CODE_COLOR)|A_ITALIC); }
    return 0;
}

static int text(MD_TEXTTYPE type, const MD_CHAR *text, MD_SIZE size, void *userdata) {
    if (current_block == MD_BLOCK_H) {
	attron( COLOR_PAIR(TITLE_COLOR)|A_BOLD);
        printw("%.*s", (int)size, text);
	attroff( COLOR_PAIR(TITLE_COLOR)|A_BOLD);
    } else if (in_link) {
	attron( COLOR_PAIR(LINK_COLOR)|A_UNDERLINE);
        printw( "%.*s", (int)size, text);
	attroff( COLOR_PAIR(LINK_COLOR)|A_UNDERLINE);
    } else {
        printw("%.*s", (int)size, text);
    }
    return 0;
}
int main(int argc, const char * argv[]){

	// raw markdown
	char * raw;
	int size_raw;

	int ch; //ncurses char
	int x = 0, y = 0;  // cursor position
		
	// End program flag
	int end= 0 ;
	
	// Help message
	if (argc != 2 ||strcmp(argv[1],"-h") ==0 ||strcmp(argv[1], "--help")==0){	
		printf("help message\n");
		return 0;
	}

	// Read file to memory
	size_raw = read_file(argv[1], &raw);	
	if(size_raw == 0){
		return 1;
	}

	// Md4c parcer pointers to callbacks and flags etc
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

	initscr();              // Start ncurses
	cbreak();               // Disable line buffering
	noecho();               // Don't echo typed chars
	keypad(stdscr, TRUE);   // Enable arrow keys
	start_color();          // Enable colors
	use_default_colors();   // Use terminal color setup
	

	//COLORS of each enviroment
	init_pair(1, -1, -1);   // default terminal
	init_pair(CODE_COLOR, COLOR_GREEN, COLOR_YELLOW) ;
	init_pair(TITLE_COLOR, COLOR_RED, -1) ;  
	init_pair(TITLE_COLOR, COLOR_YELLOW, -1)  ;
				//
	attron(COLOR_PAIR(1));
	//wbkgd(stdscr, COLOR_PAIR(1));

	md_parse(raw, size_raw, &parser, NULL);

	curs_set(1); // cursor appearing
	move(0, 0);   // cursor to origin
		      //
    	refresh();
	while((ch = getch())) {
		// mode notebook
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
			case 'q':
				end=1;
				break;
			default:
				//mvaddch(y, x, ch); // type a character at cursor
				//if (x < COLS - 1) x++;
    		}
		if (end) break;// ends the main loop
			       //
		move(0, 0);   // move hardware cursor
		md_parse(raw, size_raw, &parser, NULL);
		move(y, x);   // move hardware cursor
    		refresh();
	}

    	endwin();               // Cleanup
				//


	//printf("%s", raw);


	// Parse Markdown
	free(raw);
}

