
# include <stdio.h>
# include <stdlib.h>
# include <string.h>

//#define MD4C_USE_UTF8
# include "md4c.h"

# include "../include/render.h"

# include <curses.h>
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
	NUMBEROFELEMENTS,
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



// parse block callbacks
static int enter_block(MD_BLOCKTYPE type, void *detail, void *userdata) {
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


//Ncurses colors
# define DEFAULT_COLOR 1
# define CODE_COLOR 2
# define TITLE_COLOR 3
# define LINK_COLOR 4

typedef struct init{
int screen_start;
int screen_start_line;// the _line parameters are like the least signficant
		      // decimal place and represent the line withing a text given a screen size
int start;
int end;
int end_line;
int * text_positions;
int * line_counts;
int last_in_screen_flag;
int beggining_in_screen_flag;
Tvector tvec;
}Init_render;


Init_render* get_ptr_vars(){
	static Init_render vars={0,0,0,0,0,NULL,NULL,0,0, {NULL,0,0} };
	return &vars;
}

//next text/nl token from tokenid
int next_nl_or_text(int tokenid){
	Init_render* vars=get_ptr_vars();
	int i;
	for( i=tokenid; i< vars->tvec.size; i++)
		if(vars->tvec.tokens[i].cmd == NL || vars->tvec.tokens[i].cmd == TEXT)
			break;
	return i;

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

	vars->screen_start=next_nl_or_text(0);
	vars->start=next_nl_or_text(0);
	vars->screen_start_line=0;
	vars->last_in_screen_flag=0;

}
void destroy_renderer(){
	Init_render* vars=get_ptr_vars();
	tvec_free(&(vars->tvec));
}





//checks if cursor is above a link
int check_link(int xs,int  ys, int xe, int ye,  int cursorx, int cursory){

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

//count the lines of a text from a NL token
int count_size(int token){
	int  total_size=0 ;
	Init_render* vars=get_ptr_vars();
	Tvector tvec=vars->tvec;


	//aggregates all contiguous text
	for( int i=token+1; i<tvec.size; i++)
		if(tvec.tokens[i].cmd == TEXT ){
			total_size+=tvec.tokens[i].size;
		}
		else if(tvec.tokens[i].cmd == NL)
			break;

	return total_size;
}
//finds the char that beggins the line
int find_line(int * token, int line){
	Init_render* vars=get_ptr_vars();
	int line_number=0, i, j=0, total_size=0;
	int maxx, maxy;
	getmaxyx(stdscr, maxy, maxx);

	//if the line requested is larger that the current text it will just 
	//go to the next NL or the EOF
	for(i=*token; i<vars->tvec.size; i++){
		//goes through all text tokens between newlines
		if(vars->tvec.tokens[i].cmd == TEXT ){
			//goes through chars until line_number reaches the desired line
			//starts using the remainder of the total size up to this point
			for(j=total_size % maxx; j< vars->tvec.tokens[i].size; j+=maxx){
				if(line_number==line){
					*token=i;
					return j;
				}
				line_number++;
			}

			total_size+=vars->tvec.tokens[i].size;
		}
		//next new line if reached end of text block
		else if(vars->tvec.tokens[i].cmd == NL && i!=*token){
			*token=i;
			return -1;
		}


	}

	//end of file
	*token=i;
	return -2;
}

//next text token from tokenid
int next_text(int tokenid){
	Init_render* vars=get_ptr_vars();
	int i;
	for( i=tokenid; i< vars->tvec.size; i++)
		if(vars->tvec.tokens[i].cmd == TEXT)
			break;
	return i;

}



//goes from token to previous new line or text only if at the beggining
int prev_nl_or_text(int token){
	Init_render* vars=get_ptr_vars();
	Tvector tvec=vars->tvec;
	int i;
	for(i=token-1; i>=0; i--)
		if(tvec.tokens[i].cmd == NL  )
			break;
	if(0>=i)
		return next_nl_or_text(0);
	return i;
}

int get_last_in_screen(){
	Init_render* vars=get_ptr_vars();
	return vars->last_in_screen_flag;
}
int get_first_in_screen(){
	Init_render* vars=get_ptr_vars();
	return vars->beggining_in_screen_flag;
}
void scroll_down(){
	Init_render* vars=get_ptr_vars();
	int i=vars->screen_start;
	if(!vars->last_in_screen_flag){
		//jumps to next newline
		vars->screen_start_line+=1;
		if(-1 == find_line(&i,vars->screen_start_line)){ 
			vars->screen_start=i;
			vars->screen_start_line=0;
			
		}
	}
}
void scroll_up(){
	Init_render* vars=get_ptr_vars();
	int maxx, maxy;
	if(!vars->beggining_in_screen_flag){
		getmaxyx(stdscr, maxy, maxx);
		//jumps to next newline
		vars->screen_start_line-=1;
		if (vars->screen_start_line<0){
			vars->screen_start=prev_nl_or_text(vars->screen_start);
			vars->screen_start_line=count_size(vars->screen_start)/maxx;
		}
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
	printw("%.*s",maxx, msg);
    	attroff(A_REVERSE);
	//move back cursor
	move(y,x);
	refresh();


}
//Activates /deactivates atributes for each type of token
void attribute_renderer(int token, unsigned int attribute){
	Init_render* vars=get_ptr_vars();
	if(vars->tvec.tokens[token].start ){
		attron(attribute);
	}
	//end
	else{
		attroff(attribute);
	}

}

void print_text( char *s, int size) {
    for ( char *p = s; p-s<size; p++) {
        if (*p == '\n')
            addch(' ');
        else
            addch(*p);
    }
}
int render(int cursorx, int cursory, char * * link, size_t * link_size){

	int x, y,xe, ye, maxy, maxx;//screen size 
	getmaxyx(stdscr, maxy, maxx);

		     
	//acummulated tokens counter
	Init_render* vars=get_ptr_vars();
	int start=vars->start, end=vars->end;
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

	vars->beggining_in_screen_flag=0;
	vars->last_in_screen_flag=0;

	// ensure clean attribute state for this frame
	attrset(A_NORMAL);
	erase();               // removes leftover attributes / chars
	// if you want a default color background, apply it now:
	attron(COLOR_PAIR(1));
	move(0, 0);   // Cursor at 0 to input screen text
	int acc_flag=0;

	char finish=0;
	int i=0;
	int cmd=0;
	while(!finish){
		if(i>= tvec.size)
			break;
		cmd=tvec.tokens[i].cmd;
		switch(cmd){
			case NL: // newline
				//start rendering lines at the screen_start
				if(i < vars->screen_start)
					break;

				getyx(stdscr, y, x);
				if(y==maxy-2)
					finish=1;

				addch('\n');
				if(tvec.tokens[start].cmd==NL || tvec.tokens[end].cmd==NL){
					vars->beggining_in_screen_flag=0;
					if(start==i )
						vars->beggining_in_screen_flag=1;
					if(end==i)
						vars->last_in_screen_flag=1;
				}
				break;
			case BOLD:
				attribute_renderer(i, A_BOLD);
				break;
			case UNDER:
				attribute_renderer(i, A_UNDERLINE);
				break;
			case ITALIC:
				attribute_renderer(i, A_ITALIC);
				break;
			case HEADER1:
				attribute_renderer(i, A_BOLD | A_UNDERLINE | COLOR_PAIR(TITLE_COLOR));
				break;
			case HEADER2:
				attribute_renderer(i, A_BOLD  | COLOR_PAIR(TITLE_COLOR) );
				break;
			case HEADERPLUS:
				attribute_renderer(i, COLOR_PAIR(TITLE_COLOR));
				break;
			case CODE:
				attribute_renderer(i,COLOR_PAIR(CODE_COLOR) );
				break;
			case LINK: //Start of link 
				attribute_renderer(i,A_UNDERLINE |COLOR_PAIR(LINK_COLOR) );
				if(tvec.tokens[i].start){
					//saves this links index in the IR in this avar
					index_link=i;
				}
				else{
					index_link=-1;
				}
				break;
			case TEXT:
				int current_line_token=i, chr;

				char*text=tvec.tokens[i].text;
				//start rendering text only at screen_start
				if(i < vars->screen_start)
					break;

				new_size=tvec.tokens[i].size;

				if(i==next_text(vars->screen_start)){
					chr=find_line(&current_line_token, vars->screen_start_line);	
					//do not render text before current line's token
					if(current_line_token> i)
						break;
					//start text at the current line
					new_size-=chr;
					text+=chr;
				}

				vars->beggining_in_screen_flag=0;
				if(tvec.tokens[start].cmd==TEXT)
					if(start==i && vars->screen_start_line==0)
						vars->beggining_in_screen_flag=1;
				getyx(stdscr, y, x);


				//if text can reach or pass end of screen
				if(new_size >= (maxy-y-1)*maxx){
					new_size = (maxy-y-1)*maxx;
					finish=1;
				}
				//If reached the end of the screen
				if(new_size <= 0) {
					finish=1;
					break;
				}
				

				//print text
				print_text(text, new_size);

				if(tvec.tokens[end].cmd==TEXT)
					if(end==i && new_size >= vars->tvec.tokens[i].size)
						vars->last_in_screen_flag=1;
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
			default:
		}
		i++;

	}

	move(cursory, cursorx);   // move cursor to user position
    	refresh();
	attrset(A_NORMAL);
	return 0;
}
