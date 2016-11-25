#include <stdio.h>

#include "ch.h"
#include "hal.h"
#include "chprintf.h"
#include "orchard.h"
#include "orchard-app.h"

#include "storage.h"

extern const OrchardApp *orchard_app_list;

struct launcher_list_item {
  const char        *name;
  const OrchardApp  *entry;
};

struct launcher_list {
  unsigned int selected;
  unsigned int total;
  struct launcher_list_item items[0];
};

static uint32_t last_ui_time = 0;
#define BLINKY_DEFAULT_DELAY 60000 // in milliseconds

// Buttons
static GHandle   ghButton1, ghButton2, ghButton3;

static void redraw_list(struct launcher_list *list) {

  char tmp[20];
  uint8_t i;

  coord_t width;
  coord_t height;
  coord_t totalheight;
  coord_t header_height;
  font_t font;
  uint8_t visible_apps;
  uint8_t app_modulus;
  uint8_t max_list;
  int32_t ui_timeout;

  ui_timeout = (BLINKY_DEFAULT_DELAY - (chVTGetSystemTime() - last_ui_time)) / 1000;
  chsnprintf(tmp, sizeof(tmp), "%d of %d apps", list->selected + 1, list->total);

  // draw title bar
  font = gdispOpenFont("UI2");
  width = gdispGetWidth();
  height = gdispGetFontMetric(font, fontHeight);
  header_height = height;
  gdispClear(Black);
  gdispFillArea(0, 0, width, height, White);

  //  chsnprintf(tmp, sizeof(tmp), "%s", family->name);
  gdispDrawStringBox(0, 0, width, height,
                     tmp, font, Black, justifyLeft);
  //  chsnprintf(tmp, sizeof(tmp), "%d %d%%", ui_timeout, ggStateofCharge());
  gdispDrawStringBox(0, 0, width, height,
                     tmp, font, Black, justifyRight);

  // draw app list
  width = gdispGetWidth();
  height = gdispGetFontMetric(font, fontHeight);

  totalheight = gdispGetHeight();
  visible_apps = (uint8_t) (totalheight - header_height) / height;
  
  app_modulus = (uint8_t) list->selected / visible_apps;
  
  max_list = (app_modulus + 1) * visible_apps;
  if( max_list > list->total )
    max_list = list->total;
  
  for (i = app_modulus * visible_apps; i < max_list; i++) {
    color_t draw_color = White;
    
    if (i == list->selected) {
      gdispFillArea(0, header_height + (i - app_modulus * visible_apps) * height,
		    width, height, White);
      draw_color = Black;
    }
    
    gdispDrawStringBox(0, header_height + (i - app_modulus * visible_apps) * height,
                       width, height,
                       list->items[i].name, font, draw_color, justifyCenter);
  }

  /* draw up down pushbuttons */
  GWidgetInit  wi, wi2, wi3;

  // Apply some default values for GWIN
  gwinWidgetClearInit(&wi);
  gwinWidgetClearInit(&wi2);
  gwinWidgetClearInit(&wi3);
  
  wi.g.show = TRUE;
  wi2.g.show = TRUE;
  wi3.g.show = TRUE;

  // Apply the button parameters
  wi.g.width = 80;
  wi.g.height = 30;
  wi.g.y = totalheight - 40;
  wi.g.x = 2;
  wi.text = "Up";
  
  wi2.g.width = 80;
  wi2.g.height = 30;
  wi2.g.y = totalheight - 40;
  wi2.g.x = (width / 2)  - 40 ;
  wi2.text = "Down";

  wi3.g.width = 80;
  wi3.g.height = 30;
  wi3.g.y = totalheight - 40;
  wi3.g.x = width - 80;
  wi3.text = "Go";
  
  // Create the actual button
  ghButton1 = gwinButtonCreate(NULL, &wi);
  ghButton2 = gwinButtonCreate(NULL, &wi2);
  ghButton3 = gwinButtonCreate(NULL, &wi3);
    
  gdispCloseFont(font);
  gdispFlush();
  orchardGfxEnd();
}

static uint32_t launcher_init(OrchardAppContext *context) {

  (void)context;
  unsigned int total_apps = 0;
  const OrchardApp *current;

  /* Rebuild the app list */
  current = orchard_app_list;
  while (current->name) {
    total_apps++;
    current++;
  }

  return sizeof(struct launcher_list)
       + (total_apps * sizeof(struct launcher_list_item));
}

static void launcher_start(OrchardAppContext *context) {

  struct launcher_list *list = (struct launcher_list *)context->priv;
  const OrchardApp *current;

  /* Rebuild the app list */
  current = orchard_app_list;
  list->total = 0;
  while (current->name) {
    list->items[list->total].name = current->name;
    list->items[list->total].entry = current;
    list->total++;
    current++;
  }

  list->selected = 1;

  last_ui_time = chVTGetSystemTime();
  redraw_list(list);
}

void launcher_event(OrchardAppContext *context, const OrchardAppEvent *event) {
  struct launcher_list *list = (struct launcher_list *)context->priv;
  int32_t ui_timeout;
  const OrchardApp *led_app;

  ui_timeout = (BLINKY_DEFAULT_DELAY - (chVTGetSystemTime() - last_ui_time)) / 1000;

  if (event->type == keyEvent) {
    last_ui_time = chVTGetSystemTime();
    if ((event->key.flags == keyDown) && (event->key.code == keyCW)) {
      list->selected++;
      if (list->selected >= list->total)
        list->selected = 0;
    }
    else if ((event->key.flags == keyDown) && (event->key.code == keyCCW)) {
      list->selected--;
      if (list->selected >= list->total)
        list->selected = list->total - 1;
    }
    else if ((event->key.flags == keyDown) && (event->key.code == keySelect)) {
      orchardAppRun(list->items[list->selected].entry);
    }
    redraw_list(list);
  }

  if( ui_timeout <= 0 ) {
    led_app = orchardAppByName("Blinkies!");
    if( led_app != NULL )
      orchardAppRun(led_app);
  }
}

static void launcher_exit(OrchardAppContext *context) {

  (void)context;
}

/* the app labelled as app_1 is always the launcher. changing this
   section header will move the code further down the list and will
   cause a different app to become the launcher -- jna */

const OrchardApp _orchard_app_list_launcher
__attribute__((used, aligned(4), section(".chibi_list_app_1_launcher"))) = {
  "Launcher", 
  launcher_init,
  launcher_start,
  launcher_event,
  launcher_exit,
};
