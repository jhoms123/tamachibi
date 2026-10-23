#pragma once
#include <stdint.h>

#define M_FOOD  0
#define M_LIGHT 1
#define M_GAME  2
#define M_MED   3
#define M_CLEAN 4
#define M_LOVE  5
#define M_CALL  6
#define M_WALK  7
#define M_TRAIN 8
#define M_STUDY 9
#define M_GARDEN 10
#define M_MUSIC 11
#define M_STATS 12
#define M_SHOP 13
#define M_TOY 14
#define M_EXPLORE 15
#define M_COOK 16
#define M_JOB 17
#define M_ROOM 18
#define M_TALK 19
#define M_SETTINGS 20
#define M_ALBUM 21
#define M_SITTER 22
#define M_NEW_EGG 23
#define M_BOOK 24
#define M_CRAFT 25
#define M_HABITAT 26
#define M_EVENT 27
#define M_FAMILY 28
#define M_INSPECT 29
#define M_HISTORY 30
#define M_DIAG 31
#define M_DIARY 32
#define M_AQUARIUM 33
#define M_PLANTROOM 34
#define M_PARADISE 35
#define M_LIFE 36
#define M_PLAYER 37

#define M_NEIGHBORS 38
#define M_COMPANION 39
#define M_COMMUNITY 40
#define M_FESTIVAL 41
#define M_RESEARCH 42
#define M_ADVENTURE 43

// Call only at the successful end of an activity, never at menu dispatch.
bool completeGameplayAction(uint8_t action);

// Stable individual aptitude: lineage + temperament + saved birth seed.
uint8_t naturalActivity();
