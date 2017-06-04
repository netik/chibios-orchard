#ifdef MAKE_DTMF
#include <string.h>
#include <math.h>
#include "ch.h"
#include "hal.h"
#include "osal.h"
#include "ff.h"
#include "ffconf.h"
#include "dac_lld.h"
#include "chprintf.h"
#include "orchard.h"

/* 
 * Generate DTMF tones for our DAC without using the math library or
 * math trig functions.
 */

#define PI 3.14159265358979323846
#define TWOPI 2.0 * PI
#define TWO_OVER_PI 2.0 / PI

/*
 * DTMF Frequencies
 * 
 *           Upper Band
 *           1209 Hz 1336Hz 1477Hz 1633 Hz
 * Lower Band
 *   697 Hz       1       2      3      A
 *   770 Hz       4       5      6      B
 *   852 Hz       7       8      9      C
 *   941 Hz       *       0      #      D
 *
 **/

// Routines to compute sine and cosine to 3.2 digits
// of accuracy.
// from http://www.ganssle.com/approx/sincos.cpp)
//
//cos_32s computes cosine (x)
//
//  Accurate to about 3.2 decimal digits over the range [0, pi/2].
//  The input argument is in radians.
//
//  Algorithm:
//cos(x)= c1 + c2*x**2 + c3*x**4
//   which is the same as:
//cos(x)= c1 + x**2(c2 + c3*x**2)
//
static int dtmf_col[] = { 1209,1336,1477,1633 };
static int dtmf_row[] = { 697,770,852,941 };
static char dtmf_names[] = "123A456B789CS0PD";

static float cos_32s(float x)
{
  const float c1= 0.99940307;
  const float c2=-0.49558072;
  const float c3= 0.03679168;

  float x2;// The input argument squared

  x2=x * x;
  return (c1 + x2*(c2 + c3 * x2));
}

// we are including a local copy of floatmod so that
// we do not bring in the entire math library.
static float floatMod(float a, float b)
{
  return (a - b * floor(a / b));
}

// This is the main cosine approximation "driver"
// It reduces the input argument's range to [0, pi/2],
// and then calls the approximator.
// See the notes for an explanation of the range reduction.
//
static float cos_32(float x){
  int quad;// what quadrant are we in?

  // Get rid of values > 2* pi
  x=floatMod(x, TWOPI);

  if (x<0) // cos(-x) = cos(x) 
    x=-x;

  // was int()
  quad = x * TWO_OVER_PI;
  
  // Get quadrant # (0 to 3) we're in
  switch (quad) {
  case 0:
    return cos_32s(x);
    break;
  case 1:
    return -cos_32s(PI - x);
    break;
  case 2:
    return -cos_32s(x - PI);
    break;
  case 3:
    return cos_32s(TWOPI-x);
    break;
  }
  return 0;
}

int make_dtmf(void ) { 
  int16_t i,digit;
  int16_t sample;
  uint16_t usample;
  float dsinlow, dsinhigh;
  char fn[20];
  FIL f;
  UINT res;

  /* generate files on the SD card for every touch tone digit */
  for (digit = 0; digit < 16; digit++) {
    chsnprintf(fn, sizeof(fn), "dtmf-%c.raw", dtmf_names[digit]);
    
    if (f_open(&f, fn, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK) {
      return (-1);
    }

    /* Minimum DTMF duration is 85mS, we'll 
     * generate a 250mS touchtone, or 1/4 of DAC_SAMPLERATE
     */
    for(i = 0; i < DAC_SAMPLERATE/4; i++)
      {
        dsinlow = cos_32((PI/2.0) - (dtmf_row[digit / 4] * (2 * PI) * i) / DAC_SAMPLERATE);
        dsinhigh = cos_32((PI/2.0) - (dtmf_col[digit % 4] * (2 * PI) * i) / DAC_SAMPLERATE);
        
        // we want this at 50% of max amplitude to reduce distortion
        sample = (((dsinlow * INT16_MAX)/2) + ((dsinhigh * INT16_MAX)/2) ) * .3;

        // drop to 12bit
        usample = (float)sample;
        usample *= 0.0625;
        usample = round(usample);

        f_write(&f, &usample, sizeof(uint16_t), &res);
      }
    f_close(&f);
  }

  return(0);
}
     
#endif
