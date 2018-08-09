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

#include "ides_config.h"

#include "ides_gfx.h"
#include "unlocks.h"

#include "userconfig.h"
#include "datetime.h"
#include "dec2romanstr.h"

#include "dac_lld.h"

/* remember these many last key-pushes (app-default) */
#define KEY_HISTORY 9

static systime_t last_caesar_check = 0;
extern systime_t char_reset_at;

static int8_t lastimg = -1;

typedef struct _DefaultHandles {
  GHandle ghFightButton;
  GHandle ghExitButton;
  GListener glBadge;
  font_t fontLG, fontSM, fontXS;

  /* ^ ^ v v < > < > ent will fire the unlocks screen */
  OrchardAppEventKeyCode last_pushed[KEY_HISTORY];
} DefaultHandles;


const OrchardAppEventKeyCode konami[] = { keyUp, keyUp, keyDown, 
                                          keyDown, keyLeft, keyRight,
                                          keyLeft, keyRight, keySelect };
extern uint8_t shout_ok;

static void destroy_buttons(DefaultHandles *p) {
  gwinDestroy (p->ghFightButton);
  gwinDestroy (p->ghExitButton);
}

static void draw_buttons(DefaultHandles * p) {
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

  wi.g.x = gdispGetWidth() - 60;
  p->ghExitButton = gwinButtonCreate(NULL, &wi);
}

static void redraw_player(DefaultHandles *p) {
  // our image draws just a bit underneath the stats data. If we
  // draw the character during the HP update, it will blink and we
  // don't want that.
  const userconfig *config = getConfig();
  coord_t totalheight = gdispGetHeight();
  char tmp[20];
  char tmp2[40];
  char * img;
  int class;
  uint16_t hpx;
  
  if (config->hp < (maxhp(config->current_type, config->unlocks, config->level) * .25)) {
    if (lastimg != 2) {
      img = "deth";
      class = 2;
   }
    lastimg = 2;
  } else {
    class = 1;
    if (config->hp < (maxhp(config->current_type, config->unlocks,config->level) * .50)) {
      // show the whoop'd ass graphic if you're less than 15%
      if (lastimg != 1)
        img = "deth";
      lastimg = 1;
    } else {
      if (lastimg != 0) 
        img = "idla";
      lastimg = 0;
    }
  }

  putImageFile(getAvatarImage(config->current_type, img, class, false),
               POS_PLAYER1_X, totalheight - 42 - PLAYER_SIZE_Y);

  /* hit point bar */
  if (config->rotate)
    hpx = 61;
  else
    hpx = 141; 
      
  gdispDrawThickLine(hpx, 77, 320, 77, Red, 2, FALSE);
  gdispDrawThickLine(hpx, 98, 320, 98, Red, 2, FALSE);
  
  chsnprintf(tmp, sizeof(tmp), "HP");
  chsnprintf(tmp2, sizeof(tmp2), "%d", config->hp);

  gdispDrawStringBox (hpx+1,
		      83,
		      30,
		      gdispGetFontMetric(p->fontXS, fontHeight),
		      tmp,
		      p->fontXS, White, justifyLeft);

  drawProgressBar(hpx+22,83,120,11,maxhp(config->current_type, config->unlocks,config->level), config->hp, 0, false);

  gdispFillArea( hpx+148, 83,
                 30,gdispGetFontMetric(p->fontXS, fontHeight),
                 Black );
                       
  gdispDrawStringBox (hpx+148,
		      83,
                      30,
		      gdispGetFontMetric(p->fontXS, fontHeight),
		      tmp2,
		      p->fontXS, White, justifyLeft);
}

static void draw_stat (DefaultHandles * p,
                       uint16_t x, uint16_t y,
                       char * str1, char * str2) {
  uint16_t lmargin = 141;
  
  gdispDrawStringBox (lmargin + x,
		      y,
		      gdispGetFontMetric(p->fontSM, fontMaxWidth)*strlen(str1),
		      gdispGetFontMetric(p->fontSM, fontHeight),
		      str1,
		      p->fontSM, Yellow, justifyLeft);

  gdispDrawStringBox (lmargin + x + 15,
		      y,
                      72,
		      gdispGetFontMetric(p->fontSM, fontHeight),
		      str2,
		      p->fontSM, White, justifyRight);
  return;
}

static void redraw_badge(DefaultHandles *p) {
  // draw the entire background badge image. Shown when the screen is idle. 
  const userconfig *config = getConfig();
  coord_t totalheight = gdispGetHeight();
  coord_t totalwidth = gdispGetWidth();
  
  char tmp[20];
  char tmp2[40];
  uint16_t ypos = 0; 

  redraw_player(p);

  if (config->rotate) { 
    putImageFile(IMG_GROUND_BTN_ROT, 0, totalheight-40);
  } else {
    putImageFile(IMG_GROUND_BTNS, 0, totalheight-40);
  }

  gdispDrawStringBox (0,
		      ypos,
		      gdispGetWidth(),
		      gdispGetFontMetric(p->fontLG, fontHeight),
		      config->name,
		      p->fontLG, Yellow, justifyRight);

  chsnprintf(tmp, sizeof(tmp), "LEVEL %s", dec2romanstr(config->level));
  
  /* Level */
  ypos = ypos + gdispGetFontMetric(p->fontLG, fontHeight);
  gdispDrawStringBox (0,
		      ypos,
		      gdispGetWidth(),
		      gdispGetFontMetric(p->fontSM, fontHeight),
		      tmp,
		      p->fontSM, Yellow, justifyRight);

  ypos = ypos + gdispGetFontMetric(p->fontSM, fontHeight) + 4;

  /* end hp bar */
  ypos = ypos + 30;

  /* XP/WON */
  chsnprintf(tmp2, sizeof(tmp2), "%3d", config->xp);
  draw_stat (p, 0, ypos, "XP", tmp2);

  if (!config->rotate) {
    chsnprintf(tmp2, sizeof(tmp2), "%3d", config->won);
    draw_stat (p, 92, ypos, "WON", tmp2);
  }

  /* AGL / LOST */
  ypos = ypos + gdispGetFontMetric(p->fontSM, fontHeight) + 2;
  chsnprintf(tmp2, sizeof(tmp2), "%3d", config->agl);
  draw_stat (p, 0, ypos, "AGL", tmp2);

  if (!config->rotate) { 
    chsnprintf(tmp2, sizeof(tmp2), "%3d", config->lost);
    draw_stat (p, 92, ypos, "LOST", tmp2);
  }

  /* MIGHT */
  ypos = ypos + gdispGetFontMetric(p->fontSM, fontHeight) + 2;
  if (config->unlocks & UL_PLUSMIGHT) 
    chsnprintf(tmp2, sizeof(tmp2), "%3d", config->might + 1);
  else
    chsnprintf(tmp2, sizeof(tmp2), "%3d", config->might);
  draw_stat (p, 0, ypos, "MIGHT", tmp2);

  if (config->rotate) {
    ypos = ypos + gdispGetFontMetric(p->fontSM, fontHeight) + 2;
    chsnprintf(tmp2, sizeof(tmp2), "%3d", config->won);
    draw_stat (p, 0, ypos, "WON", tmp2);
    ypos = ypos + gdispGetFontMetric(p->fontSM, fontHeight) + 2;
    chsnprintf(tmp2, sizeof(tmp2), "%3d", config->lost);
    draw_stat (p, 0, ypos, "LOST", tmp2);
  }

  /* EFF supporter, +10% Defense and 50x35 logo*/
  if (config->unlocks & UL_PLUSDEF) 
    putImageFile(IMG_EFFLOGO, totalwidth-50, totalheight-85);

  if (config->airplane_mode)
    putImageFile(IMG_PLANE, 0, 0);

#ifdef LEADERBOARD_AGENT
  screen_alert_draw(false, "LB AGENT MODE");
#endif

}

static uint32_t default_init(OrchardAppContext *context) {
  (void)context;

  shout_ok = 1;

  orchardAppTimer(context, HEAL_INTERVAL_US, true);
  
  return 0;
}

static void default_start(OrchardAppContext *context) {
  DefaultHandles * p;
  const userconfig *config = getConfig();

  p = chHeapAlloc (NULL, sizeof(DefaultHandles));
  context->priv = p;
      
  p->fontXS = gdispOpenFont (FONT_XS);
  p->fontLG = gdispOpenFont (FONT_LG);
  p->fontSM = gdispOpenFont (FONT_FIXED);

  last_caesar_check = chVTGetSystemTime();

  gdispClear(Black);

  if (config->rotate) { 
    gdispSetOrientation (0);
  } else {
    gdispSetOrientation (GDISP_DEFAULT_ORIENTATION);
  }

  lastimg = -1;
  redraw_badge(p);
  draw_buttons(p);
  
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
  uint16_t lmargin;
  int lasttemp;
  coord_t totalheight = gdispGetHeight();
  coord_t totalwidth = gdispGetWidth();

  if (config->rotate)
    lmargin = 61;
  else
    lmargin = 141;
  
  p = context->priv;

  if (event->type == keyEvent) {
    if (event->key.flags == keyPress)
      storeKey(context, event->key.code);

    /* hitting enter or left goes into fight, unless konami has been entered,
       then we send you to the unlock screen! */
    if ( ( (event->key.code == keyLeft) || (event->key.code == keySelect) )&&
         (event->key.flags == keyPress) )  {

      if (testKonami(context) && (event->key.code == keySelect)) { 
        orchardAppRun(orchardAppByName("Unlocks"));
      } else {
        orchardAppRun(orchardAppByName("Fight"));
      }
      return;
    }

    /* hitting right will take you into setup */
    if ( (event->key.code == keyRight) &&
         (event->key.flags == keyPress) )  {
        orchardAppRun(orchardAppByName("Setup"));
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
    }
  }

  /* timed events (heal, caesar election, etc.) */
  if (event->type == timerEvent) {
    lasttemp = radioTemperatureGet (radioDriver);
    // we're going to display this in farenheit, I don't care about yo' celsius.
    lasttemp = (1.8 * lasttemp) + 32;

    if (rtc != 0) {
      /* draw the clock */
      breakTime(rtc + ST2S((chVTGetSystemTime() - rtc_set_at)), &dt);
      chsnprintf(tmp, sizeof(tmp), "%02d:%02d:%02d | %d F", dt.Hour, dt.Minute, dt.Second, lasttemp);
    } else {
      /* temp only */
      chsnprintf(tmp, sizeof(tmp), "%d F", lasttemp);
    }
    
    gdispFillArea( totalwidth - 100, totalheight - 50,
                   100, gdispGetFontMetric(p->fontXS, fontHeight),
                   Black );
    
    gdispDrawStringBox (0,
                        totalheight - 50,
                        totalwidth,
                        gdispGetFontMetric(p->fontXS, fontHeight),
                        tmp,
                        p->fontXS, White, justifyRight);
  
    /* draw class time remaining if any */
    if (char_reset_at != 0) {
      systime_t delta = ST2S((char_reset_at - chVTGetSystemTime()));
      systime_t minleft, secleft;
      minleft = delta / 60;
      secleft = delta - (minleft * 60);
      chsnprintf(tmp, sizeof(tmp), "%02d:%02d", minleft, secleft);

      /* if an unlockable class, show the class name next to the
         clock */
      if (config->current_type == p_caesar) {
        strcat(tmp," CAESAR");
      }

      if (config->current_type == p_bender) {
        strcat(tmp," BENDER");
      }

      if (config->current_type == p_senator) {
        strcat(tmp," SENATOR");
      }
      if (config->rotate) {
        gdispFillArea(0,
                      totalheight - 55 - PLAYER_SIZE_Y,
                      100,
                      gdispGetFontMetric(p->fontXS, fontHeight),
                      Black );
        
        gdispDrawStringBox( 0,
                            totalheight - 55 - PLAYER_SIZE_Y,
                            gdispGetWidth(),
                            gdispGetFontMetric(p->fontXS, fontHeight),
                            tmp,
                            p->fontXS, White, justifyLeft);
      } else {
        gdispFillArea( lmargin,
                       totalheight - 50,
                       60,
                       gdispGetFontMetric(p->fontXS, fontHeight),
                       Black );
        
        gdispDrawStringBox (lmargin,
                            totalheight - 50,
                            gdispGetWidth() - lmargin,
                            gdispGetFontMetric(p->fontXS, fontHeight),
                            tmp,
                            p->fontXS, White, justifyLeft);

      }
    }
#ifndef LEADERBOARD_AGENT
    /* Caesar Election
     * ---------------
     * Every 60 seconds...
     * If there are no nearby Caesars... 
     * ... and I am not caesar already ... 
     * and my level is >= MIN_CAESAR_LEVEL
     * and I can roll 1d100 and get <= CAESAR_CHANCE, then
     * we become caesar
    */

    if ( (chVTGetSystemTime() - last_caesar_check) > CS_ELECT_INT ) { 
      last_caesar_check = chVTGetSystemTime();

      if ( (config->level >= MIN_CAESAR_LEVEL) && 
           (config->current_type != p_caesar) && 
           (nearby_caesar() == FALSE) &&
           (config->airplane_mode == FALSE) )  {
        if (rand() % 100 <= CAESAR_CHANCE) { 
#ifdef DEBUG_FIGHT_STATE 
          chprintf(stream, "won election\r\n");
#endif
          /* become Caesar for an hour */
          char_reset_at = chVTGetSystemTime() + MAX_BUFF_TIME;
          config->current_type = p_caesar;
          config->hp = maxhp(config->current_type, config->unlocks, config->level);
          configSave(config);
          dacPlay("fight/leveiup.raw");
          screen_alert_draw(true, "YOU ARE NOW CAESAR! 2X XP");
          chThdSleepMilliseconds(ALERT_DELAY);
          gdispClear(Black);
          lastimg = -1;
          redraw_badge(p);
        } 
      }
    }
#endif
    /* is it time to revert the player? 
     * 
     * reverts bender, caesar after certain amount of time 
     * reboot will also forcibly reset this. 
     */
    if ( ( char_reset_at != 0) &&
         ( chVTGetSystemTime() >= char_reset_at ) ) {
      config->current_type = config->p_type;
      config->hp = maxhp(config->current_type, config->unlocks, config->level);
      char_reset_at = 0;
      configSave(config);

      dacPlay("fight/leveiup.raw");      
      screen_alert_draw(true, "VENI, VIDI, VICI...");
      chThdSleepMilliseconds(ALERT_DELAY);
      gdispClear(Black);
      lastimg = -1;
      redraw_badge(p);
    }
    /* if so, revert the player */

    /* Heal the player -- 60 seconds to heal, or 30 seconds if you have the buff */
    if ((config->hp < maxhp(config->current_type, config->unlocks, config->level)) && (config->current_type != p_caesar)) { 
      config->hp = config->hp + ( maxhp(config->current_type, config->unlocks, config->level) / HEAL_TIME );
      if (config->unlocks & UL_HEAL) {
        // 2x heal if unlocked
        config->hp = config->hp + ( maxhp(config->current_type, config->unlocks, config->level) / HEAL_TIME ); 
      }
      
      // if we are now fully healed, save that, and prevent
      // overflow.
      if (config->hp >= maxhp(config->current_type, config->unlocks, config->level)) {
        config->hp = maxhp(config->current_type, config->unlocks, config->level);
        configSave(config);
      }

      redraw_player(p);
    }
  }
}

static void default_exit(OrchardAppContext *context) {
  DefaultHandles * p;
  orchardAppTimer(context, 0, false); // shut down the timer
  
  p = context->priv;
  destroy_buttons(p);

  gdispCloseFont (p->fontXS);
  gdispCloseFont (p->fontLG);
  gdispCloseFont (p->fontSM);

  geventDetachSource (&p->glBadge, NULL);
  geventRegisterCallback (&p->glBadge, NULL, NULL);
  chHeapFree (context->priv);
  context->priv = NULL;

  shout_ok = 0;

  gdispSetOrientation (GDISP_DEFAULT_ORIENTATION);
  
  return;
}

orchard_app("Badge", "badge.rgb", 0, default_init, default_start,
	default_event, default_exit, 0);
