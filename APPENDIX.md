
![POV: You hit the RESET button by accident, and it did not fix your game's issues.](Thumbnail.jpg "I'm far too lazy to make another thumbnail for this.")

This document contains the full **technical details and limitations** of Bartender and its features, and it also mentions any incompatible features of other .asi mods wherever they are relevant. For a quick overview of what you may need to disable for Bartender to work, see the [README](README.md/#3---what-mods-are-incompatible-with-bartender).

&nbsp;

You really **only need to read this document** if
* you have persistent issues with your game after installing / configuring Bartender, or
* you are curious about the technicalities and limitations of Bartender or its features.

&nbsp;

There are mostly **three potential causes** for any issues you might encounter with Bartender:
* features of other .asi mods that make changes to the same parts of the game as Bartender,
* quirks in how Bartender reads and processes parameter values in its configuration files, and
* the actual parameter values themselves that you provide in Bartender's configuration files.

&nbsp;

To **help you troubleshoot issues** with Bartender, the sections below address these questions:
1. [What is there to know about Bartender's file parsing?](#2---what-is-there-to-know-about-bartenders-file-parsing)
2. [What is there to know about the "Basic" feature set?](#3---what-is-there-to-know-about-the-basic-feature-set)
3. [What is there to know about the "Advanced" feature set?](#4---what-is-there-to-know-about-the-advanced-feature-set)

&nbsp;

For a detailed **version history** of Bartender, see the plain-text version of this document (`APPENDIX.txt`).

&nbsp;

&nbsp;

&nbsp;



# 1 - What is there to know about Bartender's file parsing?

Bartender parses its configuration (.ini) files in **parameter groups**, indicated by `[GroupName]`. These groups each contain related parameters and give a logical structure in the configuration files. Each group allows you to provide values, either in relation to Heat levels or vehicles.

&nbsp;

Bartender can handle any **invalid / missing parameter groups** in its configuration files:
* duplicate (e.g. another `[Busting:General]`) and unknown groups are ignored, and
* missing groups make Bartender count each of their would-be values as omitted.

&nbsp;

Bartender can handle any **invalid values** you might provide in its parameter groups:
* duplicates (e.g. another `heat02` value) within parameter groups are ignored,
* values of incorrect type (e.g. a string instead of a decimal) count as omitted,
* negative values that should be positive are set to 0 instead of counting as omitted,
* mismatched interval values (i.e. where `max` < `min`) are each set to the lower value, and
* comma-separated value pairs / tuples with too many or too few (valid) values count as omitted.

&nbsp;

Some parameter groups allow you to define **default values**, indicated by `default` in place of a Heat level or vehicle. These default values then apply to all Heat levels or vehicles for which you do not provide explicit values. Bartender parses such parameter groups in three steps:
1. If you omit it, the `default` value is set to the game's vanilla (i.e. unmodded) value.
2. All free-roam Heat levels (format: `heatXY`) you omit are set to the `default` value.
3. All race Heat levels (format: `raceXY`) you omit are set to their free-roam values.

&nbsp;

Bartender can handle any **invalid vehicles** you might provide in its configuration files, both as values themselves and as something for which you provide other values. The sections below mention how Bartender does this on a case-by-case basis, but a vehicle is invalid if
* it doesn't exist in the game's database (i.e. lacks a VltEd node under `pvehicle`), or
* it is of the wrong class (e.g. is a helicopter when Bartender expects a regular car).

&nbsp;

The **class of a vehicle** depends on the `CLASS` VltEd parameter in its `pvehicle` node. Bartender considers a `CLASS` value of `CHOPPER` to represent a helicopter, and every other value a car. Most vanilla vehicles lack an explicit `CLASS` parameter in their `pvehicle` nodes because they inherit one from parent nodes instead, but you can add one manually to overwrite it if you wish.

&nbsp;

&nbsp;

&nbsp;



# 2 - What is there to know about the "Basic" feature set?

Regarding the "Basic" feature set **in general**:

* The configuration (.ini) files for this feature set are located in `BartenderSettings\Basic`.

* The unedited configuration files for this feature set use the game's vanilla values.

* You can disable this entire feature set by deleting all of its configuration files.

* You can disable any feature of this set by deleting the file containing its parameters. Bug fixes, however, don't have parameters and are tied to specific files implicitly instead.

* To disable the shadow and Heat-level fixes, delete all configuration files of this feature set.

* The Heat-level reset fix is incompatible with the `HeatLevelOverride` feature of the [NFSMW ExtraOptions mod](https://github.com/ExOptsTeam/NFSMWExOpts/releases) by ExOptsTeam. To disable this ExtraOptions feature, edit its `NFSMWExtraOptionsSettings.ini` configuration file. If you do this, you can still change the maximum available Heat level with VltEd: The `0xe8c24416` parameter of a given `race_bin_XY` VltEd node determines the maximum Heat level (1-10) at Blacklist rival #XY.

* If you don't install the optional missing textures (`FixMissingTextures.end`), then the game won't display a number next to Heat gauges in menus for cars with Heat levels > 5. Whether you install these textures does not affect the Heat-level reset fix in any way.

&nbsp;

Regarding **cop (Binary) strings** (`BartenderSettings\Basic\Labels.ini`):

* This feature is incompatible with the `EnableCopDestroyedStringHook` feature of the [NFSMW Unlimiter mod](https://github.com/nlgxzef/NFSMWUnlimiter/releases) by nlgxzef. Either delete Bartender's `Labels.ini` configuration file or disable Unlimiter's version of the feature by editing its `NFSMWUnlimiterSettings.ini` file.

* Bartender ignores vehicles and (Binary) string labels that don't exist in the game.

* If you don't define a valid `default` label, vehicles without labels won't cause notifications.

&nbsp;

Regarding **ground supports** (`BartenderSettings\Basic\Supports.ini`):

* Deleting this file also disables the fix for slower roadblock and Heavy / LeaderStrategy spawns.

* When the game requests a roadblock, a random roadblock cooldown between two values begins. While this cooldown is active, the game cannot make more requests for regular roadblocks.

* When the game requests a Heavy / LeaderStrategy, a fixed-length strategy cooldown begins. While this cooldown is active, the game cannot make more Heavy / LeaderStrategy requests.

* When the game requests a HeavyStrategy, it (re)sets the roadblock cooldown to a fixed value.

* Bartender fixes the slowdown of regular roadblock and Heavy / LeaderStrategy spawns by clearing the request queue from time to time. Bartender does this whenever a certain amount of time has passed without any new roadblock or strategy requests, despite them being off cooldown. This maximum-overdue delay guarantees that requests cannot block each other for too long. The most common cause of such blockages are HeavyStrategy requests, especially for HeavyStrategy 3.

* Sometimes, regular roadblocks and Heavy / LeaderStrategy spawns may appear more frequently than their cooldowns would suggest. This happens because their cooldowns only stop the game from requesting more of them, which is unrelated to actually spawning any pending requests.

* Very short cooldowns for regular roadblock or Heavy / LeaderStrategy requests may cause spam. Excessive support spam can lead to game instability, as these spawns ignore most spawn limits. You can also reduce this risk by setting their `Duration` VltEd parameters to low values.

* The `MinimumSupportDelay` VltEd parameter defines how much time needs to pass before the game can start requesting regular roadblocks and Heavy / LeaderStrategy spawns in a given pursuit.

* LeaderStrategy 5 spawns Cross by himself, while LeaderStrategy 7 spawns him with two henchmen.

* You should not use the replacement vehicles for Cross elsewhere in the game. Otherwise, these vehicles will block all LeaderStrategy spawns whenever they are present in a given pursuit.

* Only the original `copcross` vehicle triggers Cross' unique radio chatter and callouts.

* Bartender replaces vehicles that don't exist in the game with whatever the vanilla game uses.

* Bartender replaces vehicles that are helicopters with with whatever the vanilla game uses.

&nbsp;

Regarding **uncategorised features** (`BartenderSettings\Basic\Others.ini`):

* Deleting this file also disables the fix for getting busted while the green "EVADE" bar fills.

* The red "BUSTED" bar fills when you drive slowly enough and are near a cop who can see you. Once the bar is full, the cops apprehend you and end the pursuit in their favour.
 
* The cops' visual range limits the effective max. bust distance. The cops' visual range is defined by the `frontLOSdistance`, `rearLOSdistance`, and `heliLOSdistance` VltEd parameters.

* The `BustSpeed` VltEd parameter defines the speed threshold for busting.

* The green "EVADE" bar fills when you are not within line of sight of any cops. Once the bar is full, you enter "COOLDOWN" mode and need to stay hidden for a while to escape the pursuit.

* The `evadetimeout` VltEd parameter defines how long you need to stay hidden in "COOLDOWN" mode.

* The time you spend filling the green "EVADE" bar also counts towards how long you need to stay hidden in "COOLDOWN" mode. If the "EVADE" bar takes longer to fill, you will escape instantly.

* The `0x1e2a1051` VltEd parameter defines how much passive bounty you gain at each interval.

* The `DestroyCopBonusTime` VltEd parameter defines the time window for cop combos.

* Bartender sets all cop-combo limits that are < 1 to 1 instead.

&nbsp;

&nbsp;

&nbsp;



# 3 - What is there to know about the "Advanced" feature set?

Regarding the "Advanced" feature set **in general**:

* The configuration (.ini) files for this feature set are located in `BartenderSettings\Advanced`.

* The unedited configuration files for this feature set use the game's vanilla values.

* You can disable this entire feature set by deleting all of its configuration files.

* You can disable any feature of this set by deleting the file containing its parameters. This does not apply to the engagement-count fix, which is tied to this entire feature set instead.

* Bartender disables this feature set if you leave any free-roam "Chasers" spawn table empty.

* You should ensure that every HeavyStrategy you enable in a given Heat level's `pursuitsupport` VltEd node is only listed there once (e.g. there isn't a second HeavyStrategy 3), and that there is also no more than one LeaderStrategy listed. Otherwise, such duplicates can cause the game (and Bartender) to misread their `Duration` VltEd parameters.

* If this feature set is enabled, the following `pursuitlevels` VltEd parameters are ignored because this feature set fulfils their intended purposes with extended customisation: `cops`, `HeliFuelTime`, `TimeBetweenHeliActive`, and `SearchModeHeliSpawnChance`.

&nbsp;

Regarding **helicopter (de / re)spawning** (`BartenderSettings\Advanced\Helicopter.ini`):

* The helicopter uses random timers for spawning, despawning, and respawning. Each of these actions has a separate interval of possible timer values. Here, "spawning" refers to the very first appearance of a helicopter in a given pursuit, while "respawning" refers to any further appearances after the first. Heat transitions don't reset these timers.

* The helicopter only spawns at Heat levels for which you provide valid timer-interval values.

* The helicopter also (de / re)spawns in "COOLDOWN" mode according to its timers.

* The helicopter uses whatever HeliStrategy you set in VltEd.

* Only one helicopter will ever be active at any given time. This is a game limitation; you could technically spawn more, but they would count as cars and behave very oddly.

* Bartender replaces vehicles that don't exist in the game with `copheli`.

* Bartender replaces vehicles that aren't helicopters with `copheli`.

&nbsp;

Regarding **cop (de / re)spawning** (`BartenderSettings\Advanced\Cars.ini`):

* A minimum engagement count > 0 allows that many "Chasers" to (re)spawn without backup. This minimum count does not spawn cops beyond their `count` values or the global cop-spawn limit.

* In "COOLDOWN" mode, the `NumPatrolCars` VltEd parameter overwrites the min. engagement count.

* The global cop-spawn limit determines whether the game may spawn new "Chasers" at any point. The game can spawn additional "Chasers" as long as the total amount of non-roadblock and non-helicopter cops that currently exist across all pursuits is less than this global limit. This also means that any active Heavy / LeaderStrategy spawns or NPC pursuits can affect how many more "Chasers" can still spawn in your pursuit (this is vanilla behaviour).

* The global cop-spawn limit takes precedence over all other spawning-related parameters, except for the `NumPatrolCars` VltEd parameter outside of active pursuits (this is vanilla behaviour).

* If you want to use global cop spawn limits > 8, you must also install and configure the [NFSMW LimitAdjuster mod](https://zolika1351.pages.dev/mods/nfsmwlimitadjuster) (LA) by Zolika1351. This is necessary because the game cannot handle managing > 8 "Chasers" for very long. To configure LA to work with Bartender, open LA's `NFSMWLimitAdjuster.ini` configuration file and disable everything in its `[Options]` parameter group. Even with this, LA itself might still crash sometimes.

* "Chasers" will only flee at Heat levels for which you provide valid delay values.

* Bartender uses the free-roam "Chasers" spawn tables (which must contain at least one vehicle) in place of all free-roam "Roadblocks", "Events", and "Patrols" spawn tables you leave empty.

* Bartender uses the free-roam spawn tables in place of all race spawn tables you leave empty.

* Bartender ignores vehicles that are helicopters or don't exist in the game.

* Bartender adds a `copmidsize` to each non-empty table that contains only ignored vehicles.

* The `chance` values are weights (like in VltEd), not percentages. The actual spawn chance of a vehicle is its `chance` value divided by the sum of the `chance` values of all vehicles from the same spawn table. Whenever a vehicle reaches its `count` value (i.e. spawn cap), Bartender treats its `chance` value as 0 until there is room for further spawns of that vehicle again.

* Bartender sets all `count` and `chance` values that are < 1 to 1 instead.

* Bartender enforces the `count` values for "Chasers" for each active pursuit separately. For "Roadblocks" / "Events", Bartender enforces `count` values for each roadblock / event.

* Once they join a pursuit, "Events" and "Patrols" spawns also count as "Chasers" as far as membership (i.e. fleeing decisions) and the `count` values of "Chasers" are concerned.

* Each roadblock / event in the game requests a hard-coded number of vehicles. No roadblock formation in the game requests more than 5 vehicles, and no scripted event more than 8.
 
* Bartender temporarily ignores the `count` values in a "Roadblocks" / "Events" spawn table whenever a roadblock / event requests more vehicles in total than they would otherwise allow. This ensures the game cannot get stuck trying to spawn a roadblock or start a scripted event.

* Vehicles in "Roadblocks" spawn tables are not equally likely to spawn in every vehicle position of a given roadblock formation. This is because the game processes roadblock spawns in a fixed, formation-dependent order, making it (e.g.) more likely for vehicles with low `count` and high `chance` values to spawn in any position that happens to be processed first. This does not apply to vehicles with `count` values of at least 5, as no roadblock consists of more than 5 cars.

* Rarely, vehicles that are not in a "Roadblocks" spawn table will still show up in roadblocks. This is a vanilla bug: it usually happens when the game attempts to spawn a vehicle while it is processing a roadblock request, causing it to place the wrong car in the requested roadblock. Even more rarely than that, this bug can also happen with traffic cars or the helicopter.

* The "Events" spawn tables also apply to the scripted patrols in prologue (DDay) race events.

* The "Events" spawn tables don't apply to the very first scripted, pre-generated cop that spawns in a given free-roam event (e.g. a Challenge Series pursuit). Instead, this first cop is always of the type defined by the event's `CopSpawnType` VltEd parameter. This is because the game requests this vehicle before it loads any pursuit or Heat-level information, making it impossible for Bartender to know which spawn table to use for just this single vehicle.

* You should not use fast Heat transitions (`0x80deb840` VltEd parameter(s) set to < 5 seconds), else you might see a mix of cops from more than one "Events" spawn table appear in events with scripted, pre-generated cops. This happens because, depending on your loading times, the game might update the Heat level as it requests those spawns. You can avoid this issue by setting the event's `ForceHeatLevel` VltEd parameter to the target Heat level instead.

* Bartender uses different spawn tables for each of the two patrol-spawn types in the game: "Patrols" tables replace the free patrols that spawn when there is no active pursuit, and "Chasers" tables replace the searching patrols that spawn in pursuits when you are in "COOLDOWN" mode. You can control the number of patrol spawns through the `NumPatrolCars` VltEd parameter, but there are two important quirks: Free patrol spawns ignore the global cop-spawn limit, while searching patrol spawns ignore the remaining engagement count.