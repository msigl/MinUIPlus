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

#define FAVOURITES_PATH SHARED_USERDATA_PATH "/.minui/favourites.txt"
#define FAUX_FAVOURITES_PATH SDCARD_PATH "/Favourites"
#define BACKUP_RECENT_PATH SHARED_USERDATA_PATH "/.minui/recent.txt"
#define BACKUP_FAUX_RECENT_PATH SDCARD_PATH "/Recently Played"

#define FAVOURITES_NAME "Favourites"
#define TOOLS_NAME "Tools"
#define COLLECTIONS_NAME "Collections"
#define RECENT_NAME "Recently Played"

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

void MinUIPlus_initialize(Array* stack, SDL_Surface* screen);
void MinUIPlus_shutdown();
void MinUIPlus_renderLauncher(int show_version);
bool MinUIPlus_shouldRenderMenu();
int MinUIPlus_getDirty();
int MinUIPlus_getBtnUp();
int MinUIPlus_getBtnDown();
int MinUIPlus_getBtnLeft();
int MinUIPlus_getBtnRight();