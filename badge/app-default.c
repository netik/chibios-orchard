#include <string.h>

#include "ch.h"
#include "hal.h"
#include "chprintf.h"

#include "orchard-app.h"
#include "orchard-events.h"

#include "rand.h"
#include "led.h"
#include "orchard-ui.h"
#include "images.h"
#include "fontlist.h"

#include "ides_gfx.h"
#include "unlocks.h"

#include "userconfig.h"
#include "datetime.h"
#include "dec2romanstr.h"

/* we will heal the player at N hp per this interval or 2X HP if
   UL_PLUSDEF has been activated */
#define HEAL_INTERVAL_US 1000000      /* 1 second */

/* how long to reach full heal */
#define HEAL_TIME        25           /* seconds */

/* we we will attempt to find a new caesar every these many seconds */
#define CS_ELECT_INT     MS2ST(30000) /* 30 sec */
#define MAX_CAESAR_TIME  3600000000   /* 1 hour */

/* remember these many last key-pushes */
#define KEY_HISTORY 9

static uint32_t last_caesar_check = 0;
static uint32_t caesar_expires_at = 0;

typedef struct _DefaultHandles {
  GHandle ghFightButton;
  GHandle ghExitButton;
  GListener glBadge;
  /* ^ ^ v v < > < > ent will fire the unlocks screen */
  OrchardAppEventKeyCode last_pushed[KEY_HISTORY];
} DefaultHandles;


const OrchardAppEventKeyCode konami[] = { keyUp, keyUp, keyDown, 
                                     keyDown, keyLeft, keyRight,
                                     keyLeft, keyRight, keySelect };
extern uint8_t shout_ok;

static void draw_badge_buttons(DefaultHandles * p) {
  GWidgetInit wi;
  coord_t totalheight = gdispGetHeight();
  
  // Apply some default values for GWIN
  gwinWidgetClearInit(&wi);
  wi.g.show = TRUE;
  wi.g.width = 60;
  wi.g.height = 60;
  wi.g.y = totalheight - 60;
  wi.g.x = 0;
  wi.text = "";
  wi.customDraw = noRender;

  p->ghFightButton = gwinButtonCreate(NULL, &wi);

#ifdef notdef
  gwinWidgetClearInit(&wi);
  wi.g.show = TRUE;
  wi.g.width = 60;
  wi.g.height = 60;
  wi.g.y = totalheight - 60;
  wi.text = "";
  wi.customDraw = noRender;
#endif
  wi.g.x = gdispGetWidth() - 60;

  p->ghExitButton = gwinButtonCreate(NULL, &wi);
}

static void redraw_badge(void) {
  // draw the entire background badge image. Shown when the screen is idle. 
  font_t fontLG, fontSM, fontXS;
  const userconfig *config = getConfig();

  // our image draws just a bit underneath the stats data. If we
  // draw the character during the HP update, it will blink and we
  // don't want that.
  if (config->hp < (maxhp(config->unlocks,config->level) * .25)) {
    putImageFile(getAvatarImage(config->current_type, "deth", 2, false),
                 POS_PLAYER1_X, POS_PLAYER1_Y);
  } else { 
    if (config->hp < (maxhp(config->unlocks,config->level) * .50)) {
      // show the whoop'd ass graphic if you're less than 15% 
      putImageFile(getAvatarImage(config->current_type, "deth", 1, false),
                   POS_PLAYER1_X, POS_PLAYER1_Y);
    } else {
      putImageFile(getAvatarImage(config->current_type, "idla", 1, false),
                   POS_PLAYER1_X, POS_PLAYER1_Y);
    }
  }

  putImageFile(IMG_GROUND_BTNS, 0, POS_FLOOR_Y);
      
  fontXS = gdispOpenFont (FONT_XS);
  fontLG = gdispOpenFont (FONT_LG);
  fontSM = gdispOpenFont (FONT_FIXED);

  uint16_t ypos = 0; 
  uint16_t lmargin = 141;
  gdispDrawStringBox (0,
		      ypos,
		      gdispGetWidth(),
		      gdispGetFontMetric(fontLG, fontHeight),
		      config->name,
		      fontLG, Yellow, justifyRight);

  char tmp[20];
  char tmp2[40];
  chsnprintf(tmp, sizeof(tmp), "LEVEL %s", dec2romanstr(config->level));
  
  /* Level */
  ypos = ypos + gdispGetFontMetric(fontLG, fontHeight);
  gdispDrawStringBox (0,
		      ypos,
		      gdispGetWidth(),
		      gdispGetFontMetric(fontSM, fontHeight),
		      tmp,
		      fontSM, Yellow, justifyRight);

  /* XP */
  ypos = ypos + gdispGetFontMetric(fontSM, fontHeight) + 4;
  gdispDrawThickLine(141, ypos, 320, ypos, Red, 2, FALSE);
  gdispDrawThickLine(141, ypos+21, 320, ypos+21, Red, 2, FALSE);
  
  chsnprintf(tmp, sizeof(tmp), "HP");
  chsnprintf(tmp2, sizeof(tmp2), "%d", config->hp);

  /* hit point bar */
  gdispDrawStringBox (142,
		      ypos+6,
		      30,
		      gdispGetFontMetric(fontXS, fontHeight),
		      tmp,
		      fontXS, White, justifyLeft);

  drawProgressBar(163,ypos+6,120,11,maxhp(config->unlocks,config->level), config->hp, 0, false);

  gdispFillArea( 289, ypos+6, 
                 30,gdispGetFontMetric(fontXS, fontHeight),
                 Black );
                       
  gdispDrawStringBox (289,
		      ypos+6,
                      30,
		      gdispGetFontMetric(fontXS, fontHeight),
		      tmp2,
		      fontXS, White, justifyLeft);

  /* end hp bar */
  ypos = ypos + 30;

  /* XP/WON */
  chsnprintf(tmp2, sizeof(tmp2), "%3d", config->xp);
  gdispDrawStringBox (lmargin,
		      ypos,
		      45,
		      gdispGetFontMetric(fontSM, fontHeight),
		      "XP",
		      fontSM, Yellow, justifyLeft);
  gdispDrawStringBox (lmargin + 40,
		      ypos,
                      45,
		      gdispGetFontMetric(fontSM, fontHeight),
		      tmp2,
		      fontSM, White, justifyRight);

  chsnprintf(tmp2, sizeof(tmp2), "%3d", config->won);
  gdispDrawStringBox (lmargin + 94,
		      ypos,
		      45,
		      gdispGetFontMetric(fontSM, fontHeight),
		      "WON",
		      fontSM, Yellow, justifyLeft);
  gdispDrawStringBox (lmargin + 94 + 35,
		      ypos,
                      45,
		      gdispGetFontMetric(fontSM, fontHeight),
		      tmp2,
		      fontSM, White, justifyRight);

  /* AGL / LOST */
  ypos = ypos + gdispGetFontMetric(fontSM, fontHeight) + 2;
  chsnprintf(tmp2, sizeof(tmp2), "%3d", config->agl);
  gdispDrawStringBox (lmargin,
		      ypos,
		      45,
		      gdispGetFontMetric(fontSM, fontHeight),
		      "AGL",
		      fontSM, Yellow, justifyLeft);
  gdispDrawStringBox (lmargin + 40,
		      ypos,
                      45,
		      gdispGetFontMetric(fontSM, fontHeight),
		      tmp2,
		      fontSM, White, justifyRight);

  chsnprintf(tmp2, sizeof(tmp2), "%3d", config->lost);
  gdispDrawStringBox (lmargin + 94,
		      ypos,
		      60,
		      gdispGetFontMetric(fontSM, fontHeight),
		      "LOST",
		      fontSM, Yellow, justifyLeft);
  gdispDrawStringBox (lmargin + 94 + 35,
		      ypos,
                      45,
		      gdispGetFontMetric(fontSM, fontHeight),
		      tmp2,
		      fontSM, White, justifyRight);
  
  /* MIGHT */
  ypos = ypos + gdispGetFontMetric(fontSM, fontHeight) + 2;
  if (config->unlocks & UL_PLUSMIGHT) 
    chsnprintf(tmp2, sizeof(tmp2), "%3d", config->might + 1);
  else
    chsnprintf(tmp2, sizeof(tmp2), "%3d", config->might);
        

  gdispDrawStringBox (lmargin,
		      ypos,
		      60,
		      gdispGetFontMetric(fontSM, fontHeight),
		      "MIGHT",
		      fontSM, Yellow, justifyLeft);
  gdispDrawStringBox (lmargin + 40,
		      ypos,
                      45,
		      gdispGetFontMetric(fontSM, fontHeight),
		      tmp2,
		      fontSM, White, justifyRight);

  
  /* EFF supporter, +10% Defense and Logo */
  if (config->unlocks & UL_PLUSDEF) 
    putImageFile(IMG_EFFLOGO, 270, ypos+4);

  if (config->airplane_mode)
    putImageFile(IMG_PLANE, 0, 0);

  gdispCloseFont (fontXS);
  gdispCloseFont (fontLG);
  gdispCloseFont (fontSM);

}

static uint32_t default_init(OrchardAppContext *context) {
  (void)context;

  shout_ok = 1;

  orchardAppTimer(context, HEAL_INTERVAL_US, true);
  
  return 0;
}

static void default_start(OrchardAppContext *context) {
  DefaultHandles * p;

  p = chHeapAlloc (NULL, sizeof(DefaultHandles));
  context->priv = p;

  last_caesar_check = chVTGetSystemTime();

  gdispClear(Black);

  redraw_badge();
  draw_badge_buttons(p);
  
  geventListenerInit(&p->glBadge);
  gwinAttachListener(&p->glBadge);
  geventRegisterCallback (&p->glBadge, orchardAppUgfxCallback, &p->glBadge);
}

static inline void storeKey(OrchardAppContext *context, OrchardAppEventKeyCode key) { 
  /* remember the last pushed keys to enable the konami code */
  DefaultHandles * p;
  p = context->priv;

  for (int i=1; i < KEY_HISTORY; i++) {
    p->last_pushed[i-1] = p->last_pushed[i];
  }
  
  p->last_pushed[KEY_HISTORY-1] = key;
};

static inline uint8_t testKonami(OrchardAppContext *context) { 
  DefaultHandles * p;
  p = context->priv;

  for (int i=0; i < KEY_HISTORY; i++) {
    if (p->last_pushed[i] != konami[i]) 
      return false;
  }
  return true;
}

static void default_event(OrchardAppContext *context,
	const OrchardAppEvent *event) {
  DefaultHandles * p;
  GEvent * pe;
  userconfig *config = getConfig();
  tmElements_t dt;
  char tmp[40];

  p = context->priv;

  if (event->type == keyEvent) {
    if (event->key.flags == keyPress)
      storeKey(context, event->key.code);

    /* hitting enter goes into fight, unless konami has been entered,
       then we send you to the unlock screen! */
    if ( (event->key.code == keySelect) &&
         (event->key.flags == keyPress) )  {

      if (testKonami(context)) { 
        orchardAppRun(orchardAppByName("Unlocks"));
      } else {
        orchardAppRun(orchardAppByName("Fight"));
      }
      return;
    }

  }
  
  if (event->type == ugfxEvent) {
    pe = event->ugfx.pEvent;

    switch(pe->type) {
    case GEVENT_GWIN_BUTTON:
      if (((GEventGWinButton*)pe)->gwin == p->ghFightButton) {
        orchardAppRun(orchardAppByName("Fight"));
        return;
      }

      if (((GEventGWinButton*)pe)->gwin == p->ghExitButton) {
        orchardAppExit();
        return;
      }

      break;
    }
  }

  /* player healz */
  if (event->type == timerEvent) {
    /* draw the clock */
    if (rtc != 0) {
      font_t fontXS;
      fontXS = gdispOpenFont (FONT_XS);
        
      breakTime(rtc + ST2S((chVTGetSystemTime() - rtc_set_at)), &dt);
      chsnprintf(tmp, sizeof(tmp), "%02d:%02d:%02d\r\n", dt.Hour, dt.Minute, dt.Second);
      gdispCloseFont(fontXS);

      gdispFillArea( 0,0, 
                     60, gdispGetFontMetric(fontXS, fontHeight),
                     Black );
      
      gdispDrawStringBox (0,
                          0,
                          gdispGetWidth(),
                          gdispGetFontMetric(fontXS, fontHeight),
                          tmp,
                          fontXS, Grey, justifyLeft);

    }

    /* Caesar Election
     * ---------------
     * every 60 seconds...
     * If there are no nearby Caesars... 
     * ... and I am not caesar already ... 
     * and my level is >= 3 
     * and I can roll 1d10 and we get 1 to 3, then 
     * we become caesar
    */

    if ( (chVTGetSystemTime() - last_caesar_check) > CS_ELECT_INT ) { 
      last_caesar_check = chVTGetSystemTime();

      if ( (config->level >= 3) && 
           (config->current_type != p_caesar) && 
           (nearby_caesar() == FALSE))  {
        
        if (rand() % 6 == 1) {
          /* become Caesar for an hour */
          caesar_expires_at = chVTGetSystemTime() + MS2ST(3600000);
          /* Set myself to Caesar */
          
        }
      }
    }

    /* is it time to revert the player? */
    /* if so, revert the player */

    /* Heal the player -- 60 seconds to heal, or 30 seconds if you have the buff */
    if (config->hp < maxhp(config->unlocks, config->level)) { 
      config->hp = config->hp + ( maxhp(config->unlocks, config->level) / HEAL_TIME );
      if (config->unlocks & UL_HEAL) {
        // 2x heal if unlocked
        config->hp = config->hp + ( maxhp(config->unlocks, config->level) / HEAL_TIME ); 
      }
      
      // if we are now fully healed, save that, and prevent
      // overflow.
      if (config->hp >= maxhp(config->unlocks, config->level)) {
        config->hp = maxhp(config->unlocks, config->level);
        configSave(config);
        redraw_badge();
      } else {
        redraw_badge();
      }
    }
  }
}

static void default_exit(OrchardAppContext *context) {
  DefaultHandles * p;
  orchardAppTimer(context, 0, false); // shut down the timer
  
  p = context->priv;
  gwinDestroy (p->ghFightButton);
  gwinDestroy (p->ghExitButton);
  geventDetachSource (&p->glBadge, NULL);
  geventRegisterCallback (&p->glBadge, NULL, NULL);
  chHeapFree (context->priv);
  context->priv = NULL;

  shout_ok = 0;

  return;
}

orchard_app("Badge", 0, default_init, default_start,
	default_event, default_exit);
