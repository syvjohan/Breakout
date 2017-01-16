#include <iostream>
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_mixer.h>
#include <SDL_ttf.h>

#include <time.h>
#include <cassert>

#include "MemAlloc.h"
#include "Vector.h"

#define TIME_STEP  ((1.0f / 60.0f) * 1000.f) // 60 fps.

struct Color
{
	float r;
	float g;
	float b;
	float a;
};

//******
//TIMER START
//******
struct Timer
{
	Uint64 Tick;
	Uint64 PreviousTick;
	Uint64 Frequency;
};

inline void TimerInit(Timer *t)
{
	t->Frequency = SDL_GetPerformanceFrequency();
	t->Tick = t->PreviousTick = SDL_GetPerformanceCounter();
}

inline float TimerDeltaMs(const Timer *t)
{
	return ((float)(t->Tick - t->PreviousTick) / (float)t->Frequency) * 1000.0f;
}

inline void TimerTick(Timer *t)
{
	t->PreviousTick = t->Tick;
	t->Tick = SDL_GetPerformanceCounter();
}
//******
//TIMER END
//******

//******
//MENU START
//******

//Game states
enum GameState
{
	GAMESTATE_NONE = 0,
	GAMESTATE_MENU,
	GAMESTATE_PLAY,
	GAMESTATE_GAME_OVER,
	GAMESTATE_NEXT_LEVEL,
	GAMESTATE_COMPLETED_GAME,
} currentGameState;

bool gameIsStarted;

//Menu states
enum MenuState
{
	MENUSTATE_NONE = 0,
	MENUSTATE_NEW_GAME,
	MENUSTATE_CONTINUE,
	MENUSTATE_INSTRUCTION,
	MENUSTATE_INSTRUCTIONS,
	MENUSTATE_BACK,
	MENUSTATE_EXIT,
} currentMenuState;

struct Menu
{
#define MENU_OFFSET_Y 40

	struct Items
	{
		SDL_Texture *originTexture;
		SDL_Texture *shadowTexture;
		Vec2         size;
		Vec2         pos;
		bool         isHoovering;
	};

	Items title;
	Items newGame;
	Items continueGame;
	Items exitGame;
	Items instruction;
	Items instructions[5];

	Color originColor;
	Color shadowColor;

	Color goBackColor;
	Color goBackShadowColor;

	Color hooverColor;
	Color titleColor;
	Color shadowTitleColor;

	struct Background
	{
		SDL_Texture *texture;
		Vec2 frame;
		float timeToNextFrame;

#define MENU_BACKGROUND_PIXELS_PER_FRAME 1
#define MENU_BACKGROUND_TIME_BETWEEN_FRAMES (TIME_STEP * 2)

#define MENU_BACKGROUND_WIDTH  1920
#define MENU_BACKGROUND_HEIGHT 1080
	}background;

} menu;


//******
//MENU END
//******

#define MIN(X, Y) (X < Y) ? X : Y
#define MAX(X, Y) (X > Y) ? X : Y

#define ABS(X) X > 0 ? X : X *= -1

struct AxisBox
{
	Vec2 pos;
	Vec2 size;
};

struct CollisionResult
{
	bool  intersects;
	Vec2  normal;
	float length;
};

//******
//CollisionDetection
//SAT (Separating Axis Theorem)
//boxA is the one to adjust.
// Adjustment is done by adding collisionResult.normal * collisionResult.length to the position of boxA.
//******
CollisionResult CollisionDetection(const AxisBox &boxA, const AxisBox &boxB)
{
	const Vec2 xAxis = { 1, 0 };
	const Vec2 yAxis = { 0, 1 };

	const Vec2 boxAPoints[] = {
		{boxA.pos},
		{boxA.pos.x + boxA.size.x / 2.0f, boxA.pos.y},
		{boxA.pos.x + boxA.size.x / 2.0f, boxA.pos.y + boxA.size.y / 2.0f },
		{boxA.pos.x, boxA.pos.y + boxA.size.y / 2.0f }
	};

	const Vec2 boxBPoints[] = {
		{ boxA.pos },
		{ boxA.pos.x + boxA.size.x / 2.0f, boxA.pos.y },
		{ boxA.pos.x + boxA.size.x / 2.0f, boxA.pos.y + boxA.size.y / 2.0f },
		{ boxA.pos.x, boxA.pos.y + boxA.size.y / 2.0f }
	};

	float aMinX, aMaxX, aMinY, aMaxY;
	float bMinX, bMaxX, bMinY, bMaxY;

	aMinX = aMinY = bMinX = bMinY = FLT_MAX;
	aMaxX = aMaxY = bMaxX = bMaxY = FLT_MIN;

	for (int i = 0; i != 4; ++i)
	{
		float aX = boxAPoints[i].DotProduct(xAxis);
		float aY = boxAPoints[i].DotProduct(yAxis);

		float bX = boxBPoints[i].DotProduct(xAxis);
		float bY = boxBPoints[i].DotProduct(yAxis);
		
		aMinX = MIN(aX, aMinX);
		aMaxX = MAX(aX, aMaxX);
		aMinY = MIN(aY, aMinY);
		aMaxY = MAX(aY, aMaxY);

		bMinX = MIN(bX, bMinX);
		bMaxX = MAX(bX, bMaxX);
		bMinY = MIN(bY, bMinY);
		bMaxY = MAX(bY, bMaxY);
	}

	CollisionResult result;

	if (aMaxX < bMinX || aMinX > bMaxX || aMaxY < bMinY || aMinY > bMaxY)
	{
		result.intersects = false;
	}
	else
	{
		result.intersects = true;

		float minXPen, minYPen;

		float x0 = bMaxX - aMinX;
		float x1 = bMinX - aMaxX;
		float y0 = bMaxY - aMinY;
		float y1 = bMinY - aMaxY;

		if (ABS(x0) < ABS(x1))
		{
			minXPen = x0;
		}
		else
		{
			minXPen = x1;
		}

		if (ABS(y0) < ABS(y1))
		{
			minYPen = y0;
		}
		else
		{
			minYPen = y1;
		}

		if (ABS(minXPen) < ABS(minYPen))
		{
			result.normal = xAxis;
			result.length = minXPen;
		}
		else
		{
			result.normal = yAxis;
			result.length = minYPen;
		}
	}


	return result;
}

// ******
// CreateTextTextureFromFile
// Render text as ANSI(ascii).
// ******
SDL_Texture*
CreateTextTexture(SDL_Renderer *sdlRenderer, TTF_Font *font, Color &color, const char *message)
{
	SDL_Color c = {	color.r, color.g, color.b, color.a };

	SDL_Texture *result = NULL;

	SDL_Surface *surface = TTF_RenderText_Solid(font, message, c);
	if (surface && font)
	{
		result = SDL_CreateTextureFromSurface(sdlRenderer, surface);
		SDL_FreeSurface(surface);
	}

	assert(result);
	return result;
}

// ******
// LoadTextureFromFile
// ******
SDL_Texture*
LoadTextureFromFile(SDL_Renderer *renderer, const char *path)
{
	SDL_Texture *result = NULL;
	SDL_Surface *surface = IMG_Load(path);
	if (surface)
	{
		result = SDL_CreateTextureFromSurface(renderer, surface);
		SDL_FreeSurface(surface);
	}

	assert(result);
	return result;
}

//******
//SpriteDraw
//******
int SpriteDraw(SDL_Renderer *renderer, SDL_Texture *texture, Vec2 position, Vec2 size, Vec2 frame, Vec2 scale)
{
	SDL_Rect destRect = { position.ToIntX(), position.ToIntY(), size.ToIntX() * scale.ToIntX(),  size.ToIntY() * scale.ToIntY() };
	SDL_Rect srcRect  = { frame.ToIntX(), frame.ToIntY(), size.ToIntX(), size.ToIntY() };

	return SDL_RenderCopy(renderer, texture, &srcRect, &destRect);
}

//******
//DrawNotFilledRectangle
//******
void DrawNotFilledRectangle(SDL_Renderer *renderer, const Color &color, float x, float y, float w, float h)
{
	const SDL_Point points[] =
	{
		{ x, y },
		{ x + w, y },
		{ x + w, y + h },
		{ x, y + h },
		{ x, y }
	};

	SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
	assert(SDL_RenderDrawLines(renderer, points, 5) == 0);
}

//******
//DrawFilledRectangle
//******
void DrawFilledRectangle(SDL_Renderer *renderer, const Color &color, float x, float y, float w, float h)
{
	SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
	
	const SDL_Rect rect = { x, y, w, h };
	assert(SDL_RenderFillRect(renderer, &rect) == 0);
}

//******
//InRange
//******
float InRange(float min, float max) {
	return min + (max - min) * ((float)(rand() % 10001) / 10000.0f);
}

//******
//LoadSound
//******
Mix_Music* LoadSound(char *path)
{
	Mix_Music *sound = Mix_LoadMUS(path);
	assert(sound);
	return sound;
}

#define WINDOW_HEIGHT 720
#define WINDOW_WIDTH  1080

SDL_Window   *sdlWindow   = NULL;
SDL_Renderer *sdlRenderer = NULL;

//******
//PLAY START
//******

bool requestToMovePaddle = false;

SDL_Texture *spriteSheet;

TTF_Font *fontArial24;
TTF_Font *fontArial32;

Vec2 globalScale;

bool isLeftMouseBtnClicked;

const Vec2 shadowOffset = Vec2(1.0f, 1.0f);
#define OFFSET_BORDER_TEXTURES 10

//paddle
struct paddle
{
#define PADDLE_FRAME_SIZE 16
#define PADDLE_START_WIDTH PADDLE_FRAME_SIZE * 3
#define PADDLE_START_HEIGHT PADDLE_FRAME_SIZE

	Vec2  pos;
	Vec2  size;
	float maxWidth;

	float vel;
	float maxVel;
	float angle;
	int   dir;

}paddle; 

//Splitter
struct Splitter
{
	Vec2 pos;
	Vec2 size;

	Vec2 acc;
	Vec2 vel;

	Color color;

#define GRAVITY 9.80
};

#define BLOCK_TYPES 4
Color blockSplitterColor[BLOCK_TYPES];

//Explosion
struct Explosion
{
#define EXPLOSION_WIDTH  128.0f
#define EXPLOSION_HEIGHT 128.0f
#define EXPLOSION_MAX_FRAME_X 3
#define EXPLOSION_MAX_FRAME_Y 5

	Vec2 pos;

	Vec2 frame;
	float timeToNextFrame;
#define EXPLOSION_TIME_BETWEEN_FRAMES TIME_STEP * 3 //Change frame 20 times per second.
};
SDL_Texture *textureExplosion;

//Block
struct Block
{
#define BLOCK_WIDTH  32
#define BLOCK_HEIGHT 16

	Vec2 pos;

	int health;
	int type;

#define MAX_NUMBER_OF_SPLITTER 12
	Splitter blockSplitter[MAX_NUMBER_OF_SPLITTER];
	bool isSplitterActive;

	Explosion explosion;
	bool isExplosinActive;

} *blocks;

int blockMaxColumns;
int blockMaxRows;
int blockOffsetX;
int blockOffsetY;
int numberOfBlocks;

int numberOfActiveBLocks;

//Ball
struct Ball
{
#define BALL_WIDTH  8
#define BALL_HEIGHT 8

#define BALL_FRAME_X 0
#define BALL_FRAME_Y 48

	Vec2 pos;
	Vec2 vel;
	Vec2 maxVel;
}ball;

//Score text
struct ScoreTextures
{
#define MAX_TEXT_LENGTH 64
	char textPoints[MAX_TEXT_LENGTH];
	SDL_Texture *pointsTexture;
	SDL_Texture *pointsShadowTexture;
	bool requestUpdatePoints;

	char textLevel[MAX_TEXT_LENGTH];
	SDL_Texture *levelTexture;
	SDL_Texture *levelShadowTexture;
	bool requestUpdateLevel;

	Color originColor;
	Color shadowColor;

} scoreTextures;

//Score
struct Score
{
	int level;
	int points;

	float accumulator;

} score;

#define TIME_BETWEEN_LOWERING_BLOCKS 2000
float timeSinceABlockWasHited;

//Game over
struct GameOver
{
	SDL_Texture *originTexture;
	SDL_Texture *shadowTexture;

	Vec2 pos;
	Vec2 size;

	Color originColor;
	Color shadowColor;

#define TIME_TO_SHOW_GAME_OVER 4000
	float accumulator;

	SDL_Texture *backgroundTexture;
#define GAME_OVER_BACKGROUND_WIDTH  1920
#define GAME_OVER_BACKGROUND_HEIGHT 1080

} gameOver;


//Next level
struct NextLevel
{
	SDL_Texture *originTexture;
	SDL_Texture *shadowTexture;

	Vec2 pos;
	Vec2 size;

	Color originColor;
	Color shadowColor;

#define TIME_TO_SHOW_NEXT_LEVEL 2000
	float accumulator;

	SDL_Texture *backgroundTexture;
#define NEXT_LEVEL_BACKGROUND_WIDTH  1920
#define NEXT_LEVEL_BACKGROUND_HEIGHT 1080

} nextLevel;

//Level backgrounds
SDL_Texture *currentBackgroundLevelTexture;
#define LEVEL_BACKGROUND_WIDTH  1920
#define LEVEL_BACKGROUND_HEIGHT 1080

//Successfully ended game
struct CompletedGame
{
	SDL_Texture *originTexture;
	SDL_Texture *shadowTexture;

	Vec2 pos;
	Vec2 size;

	Color originColor;
	Color shadowColor;

#define TIME_TO_SHOW_COMPLETED_GAME 4000
	float accumulator;

	SDL_Texture *backgroundTexture;
#define COMPLETED_GAME_BACKGROUND_WIDTH  1920
#define COMPLETED_GAME_BACKGROUND_HEIGHT 1080

} completedGame;

float clickingCoolDownTimer;
#define COOLDOWNTIME TIME_STEP * 10

//Sounds
Mix_Music *explosionSound;
Mix_Music *hooveringInMenuSound;
Mix_Music *ballHitPaddleSound;
Mix_Music *ballhitBlockSound;

//******
//PLAY END
//******

//******
//UpdateMenuBackground
//******
void UpdateMenuBackground(float delta)
{
	//Scrolling background, moving as an rectangle.
	menu.background.timeToNextFrame -= delta;
	if (menu.background.timeToNextFrame <= 0)
	{
		menu.background.timeToNextFrame = MENU_BACKGROUND_TIME_BETWEEN_FRAMES;

		//Max y
		if (menu.background.frame.y * MENU_BACKGROUND_PIXELS_PER_FRAME > (MENU_BACKGROUND_HEIGHT - WINDOW_HEIGHT) && menu.background.frame.x != 0)
		{
			--menu.background.frame.x; //Third
		}
		//Max x
		else if (menu.background.frame.x * MENU_BACKGROUND_PIXELS_PER_FRAME > (MENU_BACKGROUND_WIDTH - WINDOW_WIDTH))
		{
			++menu.background.frame.y; //Second
		}
		//Min y
		else if (menu.background.frame.y == 0)
		{
			++menu.background.frame.x; //First
		}
		//Min X
		else if (menu.background.frame.x == 0)
		{
			--menu.background.frame.y; //Fourth
		}
	}
}

//*****
//GameUpdate
//*****
void GameUpdate(float delta)
{
	//******
	//GAMESTATE_PLAY
	//******
	if (currentGameState == GAMESTATE_PLAY)
	{
		//paddle: Movement.
		if (requestToMovePaddle)
		{
			if (paddle.vel < paddle.maxVel) paddle.vel += 0.7f;
		}
		else
		{
			(paddle.vel > 0) ? paddle.vel -= 0.9f : paddle.vel = 0.0f;
		}

		//Collisiondetection Paddle vs Window.
		if (paddle.pos.x < 0)
		{
			paddle.pos.x = 0;
			paddle.vel   = 0;
		}
		else if (paddle.pos.x + paddle.size.x > WINDOW_WIDTH)
		{
			paddle.pos.x = WINDOW_WIDTH - paddle.size.x;
			paddle.vel   = 0;
		}
		else
		{
			paddle.pos.x += paddle.vel * paddle.dir; //No collision, update movement.
		}

		//Ball: Movement.
		//Paddle has not fired ball.
		if (ball.vel.y == 0 && ball.vel.x == 0)
		{
			ball.pos = Vec2(paddle.pos.x + (paddle.size.x / 2 - (BALL_WIDTH / 2)), paddle.pos.y - BALL_HEIGHT);
		}
		//Movement for ball...
		else
		{
			ball.pos += ball.vel;
		}

		//Collisiondetection: Ball vs Blocks
		for (int i = 0; i != numberOfBlocks; ++i)
		{
			bool intersect = false;
			Block *b = &blocks[i];
			if (b->health != 0)
			{
				//Collision: bottom of block.
				if ((ball.pos.x < b->pos.x + BLOCK_WIDTH && ball.pos.x > b->pos.x) && (ball.pos.y < b->pos.y + BLOCK_HEIGHT && ball.pos.y > b->pos.y))
				{
					ball.pos.y = b->pos.y + BLOCK_HEIGHT;
					ball.vel.y = ball.maxVel.y;

					intersect = true;
				}
				//Collision: Up side of block.
				else if ((ball.pos.y + BALL_HEIGHT > b->pos.y && ball.pos.y < b->pos.y + BLOCK_HEIGHT) && (ball.pos.x < b->pos.x + BLOCK_WIDTH && ball.pos.x > b->pos.x))
				{
					ball.pos.y = b->pos.y - BALL_HEIGHT;
					ball.vel.y = -ball.maxVel.y;

					intersect = true;
				}
				//Collision: Left side of block
				else if ((ball.pos.x + BALL_WIDTH > b->pos.x && ball.pos.x < b->pos.x + BLOCK_WIDTH) && (ball.pos.y > b->pos.y && ball.pos.y < b->pos.y + BLOCK_HEIGHT))
				{
					ball.pos.x = b->pos.x - BALL_WIDTH;
					ball.vel.x = -ball.maxVel.x;

					intersect = true;
				}
				//Collision: Right side of block
				else if ((ball.pos.x < b->pos.x + BLOCK_WIDTH && ball.pos.x > b->pos.x) && (ball.pos.y > b->pos.y && ball.pos.y < b->pos.y + BLOCK_HEIGHT))
				{
					ball.pos.x = b->pos.x + BLOCK_WIDTH;
					ball.vel.x = ball.maxVel.x;

					intersect = true;
				}

				//Eval result
				if (intersect)
				{
					b->health == 1 ? b->health = 0 : b->health = 1;

					//If player ended block lifetime. Deactivate block and extend paddle length.
					if (b->health == 0)
					{
						//Extend length.
						if (paddle.size.x <= paddle.maxWidth)
						{
							paddle.size.x += PADDLE_FRAME_SIZE;
							paddle.pos.x  -= PADDLE_FRAME_SIZE / 2;
						}

						//Increase score.
						score.points += 1;

						--numberOfActiveBLocks;
						//Goto next level.
						if (numberOfActiveBLocks == 0)
						{
							score.level == 3 ? currentGameState = GAMESTATE_COMPLETED_GAME : currentGameState = GAMESTATE_NEXT_LEVEL;
						}

						//Lower block y speed.
						if (score.level == 3)
						{
							timeSinceABlockWasHited = 0.0f;
						}

						//Start explosion animation
						b->isExplosinActive = true;

						Mix_PlayMusic(explosionSound, 1); //Start explosion sound
						Mix_FadeOutMusic(1500);
					}
					else if (b->health == 1 && !b->isSplitterActive)
					{
						b->isSplitterActive = true; //activate splitter.

						if (!Mix_PlayingMusic()) Mix_PlayMusic(ballhitBlockSound, 1); //Play sound.
					}

					//Check if block is of type 1, in that case apply extra energy to ball.
					if (b->type == 0)
					{
						ball.vel.x > 0 ? ball.vel.x *= 2 : ball.vel.y *= 2;
					}

					break;
				}
			}
		}

		//Collisiondetection: Ball vs paddle
		if (ball.pos.y + BALL_HEIGHT > paddle.pos.y && paddle.pos.x < ball.pos.x + BALL_WIDTH && paddle.pos.x + paddle.size.x > ball.pos.x)
		{
			//Percentage.
			const float w = paddle.pos.x + paddle.size.x - (ball.pos.x + (BALL_WIDTH / 2));
			paddle.angle  = (w / paddle.size.x - 0.5f); // 0.5 -> -0.5

			if (paddle.angle < 0.1f && paddle.angle > -0.1f) paddle.angle = 0.0f; //Middle
			else paddle.angle > 0.1f ? paddle.angle -= 1.0 : paddle.angle += 1.0f; //Left or Right.

			ball.vel.x = ball.maxVel.x * paddle.angle;
			ball.vel.y = -ball.maxVel.y;

			Mix_HaltMusic(); //This wil cause issues, when blocks are closer paddle.
			if (!Mix_PlayingMusic())
			{
				Mix_PlayMusic(ballHitPaddleSound, 1); //Play sound.
			}
		}

		//Collisiondetection: Ball vs window sides
		//Bottom (Game over)
		if (ball.pos.y + BALL_HEIGHT > WINDOW_HEIGHT)
		{
			currentGameState = GAMESTATE_GAME_OVER;
		}
		//Top
		else if (ball.pos.y < 0)
		{
			ball.vel.y = ball.maxVel.y;
			ball.pos.y = 0;
		}
		//Right
		else if (ball.pos.x + BALL_WIDTH > WINDOW_WIDTH)
		{
			ball.vel.x = -ball.maxVel.x * paddle.angle;
			ball.pos.x = WINDOW_WIDTH - BALL_WIDTH;
		}
		//Left
		else if (ball.pos.x < 0)
		{
			ball.vel.x = ball.maxVel.x * -paddle.angle;
			ball.pos.x = 0;
		}

		//Things attached to a Block (Splitter, Explosion).
		for (int i = 0; i != numberOfBlocks; ++i)
		{
			//Splitter
			if (blocks[i].isSplitterActive)
			{
				for (int k = 0; k != MAX_NUMBER_OF_SPLITTER; ++k)
				{
					Splitter *s = &blocks[i].blockSplitter[k];
					if ( (s->pos.x + s->size.x) < WINDOW_WIDTH  && s->pos.x > 0 &&
						 (s->pos.y + s->size.y) < WINDOW_HEIGHT && s->pos.y > 0
					   ) 
					{		
						float deltaVel = delta * GRAVITY / 1000;

						s->pos.y += s->vel.y + (deltaVel / 2) * delta * s->acc.y;
						s->pos.x += s->vel.x + (deltaVel / 2) * delta * s->acc.x;

						s->vel.y += deltaVel;
					}
				}
			}
			//Explosion
			Explosion *e = &blocks[i].explosion;
			if (blocks[i].isExplosinActive)
			{
				e->timeToNextFrame -= delta;
				if (e->timeToNextFrame <= 0)
				{
					e->timeToNextFrame = EXPLOSION_TIME_BETWEEN_FRAMES;

					e->frame.x == EXPLOSION_MAX_FRAME_X ? e->frame.x = 0, ++e->frame.y : ++e->frame.x;

					if(e->frame.y == EXPLOSION_MAX_FRAME_Y) blocks[i].isExplosinActive = false; //Stop explosion animation.
				}
			}
		}

		//Textures
		//Update, Texture: Level.
		if (scoreTextures.requestUpdateLevel)
		{
			sprintf(scoreTextures.textLevel, "Level:  %d", score.level);

			//Remove old before creating new.
			SDL_DestroyTexture(scoreTextures.levelShadowTexture);
			SDL_DestroyTexture(scoreTextures.levelTexture);
			scoreTextures.levelShadowTexture = NULL;
			scoreTextures.levelTexture       = NULL;

			scoreTextures.levelTexture       = CreateTextTexture(sdlRenderer, fontArial24, scoreTextures.originColor, scoreTextures.textLevel); //Level: Origin
			scoreTextures.levelShadowTexture = CreateTextTexture(sdlRenderer, fontArial24, scoreTextures.shadowColor, scoreTextures.textLevel); //Level: Shadow
		}

		//Update,Texture: Points
		if (scoreTextures.requestUpdatePoints)
		{
			sprintf(scoreTextures.textPoints, "Score: %d", score.points);

			//Remove old before creating new.
			SDL_DestroyTexture(scoreTextures.pointsShadowTexture);
			SDL_DestroyTexture(scoreTextures.pointsTexture);
			scoreTextures.pointsShadowTexture = NULL;
			scoreTextures.pointsTexture       = NULL;

			scoreTextures.pointsTexture       = CreateTextTexture(sdlRenderer, fontArial24, scoreTextures.originColor, scoreTextures.textPoints); //Points: Origin
			scoreTextures.pointsShadowTexture = CreateTextTexture(sdlRenderer, fontArial24, scoreTextures.shadowColor, scoreTextures.textPoints); //Points: Shadow
		}

		//Level
		//Level: 1
		//Do nothing special.
		if (score.level == 1)
		{
			
		}
		//Level: 2
		else if (score.level == 2)
		{
			//Lower blocks (constant speed).
			score.accumulator += delta;
			if (score.accumulator > TIME_BETWEEN_LOWERING_BLOCKS)
			{
				score.accumulator = 0.0f;

				if (blocks[numberOfBlocks -1].pos.y > paddle.pos.y - (paddle.size.y * 6))
				{
					currentGameState = GAMESTATE_GAME_OVER;
				}
				else
				{
					for (int i = 0; i != numberOfBlocks; ++i)
					{
						blocks[i].pos.y += 10;
					}
				}
			}
		}
		//Level: 3
		else if (score.level == 3)
		{
			//Lower blocks (increase speed if failing to kill any block).
			score.accumulator += delta;
			if (score.accumulator > TIME_BETWEEN_LOWERING_BLOCKS)
			{
				score.accumulator = 0.0f;

				if (blocks[numberOfBlocks - 1].pos.y > paddle.pos.y - (paddle.size.y * 6))
				{
					currentGameState = GAMESTATE_GAME_OVER;
				}
				else
				{
					for (int i = 0; i != numberOfBlocks; ++i)
					{
						blocks[i].pos.y += 10;
					}
				}
			}

			//Increase speed.
			timeSinceABlockWasHited += delta;
			if (timeSinceABlockWasHited > (TIME_BETWEEN_LOWERING_BLOCKS * 2))
			{
				score.accumulator += delta;
			}
		}

	}
	//******
	//GAMESTATE_NEXT_LEVEL
	//******
	if (currentGameState == GAMESTATE_NEXT_LEVEL)
	{
		nextLevel.accumulator += delta;
		if (nextLevel.accumulator > TIME_TO_SHOW_NEXT_LEVEL)
		{
			score.level++;

			currentGameState = GAMESTATE_NONE;
			currentMenuState = MENUSTATE_NEW_GAME; //Goto next level.
		}
	}	
	//******
	//GAMESTATE_GAME_OVER
	//******
	if (currentGameState == GAMESTATE_GAME_OVER)
	{
		gameOver.accumulator += delta;
		if (gameOver.accumulator > TIME_TO_SHOW_GAME_OVER)
		{
			score.level = 1;

			currentGameState = GAMESTATE_MENU;
			currentMenuState = MENUSTATE_NONE;

			//Menu: Exit game.
			menu.exitGame.pos = { WINDOW_WIDTH / 2.0f - menu.exitGame.size.x / 2.0f, menu.newGame.pos.y + menu.newGame.size.y + MENU_OFFSET_Y };

			//Menu: Instruction
			menu.instruction.pos = { WINDOW_WIDTH / 2.0f - menu.instruction.size.x / 2.0f, menu.exitGame.pos.y + menu.exitGame.size.y + MENU_OFFSET_Y * 2 };

			gameIsStarted = false;
		}
	}
	//******
	//GAMESTATE_COMPLETED_GAME
	//******
	if (currentGameState == GAMESTATE_COMPLETED_GAME)
	{
		completedGame.accumulator += delta;
		if (completedGame.accumulator > TIME_TO_SHOW_COMPLETED_GAME)
		{
			score.level = 1;

			currentGameState = GAMESTATE_MENU;
			currentMenuState = MENUSTATE_NONE;

			//Menu: Exit game.
			menu.exitGame.pos = { WINDOW_WIDTH / 2.0f - menu.exitGame.size.x / 2.0f, menu.newGame.pos.y + menu.newGame.size.y + MENU_OFFSET_Y };

			//Menu: Instruction
			menu.instruction.pos = { WINDOW_WIDTH / 2.0f - menu.instruction.size.x / 2.0f, menu.exitGame.pos.y + menu.exitGame.size.y + MENU_OFFSET_Y * 2 };

			gameIsStarted = false;
		}
	}
	//******
	//GAMESTATE_MENU
	//******
	else if (currentGameState == GAMESTATE_MENU && currentMenuState == MENUSTATE_NONE)
	{
		//Scrolling background.
		UpdateMenuBackground(delta);

		if (clickingCoolDownTimer < 0)
		{
			clickingCoolDownTimer = 0;

			int x, y;
			SDL_GetMouseState(&x, &y);
			//Collisiondetection: Mouse vs Text Textures.
			//start new game
			if ((x > menu.newGame.pos.x && x < menu.newGame.pos.x + menu.newGame.size.x && (y > menu.newGame.pos.y && y < menu.newGame.pos.y + menu.newGame.size.y)))
			{
				if (isLeftMouseBtnClicked)
				{
					currentMenuState = MENUSTATE_NEW_GAME;
				}
				else
				{
					if (!Mix_PlayingMusic() && !menu.newGame.isHoovering) Mix_PlayMusic(hooveringInMenuSound, 1);
					menu.newGame.isHoovering = true;
				}
			}
			//continue game
			else if ((x > menu.continueGame.pos.x && x < menu.continueGame.pos.x + menu.continueGame.size.x && (y > menu.continueGame.pos.y && y < menu.continueGame.pos.y + menu.continueGame.size.y)))
			{
				if (isLeftMouseBtnClicked)
				{
					currentMenuState = MENUSTATE_CONTINUE;
				}
				else
				{
					if (!Mix_PlayingMusic() && !menu.continueGame.isHoovering) Mix_PlayMusic(hooveringInMenuSound, 1);
					menu.continueGame.isHoovering = true;
				}
			}
			//exit game
			else if ((x > menu.exitGame.pos.x && x < menu.exitGame.pos.x + menu.exitGame.size.x && (y > menu.exitGame.pos.y && y < menu.exitGame.pos.y + menu.exitGame.size.y)))
			{
				if (isLeftMouseBtnClicked)
				{
					currentMenuState = MENUSTATE_EXIT;
				}
				else
				{
					if (!Mix_PlayingMusic() && !menu.exitGame.isHoovering) Mix_PlayMusic(hooveringInMenuSound, 1);
					menu.exitGame.isHoovering = true;
				}
			}
			//instructions
			else if ((x > menu.instruction.pos.x && x < menu.instruction.pos.x + menu.instruction.size.x && (y > menu.instruction.pos.y && y < menu.instruction.pos.y + menu.instruction.size.y)))
			{
				if (isLeftMouseBtnClicked)
				{
					currentMenuState = MENUSTATE_INSTRUCTION;
				}
				else
				{
					if (!Mix_PlayingMusic() && !menu.instruction.isHoovering) Mix_PlayMusic(hooveringInMenuSound, 1);
					menu.instruction.isHoovering = true;
				}
			}
			else
			{
				menu.newGame.isHoovering = false;
				menu.continueGame.isHoovering = false;
				menu.exitGame.isHoovering = false;
				menu.instruction.isHoovering = false;
			}
		}
		else
		{
			clickingCoolDownTimer -= delta;
		}
	}
	//******
	//MENUSTATE_NEW_GAME
	//Initialize a new game
	//******
	else if (currentMenuState == MENUSTATE_NEW_GAME)
	{
		//Score
		score.accumulator = 0.0f;
		score.points      = 0;

		//Score textures
		scoreTextures.requestUpdatePoints = true;
		scoreTextures.requestUpdateLevel  = true;

		//Game over
		gameOver.accumulator = 0.0f;

		//Completed game
		completedGame.accumulator = 0.0f;

		//paddle
		paddle.pos   = Vec2((WINDOW_WIDTH / 2) - (paddle.size.x / 2), WINDOW_HEIGHT - PADDLE_FRAME_SIZE);
		paddle.size  = Vec2(PADDLE_START_WIDTH, PADDLE_START_HEIGHT);
		paddle.vel   = 0.0f;
		paddle.dir   = 0;
		paddle.angle = 0.0f;

		//Ball	
		ball.vel = Vec2(0.0f, 0.0f);

		//Level
		timeSinceABlockWasHited = 0.0f;

		//Next level
		nextLevel.accumulator = 0.0f;

		//Blocks
		const int length = blockMaxColumns * blockMaxRows -blockOffsetX;
		numberOfBlocks   = length - (length % blockMaxRows);

		blocks = DBG_NEW Block[numberOfBlocks];

		int x = blockOffsetX;
		int y = blockOffsetY;
		for (int i = 0; i != numberOfBlocks; ++i)
		{
			Block *b = &blocks[i];

			b->type   = rand() % 4;
			b->health = 2;

			//Block position
			if (x == blockMaxColumns)
			{
				++y;
				x = blockOffsetX;
			}
			b->pos = Vec2(x++ * BLOCK_WIDTH, y * BLOCK_HEIGHT);

			//Splitter
			b->isSplitterActive = false;
			for (int i = 0; i != MAX_NUMBER_OF_SPLITTER; ++i)
			{
				memcpy(&b->blockSplitter[i].color, &blockSplitterColor[b->type], sizeof(Color));
				memcpy(&b->blockSplitter[i].size,  &Vec2(2.0f, 2.0f), sizeof(Vec2));
				memcpy(&b->blockSplitter[i].pos,   &b->pos, sizeof(Vec2));
				memcpy(&b->blockSplitter[i].vel,   &Vec2(InRange(-0.1f, 0.3f), InRange(-6.0f, -4.0f)), sizeof(Vec2));
				memcpy(&b->blockSplitter[i].acc,   &Vec2(InRange(-0.8f, 0.8f), InRange(-0.8f, 0.8f)),  sizeof(Vec2));
			}

			//Explosion
			b->isExplosinActive = false;

			b->explosion.frame           = Vec2(0.0f, 0.0f);
			b->explosion.timeToNextFrame = EXPLOSION_TIME_BETWEEN_FRAMES;

			b->explosion.pos = Vec2(b->pos.x - (EXPLOSION_WIDTH / 2) + (BLOCK_WIDTH / 2), b->pos.y - (EXPLOSION_HEIGHT / 2) + (BLOCK_HEIGHT / 2) );
		}

		numberOfActiveBLocks = numberOfBlocks; //Counter, used for level up.

		//set states
		currentGameState = GAMESTATE_PLAY;
		currentMenuState = MENUSTATE_NONE;

		gameIsStarted = true;

		//Set Background.
		if (score.level == 1)
		{
			currentBackgroundLevelTexture = LoadTextureFromFile(sdlRenderer, "../res/images/background_level_1.png");
		}
		else if (score.level == 2)
		{
			currentBackgroundLevelTexture = LoadTextureFromFile(sdlRenderer, "../res/images/background_level_2.png");
		}
		else
		{
			currentBackgroundLevelTexture = LoadTextureFromFile(sdlRenderer, "../res/images/background_level_3.png");
		}

		//Menu: Continue
		menu.continueGame.pos = { WINDOW_WIDTH / 2.0f - menu.continueGame.size.x / 2.0f, menu.newGame.pos.y + menu.newGame.size.y + MENU_OFFSET_Y };

		//Menu: Exit game.
		menu.exitGame.pos = { WINDOW_WIDTH / 2.0f - menu.exitGame.size.x / 2.0f, menu.continueGame.pos.y + menu.continueGame.size.y + MENU_OFFSET_Y };

		//Menu: Instruction
		menu.instruction.pos = { WINDOW_WIDTH / 2.0f - menu.instruction.size.x / 2.0f, menu.exitGame.pos.y + menu.exitGame.size.y + MENU_OFFSET_Y * 2 };
	}
	//******
	//MENUSTATE_INSTRUCTION
	//******
	else if (currentMenuState == MENUSTATE_INSTRUCTION)
	{
		currentMenuState = MENUSTATE_INSTRUCTIONS;
	}
	//******
	//MENUSTATE_INSTRUCTIONS
	//******
	else if (currentMenuState == MENUSTATE_INSTRUCTIONS)
	{
		UpdateMenuBackground(delta); //Update scrolling background.

		int x, y;
		SDL_GetMouseState(&x, &y);
		//Collisiondetection: Mouse vs Text Textures.
		//Go back
		if ((x > menu.instructions[4].pos.x && x < menu.instructions[4].pos.x + menu.instructions[4].size.x && (y > menu.instructions[4].pos.y && y < menu.instructions[4].pos.y + menu.instructions[4].size.y)))
		{
			if (isLeftMouseBtnClicked)
			{
				currentMenuState = MENUSTATE_NONE;

				clickingCoolDownTimer = COOLDOWNTIME;
			}
			else
			{
				if (!Mix_PlayingMusic() && !menu.instructions[4].isHoovering) Mix_PlayMusic(hooveringInMenuSound, 1); //Play Sound
				menu.instructions[4].isHoovering = true;
			}
		}
		else
		{
			menu.instructions[0].isHoovering = false;
			menu.instructions[1].isHoovering = false;
			menu.instructions[2].isHoovering = false;
			menu.instructions[3].isHoovering = false;
			menu.instructions[4].isHoovering = false;
		}
	}
	//******
	//MENUSTATE_CONTINUE
	//******
	else if (currentMenuState == MENUSTATE_CONTINUE)
	{
		currentGameState = GAMESTATE_PLAY;
		currentMenuState = MENUSTATE_NONE;
	}

}

//******
//Gamerenderer
//******
void GameRenderer(SDL_Renderer *renderer)
{
	SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
	SDL_RenderClear(renderer);

	//******
	//Play
	//******
	if (currentGameState == GAMESTATE_PLAY)
	{
		//Background
		SpriteDraw(sdlRenderer, currentBackgroundLevelTexture, Vec2(0, 0), Vec2(WINDOW_WIDTH, WINDOW_HEIGHT), Vec2(LEVEL_BACKGROUND_WIDTH / 6, abs(LEVEL_BACKGROUND_HEIGHT - WINDOW_HEIGHT)), globalScale);

		//paddle
		SpriteDraw(sdlRenderer, spriteSheet, Vec2(paddle.pos.x, paddle.pos.y), Vec2( (PADDLE_START_WIDTH / 3), PADDLE_FRAME_SIZE), Vec2(0, PADDLE_FRAME_SIZE * 2), globalScale); //Left

		const float midSize = ( paddle.size.x - (PADDLE_START_WIDTH / 3) * 2);
		for (int i = 1; i <= midSize / PADDLE_FRAME_SIZE; ++i)
		{
			SpriteDraw(sdlRenderer, spriteSheet, Vec2(paddle.pos.x + ( (PADDLE_START_WIDTH / 3) *i), paddle.pos.y), Vec2(PADDLE_FRAME_SIZE, PADDLE_FRAME_SIZE), Vec2(PADDLE_FRAME_SIZE, PADDLE_FRAME_SIZE * 2), globalScale); //Mid
		}

		SpriteDraw(sdlRenderer, spriteSheet, Vec2(paddle.pos.x + (PADDLE_START_WIDTH / 3) + midSize, paddle.pos.y), Vec2((PADDLE_START_WIDTH / 3), PADDLE_FRAME_SIZE), Vec2(PADDLE_FRAME_SIZE * 2, PADDLE_FRAME_SIZE * 2), globalScale); //Right


		//Ball
		SpriteDraw(sdlRenderer, spriteSheet, ball.pos, Vec2(BALL_WIDTH, BALL_HEIGHT), Vec2(BALL_FRAME_X, BALL_FRAME_Y), globalScale);

		//Blocks
		for (int i = 0; i != numberOfBlocks; ++i)
		{
			Block *b = &blocks[i];
			//Block
			if (b->health != 0)
			{
				int frameY;
				b->health == 1 ? frameY = BLOCK_HEIGHT : frameY = 0;
				SpriteDraw(sdlRenderer, spriteSheet, b->pos, Vec2(BLOCK_WIDTH, BLOCK_HEIGHT), Vec2(BLOCK_WIDTH * b->type, frameY), globalScale);
			}

			//Splitter
			if (b->isSplitterActive)
			{
				for (int k = 0; k != MAX_NUMBER_OF_SPLITTER; ++k)
				{
					if ((b->blockSplitter[k].pos.x + b->blockSplitter[k].size.x) < WINDOW_WIDTH  && b->blockSplitter[k].pos.x > 0 &&
						(b->blockSplitter[k].pos.y + b->blockSplitter[k].size.y) < WINDOW_HEIGHT && b->blockSplitter[k].pos.y > 0
						)
					{
						DrawFilledRectangle(sdlRenderer, b->blockSplitter[k].color, b->blockSplitter[k].pos.x, b->blockSplitter[k].pos.y, b->blockSplitter[k].size.x, b->blockSplitter[k].size.y);
					}
				}
			}

			//Explosion
			if (b->isExplosinActive)
			{
				SpriteDraw(sdlRenderer, textureExplosion, b->explosion.pos, Vec2(EXPLOSION_WIDTH, EXPLOSION_HEIGHT),
					Vec2(b->explosion.frame.x * EXPLOSION_WIDTH, b->explosion.frame.y * EXPLOSION_HEIGHT),  globalScale );
			}
		}

		//Textures
		int offsetX = 10;
		int offsetY = 20;
		Vec2 shadowOffset = Vec2(1.0f, 1.0f);

		//Textures: Level
		int levelWidth, levelHeigth;
		if (scoreTextures.levelTexture)
		{
			SDL_QueryTexture(scoreTextures.levelTexture, NULL, NULL, &levelWidth, &levelHeigth);
			SpriteDraw(sdlRenderer, scoreTextures.levelShadowTexture, Vec2(offsetX, offsetY) - shadowOffset, Vec2(levelWidth, levelHeigth), Vec2(0, 0), globalScale); //Shadow
			SpriteDraw(sdlRenderer, scoreTextures.levelTexture, Vec2(offsetX, offsetY), Vec2(levelWidth, levelHeigth), Vec2(0, 0), globalScale); //Origin
		}

		//Textures: Score
		int scoreWidth, scoreHeigth;
		if (scoreTextures.pointsTexture)
		{
			SDL_QueryTexture(scoreTextures.pointsTexture, NULL, NULL, &scoreWidth, &scoreHeigth);
			SpriteDraw(sdlRenderer, scoreTextures.pointsShadowTexture, Vec2(offsetX, levelHeigth + offsetY) - shadowOffset, Vec2(scoreWidth, scoreHeigth), Vec2(0, 0), globalScale); //Shadow
			SpriteDraw(sdlRenderer, scoreTextures.pointsTexture, Vec2(offsetX, levelHeigth + offsetY), Vec2(scoreWidth, scoreHeigth), Vec2(0, 0), globalScale); //Origin
		}

	}
	//******
	//GAMESTATE_NEXT_LEVEL
	//******
	if (currentGameState == GAMESTATE_NEXT_LEVEL)
	{
		SpriteDraw(sdlRenderer, nextLevel.backgroundTexture, Vec2(0, 0), Vec2(WINDOW_WIDTH, WINDOW_HEIGHT), Vec2(WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2), globalScale); //Background

		SpriteDraw(sdlRenderer, nextLevel.shadowTexture, nextLevel.pos - shadowOffset, nextLevel.size, Vec2(0, 0), globalScale);
		SpriteDraw(sdlRenderer, nextLevel.originTexture, nextLevel.pos, nextLevel.size, Vec2(0, 0), globalScale);
	}
	//******
	//GAMESTATE_GAME_OVER
	//******
	if (currentGameState == GAMESTATE_GAME_OVER)
	{
		SpriteDraw(sdlRenderer, gameOver.backgroundTexture, Vec2(0, 0), Vec2(WINDOW_WIDTH, WINDOW_HEIGHT), Vec2(WINDOW_WIDTH / 4, WINDOW_HEIGHT / 4), globalScale); //Background

		SpriteDraw(sdlRenderer, gameOver.shadowTexture, gameOver.pos - shadowOffset, gameOver.size, Vec2(0, 0), globalScale); //Shadow
		SpriteDraw(sdlRenderer, gameOver.originTexture, gameOver.pos, gameOver.size, Vec2(0, 0), globalScale); //Origin
	}
	//******
	//GAMESTATE_COMPLETED_GAME
	//******
	if (currentGameState == GAMESTATE_COMPLETED_GAME)
	{
		SpriteDraw(sdlRenderer, completedGame.backgroundTexture, Vec2(0, 0), Vec2(WINDOW_WIDTH, WINDOW_HEIGHT), Vec2(0, 0), globalScale); //Background

		SpriteDraw(sdlRenderer, completedGame.shadowTexture, completedGame.pos - shadowOffset, completedGame.size, Vec2(0, 0), globalScale);
		SpriteDraw(sdlRenderer, completedGame.originTexture, completedGame.pos, completedGame.size, Vec2(0, 0), globalScale);
	}
	//******
	//Menu
	//******
	else if (currentGameState == GAMESTATE_MENU && currentMenuState == MENUSTATE_NONE)
	{
		SpriteDraw(sdlRenderer, menu.background.texture, Vec2(0, 0), Vec2(WINDOW_WIDTH, WINDOW_HEIGHT), 
			Vec2(menu.background.frame.x * MENU_BACKGROUND_PIXELS_PER_FRAME, menu.background.frame.y * MENU_BACKGROUND_PIXELS_PER_FRAME), globalScale); //Background

		SpriteDraw(sdlRenderer, menu.title.shadowTexture, menu.title.pos - shadowOffset, menu.title.size, Vec2(0, 0), globalScale); //Title: shadow
		SpriteDraw(sdlRenderer, menu.title.originTexture, menu.title.pos, menu.title.size, Vec2(0, 0), globalScale); //Title: origin

		SpriteDraw(sdlRenderer, menu.newGame.shadowTexture, menu.newGame.pos - shadowOffset, menu.newGame.size, Vec2(0, 0), globalScale); //New game: shadow
		SpriteDraw(sdlRenderer, menu.newGame.originTexture, menu.newGame.pos, menu.newGame.size, Vec2(0, 0), globalScale); //New game: origin

		if (gameIsStarted)
		{
			SpriteDraw(sdlRenderer, menu.continueGame.shadowTexture, menu.continueGame.pos - shadowOffset, menu.continueGame.size, Vec2(0, 0), globalScale); //Continue game: shadow
			SpriteDraw(sdlRenderer, menu.continueGame.originTexture, menu.continueGame.pos, menu.continueGame.size, Vec2(0, 0), globalScale); //Continue game: origin
		}

		SpriteDraw(sdlRenderer, menu.exitGame.shadowTexture, menu.exitGame.pos - shadowOffset, menu.exitGame.size, Vec2(0, 0), globalScale); //Exit game: shadow
		SpriteDraw(sdlRenderer, menu.exitGame.originTexture, menu.exitGame.pos, menu.exitGame.size, Vec2(0, 0), globalScale); //Exit game: origin

		SpriteDraw(sdlRenderer, menu.instruction.shadowTexture, menu.instruction.pos - shadowOffset, menu.instruction.size, Vec2(0, 0), globalScale); //Instruction: shadow
		SpriteDraw(sdlRenderer, menu.instruction.originTexture, menu.instruction.pos, menu.instruction.size, Vec2(0, 0), globalScale); //Instruction: origin

		//if user are hoovering over texture.
		if (menu.newGame.isHoovering)
		{
			DrawNotFilledRectangle(sdlRenderer, menu.hooverColor, menu.newGame.pos.x - OFFSET_BORDER_TEXTURES / 2, menu.newGame.pos.y - OFFSET_BORDER_TEXTURES / 2, menu.newGame.size.x + OFFSET_BORDER_TEXTURES, menu.newGame.size.y + OFFSET_BORDER_TEXTURES);
		}
		else if (menu.continueGame.isHoovering)
		{
			DrawNotFilledRectangle(sdlRenderer, menu.hooverColor, menu.continueGame.pos.x - OFFSET_BORDER_TEXTURES / 2, menu.continueGame.pos.y - OFFSET_BORDER_TEXTURES / 2, menu.continueGame.size.x + OFFSET_BORDER_TEXTURES, menu.continueGame.size.y + OFFSET_BORDER_TEXTURES);
		}
		else if (menu.exitGame.isHoovering)
		{
			DrawNotFilledRectangle(sdlRenderer, menu.hooverColor, menu.exitGame.pos.x - OFFSET_BORDER_TEXTURES / 2, menu.exitGame.pos.y - OFFSET_BORDER_TEXTURES / 2, menu.exitGame.size.x + OFFSET_BORDER_TEXTURES, menu.exitGame.size.y + OFFSET_BORDER_TEXTURES);
		}
		else if (menu.instruction.isHoovering)
		{
			DrawNotFilledRectangle(sdlRenderer, menu.hooverColor, menu.instruction.pos.x - OFFSET_BORDER_TEXTURES / 2, menu.instruction.pos.y - OFFSET_BORDER_TEXTURES / 2, menu.instruction.size.x + OFFSET_BORDER_TEXTURES, menu.instruction.size.y + OFFSET_BORDER_TEXTURES);
		}
	}
	//******
	//MENUSTATE_INSTRUCTIONS
	//******
	else if (currentMenuState == MENUSTATE_INSTRUCTIONS)
	{
		SpriteDraw(sdlRenderer, menu.background.texture, Vec2(0, 0), Vec2(WINDOW_WIDTH, WINDOW_HEIGHT),
			Vec2(menu.background.frame.x * MENU_BACKGROUND_PIXELS_PER_FRAME, menu.background.frame.y * MENU_BACKGROUND_PIXELS_PER_FRAME), globalScale); //Background

		SpriteDraw(sdlRenderer, menu.instructions[0].shadowTexture, menu.instructions[0].pos - shadowOffset, menu.instructions[0].size, Vec2(0, 0), globalScale);//Shadow
		SpriteDraw(sdlRenderer, menu.instructions[0].originTexture, menu.instructions[0].pos, menu.instructions[0].size, Vec2(0, 0), globalScale);//Origin

		SpriteDraw(sdlRenderer, menu.instructions[1].shadowTexture, menu.instructions[1].pos - shadowOffset, menu.instructions[1].size, Vec2(0, 0), globalScale);//Shadow
		SpriteDraw(sdlRenderer, menu.instructions[1].originTexture, menu.instructions[1].pos, menu.instructions[1].size, Vec2(0, 0), globalScale);//Origin

		SpriteDraw(sdlRenderer, menu.instructions[2].shadowTexture, menu.instructions[2].pos - shadowOffset, menu.instructions[2].size, Vec2(0, 0), globalScale);//Shadow
		SpriteDraw(sdlRenderer, menu.instructions[2].originTexture, menu.instructions[2].pos, menu.instructions[2].size, Vec2(0, 0), globalScale);//Origin

		SpriteDraw(sdlRenderer, menu.instructions[3].shadowTexture, menu.instructions[3].pos - shadowOffset, menu.instructions[3].size, Vec2(0, 0), globalScale);//Shadow
		SpriteDraw(sdlRenderer, menu.instructions[3].originTexture, menu.instructions[3].pos, menu.instructions[3].size, Vec2(0, 0), globalScale);//Origin

		SpriteDraw(sdlRenderer, menu.instructions[4].shadowTexture, menu.instructions[4].pos - shadowOffset, menu.instructions[4].size, Vec2(0, 0), globalScale);//Shadow
		SpriteDraw(sdlRenderer, menu.instructions[4].originTexture, menu.instructions[4].pos, menu.instructions[4].size, Vec2(0, 0), globalScale);//Origin

		//if user are hoovering over texture.
		if (menu.instructions[4].isHoovering)
		{
			DrawNotFilledRectangle(sdlRenderer, menu.hooverColor, menu.instructions[4].pos.x - OFFSET_BORDER_TEXTURES / 2, menu.instructions[4].pos.y - OFFSET_BORDER_TEXTURES / 2, menu.instructions[4].size.x + OFFSET_BORDER_TEXTURES, menu.instructions[4].size.y + OFFSET_BORDER_TEXTURES);
		}
	}

	SDL_RenderPresent(renderer);
}

#undef main // Fuck that SDL main macro, R.I.P.
//******
//main
//******
int main()
{
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

	srand(time(NULL));

	if (SDL_Init(SDL_INIT_EVERYTHING) == 0)
	{
		//Create window, sdl2 window.
		int flags = 0;
		sdlWindow = SDL_CreateWindow("BreakOut",
			SDL_WINDOWPOS_UNDEFINED,
			SDL_WINDOWPOS_UNDEFINED,
			WINDOW_WIDTH,
			WINDOW_HEIGHT,
			flags);

		assert(sdlWindow);

		//sdl2 renderer.
		sdlRenderer = SDL_CreateRenderer(sdlWindow, -1, SDL_RENDERER_ACCELERATED);
		assert(sdlRenderer);

		//sdl2 image library
		IMG_Init(IMG_INIT_PNG);

		//Initialize SDL_mixer
		if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 4096) == 0) printf("Succesfully opened OGG library.\n");
		else printf("Failed to open OGG library: %s.\n", Mix_GetError());

		//Initialize Game
		float accumulator = 0.0f;
		float deltaTimeMs = 0.0f;

		Timer updateTimer;
		Timer renderTimer;
		TimerInit(&updateTimer);
		TimerInit(&renderTimer);

		SDL_Event event;

		//Scale
		globalScale = Vec2(1.0f, 1.0f);

		//Fonts
		TTF_Init();
		fontArial24 = TTF_OpenFont("../res/fonts/arial.ttf", 24);
		assert(fontArial24);

		fontArial32 = TTF_OpenFont("../res/fonts/arial.ttf", 32);
		assert(fontArial32);

		//States
		currentGameState = GAMESTATE_MENU;
		currentMenuState = MENUSTATE_NONE;

		isLeftMouseBtnClicked = false;

		//Play
		gameIsStarted = false;

		//Load sprites
		spriteSheet = LoadTextureFromFile(sdlRenderer, "../res/images/breakout.png");

		//Load Sounds
		explosionSound        = LoadSound("../res/sounds/explosion.ogg");
		hooveringInMenuSound  = LoadSound("../res/sounds/hoovering_in_menu.ogg");
		ballHitPaddleSound    = LoadSound("../res/sounds/ball_hit_paddle.ogg");
		ballhitBlockSound     = LoadSound("../res/sounds/ball_hit_block.ogg");

		//Sound volumn
		Mix_VolumeMusic(MIX_MAX_VOLUME / 8);

		//Block splitter
		blockSplitterColor[0] = { 135, 255, 255, 255 };
		blockSplitterColor[1] = { 135, 63, 255, 255 };
		blockSplitterColor[2] = { 255, 201, 165, 25 };
		blockSplitterColor[3] = { 255, 30, 81, 255 };

		//Block explosion
		textureExplosion = LoadTextureFromFile(sdlRenderer, "../res/images/explosion.png");

		//Score textures.
		scoreTextures.originColor = { 191, 66, 244, 255 };
		scoreTextures.shadowColor = { 70, 40, 70, 255 };

		//Level
		score.level = 1;

		//Next level
		nextLevel.originColor = { 191, 66, 244, 255 };
		nextLevel.shadowColor = { 70, 40, 70, 255 };

		nextLevel.originTexture = CreateTextTexture(sdlRenderer, fontArial32, nextLevel.originColor, "Congratualtions you advanced to next level!");
		nextLevel.shadowTexture = CreateTextTexture(sdlRenderer, fontArial32, nextLevel.shadowColor, "Congratualtions you advanced to next level!");

		int nextLevelWidth, nextLevelHeigth;
		SDL_QueryTexture(nextLevel.originTexture, NULL, NULL, &nextLevelWidth, &nextLevelHeigth);
		nextLevel.size = Vec2(nextLevelWidth, nextLevelHeigth);
		nextLevel.pos  = Vec2((WINDOW_WIDTH / 2) - (nextLevel.size.x / 2), WINDOW_HEIGHT / 4);

		nextLevel.backgroundTexture = LoadTextureFromFile(sdlRenderer, "../res/images/background_between_levels.png");

		//Game over
		gameOver.originColor = { 40, 161, 201, 255 };
		gameOver.shadowColor = { 25, 107, 135, 255 };

		gameOver.originTexture = CreateTextTexture(sdlRenderer, fontArial32, gameOver.originColor, "Game Over");
		gameOver.shadowTexture = CreateTextTexture(sdlRenderer, fontArial32, gameOver.shadowColor, "Game Over");

		int gameOverWidth, gameOverHeigth;
		SDL_QueryTexture(gameOver.originTexture, NULL, NULL, &gameOverWidth, &gameOverHeigth);
		gameOver.size = Vec2(gameOverWidth, gameOverHeigth);
		gameOver.pos  = Vec2((WINDOW_WIDTH / 2) - (gameOver.size.x / 2), WINDOW_HEIGHT / 4);

		gameOver.backgroundTexture = LoadTextureFromFile(sdlRenderer, "../res/images/background_game_over.png");

		//Completed game
		completedGame.originColor = { 255, 215, 0, 255 };
		completedGame.shadowColor = { 91,  200, 0, 255 };

		completedGame.originTexture = CreateTextTexture(sdlRenderer, fontArial32, completedGame.originColor, "Congratualtions you Completed the game!");
		completedGame.shadowTexture = CreateTextTexture(sdlRenderer, fontArial32, completedGame.shadowColor, "Congratualtions you Completed the game!");

		int completedGameWidth, completedGameHeigth;
		SDL_QueryTexture(nextLevel.originTexture, NULL, NULL, &completedGameWidth, &completedGameHeigth);
		completedGame.size = Vec2(completedGameWidth, completedGameHeigth);
		completedGame.pos  = Vec2((WINDOW_WIDTH / 2) - (completedGame.size.x / 2), WINDOW_HEIGHT / 4);

		completedGame.backgroundTexture = LoadTextureFromFile(sdlRenderer, "../res/images/background_completed_game.png");

		//paddle
		paddle.maxWidth = PADDLE_FRAME_SIZE * 13;
		paddle.maxVel = 10.0f;

		//Ball	
		ball.maxVel = Vec2(1.0f, 5.0f);

		//Blocks
		blockMaxColumns = WINDOW_WIDTH % BLOCK_WIDTH;
		blockMaxRows    = 4;

		blockOffsetX = 10;
		blockOffsetY = 3;

		//set states
		currentGameState = GAMESTATE_MENU;
		currentMenuState = MENUSTATE_NONE;


		//Menu background
		menu.background.texture = LoadTextureFromFile(sdlRenderer, "../res/images/menu_background.png");
		menu.background.frame = Vec2(0.0f, 0.0f);
		menu.background.timeToNextFrame = MENU_BACKGROUND_TIME_BETWEEN_FRAMES;

		//Menu colors
		menu.originColor = { 191, 66, 244, 255  };
		menu.shadowColor = { 70, 40, 70, 255    };

		menu.hooverColor = { 196, 170, 139, 255 };

		menu.goBackColor       = { 15, 15, 100, 255   };
		menu.goBackShadowColor = { 10, 10, 80, 255    };

		menu.titleColor       = { 40, 161, 201, 255 };
		menu.shadowTitleColor = { 25, 107, 135, 255 };

		int width, heigth;

		//Menu: Title
		menu.title.originTexture = CreateTextTexture(sdlRenderer, fontArial32, menu.titleColor, "Welcome to the Breakout game");
		menu.title.shadowTexture = CreateTextTexture(sdlRenderer, fontArial32, menu.shadowTitleColor, "Welcome to the Breakout game");

		SDL_QueryTexture(menu.title.originTexture, NULL, NULL, &width, &heigth);
		menu.title.size = Vec2(width, heigth);
		menu.title.pos = { WINDOW_WIDTH / 2.0f - menu.title.size.x / 2.0f, MENU_OFFSET_Y };

		//Menu: New game
		menu.newGame.originTexture = CreateTextTexture(sdlRenderer, fontArial24, menu.originColor, "Play New Game");
		menu.newGame.shadowTexture = CreateTextTexture(sdlRenderer, fontArial24, menu.shadowColor, "Play New Game");

		SDL_QueryTexture(menu.newGame.originTexture, NULL, NULL, &width, &heigth);
		menu.newGame.size = Vec2(width, heigth);
		menu.newGame.pos  = { WINDOW_WIDTH / 2.0f - menu.newGame.size.x / 2.0f, menu.title.pos.y + menu.title.size.y + MENU_OFFSET_Y * 2.0f };

		//Menu: Continue game
		menu.continueGame.originTexture = CreateTextTexture(sdlRenderer, fontArial24, menu.originColor, "Continue Game");
		menu.continueGame.shadowTexture = CreateTextTexture(sdlRenderer, fontArial24, menu.shadowColor, "Continue Game");

		SDL_QueryTexture(menu.newGame.originTexture, NULL, NULL, &width, &heigth);
		menu.continueGame.size = Vec2(width, heigth); 
		//ContinueGame position is intialized in UpdateGame -> MENU_NEW_GAME.

		//Menu: Exit game.
		menu.exitGame.originTexture = CreateTextTexture(sdlRenderer, fontArial24, menu.originColor, "Exit Game");
		menu.exitGame.shadowTexture = CreateTextTexture(sdlRenderer, fontArial24, menu.shadowColor, "Exit Game");

		SDL_QueryTexture(menu.exitGame.originTexture, NULL, NULL, &width, &heigth);
		menu.exitGame.size = Vec2(width, heigth);
		menu.exitGame.pos  = { WINDOW_WIDTH / 2.0f - menu.exitGame.size.x / 2.0f, menu.newGame.pos.y + menu.newGame.size.y + MENU_OFFSET_Y };

		//Menu: Instruction
		menu.instruction.originTexture = CreateTextTexture(sdlRenderer, fontArial24, menu.originColor, "Instruction");
		menu.instruction.shadowTexture = CreateTextTexture(sdlRenderer, fontArial24, menu.shadowColor, "Instruction");

		SDL_QueryTexture(menu.instruction.originTexture, NULL, NULL, &width, &heigth);
		menu.instruction.size = Vec2(width, heigth);
		menu.instruction.pos  = { WINDOW_WIDTH / 2.0f - menu.instruction.size.x / 2.0f, menu.exitGame.pos.y + menu.exitGame.size.y + MENU_OFFSET_Y * 2 };

		//Menu: instruction->instructions Title
		menu.instructions[0].originTexture = CreateTextTexture(sdlRenderer, fontArial32, menu.titleColor, "Instructions");
		menu.instructions[0].shadowTexture = CreateTextTexture(sdlRenderer, fontArial32, menu.shadowTitleColor, "Instructions");

		SDL_QueryTexture(menu.instructions[0].originTexture, NULL, NULL, &width, &heigth);
		menu.instructions[0].size = Vec2(width, heigth);
		menu.instructions[0].pos = { WINDOW_WIDTH / 2.0f - menu.instructions[0].size.x / 2.0f, MENU_OFFSET_Y };

		//Menu: instruction->instructions description
		menu.instructions[1].originTexture = CreateTextTexture(sdlRenderer, fontArial24, menu.originColor, "Start boll movement by pressing arrow 'UP'.");
		menu.instructions[1].shadowTexture = CreateTextTexture(sdlRenderer, fontArial24, menu.shadowColor, "Start boll movement by pressing arrow 'UP'.");

		SDL_QueryTexture(menu.instructions[1].originTexture, NULL, NULL, &width, &heigth);
		menu.instructions[1].size = Vec2(width, heigth);
		menu.instructions[1].pos = { WINDOW_WIDTH / 2.0f - menu.instructions[1].size.x / 2.0f, menu.instructions[0].pos.y + menu.instructions[0].size.y + MENU_OFFSET_Y };

		menu.instructions[2].originTexture = CreateTextTexture(sdlRenderer, fontArial24, menu.originColor, "Move paddle by pressing 'LEFT' and 'RIGHT' arrow.");
		menu.instructions[2].shadowTexture = CreateTextTexture(sdlRenderer, fontArial24, menu.shadowColor, "Move paddle by pressing 'LEFT' and 'RIGHT' arrow.");

		SDL_QueryTexture(menu.instructions[2].originTexture, NULL, NULL, &width, &heigth);
		menu.instructions[2].size = Vec2(width, heigth);
		menu.instructions[2].pos = { WINDOW_WIDTH / 2.0f - menu.instructions[2].size.x / 2.0f, menu.instructions[1].pos.y + menu.instructions[1].size.y + MENU_OFFSET_Y / 4 };

		menu.instructions[3].originTexture = CreateTextTexture(sdlRenderer, fontArial24, menu.originColor, "Game has 3 levels. If you fail to catch the ball with the paddle it is Game Over.");
		menu.instructions[3].shadowTexture = CreateTextTexture(sdlRenderer, fontArial24, menu.shadowColor, "Game has 3 levels. If you fail to catch the ball with the paddle it is Game Over.");

		SDL_QueryTexture(menu.instructions[3].originTexture, NULL, NULL, &width, &heigth);
		menu.instructions[3].size = Vec2(width, heigth);
		menu.instructions[3].pos = { WINDOW_WIDTH / 2.0f - menu.instructions[3].size.x / 2.0f, menu.instructions[2].pos.y + menu.instructions[2].size.y + MENU_OFFSET_Y / 4 };

		menu.instructions[4].originTexture = CreateTextTexture(sdlRenderer, fontArial24, menu.goBackColor, "GO BACK!");
		menu.instructions[4].shadowTexture = CreateTextTexture(sdlRenderer, fontArial24, menu.goBackShadowColor, "GO BACK!");

		SDL_QueryTexture(menu.instructions[4].originTexture, NULL, NULL, &width, &heigth);
		menu.instructions[4].size = Vec2(width, heigth);
		menu.instructions[4].pos = { WINDOW_WIDTH / 2.0f - menu.instructions[4].size.x / 2.0f, menu.instructions[3].pos.y + menu.instructions[3].size.y + MENU_OFFSET_Y * 2 };


		//GAME LOOP START!
		while (currentMenuState != MENUSTATE_EXIT)
		{
			TimerTick(&updateTimer);
			deltaTimeMs = TimerDeltaMs(&updateTimer);
			accumulator += deltaTimeMs;
			while (accumulator >= TIME_STEP)
			{
				//Event poll
				while (SDL_PollEvent(&event))
				{
					switch (event.type)
					{
						case SDL_KEYDOWN:
							switch (event.key.keysym.sym)
							{
								case SDLK_LEFT:
									paddle.dir = -1;
									requestToMovePaddle = true;
									break;

								case SDLK_RIGHT:
									paddle.dir = 1;
									requestToMovePaddle = true;
									break;

								case SDLK_UP:
									ball.vel = Vec2(0.0f, -ball.maxVel.y);
									break;

								case SDLK_ESCAPE:
									if (currentGameState == GAMESTATE_PLAY)
									{
										currentGameState = GAMESTATE_MENU;
										currentMenuState = MENUSTATE_NONE;
									}
									break;
							}
						break;
						case SDL_KEYUP:
							switch (event.key.keysym.sym)
							{
								case SDLK_LEFT:
									requestToMovePaddle = false;
									break;

								case SDLK_RIGHT:
									requestToMovePaddle = false;
									break;
							}
							break;
						case SDL_MOUSEBUTTONDOWN:
							if (event.button.clicks == SDL_BUTTON_LEFT) isLeftMouseBtnClicked = true;
							break;
						case SDL_MOUSEBUTTONUP:
							if (isLeftMouseBtnClicked) isLeftMouseBtnClicked = false;
							break;
						case SDL_QUIT:
							currentMenuState = MENUSTATE_EXIT;
							break;
					}
				}
				
				accumulator -= TIME_STEP; 

				//Do Update
				GameUpdate(TIME_STEP); // 60 fps.
			}

			TimerTick(&renderTimer);

			//Do renderer
			GameRenderer(sdlRenderer);
		}

		//GAME LOOP END!

		//Destroy
		//Play textures
		//Play: block, player, ball
		SDL_DestroyTexture(spriteSheet);

		//Play: game over
		SDL_DestroyTexture(gameOver.originTexture);
		SDL_DestroyTexture(gameOver.shadowTexture);
		gameOver.originTexture = NULL;
		gameOver.shadowTexture = NULL;

		//Play: next level
		SDL_DestroyTexture(nextLevel.originTexture);
		SDL_DestroyTexture(nextLevel.shadowTexture);
		nextLevel.originTexture = NULL;
		nextLevel.shadowTexture = NULL;

		//Play: completed game
		SDL_DestroyTexture(completedGame.originTexture);
		SDL_DestroyTexture(completedGame.shadowTexture);
		completedGame.originTexture = NULL;
		completedGame.shadowTexture = NULL;

		//Play: explosion
		SDL_DestroyTexture(textureExplosion);
		textureExplosion = NULL;

		//Play: backgrounds
		SDL_DestroyTexture(nextLevel.backgroundTexture);
		SDL_DestroyTexture(completedGame.backgroundTexture);
		SDL_DestroyTexture(gameOver.backgroundTexture);
		SDL_DestroyTexture(currentBackgroundLevelTexture);
		nextLevel.backgroundTexture     = NULL;
		completedGame.backgroundTexture = NULL;
		gameOver.backgroundTexture      = NULL;
		currentBackgroundLevelTexture   = NULL;

		//Play: level
		SDL_DestroyTexture(scoreTextures.levelShadowTexture);
		SDL_DestroyTexture(scoreTextures.levelTexture);
		scoreTextures.levelShadowTexture = NULL;
		scoreTextures.levelTexture = NULL;

		//Play: score
		SDL_DestroyTexture(scoreTextures.pointsShadowTexture);
		SDL_DestroyTexture(scoreTextures.pointsTexture);
		scoreTextures.pointsShadowTexture = NULL;
		scoreTextures.pointsTexture = NULL;

		//Menu textures
		//Menu: background
		SDL_DestroyTexture(menu.background.texture);
		menu.background.texture = NULL;

		//Menu: continue game
		SDL_DestroyTexture(menu.continueGame.originTexture);
		SDL_DestroyTexture(menu.continueGame.originTexture);
		menu.continueGame.originTexture = NULL;
		menu.continueGame.originTexture = NULL;

		//Menu: exit game
		SDL_DestroyTexture(menu.exitGame.originTexture);
		SDL_DestroyTexture(menu.exitGame.shadowTexture);
		menu.exitGame.originTexture = NULL;
		menu.exitGame.shadowTexture = NULL;

		//Menu: instruction
		SDL_DestroyTexture(menu.instruction.originTexture);
		SDL_DestroyTexture(menu.instruction.shadowTexture);
		menu.instruction.originTexture = NULL;
		menu.instruction.shadowTexture = NULL;

		//Menu: new game
		SDL_DestroyTexture(menu.newGame.originTexture);
		SDL_DestroyTexture(menu.newGame.shadowTexture);
		menu.newGame.originTexture = NULL;
		menu.newGame.shadowTexture = NULL;

		//Menu: instructions
		SDL_DestroyTexture(menu.instructions[0].originTexture);
		SDL_DestroyTexture(menu.instructions[0].shadowTexture);
		menu.instructions[0].originTexture = NULL;
		menu.instructions[0].shadowTexture = NULL;

		SDL_DestroyTexture(menu.instructions[1].originTexture);
		SDL_DestroyTexture(menu.instructions[1].shadowTexture);
		menu.instructions[1].originTexture = NULL;
		menu.instructions[1].shadowTexture = NULL;

		SDL_DestroyTexture(menu.instructions[2].originTexture);
		SDL_DestroyTexture(menu.instructions[2].shadowTexture);
		menu.instructions[2].originTexture = NULL;
		menu.instructions[2].shadowTexture = NULL;

		SDL_DestroyTexture(menu.instructions[3].originTexture);
		SDL_DestroyTexture(menu.instructions[3].shadowTexture);
		menu.instructions[3].originTexture = NULL;
		menu.instructions[3].shadowTexture = NULL;

		SDL_DestroyTexture(menu.instructions[4].originTexture);
		SDL_DestroyTexture(menu.instructions[4].shadowTexture);
		menu.instructions[4].originTexture = NULL;
		menu.instructions[4].shadowTexture = NULL;

		//Sounds
		Mix_FreeMusic(explosionSound);
		Mix_FreeMusic(hooveringInMenuSound);
		Mix_FreeMusic(ballHitPaddleSound);
		Mix_FreeMusic(ballhitBlockSound);

		//Game: sdl renderer
		SDL_DestroyRenderer(sdlRenderer);
		sdlRenderer = NULL;

		//Game: sdl window
		SDL_DestroyWindow(sdlWindow);
		sdlWindow = NULL;

		delete[] blocks;
		blocks = NULL;

		//Close mixer
		Mix_CloseAudio();
		Mix_Quit();

		//Close TTF
		TTF_Quit();

		// Close SDL2_Image
		IMG_Quit();

		// Always called to quit SDL.
		SDL_Quit();
	}

	return 0;
}