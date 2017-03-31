#define random8(a,b) ((rand() % b) + a)
typedef uint8_t   fract8;

/// Representation of an RGB pixel (Red, Green, Blue)
typedef struct _CRGB {
      union {
        uint8_t r;
        uint8_t red;
      };
      union {
        uint8_t g;
        uint8_t green;
      };
      union {
        uint8_t b;
        uint8_t blue;
      };
} CRGB;

