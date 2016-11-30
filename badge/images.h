
#define SHELL_BANNER "\r\n\r\n\
 .oooooo..o    o    ooooooooo.      o      .oooooo.         o    ooooooooo.   \r\n\
d8P'    `Y8 `8.8.8' `888   `Y88. `8.8.8'  d8P'  `Y8b     `8.8.8' `888   `Y88. \r\n\
Y88bo.      .8'8`8.  888   .d88' .8'8`8. 888      888    .8'8`8.  888   .d88' \r\n\
 `\"Y8888o.     \"     888ooo88P'     \"    888      888       \"     888ooo88P' \r\n\
     `\"Y88b          888                 888      888             888`88b.\r\n\
oo     .d8P          888                 `88b    d88b             888  `88b.\r\n\
 8\"\"88888P'         o888o                 `Y8bood8P'Ybd'         o888o  o888o\r\n"


#define SHELL_CREDITS "\r\n \
Defcon 25 Unofficial Badge Team\r\n \
\r\n\
John Adams (hw,sw)\r\n \
Bill Paul (hw,sw)\r\n \
Egan Hirvelda (game mechanics)\r\n \
Matthew (graphics,art)\r\n\r\n"

// defining this here because I don't want to keep looking it up
// and I want to save calls to getwidth/height
#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240

// Remember! Filenames must be in old school dos format (8x3)
#define IMG_SPLASH        "SPQR.RGB"
#define IMG_GUARD         "GUARD_01.RGB"
#define IMG_GUARD_IDLE_L  "GUARD_02.RGB"
#define IMG_GUARD_IDLE_R  "grdr02.rgb"
#define IMG_GROUND        "ground.rgb"

#define IMG_SPLASH_DISPLAY_TIME          5000 // longer if music playing
#define IMG_SPLASH_NO_SOUND_DISPLAY_TIME 1000

#define POS_PLAYER1_X  0
#define POS_PLAYER1_Y  45

#define POS_PLAYER2_X  180
#define POS_PLAYER2_Y  45

// used for enemy selection and stats when buttons on screen
#define POS_PCENTER_X  30
#define POS_PCENTER_Y  45

#define POS_FLOOR_Y    200
