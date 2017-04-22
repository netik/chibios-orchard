#include <stdio.h>
#include <string.h>
#include "ch.h"
#include "hal.h"
#include "chprintf.h"
#include "orchard.h"
#include "orchard-app.h"
#include "fontlist.h"
#include "dac_lld.h"
#include "ides_gfx.h"
#include "userconfig.h"
#include "dec2romanstr.h"

#define LAUNCHER_COLS 3
#define LAUNCHER_ROWS 2
#define LAUNCHER_PERPAGE (LAUNCHER_ROWS * LAUNCHER_COLS)

extern const OrchardApp *orchard_app_list;
extern uint8_t shout_ok;
static uint32_t last_ui_time = 0;

struct launcher_list_item {
  const char        *name;
  const OrchardApp  *entry;
};

struct launcher_list {
  font_t  fontLG;
  font_t  fontXS;
  font_t  fontFX;
  GHandle ghTitleL;
  GHandle ghTitleR;
  GHandle ghButtonUp;
  GHandle ghButtonDn;

  unsigned int total;
  
  /* app grid */
  GHandle ghButtons[6];
  GHandle ghLabels[6];
  GListener gl;

  unsigned int page;
  unsigned int selected;

  struct launcher_list_item items[0];
};

static void redraw_list(struct launcher_list *list);
  
static void draw_launcher_buttons(struct launcher_list * list) {
  GWidgetInit wi;
  userconfig *config = getConfig();
  unsigned int i,j;
  char tmp[20];
  
  gwinWidgetClearInit(&wi);
   
  // Create label widget: ghTitleL
  wi.g.show = TRUE;
  wi.g.x = 0;
  wi.g.y = 0;
  wi.g.width = 160;
  wi.g.height = 20;
  wi.text = config->name;
  wi.customDraw = gwinLabelDrawJustifiedLeft;
  wi.customParam = 0;
  wi.customStyle = &DarkPurpleStyle;
  list->ghTitleL = gwinLabelCreate(0, &wi);
  gwinLabelSetBorder(list->ghTitleL, FALSE);
  gwinSetFont(list->ghTitleL, list->fontFX);
  gwinRedraw(list->ghTitleL);

  chsnprintf(tmp, sizeof(tmp), "LEVEL %s", dec2romanstr(config->level+1));

  // Create label widget: ghTitleR
  wi.g.show = TRUE;
  wi.g.x = 160;
  wi.g.y = 0;
  wi.g.width = 160;
  wi.g.height = 20;
  wi.text = tmp;
  wi.customDraw = gwinLabelDrawJustifiedRight;
  wi.customParam = 0;
  wi.customStyle = &DarkPurpleStyle;
  list->ghTitleR = gwinLabelCreate(0, &wi);
  gwinSetFont(list->ghTitleR, list->fontFX);
  gwinLabelSetBorder(list->ghTitleR, FALSE);
  gwinRedraw(list->ghTitleR);
  
  /* draw the button grid
   *
   * buttons
   * xloc: 0, 90, 180
   * yloc: 30, 140 
   *
   * label
   * xloc: 0, 90, 180
   * yloc: 110, 220 
   **/
  /* Each button in the launcher grid is a transparent area that we
     draw the image onto after the fact using redraw_list. We cannot
     use standard ugfx button rendering here because we are severly
     ram-constrained. we must put images and then close them as soon
     as possible
  */

  gwinSetDefaultFont(list->fontXS);
  
  for (i = 0; i < LAUNCHER_ROWS; i++) {
      for (j = 0; j < LAUNCHER_COLS; j++) {
        wi.g.show = TRUE;
        wi.g.x = j * 90+1;
        wi.g.y = 31 + (110 * i);
        wi.g.width = 80;
        wi.g.height = 80;
        wi.text = "";
        wi.customParam = 0;
        wi.customDraw = noRender; 
        wi.customStyle = 0;
        list->ghButtons[(i*LAUNCHER_COLS)+j] = gwinButtonCreate(0, &wi);
        
        // Create label widget: ghLabel1
        wi.g.show = TRUE;
        wi.g.x = j * 90;
        wi.g.y = 110 * (i+1);
        wi.g.width = 80;
        wi.g.height = 20;
        wi.text = "---";
        wi.customDraw = gwinLabelDrawJustifiedCenter;
        wi.customParam = 0;
        wi.customStyle = 0;
        list->ghLabels[(i*LAUNCHER_COLS)+j] = gwinLabelCreate(0, &wi);
      }
  }

  // create button widget: ghButtonUp
  wi.g.show = TRUE;
  wi.g.x = 270;
  wi.g.y = 30;
  wi.g.width = 50;
  wi.g.height = 50;
  wi.text = "";
  wi.customDraw = gwinButtonDraw_ArrowUp;
  wi.customParam = 0;
  wi.customStyle = &DarkPurpleStyle;
  list->ghButtonUp = gwinButtonCreate(0, &wi);

  // create button widget: ghButtonDn
  wi.g.show = TRUE;
  wi.g.x = 270;
  wi.g.y = 190;
  wi.g.width = 50;
  wi.g.height = 50;
  wi.text = "";
  wi.customDraw = gwinButtonDraw_ArrowDown;
  wi.customParam = 0;
  wi.customStyle = &DarkPurpleStyle;
  list->ghButtonDn = gwinButtonCreate(0, &wi);
  gwinRedraw(list->ghButtonDn);

  list->selected = 0;
  list->page = 0;

  redraw_list(list);
}

static void redraw_list(struct launcher_list *list) {
  unsigned int i,j;
  unsigned int actualid;
  
  /* given a page number, put down an image for each of the buttons
     and set the label. */
  for (i = 0; i < LAUNCHER_ROWS; i++) { 
    for (j = 0; j < LAUNCHER_COLS; j++) {
      actualid = (list->page * LAUNCHER_PERPAGE) + (i*LAUNCHER_COLS) + j;
      
      if (actualid < list->total) {
        if (list->items[actualid].entry->icon != NULL)
          putImageFile(list->items[actualid].entry->icon, j * 90, 30 + (110 * i));
        
        gwinSetText(list->ghLabels[(i*LAUNCHER_COLS) + j], list->items[actualid].name, FALSE);
      } else {
        gwinSetText(list->ghLabels[(i*LAUNCHER_COLS) + j], "", TRUE);
        gdispFillArea(j * 90, 30 + (110 * i), 81, 81, Black );
      }
    }
  }

  // row y
  i = (list->selected - (list->page * LAUNCHER_PERPAGE)) / LAUNCHER_COLS;
    
  // col x
  j = list->selected % LAUNCHER_COLS;

  chprintf(stream, "select: i %d j %d selected %d\r\n", i, j, list->selected);
  gdispDrawBox(j * 90, 30 + (110 * i), 81, 81, Red );
}

static uint32_t launcher_init(OrchardAppContext *context) {

  (void)context;
  
  gdispClear(Black);
  shout_ok = 1;

  return (0);
}

static void launcher_start(OrchardAppContext *context) {

  userconfig *config = getConfig();
  struct launcher_list *list;
  const OrchardApp *current;
  unsigned int total_apps = 0;

  /* How many apps do we have? */
  current = orchard_app_list;
  while (current->name) {
    if ((current->flags & APP_FLAG_HIDDEN) == 0)
      total_apps++;
    current++;
  }

  list = chHeapAlloc (NULL, sizeof(struct launcher_list) +
    + (total_apps * sizeof(struct launcher_list_item)) );

  context->priv = list;

  list->fontXS = gdispOpenFont(FONT_XS);
  list->fontLG = gdispOpenFont(FONT_LG);
  list->fontFX = gdispOpenFont(FONT_FIXED);
  
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

  list->selected = 0;

  draw_launcher_buttons(list);
  redraw_list(list);
  
  // set up our local listener
  geventListenerInit(&list->gl);
  gwinAttachListener(&list->gl);
  geventRegisterCallback (&list->gl, orchardAppUgfxCallback, &list->gl);

  // set up our idle timer
  orchardAppTimer(context, 1000, true);
  last_ui_time = chVTGetSystemTime();

  // forcibly run the name app if our name is blank. Sorry. 
  if (strlen(config->name) == 0) { 
    orchardAppRun(orchardAppByName("Set your name"));
    return;
  }

  // if you don't have a type set, force that too.
  if (config->p_type == 0) { 
    orchardAppRun(orchardAppByName("Choosetype"));
    return;
  }
  return;
}

void launcher_event(OrchardAppContext *context, const OrchardAppEvent *event) {
  GEvent * pe;
  struct launcher_list *list = (struct launcher_list *)context->priv;
  int i,j;
  
  if( (chVTGetSystemTime() - last_ui_time) > UI_IDLE_TIME ) {
    // automatic timeout to the badge screen
    orchardAppRun(orchardAppByName("Badge"));
    return;
  }

  if (event->type == keyEvent) {
    // row y
    i = (list->selected - (list->page * LAUNCHER_PERPAGE)) / LAUNCHER_COLS;
    // col x
    j = list->selected % LAUNCHER_COLS;
    gdispDrawBox(j * 90, 30 + (110 * i), 81, 81, Black );
    
    
    if ( (event->key.code == keyUp) &&
         (event->key.flags == keyPress) )  {
      last_ui_time = chVTGetSystemTime();
      list->selected = list->selected - LAUNCHER_COLS;
    }
    if ( (event->key.code == keyDown) &&
         (event->key.flags == keyPress) )  {
      last_ui_time = chVTGetSystemTime();
      if (list->selected + LAUNCHER_COLS < (list->total - 1))  { 
        list->selected = list->selected + LAUNCHER_COLS;
      }
    }
    if ( (event->key.code == keyLeft) &&
         (event->key.flags == keyPress) )  {
      last_ui_time = chVTGetSystemTime();
      if (list->selected > 0) { 
        list->selected--;
      }
    }
    if ( (event->key.code == keyRight) &&
         (event->key.flags == keyPress) )  {
      last_ui_time = chVTGetSystemTime();
      if (list->selected < (list->total - 1)) 
        list->selected++;
    }

    if (list->selected > 250) {
      list->selected  = 0;
    }

    if (list->selected > list->total) {
      list->selected = list->total;

    }

    if (list->selected >= ((list->page + 1) * LAUNCHER_PERPAGE))  {
      list->page++;
      redraw_list(list);
      return;
    }

    if (list->page > 0) {
      if (list->selected < (list->page * LAUNCHER_PERPAGE))  {
        list->page--;
        redraw_list(list);
        return;
      }
    }
    
    if ( (event->key.code == keySelect) &&
         (event->key.flags == keyPress) )  {
      orchardAppRun(list->items[list->selected].entry);
      return;
    }

    // row y
    i = (list->selected - (list->page * LAUNCHER_PERPAGE)) / LAUNCHER_COLS;
    // col x
    j = list->selected % LAUNCHER_COLS;

    // col
    chprintf(stream, "select: i %d j %d selected %d tot %d\r\n", i, j, list->selected, list->total);
    gdispDrawBox(j * 90, 30 + (110 * i), 81, 81, Red);
    return;
    
  }
  
  if (event->type == ugfxEvent) {
    pe = event->ugfx.pEvent;
    
    switch(pe->type) {
    case GEVENT_GWIN_BUTTON:
      last_ui_time = chVTGetSystemTime();
      for (i = 0; i < 6; i++) {  
        if (( ((GEventGWinButton*)pe)->gwin == list->ghButtons[i]) &&
            ((list->page*LAUNCHER_PERPAGE)+i < list->total)) { 
          chprintf(stream, "Running app %d.\r\n", (list->page*LAUNCHER_PERPAGE)+i);
          orchardAppRun(list->items[(list->page*LAUNCHER_PERPAGE)+i].entry);
          return;
        }
      }

      // only update the location if we're still within a valid page range
      if (((GEventGWinButton*)pe)->gwin == list->ghButtonDn) {
        if (((list->page + 1) * LAUNCHER_PERPAGE) < list->total) { 
          list->page++;
        }
      }
      if (((GEventGWinButton*)pe)->gwin == list->ghButtonUp) {
        if (list->page > 0) {
          list->page--;
        }
      }
      break;
    default:
      break;
    }
  }
  
  // wraparound
  if (list->selected > 255) // underflow
    list->selected = list->total-1;
  
  if (list->selected >= list->total)
    list->selected = 0;
  
  if (event->type == ugfxEvent || event->type == keyEvent) {
    redraw_list(list);
  }
  
  return;
}

static void launcher_exit(OrchardAppContext *context) {
  struct launcher_list *list;
  int i;
  list = (struct launcher_list *)context->priv;

  gdispCloseFont(list->fontXS);
  gdispCloseFont(list->fontLG);
  gdispCloseFont(list->fontFX);
  
  gwinDestroy(list->ghTitleL);
  gwinDestroy(list->ghTitleR);
  gwinDestroy(list->ghButtonUp);
  gwinDestroy(list->ghButtonDn);

  /* nuke the grid */
  for (i=0; i< 6; i++) { 
    gwinDestroy(list->ghButtons[i]);
    gwinDestroy(list->ghLabels[i]);
  }

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
  NULL,
  APP_FLAG_HIDDEN,
  launcher_init,
  launcher_start,
  launcher_event,
  launcher_exit,
};
