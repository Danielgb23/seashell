# include <sys/stat.h>

# include <stdio.h>
# include <stdlib.h>
# include <string.h>

# include "render.h"

#include <ncurses.h>
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
	int fail;// flags
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

					//file type of link
					if(prefix(FILE_TAG, link_str)){
						//trim string
						size_t l = strlen(FILE_TAG);
						// Read file to memory
						FILE * fp = file_open(link_str+l);	
						fail=1;
						if (fp != NULL){
							size_raw = read_file(fp, &raw);
							if(size_raw == 0){
								//goes back to previous file
								FILE * fp = file_open(previous);	
								size_raw = read_file(fp, &raw);
								}

							else{
								fail=0;
								//restarts
								render_init(raw, size_raw);
							}
						}
					}
					free(link_str);

					if(fail){
						display_msg("File unavailable or empty.", x, y);
						napms(800);
					}
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

