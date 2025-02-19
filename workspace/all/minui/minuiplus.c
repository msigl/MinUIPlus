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
#include "minuiplus.h"

typedef struct {
  char* key;
  SDL_Surface *value;
} MapEntry;

typedef struct {
  MapEntry* entries;
  int size;
  int capacity;
} Map;

static char* THEME = "outline";

static int img_used_width;
static Map backgroundMap;
static Map darkBackgroundMap;
static Map logoMap;

static Array* stackRef;
static Directory* topRef;
static SDL_Surface* screenRef;


static void initializeMap(Map* map, int initialCapacity) {
  map->size = 0;
  map->capacity = initialCapacity;
  map->entries = malloc(initialCapacity * sizeof(MapEntry));
}

static void resizeMap(Map* map) {
  map->capacity *= 2;  // Double the capacity
  map->entries = realloc(map->entries, map->capacity * sizeof(MapEntry));  // Reallocate memory
}

static void putMapEntry(Map* map, char* key, SDL_Surface* value) {
  // override existing value
  for (int i = 0; i < map->size; i++) {
    if (strcmp(map->entries[i].key, key) == 0) {
        map->entries[map->size].key = strdup(key);
        map->entries[map->size].value = value;
        return;
    }
  }
  // add new value and increase capacity if needed
  if (map->size == map->capacity) {
    resizeMap(map);
  }
  map->entries[map->size].key = strdup(key);
  map->entries[map->size].value = value;
  map->size++;
}

static SDL_Surface* getMapEntry(Map* map, char* key) {
  for (int i = 0; i < map->size; i++) {
    if (strcmp(map->entries[i].key, key) == 0) {
      return map->entries[i].value;
    }
  }
  return NULL;  // Key not found
}

static void freeMap(Map* map) {
  for (int i = 0; i < map->size; i++) {
    free(map->entries[i].key);
  }
  free(map->entries);
}

static SDL_Surface* loadBackgroundImage(char* name, SDL_Surface* screen) {
  char imagePath[MAX_PATH];
  sprintf(imagePath, "%s/res/theme/%s/%s.png", ROOT_SYSTEM_PATH, THEME, name);
  if(!exists(imagePath)) {
    sprintf(imagePath, "%s/res/theme/%s/%s.png", ROOT_SYSTEM_PATH, THEME, "_default");
  }
  SDL_Surface* image = IMG_Load(imagePath);
  SDL_Surface* optimizedImage = SDL_ConvertSurface(image, screen->format, 0);
  int destHeight = screen->h;
  float scale = destHeight / (float) optimizedImage->h;
  int destWidth = optimizedImage->w * scale;
  int usedWidthPerImage = destWidth - (100 * scale);
  img_used_width = usedWidthPerImage;
  SDL_Surface* preScaled = SDL_CreateRGBSurfaceWithFormat(0, destWidth, destHeight, 32, SDL_PIXELFORMAT_RGBA32);
  SDL_SetSurfaceBlendMode(preScaled, SDL_BLENDMODE_BLEND);
  SDL_Rect destRect = { 0, 0, destWidth, destHeight };
  SDL_BlitScaled(image, NULL, preScaled, &destRect);
  SDL_FreeSurface(image);
  SDL_FreeSurface(optimizedImage);
  return preScaled;
}

static SDL_Surface* loadDarkenedBackgroundImage(char* name, SDL_Surface* screen) {
  SDL_Surface* image = loadBackgroundImage(name, screen);
  SDL_Surface* overlay = SDL_CreateRGBSurfaceWithFormat(
    0, image->w, image->h, image->format->BitsPerPixel, image->format->format);
  Uint32 halfTransparentBlack = SDL_MapRGBA(overlay->format, 0, 0, 0, 128);
  SDL_FillRect(overlay, NULL, halfTransparentBlack);
  SDL_SetSurfaceBlendMode(overlay, SDL_BLENDMODE_BLEND);
  SDL_BlitSurface(overlay, NULL, image, NULL);
  return image;
}

static SDL_Surface* loadLogo(char* name, SDL_Surface* screen) {
  char imagePath[MAX_PATH];
  sprintf(imagePath, "%s/res/theme/%s_Logo.png", ROOT_SYSTEM_PATH, name);     
  if (exists(imagePath)) {
    SDL_Surface* image = IMG_Load(imagePath);
    SDL_Surface* optimizedImage = SDL_ConvertSurface(image, screen->format, 0);
    int destWidth = screen->w * 0.60; // 60% of screen width
    float scale = destWidth / (float) optimizedImage->w;
    int destHeight = optimizedImage->h * scale;
    SDL_Surface* preScaled = SDL_CreateRGBSurfaceWithFormat(0, destWidth, destHeight, 32, SDL_PIXELFORMAT_RGBA32);
    SDL_SetSurfaceBlendMode(preScaled, SDL_BLENDMODE_BLEND);
    SDL_Rect destRect = { 0, 0, destWidth, destHeight };
    SDL_BlitScaled(image, NULL, preScaled, &destRect);
    SDL_FreeSurface(image);
    SDL_FreeSurface(optimizedImage);
    return preScaled;
  } 
  return NULL;
}

void MinUIPlus_initialize(Directory* top, Array* stack, SDL_Surface* screen) {
  topRef = top;
  screenRef = screen;
  stackRef = stack;

  initializeMap(&backgroundMap, 20);
  initializeMap(&darkBackgroundMap, 20);
  initializeMap(&logoMap, 20);

  for (int i=0; i<top->entries->count; i++) {
    Entry* entry = top->entries->items[i];
    putMapEntry(&backgroundMap, entry->name, loadBackgroundImage(entry->name, screen));
    putMapEntry(&darkBackgroundMap, entry->name, loadDarkenedBackgroundImage(entry->name, screen));
    putMapEntry(&logoMap, entry->name, loadLogo(entry->name, screen));
  }

}

void MinUIPlus_shutdown() {
  freeMap(&backgroundMap);
  freeMap(&darkBackgroundMap);
  freeMap(&logoMap);
}

void MinUIPlus_renderLauncher(int show_version) {
  if (show_version || stackRef->count > 1 || !topRef || topRef->entries->count < 1) {
    return;
  }

  // selected entry background
  Entry* entry = topRef->entries->items[topRef->selected];
  SDL_Surface* image = getMapEntry(&backgroundMap, entry->name);
  int offsetX = (screenRef->w - image->w) / 2;
  SDL_Rect destRect = { offsetX, 0, image->w, screenRef->h };
  SDL_BlitSurface(image, NULL, screenRef, &destRect);

  int numberOfImagesOnEachSideOfCenter = ceil(((float)screenRef->w/2) / (float)img_used_width) - 1;
  int helperIndexRightSide = topRef->selected + 1;
  int helperIndexLeftSide = topRef->selected - 1;

  for(int i = 1; i <= numberOfImagesOnEachSideOfCenter; i++) {
    // right side
    if(helperIndexRightSide >= topRef->entries->count) {
      helperIndexRightSide = 0;
    }
    Entry* entry = topRef->entries->items[helperIndexRightSide];
    SDL_Surface* image = getMapEntry(&darkBackgroundMap, entry->name);
    if(image) {
      int offsetX = (screenRef->w - image->w) / 2 + (img_used_width * i);
      SDL_Rect destRect = { offsetX, 0, image->w, screenRef->h };
      SDL_BlitSurface(image, NULL, screenRef, &destRect);
    }
    helperIndexRightSide++;
    // left side
    if(helperIndexLeftSide < 0) {
      helperIndexLeftSide = topRef->entries->count-1;
    }
    entry = topRef->entries->items[helperIndexLeftSide];
    image = getMapEntry(&darkBackgroundMap, entry->name);
    if(image) {
      int offsetX = (screenRef->w - image->w) / 2 - (img_used_width * i);
      SDL_Rect destRect = { offsetX, 0, image->w, screenRef->h };
      SDL_BlitSurface(image, NULL, screenRef, &destRect);
    }
    helperIndexLeftSide--;
  }

  // draw logo of selected entry
  SDL_Surface* logo = getMapEntry(&logoMap, entry->name);
  if(logo) {
    SDL_Rect destRect = { screenRef->w / 2 - logo->w / 2, screenRef->h / 2 - logo->h / 2, logo->w, logo->h };
    SDL_BlitSurface(logo, NULL, screenRef, &destRect);
  } else {
    GFX_blitMessage(font.large, entry->name, screenRef, &(SDL_Rect){0,0,screenRef->w,screenRef->h});
  }
}

bool MinUIPlus_shouldRenderMenu() {
  return stackRef->count > 1;
}

int MinUIPlus_getBtnUp() {
  if(stackRef->count < 2) {
    return BTN_LEFT;
  }
  return BTN_UP;
}

int MinUIPlus_getBtnDown() {
  if(stackRef->count < 2) {
    return BTN_RIGHT;
  }
  return BTN_DOWN;
}

int MinUIPlus_getBtnLeft() {
  if(stackRef->count < 2) {
    return BTN_UP;
  }
  return BTN_LEFT;
}

int MinUIPlus_getBtnRight() {
  if(stackRef->count < 2) {
    return BTN_DOWN;
  }
  return BTN_RIGHT;
}