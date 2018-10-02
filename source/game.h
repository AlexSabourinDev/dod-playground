#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


#define kMaxSpriteCount 1100000

typedef struct
{
    float posX, posY;
} sprite_pos_data_t;

typedef struct
{
    uint8_t colR, colG, colB;
    uint8_t sprite;
} sprite_sprite_data_t;

void game_initialize(void);
void game_destroy(void);
// returns amount of sprites
int game_update(sprite_pos_data_t* posData, sprite_sprite_data_t* spriteData, double time, float deltaTime);


#ifdef __cplusplus
}
#endif
