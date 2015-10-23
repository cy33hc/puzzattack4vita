#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <psp2/rtc.h>
#include <psp2/ctrl.h>
#include <vita2d.h>

#include "vita_audio.h"
#include "brick0.h"
#include "brick1.h"
#include "brick2.h"
#include "brick3.h"
#include "brick4.h"
#include "brick5.h"
#include "brick6.h"
#include "cursor.h"
#include "lightmask.h"
#include "background.h"
#include "font.h"
#include "cursor_sound.h"
#include "pop_sound.h"

#define SCREEN_WIDTH 270
#define SCREEN_HEIGHT 540

#define BOARD_HEIGHT 12
#define BOARD_WIDTH 6

#define BRICK_WIDTH 45
#define BRICK_HEIGHT 45

#define NUM_BRICK_TYPES 6

#define STATE_PAUSED 0
#define STATE_RISING 1
#define STATE_GAMEOVER 2
#define STATE_PAUSEGAME 3

#define LIGHT_TIME 20
#define STAND_TIME_BREAK 5
#define STAND_TIME_FALL 2

#define BLACK   RGBA8(  0,   0,   0, 255)
#define WHITE   RGBA8(255, 255, 255, 255)
#define GREEN   RGBA8(  0, 255,   0, 255)
#define RED     RGBA8(255,   0,   0, 255)
#define BLUE    RGBA8(  0,   0, 255, 255)

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

typedef struct resource_t {
	vita2d_texture *brick_surf[NUM_BRICK_TYPES+1];
	vita2d_texture *cursor;
	vita2d_texture *lightmask;
	vita2d_texture *background;
	vita2d_font *font;
}resource_t;

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
void board_find_matches(board_t *board);
void board_fall_bricks(board_t *board);
void board_erase_bricks(board_t *board);
void board_draw (board_t *board, resource_t *resource, int x, int y);
int board_think(board_t *board, uint64_t ticks);

int load_data (resource_t *resource);
void free_data (resource_t *resource);
void input_process(board_t *board, resource_t *resource, SceCtrlData *pad, SceCtrlData *old_pad);

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
	board->rise_time = 1000;
	board->next_rise_time = 1000;
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

void board_find_matches(board_t *board)
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
		pspAudioOutput((void *)pop_sound, pop_sound_size);
	}
}

void board_process_lights(board_t *board, uint64_t ticks)
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

void board_process_standing(board_t *board, uint64_t ticks)
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

void board_draw (board_t *board, resource_t *resource, int x, int y)
{
	int i, j;
	int dx,dy;

	vita2d_start_drawing();
	vita2d_clear_screen();

	vita2d_draw_texture(resource->background, 0, 0);

	for ( i=0; i<BOARD_HEIGHT; i++ ) {
		for ( j=0; j<BOARD_WIDTH; j++ ) {
			dx=(j*BRICK_WIDTH)+x;
			dy=((BOARD_HEIGHT-1)*BRICK_HEIGHT-(i*BRICK_HEIGHT))+y;
			vita2d_draw_texture(resource->brick_surf[board->bricks[i][j]], dx, dy);
		}
	}
	for ( i=0; i<BOARD_HEIGHT; i++ ) {
		for ( j=0; j<BOARD_WIDTH; j++ ) {
			if(board->lit[i][j]) {
				dx=(j*BRICK_WIDTH)+x;
				dy=((BOARD_HEIGHT-1)*BRICK_HEIGHT-(i*BRICK_HEIGHT))+y;
				vita2d_draw_texture(resource->lightmask, dx, dy);
			}
		}
	}

	dx = (board->curs_x * BRICK_WIDTH)+x;
	dy = ((BOARD_HEIGHT-1)*BRICK_HEIGHT-(board->curs_y*BRICK_HEIGHT))+y;
	vita2d_draw_texture(resource->cursor, dx, dy);

	if (board->state == STATE_GAMEOVER) {
		vita2d_font_draw_text(resource->font, 370, 260, WHITE, 50, "Game Over");
	}
	else if (board->state == STATE_PAUSEGAME) {
		vita2d_font_draw_text(resource->font, 400, 260, WHITE, 50, "Paused");
	}

	char str[22];
	sprintf(str,"Score: %d", board->score);
	vita2d_font_draw_text(resource->font, 10, 10, WHITE, 35, str);

	vita2d_end_drawing();
	vita2d_swap_buffers();
}

int board_think (board_t *board, uint64_t ticks) 
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
	board_find_matches(board);

	return 0;
}

int load_data( resource_t *resource )
{
	resource->brick_surf[0] = vita2d_load_PNG_buffer(brick0_png);
	resource->brick_surf[1] = vita2d_load_PNG_buffer(brick1_png);
	resource->brick_surf[2] = vita2d_load_PNG_buffer(brick2_png);
	resource->brick_surf[3] = vita2d_load_PNG_buffer(brick3_png);
	resource->brick_surf[4] = vita2d_load_PNG_buffer(brick4_png);
	resource->brick_surf[5] = vita2d_load_PNG_buffer(brick5_png);
	resource->brick_surf[6] = vita2d_load_PNG_buffer(brick6_png);
	resource->cursor = vita2d_load_PNG_buffer(cursor_png);
	resource->lightmask = vita2d_load_PNG_buffer(lightmask_png);
	resource->background = vita2d_load_PNG_buffer(background_png);
	resource->font = vita2d_load_font_mem(basicfont, basicfont_size);

	return 0;
}

void free_data (resource_t *resource)
{
	int i;

	for (i=0;i<NUM_BRICK_TYPES+1;i++) {
		vita2d_free_texture(resource->brick_surf[i]);
	}
	vita2d_free_texture(resource->cursor);
	vita2d_free_texture(resource->lightmask);
	vita2d_free_texture(resource->background);
	vita2d_free_font(resource->font);
}

void input_process(board_t *board, resource_t *resource, SceCtrlData *pad, SceCtrlData *old_pad)
{
    if (board->state == STATE_GAMEOVER) return;

	unsigned int keys_down = pad->buttons & ~old_pad->buttons;

	if (keys_down & SCE_CTRL_UP)
	{
		board_cursor_up(board);
		pspAudioOutput((void *)cursor_sound, cursor_sound_size);
	}
	else if (keys_down & SCE_CTRL_DOWN)
	{
		board_cursor_down(board);
		pspAudioOutput((void *)cursor_sound, cursor_sound_size);
	}
	else if (keys_down & SCE_CTRL_LEFT)
	{
		board_cursor_left(board);
		pspAudioOutput((void *)cursor_sound, cursor_sound_size);
	}
	else if (keys_down & SCE_CTRL_RIGHT)
	{
		board_cursor_right(board);
		pspAudioOutput((void *)cursor_sound, cursor_sound_size);
	}
	else if (keys_down & SCE_CTRL_CROSS)
	{
		board_swap_cursor(board);
		pspAudioOutput((void *)cursor_sound, cursor_sound_size);
	}
	else if (keys_down & SCE_CTRL_LTRIGGER || keys_down & SCE_CTRL_RTRIGGER)
	{
		board_scroll(board);
	}
}

int main (int argc, char **argv)
{
	board_t *board;
	resource_t resource;
	uint64_t ticks, ticks_last, ticks_diff;

	srand(time(NULL));

	vita2d_init();
	pspAudioInit(-1, 1);

	load_data(&resource);

	board = board_new();
	board_fill(board,6);

	sceRtcGetCurrentTick(&ticks);

	uint64_t res = sceRtcGetTickResolution();
	uint64_t ticks_per_sec = res / 60;
	SceCtrlData pad, old_pad;
	memset(&pad, 0, sizeof(pad));
	memset(&old_pad, 0, sizeof(old_pad));

	board->state = STATE_RISING;
	while (1) {
		sceCtrlPeekBufferPositive(0, &pad, 1);
		unsigned int keys_down = pad.buttons & ~old_pad.buttons;		

		if (board->state == STATE_GAMEOVER)
		{
			if (keys_down & SCE_CTRL_START) {
				board_init(board);
				board_fill(board, 6);
				sceRtcGetCurrentTick(&ticks);
				board->state = STATE_RISING;
			}
			else
			{
				continue;
			}
		}
		if (board->state == STATE_PAUSEGAME) {
			if (keys_down & SCE_CTRL_SELECT) {
				sceRtcGetCurrentTick(&ticks);
				board->state = STATE_RISING;
			} else {
				continue;
			}
		} else if (keys_down & SCE_CTRL_SELECT) {
			board->state = STATE_PAUSEGAME;
			board_draw(board,&resource, 345, 0);
			unsigned int retTime = time(0) + 2;
			while (time(0) < retTime);
			continue;
		}

		input_process(board, &resource, &pad, &old_pad);
		old_pad = pad;

		ticks_last = ticks;
		sceRtcGetCurrentTick(&ticks);
		ticks_diff = (ticks - ticks_last) / ticks_per_sec; 
		if(board_think(board, ticks_diff)) {
			break;
		}

		board_draw(board,&resource, 345, 0);
	}

	vita2d_fini();
	pspAudioShutdown();
	free_data(&resource);
	free(board);

	return 0;
}
