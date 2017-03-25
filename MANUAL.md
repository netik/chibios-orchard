


Hardware Precautions
--------------------

WARNING! This device uses a Lithium-Ion Polymer Battery (LiPo.)

LiPo cells are affected by the same problems as other lithium-ion
cells. Overcharge, over-discharge, over-temperature, short circuit,
crush and nail penetration may all result in a catastrophic failure,
including the pouch rupturing, the electrolyte leaking, and
fire.

While we've gone through alot of trouble to protect you, don't be
stupid.  The LiPo cell present on this device has two levels of
protection. There is a protection circuit present on the battery
itself, and there is a low-dropoff / charging circuit on the board
which will turn off power if the LiPo drops too far down in voltage.

We have many additional LiPos available if you want to have a backup
battery. Ask for one!

Charging

By our estimates, the device should reach a full charge and read
4.2VDC in around 4-4.5 hours on

Charge Lifetime

The battery should last in the 8-15 hour range, depending on LED
brightness setting and use. Battery draw is around a constant 60mA with
occasional bursts to 300mA or 450mA on SD card access.

You can increase the battery's lifetime by reducing the brightness in
the setup menu or by picking LED patterns that don't activate all of
the LEDs.

Combat notes

* At the end of a round, damage is calculated.

* If you both pick the same attack (hi,mid,low) you will hear a
  'clank' sound and damage is reduced 20% for both of you. Damage will
  be displayed in Yellow

* If a hit is a normal hit, damage is displayed in Red.
  The formula for damage is:
  

* Either hit is a critical hit, you will hear two clanks, and damage
  will be 2x normal. Chance-to-Crit is per attack, and LUCK is the
  percentage of hits that will crit. This damage is always displayed
  in Purple.

Character Levels
------------------

On the enemy select screen, the character's name and level will be displayed in
the following colors, depending on your level.

Red:     Mob Level >= Char Level + 5
Orange:  Mob Level =  Char Level + (3 or 4)
Yellow:  Mob Level =  Char Level ± 2
Green:   Mob Level <= Char Level - 3, but above Gray Level
Gray:    Mob Level <= Gray Level

XP
---

XP is calculated similarly to WoW:

XP = (Char Level * 5) + 45, where Char Level = Mob Level, for mobs in Azeroth

XP = (Base XP) * (1 + 0.05 * (Mob Level - Char Level) ), where Mob Level > Char Level

ZD =  5, when Char Level =  1 -  7
ZD =  6, when Char Level =  8 -  9
ZD =  7, when Char Level = 10 - 11

Using the ZD values above, the formula for Mob XP for any mob of level
lower than your character is:

XP = (Base XP) * (1 - (Char Level - Mob Level)/ZD )
  where Mob Level < Char Level, and Mob Level > Gray Level


Outside links

Battery:
https://pkcell.en.alibaba.com/product/60307794162-220952477/Rechargeable_3_7v_1200mah_lipo_battery_for_electronical_products.html

