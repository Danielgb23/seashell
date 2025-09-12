# include <sys/stat.h>

# include <stdio.h>
# include <stdlib.h>
# include <string.h>

//#define MD4C_USE_UTF8
# include "md4c.h"

# include <curses.h>
//###################################################################################################
//Read files
#define MAX_FILE_SIZE (256 * 1024 * 1024)  // 256 MB
FILE * file_open(const char *filename){
	// open file
	FILE* fp;
	fp=fopen(filename, "r");
	if (fp == NULL) {
	    //perror("no such file.");
	    return NULL;
	}
	// File size limit
	struct stat st;
    	if (stat(filename, &st) != 0) return 0;

    	if (st.st_size > MAX_FILE_SIZE) {
        fprintf(stderr, "File too large (%ld bytes)\n", st.st_size);
        return NULL;
    }
	return fp;
}


int read_file(FILE*  fp, char * * out){
		
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

	*out = buffer;
	return size;
}

//###################################################################################################
//Link stack
#define MAX_STACK 128
# define FILE_TAG "file://"
typedef struct link{
char * href;
int xs, ys, xe, ye;
}Link;

char *strndup(const char *s, size_t n) {
    size_t len = 0;
    while (s[len] && len < n) len++;   // determine actual length to copy

    char *copy = malloc(len + 1);
    if (!copy) return NULL;

    for (size_t i = 0; i < len; i++)
        copy[i] = s[i];

    copy[len] = '\0';
    return copy;
}

Link link_arr [MAX_STACK];
int link_top = -1;

void push_link(const char *href, int size) {
    	if (link_top < MAX_STACK - 1) {
        	link_arr[++link_top].href = strndup(href, size);
    	}
}

void destroy_link_arr() {
	for (int i=0; i<link_top; i++){
         free(link_arr[i].href);
    }
	link_top=-1;
}



void add_link_pos(int xstart, int ystart, int  xend,int  yend){
link_arr[link_top].xs=xstart;
link_arr[link_top].ys=ystart;
link_arr[link_top].xe=xend;
link_arr[link_top].ye=yend;



}

int prefix(const char *pre, const char *str)
{
    return strncmp(pre, str, strlen(pre)) == 0;
}


char * checks_links(int x,int  y){
	// checks for links
	char * link = NULL;
	for(int i=0; i<link_top+1; i++){
		//One line link
		if( link_arr[i].ye == link_arr[i].ys && y == link_arr[i].ys)		
		{
			// Cursor inside the link
			if (x <= link_arr[i].xe-1 && x >= link_arr[i].xs )
				link=link_arr[i].href;
		}
		else{
			//first line of multiline link
			if (y==link_arr[i].ys){
				if(x >= link_arr[i].xs )
					link=link_arr[i].href;
				
			}
			//last line of multiline link
			else if (y==link_arr[i].ye){
				if(x <= link_arr[i].xe-1 )
					link=link_arr[i].href;
			}
			//middle lines of multiline link
			else if (y>=link_arr[i].ys && y<=link_arr[i].ye){
					link=link_arr[i].href;
			}
			
		}
	}
	return link;

}
//###################################################################################################
// Md4c + Ncurses
# define CODE_COLOR 2
# define TITLE_COLOR 3
# define LINK_COLOR 4

//put in struct and put in userdata as pointers declared in main
static int current_block = 0;
static int in_link = 0;
static int in_em = 0;   // italics
static int in_strong = 0; // bold
static int in_under = 0; // under
static int in_code = 0; // under
static int save_links = 1;
static int last_line_flag = 0;
static int find_extr = 1;
static int first_call = 1;

static int last_in_screen_flag=0;
static int beggining_in_screen_flag=0;
static char * last_char=NULL;
static char * last_char_eff=NULL;
static char * first_char_eff=NULL;

// parse block callbacks
static int enter_block(MD_BLOCKTYPE type, void *detail, void *userdata) {
    current_block = type;
    if (type == MD_BLOCK_H){
	    printw("\n");
	MD_BLOCK_H_DETAIL *d = (MD_BLOCK_H_DETAIL *)detail;
	if(d->level ==1) attron( A_BOLD | A_UNDERLINE);
	if(d->level ==2) attron( A_BOLD );
    }
	if(last_line_flag)
		return 1;
    return 0;
}
static int leave_block(MD_BLOCKTYPE type, void *detail, void *userdata) {
    	if (type == MD_BLOCK_H || type == MD_BLOCK_P) printw("\n");
    	current_block = 0;
	if (type == MD_BLOCK_H){
		MD_BLOCK_H_DETAIL *d = (MD_BLOCK_H_DETAIL *)detail;
		if(d->level ==1) attroff( A_BOLD | A_UNDERLINE);
		if(d->level ==2) attroff( A_BOLD );
	}
    	return 0;
}
//Parse SPAN callbacks
static int enter_span(MD_SPANTYPE type, void *detail, void *userdata) {
    if (type == MD_SPAN_A) {
	in_link = 1;
 	MD_SPAN_A_DETAIL *d = (MD_SPAN_A_DETAIL*) detail;
	//pushes link into list
	if(save_links)
    		push_link(d->href.text, d->href.size);
    }
    if (type == MD_SPAN_EM) { in_em = 1; attron(A_ITALIC); }
    if (type == MD_SPAN_STRONG) { in_strong = 1; attron( A_BOLD); }
    if (type == MD_SPAN_U) { in_under = 1; attron(A_UNDERLINE); }
    if (type == MD_SPAN_CODE) { in_code = 1; attron(COLOR_PAIR(CODE_COLOR)| A_ITALIC); }

	if(last_line_flag)
		return 1;
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

//PARSE text callbacks
static int text(MD_TEXTTYPE type, const MD_CHAR *text, MD_SIZE size,  void *userdata) {
	int x, y; //cursor current position
	size_t new_size=size;
	last_line_flag=0;

	//Last char in the callback, the effective last char is this variable
	//after the md_parse call is done
	last_char=(char*)text+size-1;	
	
	//find beggining
	if(find_extr && first_call){
		first_char_eff = (char *)text;
		first_call=0;
	}
	
	//Blocks texts from leaving the screen
	if(!find_extr ){

		//last character possible rendered from file is (flag=1) or is not in the screen
		last_in_screen_flag=0;
		if(last_char_eff<=last_char)
			last_in_screen_flag=1;

		//first character possible rendered from file is or is not in the screen
		if(first_call){
			beggining_in_screen_flag=0;
			if(first_char_eff>=text)
				beggining_in_screen_flag=1;
			first_call=0;
		}

		getyx(stdscr, y, x);
		//if reached or passed end of screen
		if(size >= (LINES-y-1)*COLS){
			new_size=(LINES-y-1)*COLS;
			last_line_flag=1;
		}
		//If reached the end of the screen
		if(new_size<=0) {
			last_line_flag=1;
			return 1;
		}
		//titles
		if (current_block == MD_BLOCK_H) {
			attron( COLOR_PAIR(TITLE_COLOR) );
			printw("%.*s", (int)new_size, text);
			attroff( COLOR_PAIR(TITLE_COLOR));
		//LINKs
		} else if (in_link) {
			int xs, ys, xe, ye;
			getyx(stdscr, ys, xs);//get start of link
			attron( COLOR_PAIR(LINK_COLOR)|A_UNDERLINE);
		 	printw( "%.*s", (int)new_size, text);
			attroff( COLOR_PAIR(LINK_COLOR)|A_UNDERLINE);
			getyx(stdscr, ye, xe);//get end of link
			//saves positions of link into list
			if (save_links)
				add_link_pos(xs,ys, xe,ye);
		//Everything else
		} else {
			printw("%.*s", (int)new_size, text);
		}

		getyx(stdscr, y, x);
		// if on the last line
		if(y == LINES-1  ) {
			last_line_flag=1;
		}
	}
	return 0;
}

//###################################################################################################
int main(int argc, const char * argv[]){
	


	// raw markdown
	char * raw=NULL; //raw text saved in memory
	int size_raw;
	
	char * screen_prev=NULL; //previous screen position scroll
	char * screen_start=NULL;//start of current screen for scroll
	char * newline;//newline position

	//char * previous_path;
	char * previous=NULL;//path of previous file
				   //
	int ch; //ncurses char
	int x = 0, y = 0;  // cursor position
	char * link=NULL; //current link under the cursor
	int maxy, maxx;//screen size 
	MEVENT event;


	// End program flag
	int end= 0 ;
	
	// Help message
	if (argc != 2 ||strcmp(argv[1],"-h") ==0 ||strcmp(argv[1], "--help")==0){	
		printf("help message\n");
		return 0;
	}

	// Read file to memory
	previous=(char*)argv[1];
	FILE * fp = file_open(argv[1]);	
	size_raw = read_file(fp, &raw);
	if(size_raw == 0){
		printf("No data in file");
		return 0;
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
	mousemask(BUTTON1_CLICKED | BUTTON3_CLICKED | REPORT_MOUSE_POSITION, NULL);
	start_color();          // Enable colors
	use_default_colors();   // Use terminal color setup
	

	//COLORS of each enviroment
	init_pair(1, -1, -1);   // default terminal
	init_pair(CODE_COLOR, COLOR_GREEN, COLOR_YELLOW) ;
	init_pair(TITLE_COLOR, COLOR_MAGENTA, -1) ;  
	init_pair(LINK_COLOR, COLOR_YELLOW, -1)  ;
				//
	attron(COLOR_PAIR(1));
	//wbkgd(stdscr, COLOR_PAIR(1));
	
	move(0, 0);   // cursor to origin
	//render markdown
	screen_start = raw;
	screen_prev = raw;

	//find last and first text char rendered from the raw file address in the raw file
	
	save_links = 0; // stop saving links
	find_extr = 1;
	first_call=1;
	md_parse(screen_start, size_raw, &parser, NULL);
	last_char_eff=last_char;
	find_extr=0;//after first run start displaying text

	save_links = 1; // save links
	//show first screen
	move(0, 0);   // cursor to origin
	last_line_flag=0;
	first_call=1;
	md_parse(screen_start, size_raw, &parser, NULL);

	save_links = 0; // stop saving links
	curs_set(1); // cursor appearing
	move(0, 0);   // cursor to origin
    	refresh();

	timeout(100); // don't block forever
	while((ch = getch())!= 'q') {
		getmaxyx(stdscr, maxy, maxx);
		// mode notebook
    		switch(ch) {
		
			case 27://ESC
				y++;
				break;
			// MOVEMENT OF THE CURSOR
        		case KEY_UP:
			case 'k':
				if (y > 0) y--;
				else if(!beggining_in_screen_flag){
					//find new next line in raw or end of string
					for(newline=screen_start-1; newline>raw && (*newline) != '\n'; newline--);	
					//next character from \n if not end
					screen_start=newline>raw? newline-1:raw; 
					destroy_link_arr();
					save_links=1;//to find again the links
					clrtoeol();

				}
				break;
        		case KEY_DOWN:
        		case 'j':
				if (y < maxy - 2) y++;
				else if(!last_in_screen_flag){
					//find new next line in raw or end of string
					for(newline=screen_start+1; (*newline) && (*newline) != '\n'; newline++);	
					//next character from \n if not end
					screen_start= newline+1; 
					destroy_link_arr();
					save_links=1;//to find again the links
					clrtoeol();

				}
				break;
        		case KEY_LEFT:
        		case 'h':
				if (x > 0) x--;
				break;
        		case KEY_RIGHT:
        		case 'l':
				if (x < maxx - 1) x++;
				break;

			case KEY_MOUSE:
			        if (getmouse(&event) == OK) {
			            if (event.bstate & BUTTON1_CLICKED) {
			                // Move cursor to clicked position
			                y=event.y;
					x=event.x;
			            }
			        }

				link=checks_links(x, y);
				if(link==NULL)
			        	break;
        		case '\n':
				//checks links when enter is pressed
				//check if it's a file link
				if(link != NULL && prefix(FILE_TAG, link)){
					//trim string
					size_t l = strlen(FILE_TAG);
					// Read file to memory
					FILE * fp = file_open(link+l);	
					if (fp != NULL){
						size_raw = read_file(fp, &raw);
						if(size_raw == 0){
							//goes back to previous file
							FILE * fp = file_open(previous);	
							size_raw = read_file(fp, &raw);
											}

						else{


							screen_start=raw;
							screen_prev=raw;
							//find last and first text char 
							//rendered from the raw file address in the raw file
							find_extr = 1;
							first_call=1;
							md_parse(screen_start, size_raw, &parser, NULL);
							last_char_eff=last_char;
							find_extr=0;//after first run start displaying text

							//reset links
							destroy_link_arr();
							save_links=1;
							clear();
						}
					}
					else{

						// Print link at the last line
					    	move(maxy-1, 0);            // move to last line
					    	clrtoeol();                 // clear it
					    	attron(A_REVERSE);          // optional: highlight status bar
					    	printw("File unavailable");
					    	attroff(A_REVERSE);
    						refresh();
						napms(800);
					}
					//size_raw = read_file(link + l, &raw);	
				}
				break;
			default:
				//mvaddch(y, x, ch); // type a character at cursor
				//if (x < maxx - 1) x++;
    		}


		//RENDER ###########################################################################
		// ensure clean attribute state for this frame
		attrset(A_NORMAL);
		erase();               // removes leftover attributes / chars
		// if you want a default color background, apply it now:
		attron(COLOR_PAIR(1));
		move(0, 0);   // Cursor at 0 to input screen text
			      
		last_line_flag=0;
		first_call=1;
		md_parse(
			screen_start,
			size_raw-(screen_start?(screen_start-raw): 0),
			&parser,
			NULL
		);		//render markdown
		save_links=0;

		//RENDER ###########################################################################
	
		link=checks_links(x, y);
							     
		// Print link at the last line
    		move(maxy-1, 0);            // move to last line
    		clrtoeol();                 // clear it
    		attron(A_REVERSE);          // optional: highlight status bar
		if(link != NULL)
    			printw("%s", link);
    		attroff(A_REVERSE);

		move(y, x);   // move cursor to user position
    		refresh();
	}

    	endwin();               // Cleanup
				//
	//printf("%s", raw);
	
	//destroys
	fclose(fp);
	destroy_link_arr();
	free(raw);
}

