#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <msettings.h>
#include <sys/types.h>
#include <dirent.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>

#include "defines.h"
#include "api.h"
#include "utils.h"

typedef struct Array {
  int count;
  int capacity;
  void** items;
} Array;

#define INT_ARRAY_MAX 27
typedef struct IntArray {
  int count;
  int items[INT_ARRAY_MAX];
} IntArray;

typedef struct Directory {
  char* path;
  char* name;
  Array* entries;
  IntArray* alphas;
  // rendering
  int selected;
  int start;
  int end;
} Directory;

typedef struct Entry {
  char* path;
  char* name;
  char* unique;
  int type;
  int alpha; // index in parent Directory's alphas Array, which points to the index of an Entry in its entries Array :sweat_smile:
} Entry;

void MinUIPlus_initialize(Directory* top, Array* stack, SDL_Surface* screen);
void MinUIPlus_shutdown();
void MinUIPlus_renderLauncher(int show_version);
bool MinUIPlus_shouldRenderMenu();
int MinUIPlus_getBtnUp();
int MinUIPlus_getBtnDown();
int MinUIPlus_getBtnLeft();
int MinUIPlus_getBtnRight();