# Ides of DEF CON Firmware Changelog

## July 6th, 2017 v1.1 update

### Fixes

* cmd-peer.c: peers are created from levels 1-10 instead of all at level 9.
* cmd-peer.c: console operations on the enemies/peers table will no longer crash the badge during boot
* slight code cleanup on mutexes and enemy handling

### New

* **SINGLE_PLAYER**: It is now possible to play a one-player game with the simulated enemies/peers
* **SINGLE_PLAYER**: Holding down RIGHT on the joypad at boot will generate enemies for you to play against. If successful, you will hear a quick F-major chord and the message 'Test enemies generated' will appear on the console.
* **SINGLE_PLAYER**: 
* Peer simulation now creates simulated characters which are closer to 'real' XP and HP values.

### Removes

* cmd-peer.c: remove 'peerping' command, it pointed to empty code.
* app-dialer.c-jna: old useless code

### Git

````
	d2bdd48 Add SINGLE_PLAYER mode.
	d6a184b fix level/hp calculation for peersim created enemies
	799ea9c Add UL_SIMULATED for future support of single-player mode
	5c7e8bf add updated TODO
	bcf8b3d Merge branch 'orchard-r2' of https://github.com/netik/chibios-orchard into orchard-r2
	1613a7b add changelog
	7db78a1 Fix a search and replace botch
	c0d93c3 Init the orchard app before showing the banners. Else, if someone attempts to manipulate the enemies table, we will segv because the enemies_mutex is not init'd.
	e036584 Update app_fight_state_flow.txt
````

## 06/21/2017 v1.0 release

badge.bin MD5: 88842d76a2258993b3f113c7e55c4b12

### Known issues (not fixed in 1.0):

* main.c / leaderboard: When dumping pings, if user name contains characters that are valid JSON, badge will emit invalid JSON to the leaderboard agent (fixed in 1.1)
* Bitbucket issue #104 - At times a double victory is possible
* BB issue #103 - When XP Is > 10,000 display issues may occur

### Not fixable 
( without a replacement of all running badge code because fixes involve protocol changes. )

* BB issue #102 - XP overflows at max uint16_t (65535)
