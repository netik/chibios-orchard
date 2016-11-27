#include <stdio.h>

#include "ch.h"
#include "hal.h"
#include "chprintf.h"
#include "orchard.h"
#include "orchard-app.h"

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
#define BLINKY_DEFAULT_DELAY 5000 // in milliseconds

// Buttons
static GHandle ghButton1, ghButton2, ghButton3;
static GListener gl;

static void draw_launcher_buttons(void) {
  
  /* draw up down pushbuttons */
  GWidgetInit  wi, wi2, wi3;
  coord_t width;
  coord_t totalheight;
  
  width = gdispGetWidth();
  totalheight = gdispGetHeight();
  
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
  wi.text = "";
  wi.customDraw = gwinButtonDraw_ArrowUp;
  
  wi2.g.width = 80;
  wi2.g.height = 30;
  wi2.g.y = totalheight - 40;
  wi2.g.x = (width / 2)  - 40 ;
  wi2.text = "";
  wi2.customDraw = gwinButtonDraw_ArrowDown;

  wi3.g.width = 80;
  wi3.g.height = 30;
  wi3.g.y = totalheight - 40;
  wi3.g.x = width - 80;
  wi3.text = "Go";
  
  // Create the actual button
  ghButton1 = gwinButtonCreate(NULL, &wi);
  ghButton2 = gwinButtonCreate(NULL, &wi2);
  ghButton3 = gwinButtonCreate(NULL, &wi3);
  
}

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

  chsnprintf(tmp, sizeof(tmp), "%d of %d apps", list->selected + 1, list->total);
  //gdispFillArea(0, 0, gdispGetWidth(), gdispGetHeight() / 2, Black);
  // draw title bar
  font = gdispOpenFont("UI2");
  width = gdispGetWidth();
  height = gdispGetFontMetric(font, fontHeight);
  header_height = height;
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
    } else {
      gdispFillArea(0, header_height + (i - app_modulus * visible_apps) * height,
		    width, height, Black);
      draw_color = White;
    }
    
     
    gdispDrawStringBox(0, header_height + (i - app_modulus * visible_apps) * height,
                       width, height,
                       list->items[i].name, font, draw_color, justifyCenter);
  }

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

  gdispClear(Black);
  draw_launcher_buttons();

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

  // set up our local listener
  geventListenerInit(&gl);
  gwinAttachListener(&gl);
  geventRegisterCallback (&gl, orchardAppUgfxCallback, &gl);

  return;
}

void launcher_event(OrchardAppContext *context, const OrchardAppEvent *event) {
	GEvent * pe;
	struct launcher_list *list = (struct launcher_list *)context->priv;

	if (event->type == ugfxEvent) {
		pe = event->ugfx.pEvent;

		switch(pe->type) {
		case GEVENT_GWIN_BUTTON:
			if (((GEventGWinButton*)pe)->gwin == ghButton1) {
				list->selected--;
				if (list->selected >= list->total)
					list->selected = 0;
			}

			if (((GEventGWinButton*)pe)->gwin == ghButton2) {
				list->selected++;
				if (list->selected >= list->total)
				list->selected = list->total-1;
			}

			if (((GEventGWinButton*)pe)->gwin == ghButton3) {
				orchardAppRun(list->items[list->selected].entry);
				return;
      			}

      			redraw_list(list);
			break;
		default:
			break;
		}
	}

	return;
}

static void launcher_exit(OrchardAppContext *context) {

  (void)context;

  gwinDestroy (ghButton1);
  gwinDestroy (ghButton2);
  gwinDestroy (ghButton3);

  geventRegisterCallback (&gl, NULL, NULL);
  geventDetachSource (&gl, NULL);
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
