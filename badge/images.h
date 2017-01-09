
#define SHELL_BANNER "\r\n\r\n\
 .oooooo..o    o    ooooooooo.      o      .oooooo.         o    ooooooooo.   \r\n\
d8P'    `Y8 `8.8.8' `888   `Y88. `8.8.8'  d8P'  `Y8b     `8.8.8' `888   `Y88. \r\n\
Y88bo.      .8'8`8.  888   .d88' .8'8`8. 888      888    .8'8`8.  888   .d88' \r\n\
 `\"Y8888o.     \"     888ooo88P'     \"    888      888       \"     888ooo88P' \r\n\
     `\"Y88b          888                 888      888             888`88b.\r\n\
oo     .d8P          888                 `88b    d88b             888  `88b.\r\n\
 8\"\"88888P'         o888o                 `Y8bood8P'Ybd'         o888o  o888o\r\n"


#define SHELL_CREDITS "\r\n \
DEFCON 25 Unofficial Badge Team\r\n \
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
//                         12345678.123
#define IMG_SPLASH        "SPQR.RGB"

#define IMG_GROUND        "ground.rgb" // ground 
#define IMG_GROUND_BTNS   "grbutt.rgb" // ground with no buttons
#define IMG_GROUND_BCK    "grback.rgb" // ground with back button

#define IMG_BLOCK         "block.rgb"
#define IMG_ATTACK        "attack.rgb"

// guard
#define IMG_GATTH1        "gatth1.rgb"
#define IMG_GATTH1R       "gatth1r.rgb"
#define IMG_GATTL1        "gattl1.rgb"
#define IMG_GATTL1R       "gattl1r.rgb"
#define IMG_GATTM1        "gattm1.rgb"
#define IMG_GATTM1R       "gattm1r.rgb"
#define IMG_GCRCH1        "gcrch1.rgb"
#define IMG_GCRCH1R       "gcrch1r.rgb"
#define IMG_GIDLA1        "gidla1.rgb"
#define IMG_GIDLA1R       "gidla1r.rgb"

#define IMG_GUARD_IDLE_L  IMG_GIDLA1
#define IMG_GUARD_IDLE_R  IMG_GIDLA1R

// senator
#define IMG_SATTH1        "satth1.rgb"
#define IMG_SATTH1R       "sstth1r.rgb"
#define IMG_SATTL1        "sattl1.rgb"
#define IMG_SATTL1R       "sattl1r.rgb"
#define IMG_SATTM1        "sattm1.rgb"
#define IMG_SATTM1R       "sattm1r.rgb"
#define IMG_SCRCH1        "scrch1.rgb"
#define IMG_SCRCH1R       "scrch1r.rgb"
#define IMG_SIDLA1        "sidla1.rgb"
#define IMG_SIDLA1R       "sidla1r.rgb"

// casear
#define IMG_CATTH1        "catth1.rgb"
#define IMG_CATTH1R       "catth1r.rgb"
#define IMG_CATTL1        "cattl1.rgb"
#define IMG_CATTL1R       "cattl1r.rgb"
#define IMG_CATTM1        "cattm1.rgb"
#define IMG_CATTM1R       "cattm1r.rgb"
#define IMG_CCRCH1        "ccrch1.rgb"
#define IMG_CCRCH1R       "ccrch1r.rgb"
#define IMG_CIDLA1        "cidla1.rgb"
#define IMG_CIDLA1R       "cidla1r.rgb"

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
