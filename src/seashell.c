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
	if(!fp){
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

	*out = buffer;
	return size;
}

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
// TOKEN DATA STRUCTURE #################################################################

//Commands for renderer tokens
enum Cmd {
	NL, // newline
	BOLD,
	UNDER,
	ITALIC,
	HEADER1,
	HEADER2,
	HEADERPLUS,
	CODE,
	TEXT,
	LINK, //Start of link 
};

typedef struct token{
	enum Cmd cmd; //command id for the renderer
	char * text; //text address to be rendered or link href if link command
	size_t size; //size of text or link
	char start; //bool of start or end token
}Token;

typedef struct tvector{
	Token * tokens;
	size_t size;
	size_t capacity;
} Tvector;

// Initialize vector
void tvec_init(Tvector *vec) {
    vec->tokens = NULL;
    vec->size = 0;
    vec->capacity = 0;
}

// Grow if needed
static void tvec_grow(Tvector *vec) {
    size_t new_capacity = (vec->capacity == 0) ? 4 : vec->capacity * 2;
    Token *new_data = realloc(vec->tokens, new_capacity * sizeof(Token));
    if (!new_data) {
        perror("realloc");
    }
    vec->tokens = new_data;
    vec->capacity = new_capacity;
}

// Push a copy of a token
void tvec_push(Tvector *vec, enum Cmd cmd, char *text, size_t size, char start) {
    if (vec->size == vec->capacity) {
        tvec_grow(vec);
    }
    Token *t = &vec->tokens[vec->size++];
    t->cmd = cmd;
    t->size = size;
    t->text=text;
    t->start=start;

}

// Free vector
void tvec_free(Tvector *vec) {
    free(vec->tokens);
    vec->tokens = NULL;
    vec->size = 0;
    vec->capacity = 0;
}

//PARSER into IR ###########################################################################


static int current_block = 0;

// parse block callbacks
static int enter_block(MD_BLOCKTYPE type, void *detail, void *userdata) {
    current_block = type;
    Tvector * tvec=(Tvector *)userdata;
    if (type == MD_BLOCK_H){

	tvec_push( tvec,NL,(char*) NULL, 0, 1);	//adds newline
	MD_BLOCK_H_DETAIL *d = (MD_BLOCK_H_DETAIL *)detail;
	if(d->level ==1) tvec_push( tvec,HEADER1,(char*) NULL, 0, 1);	
	else if(d->level ==2) tvec_push( tvec,HEADER2,(char*) NULL, 0, 1);
	else tvec_push( tvec,HEADERPLUS,(char*) NULL, 0, 1);
    }
    return 0;
}
static int leave_block(MD_BLOCKTYPE type, void *detail, void *userdata) {
    	Tvector * tvec=(Tvector *)userdata;
    	if (type == MD_BLOCK_H || type == MD_BLOCK_P)
		tvec_push(tvec,NL,(char*) NULL, 0, 1);	//adds newline

	if (type == MD_BLOCK_H){
		MD_BLOCK_H_DETAIL *d = (MD_BLOCK_H_DETAIL *)detail;
		if(d->level ==1) tvec_push(tvec,HEADER1,(char*) NULL, 0, 0);	
		else if(d->level ==2) tvec_push(tvec,HEADER2,(char*) NULL, 0, 0);
		else tvec_push(tvec,HEADERPLUS,(char*) NULL, 0, 0);
	}
    	return 0;
}
//Parse SPAN callbacks
static int enter_span(MD_SPANTYPE type, void *detail, void *userdata) {
    	Tvector * tvec=(Tvector *)userdata;
    if (type == MD_SPAN_A) {
 	MD_SPAN_A_DETAIL *d = (MD_SPAN_A_DETAIL*) detail;
    	tvec_push(tvec,LINK,(char*) d->href.text, d->href.size, 1);
    }
    if (type == MD_SPAN_EM) tvec_push(tvec,ITALIC,(char*) NULL, 0, 1);
    if (type == MD_SPAN_STRONG) tvec_push(tvec,BOLD,(char*) NULL, 0, 1);
    if (type == MD_SPAN_U) tvec_push(tvec,UNDER,(char*) NULL, 0, 1);
    if (type == MD_SPAN_CODE) tvec_push(tvec,CODE,(char*) NULL, 0, 1);

    return 0;
}
static int leave_span(MD_SPANTYPE type, void *detail, void *userdata) {
    	Tvector * tvec=(Tvector *)userdata;

    if (type == MD_SPAN_A) {
 	MD_SPAN_A_DETAIL *d = (MD_SPAN_A_DETAIL*) detail;
    	tvec_push(tvec,LINK,(char*) d->href.text, d->href.size, 0);
    }
    if (type == MD_SPAN_EM) tvec_push(tvec,ITALIC,(char*) NULL, 0, 0);
    if (type == MD_SPAN_STRONG) tvec_push(tvec,BOLD,(char*) NULL, 0, 0);
    if (type == MD_SPAN_U) tvec_push(tvec,UNDER,(char*) NULL, 0, 0);
    if (type == MD_SPAN_CODE) tvec_push(tvec,CODE,(char*) NULL, 0, 0);

    return 0;
}

//PARSE text callbacks
static int text(MD_TEXTTYPE type, const MD_CHAR *text, MD_SIZE size,  void *userdata) {
    	Tvector * tvec=(Tvector *)userdata;
	tvec_push(tvec,TEXT,(char*) text,(size_t) size, 0);	
	return 0;
}

// RENDER ###############################################################################
//put in struct and put in userdata as pointers declared in main

static int last_in_screen_flag=0;
static int beggining_in_screen_flag=0;

//Ncurses colors
# define DEFAULT_COLOR 1
# define CODE_COLOR 2
# define TITLE_COLOR 3
# define LINK_COLOR 4

typedef struct init{
int screen_start;
int start;
int end;
int * text_positions;
int * line_counts;
Tvector tvec;
}Init_render;


Init_render* get_ptr_vars(){
	static Init_render vars={0,0,0,NULL,NULL, {NULL,0,0}};
	return &vars;
}

//initializes renderer or returns initial variables
void render_init(char* raw, int size_raw){
	Init_render * vars=get_ptr_vars();

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


	tvec_init(&(vars->tvec));

	md_parse(raw, size_raw, &parser, &(vars->tvec));

	//reinitializes parsed IR
		//gives pointer address of static var

		int i, start_flag=0, count=0;
		for(i=0;i<vars->tvec.size ; i++)
			if(vars->tvec.tokens[i].cmd == NL ||vars->tvec.tokens[i].cmd == TEXT ){
				//first NL or TEXT
				if(start_flag==1){
				vars->start=i;
				   start_flag=0;
				}
	
				//last NL or TEXT
				vars->end=i;
				//co->nts total NLs or TEXTs
				count++;
			}




}
void destroy_renderer(){
	Init_render* vars=get_ptr_vars();
	tvec_free(&(vars->tvec));
}





int check_link(int xs,int  ys, int xe, int ye,  int cursorx, int cursory){
	// checks for links
		//One line link
		if( ye == ys && cursory == ys)		
		{
			// Cursor inside the link
			if (cursorx <= xe-1 && cursorx >= xs )
				return 1;
		}
		else{
			//first line of multiline link
			if (cursory==ys){
				if(cursorx >= xs )
					return 1;
				
			}
			//last line of multiline link
			else if (cursory==ye){
				if(cursorx <= xe-1 )
					return 1;
			}
			//middle lines of multiline link
			else if (cursory>=ys && cursory<=ye){
				return 1;
			}
			
		}
	return 0;

}


//next line
void next_line(){
	Init_render* vars=get_ptr_vars();
	int start=vars->screen_start;
	Tvector tvec=vars->tvec;
	int i;
	for(i=start+1; i<tvec.size; i++)
		if(tvec.tokens[i].cmd == NL )
			break;
	vars->screen_start=i;
}

//previous line
void prev_line( ){
	Init_render* vars=get_ptr_vars();
	int start=vars->screen_start;
	Tvector tvec=vars->tvec;
	int i;
	for(i=start-1; i>=0; i--)
		if(tvec.tokens[i].cmd == NL  )
			break;

	vars->screen_start=i;
}

void scroll_down( ){
	if(!last_in_screen_flag){
		//find previous new line in raw or end of string
		next_line();
	}
}
void scroll_up(){
	if(!beggining_in_screen_flag){
		//find new next line in raw or end of string
		prev_line();
	}

}

void display_msg(const char* msg, int x, int y){
	int maxx, maxy;
	getmaxyx(stdscr, maxy, maxx);
	// Print message at the last line
    	move(maxy-1, 0);            // move to last line
	attrset(A_NORMAL);
    	clrtoeol();                 // clear it
    	attron(A_REVERSE);          // optional: highlight status bar
	printw("%s", msg);
    	attroff(A_REVERSE);
	//move back cursor
	move(y,x);
	refresh();


}

int render(int cursorx, int cursory, char * * link, size_t * link_size){

	int x, y,xe, ye, maxy, maxx;//screen size 
	getmaxyx(stdscr, maxy, maxx);

	Init_render* vars=get_ptr_vars();
	int start=vars->start, end=vars->end, screen_start=vars->screen_start;
	Tvector tvec=vars->tvec;

	size_t new_size=0;
	 int   index_link=-1;
	//COLORS of each enviroment
	init_pair(DEFAULT_COLOR, -1, -1);   // default terminal
	init_pair(CODE_COLOR, COLOR_GREEN, COLOR_YELLOW) ;
	init_pair(TITLE_COLOR, COLOR_MAGENTA, -1) ;  
	init_pair(LINK_COLOR, COLOR_YELLOW, -1)  ;

	*link=NULL;
	*link_size=0;

	beggining_in_screen_flag=0;
	last_in_screen_flag=0;

	// ensure clean attribute state for this frame
	attrset(A_NORMAL);
	erase();               // removes leftover attributes / chars
	// if you want a default color background, apply it now:
	attron(COLOR_PAIR(1));
	move(0, 0);   // Cursor at 0 to input screen text

	char finish=0;
	int i=screen_start;
	while(!finish){
		if(i>= tvec.size)
			break;
		switch(tvec.tokens[i].cmd){
			case NL: // newline
			printw("\n");
			if(tvec.tokens[start].cmd==NL || tvec.tokens[end].cmd==NL){
				if(start==i )
					beggining_in_screen_flag=1;
				if(end==i)
					last_in_screen_flag=1;
				}
				break;
			case BOLD:
				//start
				if(tvec.tokens[i].start)
					attron(A_BOLD);
				//end
				else
					attroff(A_BOLD);

				break;
			case UNDER:
				if(tvec.tokens[i].start)
					attron(A_UNDERLINE);
				else
					attroff(A_UNDERLINE);
				break;
			case ITALIC:
				if(tvec.tokens[i].start)
					attron(A_ITALIC);
				else
					attroff(A_ITALIC);
				break;
			case HEADER1:
				if(tvec.tokens[i].start)
					attron( A_BOLD | A_UNDERLINE | COLOR_PAIR(TITLE_COLOR));
				else
					attroff( A_BOLD | A_UNDERLINE | COLOR_PAIR(TITLE_COLOR));
				break;
			case HEADER2:
				if(tvec.tokens[i].start)
					attron( A_BOLD |COLOR_PAIR(TITLE_COLOR));
				else
					attroff( A_BOLD  | COLOR_PAIR(TITLE_COLOR));
				break;
			case HEADERPLUS:
				if(tvec.tokens[i].start)
					attron( COLOR_PAIR(TITLE_COLOR));
				else
					attroff( COLOR_PAIR(TITLE_COLOR));
				break;
			case CODE:
				if(tvec.tokens[i].start)
					attron( COLOR_PAIR(CODE_COLOR));
				else
					attroff( COLOR_PAIR(CODE_COLOR));
				break;
			case TEXT:
				
			if(tvec.tokens[start].cmd==TEXT)
				if(start==i)
					beggining_in_screen_flag=1;

				new_size=tvec.tokens[i].size;
				getyx(stdscr, y, x);
				//if text can reach or pass end of screen
				if(new_size >= (maxy-y-1)*maxx){
					new_size = (maxy-y-1)*maxx;
				}
				//If reached the end of the screen
				if(new_size <= 0) {
					finish=1;
					break;
				}
				//print text
				printw("%.*s",(int)new_size,tvec.tokens[i].text );

				if(tvec.tokens[end].cmd==TEXT)
					if(end==i)
						last_in_screen_flag=1;
				//if text was flagged as link (indexlink!=-1)
				if(index_link>=0){
					//gets end of link text in screen and checks if cursor
					//is over it
					getyx(stdscr, ye, xe);
					if(check_link(x,y,xe,ye, cursorx, cursory)){

						*link=tvec.tokens[index_link].text;
						*link_size=tvec.tokens[index_link].size;
					}
				}
				break;
			case LINK: //Start of link 
				if(tvec.tokens[i].start){
					attron( A_UNDERLINE |COLOR_PAIR(LINK_COLOR));
					//saves this links index in the IR in this avar
					index_link=i;
				}
				else{
					attroff( A_UNDERLINE  | COLOR_PAIR(LINK_COLOR));
					index_link=-1;
				}
				break;
		}
		i++;

	}



   	 move(cursory, cursorx);   // move cursor to user position
    	refresh();
	attrset(A_NORMAL);
	return 0;
}
//MAIN #############################################################################################
# define FILE_TAG "file://"
int prefix(const char *pre, const char *str)
{
    return strncmp(pre, str, strlen(pre)) == 0;
}

int main(int argc, const char * argv[]){
	// raw markdown
	char * raw=NULL; //raw text saved in memory
	int size_raw;

	


	//char * previous_path;
	char * previous=NULL;//path of previous file
				   //
	int ch; //ncurses char
	int x = 0, y = 0;  // cursor position
	char * link=NULL ,* link_str; //current link under the cursor
	size_t link_size=0; //current link under the cursor
			    //
	int maxy, maxx;//screen size 
	MEVENT event;


	
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
		printf("No data in file or file non existant\n");
		return 0;
	}

							



	initscr();              // Start ncurses
	cbreak();               // Disable line buffering
	noecho();               // Don't echo typed chars
	keypad(stdscr, TRUE);   // Enable arrow keys
	mousemask(BUTTON1_CLICKED | BUTTON3_CLICKED| BUTTON4_PRESSED| BUTTON5_PRESSED| REPORT_MOUSE_POSITION, NULL);
	start_color();          // Enable colors
	use_default_colors();   // Use terminal color setup

	render_init(raw, size_raw);
	render(x,y, &link,  &link_size);



	//timeout(100); // don't block forever
	while((ch = getch())!= 'q') {
		getmaxyx(stdscr, maxy, maxx);
		// mode notebook
    		switch(ch) {
			case KEY_RESIZE:
				render_init(raw, size_raw);
				break;
			case 27://ESC
				y++;
				break;
			// MOVEMENT OF THE CURSOR
        		case KEY_UP:
			case 'k':
				if (y > 0) y--;
				else scroll_up();
				break;
        		case KEY_DOWN:
        		case 'j':
				if (y < maxy - 2) y++;
				else scroll_down();
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
				    if(event.bstate & BUTTON4_PRESSED){

					if (y < maxy - 2) y++;
					scroll_up();
					break;
				}
				    if(event.bstate & BUTTON5_PRESSED){

					if (y > 0) y--;
					scroll_down();

					break;
					}
				render(x,y, &link,  &link_size);
			        }

				if(link==NULL)
			        	break;
        		case '\n':
				//checks links when enter is pressed
				//check if it's a file link
				
				if(link != NULL ){

					link_str=strndup(link,link_size);
					if(prefix(FILE_TAG, link_str)){
						//trim string
						size_t l = strlen(FILE_TAG);
						// Read file to memory
						FILE * fp = file_open(link_str+l);	
						if (fp != NULL){
							size_raw = read_file(fp, &raw);
							if(size_raw == 0){
								//goes back to previous file
								FILE * fp = file_open(previous);	
								size_raw = read_file(fp, &raw);
								}

							else{
								//find last and first text char 
								//rendered from the raw file address in the raw file

								render_init(raw, size_raw);
								clear();
							}
						}
						else{
							display_msg("File unvavailable.", x, y);
							napms(800);
						}
						//size_raw = read_file(link + l, &raw);	
					}
					free(link_str);
					}
				break;
			default:
				//mvaddch(y, x, ch); // type a character at cursor
				//if (x < maxx - 1) x++;
    		}


		//RENDER ###########################################################################
			      
		render(x,y, &link,  &link_size);


		// Print link at the last line
	   	if(link != NULL){
			link_str=strndup(link, link_size);
			display_msg(link,  x, y);
			free(link_str);
							     
		}
	}

    	endwin();               // Cleanup
				//
	//printf("%s", raw);
	
	//destroys
	destroy_renderer();
	fclose(fp);
	free(raw);
}

