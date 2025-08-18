
![POV: Cross busts your sorry ass at Rockport's hottest bar.](Thumbnail.jpg "Graphic design is my passion.")

Bartender adds **new customisation options** to pursuits in *Need for Speed: Most Wanted* (2005). These new options come in two feature sets:
* the **"Basic" feature set** lets you change many otherwise hard-coded values of the game, and
* the **"Advanced" feature set** lets you [change cop-spawning behaviour and tables without limits](https://youtu.be/XwFSpc97hF4).

&nbsp;

The sections below **address these questions** in detail:
1. [What does the "Basic" feature set do?](#1---what-does-the-basic-feature-set-do)
2. [What does the "Advanced" feature set do?](#2---what-does-the-advanced-feature-set-do)
3. [What mods are (in)compatible with Bartender?](#3---what-mods-are-incompatible-with-bartender)
4. [How do I install Bartender for my game?](#4---how-do-i-install-bartender-for-my-game)
5. [How may I share or bundle Bartender?](#5---how-may-i-share-or-bundle-bartender)

&nbsp;

&nbsp;

&nbsp;



# 1 - What does the "Basic" feature set do?

The "Basic" feature set **lets you change** (per Heat level)
* how quickly and at what distances from cops the red "BUSTED" bar fills,
* how quickly the green "EVADE" bar fills once all cops have lost sight of you,
* at what time interval you gain passive bounty,
* the maximum combo-bounty multiplier for destroying cops quickly,
* the internal cooldown for regular roadblock spawns,
* the internal cooldown for Heavy / LeaderStrategy spawns,
* what vehicles spawn through HeavyStrategy 3 (the ramming SUVs),
* what vehicles spawn through HeavyStrategy 4 (the roadblock SUVs), and
* what vehicles spawn through LeaderStrategy 5 / 7 (Cross and his henchmen).

&nbsp;

The "Basic" feature set **fixes five bugs**:
* helicopters no longer cast static shadows (like cars do) with incorrect placements,
* Heat levels > 5 are no longer reset back to 5 when you enter free-roam or start an event,
* Heat levels > 5 are now shown correctly in menus (requires [Binary](https://github.com/SpeedReflect/Binary/releases) for missing textures),
* you can no longer get busted due to line-of-sight issues while the "EVADE" bar fills, and
* regular roadblock and Heavy / LeaderStrategy spawns no longer slow down in longer pursuits.

&nbsp;

You can also **assign new (Binary) strings** for the game to display when cop vehicles are destroyed, similar to the [NFSMW Unlimiter mod](https://github.com/nlgxzef/NFSMWUnlimiter/releases) by nlgxzef. Bartender's version of this feature is easier to configure and will never cause `FENG: Default string error` pop-ups, as it checks every string label you provide against the game's (potentially modified) language files whenever you launch it.

&nbsp;

&nbsp;

&nbsp;



# 2 - What does the "Advanced" feature set do?

The "Advanced" feature set **lets you change** (per Heat level)
* how many cops can (re)spawn without backup once a wave is exhausted,
* the global cop-spawn limit for how many cops in total may chase you at any given time,
* how quickly (if at all) cops flee the pursuit if they don't belong,
* what vehicles (any amount, with counts and chances) may spawn to chase and search for you,
* what vehicles (same liberties as above) may spawn in regular roadblocks,
* what vehicles (ditto) may spawn as pre-generated cops in scripted events,
* what vehicles (without counts) may spawn as free patrols when there is no active pursuit,
* what vehicle spawns in place of the regular helicopter, and
* when exactly (if at all) the helicopter can (de / re)spawn.

&nbsp;

The "Advanced" feature set **fixes two bugs**:
* HeavyStrategy 3 spawns no longer have a chance to abort their ramming attempts instantly, and
* the engagement count shown above the pursuit board now (accurately) counts chasing cops only.

&nbsp;

&nbsp;

&nbsp;



# 3 - What mods are (in)compatible with Bartender?

Bartender should be **fully compatible** with all [VltEd](https://nfs-tools.blogspot.com/2019/02/nfs-vlted-v46-released.html) and [Binary](https://github.com/SpeedReflect/Binary/releases) mods. Other .asi mods without pursuit features should also be compatible unless they are listed below.

&nbsp;

Some popular .asi mods **require (re)configuration** to be compatible with Bartender:
* In [NFSMW ExtraOptions](https://github.com/ExOptsTeam/NFSMWExOpts/releases) by ExOptsTeam, disable the `HeatLevelOverride` feature.
* In [NFSMW Unlimiter](https://github.com/nlgxzef/NFSMWUnlimiter/releases) by nlgxzef, disable the `EnableCopDestroyedStringHook` feature.
* In [NFSMW LimitAdjuster](https://zolika1351.pages.dev/mods/nfsmwlimitadjuster) by Zolika1351, disable every cop-related feature under `[Options]`.

&nbsp;

&nbsp;

&nbsp;



# 4 - How do I install Bartender for my game?

**Before installing** Bartender:
1. make sure you have read and understood the [section about mod (in)compatibility](#3---what-mods-are-(in)compatible-with-bartender) above,
2. make sure your game's `speed.exe` is compatible (i.e. 5.75 MB / 6,029,312 bytes large), and
3. install an .asi loader or any mod with one (e.g. the [WideScreenFix mod](https://github.com/ThirteenAG/WidescreenFixesPack/releases/tag/nfsmw) by ThirteenAG).

&nbsp;

To **install** Bartender:
1. if it doesn't exist already, create a `scripts` folder in your game's installation folder;
2. copy the `BartenderSettings` folder and .asi file into your game's `scripts` folder;
3. if Bartender's .asi file gets flagged by your antivirus software, whitelist the file; and
4. (optional) in User Mode of [Binary 2.8.3](https://github.com/SpeedReflect/Binary/releases/tag/v2.8.3) or newer, load and apply `FixMissingTextures.end`.

&nbsp;

**After installing** Bartender: 
1. check out its configuration (.ini) files in the `scripts\BartenderSettings` folder, and
2. see the [appendix file](APPENDIX.md) should you encounter any issues or want more feature details.

&nbsp;

To **uninstall** Bartender, remove its files from your game's `scripts` folder. There is no need to remove the optional missing textures, as the game won't ever use them without Bartender.

&nbsp;

To **update** Bartender, uninstall it and repeat the installation process above. Whenever you update, make sure to replace *all* old configuration files!

&nbsp;

&nbsp;

&nbsp;



# 5 - How may I share or bundle Bartender?

You are free to bundle Bartender and its files with your own pursuit mod, **no credit required**. In the interest of code transparency, however, consider linking to [Bartender's GitHub repository](https://github.com/rng-guy/NFSMWBartender) somewhere in your mod's documentation (e.g. README).

&nbsp;

Finally, Bartender would not have seen the light of day without
* **DarkByte**, for [Cheat Engine](https://www.cheatengine.org/);
* **[rx](https://github.com/rxyyy)**, for encouraging me to try .asi modding;
* **[nlgxzef](https://github.com/nlgxzef)**, for the Most Wanted debug symbols;
* **GuidedHacking**, for their [Cheat Engine tutorials](https://www.youtube.com/playlist?list=PLt9cUwGw6CYFSoQHsf9b12kHWLdgYRhmQ);
* **Matthias C. M. Troffaes**, for his [configuration-file parser](https://github.com/mcmtroffaes/inipp);
* **[ExOptsTeam](https://github.com/ExOptsTeam)**, for permitting me to use their Heat-level fixes;
* **Sebastiano Vigna**, for his [pseudorandom number generators](https://prng.di.unimi.it/);
* **trelbutate**, for his [NFSMW Cop Car Healthbars mod](https://github.com/trelbutate/MWHealthbars/) as a resource; and
* **Orsal**, **Aven**, **Astra King79**, and **MORELLO**, for testing and providing feedback.