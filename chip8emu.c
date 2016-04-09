#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>
#include <assert.h>

#include <SDL2/SDL.h>

#define die(fmt, args...) do { fprintf(stderr, fmt, ##args); exit(1); } while(0)

#define DISPLAY_MEM	0xf00
#define STACK_MEM	0xea0
#define PROGRAM_MEM	0x200
#define SURFACE_WIDTH	640
#define SURFACE_HEIGHT	320
#define SCREEN_WIDTH	64
#define SCREEN_HEIGHT	32

struct chip8_video {
	uint32_t bgcolor;
	uint32_t fgcolor;
	SDL_Surface *screen;
	SDL_Surface *surface;
	SDL_Window *window;
};

struct chip8_state {
	uint8_t v[16];

	uint16_t ip;
	uint16_t mp;
	int16_t sp;

	uint8_t dt;
	uint8_t st;

	uint8_t key[16];

	struct chip8_video video;

	uint16_t *stack;
	uint8_t mem[4096];
};

static struct chip8_state chip8;
static struct chip8_state *c8 = &chip8;
static struct chip8_video *c8v = &chip8.video;

static void chip8_video_init(void)
{
	SDL_Init(SDL_INIT_VIDEO);
	c8v->window = SDL_CreateWindow("chip8 emulator", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SURFACE_WIDTH, SURFACE_HEIGHT, SDL_WINDOW_SHOWN);
	c8v->surface = SDL_GetWindowSurface(c8v->window);
	c8v->screen = SDL_CreateRGBSurface(0, 64, 32, 32, 0xff, 0xff00, 0xff0000, 0xff000000);
	assert(c8v->window);
	assert(c8v->surface);
	assert(c8v->screen);
	c8v->bgcolor = SDL_MapRGB(c8v->screen->format, 0x00, 0x00, 0x00);
	c8v->fgcolor = SDL_MapRGB(c8v->screen->format, 0xff, 0xff, 0xff);
}
static void chip8_video_close(void)
{
	SDL_DestroyWindow(c8v->window);
	SDL_FreeSurface(c8v->screen);
	SDL_Quit();
}
static void chip8_dump(void)
{
	printf("CHIP8 State\n");
	printf("IP 0x%x MP 0x%x SP 0x%x DT %d ST %d\n",
		c8->ip, c8->mp, c8->sp, c8->dt, c8->st);
	printf("V0 %d V1 %d V2 %d V3 %d V4 %d V5 %d V6 %d V7 %d\n",
		c8->v[0], c8->v[1], c8->v[2], c8->v[3], c8->v[4], c8->v[5], c8->v[6], c8->v[7]);
	printf("V8 %d V9 %d VA %d VB %d VC %d VD %d VE %d VF %d \n",
		c8->v[8], c8->v[9], c8->v[0xa], c8->v[0xb], c8->v[0xc], c8->v[0xd], c8->v[0xe], c8->v[0xf]);
}
static void chip8_init(void)
{
	uint8_t fonts[] = {
		0xF0, 0x90, 0x90, 0x90, 0xF0,  /* 0 */
		0x20, 0x60, 0x20, 0x20, 0x70,  /* 1 */
		0xF0, 0x10, 0xF0, 0x80, 0xF0,  /* 2 */
		0xF0, 0x10, 0xF0, 0x10, 0xF0,  /* 3 */
		0x90, 0x90, 0xF0, 0x10, 0x10,  /* 4 */
		0xF0, 0x80, 0xF0, 0x10, 0xF0,  /* 5 */
		0xF0, 0x80, 0xF0, 0x90, 0xF0,  /* 6 */
		0xF0, 0x10, 0x20, 0x40, 0x40,  /* 7 */
		0xF0, 0x90, 0xF0, 0x90, 0xF0,  /* 8 */
		0xF0, 0x90, 0xF0, 0x10, 0xF0,  /* 9 */
		0xF0, 0x90, 0xF0, 0x90, 0x90,  /* A */
		0xE0, 0x90, 0xE0, 0x90, 0xE0,  /* B */
		0xF0, 0x80, 0x80, 0x80, 0xF0,  /* C */
		0xE0, 0x90, 0x90, 0x90, 0xE0,  /* D */
		0xF0, 0x80, 0xF0, 0x80, 0xF0,  /* E */
		0xF0, 0x80, 0xF0, 0x80, 0x80, /* F */
	};

	chip8_video_init();

	c8->ip = PROGRAM_MEM;
	c8->mp = 0;
	c8->sp = 0;
	c8->stack = (uint16_t *)&c8->mem[STACK_MEM];

	memcpy(c8->mem, fonts, sizeof(fonts));
}
static void chip8_close(void)
{
	chip8_video_close();
	chip8_dump();
	exit(1);
}
static void chip8_video_clearscreen(void)
{
	SDL_Rect rect;
	rect.w = SCREEN_WIDTH;
	rect.h = SCREEN_HEIGHT;
	rect.x = 0;
	rect.y = 0;
	SDL_FillRect(c8v->screen, &rect, c8v->bgcolor);	
	rect.x = 0;
	rect.y = 0;
	rect.w = SURFACE_WIDTH;
	rect.h = SURFACE_HEIGHT;
	SDL_BlitScaled(c8v->screen, NULL, c8v->surface, &rect);
	SDL_UpdateWindowSurface(c8v->window);
}
static int chip8_video_draw(uint8_t *s, uint8_t x, uint8_t y, uint8_t n)
{
	int i, j;
	int wrap = 0;
	int collision = 0;
	uint32_t *pixel;
	SDL_Rect rect;

	if ((y + n > c8v->screen->h) || (x + 8 > c8v->screen->w))
		wrap = 1;
	else
		wrap = 0;

	for (i = 0; i < n; i++) {
		for (j = 0; j < 8; j++) {
			if ((0x80>>j) & s[i]) {
				pixel = (uint32_t*)c8v->screen->pixels + ((y+i)%c8v->screen->h) * c8v->screen->w + (x+j)%c8v->screen->w;
				if (*pixel == c8v->fgcolor) {
					collision = 1;
					*pixel = c8v->bgcolor;
				}
				else {
					*pixel = c8v->fgcolor;
				}
			}
		}
	}

	wrap = 1;
	if (wrap) {
		rect.x = 0;
		rect.y = 0;
		rect.w = SURFACE_WIDTH;
		rect.h = SURFACE_HEIGHT;
		SDL_BlitScaled(c8v->screen, NULL, c8v->surface, &rect);
		SDL_UpdateWindowSurface(c8v->window);
	} else {
		rect.x = x;
		rect.y = y;
		rect.w = 8;
		rect.h = n;
		SDL_UpdateWindowSurfaceRects(c8v->window, &rect, 1);
	}
	return collision;
}
/* 0xff means no key pressed */
static uint8_t chip8_video_key_wait(void)
{
	uint8_t i;

	for (i = 0; i < 0x0f; i++) {
		if (c8->key[i])
			return i;
	}
	return 0xff;
}
static void chip8_video_key_process(void)
{
	int key;
	SDL_Event e;

	while (SDL_PollEvent(&e)) {
		if (e.type == SDL_QUIT)
			chip8_close();

		if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
			switch (e.key.keysym.sym) {
				case SDLK_0:
				case SDLK_1:
				case SDLK_2:
				case SDLK_3:
				case SDLK_4:
				case SDLK_5:
				case SDLK_6:
				case SDLK_7:
				case SDLK_8:
				case SDLK_9:
					key = e.key.keysym.sym - SDLK_0;
					break;
				case SDLK_a:
				case SDLK_b:
				case SDLK_c:
				case SDLK_d:
				case SDLK_e:
				case SDLK_f:
					key = e.key.keysym.sym - SDLK_a + 10;
					break;
				case SDLK_ESCAPE:
					chip8_close();
					break;
				default:
					return;
					break;
			}
			if (e.type == SDL_KEYDOWN)
				c8->key[key] = 1;
			else
				c8->key[key] = 0;
		}
	}
}

#define opC	((op>>12)&0x000f)
#define opX	((op>>8)&0x000f)
#define opY	((op>>4)&0x000f)
#define opN	(op&0x000f)
#define opNN	(op&0x00ff)
#define opNNN	(op&0x0fff)

static uint16_t rand16(void)
{
	return (uint16_t)(random() & 0xffff);
}

static void clear_screen(void)
{
	chip8_video_clearscreen();
}
static void sub_call(uint16_t addr)
{
	c8->stack[c8->sp] = c8->ip + 2;
	c8->sp += 2;
	c8->ip = addr;
}
static void sub_return(void)
{
	c8->sp -= 2;
	assert(c8->sp >= 0);
	c8->ip = c8->stack[c8->sp];
}
static uint8_t wait_input(void)
{
	return chip8_video_key_wait();
}
static int op0(uint16_t op)
{
	switch (opNNN) {
		case 0x0e0:
			clear_screen();
			break;
		case 0x0ee:
			sub_return();
			return 0;
			break;
		default:
			die("bad op0: %04x\n", op);
			break;
	};

	return 1;
}

static int op1(uint16_t op)
{
	c8->ip = opNNN;
	return 0;
}

static int op2(uint16_t op)
{
	sub_call(opNNN);
	return 0;
}

static int op3(uint16_t op)
{
	if (c8->v[opX] == opNN) {
		return 2;
	}
	return 1;
}

static int op4(uint16_t op)
{
	if (c8->v[opX] != opNN)
		return 2;
	return 1;
}

static int op5(uint16_t op)
{
	assert(opN == 0);
	if (c8->v[opX] == c8->v[opY]) {
		return 2;
	}
	return 1;
}

static int op6(uint16_t op)
{
	c8->v[opX] = opNN;
	return 1;
}

static int op7(uint16_t op)
{
	c8->v[opX] += opNN;
	return 1;
}

static int op8(uint16_t op)
{
	uint8_t *vx, *vy, *vf;

	vx = &c8->v[opX];
	vy = &c8->v[opY];
	vf = &c8->v[0xf];

	switch (opN) {
		case 0:
			*vx = *vy;
			break;
		case 1:
			*vx |= *vy;
			break;
		case 2:
			*vx &= *vy;
			break;
		case 3:
			*vx ^= *vy;
			break;
		case 4:
			*vx += *vy;
			/* carry ? */
			if (*vx < *vy) {
				*vf= 1;
			}
			else {
				*vf = 0;
			}
			break;
		case 5:
			/* borrow ? */
			if (*vx < *vy) {
				*vf = 1;
			} else {
				*vf = 0;
			}
			*vx -= *vy;
			break;
		case 6:
			*vf = (*vx & 0x1);
			*vx = (*vx >> 1);
			break;
		case 7:
			/* borrow ? */
			if (*vy < *vx) {
				*vf = 1;
			} else {
				*vf = 0;
			}
			*vx = *vy - *vx;
			break;
		case 0xE:
			*vf = ((*vx & 0x80) >> 7);
			*vx = (*vx << 1);
			break;
		default:
			die("bad op8: %04x\n", op);
			break;
	}
	return 1;
}

static int op9(uint16_t op)
{
	assert(opN == 0);
	if (c8->v[opX] != c8->v[opY])
		return 2;
	return 1;
}

static int opa(uint16_t op)
{
	c8->mp = opNNN;
	return 1;
}

static int opb(uint16_t op)
{
	c8->ip = c8->v[0] + opNNN;
	return 0;
}

static int opc(uint16_t op)
{
	c8->v[opX] = opNN & rand16();
	return 1;
}

static int opd(uint16_t op)
{
	c8->v[0xf] = chip8_video_draw(&c8->mem[c8->mp], c8->v[opX], c8->v[opY], opN);
	return 1;
}

static int ope(uint16_t op)
{
	uint8_t key = c8->v[opX];

	switch (opNN) {
		case 0x9e:
			if (c8->key[key] != 0) {
				return 2;
			}
			break;
		case 0xa1:
			if (c8->key[key] == 0) {
				return 2;
			}
			break;
		default:
			die("bad ope: %04x\n", op);
			break;
	}
	
	return 1;
}

static int opf(uint16_t op)
{
	uint8_t *vx = &c8->v[opX];

	switch (opNN) {
		case 0x07:
			*vx = c8->dt;
			break;
		case 0x0a:
			*vx = wait_input();
			/*if no key was pressed, redo this instruction */
			if (*vx == 0xff)
				c8->ip -= 2;
			break;
		case 0x15:
			c8->dt = *vx;
			break;
		case 0x18:
			c8->st = *vx;
			break;
		case 0x1e:
			c8->mp += *vx;
			break;
		case 0x29:
			c8->mp = (*vx) * 5;
			break;
		case 0x33:
			c8->mem[c8->mp] = (*vx) / 100;
			c8->mem[c8->mp + 1] = ((*vx) % 100) / 10;
			c8->mem[c8->mp + 2] = (*vx) % 10;
			break;
		case 0x55:
			{
				int i;
				for (i = 0; i <= opX; i++) {
					c8->mem[c8->mp + i] = c8->v[i];
				}
			}
			break;
		case 0x65:
			{
				int i;
				for (i = 0; i <= opX; i++) {
					c8->v[i] = c8->mem[c8->mp + i];
				}
			}
			break;
		default:
			die("bad opf: %04x\n", op);
			break;
	}
	return 1;
}

typedef int (*op_fun_t) (uint16_t);
static op_fun_t optables[] = {
	op0, op1, op2, op3, op4, op5, op6, op7,
	op8, op9, opa, opb, opc, opd, ope, opf,
};

static void chip8_decode(uint16_t op)
{
	uint16_t n;
       
	/*
	 * c8->ip += 2 * optables[opC](op);
	 * This is wrong because op_fun may modify c8->ip !
	 */
	n = optables[opC](op);
	c8->ip += 2 * n;
}

static void load_prog(const char *file)
{
	FILE * fp;
	void *mem;
	int rc;

	assert(file);

	fp = fopen(file, "r");
	assert(fp);
	mem = &c8->mem[PROGRAM_MEM];
	while (!feof(fp)) {
		rc = fread(mem, 1, 256, fp);
		if (rc == 0) {
			if (ferror(fp)) {
				die("cannot load program\n");
			} else {	/* eof ? */
				//die("strange\n");
				break;
			}
		}
		mem += rc;
	}
	fclose(fp);
}
int main(int argc, char **argv)
{
	int i;
	uint32_t last, now;

	assert(argc == 2);

	srandom(time(NULL));

	chip8_init();

	load_prog(argv[1]);

	last = SDL_GetTicks();
	while (1) {
		chip8_video_key_process();
		chip8_decode(ntohs(*(uint16_t*)&c8->mem[c8->ip]));
		if ((now = SDL_GetTicks()) - last >= 15) {
			if (c8->dt > 0)
				c8->dt--;
			if (c8->st > 0)
				c8->st--;
			last = now;
		}

	}

	chip8_close();

	return 0;
}
