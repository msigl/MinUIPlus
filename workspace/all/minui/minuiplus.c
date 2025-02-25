// find . -name "*.png" | xargs -L1 -I{} convert -scale x721 "{}" x720/"{}" // can be used to convert pngs into a different size
// find . -name "*.png" | xargs -L1 -I{} convert -monochrome -write "{}" ./"{}" // can be used to convert pngs to black and white
// find . -type f -name "*.cfg" -exec dos2unix {} \+; // to convert from crlf to lf, was needed for *.sh and *.cfg after cloning repo with windows ...

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

// TODO
/*static char* FOLDER_MAP_THEME = { "auto-favorites", "auto-lastplayed", "gb", "gbc", "gba", "nes", 
                                  "md", "psx", "snes", "ngp", "ngpc", "pico8", 
                                  "gamegear", "mastersystem", "", "tg16", "virtualboy" };
static char* FOLDER_MAP_MINUI = { "Favourites", "Recently Played", "Game Boy Advance", "Game Boy Color", "Game Boy", "Nintendo Entertainment System",
                                  "Sega Genesis", "Sony PlayStation", "Super Nintendo Entertainment System", "Neo Geo Pocket", "Neo Geo Pocket Color", "Pico-8",
                                  "Sega Game Gear", "Sega Master System", "Super Game Boy", "TurboGrafx-16", "Virtual Boy" };
static char* FALLBACK_NAME = "_default";*/

static char* THEMES[] = { "default", "outline", "MinUI" }; // MinUI needs to be last
static int THEME_INDEX = 0;
static char* THEME;

static int img_used_width;
static Map backgroundMap;
static Map darkBackgroundMap;
static Map logoMap;

static Array* stackRef;
static Directory* topRef;
static SDL_Surface* screenRef;

static bool isMinUIThemeActive() {
  return strcmp(THEME, "MinUI") == 0;
}

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

// Helper function to get a pixel from an SDL_Surface at (x, y)
static Uint32 getpixel(SDL_Surface* surface, int x, int y) {
    int bpp = surface->format->BytesPerPixel;
    Uint8* p = (Uint8*)surface->pixels + y * surface->pitch + x * bpp;
    switch(bpp) {
        case 1:
            return *p;
        case 2:
            return *(Uint16 *)p;
        case 3:
            if(SDL_BYTEORDER == SDL_BIG_ENDIAN)
                return p[0] << 16 | p[1] << 8 | p[2];
            else
                return p[0] | p[1] << 8 | p[2] << 16;
        case 4:
            return *(Uint32 *)p;
        default:
            return 0;  // Should not happen.
    }
}

// Function that counts transparent pixels (alpha == 0) in the last row of a surface.
static int countTransparentPixelsInFirstRow(SDL_Surface* surface) {
    if (!surface)
        return 0;
    
    int transparentCount = 0;
    int y = 0; // first row, last row would be surface->h - 1

    // Lock the surface if needed.
    if (SDL_MUSTLOCK(surface)) {
        if (SDL_LockSurface(surface) < 0) {
            //SDL_Log("Couldn't lock surface: %s", SDL_GetError());
            return 0;
        }
    }

    for (int x = 0; x < surface->w; x++) {
        Uint32 pixel = getpixel(surface, x, y);
        Uint8 r, g, b, a;
        SDL_GetRGBA(pixel, surface->format, &r, &g, &b, &a);
        if(a == 0) {  // Fully transparent
            transparentCount++;
        }
    }

    if (SDL_MUSTLOCK(surface)) {
        SDL_UnlockSurface(surface);
    }
    
    return transparentCount;
}

static int calculateImageUsedWidth(SDL_Surface* image) {
  int width = image->w - countTransparentPixelsInFirstRow(image);
  width += (int)(((float)width) / 100) * 5; // + 5% of image width as spacer between images
  return width;
}

static SDL_Surface* loadBackgroundImage(char* theme, char* name, SDL_Surface* screen) {
  char imagePath[MAX_PATH];
  bool resizeNeeded = true;
  sprintf(imagePath, "%s/res/theme/%s/x%d/%s.png", ROOT_SYSTEM_PATH, theme, screen->h, name);
  if(!exists(imagePath)) {
    sprintf(imagePath, "%s/res/theme/%s/%s.png", ROOT_SYSTEM_PATH, theme, name);
    if(!exists(imagePath)) {
      sprintf(imagePath, "%s/res/theme/%s/%s.png", ROOT_SYSTEM_PATH, theme, "_default");
    }  
  } else {
    resizeNeeded = false;
  }

  SDL_Surface* image = IMG_Load(imagePath);
  if(!resizeNeeded) {
    img_used_width = calculateImageUsedWidth(image);
    return image;
  }
  
  int destHeight = screen->h;
  float scale = destHeight / (float) image->h;
  int destWidth = image->w * scale;
  SDL_Surface* preScaled = SDL_CreateRGBSurfaceWithFormat(0, destWidth, destHeight, 32, SDL_PIXELFORMAT_RGBA32);
  SDL_SetSurfaceBlendMode(preScaled, SDL_BLENDMODE_BLEND);
  SDL_Rect destRect = { 0, 0, destWidth, destHeight };
  SDL_BlitScaled(image, NULL, preScaled, &destRect);
  img_used_width = calculateImageUsedWidth(preScaled);
  SDL_FreeSurface(image);
  return preScaled;
}

static SDL_Surface* loadDarkenedBackgroundImage(char* theme, char* name, SDL_Surface* screen) {
  SDL_Surface* image = loadBackgroundImage(theme, name, screen);
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
  sprintf(imagePath, "%s/res/theme/logo/%dx/%s.png", ROOT_SYSTEM_PATH, screen->w, name);
  bool resizeNeeded = false;
  if(!exists(imagePath)) {
    sprintf(imagePath, "%s/res/theme/logo/%s.png", ROOT_SYSTEM_PATH, name);  
    resizeNeeded = true;
  }
  if (exists(imagePath)) {
    SDL_Surface* image = IMG_Load(imagePath);
    if(!resizeNeeded) {
      return image;
    }
    int destWidth = screen->w * 0.60; // 60% of screen width
    float scale = destWidth / (float) image->w;
    int destHeight = image->h * scale;
    SDL_Surface* preScaled = SDL_CreateRGBSurfaceWithFormat(0, destWidth, destHeight, 32, SDL_PIXELFORMAT_RGBA32);
    SDL_SetSurfaceBlendMode(preScaled, SDL_BLENDMODE_BLEND);
    SDL_Rect destRect = { 0, 0, destWidth, destHeight };
    SDL_BlitScaled(image, NULL, preScaled, &destRect);
    SDL_FreeSurface(image);
    return preScaled;
  } 
  return NULL;
}

static char* getThemedName(char* theme, char* name) {
  char* themedName = malloc(512);
  sprintf(themedName, "%s/%s", theme, name);
  return themedName;
}

void MinUIPlus_initialize(Array* stack, SDL_Surface* screen) {
  topRef = stack->items[0];
  screenRef = screen;
  stackRef = stack;

  LOG_info("MinUIPlus_initialize / begin:  %s, %s\n", topRef->path, topRef->name);

  THEME = THEMES[THEME_INDEX];

  initializeMap(&backgroundMap, 50);
  initializeMap(&darkBackgroundMap, 50);
  initializeMap(&logoMap, 50);

  for (int i=0; i<topRef->entries->count; i++) {
    Entry* entry = topRef->entries->items[i];
    size_t length = sizeof(THEMES) / sizeof(THEMES[0]);
    for (int j = 0; j < length - 1; j++) { // -1 because we don't want the last array entry (= minui default theme)
      char* themedName = getThemedName(THEMES[j], entry->name);
      putMapEntry(&backgroundMap, themedName, loadBackgroundImage(THEMES[j], entry->name, screen));
      putMapEntry(&darkBackgroundMap, themedName, loadDarkenedBackgroundImage(THEMES[j], entry->name, screen));
      putMapEntry(&logoMap, themedName, loadLogo(entry->name, screen));
      free(themedName);
    }    
  }

  LOG_info("MinUIPlus_initialize / end\n");
}

void MinUIPlus_shutdown() {
  LOG_info("MinUIPlus_shutdown\n");
  freeMap(&backgroundMap);
  freeMap(&darkBackgroundMap);
  freeMap(&logoMap);
  free(THEME);
}

void MinUIPlus_renderLauncher(int show_version) {
  if (show_version || stackRef->count > 1 || !topRef || topRef->entries->count < 1 || isMinUIThemeActive()) {
    return;
  }

  // selected entry background
  Entry* entry = topRef->entries->items[topRef->selected];
  char* entryThemeName = getThemedName(THEME, entry->name);
  SDL_Surface* image = getMapEntry(&backgroundMap, entryThemeName);
  free(entryThemeName);
  int offsetX = (screenRef->w - image->w) / 2;
  SDL_Rect destRect = { offsetX, 0, image->w, screenRef->h };
  SDL_BlitSurface(image, NULL, screenRef, &destRect);

  int numberOfImagesOnEachSideOfCenter = ceil(((float)screenRef->w/2) / (float)img_used_width);
  int helperIndexRightSide = topRef->selected + 1;
  int helperIndexLeftSide = topRef->selected - 1;

  for(int i = 1; i <= numberOfImagesOnEachSideOfCenter; i++) {
    // right side
    if(helperIndexRightSide >= topRef->entries->count) {
      helperIndexRightSide = 0;
    }
    Entry* entry = topRef->entries->items[helperIndexRightSide];
    char* entryThemeName = getThemedName(THEME, entry->name);
    SDL_Surface* image = getMapEntry(&darkBackgroundMap, entryThemeName);
    free(entryThemeName);
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
    entryThemeName = getThemedName(THEME, entry->name);
    image = getMapEntry(&darkBackgroundMap, entryThemeName);
    free(entryThemeName);
    if(image) {
      int offsetX = (screenRef->w - image->w) / 2 - (img_used_width * i);
      SDL_Rect destRect = { offsetX, 0, image->w, screenRef->h };
      SDL_BlitSurface(image, NULL, screenRef, &destRect);
    }
    helperIndexLeftSide--;
  }

  // draw logo of selected entry
  SDL_Surface* logo = getMapEntry(&logoMap, getThemedName(THEME, entry->name));
  if(logo) {
    int heightFactor = 2;
    if(strcmp(THEME, "outline") == 0) {
      heightFactor = 3;
    }
    SDL_Rect destRect = { screenRef->w / 2 - logo->w / 2, screenRef->h / heightFactor - logo->h / 2, logo->w, logo->h };
    SDL_BlitSurface(logo, NULL, screenRef, &destRect);
  } else {
    GFX_blitMessage(font.large, entry->name, screenRef, &(SDL_Rect){0,0,screenRef->w,screenRef->h});
  }
}

bool MinUIPlus_shouldRenderMenu() {
  return stackRef->count > 1 || isMinUIThemeActive();
}

int MinUIPlus_getBtnUp() {
  if(stackRef->count < 2 && !isMinUIThemeActive()) {
    return BTN_LEFT;
  }
  return BTN_UP;
}

int MinUIPlus_getBtnDown() {
  if(stackRef->count < 2 && !isMinUIThemeActive()) {
    return BTN_RIGHT;
  }
  return BTN_DOWN;
}

int MinUIPlus_getBtnLeft() {
  if(stackRef->count < 2 && !isMinUIThemeActive()) {
    return BTN_UP;
  }
  return BTN_LEFT;
}

int MinUIPlus_getBtnRight() {
  if(stackRef->count < 2 && !isMinUIThemeActive()) {
    return BTN_DOWN;
  }
  return BTN_RIGHT;
}

int MinUIPlus_getDirty() {
  if(stackRef->count == 1 && PAD_justPressed(BTN_SELECT)) {
    THEME_INDEX++;
    size_t length = sizeof(THEMES) / sizeof(THEMES[0]);
    if(THEME_INDEX >= length) {
      THEME_INDEX = 0;
    }
    THEME = THEMES[THEME_INDEX];
    return 1;
  }
  return 0;
}