
── ■ │ WHAT IS NFSMW BARTENDER? (v3.00.01) │ ■ ────────────────────────────────────────────────────

VIEW THIS DOCUMENT with better formatting on GitHub: https://github.com/rng-guy/NFSMWBartender

Bartender adds NEW CUSTOMISATION OPTIONS to pursuits. These new options come in two feature sets:
 • the "BASIC" FEATURE SET lets you change many otherwise hard-coded values of the game, and
 • the "ADVANCED" FEATURE SET lets you change cop-spawning behaviour and tables without limits.

For OPTIMAL CUSTOMISABILITY, use VltEd along with Bartender. Bartender doesn't replace VltEd,
as Bartender's features are deliberately limited to things you cannot do with VltEd alone.

The SECTIONS BELOW address these questions in detail:
 1) • What does the "Basic" feature set do?
 2) • What does the "Advanced" feature set do?
 3) • What mods are (in)compatible with Bartender?
 4) • What other mods does Bartender depend on?
 5) • How do I install Bartender for my game?
 6) • How may I share or bundle Bartender?






── ■ │ 1 - WHAT DOES THE "BASIC" FEATURE SET DO? │ ■ ──────────────────────────────────────────────

The "Basic" feature set LETS YOU CHANGE (per Heat level)
 • at what time interval you gain passive bounty,
 • whether the cops can start non-player pursuits,
 • the maximum combo-bounty multiplier for destroying cops quickly,
 • how quickly and below what distance from cops the red "BUSTED" bar fills,
 • how quickly the green "EVADE" bar fills once all cops have lost sight of you,
 • whether hiding spots make racers in "COOLDOWN" mode completely invisible to cops,
 • whether player-damaged cop vehicles are destroyed instantly if flipped over,
 • when exactly (if at all) cop vehicles are destroyed regardless of damage if flipped over,
 • when exactly (if at all) racer vehicles are reset if flipped over,
 • which pursuit jurisdiction dispatch announces over radio after a Heat transition,
 • which of the enabled ground supports the cops can request in non-player pursuits,
 • the internal cooldown between non-Strategy roadblock requests,
 • the internal cooldown between Heavy / LeaderStrategy requests,
 • at what distance from racers roadblocks can spawn,
 • whether a successful roadblock spawn cancels the current cop formation,
 • when and to what extent (if at all) roadblock vehicles can join pursuits,
 • whether roadblock vehicles react to racers entering "COOLDOWN" mode,
 • whether roadblock vehicles react to racers hitting their spike strips,
 • whether HeavyStrategy 3 requests interact with roadblock requests and spawns,
 • which vehicles spawn in place of the ramming SUVs through HeavyStrategy 3,
 • which vehicles spawn in place of the roadblock SUVs through HeavyStrategy 4,
 • which vehicle spawns in place of Cross through LeaderStrategy 5, and
 • which vehicles spawn in place of Cross and his henchmen through LeaderStrategy 7.

The "Basic" feature set ALSO LETS YOU CHANGE (in general)
 • which pursuit stats (e.g. pursuit length, bounty) the game also tracks in races;
 • whether a given cop vehicle is affected by pursuit-breaker instakills of any kind;
 • which notification string the game displays whenever you destroy a given cop vehicle;
 • which radio callsigns and chatter a given cop vehicle can trigger in player pursuits;
 • under which conditions (if at all) a given cop vehicle shows up on the radar / mini-map;
 • how (if at all) line of sight affects the colour of the helicopter cone-of-vision icon; and
 • the selection, order, and length of interactive themes that play during player pursuits.

The "Basic" feature set ALWAYS FIXES FIFTEEN BUGS / ISSUES automatically:
 • the game now always updates the passive bounty increment after races,
 • the (mini-)map icons for cop vehicles now always flash at the correct pace,
 • transitions to Heat levels > 5 now trigger their proper radio announcements,
 • the game no longer plays each Heat-level announcement just once per game launch,
 • Challenge Series races now use the Heat level limits defined for them in VltEd,
 • the game now reads VltEd arrays correctly at each Blacklist rank and Heat level,
 • vehicles joining pursuits from roadblocks no longer ignore spawn limits for cops,
 • the helicopter mini-map icon is now always visible whenever a helicopter is active,
 • the helicopter vision-cone icon now always disappears whenever a helicopter is destroyed,
 • helicopters no longer cast static shadows (like cars do) with incorrect placements,
 • the game now counts spike-strip deployments accurately in cost-to-state calculations,
 • the game is no longer inadvertently biased in how it chooses to make Strategy requests,
 • Heat levels > 5 are no longer reset back to 5 when you enter free-roam or start an event,
 • Heat levels > 5 are now shown correctly in menus (requires Binary for missing textures), and
 • racers can no longer get busted due to line-of-sight issues while their green "EVADE" bar fills.

The "Basic" feature set CAN FIX THREE MORE BUGS / ISSUES, depending on its configuration:
 • the game no longer ignores VltEd settings for roadblocks and Strategies in races,
 • non-Strategy roadblock requests can no longer be stalled by HeavyStrategy 3 requests, and
 • HeavyStrategy 3 requests no longer become very rare at Heat levels with frequent roadblocks.






── ■ │ 2 - WHAT DOES THE "ADVANCED" FEATURE SET DO? │ ■ ───────────────────────────────────────────

The "Advanced" feature set LETS YOU CHANGE (per Heat level)
 • which roadblock setups can spawn through roadblock requests,
 • how likely cops are to call out roadblocks / spikes over the radio,
 • whether Heat increases passively whenever racers are being pursued,
 • how much Heat racers gain / lose whenever they assault a cop vehicle,
 • how much Heat racers gain / lose whenever their cost-to-state score increases,
 • how many chasing cops can (re)spawn regardless of the remaining engagement count,
 • below what total number of active cops in the world the game can spawn new chasing cops,
 • whether spawning decisions for chasing cops are independent of all other pursuit vehicles,
 • whether spawning decisions for traffic cars are independent of those for cops,
 • whether only chasing cops that have been destroyed can decrement the engagement count,
 • whether Heat transitions immediately trigger backup to update the engagement count,
 • how far away new chasing cops must spawn from all already active cops,
 • above what count (if at all) no more cops can join the pursuit from roadblocks,
 • when exactly (if at all) and how many cops that joined from roadblocks can flee the pursuit,
 • when exactly (if at all) and how many chasing cops from other Heat levels can flee the pursuit,
 • which vehicles (any amount, with counts and chances) can spawn to chase and search for racers,
 • which vehicles (same liberties as above) can spawn in non-Strategy roadblocks,
 • which vehicles (ditto) can spawn as pre-generated cops in scripted events,
 • which vehicles (ditto again) can spawn as free patrols outside pursuits,
 • which vehicle spawns in place of the regular helicopter,
 • when exactly (if at all) the helicopter can first spawn in each player pursuit,
 • when exactly (if at all) the helicopter can respawn if it runs out of fuel,
 • when exactly (if at all) the helicopter can respawn if it gets wrecked,
 • when exactly (if at all) the helicopter can respawn if it loses you,
 • when exactly (if at all) the helicopter can rejoin instead if it loses you,
 • when exactly (if at all) the helicopter can run out of fuel after each (re)spawn,
 • the internal cooldown between the helicopter's ramming attempts through HeliStrategy 2,
 • below what racer speed HeavyStrategy 3 vehicles cancel their ramming attempts early,
 • how many vehicles can spawn through each successful HeavyStrategy 3 request,
 • whether HeavyStrategy 3 vehicles can join the pursuit once their request expires,
 • above what count (if at all) no more cops can join the pursuit from HeavyStrategy 3 requests,
 • when exactly (if at all) LeaderStrategy 5 / 7 Cross and / or his henchmen become aggressive,
 • when exactly (if at all) the game can request a new LeaderStrategy once Cross is gone, and
 • when exactly (if at all) the game can request a new Strategy while another is still active.

The "Advanced" feature set ALSO LETS YOU CHANGE (in general)
 • what combination and arrangement of parts makes up a given roadblock setup,
 • how likely each roadblock setup is to spawn with vertically mirrored parts instead,
 • how much Heat racers gain / lose whenever they destroy a cop vehicle of a given type, and
 • which non-chasing cops are also tracked by the engagement count shown above the pursuit board.

The "Advanced" feature set ALWAYS FIXES EIGHT BUGS / ISSUES automatically:
 • HeavyStrategy 4 roadblocks can now spawn with more than 4 vehicles,
 • failed roadblock spawn attempts can no longer stall spawns for chasing cops,
 • the game no longer ignores VltEd settings for roadblocks and Strategies in races,
 • the Heat gauge no longer skips the transition animation for rapid Heat-level changes,
 • the helicopter AI is no longer crippled whenever a roadblock is present in the pursuit,
 • early Strategy despawns or cancellations no longer stall the game from making new requests,
 • the engagement count above the pursuit board now always tracks relevant cops accurately, and
 • the pathfinding of new cop vehicles no longer breaks whenever a race pursuit moves to free-roam.

The "Advanced" feature set CAN FIX FIVE MORE BUGS / ISSUES, depending on its configuration:
 • the cops no longer stop callling out roadblocks / spikes over the radio,
 • the game no longer fails to use four of its vanilla roadblock setups in pursuits,
 • the helicopter can no longer waste its spawn attempts by losing you nearly instantly,
 • HeavyStrategy 3 vehicles no longer spawn in passive mode without trying to ram anything, and
 • traffic spawns no longer slow down or stop at Heat levels with many cops / frequent roadblocks.






── ■ │ 3 - WHAT MODS ARE (IN)COMPATIBLE WITH BARTENDER? │ ■ ───────────────────────────────────────

Almost all VLTED AND BINARY MODS should be fully compatible with all Bartender configurations.

Bartender's "BASIC" FEATURE SET changes the way the game accesses some VltEd arrays:
 • In "pursuitlevels", "0x80deb840"             now uses the correct index at each Blacklist rank.
 • In "aivehicle",     "RepPointsForDestroying" now uses the correct index at each Heat level.

Bartender's "ADVANCED" FEATURE SET forces the game to no longer ignore the roadblock-related
"pursuitlevels" and the Strategy-related "pursuitsupport" VltEd settings in race pursuits. 
The feature set also replaces four specific "pursuitlevels" parameters in all pursuits:
 • the "cops" array,
 • "HeliFuelTime",
 • "TimeBetweenHeliActive", and
 • "SearchModeHeliSpawnChance".

Most OTHER .ASI MODS should be fully compatible with all Bartender configurations.
However, some pursuit-related .asi mods require manual (re)configuration for compatibility:
 • For "NFSMW LimitAdjuster" by Zolika1351, see the section about dependencies below.
 • For "XNFSMusicPlayer"     by xan1242,    delete Bartender's "[Music:Playlist]" parameter group.
 • In  "NFSMW Unlimiter"     by nlgxzef,    disable its "EnableCopDestroyedStringHook" feature.
 • In  "NFSMW ExtraOptions"  by ExOptsTeam, disable its "HeatLevelOverride",
                                                        "PursuitActionMode", and
                                                        "ZeroBountyFix" features.






── ■ │ 4 - WHAT OTHER MODS DOES BARTENDER DEPEND ON? │ ■ ──────────────────────────────────────────

Under certain conditions, Bartender MAY REQUIRE the "NFSMW LimitAdjuster" mod by Zolika1351.
Specifically, you likely need that mod if you configure Bartender in any of the following ways:
 • "[Chasers:Limits]"       in "CarSpawns.ini" : You define a global cop-spawn limit > 8.
 • "[Chasers:Independence]" in "CarSpawns.ini" : You enable independent spawns for chasing cops.
 • "[Traffic:Independence]" in "CarSpawns.ini" : You enable independent spawns for traffic cars.
 • "[Heavy3:Count]"         in "Strategies.ini": You define a vehicle count > 2 per request.
 • "[Heavy3:Joining]"       in "Strategies.ini": You enable joining from HeavyStrategy 3 requests.
 • "[Heavy3:Unblocking]"    in "Strategies.ini": You define very short unblock delays.
 
TO CONFIGURE "NFSMW LimitAdjuster" for optimal compatibility with Bartender:
 1) • place "NFSMWLimitAdjuster.asi" / ".ini" into the same folder as "speed.exe" (not "scripts");
 2) • under "[Options]" in "NFSMWLimitAdjuster.ini", disable every cop-related feature; 
 3) • under "[Limits]"  in "NFSMWLimitAdjuster.ini", set "TrafficCars"      to  50 (or higher), 
                                                         "PursuitCops"      to 255, and
                                                         "Vehicles_SoftCap" to 255.






── ■ │ 5 - HOW DO I INSTALL BARTENDER FOR MY GAME? │ ■ ────────────────────────────────────────────

BEFORE INSTALLING Bartender:
 1) • make sure your original copy of the game wasn't a repack or came pre-modified in any way,
 2) • read and understand the two sections about mod (in)compatibilities and dependencies above,
 3) • make sure your game's "speed.exe" is compatible (i.e. 5.75 MB / 6,029,312 bytes large), and
 4) • install an .asi loader or any mod with one (e.g. the "WideScreenFix" mod by ThirteenAG).

TO INSTALL Bartender:
 1) • if it doesn't exist already, create a "scripts" folder in your game's installation folder;
 2) • copy the contents of Bartender's "scripts" folder to your game's "scripts" folder;
 3) • if Bartender's .asi file gets flagged by your antivirus software, whitelist the file; and
 4) • (optional) in User Mode of Binary 2.8.3 or newer, load and apply "FixMissingTextures.end".

AFTER INSTALLING Bartender:
 1) • edit the configuration (.ini) files in the "BartenderSettings" folder to your liking; and
 2) • if you encounter any issues or want more feature details, see the "APPENDIX.txt" file.

TO UNINSTALL Bartender, remove its files from your game's "scripts" folder. There's no need
to remove the optional missing textures, as the game doesn't use them without Bartender.

TO UPDATE Bartender, uninstall it and repeat the installation process above.
If you update from a version older than v3.00.00, replace all old configuration files.






── ■ │ 6 - HOW MAY I SHARE OR BUNDLE BARTENDER? │ ■ ───────────────────────────────────────────────

You are free to bundle Bartender and its files with your own pursuit mod, NO CREDIT REQUIRED.
In the interest of code transparency, however, consider linking to Bartender's GitHub repository 
(https://github.com/rng-guy/NFSMWBartender) somewhere in your mod's documentation (e.g. README).

Finally, Bartender wouldn't have seen the light of day without
 • DarkByte, for Cheat Engine;
 • rx, for encouraging me to try .asi modding;
 • nlgxzef, for the Most Wanted debug symbols;
 • GuidedHacking, for their Cheat Engine tutorials;
 • Sebastiano Vigna, for his pseudorandom number generators;
 • Matthias C. M. Troffaes, for his configuration-file parser;
 • ExOptsTeam, for permitting me to use their Heat-level fixes;
 • Martin Leitner-Ankerl, for his performant hashmap implementation;
 • trelbutate, for his "NFSMW Cop Car Healthbars" mod as a learning resource; and
 • Orsal, zeta1207, Aven, Astra King79, and MORELLO, for testing and providing feedback.