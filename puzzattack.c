#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <SDL/SDL.h>

#include "SDL/SDL_ttf.h"
#include "SDL/SDL_mixer.h"
#include "SDL_vita_button.h"

#define SRCDIR "app0:assets"

#define SCREEN_WIDTH 960
#define SCREEN_HEIGHT 544

#define BOARD_HEIGHT 12
#define BOARD_WIDTH 6

#define BRICK_WIDTH 45
#define BRICK_HEIGHT 45

#define NUM_BRICK_TYPES 6

#define STATE_PAUSED 0
#define STATE_RISING 1
#define STATE_GAMEOVER 2
#define STATE_PAUSEGAME 3

#define LIGHT_TIME 500
#define STAND_TIME_BREAK 250
#define STAND_TIME_FALL 125

typedef struct board_t {
	int bricks[BOARD_HEIGHT][BOARD_WIDTH];		// array of actual bricks
	int lit[BOARD_HEIGHT][BOARD_WIDTH];		// timer for lit up bricks
	int standing[BOARD_HEIGHT][BOARD_WIDTH];	// timer for empty brick places, before falling
	int state;
	int pause_time;
	int rise_time;
	int next_rise_time;
	int curs_x, curs_y;
	int score;
} board_t;

typedef struct resources_t {
	SDL_Surface *brick_surf[NUM_BRICK_TYPES+1];
	SDL_Surface *cursor;
	SDL_Surface *lightmask;
	SDL_Surface *background;
	TTF_Font *font30;
	TTF_Font *font50;
	Mix_Chunk *cursor_sound;
	Mix_Chunk *explode_sound;
}resources_t;

SDL_Color whiteColor = { 255, 255, 255 };

board_t *board_new();
void board_init(board_t *board);
void board_fill ( board_t *board, int fillHeight);
void board_clear ( board_t *board );
void board_scroll(board_t *board);
void board_cursor_left (board_t *board);
void board_cursor_right (board_t *board);
void board_cursor_up (board_t *board);
void board_cursor_down (board_t *board);
void board_swap_cursor (board_t *board);
void board_find_matches(board_t *board, resources_t *resources);
void board_fall_bricks(board_t *board);
void board_erase_bricks(board_t *board);
void board_draw (SDL_Surface *screen, board_t *board, resources_t *resources, int x, int y);
int board_think(board_t *board, resources_t *resources, int ticks);

int load_resources (resources_t *resources);
void free_resources (resources_t *resources);
SDL_Surface *screen_init();
int input_process(SDL_Surface *screen, board_t *board, resources_t *resources);

void displaySurface( int x, int y, SDL_Surface* source, SDL_Surface* destination, SDL_Rect* clip)
{
    //Holds offsets
    SDL_Rect offset;

    //Get offsets
    offset.x = x;
    offset.y = y;

    //Blit
    SDL_BlitSurface( source, clip, destination, &offset );
}

int displayMessage(TTF_Font *font, const char* text, int x, int y, SDL_Surface* screen) {
	SDL_Surface *message = NULL;
	
    //Render the text
    message = TTF_RenderText_Blended( font, text, whiteColor );

    //If there was an error in rendering the text
    if( message == NULL )
    {
        return 1;
    }

    //Show the message on the screen
    displaySurface( x, y, message, screen, NULL);

    //Free the message
    SDL_FreeSurface( message );

	return 0;
}

board_t *board_new() 
{
	board_t *board;
	board = malloc(sizeof(board_t));

	board_init(board);

	return board;
}

void board_init(board_t *board)
{
	board->state = STATE_PAUSED;
	board_clear(board);
	board->curs_x = 0;
	board->curs_y = 0;
	board->rise_time = 10000;
	board->next_rise_time = 10000;
	board->pause_time = 0;
	board->score = 0;
}

void board_fill (board_t *board, int height)
{
	int i, j;

	if (height <= BOARD_HEIGHT) {
		for(i=0;i<height;i++) {
			for(j=0;j<BOARD_WIDTH;j++) {
				board->bricks[i][j] = (rand() % NUM_BRICK_TYPES) + 1;
			}
		}
	}
}

void board_clear ( board_t *board )
{
	int i, j;

	for ( i=0; i<BOARD_HEIGHT; i++ ) {
		for ( j=0; j<BOARD_WIDTH; j++ ) {
			board->bricks[i][j] = 0;
			board->lit[i][j] = 0;
			board->standing[i][j] = 0;
		}
	}
}

void board_scroll (board_t *board)
{
	int i, j;
	
	/* check if there are bricks on the top line. if so, game over */
	for(i=0;i<BOARD_WIDTH;i++) {
		if(board->bricks[BOARD_HEIGHT-1][i]) {
			board->state = STATE_GAMEOVER;
			return;
		}
	}
	
	/* move bricks up one line */
	for (i=BOARD_HEIGHT-1;i>0;i--) {
		for (j=0;j<BOARD_WIDTH;j++) {
			board->bricks[i][j] = board->bricks[i-1][j];
			board->lit[i][j] = board->lit[i-1][j];
			board->standing[i][j] = board->standing[i-1][j];
		}
	}
	if (board->curs_y < BOARD_HEIGHT - 1) 
		board->curs_y += 1;
	
	/* create a new line of random bricks */
	for(i=0;i<BOARD_WIDTH;i++) {
		board->bricks[0][i] = (rand() % NUM_BRICK_TYPES) + 1;
		board->lit[0][i] = 0;
		board->standing[0][i] = 0;
	}
}

void board_cursor_left(board_t *board)
{
	if (board->curs_x > 0)
		board->curs_x--;
}

void board_cursor_right(board_t *board)
{
	if (board->curs_x < BOARD_WIDTH - 2)
		board->curs_x++;
}

void board_cursor_up(board_t *board)
{
	if(board->curs_y < BOARD_HEIGHT - 1)
		board->curs_y++;
}

void board_cursor_down(board_t *board)
{
	if(board->curs_y > 0)
		board->curs_y--;
}

void board_swap_cursor (board_t *board)
{
	int temp;

	int x, y;

	x = board->curs_x;
	y = board->curs_y;

	if (y >= 0 && y < BOARD_HEIGHT  && x >= 0 && x < BOARD_WIDTH-1 && !board->lit[y][x] && !board->lit[y][x+1]) {
		temp = board->bricks[y][x];
		board->bricks[y][x] = board->bricks[y][x+1];
		board->bricks[y][x+1] = temp;
		if(y>0) board->standing[y][x] = STAND_TIME_FALL;
		if(y>0) board->standing[y][x+1] = STAND_TIME_FALL;
	}
	board_fall_bricks(board);
}

void board_find_matches(board_t *board, resources_t *resources)
{
	int i, j, k;
	int count;
	int lit = 0;

	for(i=0;i<BOARD_HEIGHT;i++) {
		for(j=0;j<BOARD_WIDTH;j++) {
			if(board->bricks[i][j] && !board->lit[i][j]) {
				/* check for matches vertically */
				count = 0;
				if(i<BOARD_HEIGHT-2) {
					for(k=i+1;k<BOARD_HEIGHT;k++) {
						if((!board->lit[k][j]||board->lit[i][j]==LIGHT_TIME) && !board->standing[i][k] && board->bricks[k][j] == board->bricks[i][j]) {
							count++;
							if (count == 2) {
								board->lit[k][j] = LIGHT_TIME;
								board->lit[k-1][j] = LIGHT_TIME;
								board->lit[k-2][j] = LIGHT_TIME;
								lit++;
							} else if (count > 2) {
								board->lit[k][j] = LIGHT_TIME;
								lit++;
							}
						} else {
							break;
						}
					}
				}
				/* check for matches horizontally */
				count = 0;
				if(j<BOARD_WIDTH-2) {
					for(k=j+1;k<BOARD_WIDTH;k++) {
						if((!board->lit[i][k]||board->lit[i][k]==LIGHT_TIME) && !board->standing[i][k] && board->bricks[i][k] == board->bricks[i][j]) {
							count++;
							if (count == 2) {
								board->lit[i][k] = LIGHT_TIME;
								board->lit[i][k-1] = LIGHT_TIME;
								board->lit[i][k-2] = LIGHT_TIME;
								lit++;
							} else if (count > 2) {
								board->lit[i][k] = LIGHT_TIME;
								lit++;
							}
						} else {
							break;
						}
					}
				}
			}
		}
	}
	if (lit) {
		board->state = STATE_PAUSED;
		board->pause_time += LIGHT_TIME * lit;
		board->score += 100 * lit * lit;
		printf("score: %d\n",board->score);
		Mix_PlayChannel( -1, resources->explode_sound, 0 );
	}
}

void board_process_lights(board_t *board, int ticks)
{
	int i, j;

	for(i=0;i<BOARD_HEIGHT;i++) {
		for(j=0;j<BOARD_WIDTH;j++) {
			if(board->lit[i][j]) {
				board->lit[i][j] -= ticks;
				if (board->lit[i][j] <= 0) {
					board->lit[i][j] = 0;
					board->bricks[i][j] = 0;
					board->standing[i][j] = STAND_TIME_BREAK;
				}
			}
		}
	}
}

void board_process_standing(board_t *board, int ticks)
{
	int i, j;

	for(i=0;i<BOARD_HEIGHT;i++) {
		for(j=0;j<BOARD_WIDTH;j++) {
			if(board->standing[i][j]) {
				board->standing[i][j] -= ticks;
				if(board->standing[i][j] <= 0) {
					board->standing[i][j] = 0;
				}
			}
		}
	}
}

void board_fall_bricks(board_t *board)
{
	int i, j;
	int dropped;

	do {
		dropped = 0;
		for(i=1;i<BOARD_HEIGHT;i++) {
			for(j=0;j<BOARD_WIDTH;j++) {
				if(board->bricks[i][j] && !board->bricks[i-1][j] && !board->lit[i][j] && !board->standing[i][j] && !board->standing[i-1][j]) {
					board->bricks[i-1][j] = board->bricks[i][j];
					board->bricks[i][j] = 0;
					dropped = 1;
				}
			}
		}
	} while (dropped);
}

void board_erase_bricks(board_t *board)
{
	int i, j;

	for(i=BOARD_HEIGHT-1;i>=0;i--) {
		for(j=0;j<BOARD_WIDTH;j++) {
			if(board->lit[i][j]) {
				board->lit[i][j] = 0;
				board->bricks[i][j] = 0;
			}
		}
	}
}

void board_draw (SDL_Surface *screen, board_t *board, resources_t *resources, int x, int y)
{
	int i, j;
	SDL_Rect dest;

	dest.x = 0; dest.y = 0;
	SDL_BlitSurface(resources->background,NULL,screen,&dest);
	
	for ( i=0; i<BOARD_HEIGHT; i++ ) {
		for ( j=0; j<BOARD_WIDTH; j++ ) {
			dest.x=(j*BRICK_WIDTH)+x;
			dest.y=((BOARD_HEIGHT-1)*BRICK_HEIGHT-(i*BRICK_HEIGHT))+y;
			SDL_BlitSurface(resources->brick_surf[board->bricks[i][j]], NULL, screen, &dest);
		}
	}
	for ( i=0; i<BOARD_HEIGHT; i++ ) {
		for ( j=0; j<BOARD_WIDTH; j++ ) {
			if(board->lit[i][j]) {
				dest.x=(j*BRICK_WIDTH)+x;
				dest.y=((BOARD_HEIGHT-1)*BRICK_HEIGHT-(i*BRICK_HEIGHT))+y;
				SDL_BlitSurface(resources->lightmask, NULL, screen, &dest);
			}
		}
	}

	dest.x = (board->curs_x * BRICK_WIDTH)+x;
	dest.y = (BOARD_HEIGHT-1)*BRICK_HEIGHT-(board->curs_y*BRICK_HEIGHT);
	SDL_BlitSurface(resources->cursor,NULL,screen,&dest);

	if (board->state == STATE_PAUSEGAME) {
		displayMessage(resources->font50, "Paused", 390, 260, screen);
	}
	
	if (board->state == STATE_GAMEOVER) {
		displayMessage(resources->font50, "Game Over", 355, 260, screen);
	}
	
	char message[30];
	sprintf(message,"Score: %d", board->score);
	displayMessage(resources->font30, message, 10, 10, screen);

	SDL_UpdateRect(screen, 0, 0, 0, 0);
}

int board_think (board_t *board, resources_t *resources, int ticks) 
{
	switch (board->state) {
		case STATE_RISING:
			board->rise_time -= ticks;
			if (board->rise_time <= 0) {
				board_scroll(board);
				board->rise_time = board->next_rise_time;
				board->next_rise_time *= 0.95;
			}
			break;
		case STATE_PAUSED:
			board->pause_time -= ticks;
			if(board->pause_time <= 0) {
				board->pause_time = 0;
				board->state = STATE_RISING;
			}
			break;
		case STATE_GAMEOVER:
			return 1;
	}
	board_process_lights(board,ticks);
	board_process_standing(board,ticks);
	board_fall_bricks(board);
	board_find_matches(board, resources);

	return 0;
}

int load_resources( resources_t *resources )
{
	int i;
	char buf[256];
	SDL_Surface *temp;
	Uint32 colorkey;

	for (i=0;i<NUM_BRICK_TYPES+1;i++) {
		sprintf(buf,"%s/brick%i.bmp",SRCDIR,i);
		if ((temp = SDL_LoadBMP(buf)) == NULL) {
			sprintf(buf,"%s/brick%i.bmp","bmp",i);
			if ((temp = SDL_LoadBMP(buf)) == NULL) {
				return 1;
			}
		}
		resources->brick_surf[i] = SDL_DisplayFormat(temp);
		SDL_FreeSurface(temp);
	}
	sprintf(buf,"%s/cursor.bmp",SRCDIR);
	if((temp = SDL_LoadBMP(buf)) == NULL) {
			sprintf(buf,"%s/cursor.bmp","bmp");
			if((temp = SDL_LoadBMP(buf)) == NULL) {
				return 1;
			}
	}
	resources->cursor = SDL_DisplayFormat(temp);
	colorkey = SDL_MapRGB(resources->cursor->format, 0, 0, 255);
	SDL_SetColorKey(resources->cursor, SDL_SRCCOLORKEY, colorkey);
	SDL_FreeSurface(temp);

	sprintf(buf,"%s/lightmask.bmp",SRCDIR);
	if((temp = SDL_LoadBMP(buf)) == NULL) {
			sprintf(buf,"%s/lightmask.bmp","bmp");
			if((temp = SDL_LoadBMP(buf)) == NULL) {
				return 1;
			}
	}
	resources->lightmask = SDL_DisplayFormat(temp);
	colorkey = SDL_MapRGB(resources->lightmask->format, 0, 0, 255);
	SDL_SetColorKey(resources->lightmask, SDL_SRCCOLORKEY, colorkey);
	SDL_FreeSurface(temp);

	sprintf(buf,"%s/background.bmp",SRCDIR);
	if((temp = SDL_LoadBMP(buf)) == NULL) {
			sprintf(buf,"%s/background.bmp","bmp");
			if((temp = SDL_LoadBMP(buf)) == NULL) {
				return 1;
			}
	}
	resources->background = SDL_DisplayFormat(temp);
	colorkey = SDL_MapRGB(resources->background->format, 0, 0, 255);
	SDL_SetColorKey(resources->background, SDL_SRCCOLORKEY, colorkey);
	SDL_FreeSurface(temp);

	sprintf(buf,"%s/Ubuntu-R.ttf",SRCDIR);
	resources->font30 = TTF_OpenFont(buf, 30 );
	if (resources->font30 == NULL) {
		return 1;
	}

	resources->font50 = TTF_OpenFont(buf, 50 );
	if (resources->font50 == NULL) {
		return 1;
	}

	sprintf(buf,"%s/cursor.wav",SRCDIR);
	resources->cursor_sound = Mix_LoadWAV(buf);
	if (resources->cursor_sound == NULL) {
		return 1;
	}
	
	sprintf(buf,"%s/explode.wav",SRCDIR);
	resources->explode_sound = Mix_LoadWAV(buf);
	if (resources->explode_sound == NULL) {
		return 1;
	}

	return 0;
}

void free_resources (resources_t *resources)
{
	int i;

	for (i=0;i<NUM_BRICK_TYPES+1;i++) {
		SDL_FreeSurface(resources->brick_surf[i]);
	}

	SDL_FreeSurface(resources->background);
	SDL_FreeSurface(resources->lightmask);
	SDL_FreeSurface(resources->cursor);
	
	TTF_CloseFont(resources->font30);
	TTF_CloseFont(resources->font50);
	
	Mix_FreeChunk(resources->cursor_sound);
	Mix_FreeChunk(resources->explode_sound);
}

SDL_Surface *screen_init()
{
	SDL_Surface *screen;

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_AUDIO)) {
		return NULL;
	}
	atexit(SDL_Quit);

	screen = SDL_SetVideoMode(SCREEN_WIDTH, SCREEN_HEIGHT, 32, 0);
	if (screen == NULL) {
		return NULL;
	}

	SDL_ShowCursor(SDL_DISABLE);

	//Initialize SDL_ttf
	if( TTF_Init() == -1 ) {
		return NULL;
	}

	//Initialize SDL_mixer
	if( Mix_OpenAudio( 44100, MIX_DEFAULT_FORMAT, 2, 4096 ) == -1 ) {
		return NULL;
	}

	return screen;
}

int input_process(SDL_Surface *screen, board_t *board, resources_t *resources)
{
	SDL_Event event;
	short playSound = 0;

	if (board->state == STATE_GAMEOVER) {
		while (SDL_PollEvent(&event) != 0) {
			switch (event.type) {
				case SDL_JOYBUTTONDOWN:
					switch(event.jbutton.button) {
						case VITA_BTN_START:
							board_init(board);
							board_fill(board, 6);
							board->state = STATE_RISING;
							return 0;
					}
			}
		}
		return 1;
	}
	else if (board->state == STATE_PAUSEGAME) {
		while (SDL_PollEvent(&event) != 0) {
			switch (event.type) {
				case SDL_JOYBUTTONDOWN:
					switch(event.jbutton.button) {
						case VITA_BTN_SELECT:
							board->state = STATE_RISING;
							return 0;
					}
			}
		}
		return 1;
	}	
	else {
		while (SDL_PollEvent(&event) != 0) {
			switch (event.type) {
				case SDL_JOYBUTTONDOWN:
					switch(event.jbutton.button) {
						case VITA_BTN_CROSS:
							board_swap_cursor(board);
							playSound = 1;
							break;
						case VITA_BTN_LTRIGGER:
						case VITA_BTN_RTRIGGER:
							board_scroll(board);
							break;
						case VITA_BTN_LEFT:
							board_cursor_left(board);
							playSound = 1;
							break;
						case VITA_BTN_RIGHT:
							board_cursor_right(board);
							playSound = 1;
							break;
						case VITA_BTN_UP:
							board_cursor_up(board);
							playSound = 1;
							break;
						case VITA_BTN_DOWN:
							board_cursor_down(board);
							playSound = 1;
							break;
						case VITA_BTN_SELECT:
							board->state = STATE_PAUSEGAME;
							break;
						default:
							break;
					}
					break;
			}

			if (playSound == 1) {
				Mix_PlayChannel( -1, resources->cursor_sound, 0 );
			}
		}
	}
	return 0;
}

int main (int argc, char **argv)
{
	SDL_Surface *screen;
	board_t *board;
	resources_t resources;
	int ticks, ticks_last, ticks_diff;
	SDL_Joystick *joystick;
	
	srand(time(NULL));

	screen = screen_init();
	joystick = SDL_JoystickOpen(0);
	if (screen == NULL) {
		exit(1);
	}
	if(load_resources(&resources)) {
		exit(1);
	}

	board = board_new();
	board_fill(board,6);

	ticks = SDL_GetTicks();
	board->state = STATE_RISING;
	while (1) {
		if(input_process(screen,board, &resources))
			continue;
		ticks_last = ticks;
		ticks = SDL_GetTicks();
		ticks_diff = ticks - ticks_last;
		board_think(board, &resources, ticks_diff);
		board_draw(screen,board,&resources,345, 0);
	}

	free_resources(&resources);
	free(board);
	SDL_JoystickClose(joystick);
	SDL_Quit();

	return 0;
}
