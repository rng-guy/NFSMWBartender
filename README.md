
![POV: Cross busts your sorry ass at Rockport's hottest bar.](Thumbnail.jpg "Graphic design is my passion.")

Bartender adds **new customisation options** to pursuits in *Need for Speed: Most Wanted* (2005). These options come in two feature sets:
* the **"Basic" feature set** lets you change many otherwise hard-coded values of the game, and
* the **"Advanced" feature set** lets you [change cop-spawning behaviour and tables without limits](https://youtu.be/XwFSpc97hF4).

&nbsp;

The sections below **address the following questions** in detail:
1. [What does the "Basic" feature set do?](#1---what-does-the-basic-feature-set-do)
2. [What does the "Advanced" feature set do?](#2---what-does-the-advanced-feature-set-do)
3. [What should I know before I use Bartender?](#3---what-should-i-know-before-i-use-bartender)
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

The "Basic" feature set **fixes four bugs**:
* Heat levels > 5 are no longer reset back to 5 when you enter free-roam or start an event,
* Heat levels > 5 are now shown correctly in menus (requires [Binary](https://github.com/SpeedReflect/Binary/releases) for missing textures),
* you can no longer get busted due to line-of-sight issues while the "EVADE" bar fills, and
* regular roadblock and Heavy / LeaderStrategy spawns no longer slow down in longer pursuits.

&nbsp;

You can also **assign new (Binary) strings** for the game to display when cop vehicles are destroyed, similar to the [NFSMW Unlimiter mod](https://github.com/nlgxzef/NFSMWUnlimiter/releases) by nlgxzef. Bartender's version of this feature is easier to configure and will never cause `FENG: Default string error` pop-ups, as it checks every string you provide against the game's (potentially modified) language files whenever you launch it.

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

The "Advanced" feature set **also fixes** the engagement count displayed above the pursuit board: The count now accurately reflects how many chasing cop spawns remain in the current wave by disregarding vehicles that spawn through any Heavy / LeaderStrategy, vehicles that join the pursuit by detaching themselves from roadblocks, and the helicopter.

&nbsp;

&nbsp;

&nbsp;



# 3 - What should I know before I use Bartender?

If you configure Bartender improperly, it might cause **stability issues** in your game due to how much control it gives you over the game's cop-spawning logic. Also, there are a few quirks to the way Bartender parses its configuration files that you should be aware of before you edit any of them.

&nbsp;

To help you **avoid game instability**, the subsections below contain
* everything you need to make informed edits to Bartender's configuration files, and
* detailed compatibility notes for mods known to conflict with any Bartender features.

&nbsp;

Barring any exceptions mentioned in the subsections below, Bartender should be **fully compatible** with all [VltEd](https://nfs-tools.blogspot.com/2019/02/nfs-vlted-v46-released.html) and [Binary](https://github.com/SpeedReflect/Binary/releases) mods. Any .asi mods without pursuit features should also be compatible.

&nbsp;

&nbsp;

## 3.1 - What should I know about the file parsing?

Some **parameter groups** (indicated by `[GroupName]`) in Bartender's configuration files allow you to provide a `default` value. For each parameter group, a comment in the relevant file states whether the group allows this. Bartender parses groups that allow `default` values in three steps:
1. If you omitted it, the `default` value is set to the game's vanilla (i.e. unmodded) value.
2. All free-roam Heat levels (format: `heatXY`) you omitted are set to the `default` value.
3. All race Heat levels (format: `raceXY`) you omitted are set to their free-roam values.

&nbsp;

Bartender can handle any **invalid values** you might provide in its configuration files:
* negative values that should be positive are set to 0 during parsing instead,
* values of incorrect type (e.g. a string instead of a decimal) count as omitted,
* mismatched interval values (i.e. where `max` < `min`) are each set to the lower value, and
* comma-separated value pairs / tuples with too many or too few (valid) values count as omitted.

&nbsp;

&nbsp;

## 3.2 - What should I know about the "Basic" feature set?

Regarding the "Basic" feature set **in general**:

* The configuration (.ini) files for this feature set are located in `BartenderSettings/Basic`.

* The unedited configuration files for this feature set use the game's vanilla values.

* You can disable this entire feature set by deleting all of its configuration files.

* You can disable any feature of this set by deleting the file containing its parameters. Bug fixes, however, don't have parameters and are tied to specific files implicitly instead.

* To disable the two Heat-level fixes, delete all configuration files of this feature set.

* The Heat-level reset fix is incompatible with the `HeatLevelOverride` feature of the [NFSMW ExtraOptions mod](https://github.com/ExOptsTeam/NFSMWExOpts/releases) by ExOptsTeam. To disable this ExtraOptions feature, edit its `NFSMWExtraOptionsSettings.ini` configuration file. If you do this, you can still change the maximum available Heat level with VltEd: The `0xe8c24416` parameter of a given `race_bin_XY` VltEd entry determines the maximum Heat level (1-10) at Blacklist rival #XY.

* If you don't install the optional missing textures (`FixMissingTextures.end`), then the game will not display a number next to Heat gauges in menus for cars with Heat levels above 5. Whether you install these textures does not affect the Heat-level reset fix in any way.

&nbsp;

Regarding **cop (Binary) strings** (`BartenderSettings\Basic\Labels.ini`):

* This feature is incompatible with the `EnableCopDestroyedStringHook` feature of the [NFSMW Unlimiter mod](https://github.com/nlgxzef/NFSMWUnlimiter/releases) by nlgxzef. Either delete Bartender's `Labels.ini` configuration file or disable Unlimiter's version of the feature by editing its `NFSMWUnlimiterSettings.ini` file.

&nbsp;

Regarding **ground supports** (`BartenderSettings\Basic\Supports.ini`):

* Deleting this file also disables the fix for slower roadblock and Heavy / LeaderStrategy spawns.

* You should assign low `MAXIMUM_AI_SPEED` values (~50) to the `aivehicle` VltEd entries of all vehicles you provide as replacements for the ramming SUVs in HeavyStrategy 3 spawns. If you don't limit their speeds, they might cause stability issues by joining the pursuit as regular cops (regardless of the global cop-spawn limit) after their ramming attempt(s).

* You should not use the vehicles you provide as replacements for Cross in LeaderStrategy spawns anywhere else in the game, as they will block LeaderStrategy spawns whenever they are present.

&nbsp;

Regarding **uncategorised features** (`BartenderSettings\Basic\Others.ini`):

* Deleting this file also disables the fix for getting busted while the "EVADE" bar fills.

&nbsp;

&nbsp;

## 3.3 - What should I know about the "Advanced" feature set?

Regarding the "Advanced" feature set **in general**:

* The configuration (.ini) files for this feature set are located in `BartenderSettings/Advanced`.

* The unedited configuration files for this feature set use the game's vanilla values.

* You can disable this entire feature set by deleting all of its configuration files.

* You can disable any feature of this set by deleting the file containing its parameters. This does not apply to the engagement-count fix, which is tied to this entire feature set instead.

* You must provide at least one vehicle in the "Chasers" spawn table of each free-roam Heat level (`BartenderSettings\Advanced\Cars.ini`), else Bartender disables this entire feature set.

* You should ensure that every HeavyStrategy you enable in a given Heat level's `pursuitsupport` VltEd entry is only listed there once (e.g. there is not a second HeavyStrategy 3), and that there is also no more than one LeaderStrategy listed. Otherwise, such duplicates can cause the game (and Bartender) to misread their `Duration` VltEd parameters.

* If this feature set is enabled, the following `pursuitlevels` VltEd parameters are ignored because this feature set fulfils their intended purposes with extended customisation: `cops`, `HeliFuelTime`, `TimeBetweenHeliActive`, and `SearchModeHeliSpawnChance`.

&nbsp;

Regarding **helicopter (de / re)spawning** (`BartenderSettings\Advanced\Helicopter.ini`):

* You must assign the `CHOPPER` class to the `pvehicle` VltEd entries of all vehicles you provide as replacements for the regular helicopter. You can do this directly or through parent entries.

&nbsp;

Regarding **cop (de / re)spawning** (`BartenderSettings\Advanced\Cars.ini`):

* Until HeavyStrategy 3 and LeaderStrategy spawns have left the pursuit, they can block new "Chasers" from spawning (but not the other way around). This is vanilla behaviour: These spawns also count toward the total number of cops loaded by the game, and the game then compares this number against the global cop-spawn limit to make spawn decisions for "Chasers". Cops spawned in NPC pursuits can also affect how many "Chasers" the game may spawn in yours, as the total number of cops loaded by the game includes all non-roadblock cars of every active pursuit at once.

* You must install the [NFSMW LimitAdjuster mod](https://zolika1351.pages.dev/mods/nfsmwlimitadjuster) (LA) by Zolika1351 for game stability if you set any global cop-spawn limit above 8. Without LA, the game will start unloading models and assets because its default cop loader simply cannot handle managing more than 8 vehicles for very long. To fully unlock the global cop-spawn limit without taking spawning control away from Bartender, open LA's `NFSMWLimitAdjuster.ini` configuration file and disable *everything* in its `[Options]` parameter group. Even with these changes, LA itself might still crash sometimes.

* You must assign the `CAR` class to the `pvehicle` VltEd entries of all vehicles you provide in any of the spawn tables. You can do this directly or through parent entries.

* All `count` and `chance` values you provide in any of the spawn tables must each be > 0. Vehicles with `count` and / or `chance` values of 0 are invalid and count as omitted.

* The `chance` values are weights (like in VltEd), not percentages. The actual spawn chance of a vehicle is its `chance` value divided by the sum of the `chance` values of all vehicles from the same spawn table. Whenever a vehicle reaches its `count` value (i.e. spawn cap), Bartender treats its `chance` value as 0 until there is room for further spawns of that vehicle again.

* Bartender uses the free-roam "Chasers" spawn tables (which must contain at least one vehicle) in place of all free-roam "Roadblocks", "Events", and "Patrols" spawn tables you leave empty.

* Bartender uses the free-roam spawn tables in place of all race spawn tables you leave empty.

* Vehicles in "Roadblocks" spawn tables are not equally likely to spawn in every vehicle position of a given roadblock formation. This is because the game processes roadblock spawns in a fixed, formation-dependent order, making it (e.g.) more likely for vehicles with low `count` and high `chance` values to spawn in any position that happens to be processed first. This does not apply to vehicles with `count` values of at least 5, as no roadblock consists of more than 5 cars.

* Rarely, vehicles that are not in a "Roadblocks" spawn table will still show up in roadblocks. This is a vanilla bug: it usually happens when the game attempts to spawn a "Chaser" while it is processing a roadblock request, causing it to place the wrong car in the requested roadblock. Even more rarely than that, this bug can also happen with traffic cars or the helicopter.

* The "Events" spawn tables don't apply to the scripted patrols that spawn in any of the prologue D-Day events; those spawns are special and a real hassle to deal with, even among event spawns.

* The "Events" spawn tables don't apply to the very first scripted, pre-generated cop that spawns in a given event. Instead, this first cop is always of the type listed in the event's `CopSpawnType` VltEd parameter. This is because the game requests this vehicle before it loads any pursuit or Heat-level information, making it impossible for Bartender to know which spawn table to use for this single vehicle.

* Bartender temporarily ignores the `count` values in a "Roadblocks" / "Events" spawn table whenever a roadblock / event requests more vehicles in total than they would otherwise allow. You can avoid this by ensuring that the totals of all `count` values amount to at least 5 / 6 for each "Roadblocks" / "Events" spawn table, as no roadblock / event in the vanilla game consists of more than 5 / 6 vehicles in total.

* You should not use fast Heat transitions (`0x80deb840` VltEd parameter(s) set to < 5 seconds), else you might see a mix of cops from more than one "Events" spawn table appear in events with scripted, pre-generated cops. This happens because, depending on your loading times, the game might update the Heat level as it requests those spawns. You can avoid this issue by setting the event's `ForceHeatLevel` VltEd parameter to the target Heat level instead.

* Bartender uses different spawn tables for each of the two patrol-spawn types in the game: "Patrols" tables replace the free patrols that spawn when there is no active pursuit, and "Chasers" tables replace the searching patrols that spawn in pursuits when you are in "COOLDOWN" mode. You can control the number of patrol spawns through the `NumPatrolCars` VltEd parameter, but there are two important quirks: Free patrol spawns ignore the global cop-spawn limit, while searching patrol spawns ignore the remaining engagement count.

&nbsp;

&nbsp;

&nbsp;



# 4 - How do I install Bartender for my game?

**Before installing** Bartender:
1. make sure you have read and understood the ["What should I know...?" section](#3---what-should-i-know-before-i-use-bartender) above,
2. make sure your game's `speed.exe` is compatible (i.e. 5.75 MB / 6,029,312 bytes large), and
3. install an .asi loader or any mod with one (e.g. the [WideScreenFix mod](https://github.com/ThirteenAG/WidescreenFixesPack/releases/tag/nfsmw) by ThirteenAG).

&nbsp;

To **install** Bartender:
1. if it doesn't exist already, create a `scripts` folder in your game's installation folder;
2. copy the `BartenderSettings` folder and .asi file into the game's `scripts` folder; and
3. (optional) in User Mode of [Binary 2.8.3](https://github.com/SpeedReflect/Binary/releases/tag/v2.8.3) or newer, load and apply `FixMissingTextures.end`.

&nbsp;

**After installing** Bartender, you can customise its features through its configuration (.ini) files. You can find these configuration files in the game's new `scripts\BartenderSettings` folder.

To **uninstall** Bartender, remove its files from your game's `scripts` folder. There is no need to remove the optional missing textures, as the game will not use them without Bartender.

To **update** Bartender, uninstall it and repeat the installation process above.

> [!WARNING]
> Whenever you update, make sure to replace *all* old configuration files!

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
* **[ExOptsTeam](https://github.com/ExOptsTeam)**, for permitting me to use their Heat-level fixes;
* **trelbutate**, for his [NFSMW Cop Car Healthbars mod](https://github.com/trelbutate/MWHealthbars/) as a resource; and
* **Orsal**, **Aven**, **Astra King79**, and **MORELLO**, for testing and providing feedback.