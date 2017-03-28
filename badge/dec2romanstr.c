/* 
 * Convert a decimal number to roman numeral (up to uint16_t max)
 */

#include <string.h>

char *dec2romanstr(unsigned int num) {
  static char res[16] = "\0";
  res[0] = '\0';
  
  int del[] = {1000,900,500,400,100,90,50,40,10,9,5,4,1};
  char * sym[] = { "M", "CM", "D", "CD", "C", "XC", "L", "XL", "X", "IX", "V", "IV", "I" };

  int i = 0;
  while (num){
    while (num/del[i]){
      strcat(res, sym[i]);
      num -= del[i];
    }
    i++;
  }

  return(res);
}
