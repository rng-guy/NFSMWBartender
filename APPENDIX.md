
![POV: You hit the RESET button by accident, and it did not fix your game's issues.](Thumbnail.jpg "I'm far too lazy to make another thumbnail for this.")

This document contains the full **technical details and limitations** of Bartender and its features, and it also mentions any incompatible features of other .asi mods wherever they are relevant. For a quick overview of what you may need to disable for Bartender to work, see the [README](README.md/#3---what-mods-are-incompatible-with-bartender).

&nbsp;

You really **only need to read this document** if
* you have persistent issues with your game after installing / configuring Bartender, or
* you are curious about the technicalities and limitations of Bartender or its features.

&nbsp;

First, if Bartender's .asi file gets **falsely flagged as a virus** by your antivirus software, you will need to whitelist the file. If the flagging happens as you attempt to launch your game with Bartender installed, the .asi loader will display a pop-up window with an error message ("Error code: 225") before your antivirus likely (re)moves the .asi file.

&nbsp;

Last, there are three **possible causes for in-game issues** you might encounter with Bartender:
* features of other .asi mods that make changes to the same parts of the game as Bartender,
* the quirks of how Bartender reads and processes parameters in its configuration files, or
* the actual parameter values themselves that you define in Bartender's configuration files.

&nbsp;

To help you **solve in-game issues** with Bartender, the sections below address these questions:
1. [What is there to know about Bartender's file parsing?](#1---what-is-there-to-know-about-bartenders-file-parsing)
2. [What is there to know about the "Basic" feature set?](#2---what-is-there-to-know-about-the-basic-feature-set)
3. [What is there to know about the "Advanced" feature set?](#3---what-is-there-to-know-about-the-advanced-feature-set)

&nbsp;

For a detailed **version history** of Bartender, see the plain-text version of this document (`APPENDIX.txt`).

&nbsp;

&nbsp;

&nbsp;



# 1 - What is there to know about Bartender's file parsing?

Bartender only recognises **Heat levels** from 1 to 10. If you want to use Bartender to customise Heat levels above 10, you need to recompile it with the appropriate `maxHeatLevel` value yourself.

&nbsp;

Bartender parses its configuration (.ini) files in **parameter groups**, indicated by `[GroupName]`. These groups each contain related parameters and give a logical structure in the configuration files. Each group allows you to define values, either in relation to Heat levels or vehicles.

&nbsp;

Bartender can handle any **invalid / missing parameter groups** in its configuration files:
* duplicate (e.g. another `[State:Busting]`) and unknown groups are ignored, and
* missing groups make Bartender count each of their would-be values as omitted.

&nbsp;

Bartender can handle any **invalid values** you might define in its parameter groups:
* duplicates (e.g. another `heat02` value) within parameter groups are ignored,
* values of incorrect type (e.g. a string instead of a decimal) count as omitted,
* negative values that should be positive are set to 0 instead of counting as omitted,
* mismatched interval values (i.e. where `max` < `min`) are each set to the lower value, and
* comma-separated value pairs / tuples with too many or too few (valid) values count as omitted.

&nbsp;

Some parameter groups allow you to define **default values**, indicated by `default` in place of a Heat level or vehicle. These default values then apply to all Heat levels or vehicles for which you don't define explicit values. Bartender parses such parameter groups in three steps:
1. If you omit it, the `default` value is set to the game's vanilla (i.e. unmodded) value.
2. All free-roam Heat levels (format: `heatXY`) you omit are set to the `default` value.
3. All race Heat levels (format: `raceXY`) you omit are set to their free-roam values.

&nbsp;

Bartender can handle any **invalid vehicles** you might define in its configuration files, both as values themselves and as something for which you define other values. The sections below mention how Bartender does this on a case-by-case basis, but a vehicle is invalid if
* it doesn't exist in the game's database (i.e. lacks a VltEd node under `pvehicle`), or
* it's of the wrong class (e.g. is a helicopter when Bartender expects a regular car).

&nbsp;

The **class of a vehicle** depends on the `CLASS` VltEd parameter in its `pvehicle` node. Bartender considers a `CLASS` value of `CHOPPER` to represent a helicopter, and every other value a car. Most vanilla vehicles lack an explicit `CLASS` parameter in their `pvehicle` nodes because they inherit one from parent nodes instead, but you can add one manually to overwrite it if you wish.

&nbsp;

&nbsp;

&nbsp;



# 2 - What is there to know about the "Basic" feature set?

Regarding the "Basic" feature set **as a whole**:

* The configuration (.ini) files for this feature set are located in `BartenderSettings\Basic`.

* The unedited configuration files for this feature set mostly match the game's vanilla values.

* You can disable this entire feature set by deleting all of its configuration files.

* You can disable any feature of this set by deleting the file containing its parameter group. Fixes, however, don't have parameters and are tied to specific files implicitly instead.

* To disable the shadow and Heat-level fixes, delete all configuration files of this feature set.

* The Heat-level reset fix is incompatible with the `HeatLevelOverride` feature of the [NFSMW ExtraOptions mod](https://github.com/ExOptsTeam/NFSMWExOpts/releases) by ExOptsTeam. To disable this ExtraOptions feature, edit its `NFSMWExtraOptionsSettings.ini` configuration file. If you do this, you can still change the maximum available Heat level with VltEd: The `0xe8c24416` parameter of a given `race_bin_XY` VltEd node determines the maximum Heat level (1-10) at Blacklist rival #XY.

* If you don't install the optional missing textures (`FixMissingTextures.end`), then the game won't display a number next to Heat gauges in menus for cars with Heat levels > 5. Whether you install these textures doesn't affect the Heat-level reset fix in any way.

&nbsp;

Regarding **cosmetic features** (`BartenderSettings\Basic\Cosmetic.ini`):

* The cop-destruction string feature is incompatible with the `EnableCopDestroyedStringHook` feature of the [NFSMW Unlimiter mod](https://github.com/nlgxzef/NFSMWUnlimiter/releases) by nlgxzef. To resolve this conflict, either delete Bartender's `[Vehicles:Strings]` parameter group or disable Unlimiter's version of the feature by editing its `NFSMWUnlimiterSettings.ini` configuration file.

* You can use the [Binary tool](https://github.com/SpeedReflect/Binary/releases/tag/v2.8.3) by MaxHwoy to edit the game's strings and add new ones.

* For destruction strings, Bartender ignores vehicles and strings that don't exist in the game.

* If you don't define a valid `default` string, string-less vehicles won't trigger notifications.

* If you define no vehicle strings and no `default`, Bartender disables its string feature.

* You might hear cops use callsigns you didn't assign to them. This is vanilla behaviour: The game maintains a pool of "actors" that it constantly shuffles between all active vehicles. This is also the reason why you may still hear Cross or helicopter lines even if they are gone, and why dispatch may sometimes refer to Cross by the callsign of another unrelated vehicle.

* The game automatically assigns the helicopter-exclusive callsign to all helicopters.

* For callsigns, Bartender ignores vehicles that are helicopters or don't exist in the game. Bartender also ignores all callsigns other than `patrol`, `elite`, `rhino`, and `cross`.

* If you don't define a valid `default` callsign, Bartender uses `patrol` instead.

* If you define no vehicle callsigns and no `default`, Bartender disables its callsign feature.

* You can define a playlist of up to 20 tracks using the game's four interactive pursuit themes. By default, Bartender loops through this custom playlist from top to bottom in each pursuit.

* For the pursuit-theme playlist, Bartender ignores themes that don't exist in the game.

* If you don't define any valid tracks, Bartender disables all its playlist features.

* Bartender can shuffle the first playlist track that plays in each pursuit, and / or shuffle the follow-up track(s) instead of playing them in order.

* If you don't define any shuffle settings, Bartender shuffles the first track in each pursuit.

* If you don't define a track length, the first track in each pursuit keeps playing forever.

* Track transitions may take a few minutes (at worst) due to quirks of the audio scheduler.

&nbsp;

Regarding **general features** (`BartenderSettings\Basic\General.ini`):

* Deleting this file disables the fix for getting busted while the green "EVADE" bar fills.

* The `0x1e2a1051` VltEd parameter defines how much passive bounty you gain after each interval.

* The `DestroyCopBonusTime` VltEd parameter defines the time window for combo-bounty streaks.

* Bartender sets all combo-bounty multiplier limits that are < 1 to 1 instead.

* The red "BUSTED" bar fills when you drive slowly enough and are near a cop who can see you. Once the bar is full, the cops apprehend you and end your pursuit in their favour.

* The cops' visual range limits the effective max. bust distance. The cops' visual range is defined by the `frontLOSdistance`, `rearLOSdistance`, and `heliLOSdistance` VltEd parameters.

* The `BustSpeed` VltEd parameter defines the speed threshold for busting.
   
* The green "EVADE" bar fills when you are not within line of sight of any cops. Once the bar is full, you enter "COOLDOWN" mode and need to stay hidden for a while to escape your pursuit.

* The `evadetimeout` VltEd parameter defines how long you need to stay hidden in "COOLDOWN" mode.

* The time you spend filling the green "EVADE" bar also counts towards how long you need to stay hidden in "COOLDOWN" mode. If the "EVADE" bar takes longer to fill, you escape instantly.

* The two checks for whether a flipped cop should be destroyed are mutually independent: One check is damage-based, while the other is time-based and disregards vehicle damage.

* The time-based flip check only happens at Heat levels for which you define a valid delay value.

* Resets of flipped racers only happen at Heat levels for which you define a valid delay value.

&nbsp;

Regarding **ground supports** (`BartenderSettings\Basic\Supports.ini`):

* Deleting this file disables the fix for the biases in the game's Strategy-selection process.

* When the game requests a non-Strategy roadblock, a random roadblock cooldown begins. While this cooldown is active, the game cannot make more non-Strategy roadblock requests.

* When the game requests a Strategy, a fixed-length Strategy cooldown begins. While this cooldown is active, the game cannot make more Strategy requests.

* When the game requests a HeavyStrategy, it (re)sets the roadblock cooldown to a fixed value.

* Not every request results in a successful spawn of whatever the game requested.

* Bartender can prevent HeavyStrategy 3 requests from resetting the roadblock cooldown. This avoids the potential issue of roadblocks becoming much rarer than expected.

* Strategy requests block each other: Whenever there is an active Strategy request, the game will not attempt to make more. You can change this with Bartender's "Advanced" feature set.

* In the vanilla game, active roadblocks of any kind also block new HeavyStrategy 3 requests. Bartender can prevent this blocking, making HeavyStrategy 3 requests much more frequent.

* Strategy requests end when their "Duration" VltEd parameters expire or their vehicles are gone.

* Bartender fixes the game's implicit biases in its Strategy-selection process by forcing it to check every available Strategy before making a new request. The vanilla game instead goes through them in the same order as they are defined in VltEd, making the game much more likely to request whatever Strategy happens to come first there.

* The `MinimumSupportDelay` VltEd parameter defines how much time needs to pass before the game can make non-Strategy roadblock and Strategy requests in a given pursuit.

* Vehicles joining pursuits from roadblocks after some time is vanilla behaviour, but the age threshold usually makes it a rare event because most roadblocks despawn too early.

* You shouldn't use low roadblock-age thresholds (< 20 seconds) for roadblock vehicles to join pursuits: Too many vehicles joining can cause game instability, as they ignore spawn limits.

* Age-based joining from roadblocks only happens at Heat levels for which you define valid age and distance values, and has no bearing on other means by which roadblock vehicles may join.

* Roadblock vehicles can react to racers entering "COOLDOWN" mode and / or spike-strip hits. For the former, one vehicle joins the pursuit; for the latter, all of them join the pursuit.

* LeaderStrategy 5 spawns Cross by himself, while LeaderStrategy 7 spawns him with two henchmen.

* You shouldn't use the replacement vehicles for Cross for any other cop in the game unless you also use Bartender's "Advanced" feature set. Otherwise, these vehicles interfere with LeaderStrategy spawns whenever they are present in a given pursuit.

* Bartender replaces vehicles that don't exist in the game with whatever the vanilla game uses.

* Bartender replaces vehicles that are helicopters with whatever the vanilla game uses.

&nbsp;

&nbsp;

&nbsp;



# 3 - What is there to know about the "Advanced" feature set?

Regarding the "Advanced" feature set **as a whole**:

* The configuration (.ini) files for this feature set are located in `BartenderSettings\Advanced`.

* The unedited configuration files for this feature set resemble the game's vanilla values.

* You can disable this entire feature set by deleting all of its configuration files.

* Bartender disables this feature set if you leave any free-roam "Chasers" spawn table empty.

* Rarely, the engagement count above the pursuit board may appear to be inaccurate compared to how many cops are actually around you at a given moment. That is because Bartender's fix makes the engagement count track "Chasers" only, while disregarding any vehicles that join your pursuit through Strategy spawns, from roadblocks, or as helicopters.

* For each Heat level, you should set a `FullEngagementCopCount` VltEd parameter > 0. Otherwise, Bartender may fail to update the displayed engagement count accurately.

* If enabled, this feature set overrides the following `pursuitlevels` VltEd parameters: the `cops` array, `HeliFuelTime`, `TimeBetweenHeliActive`, and `SearchModeHeliSpawnChance`.

&nbsp;

Regarding **cop (de / re)spawning** (`BartenderSettings\Advanced\Cars.ini`):

* A minimum engagement count > 0 allows that many "Chasers" to (re)spawn without backup. This minimum count doesn't spawn cops beyond their `count` values or the global cop-spawn limit.

* In "COOLDOWN" mode, the `NumPatrolCars` VltEd parameter overrides the min. engagement count.

* The global cop-spawn limit determines whether the game may spawn new "Chasers" at any point. The game can spawn additional "Chasers" as long as the total amount of non-roadblock and non-helicopter cops that currently exist across all pursuits is less than this global limit. This also means that any active Strategy spawns or NPC pursuits can affect how many more "Chasers" can still spawn in your pursuit (this is vanilla behaviour).

* The global cop-spawn limit takes precedence over all other spawning-related parameters, except for the `NumPatrolCars` VltEd parameter outside of active pursuits (this is vanilla behaviour).

* If you want to use global cop-spawn limits > 8 and / or make "Chasers" spawns independent of other vehicles, you must also install and configure the [NFSMW LimitAdjuster mod](https://zolika1351.pages.dev/mods/nfsmwlimitadjuster) (LA) by Zolika1351. This is necessary because the vanilla game becomes unstable if there are too many vehicles present. To configure LA to work with Bartender, open LA's `NFSMWLimitAdjuster.ini` configuration file; there, set `PursuitCops` to 255 and disable every single cop-related feature under `[Options]`. After configuring LA like this, use Bartender to set global cop-spawn limits.

* "Chasers" will only flee at Heat levels for which you define valid flee-delay values.

* Bartender uses the free-roam "Chasers" spawn tables (which must contain at least one vehicle) in place of all free-roam "Roadblocks", "Events", and "Patrols" spawn tables you leave empty.

* Bartender uses the free-roam spawn tables in place of all race spawn tables you leave empty.

* Bartender ignores vehicles that are helicopters or don't exist in the game.

* Bartender adds a `copmidsize` to each non-empty table that contains only ignored vehicles.

* The `chance` values are weights (like in VltEd), not percentages. The actual spawn chance of a vehicle is its `chance` value divided by the sum of the `chance` values of all vehicles from the same spawn table. Whenever a vehicle reaches its `count` value (i.e. spawn cap), Bartender treats its `chance` value as 0 until there is room for further spawns of that vehicle again.

* Bartender sets all `count` and `chance` values that are < 1 to 1 instead.

* Bartender enforces the `count` values for "Chasers" for each active pursuit separately. For "Roadblocks" / "Events", Bartender enforces `count` values for each roadblock / event. "Patrols" lack `count` values because they technically don't belong to anything trackable.

* Once they join a given pursuit, "Events" and "Patrols" spawns also count as "Chasers" as far as membership (i.e. fleeing decisions) and the `count` values of "Chasers" are concerned.

* The "Roadblocks" spawn tables don't apply to HeavyStrategy 4 roadblocks.

* Each roadblock / event in the game requests a hard-coded number of vehicles. No roadblock formation in the game requests more than 5 vehicles, and no scripted event more than 8.
 
* Bartender temporarily ignores the `count` values in a "Roadblocks" / "Events" spawn table whenever a roadblock / event requests more vehicles in total than they would otherwise allow. This ensures the game cannot get stuck trying to spawn a roadblock or start a scripted event.

* Vehicles in "Roadblocks" spawn tables are not equally likely to spawn in every vehicle position of a given roadblock formation. This is because the game processes roadblock spawns in a fixed, formation-dependent order, making it (e.g.) more likely for vehicles with low `count` and high `chance` values to spawn in any position that happens to be processed first. This doesn't apply to vehicles with `count` values of at least 5, as no roadblock consists of more than 5 cars.

* Rarely, vehicles that are not in a "Roadblocks" spawn table may still show up in roadblocks. This is a vanilla bug: it usually happens when the game attempts to spawn a vehicle while it's processing a roadblock request, causing it to place the wrong car in the requested roadblock. Even more rarely than that, this bug can also happen with traffic cars or the helicopter.

* The "Events" spawn tables also apply to the scripted patrols in prologue (DDay) race events.

* The "Events" spawn tables don't apply to the very first scripted, pre-generated cop that spawns in a given free-roam event (e.g. a Challenge Series pursuit). Instead, this first cop is always of the type defined by the event's `CopSpawnType` VltEd parameter. This is because the game requests this vehicle before it loads any pursuit or Heat-level information, making it impossible for Bartender to know which spawn table to use for just this single vehicle.

* You shouldn't use fast Heat transitions (`0x80deb840` VltEd parameter(s) set to < 5 seconds), else you might see a mix of cops from more than one "Events" spawn table appear in events with scripted, pre-generated cops. This happens because, depending on your loading times, the game might update the Heat level as it requests those spawns. You can also avoid this issue by setting the event's `ForceHeatLevel` VltEd parameter to the target Heat level instead.

* Bartender uses different spawn tables for each of the two patrol-spawn types in the game: "Patrols" tables replace the free patrols that spawn when there is no active pursuit, and "Chasers" tables replace the searching patrols that spawn in pursuits when racers are in "COOLDOWN" mode. You can control the number of patrol spawns through the `NumPatrolCars` VltEd parameter, but there are two important quirks: Free patrol spawns ignore the global cop-spawn limit, while searching patrol spawns ignore the remaining engagement count.

&nbsp;

Regarding **helicopter (de / re)spawning** (`BartenderSettings\Advanced\Helicopter.ini`):

* Bartender uses separate, random timers for (re)spawning the helicopter and setting its fuel. Each despawn context (e.g. the helicopter getting wrecked) has its own respawn-delay interval.

* The helicopter only (re)spawns at Heat levels for which you define valid (re)spawn-delay values.

* In a given pursuit, the helicopter must have a successful first spawn at some point before context-dependent respawns can happen. Such first spawns carry over across Heat transitions.
 
* If you don't define valid respawn-delay values for some despawn context (e.g. getting wrecked), then the helicopter won't respawn if that context happens. If a transition to another Heat level with valid respawn-delay values takes place, however, the helicopter may respawn again.

* The helicopter can only rejoin after losing you at Heat levels for which you define valid rejoin-delay and minimum-fuel values. The helicopter can only rejoin if it loses you.

* The helicopter rejoins with whatever amount of fuel it had left, minus the rejoin delay. If the helicopter were to rejoin with less fuel than the required minimum, it counts as having run out of fuel and triggers the appropriate respawn delay instead.

* Whether an active helicopter may rejoin a given pursuit is unaffected by Heat transitions, as this is locked in as soon as it (re)spawns. This means a rejoining helicopter can keep rejoining your pursuit until it either gets wrecked or runs out of fuel. Its vehicle is also locked in to ensure it rejoins with the same model and overall properties.

* Rejoining helicopters don't count towards the total number of helicopters deployed.

* The helicopter only spawns with limited fuel at Heat levels for which you define valid fuel-time values. Unlimited fuel means the helicopter must either lose you or get wrecked.

* The helicopter also (re)spawns in "COOLDOWN" mode according to its (re)spawn delays.

* The helicopter only ever (re)spawns and rejoins in player pursuits.

* Only one helicopter can ever be active at any given time. This is a game limitation; we could technically spawn more, but they would count as cars and behave very oddly.

* The helicopter uses whatever HeliStrategy you set in VltEd.

* Bartender replaces vehicles that don't exist in the game with `copheli`.

* Bartender replaces vehicles that aren't helicopters with `copheli`.

&nbsp;

Regarding **strategy requests** (`BartenderSettings\Advanced\Strategies.ini`):

* Defining low racer-speed thresholds for HeavyStrategy 3 spawns fixes the vanilla issue of them attempting to flee a given pursuit instantly without trying to ram anything. This is because the vanilla game forces HeavyStrategy 3 spawns to flee if the racer's speed drops below the `CollapseSpeed` VltEd parameter at any point. At higher Heat levels, this can lead to many passive spawns because of higher `CollapseSpeed` values and far more aggressive cops.

* LeaderStrategy Cross and / or his henchmen only become aggressive at Heat levels for which you define valid aggro-delay values. Henchmen, however, always become aggressive when Cross leaves.

* Once aggressive, Cross and / or his henchmen act like regular cops and can join formations. Also, neither Cross nor his henchmen can return to being passive again until they despawn.

* Aggro delays longer than a given LeaderStrategy's `Duration` VltEd parameter have no effect.

* The aggro delays of any active LeaderStrategy are unaffected by Heat transitions, as their values are locked in as soon as the game requests said LeaderStrategy.

* Resets of Cross' spawn flag only happen at Heat levels for which you define valid reset-delay values. The vanilla game never resets this flag if Cross gets wrecked.

* Once Cross' spawn flag is reset, the game can attempt LeaderStrategy requests again.

* For LeaderStrategy 7, the henchmen must also despawn first before the game can request another Strategy. You can circumvent this with Bartender's unblocking feature (see below).

* Bartender can unblock the Strategy-request queue while there is still an active Strategy. Without unblocking, an active Strategy request prevents the game from making a new request. Unblocking allows multiple Strategy requests to spawn at the same time, and they each continue until either their `Duration` VltEd parameters expire or all their vehicles have despawned.

* Unblocking happens whenever a Strategy request leads to a successful spawn.

* Bartender only unblocks requests at Heat levels for which you define valid unblock-delay values.

* Unblock delays longer than a given Strategy's `Duration` VltEd parameter have no effect.

* The unblock delays of any active Strategy are unaffected by Heat transitions, as their values are locked in as soon as the game spawns said Strategy.

* Even with unblocking, no new LeaderStrategy can spawn while a LeaderStrategy Cross is present.

* It is generally safe to use unblock delays of 0 for HeavyStrategy 4 and LeaderStrategy 5 / 7.

* You shouldn't use short unblock delays for HeavyStrategy 3. Having too many overlapping HeavyStrategy 3 spawns can lead to game instability since they ignore all spawn limits. It's often better to just use a lower (~20 seconds) `Duration` VltEd parameter instead.