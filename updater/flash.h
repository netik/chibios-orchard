#ifndef _FLASH_H_
#define _FLASH_H_
extern void flashStart (void);
extern void flashErase (uint8_t * buf);
extern void flashProgram (uint8_t * src, uint8_t * dst);
#endif /* _FLASH_H_ */
