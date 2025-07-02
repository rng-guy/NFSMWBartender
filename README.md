
![POV: Cross busts your sorry ass at Rockport's hottest bar.](Thumbnail.jpg "Graphic design is my passion.")

This .asi mod adds new customisation options to pursuits in *Need for Speed: Most Wanted* (2005). These new options come in **two feature sets**:
* the "**Basic**" set lets you change many otherwise hard-coded values of the game, and
* the "**Advanced**" set lets you [change cop-spawning behaviour and spawn tables without limits](https://youtu.be/XwFSpc97hF4).

&nbsp;

For **details** on which features and configuration files each set includes, see the sections below. There are also separate sections for [installation](#installation) instructions and the [limitations](#limitations) of this mod.

&nbsp;

&nbsp;

&nbsp;



# "Basic" feature set

This feature set **lets you change** (per Heat level)
* at which distance and how quickly you can get busted,
* how long it takes to lose the cops and enter Cooldown mode,
* at which time interval you gain passive bounty,
* the maximum combo-bounty multiplier for destroying cops quickly,
* the internal cooldown for regular roadblock spawns,
* the internal cooldown for Heavy and LeaderStrategy spawns,
* which vehicles spawn through HeavyStrategy 3 (the ramming SUVs),
* which vehicles spawn through HeavyStrategy 4 (the roadblock SUVs), and
* which vehicles spawn through LeaderStrategy 5 / 7 (Cross and his henchmen).

&nbsp;

This feature set **fixes two bugs**:
* you can no longer get busted due to line-of-sight issues while the "EVADE" bar fills, and
* regular roadblock and Heavy / LeaderStrategy spawns no longer slow down in longer pursuits.

&nbsp;

You can also **assign new (Binary) strings** for the game to display when cop vehicles are destroyed, similar to the [NFSMW Unlimiter](https://github.com/nlgxzef/NFSMWUnlimiter/releases) mod by nlgxzef. Compared to Unlimiter, this mod's version of this feature is easier to configure, leaner, and even checks strings for correctness on game launch, ignoring any specified strings that do not actually exist in the game's (modified) binary files.

&nbsp;

The **configuration (.ini) files** for this set are located in `scripts/BartenderSettings/Basic`.

To **disable** a given feature of this set, delete its .ini section or the entire file.

> [!IMPORTANT]
> Before you use this feature set, see the [limitations](#Limitations) section further below.

&nbsp;

&nbsp;

&nbsp;



# "Advanced" feature set

This feature set **lets you change** (per Heat level)
* how many cops can (re)spawn without backup once a wave is exhausted,
* the global cop-spawn limit for how many cops in total may chase you at any given time,
* how quickly cops flee the pursuit if they don't belong (if at all),
* which vehicles may spawn to chase and search for you (any amount; with counts and chances),
* which vehicles may spawn in regular roadblocks (same liberties as above),
* which vehicles may spawn as pre-generated cops in scripted events (ditto),
* which vehicles may spawn as free patrols when there is no active pursuit (chances only),
* which vehicle spawns in place of the regular helicopter, and
* when exactly the helicopter can (de / re)spawn (if at all).

&nbsp;

This feature set **also fixes** the displayed engagement count in the centre of the pursuit bar: its value is now perfectly accurate and reflects how many chasing cops remain in the current wave. The count ignores vehicles spawned through any Heavy or LeaderStrategy, the helicopter, and any vehicles that join the pursuit by detaching themselves from roadblocks.

&nbsp;

The **configuration (.ini) files** for this set are located in `scripts/BartenderSettings/Advanced`.

To **disable** this feature set, delete the entire `scripts/BartenderSettings/Advanced` folder.

> [!IMPORTANT]
> Before you use this feature set, see the [limitations](#Limitations) section further below.

&nbsp;

&nbsp;

&nbsp;



# Installation

**Before installing** this mod:
1. make sure you have read and understood the [limitations](#Limitations) section further below,
2. make sure your game's `Speed.exe` is compatible (i.e. 5.75 MB / 6,029,312 bytes large), and
3. install an .asi loader or any mod with one (e.g. the [WideScreenFix](https://github.com/ThirteenAG/WidescreenFixesPack/releases/tag/nfsmw) mod by ThirteenAG).

&nbsp;

To **install** this mod, copy its `BartenderSettings` folder and compiled .asi file into the `scripts` folder located in your game's installation folder. If your game does not have a `scripts` folder, create one.

**After installing** this mod, you can customise its features through its configuration (.ini) files. You can find these configuration files in the `scripts\BartenderSettings` folder.

To **uninstall** this mod, remove its files from your game's `scripts` folder.

&nbsp;

To **update** this mod, uninstall it and repeat the installation process above.

> [!WARNING]
> Whenever you update this mod, make sure to replace *all* old configuration files properly!

&nbsp;

&nbsp;

&nbsp;



# Limitations

The two feature sets of this mod each come with caveats and limitations. To avoid nasty surprises and game instability, make sure you understand them all before you use this mod in any capacity.

Both feature sets of this mod should be **compatible** with all VltEd, Binary, and most .asi mods. All known and notable exceptions to this are explicitly mentioned in this section.

&nbsp;

&nbsp;

## "Basic" feature set

**Cop (Binary) strings** (`BartenderSettings\Basic\Labels.ini`):

* This feature is completely incompatible with the `EnableCopDestroyedStringHook` feature of the [NFSMW Unlimiter](https://github.com/nlgxzef/NFSMWUnlimiter/releases) mod by nlgxzef. Either delete this mod's `Labels.ini` configuration file or disable Unlimiter's version of the feature by editing its `NFSMWUnlimiterSettings.ini` file.

&nbsp;

**Ground supports** (`BartenderSettings\Basic\Supports.ini`):

* All vehicles you specify to replace the HeavyStrategy 3 spawns (the ramming SUVs) should each have a low `MAXIMUM_AI_SPEED` value (the vanilla SUVs use 50) assigned to them in their `aivehicle` VltEd entry; otherwise, they might cause stability issues by joining the pursuit long-term after their ramming attempt(s), effectively circumventing the global cop-spawn limit.

* All vehicles you specify to replace Cross in LeaderStrategy 5 / 7 should each not be used by any other cop elsewhere. If another cop uses the same vehicle as Cross, no LeaderStrategy will be able to spawn as long as that cop is present in the pursuit.

&nbsp;

&nbsp;

## "Advanced" feature set

**General**:

* The entire feature set disables itself if *any* Heat level lacks a valid "Chasers" spawn table (`BartenderSettings\Advanced\Cars.ini`); you must specify at least one vehicle for each level.

* If the feature set is enabled, the following `pursuitlevels` VltEd parameters are ignored because the feature set fulfils their intended purposes with much greater customisation: `cops`, `HeliFuelTime`, `TimeBetweenHeliActive`, and `SearchModeHeliSpawnChance`.

&nbsp;

**Helicopter (de / re)spawning** (`BartenderSettings\Advanced\Helicopter.ini`):

* All vehicles you specify to replace the helicopter must each have the ``CHOPPER`` class assigned to them in their `pvehicle` VltEd entry, either directly or through a parent entry.

&nbsp;

**Cop (de / re)spawning** (`BartenderSettings\Advanced\Cars.ini`):

* Until HeavyStrategy 3 and LeaderStrategy spawns have left the pursuit, they can block new "Chasers" from spawning (but not the other way around). This is vanilla behaviour, as these spawns count toward the total number of cops loaded that the global cop-spawn limit (which only affects "Chasers") is compared against. This total is calculated across all active pursuits, meaning cops spawned in NPC pursuits can also affect how many "Chasers" may spawn in yours.
  
* Pushing any global cop-spawn limit beyond 8 requires the [NFSMW LimitAdjuster](https://zolika1351.pages.dev/mods/nfsmwlimitadjuster) (LA) mod by Zolika1351 to work properly. Without it, the game will start unloading models and assets because its default car loader cannot handle the workload of managing (potentially) dozens of vehicles. To make LA compatible with this mod, open its `NFSMWLimitAdjuster.ini` configuration file and disable *all* features in its `[Options]` section; this will fully unlock the spawn limit without forcing an infinite amount of cops to spawn. Note that LA is not perfectly stable either: It is prone to crashing in the first 30 seconds of the first pursuit in a play session, but will generally stay stable if it does not crash there.

* All vehicles you specify in any of the spawn tables must each have the `CAR` class assigned to them in their `pvehicle` VltEd entry, either directly or through a parent entry.

* Vehicles in "Roadblocks" spawn tables are not equally likely to spawn in every vehicle position of a given roadblock formation. This is because the game processes roadblock vehicles in a fixed, formation-dependent order, making it (e.g.) more likely for vehicles with low `count` and high `chance` values to spawn in any position that happens to be processed first. This does not apply to vehicles with `count` values of at least 5, as no roadblock contains more than 5 cars.

* Rarely, cops that are not in "Roadblocks" spawn tables might still show up in roadblocks. This is a vanilla bug; it usually happens when the game attempts to spawn a "Chaser" while it is processing a roadblock request, causing it to place the wrong car in the requested roadblock. This bug is not restricted to cop spawns: if the stars align, it can also happen with traffic.

* The "Events" spawn tables do *not* apply to the scripted patrols that spawn in any of the prologue D-Day events; those spawns are special and a real hassle to deal with, even among event spawns.

* The "Events" spawn tables do *not* apply to the very first scripted, pre-generated cop that spawns in a given event; instead, this first cop is always of the type specified in the event's `CopSpawnType` VltEd parameter. This is because the game requests this vehicle before it has loaded any pursuit or Heat level information, making it impossible for the mod to know which spawn table to use to replace it. This vehicle, however, is still properly accounted for in `count` calculations for any following vehicle spawns.

* `count` values in "Roadblocks" and "Events" spawn tables are ignored whenever the game requests more vehicles in total than these values would allow: When all their `count` values have been exhausted for a given roadblock / event, every vehicle in the relevant table may spawn without restriction until the next roadblock / event begins.

* Making Heat transitions very fast (`0x80deb840` VltEd parameter(s) set to < 5 seconds) can cause a mix of cops from more than one "Events" spawn table to appear in events that feature scripted, pre-generated cops. This happens because, depending on your loading times, the game might update the Heat level as it requests those spawns. If you want to keep fast transitions, you can avoid this issue by setting the event's `ForceHeatLevel` VltEd parameter to the target Heat level.

* There are two types of patrol spawns: free patrols that spawn when there is no active pursuit, and searching patrols that spawn in pursuits when you are in Cooldown mode. The free patrols are overwritten by the "Patrols" spawn table, and the searching patrols are taken from the "Chasers" table. For both patrol types, the `NumPatrolCars` VltEd parameter controls how many cars may spawn at any given time; free patrol spawns ignore the global cop-spawn limit, while searching patrol spawns ignore the remaining engagement count (but not the global limit). All of this is vanilla behaviour, which is why you should not set high `NumPatrolCars` values.

&nbsp;

&nbsp;

&nbsp;



# Acknowledgement(s)

You are free to bundle this mod and any of its .ini files with your own pursuit mod(s), **no credit required**. If you include the .asi file, however, I ask that you do your users a favour and provide a link to this mod's GitHub repository in your mod's README file.

&nbsp;

This mod would not have seen the light of day without
* **[rx](https://github.com/rxyyy)**, for encouraging me to try .asi modding;
* **[nlgxzef](https://github.com/nlgxzef)**, for the Most Wanted debug symbols;
* **DarkByte**, for [Cheat Engine](https://www.cheatengine.org/);
* **GuidedHacking**, for their [Cheat Engine tutorials](https://www.youtube.com/playlist?list=PLt9cUwGw6CYFSoQHsf9b12kHWLdgYRhmQ);
* **trelbutate**, for his [NFSMW Cop Car Healthbars](https://github.com/trelbutate/MWHealthbars/) mod as a resource; and
* **Orsal**, **Aven**, **Astra King79**, and **MORELLO**, for testing and providing feedback.