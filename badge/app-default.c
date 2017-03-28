#include <string.h>

#include "ch.h"
#include "hal.h"
#include "chprintf.h"

#include "orchard-app.h"
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
#define HEAL_INTERVAL_US 1000000
#define HEAL_AMT 5

typedef struct _DefaultHandles {
  GHandle ghFightButton;
  GHandle ghExitButton;
  GListener glBadge;
} DefaultHandles;

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

  gwinWidgetClearInit(&wi);
  wi.g.show = TRUE;
  wi.g.width = 60;
  wi.g.height = 60;
  wi.g.y = totalheight - 60;
  wi.g.x = gdispGetWidth() - 60;
  wi.text = "";
  wi.customDraw = noRender;

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
    putImageFile(getAvatarImage(config->p_type, "deth", 2, false),
                 POS_PLAYER1_X, POS_PLAYER1_Y);
  } else 
    if (config->hp < (maxhp(config->unlocks,config->level) * .50)) {
      // show the whoop'd ass graphic if you're less than 15% 
      putImageFile(getAvatarImage(config->p_type, "deth", 1, false),
                   POS_PLAYER1_X, POS_PLAYER1_Y);
    }
    else
      putImageFile(getAvatarImage(config->p_type, "idla", 1, false),
                   POS_PLAYER1_X, POS_PLAYER1_Y);
  
    putImageFile(IMG_GROUND_BTNS, 0, POS_FLOOR_Y);

  fontXS = gdispOpenFont (FONT_XS);
  fontLG = gdispOpenFont (FONT_LG);
  fontSM = gdispOpenFont (FONT_FIXED);

  uint16_t ypos = 0; // cursor, so if we move or add things we don't have to rethink this
  uint16_t lmargin = 141; // cursor, so if we move or add things we don't have to rethink this
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
  
  gdispClear(Black);

  redraw_badge();
  draw_badge_buttons(p);
  
  geventListenerInit(&p->glBadge);
  gwinAttachListener(&p->glBadge);
  geventRegisterCallback (&p->glBadge, orchardAppUgfxCallback, &p->glBadge);
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
    // Enter to fight
    if ( (event->key.code == keySelect) &&
         (event->key.flags == keyPress) )  {
      orchardAppRun(orchardAppByName("Fight"));
      return;
    }
    // Left to go back to the launcher
    if ( (event->key.code == keyLeft) &&
         (event->key.flags == keyPress) )  {
	orchardAppExit();
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
    
    if (config->hp < maxhp(config->unlocks, config->level)) { 
        config->hp = config->hp + HEAL_AMT;
        if (config->unlocks & UL_HEAL) {
          // 2x heal if unlocked
          config->hp = config->hp + HEAL_AMT;
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
