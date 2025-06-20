
![POV: Cross busts your sorry ass at Rockport's hottest bar.](Thumbnail.jpg "Graphic design is my passion.")

This .asi mod adds new customisation options to pursuits in *Need for Speed: Most Wanted* (2005). These new options come in **two feature sets**:
* the "**Basic**" set lets you change many otherwise hard-coded values of the game, and
* the "**Advanced**" set lets you change cop-spawning behaviour and spawn tables without limits.

&nbsp;

For more details on which features (and .ini files) each set includes, see the following sections. There are also separate sections for **installation** instructions and the **limitations** of this mod.

&nbsp;

&nbsp;

&nbsp;



# "Basic" feature set

The **.ini files** for this feature set are located in `scripts/BartenderSettings/Basic`.

To **disable** a feature of this set, delete its .ini section (or the respective .ini file).

See the "**Limitations**" section further below before using this feature set.

&nbsp;

This feature set **lets you change** (per Heat level)
* at which distance and how quickly you can get busted,
* how long it takes to lose the cops and enter Cooldown mode,
* the maximum bounty multiplier for destroying cops quickly,
* the internal cooldown for regular roadblock spawns,
* the internal cooldown for Heavy and LeaderStrategy spawns,
* which vehicles spawn through HeavyStrategy 3 (ramming SUVs),
* which vehicles spawn through HeavyStrategy 4 (roadblock SUVs), and
* which vehicles spawn through LeaderStrategy 5 / 7 (Cross and his henchmen).

&nbsp;

This feature set **fixes two bugs**:
* you can no longer get BUSTED due to line-of-sight issues while the EVADE bar fills, and
* regular roadblock and Heavy / LeaderStrategy spawns no longer slow down in longer pursuits.

&nbsp;

You can also **assign new (Binary) strings** for the game to display when cop vehicles are destroyed, similar to the [NFSMW Unlimiter](https://github.com/nlgxzef/NFSMWUnlimiter/releases) mod by nlgxzef. Compared to Unlimiter, this mod's version of this feature is easier to configure, leaner, and even checks strings for correctness on game launch, ignoring any specified strings that do not actually exist in the game's (modified) binary files.

&nbsp;

&nbsp;

&nbsp;



# "Advanced" feature set

The **.ini files** for this feature set are located in `scripts/BartenderSettings/Advanced`.

To **disable** this feature set, delete all .ini files in that folder (or the folder itself).

See the "**Limitations**" section further below before using this feature set.

&nbsp;

This feature set **lets you change** (per Heat level)
* how many cops can (re)spawn without backup once a wave is exhausted,
* the global spawn limit for how many cops may chase you at once,
* how quickly cops leave the pursuit if they don't belong (if at all),
* which vehicles may spawn to chase you (any amount, with counts and chances),
* which vehicles may spawn in roadblocks (any amount, with chances),
* which vehicles may spawn in scripted events (same as above),
* which vehicles may spawn as patrols in free-roam (ditto),
* which vehicle spawns in place of the regular helicopter, and
* when exactly the helicopter can (de / re)spawn (if at all).

&nbsp;

This feature set **also fixes** the displayed engagement count in the centre of the pursuit bar: its value is now perfectly accurate and reflects how many chasing cops remain in the current wave. The count ignores vehicles spawned through any Heavy or LeaderStrategy, the helicopter, and any vehicles that join the pursuit by detaching themselves from roadblocks.

&nbsp;

&nbsp;

&nbsp;



# Installation

**Before installing** this mod:
1. make sure you have read and understood the "**Limitations**" section of this README below,
2. make sure your game's `Speed.exe` is compatible (i.e. 5.75 MB / 6.029.312 bytes large), and
3. install an .asi loader or any mod with one (e.g. the [WideScreenFix](https://github.com/ThirteenAG/WidescreenFixesPack/releases/tag/nfsmw) mod by ThirteenAG).

&nbsp;

To **install** this mod, copy its `BartenderSettings` folder and compiled .asi file into the `scripts` folder located in your game's installation folder. If your game does not have a `scripts` folder, create one.

**After installing** this mod, you can configure its features by editing its configuration files. You can find these configuration (.ini) files in the `BartenderSettings` folder.

To **uninstall** this mod, remove its files from your game's `scripts` folder.

To **reinstall** this mod, uninstall it and repeat the installation process above.

&nbsp;

&nbsp;

&nbsp;



# Limitations

The two feature sets of this mod each come with caveats and limitations. To avoid nasty surprises and game instability, make sure you understand them all before you use this mod in any capacity.

Both feature sets of this mod should be **compatible** with all VltEd, Binary, and most .asi mods. All known and notable exceptions to this are explicitly mentioned in this section.

&nbsp;

"**Basic**" feature set:
* All vehicles you specify to replace the ramming SUVs (`BartenderSettings\Basic\Supports.ini`) should each have a low `MAXIMUM_AI_SPEED` value (the vanilla SUVs use 50) in their VltEd `aivehicle` entries. If they don't, they might cause stability issues by joining the pursuit after their ramming attempt(s); this effectively makes them circumvent the global spawn limit, and the (vanilla) game really does not like managing more than 8 active cops for very long.

* All vehicles you specify to replace Cross (`BartenderSettings\Basic\Supports.ini`) should each be unique to him, i.e. not be used by other cops elsewhere. If another cop with the same vehicle as Cross is present in a pursuit, no LeaderStrategy will be able to spawn.

* The string assignment for cops (`BartenderSettings\Basic\Labels.ini`) is incompatible with the `EnableCopDestroyedStringHook` feature of the [NFSMW Unlimiter](https://github.com/nlgxzef/NFSMWUnlimiter/releases) mod by nlgxzef. Either delete this mod's `Labels.ini` configuration file or disable Unlimiter's version of the feature by editing its `NFSMWUnlimiterSettings.ini` configuration file.

&nbsp;

"**Advanced**" feature set:
* The feature set disables itself if ANY Heat level lacks a valid "Chasers" spawn table (`BartenderSettings\Advanced\Cars.ini`); you must specify at least one car for each level.

* If the feature set is enabled, the following `pursuitlevels` VltEd parameters are ignored because the feature set fulfils their intended purposes with much greater customisation: `cops`, `HeliFuelTime`, `TimeBetweenHeliActive`, and `SearchModeHeliSpawnChance`.

* All vehicles you specify in any of the spawn tables (`BartenderSettings\Advanced\Cars.ini`) must each have the `CAR` class assigned to them in their `pvehicle` VltEd entries.

* All vehicles you specify to replace the helicopter (`BartenderSettings\Advanced\Helicopter.ini`) must each have the `CHOPPER` class assigned to them in their `pvehicle` VltEd entries.

* Rarely, cops that are not in "Roadblock" spawn tables (`BartenderSettings\Advanced\Cars.ini`) might still show up in roadblocks. This is a vanilla bug; it usually happens when the game attempts to spawn a "Chaser" while it is processing a roadblock request, causing it to place the wrong car in the requested roadblock. This bug isn't restricted to cop spawns: if the stars align, it can also happen with traffic cars or even the helicopter.

* The "Event" spawn tables (`BartenderSettings\Advanced\Cars.ini`) apply only after the first scripted, pre-generated cop has already spawned in a given event; instead, this first cop is always of the type specified in the event's `CopSpawnType` VltEd parameter. This is because the game actually requests the first cop before it loads any pursuit or Heat level information, making it impossible for the mod to know which spawn table to use for the very first vehicle.

* The global cop spawn limit (`BartenderSettings\Advanced\General.ini`) has a few odd quirks. These quirks are all vanilla behaviour; this mod is just more likely to expose you to them:
  * The game compares the limit to the total number of cops in the world, not just a pursuit.
  * The total number of cops includes all ground vehicles: "Chasers", supports, and roadblocks.
  * The game only removes vehicles from the total once they have fully despawned / disappeared.
  * If the total is at or above the limit, no more "Chasers" can spawn until it less again.
  * If the total is at or above the limit, new supports and roadblocks *can* still spawn.

  The big implication here is that support and roadblock vehicles can block new "Chasers" from spawning in pursuits if they spawn first. Keep this in mind if you encounter a situation where the game seemingly refuses to spawn as many "Chasers" as the global spawn limit should allow.

* Setting any global cop spawn limit (`BartenderSettings\Advanced\General.ini`) above 8 requires the [NFSMW LimitAdjuster](https://zolika1351.pages.dev/mods/nfsmwlimitadjuster) (LA) mod by Zolika1351 to work properly. Without it, the game will randomly start unloading models and assets because its default car loader cannot handle the workload of managing (potentially) dozens of extra vehicles. To make LA compatible with this mod, open its `LimitAdjuster.ini` configuration file and disable *all* features in its `[Options]` section; this will unlock the spawn limit without forcing an infinite amount of cops to spawn. Note that LA is not perfectly stable either: It is prone to crashing in the first 30 seconds of the first pursuit in a play session, but will generally stay stable if it does not crash there.

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