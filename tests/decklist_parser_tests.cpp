#include <catch2/catch_test_macros.hpp>

#include <ppp/plugins/mtg_card_downloader/decklist_parser.hpp>

TEST_CASE("Parse decklist with raw names", "[decklist_raw_names]")
{
    const QString decklist{
        R"(
Activated Sleeper
Ajani, Sleeper Agent
Arcane Signet
Argentum Masticore
Bala Ged Recovery / Bala Ged Sanctuary
Birthing Pod
Blighted Agent
Bloated Contaminator
Branchloft Pathway / Boulderloft Pathway
Breeding Pool
Brimaz, Blight of Oreskos
Cankerbloom
Chromatic Lantern
Clearwater Pathway / Murkwater Pathway
Condemn
Cultivate
Darkbore Pathway / Slitherbore Pathway
Darkwater Catacombs
Debt of Loyalty
Deserted Beach
Dread Return
Emergence Zone
Ephemerate
Ertai, the Corrupted
Exhume
Fabled Passage
Flooded Strand
Forest
Forest
Forest
Forest
Geth, Lord of the Vault
Ghost Quarter
Glissa Sunslayer
Glissa, Herald of Predation
Glissa's Retriever
Godless Shrine
Grafted Butcher
Hallowed Fountain
Heliod, the Radiant Dawn / Heliod, the Warped Eclipse
Hengegate Pathway / Mistgate Pathway
Heroic Intervention
Hex Parasite
In Search of Greatness
Island
Ixhel, Scion of Atraxa
Jace, the Perfected Mind
Jin-Gitaxias, Progress Tyrant
Kindred Boon
Mana Drain
Marsh Flats
Massacre Wurm
Mikokoro, Center of the Sea
Misty Rainforest
Neoform
Nissa of Shadowed Boughs
Nissa, Ascended Animist
Opulent Palace
Overgrown Farmland
Overgrown Tomb
Phyrexian Arena
Phyrexian Censor
Phyrexian Delver
Phyrexian Obliterator
Phyrexian Revoker
Phyrexian Vindicator
Plains
Polluted Delta
Pyre of Heroes
Raffine's Tower
Reanimate
Reaper of Sheoldred
Reliquary Tower
Sandsteppe Citadel
Sheoldred / The True Scriptures
Single Combat
Skrelv, Defector Mite
Spellskite
Swamp
Swamp
Swamp
Tainted Observer
Tamiyo, Compleated Sage
Teleportation Circle
Temple Garden
Thrummingbird
Time Wipe
Tyrranax Rex
Unnatural Restoration
Vanquish the Horde
Venser, Corpse Puppet
Verdant Catacombs
Villainous Wealth
Vorinclex / The Grand Evolution
Vorinclex, Voice of Hunger
Watery Grave
White Sun's Twilight
Windswept Heath
Yawgmoth's Will
)"
    };
    auto decklist_type{ InferDecklistType(decklist) };
    const auto cards{ ParseDecklist(decklist_type, decklist) };
    REQUIRE(decklist_type == DecklistType::RawNames);
    REQUIRE(cards.size() == 99);
}

TEST_CASE("Parse MODO decklist", "[decklist_modo]")
{
    const QString decklist{
        R"(
1 Allay
1 Arcane Signet
1 Astral Cornucopia
1 Aura of Silence
1 Bathe in Light
1 Borrowed Time
1 Capsize
1 Case of the Ransacked Lab
1 Change of Heart
1 Clockspinning
1 Clown Car
1 Command Tower
1 Commit / Memory
1 Coveted Jewel
1 Dakra Mystic
1 Day's Undoing
1 Defiler of Faith
1 Disciple of Caelus Nin
1 Divine Resilience
1 Docent of Perfection / Final Iteration
1 Emrakul, the Promised End
1 Everflowing Chalice
1 Fascination
1 Field of Ruin
1 Fierce Guardianship
1 Flooded Strand
1 Flusterstorm
1 Forced Fruition
1 Fractured Identity
1 Geier Reach Sanitarium
1 Grasp of Fate
1 Hallowed Fountain
1 Hullbreaker Horror
1 Imprisoned in the Moon
1 Invulnerability
11 Island
1 Knight of the White Orchid
1 Kwain, Itinerant Meddler
1 Lore Broker
1 Loyal Warhound
1 Machine God's Effigy
1 Metallurgic Summonings
1 Mikokoro, Center of the Sea
1 Mind Games
1 Minds Aglow
1 Mistrise Village
1 Mystic Remora
1 Noyan Dar, Roil Shaper
1 Oblation
1 Octavia, Living Thesis
1 Orvar, the All-Form
1 Ossification
1 Plagiarize
8 Plains
1 Primal Amulet / Primal Wellspring
1 Prison Realm
1 Prosperity
1 Pursuit of Knowledge
1 Reliquary Tower
1 Rhystic Study
1 Seachrome Coast
1 Seal from Existence
1 Sejiri Shelter / Sejiri Glacier
1 Skyscribing
1 Snow-Covered Plains
1 Sol Ring
1 Spell Burst
1 Surge of Salvation
1 Surveyor's Scope
1 Swiftfoot Boots
1 Temple Bell
1 Temporal Cascade
1 Tidal Control
1 Time Stop
1 Tyrite Sanctum
1 Unexplained Absence
1 Vision Skeins
1 Wandering Archaic / Explore the Vastlands
1 Whim of Volrath
1 Will of the Temur
1 Yavimaya, Cradle of Growth
1 Your Temple Is Under Attack
)"
    };
    auto decklist_type{ InferDecklistType(decklist) };
    const auto cards{ ParseDecklist(decklist_type, decklist) };
    REQUIRE(decklist_type == DecklistType::MODO);
    REQUIRE(cards.size() == 82);
}

TEST_CASE("Parse decklist with names and set code", "[decklist_names_and_set]")
{
    const QString decklist{
        R"(
1 Aether Mutation (APC)
1 Ancestors' Aid (LCI)
1 Armored Scrapgorger (ONE)
1 Aura Barbs (PLST)
1 Baral's Expertise (AER)
1 Blessed Breath (CHK)
1 Blindblast (JMP)
1 Blue Sun's Twilight (ONE)
1 Blur (CLB)
1 Bombadil's Song (LTR)
1 Candles' Glow (CHK)
1 Canopy Vista (MKC)
1 Cinder Glade (MKC)
1 Clifftop Retreat (DOM)
1 Cloudshift (AVR)
1 Command Tower (ZNC)
1 Cytoshape (DIS)
1 Depths of Desire (XLN)
1 Desperate Ritual (CHK)
1 Drover of the Mighty (XLN)
1 Enter the Unknown (RIX)
1 Exotic Orchard (MKC)
1 Expedite (CMR)
1 Feather, the Redeemed (WAR)
1 Fists of Flame (CMR)
5 Forest (ICE)
1 Gaea's Gift (BRO)
1 Galvanic Discharge (MH3)
1 Geistwave (MID)
1 Gift of the Viper (MH3)
1 Gold Rush (OTJ)
1 Hardbristle Bandit (OTJ)
1 Ilysian Caryatid (THB)
1 Into the Fray (SOK)
1 Involuntary Employment (SNC)
7 Island (ICE)
1 Jadzi, Oracle of Arcavios / Journey to the Oracle (STX)
1 Kodama's Reach (CHK)
1 Luxurious Libation (SNC)
1 Mossfire Valley (MKC)
5 Mountain (ICE)
1 Myr Convert (ONE)
1 Natural Affinity (MMQ)
1 Oasis Gardener (OTJ)
1 Opaline Unicorn (THS)
1 Otherworldly Outburst (EMN)
1 Overgrown Farmland (MID)
1 Ovinize (DMR)
1 Paradise Druid (WAR)
1 Path to Exile (MKC)
1 Peer Through Depths (MMA)
2 Plains (ICE)
1 Psychic Puppetry (CHK)
1 Rampant Growth (C21)
1 Ranging Raptors (XLN)
1 Ravenform (KHM)
1 Revenge of the Drowned (MID)
1 Rile (AFC)
1 Scale the Heights (J22)
1 Shelter (MH1)
1 Shore Up (DMU)
1 Sigil Blessing (DDG)
1 Slip Through Space (OGW)
1 Snakeskin Veil (KHM)
1 Snap (WOC)
1 Spawning Breath (ROE)
1 Spiritualize (ODY)
1 Splicer's Skill (MH1)
1 Startle (MID)
1 Storm-Kiln Artist (OTC)
1 Strionic Resonator (BRC)
1 Sudden Breakthrough (STX)
1 Sungrass Prairie (MKC)
1 Temple of Abandon (MKC)
1 Temple of Epiphany (M21)
1 Temple of Mystery (M20)
1 Temple of Plenty (MKC)
1 Temple of Triumph (M21)
1 Traitorous Greed (M21)
1 Tyvar's Stand (ONE)
1 Veil of Secrecy (BOK)
1 Wose Pathfinder (LTR)
1 Yavimaya Coast (9ED)
1 You See a Guard Approach (AFR)
)"
    };
    auto decklist_type{ InferDecklistType(decklist) };
    const auto cards{ ParseDecklist(decklist_type, decklist) };
    REQUIRE(decklist_type == DecklistType::NamesAndSet);
    REQUIRE(cards.size() == 84);
}

TEST_CASE("Parse MTGA decklist", "[decklist_mtga]")
{
    const QString decklist{
        R"(
About
Angel Typal

Commander
1 Gisela, Blade of Goldnight (AVR) 209

Deck
1 Adarkar Valkyrie (C14) 63
1 Agrus Kos, Wojek Veteran (RAV) 190
1 Angrath's Marauders (XLN) 132
1 Arcbond (FRF) 91
1 Archon of Justice (M12) 6
1 Battlefield Forge (ORI) 244
1 Boros Guildgate (CMR) 478
1 Boros Guildmage (RAV) 242
1 Calamity Bearer (KHM) 125
1 Conquering Manticore (ROE) 139
1 Cradle of Vitality (C13) 7
1 Crib Swap (C15) 65
1 Dictate of the Twin Gods (JOU) 93
1 Dreamstone Hedron (ROE) 216
1 Duergar Hedge-Mage (EVE) 137
1 Evolving Wilds (BFZ) 236
1 Fated Retribution (BNG) 11
1 Furnace of Rath (8ED) 187
1 Grab the Reins (MRD) 95
1 Gratuitous Violence (ONS) 212
1 Hamletback Goliath (M13) 136
1 Hedron Archive (BFZ) 223
1 Hoarding Dragon (M15) 149
1 Impelled Giant (EVE) 58
1 Intimidation Bolt (ARB) 99
1 Knight of the White Orchid (ORI) 21
1 Legion's Initiative (DGM) 81
1 Loxodon Warhammer (MRD) 201
1 Malignus (AVR) 148
1 Master Warcraft (RAV) 250
1 Mimic Vat (SOM) 175
1 Miraculous Recovery (DDL) 30
1 Mob Rule (FRF) 109
1 Molten Primordial (GTC) 101
1 Mountain (PCA) 147
1 Mountain (ZEN) 244
1 Mountain (C13) 352
1 Mountain (DDN) 35
1 Mountain (M11) 245
1 Mountain (BFZ) 265
1 Mountain (BFZ) 266
1 Mountain (BFZ) 268
1 Mountain (ORI) 268
1 Mountain (ORI) 266
1 Mountain (DTK) 259
1 Mountain (DTK) 261
1 Mountain (M19) 274
1 Mountain (M19) 273
1 Nim Deathmantle (2X2) 309
1 Nobilis of War (MM2) 195
1 Oblation (C14) 83
1 Oblivion Ring (M12) 27
1 Orim's Thunder (C15) 78
1 Phyrexian Rebirth (MBS) 15
1 Plains (DDO) 33
1 Plains (DDN) 38
1 Plains (ZEN) 233
1 Plains (RAV) 289
1 Plains (M13) 232
1 Plains (BFZ) 252
1 Plains (M19) 264
1 Plains (BFZ) 254
1 Plains (J22) 98
1 Plains (BFZ) 250
2 Plains (M19) 261
1 Razia, Boros Archangel (RAV) 223
1 Realm-Cloaked Giant / Cast Off (ELD) 26
1 Reforge the Soul (AVR) 151
1 Reliquary Tower (C15) 301
1 Return to Dust (TSP) 39
1 Ride Down (KTK) 194
1 Rout (CNS) 80
1 Sandstone Bridge (BFZ) 243
1 Skullclamp (DST) 140
1 Solemn Simulacrum (C15) 269
1 Soulbright Flamekin (MM2) 126
1 Spark Trooper (GTC) 199
1 Spitemare (DDH) 16
1 Stasis Snare (BFZ) 50
1 Stonehewer Giant (MOR) 24
1 Sunbond (BNG) 28
1 Sunforger (RAV) 272
1 Sunhome, Fortress of the Legion (RAV) 282
1 Swiftfoot Boots (C14) 275
1 Taj-Nar Swordsmith (MM2) 36
1 Tajic, Blade of the Legion (DGM) 107
1 Temple of Triumph (M21) 256
1 Thousand-Year Elixir (LRW) 263
1 Thunderblust (EVE) 63
1 Unwilling Recruit (EVE) 64
1 War Elemental (MRD) 112
1 Well of Lost Dreams (C13) 271
1 Wind-Scarred Crag (FRF) 175
1 Wing Shards (C14) 97
1 Woolly Razorback (CSP) 25
1 Word of Seizing (C14) 185
1 Zealous Conscripts (AVR) 166
)"
    };
    auto decklist_type{ InferDecklistType(decklist) };
    const auto cards{ ParseDecklist(decklist_type, decklist) };
    REQUIRE(decklist_type == DecklistType::MTGA);
    REQUIRE(cards.size() == 98);
}

TEST_CASE("Parse Moxfield decklist", "[decklist_moxfield]")
{
    const QString decklist{
        R"(
1 Amber Gristle O'Maul (CLB) 159 #Dwarf
1 Avaricious Dragon (ORI) 131 #Payoff Dragon
1 Axgard Cavalry (KHM) 121 #Dwarf
1 Blast-Furnace Hellkite (BRC) 12 #Payoff Dragon
1 Bloodfire Dwarf (APC) 56 #Dwarf
1 Bogardan Hellkite (TSP) 147 #Payoff Dragon #Removal
1 Buried Ruin (BRC) 177
1 Chaos Warp (MKC) 149 #Removal
1 Cursed Mirror (BRC) 115 #Ramp
1 Deflecting Swat (SLD) 1552
1 Dragon Mage (M20) 135 #Payoff Dragon
1 Duplicant (ARC) 106 #Payoff Artifact #Removal
1 Dwarven Blastminer (ONS) 199 #Dwarf
1 Dwarven Grunt (PLST) ODY-185 #Dwarf
1 Dwarven Mine (ELD) 243
1 Dwarven Miner (MIR) 169 #Dwarf
1 Dwarven Nomad (MIR) 170 #Dwarf
1 Dwarven Recruiter (PLST) ODY-186 #Dwarf
1 Dwarven Scorcher (JUD) 86 #Dwarf
1 Dwarven Thaumaturgist (WTH) 98 #Dwarf
1 Dwarven Trader (HML) 72b #Dwarf
1 Electrostatic Infantry (DMU) 122 #Dwarf
1 Enslaved Dwarf (TOR) 96 #Dwarf
1 Fearless Liberator (KHM) 135 #Dwarf
1 Flywheel Racer (MOM) 259 #Ramp #Tap
1 Ganax, Astral Hunter (CLB) 176 #Payoff Dragon
1 Gemstone Caverns (EOS) 16
1 Gerrard's Hourglass Pendant (DMC) 17 #Protect
1 Graaz, Unstoppable Juggernaut (ONE) 334 #Payoff Artifact
1 Great Furnace (BRC) 187
1 Great Train Heist (OTJ) 125 #Ramp
1 Hellkite Charger (CMM) 232 #Payoff Dragon
1 Hellkite Igniter (BRC) 117 #Payoff Dragon
1 Hexplate Wallbreaker (ONC) 14 #Payoff Artifact
1 Hoard-Smelter Dragon (SOM) 93 #Payoff Dragon
1 Holdout Settlement (OGW) 172
1 Hunted Dragon (C15) 159 #Payoff Dragon
1 Knollspine Dragon (GN3) 82 #Payoff Dragon
1 Liberated Dwarf (JUD) 95 #Dwarf
1 Luxurious Locomotive (OTJ) 244 *F* #Payoff Artifact #Tap
1 Magda, the Hoardmaster (OTJ) 133 #Dwarf
1 Meteor Golem (M19) 241 #Payoff Artifact #Removal
1 Mines of Moria (LTR) 257
1 Mirrorworks (BRC) 149 #Payoff Artifact
1 Moonveil Dragon (DKA) 99 #Payoff Dragon
9 Mountain (WC98) br346
16 Mountain (WC98) br343
1 Null Brooch (EXO) 136 #Protect
1 Oblivion Stone (BRC) 153 #Removal
1 Pithing Needle (MID) 257 #Payoff Artifact
1 Plundering Barbarian (AFR) 158 #Dwarf
1 Scourge of the Throne (MKC) 160 #Payoff Dragon
7 Seven Dwarves (ELD) 141 #Dwarf
1 Silent Gravestone (RIX) 182 #Payoff Artifact
1 Solemn Simulacrum (DDU) 62 #Ramp
1 Sorcerous Spyglass (TSR) 401 #Payoff Artifact
1 Soul of New Phyrexia (MOC) 382 #Protect
1 Soulless Jailer (ONE) 241 #Payoff Artifact
1 Spark Mage (ODY) 222 #Dwarf
1 Spine of Ish Sah (BRC) 162 #Payoff Artifact #Removal
1 Springleaf Drum (LRW) 261 #Ramp #Tap
1 Survivors' Encampment (HOU) 184
1 The Stasis Coffin (BRO) 245 #Protect
1 Tyrant's Familiar (C14) 39 #Payoff Dragon #Removal
1 Universal Automaton (MH1) 235 #Dwarf
1 Unlicensed Hearse (OTP) 64 #Tap
1 Vault Robber (KHM) 158 #Dwarf
1 Wondrous Crucible (BRC) 20 #Payoff Artifact
1 Worldgorger Dragon (JUD) 103 #Payoff Dragon #Protect
)"
    };
    auto decklist_type{ InferDecklistType(decklist) };
    const auto cards{ ParseDecklist(decklist_type, decklist) };
    REQUIRE(decklist_type == DecklistType::Moxfield);
    REQUIRE(cards.size() == 69);
}

TEST_CASE("Parse Archidekt decklist", "[decklist_archidekt]")
{
    const QString decklist{
        R"(
1x Amoeboid Changeling (h09) 3 *F* [Protection]
1x Ancient Ziggurat (h09) 31 *F* [Land]
1x Aphetto Dredging (h09) 28 *F* [Recursion]
1x Arid Mesa (mh2) 244 [Land]
1x Ash Barrens (brc) 174 [Land]
1x Basal Sliver (tsp) 96 [Ramp]
1x Birthing Boughs (mh1) 221 [Tokens]
1x Bloodline Pretender (khm) 235 [Counters]
1x Brood Sliver (h09) 22 *F* [Tokens]
1x Cavern of Souls (avr) 226 [Land]
1x Cloudshredder Sliver (cmm) 919 [Evasion]
1x Coat of Arms (h09) 29 *F* [Anthem]
1x Command Tower (mkc) 256 [Land]
1x Constricting Sliver (m15) 7 [Removal]
1x Crypt Sliver (cmm) 863 [Protection]
1x Cryptolith Rite (soi) 200 [Ramp]
1x Crystalline Sliver (h09) 11 *F* [Protection]
1x Darkheart Sliver (tsr) 249 [Lifegain]
1x Distant Melody (h09) 27 *F* [Draw]
1x Dormant Sliver (tsr) 250 [Draw]
1x Dregscape Sliver (mh1) 88 [Recursion]
1x Elven Chorus (ltr) 160 [Ramp]
1x Endless Whispers (5dn) 49 [Recursion]
1x Essence Sliver (lgn) 13 [Lifegain]
5x Forest (h09) 41 *F* [Land]
1x Frenetic Sliver (plc) 157 [Protection]
1x Fungus Sliver (h09) 21 *F* [Pump]
1x Gaea's Will (mh2) 162 [Recursion]
1x Gemhide Sliver (h09) 8 *F* [Ramp]
1x Gemstone Mine (dmr) 455 [Land]
1x Gilded Lotus (m13) 206 [Ramp]
1x Harmonic Sliver (cmm) 924 [Removal]
1x Hatchery Sliver (cmm) 741 [Tokens]
1x Haunting Voyage (khm) 296 [Recursion]
1x Heart Sliver (h09) 7 *F* [Pump]
1x Hibernation Sliver (h09) 12 *F* [Protection]
1x Hollowhead Sliver (cmm) 878 [Draw]
1x Homing Sliver (h09) 19 *F* [Tutor]
1x Horned Sliver (tmp) 234 [Evasion]
1x Hunter Sliver (lgn) 102 [Removal]
1x Indatha Triome (iko) 248 [Land]
2x Island (h09) 38 *F* [Land]
1x Jetmir's Garden (plst) SNC-250 [Land]
1x Ketria Triome (plst) IKO-250 [Land]
1x Kindred Discovery (wot) 69 [Draw]
1x Lavabelly Sliver (cmm) 927 [Burn]
1x Magma Sliver (sld) 641 *F* [Pump]
1x Mana Echoes (ons) 218 [Ramp]
1x Manaweft Sliver (m14) 184 [Ramp]
1x Might Sliver (h09) 23 *F* [Pump]
1x Mirror Entity (mkc) 75 [Anthem]
3x Mountain (h09) 40 *F* [Land]
1x Muscle Sliver (h09) 9 *F* [Pump]
1x Myriad Landscape (c19) 261 [Land]
1x Nature's Lore (mkc) 178 [Ramp]
1x Necrotic Sliver (h09) 20 *F* [Removal]
1x Path of Ancestry (brc) 192 [Land]
2x Plains (h09) 37 *F* [Land]
1x Predatory Sliver (m14) 189 [Anthem]
1x Psionic Sliver (tsp) 72 [Removal]
1x Quick Sliver (h09) 10 *F* [Protection]
1x Rampant Growth (c21) 204 [Ramp]
1x Raugrin Triome (piko) 251p [Land]
1x Realmwalker (khm) 188 [Draw]
1x Reflections of Littjara (khm) 400 *F* [Copy]
1x Regal Sliver (cmm) 724 [Draw]
1x Root Sliver (lgn) 137 [Protection]
1x Sacred Foundry (rav) 280 [Land]
1x Savai Triome (piko) 253p [Land]
1x Secluded Courtyard (neo) 275 [Land]
1x Sentinel Sliver (m14) 30 [Protection]
1x Shifting Sliver (cmm) 855 [Evasion]
1x Shire Terrace (ltr) 261 [Land]
1x Sinew Sliver (cmm) 837 [Anthem]
1x Sliver Gravemother (cmm) 707 *F* [Commander{top}]
1x Sliver Hivelord (cmm) 937 [Commander{top}]
1x Sliver Legion (fut) 158 [Commander{top}]
1x Sliver Overlord (h09) 24 *F* [Commander{top}]
1x Sliver Queen (sth) 129 [Commander{top}]
1x Spara's Headquarters (psnc) 257p [Land]
1x Spiteful Sliver (cmm) 881 [Removal]
2x Swamp (h09) 39 *F* [Land]
1x Synapse Sliver (cmm) 857 [Draw]
1x Syphon Sliver (m14) 117 [Lifegain]
1x Tarnished Citadel (ody) 329 [Land]
1x Taunting Sliver (cmm) 727 [Protection]
1x Telekinetic Sliver (tsp) 84 [Removal]
1x The First Sliver (mh1) 200 [Commander{top}]
1x Urza's Incubator (c15) 273 [Ramp]
1x Virulent Sliver (h09) 2 *F* [Infect]
1x Vivid Creek (h09) 35 *F* [Land]
1x Vivid Grove (h09) 36 *F* [Land]
1x Ward Sliver (lgn) 25 [Protection]
1x Wild Pair (h09) 30 *F* [Tutor]
1x Winged Sliver (h09) 4 *F* [Evasion]
1x Zagoth Triome (piko) 259p [Land]
)"
    };
    auto decklist_type{ InferDecklistType(decklist) };
    const auto cards{ ParseDecklist(decklist_type, decklist) };
    REQUIRE(decklist_type == DecklistType::Archidekt);
    REQUIRE(cards.size() == 96);
}
