#ifndef __SOUND_H__
#define __SOUND_H__

extern void playTone(uint8_t freq, uint16_t duration);
extern void playStartupSong(void);
extern void playConfigReset(void);
extern void playNope(void);
extern void playHardFail(void);
extern void playAttacked(void);
extern void playVictory(void);
extern void playDefeat(void);
extern void playDodge(void);
extern void playHit(void);
extern void playPacman(void);
extern void playMsPacman(void);
extern void playCantina(void);
extern void playKombat(void);
extern void playMario(void);
extern void playDoh(void);
extern void playWoohoo(void);
extern void playDrWho(void);
extern void playRickroll(void);

#endif /* __SOUND_H__ */
