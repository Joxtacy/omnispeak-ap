//ap definitions
#ifndef AP_DEFS_H
#define AP_DEFS_H

//location base ranges
#define AP_LOC_BASE_LEVEL_COMPLETE	10000
#define AP_LOC_BASE_KEYGEM			20000
#define AP_LOC_BASE_KEYCARD			30000
// 40000 was the original pointsanity base when class+instance were packed
// into AP_LOC_LEVEL_STRIDE = 100. Per-level instance counts (up to 124 for
// 100-pt items in CK5 lvl 1) blew through that scheme, so pointsanity moved
// to a roomier per-class layout at 100000+ (see AP_LOC_BASE_POINTSANITY
// below). 40000–99999 is intentionally unused — do not repurpose without
// coordinating with the apworld.
#define AP_LOC_BASE_KEG				50000
#define AP_LOC_BASE_FLASK			60000
#define AP_LOC_BASE_POINTSANITY		100000

//spacing
#define AP_LOC_EPISODE_STRIDE		2000
#define AP_LOC_LEVEL_STRIDE			100
#define AP_LOC_ITEM_STRIDE			1

#define AP_EPISODE_CK4				1
#define AP_EPISODE_CK5				2

#define LOC_LEVEL_COMPLETE(ep, lvl) \
(AP_LOC_BASE_LEVEL_COMPLETE + \
((ep) * AP_LOC_EPISODE_STRIDE) + \
((lvl) * AP_LOC_LEVEL_STRIDE))

#define LOC_KEYGEM(ep, lvl, gem) \
(AP_LOC_BASE_KEYGEM + \
((ep) * AP_LOC_EPISODE_STRIDE) + \
((lvl) * AP_LOC_LEVEL_STRIDE) + \
(gem))

#define LOC_SECURITY_KEYCARD(ep, lvl) \
(AP_LOC_BASE_KEYCARD + \
((ep) * AP_LOC_EPISODE_STRIDE) + \
((lvl) * AP_LOC_LEVEL_STRIDE))

// Pointsanity uses its own strides, separate from the global
// AP_LOC_*_STRIDE values above, because real maps have up to ~124 instances
// of a single point class per level — too dense to share AP_LOC_LEVEL_STRIDE.
// Layout:
//   loc = BASE + cls * CLASS_STRIDE + ep * EP_STRIDE + lvl * LVL_STRIDE + inst
// cls 0..5 maps to point values 100/200/500/1000/2000/5000 (engine item
// index minus 4). Round-decimal addressing lets a reader eyeball the class
// from the 100k digit: 1xxxxx = 100pt, 2xxxxx = 200pt, ..., 6xxxxx = 5000pt.
// Apworld decodes with:
//   cls      = (loc - BASE) / CLASS_STRIDE
//   ep       = ((loc - BASE) % CLASS_STRIDE) / EP_STRIDE
//   lvl      = ((loc - BASE) % EP_STRIDE) / LVL_STRIDE
//   instance = (loc - BASE) % LVL_STRIDE
#define AP_POINTSANITY_CLASS_STRIDE		100000
#define AP_POINTSANITY_EPISODE_STRIDE	20000
#define AP_POINTSANITY_LEVEL_STRIDE		1000

#define LOC_POINTSANITY(cls, ep, lvl, inst) \
(AP_LOC_BASE_POINTSANITY + \
((cls) * AP_POINTSANITY_CLASS_STRIDE) + \
((ep)  * AP_POINTSANITY_EPISODE_STRIDE) + \
((lvl) * AP_POINTSANITY_LEVEL_STRIDE) + \
(inst))

#define LOC_KEG(ep, lvl, idx) \
(AP_LOC_BASE_KEG + \
((ep) * AP_LOC_EPISODE_STRIDE) + \
((lvl) * AP_LOC_LEVEL_STRIDE) + \
(idx))

#define LOC_FLASK(ep, lvl, idx) \
(AP_LOC_BASE_FLASK + \
((ep) * AP_LOC_EPISODE_STRIDE) + \
((lvl) * AP_LOC_LEVEL_STRIDE) + \
(idx))

typedef enum
{
	CK_KEYGEM_RED = 0,
	CK_KEYGEM_YELLOW = 1,
	CK_KEYGEM_BLUE = 2,
	CK_KEYGEM_GREEN = 3,
	CK_EXTRA_LIFE = 10,
	CK_STUNNER_AMMO = 11
} CK_ItemId;

typedef enum
{
	//Universal Items
	AP_ITEM_POGO,
	AP_ITEM_STUNNER,
	AP_ITEM_STUNNER_AMMO,
	AP_ITEM_EXTRA_KEEN,

	//CK4 Specific Item
	AP_ITEM_WETSUIT,

	//BWB Megarocket
	AP_ITEM_BWBMR,

	//CK4 Border Village
	AP_ITEM_BV,

	//CK4 Slug Village
	AP_ITEM_SV,

	//CK4 Perilous Pit
	AP_ITEM_PP,
	AP_ITEM_PP_RED_GEM,
	AP_ITEM_PP_BLUE_GEM,
	AP_ITEM_PP_GEMSET,

	//CK4 Cave of the Descendents
	AP_ITEM_COTD,
	AP_ITEM_COTD_RED_GEM,
	AP_ITEM_COTD_YELLOW_GEM,
	AP_ITEM_COTD_GEMSET,

	//CK4 Chasm of Chills
	AP_ITEM_COC,

	//CK4 Crystalus
	AP_ITEM_CRYS,
	AP_ITEM_CRYS_RED_GEM,
	AP_ITEM_CRYS_BLUE_GEM,
	AP_ITEM_CRYS_YELLOW_GEM,
	AP_ITEM_CRYS_GREEN_GEM,
	AP_ITEM_CRYS_GEMSET,

	//CK4 Hilville
	AP_ITEM_HI,

	//CK4 Sand Yego
	AP_ITEM_SY,
	AP_ITEM_SY_GREEN_GEM,
	AP_ITEM_SY_GEMSET,

	//CK4 Miragia
	AP_ITEM_MIR,

	//CK4 Lifewater Oasis
	AP_ITEM_LO,
	AP_ITEM_LO_GREEN_GEM,
	AP_ITEM_LO_GEMSET,

	//CK4 Pyramid of the Moons
	AP_ITEM_POTM,
	AP_ITEM_POTM_YELLOW_GEM,
	AP_ITEM_POTM_GEMSET,

	//CK4 Pyramid of Shadows
	AP_ITEM_POS,
	AP_ITEM_POS_BLUE_GEM,
	AP_ITEM_POS_GEMSET,

	//CK4 Pyramid of the Gnosticine Ancients
	AP_ITEM_POTGA,
	AP_ITEM_POTGA_RED_GEM,
	AP_ITEM_POTGA_GREEN_GEM,
	AP_ITEM_POTGA_GEMSET,

	//CK4 Pyramid of the Forbidden
	AP_ITEM_POTF,
	AP_ITEM_POTF_RED_GEM_1,
	AP_ITEM_POTF_RED_GEM_2, //pof has 2 red gems
	AP_ITEM_POTF_BLUE_GEM,
	AP_ITEM_POTF_GREEN_GEM,
	AP_ITEM_POTF_YELLOW_GEM,
	AP_ITEM_POTF_GEMSET,

	//CK4 Isle of Tar
	AP_ITEM_IOT,
	AP_ITEM_IOT_RED_GEM,
	AP_ITEM_IOT_BLUE_GEM,
	AP_ITEM_IOT_YELLOW_GEM,
	AP_ITEM_IOT_GEMSET,

	//CK4 Isle of Fire
	AP_ITEM_IOF,
	AP_ITEM_IOF_BLUE_GEM,
	AP_ITEM_IOF_YELLOW_GEM,
	AP_ITEM_IOF_GEMSET,
	
	//CK4 Well of Wishes
	AP_ITEM_WOW,

	//CK5 Ion Ventilation System
	AP_ITEM_IVS,

	//CK5 Security Center
	AP_ITEM_SC,
	AP_ITEM_SC_RED_GEM,
	AP_ITEM_SC_BLUE_GEM,
	AP_ITEM_SC_KEYCARD,
	AP_ITEM_SC_GEMSET,

	//CK5 Defense Tunnel Vlook
	AP_ITEM_DTV,
	AP_ITEM_DTV_RED_GEM,
	AP_ITEM_DTV_YELLOW_GEM,
	AP_ITEM_DTV_KEYCARD,
	AP_ITEM_DTV_GEMSET,

	//CK5 Energy Flow Systems
	AP_ITEM_EFS,
	AP_ITEM_EFS_RED_GEM,
	AP_ITEM_EFS_YELLOW_GEM,
	AP_ITEM_EFS_BLUE_GEM,
	AP_ITEM_EFS_GREEN_GEM,
	AP_ITEM_EFS_GEMSET,

	//CK5 Defense Tunnel Burrh
	AP_ITEM_DTB,
	AP_ITEM_DTB_YELLOW_GEM,
	AP_ITEM_DTB_RED_GEM,
	AP_ITEM_DTB_BLUE_GEM,
	AP_ITEM_DTB_GREEN_GEM,
	AP_ITEM_DTB_KEYCARD,
	AP_ITEM_DTB_GEMSET,

	//CK5 Regulation Control Center
	AP_ITEM_RCC,
	AP_ITEM_RCC_RED_GEM,
	AP_ITEM_RCC_YELLOW_GEM,
	AP_ITEM_RCC_BLUE_GEM,
	AP_ITEM_RCC_GEMSET,

	//CK5 Defense Tunnel Sorra
	AP_ITEM_DTS,
	AP_ITEM_DTS_KEYCARD,
	AP_ITEM_DTS_YELLOW_GEM,
	AP_ITEM_DTS_GEMSET,

	//CK5 Neutrino Burst Injector
	AP_ITEM_NBI,
	AP_ITEM_NBI_BLUE_GEM,
	AP_ITEM_NBI_RED_GEM,
	AP_ITEM_NBI_GEMSET,

	//CK5 Defense Tunnel Teln
	AP_ITEM_DTT,
	AP_ITEM_DTT_RED_GEM,
	AP_ITEM_DTT_YELLOW_GEM,
	AP_ITEM_DTT_GREEN_GEM,
	AP_ITEM_DTT_BLUE_GEM,
	AP_ITEM_DTT_KEYCARD,
	AP_ITEM_DTT_GEMSET,

	//CK5 Brownian Motion Inducer
	AP_ITEM_BMI,
	AP_ITEM_BMI_YELLOW_GEM,
	AP_ITEM_BMI_BLUE_GEM,
	AP_ITEM_BMI_GEMSET,

	//CK5 Gravitational Damping Hub
	AP_ITEM_GDH,
	AP_ITEM_GDH_KEYCARD,
	AP_ITEM_GDH_GREEN_GEM,
	AP_ITEM_GDH_RED_GEM,
	AP_ITEM_GDH_GEMSET,

	//CK5 Quantum Explosion Dynamo
	AP_ITEM_QED,
	AP_ITEM_QED_BLUE_GEM,
	AP_ITEM_QED_GREEN_GEM,
	AP_ITEM_QED_RED_GEM,
	AP_ITEM_QED_YELLOW_GEM,
	AP_ITEM_QED_GEMSET,

	//CK5 Korath III Base (secret level; no gems/keycard)
	AP_ITEM_KORATH,

	//Victory Items
	AP_ITEM_KEEN4_COMPLETE,
	AP_ITEM_KEEN5_COMPLETE,

	AP_ITEM_MAX
} AP_ItemId;

typedef enum
{
	AP_LEVEL_BORDER_VILLAGE = 1,
	AP_LEVEL_SLUG_VILLAGE = 2,
	AP_LEVEL_THE_PERILOUS_PIT = 3,
	AP_LEVEL_CAVE_OF_THE_DESCENDENTS = 4,
	AP_LEVEL_CHASM_OF_CHILLS = 5,
	AP_LEVEL_CRYSTALUS = 6,
	AP_LEVEL_HILVILLE = 7,
	AP_LEVEL_SAND_YEGO = 8,
	AP_LEVEL_MIRAGIA = 9,
	AP_LEVEL_LIFEWATER_OASIS = 10,
	AP_LEVEL_PYRAMID_OF_THE_MOONS = 11,
	AP_LEVEL_PYRAMID_OF_SHADOWS = 12,
	AP_LEVEL_PYRAMID_OF_THE_GNOSTICENE_ANCIENTS = 13,
	AP_LEVEL_PYRAMID_OF_THE_FORBIDDEN = 14,
	AP_LEVEL_ISLE_OF_TAR = 15,
	AP_LEVEL_ISLE_OF_FIRE = 16,
	AP_LEVEL_WELL_OF_WISHES = 17,
	AP_LEVEL_BEAN_WITH_BACON_MEGAROCKET = 18
} CK4_LevelId;

typedef enum
{
	AP_LEVEL_ION_VENTILATION_SYSTEM = 1,
	AP_LEVEL_SECURITY_CENTER = 2,
	AP_LEVEL_DEFENSE_TUNNEL_VLOOK = 3,
	AP_LEVEL_ENERGY_FLOW_SYSTEMS = 4,
	AP_LEVEL_DEFENSE_TUNNEL_BURRH = 5,
	AP_LEVEL_REGULATION_CONTROL_CENTER = 6,
	AP_LEVEL_DEFENSE_TUNNEL_SORRA = 7,
	AP_LEVEL_NEUTRINO_BURST_INJECTOR = 8,
	AP_LEVEL_DEFENSE_TUNNEL_TELN = 9,
	AP_LEVEL_BROWNIAN_MOTION_INDUCER = 10,
	AP_LEVEL_GRAVITATIONAL_DAMPING_HUB = 11,
	AP_LEVEL_QUANTUM_EXPLOSION_DYNAMO = 12,
	// Secret level. Reached via the hidden teleporter inside the
	// Gravitational Damping Hub; engine level slot 13 (currentLevel == 0xD).
	AP_LEVEL_KORATH_III_BASE = 13
} CK5_LevelId;

static const int CK4_MAX_POINTS[] = 
{
	0,
	8600, //border village
	33800, //slug village
	18100, //perilous pit
	81900, //cave of the descendents
	57100, //chasm of chills
	89100, //crystalus
	33100, //hilville
	56900, //sand yego
	54500, //miragia
	21100, //lifewater oasis
	63500, //pyramid of the moons
	33000, //pyramid of shadows
	21800, //pyramid of the gnosticine ancients
	77300, //pyramid of the forbidden
	28800, //isle of tar
	38000, //isle of fire
	20600 //well of wishes
};

static const int CK5_MAX_POINTS[] = 
{
	0,	
	14800, //ion ventilation system
	210600, //security center
	98400, //defense tunnel vlook
	136000, //energy flow systems
	81100, //defense tunnel burrh
	73200, //regulation control center
	104600, //defense tunnel sorra
	103700, //neutrino burst injector
	101200, //defense tunnel teln
	100700, //brownian motion inducer
	158700, //gravitational damping hub
	87300 //quantum explosion dynamo
};

#endif