
![POV: Cross busts your sorry ass at Rockport's hottest bar.](Thumbnail.jpg "Graphic design is my passion.")

Bartender adds **new customisation options** to pursuits in *Need for Speed: Most Wanted* (2005). These new options come in two feature sets:
* the **"Basic" feature set** lets you change many otherwise hard-coded values of the game, and
* the **"Advanced" feature set** lets you [change cop-spawning behaviour and tables without limits](https://youtu.be/s0U-1PTUqYI).

&nbsp;

For **optimal customisability**, use [VltEd](https://nfs-tools.blogspot.com/2019/02/nfs-vlted-v46-released.html) along with Bartender. Bartender doesn't replace VltEd, as Bartender's features are deliberately limited to things you cannot do with VltEd alone.

&nbsp;

The **sections below** address these questions in detail:
1. [What does the "Basic" feature set do?](#1---what-does-the-basic-feature-set-do)
2. [What does the "Advanced" feature set do?](#2---what-does-the-advanced-feature-set-do)
3. [What mods are (in)compatible with Bartender?](#3---what-mods-are-incompatible-with-bartender)
4. [What other mods does Bartender depend on?](#4---what-other-mods-does-bartender-depend-on)
5. [How do I install Bartender for my game?](#5---how-do-i-install-bartender-for-my-game)
6. [How may I share or bundle Bartender?](#6---how-may-i-share-or-bundle-bartender)

&nbsp;

&nbsp;

&nbsp;



# 1 - What does the "Basic" feature set do?

The "Basic" feature set **lets you change** (per Heat level)
* at what time interval you gain passive bounty,
* the maximum combo-bounty multiplier for destroying cops quickly,
* how quickly and below what distance from cops the red "BUSTED" bar fills,
* how quickly the green "EVADE" bar fills once all cops have lost sight of you,
* whether player-damaged cop vehicles are destroyed instantly if flipped over,
* when exactly (if at all) cop vehicles are destroyed regardless of damage if flipped over,
* when exactly (if at all) racer vehicles are reset if flipped over,
* which of the enabled ground supports the cops may request in non-player pursuits,
* the internal cooldown between non-Strategy roadblock requests,
* the internal cooldown between Heavy / LeaderStrategy requests,
* under which conditions and to what extent (if at all) roadblock vehicles can join pursuits,
* whether roadblock vehicles react to racers entering "COOLDOWN" mode and / or hitting spikes,
* whether HeavyStrategy 3 requests interact with roadblock requests and spawns,
* which vehicles spawn in place of the ramming SUVs through HeavyStrategy 3,
* which vehicles spawn in place of the roadblock SUVs through HeavyStrategy 4, and
* which vehicles spawn in place of Cross and his henchmen through LeaderStrategy 5 / 7.

&nbsp;

The "Basic" feature set **also lets you change** (in general)
* which notification string the game displays whenever you destroy a given cop vehicle;
* which radio callsigns and chatter a given cop vehicle can trigger in player pursuits;
* under which conditions (if at all) a given cop vehicle shows up on the radar / mini-map;
* how (if at all) line of sight affects the colour of the helicopter's cone-of-vision icon; and
* the selection, order, and length of interactive themes that play during player pursuits.

&nbsp;

The "Basic" feature set **fixes six bugs / issues**:
* the helicopter mini-map icon is now always visible whenever a helicopter is active,
* helicopters no longer cast static shadows (like cars do) with incorrect placements,
* the game is no longer inadvertently biased in how it chooses to make Strategy requests,
* Heat levels > 5 are no longer reset back to 5 when you enter free-roam or start an event,
* Heat levels > 5 are now shown correctly in menus (requires [Binary](https://github.com/SpeedReflect/Binary/releases) for missing textures), and
* you can no longer get busted due to line-of-sight issues while the green "EVADE" bar fills.

&nbsp;

&nbsp;

&nbsp;



# 2 - What does the "Advanced" feature set do?

The "Advanced" feature set **lets you change** (per Heat level)
* how many chasing cops may (re)spawn regardless of the remaining engagement count,
* below what total number of active cops in the world the game may spawn new chasing cops,
* whether spawning decisions for chasing cops are independent of all other pursuit vehicles,
* how far away new chasing cops must spawn from all already active cops,
* when exactly (if at all) and how many chasing cops from other Heat levels can flee the pursuit,
* which vehicles (any amount, with counts and chances) may spawn to chase and search for racers,
* which vehicles (same liberties as above) may spawn in non-Strategy roadblocks,
* which vehicles (ditto) may spawn as pre-generated cops in scripted events,
* which vehicles (ditto again) may spawn as free patrols outside pursuits,
* which vehicle spawns in place of the regular helicopter,
* when exactly (if at all) the helicopter can first spawn in each player pursuit,
* when exactly (if at all) the helicopter can respawn if it runs out of fuel,
* when exactly (if at all) the helicopter can respawn if it gets wrecked,
* when exactly (if at all) the helicopter can rejoin the pursuit early if it loses you,
* when exactly (if at all) the helicopter can run out of fuel after each (re)spawn,
* the internal cooldown between the helicopter's ramming attempts through HeliStrategy 2,
* below what racer speed HeavyStrategy 3 spawns cancel their ramming attempts early,
* when exactly (if at all) LeaderStrategy Cross and / or his henchmen become aggressive,
* when exactly (if at all) the game can request a new LeaderStrategy once Cross is gone, and
* when exactly (if at all) the game can request a new Strategy while another is still active.

&nbsp;

The "Advanced" feature set **also lets you change** (in general)
* which non-chasing cops are also tracked by the engagement count shown above the pursuit board.

&nbsp;

The "Advanced" feature set **fixes three bugs / issues**:
* non-Strategy roadblock and Heavy / LeaderStrategy requests are no longer disabled in races,
* early Strategy despawns or cancellations no longer stall the game from making new requests, and
* the engagement count shown above the pursuit board now always tracks relevant cops accurately.

&nbsp;

&nbsp;

&nbsp;



# 3 - What mods are (in)compatible with Bartender?

Almost all **[VltEd](https://nfs-tools.blogspot.com/2019/02/nfs-vlted-v46-released.html) and [Binary](https://github.com/SpeedReflect/Binary/releases) mods** should be fully compatible with all Bartender configurations. However, Bartender's "Advanced" feature set overrides some `pursuitlevels` VltEd parameters:
* the `cops` array,
* `HeliFuelTime`,
* `TimeBetweenHeliActive`, and
* `SearchModeHeliSpawnChance`.

&nbsp;

Most **other .asi mods** should be fully compatible with all Bartender configurations. However, some pursuit-related .asi mods require manual (re)configuration for compatibility:
* In [NFSMW ExtraOptions](https://github.com/ExOptsTeam/NFSMWExOpts/releases) by ExOptsTeam, disable the `HeatLevelOverride` feature.
* In [NFSMW Unlimiter](https://github.com/nlgxzef/NFSMWUnlimiter/releases) by nlgxzef, disable the `EnableCopDestroyedStringHook` feature.
* For [XNFSMusicPlayer](https://github.com/xan1242/XNFSMusicPlayer/releases) by xan1242, delete Bartender's `[Music:Playlist]` parameter group.
* For [NFSMW LimitAdjuster](https://zolika1351.pages.dev/mods/nfsmwlimitadjuster) by Zolika1351, see the [section about dependencies](#4---what-other-mods-does-bartender-depend-on) below.

&nbsp;

&nbsp;

&nbsp;



# 4 - What other mods does Bartender depend on?

Under certain conditions, Bartender **may require** the [NFSMW LimitAdjuster](https://zolika1351.pages.dev/mods/nfsmwlimitadjuster) mod by Zolika1351. Specifically, you likely need that mod if you configure Bartender in any of the following ways:
* `[Chasers:Limits]` in `Cars.ini`: You define a global cop-spawn limit > 8.
* `[Chasers:Independence]` in `Cars.ini`: You enable independent spawns for chasing cops.
* `[Joining:Definitions]` in `Supports.ini`: You make joining from roadblocks frequent.
* `[Heavy3:Unblocking]` in `Strategies.ini`: You define very short unblock delays.

&nbsp;
 
**To configure** [NFSMW LimitAdjuster](https://zolika1351.pages.dev/mods/nfsmwlimitadjuster) for optimal compatibility with Bartender:
1. under `[Limits]` in `NFSMWLimitAdjuster.ini`, set the `PursuitCops` parameter to 255; and
2. under `[Options]` in `NFSMWLimitAdjuster.ini`, disable every cop-related feature.

&nbsp;

&nbsp;

&nbsp;



# 5 - How do I install Bartender for my game?

**Before installing** Bartender:
1. read and understand the two sections about [mod (in)compatibilities](#3---what-mods-are-incompatible-with-bartender) and [dependencies](#4---what-other-mods-does-bartender-depend-on) above,
2. make sure your game's `speed.exe` is compatible (i.e. 5.75 MB / 6,029,312 bytes large), and
3. install an .asi loader or any mod with one (e.g. the [WideScreenFix mod](https://github.com/ThirteenAG/WidescreenFixesPack/releases/tag/nfsmw) by ThirteenAG).

&nbsp;

**To install** Bartender:
1. if it doesn't exist already, create a `scripts` folder in your game's installation folder;
2. copy the `BartenderSettings` folder and .asi file into your game's `scripts` folder;
3. if Bartender's .asi file gets flagged by your antivirus software, whitelist the file; and
4. (optional) in User Mode of [Binary 2.8.3](https://github.com/SpeedReflect/Binary/releases/tag/v2.8.3) or newer, load and apply `FixMissingTextures.end`.

&nbsp;

**After installing** Bartender: 
1. edit the configuration (.ini) files in the `BartenderSettings` folder to your liking; and
2. if you encounter any issues or want more feature details, see the [appendix file](APPENDIX.md).

&nbsp;

**To uninstall** Bartender, remove its files from your game's `scripts` folder. There's no need to remove the optional missing textures, as the game doesn't use them without Bartender.

&nbsp;

**To update** Bartender, uninstall it and repeat the installation process above. If you update from a version older than v2.04.01, replace all old configuration files.

&nbsp;

&nbsp;

&nbsp;



# 6 - How may I share or bundle Bartender?

You are free to bundle Bartender and its files with your own pursuit mod, **no credit required**. In the interest of code transparency, however, consider linking to [Bartender's GitHub repository](https://github.com/rng-guy/NFSMWBartender) somewhere in your mod's documentation (e.g. README).

&nbsp;

Finally, Bartender wouldn't have seen the light of day without
* **DarkByte**, for [Cheat Engine](https://www.cheatengine.org/);
* **[rx](https://github.com/rxyyy)**, for encouraging me to try .asi modding;
* **[nlgxzef](https://github.com/nlgxzef)**, for the Most Wanted debug symbols;
* **GuidedHacking**, for their [Cheat Engine tutorials](https://www.youtube.com/playlist?list=PLt9cUwGw6CYFSoQHsf9b12kHWLdgYRhmQ);
* **Matthias C. M. Troffaes**, for his [configuration-file parser](https://github.com/mcmtroffaes/inipp);
* **[ExOptsTeam](https://github.com/ExOptsTeam)**, for permitting me to use their Heat-level fixes;
* **Sebastiano Vigna**, for his [pseudorandom number generators](https://prng.di.unimi.it/);
* **Martin Leitner-Ankerl**, for his [performant hashmap implementation](https://github.com/martinus/unordered_dense);
* **trelbutate**, for his [NFSMW Cop Car Healthbars mod](https://github.com/trelbutate/MWHealthbars/) as a resource; and
* **[Orsal](https://nfsmods.xyz/usermods/20407)**, **[Aven](https://nfsmods.xyz/usermods/1610)**, **Astra King79**, and **[MORELLO](https://nfsmods.xyz/usermods/6960)**, for testing and providing feedback.