#include <stdio.h>

#include "ch.h"
#include "hal.h"
#include "chprintf.h"
#include "orchard.h"
#include "orchard-app.h"
#include "fontlist.h"

#include "userconfig.h"

extern const OrchardApp *orchard_app_list;
extern uint8_t shout_ok;
static uint32_t last_ui_time = 0;

struct launcher_list_item {
  const char        *name;
  const OrchardApp  *entry;
};

struct launcher_list {
  GHandle ghButton1;
  GHandle ghButton2;
  GHandle ghButton3;
  GListener gl;
  unsigned int selected;
  unsigned int total;
  struct launcher_list_item items[0];
};

static void draw_launcher_buttons(struct launcher_list * l) {
  
  /* draw up down pushbuttons */
  GWidgetInit  wi, wi2, wi3;
  coord_t width;
  coord_t totalheight;
  font_t font;

  font = gdispOpenFont(FONT_LG);
  width = gdispGetWidth();
  totalheight = gdispGetHeight();
  
  // Apply some default values for GWIN
  gwinWidgetClearInit(&wi);
  gwinWidgetClearInit(&wi2);
  gwinWidgetClearInit(&wi3);
  gwinSetDefaultStyle(&BlackWidgetStyle, FALSE);
  
  wi.g.show = TRUE;
  wi2.g.show = TRUE;
  wi3.g.show = TRUE;

  // Apply the button parameters
  gwinSetDefaultFont(font);
  wi.g.width = 80;
  wi.g.height = 50;
  wi.g.y = totalheight - 50;
  wi.g.x = 2;
  wi.text = "";
  wi.customDraw = gwinButtonDraw_ArrowUp;
  
  wi2.g.width = 80;
  wi2.g.height = 50;
  wi2.g.y = totalheight - 50;
  wi2.g.x = (width / 2)  - 40 ;
  wi2.text = "";
  wi2.customDraw = gwinButtonDraw_ArrowDown;

  wi3.g.width = 80;
  wi3.g.height = 50;
  wi3.g.y = totalheight - 50;
  wi3.g.x = width - 80;
  wi3.text = "Go";
  
  // Create the actual button
  l->ghButton1 = gwinButtonCreate(NULL, &wi);
  l->ghButton2 = gwinButtonCreate(NULL, &wi2);
  l->ghButton3 = gwinButtonCreate(NULL, &wi3);
  gdispCloseFont(font);
  
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
  uint8_t drawn;

  userconfig *config = getConfig();

  chsnprintf(tmp, sizeof(tmp), "%d of %d apps", list->selected + 1, list->total);
  //gdispFillArea(0, 0, gdispGetWidth(), gdispGetHeight() / 2, Black);
  // draw title bar
  font = gdispOpenFont(FONT_FIXED);
  width = gdispGetWidth();
  height = gdispGetFontMetric(font, fontHeight);
  header_height = height;
  gdispFillArea(0, 0, width, height, Red);

  //  chsnprintf(tmp, sizeof(tmp), "%s", family->name);
  gdispDrawStringBox(0, 0, width, height,
                     tmp, font, White, justifyLeft);
  //  chsnprintf(tmp, sizeof(tmp), "%d %d%%", ui_timeout, ggStateofCharge());
  gdispDrawStringBox(0, 0, width, height,
                     config->name, font, White, justifyRight);

  // draw app list
  width = gdispGetWidth();
  height = gdispGetFontMetric(font, fontHeight);

  /*
   * When calculating the number of visible apps to show, don't forget
   * to factor in the space at the bottom of the screen being used for
   * the selection buttons, otherwise we might draw over them.
   */

  totalheight = gdispGetHeight() - 50;
  visible_apps = (uint8_t) (totalheight - header_height) / height;
  
  app_modulus = (uint8_t) list->selected / visible_apps;
  
  max_list = (app_modulus + 1) * visible_apps;
  if( max_list > list->total )
    max_list = list->total;

  drawn = 0;  
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
    drawn++;
  }

  /* Blank any leftover area */

  gdispFillArea (0, header_height + (drawn * height),
    gdispGetWidth(), (visible_apps - drawn) * height, Black);

  gdispCloseFont(font);
  gdispFlush();
}

static uint32_t launcher_init(OrchardAppContext *context) {

  (void)context;

  gdispClear(Black);
  shout_ok = 1;

  return (0);
}

static void launcher_start(OrchardAppContext *context) {

  struct launcher_list *list;
  const OrchardApp *current;
  unsigned int total_apps = 0;

  /* Rebuild the app list */
  current = orchard_app_list;
  while (current->name) {
    if ((current->flags & APP_FLAG_HIDDEN) == 0)
      total_apps++;
    current++;
  }

  list = chHeapAlloc (NULL, sizeof(struct launcher_list) +
    + (total_apps * sizeof(struct launcher_list_item)) );

  context->priv = list;

  draw_launcher_buttons(list);

  /* Rebuild the app list */
  current = orchard_app_list;
  list->total = 0;
  while (current->name) {
    if ((current->flags & APP_FLAG_HIDDEN) == 0) {
        list->items[list->total].name = current->name;
        list->items[list->total].entry = current;
        list->total++;
    }
    current++;
  }

  list->selected = 1;

  redraw_list(list);

  // set up our local listener
  geventListenerInit(&list->gl);
  gwinAttachListener(&list->gl);
  geventRegisterCallback (&list->gl, orchardAppUgfxCallback, &list->gl);

  // set up our idle timer
  orchardAppTimer(context, 1000, true);
  last_ui_time = chVTGetSystemTime();
  
  return;
}

void launcher_event(OrchardAppContext *context, const OrchardAppEvent *event) {
  GEvent * pe;
  struct launcher_list *list = (struct launcher_list *)context->priv;
  
  if( (chVTGetSystemTime() - last_ui_time) > UI_IDLE_TIME ) {
    orchardAppRun(orchardAppByName("Badge"));
  }
  else
    if (event->type == ugfxEvent) {
      pe = event->ugfx.pEvent;
      
      switch(pe->type) {
      case GEVENT_GWIN_BUTTON:
	if (((GEventGWinButton*)pe)->gwin == list->ghButton1) {
	  last_ui_time = chVTGetSystemTime();
	  list->selected--;
	  
	  if (list->selected >= list->total)
	    list->selected = 0;
	}
	
	if (((GEventGWinButton*)pe)->gwin == list->ghButton2) {
	  last_ui_time = chVTGetSystemTime();
	  list->selected++;
	  if (list->selected >= list->total)
	    list->selected = list->total-1;
	}
	
	if (((GEventGWinButton*)pe)->gwin == list->ghButton3) {
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
  struct launcher_list *list;

  list = (struct launcher_list *)context->priv;

  gwinDestroy (list->ghButton1);
  gwinDestroy (list->ghButton2);
  gwinDestroy (list->ghButton3);

  geventRegisterCallback (&list->gl, NULL, NULL);
  geventDetachSource (&list->gl, NULL);

  chHeapFree (context->priv);
  context->priv = NULL;
  shout_ok = 0;
}

/* the app labelled as app_1 is always the launcher. changing this
   section header will move the code further down the list and will
   cause a different app to become the launcher -- jna */

const OrchardApp _orchard_app_list_launcher
__attribute__((used, aligned(4), section(".chibi_list_app_1_launcher"))) = {
  "Launcher", 
  APP_FLAG_HIDDEN,
  launcher_init,
  launcher_start,
  launcher_event,
  launcher_exit,
};
