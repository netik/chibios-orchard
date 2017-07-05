# Ides of DEF CON Firmware Changelog

04/05/2017 v1.1 Patchset 1
Fixes:

* cmd-peer.c: peers are created from levels 1-10 instead of all at level 9.
* cmd-peer.c: peers 

New:

* Peer simulation now creates simulated characters which are closer to 'real' XP and HP values.

Removes:

* cmd-peer.c: remove 'peerping' command, it pointed to empty code.


Git:

````
c0d93c3 (HEAD -> orchard-r2) Init the orchard app before showing the banners. Else, if someone attempts to manipulate the enemies table, we will segv because the enemies_mutex is not init'd.
e036584 (origin/orchard-r2, origin/HEAD) Update app_fight_state_flow.txt
f515808 (tag: dc25_release) slighty more correct json escapeing
````

04/03/2017 v1.0 Release (programmed at Macrofab to all badges)
badge.bin MD5: 88842d76a2258993b3f113c7e55c4b12

Known issues (not fixed in 1.0):

* cmd-peer.c: operations on the enemies/peers table will crash the badge if the system hasn't reached the main menu (fixed in 1.1)
* main.c / leaderboard: When dumping pings, if user name contains characters that are valid JSON, badge will emit invalid JSON to the leaderboard agent (fixed in 1.1)
* Bitbucket issue #104 - At times a double victory is possible

Not fixable without a replacement of all running badge code because fixes involve protocol changes.

* BB issue #102 - XP overflows at max uint16_t (65535)
* BB issue #103 - When XP Is > 10,000 display issues may occur
 
