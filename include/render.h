# include <stdio.h>
# include <stdlib.h>
# include <string.h>

# include "md4c.h"

int render(int cursorx, int cursory, char * * link, size_t * link_size);

void scroll_down();
void scroll_up();
int get_last_in_screen();
int get_first_in_screen();

void display_msg(const char* msg, int x, int y);

void destroy_renderer();

void render_init(char* raw, int size_raw);
