#pragma once
#define IC_CARD_LIST_SIZE    (0x05)

#include <string>

using namespace std;

typedef struct
{
    int len;
    int system_tick;
    string data;
} card_block_t;

int ic_card_physics_read(card_block_t *p_block);