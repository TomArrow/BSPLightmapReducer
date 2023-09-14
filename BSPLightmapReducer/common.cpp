#include "common.h"
#define _USE_MATH_DEFINES
#include <math.h>
#include <sstream>


// Code is 99%-100% from jomme, from various files.
// Most of it is likely still the same as in the original Jedi Knight source code releases
//



std::string errorInfo = "";

static	int			cmd_argc;
static	char* cmd_argv[MAX_STRING_TOKENS];		// points into cmd_tokenized
static	char		cmd_tokenized[BIG_INFO_STRING + MAX_STRING_TOKENS];	// will have 0 bytes inserted



vec_t VectorLength(const vec3_t v) {
	return (vec_t)sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}
vec_t VectorLength2(const vec2_t v) {
	return (vec_t)sqrtf(v[0] * v[0] + v[1] * v[1]);
}

vec_t VectorNormalize(vec3_t v) {
	float	length, ilength;

	length = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
	length = sqrt(length);

	if (length) {
		ilength = 1 / length;
		v[0] *= ilength;
		v[1] *= ilength;
		v[2] *= ilength;
	}

	return length;
}


int		Cmd_Argc(void) {
	return cmd_argc;
}
const char* Cmd_Argv(int arg) {
	if ((unsigned)arg >= cmd_argc) {
		return "";
	}
	return cmd_argv[arg];
}

void Q_strncpyz(char* dest, int destCapacity, const char* src, int destsize) {
	// bk001129 - also NULL dest
	if (!dest) {
		Com_Error(ERR_FATAL, "Q_strncpyz: NULL dest");
	}
	if (!src) {
		Com_Error(ERR_FATAL, "Q_strncpyz: NULL src");
	}
	if (destsize < 1) {
		Com_Error(ERR_FATAL, "Q_strncpyz: destsize < 1");
	}

	//strncpy(dest, src, destsize - 1);
	strncpy_s(dest, destCapacity, src, destsize - 1);
	dest[destsize - 1] = 0;
}

void QDECL Com_Printf(const char* fmt, ...) {
	va_list		argptr;
	char		msg[MAXPRINTMSG];

	va_start(argptr, fmt);
	//vsprintf(msg, fmt, argptr);
	vsprintf_s(msg, sizeof(msg), fmt, argptr);
	va_end(argptr);

	std::cout << msg;
}

const char* DPrintFLocation = NULL;

// Just using this for error output now. :P :) XD
void QDECL Com_DPrintf(const char* fmt, ...) {
	//#ifdef DEBUG
	va_list		argptr;
	char		msg[MAXPRINTMSG];

	va_start(argptr, fmt);
	//vsprintf(msg, fmt, argptr);
	vsprintf_s(msg, sizeof(msg), fmt, argptr);
	va_end(argptr);

	if (DPrintFLocation) {
		std::cerr << DPrintFLocation << ":" << msg;
	}
	else {
		std::cerr << msg;
	}
	//#endif
}
void QDECL Com_Error(int ignore, const char* fmt, ...) {
	va_list		argptr;
	char		msg[MAXPRINTMSG];

	va_start(argptr, fmt);
	//vsprintf(msg, fmt, argptr);
	vsprintf_s(msg, sizeof(msg), fmt, argptr);
	va_end(argptr);

	if (DPrintFLocation) {
		std::cerr << DPrintFLocation << ":" << msg;
	}
	else {
		std::cerr << msg;
	}
}


void Com_Memcpy(void* dest, const void* src, const size_t count)
{
	memcpy(dest, src, count);
}
void Com_Memset(void* dest, const int val, const size_t count)
{
	memset(dest, val, count);
}



int Q_vsnprintf(char* str, int capacity, size_t size, const char* format, va_list ap) {
	int retval;
	//retval = _vsnprintf(str, size, format, ap);
	retval = _vsnprintf_s(str, capacity, size, format, ap);
	if (retval < 0 || retval == size) {
		// Microsoft doesn't adhere to the C99 standard of vsnprintf,
		// which states that the return value must be the number of
		// bytes written if the output string had sufficient length.
		//
		// Obviously we cannot determine that value from Microsoft's
		// implementation, so we have no choice but to return size.
		str[size - 1] = '\0';
		return size;
	}
	return retval;
}
void QDECL Com_sprintf(char* dest, int size, const char* fmt, ...) {
	int		len;
	va_list		argptr;

	va_start(argptr, fmt);
	len = Q_vsnprintf(dest, size, size, fmt, argptr);
	va_end(argptr);
	if (len >= size) {
		Com_DPrintf("Com_sprintf: overflow of %i in %i\n", len, size);
#ifdef	_DEBUG
		//__asm {
		//	int 3;
		//}
#endif
	}
}

void Cmd_TokenizeString(const char* text_in) {
	const char* text;
	char* textOut;

	// clear previous args
	cmd_argc = 0;

	if (!text_in) {
		return;
	}

	text = text_in;
	textOut = cmd_tokenized;

	while (1) {
		if (cmd_argc == MAX_STRING_TOKENS) {
			return;			// this is usually something malicious
		}

		while (1) {
			// skip whitespace
			while (*text && *text <= ' ') {
				text++;
			}
			if (!*text) {
				return;			// all tokens parsed
			}

			// skip // comments
			if (text[0] == '/' && text[1] == '/') {
				return;			// all tokens parsed
			}

			// skip /* */ comments
			if (text[0] == '/' && text[1] == '*') {
				while (*text && (text[0] != '*' || text[1] != '/')) {
					text++;
				}
				if (!*text) {
					return;		// all tokens parsed
				}
				text += 2;
			}
			else {
				break;			// we are ready to parse a token
			}
		}

		// handle quoted strings
		if (*text == '"') {
			cmd_argv[cmd_argc] = textOut;
			cmd_argc++;
			text++;
			while (*text && *text != '"') {
				/*if (*text == '\\' && text[1] == '"') {
					text++; // Allow double quotes inside double quotes if they are escaped
				}*/ // This causes issues on some demos... sad.
				*textOut++ = *text++;
			}
			*textOut++ = 0;
			if (!*text) {
				return;		// all tokens parsed
			}
			text++;
			continue;
		}

		// regular token
		cmd_argv[cmd_argc] = textOut;
		cmd_argc++;

		// skip until whitespace, quote, or command
		while (*text > ' ') {
			if (text[0] == '"') {
				break;
			}

			if (text[0] == '/' && text[1] == '/') {
				break;
			}

			// skip /* */ comments
			if (text[0] == '/' && text[1] == '*') {
				break;
			}

			/*if (*text == '\\' && text[1] == '"') {
				text++; // Allow double quotes as string content if they are escaped (let's hope this does not interefere with other stuff)
			}*/ // This causes issues on some demos... sad.

			*textOut++ = *text++;
		}

		*textOut++ = 0;

		if (!*text) {
			return;		// all tokens parsed
		}
	}

}

#define VABUFFERSIZE 32000
char* QDECL va(const char* format, ...) {
	va_list		argptr;
	static char		string[10][VABUFFERSIZE];	// in case va is called by nested functions
	static int		index = 0;
	char* buf;

	buf = string[index % 10];
	index++;

	va_start(argptr, format);
	//vsprintf(buf, format, argptr);
	vsprintf_s(buf, VABUFFERSIZE, format, argptr);
	va_end(argptr);

	return buf;
}

void sanitizeFilename(const char* input, char* output, qboolean allowExtension) {

	char* lastDot = NULL;
	const char* inputStart = input;
	while (*input) {
		if (*input == '.' && input != inputStart) { // Even tho we allow extensions (dots), we don't allow the dot at the start of the filename.
			lastDot = output;
		}
		if ((*input == 32) // Don't allow ! exclamation mark. Linux doesn't like that.
			|| (*input >= 34 && *input < 42)
			|| (*input >= 43 && *input < 46)
			|| (*input >= 48 && *input < 58)
			|| (*input >= 59 && *input < 60)
			|| (*input == 61)
			|| (*input >= 64 && *input < 92)
			|| (*input >= 93 && *input < 96) // Don't allow `. Linux doesn't like that either, at least not in shell scripts.
			|| (*input >= 97 && *input < 124)
			|| (*input >= 125 && *input < 127)
			) {
			*output++ = *input;
		}
		else if (*input == '|') {
			*output++ = 'I';
		}
		else {
			*output++ = '-';
		}
		input++;
	}
	*output = 0;

	if (allowExtension && lastDot) {
		*lastDot = '.';
	}
}

/*void sanitizeFilename(std::string input, std::stringstream output) {

	while (*input) {
		if ((*input >= 32 && *input < 47)
			|| (*input >= 48 && *input < 60)
			|| (*input == 61)
			|| (*input >= 64 && *input < 92)
			|| (*input >= 93 && *input < 124)
			|| (*input >= 125 && *input < 127)
			) {
			*output++ = *input;
		}
		else if (*input == '|') {
			*output++ = 'I';
		}
		input++;
	}
	*output = 0;
}*/


/*
===================
Info_RemoveKey
===================
*/
void Info_RemoveKey(char* s, const char* key, bool isMOHAADemo) {
	char* start;
	char	pkey[MAX_INFO_KEY];
	char	value[MAX_INFO_VALUE];
	char* o;

	if (strlen(s) >= (isMOHAADemo ? MAX_INFO_STRING_MAX : MAX_INFO_STRING)) {
		Com_Error(ERR_DROP, "Info_RemoveKey: oversize infostring");
	}

	if (strchr(key, '\\')) {
		return;
	}

	while (1) {
		start = s;
		if (*s == '\\')
			s++;
		o = pkey;
		while (*s != '\\') {
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value;
		while (*s != '\\' && *s) {
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;

		if (!strcmp(key, pkey)) {
			memmove(start, s, strlen(s) + 1); // remove this part
			return;
		}

		if (!*s)
			return;
	}

}

/*
===================
Info_RemoveKey_Big
===================
*/
void Info_RemoveKey_Big(char* s, const char* key) {
	char* start;
	char	pkey[BIG_INFO_KEY];
	char	value[BIG_INFO_VALUE];
	char* o;

	if (strlen(s) >= BIG_INFO_STRING) {
		Com_Error(ERR_DROP, "Info_RemoveKey_Big: oversize infostring");
	}

	if (strchr(key, '\\')) {
		return;
	}

	while (1) {
		start = s;
		if (*s == '\\')
			s++;
		o = pkey;
		while (*s != '\\') {
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value;
		while (*s != '\\' && *s) {
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;

		if (!strcmp(key, pkey)) {
			memmove(start, s, strlen(s) + 1); // remove this part
			return;
		}

		if (!*s)
			return;
	}

}


/*
===============
Info_ValueForKey

Searches the string for the given
key and returns the associated value, or an empty string.
FIXME: overflow check?
===============
*/
const char* Info_ValueForKey(const char* s, int maxLength, const char* key) {
	char	pkey[BIG_INFO_KEY];
	static	char value[2][BIG_INFO_VALUE];	// use two buffers so compares
											// work without stomping on each other
	static	int	valueindex = 0;
	char* o;

	if (!s || !key) {
		return "";
	}

	//if (strlen(s) >= BIG_INFO_STRING) {
	if (strnlen_s(s, maxLength) >= BIG_INFO_STRING) {
		Com_Error(ERR_DROP, "Info_ValueForKey: oversize infostring");
	}

	valueindex ^= 1;
	if (*s == '\\')
		s++;
	while (1) {
		o = pkey;
		while (*s != '\\') {
			if (!*s)
				return "";
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value[valueindex];

		while (*s != '\\' && *s) {
			*o++ = *s++;
		}
		*o = 0;

		if (!_stricmp(key, pkey))
			return value[valueindex];

		if (!*s)
			break;
		s++;
	}

	return "";
}


/*
==================
Info_SetValueForKey

Changes or adds a key/value pair
==================
*/
qboolean Info_SetValueForKey(char* s, int capacity, const char* key, const char* value, bool isMOHAADemo) {
	char	newi[MAX_INFO_STRING_MAX];

	if (strlen(s) >= (isMOHAADemo ? MAX_INFO_STRING_MAX : MAX_INFO_STRING)) {
		Com_Error(ERR_DROP, "Info_SetValueForKey: oversize infostring");
	}

	if (strchr(key, '\\') || strchr(value, '\\')) {
		Com_DPrintf("Can't use keys or values with a \\\n");
		return qfalse;
	}

	if (strchr(key, ';') || strchr(value, ';')) {
		Com_DPrintf("Can't use keys or values with a semicolon\n");
		return qfalse;
	}

	if (strchr(key, '\"') || strchr(value, '\"')) {
		Com_DPrintf("Can't use keys or values with a \"\n");
		return qfalse;
	}

	Info_RemoveKey(s, key, isMOHAADemo);

	if (!strlen(value))
		return qfalse;

	Com_sprintf(newi, sizeof(newi), "\\%s\\%s", key, value);

	// q3infoboom exploit
	if (strlen(newi) + strlen(s) >= (isMOHAADemo ? MAX_INFO_STRING_MAX : MAX_INFO_STRING)) {
		Com_DPrintf("Info string length exceeded\n");
		return qfalse;
	}

	//strcat(newi, s);
	strcat_s(newi, sizeof(newi), s);
	//strcpy(s, newi);
	strcpy_s(s, capacity, newi);
	return qtrue;
}

/*
==================
Info_SetValueForKey_Big

Changes or adds a key/value pair
==================
*/
void Info_SetValueForKey_Big(char* s, int capacity, const char* key, const char* value) {
	char	newi[BIG_INFO_STRING];

	if (strlen(s) >= BIG_INFO_STRING) {
		Com_Error(ERR_DROP, "Info_SetValueForKey: oversize infostring");
	}

	if (strchr(key, '\\') || strchr(value, '\\')) {
		Com_DPrintf("Can't use keys or values with a \\\n");
		return;
	}

	if (strchr(key, ';') || strchr(value, ';')) {
		Com_DPrintf("Can't use keys or values with a semicolon\n");
		return;
	}

	if (strchr(key, '\"') || strchr(value, '\"')) {
		Com_DPrintf("Can't use keys or values with a \"\n");
		return;
	}

	Info_RemoveKey_Big(s, key);

	if (!strlen(value))
		return;

	Com_sprintf(newi, sizeof(newi), "\\%s\\%s", key, value);

	// q3infoboom exploit
	if (strlen(newi) + strlen(s) >= BIG_INFO_STRING) {
		Com_DPrintf("BIG Info string length exceeded\n");
		return;
	}

	//strcat(s, newi);
	strcat_s(s, capacity, newi);
}




// MOD-weapon mapping array.
int weaponFromMOD_JK2[MOD_MAX_JK2] =
{
	WP_NONE_JK2,				//MOD_UNKNOWN_JK2,
	WP_STUN_BATON_JK2,			//MOD_STUN_BATON_JK2,
	WP_NONE_JK2,				//MOD_MELEE_JK2,
	WP_SABER_JK2,				//MOD_SABER_JK2,
	WP_BRYAR_PISTOL_JK2,		//MOD_BRYAR_PISTOL_JK2,
	WP_BRYAR_PISTOL_JK2,		//MOD_BRYAR_PISTOL_ALT_JK2,
	WP_BLASTER_JK2,				//MOD_BLASTER_JK2,
	WP_DISRUPTOR_JK2,			//MOD_DISRUPTOR_JK2,
	WP_DISRUPTOR_JK2,			//MOD_DISRUPTOR_SPLASH_JK2,
	WP_DISRUPTOR_JK2,			//MOD_DISRUPTOR_SNIPER_JK2,
	WP_BOWCASTER_JK2,			//MOD_BOWCASTER_JK2,
	WP_REPEATER_JK2,			//MOD_REPEATER_JK2,
	WP_REPEATER_JK2,			//MOD_REPEATER_ALT_JK2,
	WP_REPEATER_JK2,			//MOD_REPEATER_ALT_SPLASH_JK2,
	WP_DEMP2_JK2,				//MOD_DEMP2_JK2,
	WP_DEMP2_JK2,				//MOD_DEMP2_ALT_JK2,
	WP_FLECHETTE_JK2,			//MOD_FLECHETTE_JK2,
	WP_FLECHETTE_JK2,			//MOD_FLECHETTE_ALT_SPLASH_JK2,
	WP_ROCKET_LAUNCHER_JK2,		//MOD_ROCKET_JK2,
	WP_ROCKET_LAUNCHER_JK2,		//MOD_ROCKET_SPLASH_JK2,
	WP_ROCKET_LAUNCHER_JK2,		//MOD_ROCKET_HOMING_JK2,
	WP_ROCKET_LAUNCHER_JK2,		//MOD_ROCKET_HOMING_SPLASH_JK2,
	WP_THERMAL_JK2,				//MOD_THERMAL_JK2,
	WP_THERMAL_JK2,				//MOD_THERMAL_SPLASH_JK2,
	WP_TRIP_MINE_JK2,			//MOD_TRIP_MINE_SPLASH_JK2,
	WP_TRIP_MINE_JK2,			//MOD_TIMED_MINE_SPLASH_JK2,
	WP_DET_PACK_JK2,			//MOD_DET_PACK_SPLASH_JK2,
	WP_NONE_JK2,				//MOD_FORCE_DARK_JK2,
	WP_NONE_JK2,				//MOD_SENTRY_JK2,
	WP_NONE_JK2,				//MOD_WATER_JK2,
	WP_NONE_JK2,				//MOD_SLIME_JK2,
	WP_NONE_JK2,				//MOD_LAVA_JK2,
	WP_NONE_JK2,				//MOD_CRUSH_JK2,
	WP_NONE_JK2,				//MOD_TELEFRAG_JK2,
	WP_NONE_JK2,				//MOD_FALLING_JK2,
	WP_NONE_JK2,				//MOD_SUICIDE_JK2,
	WP_NONE_JK2,				//MOD_TARGET_LASER_JK2,
	WP_NONE_JK2,				//MOD_TRIGGER_HURT_JK2,
};

// MOD-weapon mapping array.
int weaponFromMOD_GENERAL[MOD_MAX_GENERAL] =
{
	WP_NONE_GENERAL,				//MOD_UNKNOWN_GENERAL,
	WP_STUN_BATON_GENERAL,			//MOD_STUN_BATON_GENERAL,
	WP_NONE_GENERAL,				//MOD_MELEE_GENERAL,
	WP_SABER_GENERAL,				//MOD_SABER_GENERAL,
	WP_BRYAR_PISTOL_GENERAL,		//MOD_BRYAR_PISTOL_GENERAL,
	WP_BRYAR_PISTOL_GENERAL,		//MOD_BRYAR_PISTOL_ALT_GENERAL,
	WP_BLASTER_GENERAL,				//MOD_BLASTER_GENERAL,
	WP_DISRUPTOR_GENERAL,			//MOD_DISRUPTOR_GENERAL,
	WP_DISRUPTOR_GENERAL,			//MOD_DISRUPTOR_SPLASH_GENERAL,
	WP_DISRUPTOR_GENERAL,			//MOD_DISRUPTOR_SNIPER_GENERAL,
	WP_BOWCASTER_GENERAL,			//MOD_BOWCASTER_GENERAL,
	WP_REPEATER_GENERAL,			//MOD_REPEATER_GENERAL,
	WP_REPEATER_GENERAL,			//MOD_REPEATER_ALT_GENERAL,
	WP_REPEATER_GENERAL,			//MOD_REPEATER_ALT_SPLASH_GENERAL,
	WP_DEMP2_GENERAL,				//MOD_DEMP2_GENERAL,
	WP_DEMP2_GENERAL,				//MOD_DEMP2_ALT_GENERAL,
	WP_FLECHETTE_GENERAL,			//MOD_FLECHETTE_GENERAL,
	WP_FLECHETTE_GENERAL,			//MOD_FLECHETTE_ALT_SPLASH_GENERAL,
	WP_ROCKET_LAUNCHER_GENERAL,		//MOD_ROCKET_GENERAL,
	WP_ROCKET_LAUNCHER_GENERAL,		//MOD_ROCKET_SPLASH_GENERAL,
	WP_ROCKET_LAUNCHER_GENERAL,		//MOD_ROCKET_HOMING_GENERAL,
	WP_ROCKET_LAUNCHER_GENERAL,		//MOD_ROCKET_HOMING_SPLASH_GENERAL,
	WP_THERMAL_GENERAL,				//MOD_THERMAL_GENERAL,
	WP_THERMAL_GENERAL,				//MOD_THERMAL_SPLASH_GENERAL,
	WP_TRIP_MINE_GENERAL,			//MOD_TRIP_MINE_SPLASH_GENERAL,
	WP_TRIP_MINE_GENERAL,			//MOD_TIMED_MINE_SPLASH_GENERAL,
	WP_DET_PACK_GENERAL,			//MOD_DET_PACK_SPLASH_GENERAL,
	WP_NONE_GENERAL,				//MOD_FORCE_DARK_GENERAL,
	WP_NONE_GENERAL,				//MOD_SENTRY_GENERAL,
	WP_NONE_GENERAL,				//MOD_WATER_GENERAL,
	WP_NONE_GENERAL,				//MOD_SLIME_GENERAL,
	WP_NONE_GENERAL,				//MOD_LAVA_GENERAL,
	WP_NONE_GENERAL,				//MOD_CRUSH_GENERAL,
	WP_NONE_GENERAL,				//MOD_TELEFRAG_GENERAL,
	WP_NONE_GENERAL,				//MOD_FALLING_GENERAL,
	WP_NONE_GENERAL,				//MOD_SUICIDE_GENERAL,
	WP_NONE_GENERAL,				//MOD_TARGET_LASER_GENERAL,
	WP_NONE_GENERAL,				//MOD_TRIGGER_HURT_GENERAL,
	// JK3
	WP_NONE_GENERAL,//MOD_TURBLAST_GENERAL,
	WP_NONE_GENERAL,//MOD_VEHICLE_GENERAL,
	WP_CONCUSSION_GENERAL,//MOD_CONC_GENERAL,
	WP_CONCUSSION_GENERAL,//MOD_CONC_ALT_GENERAL,
	WP_NONE_GENERAL,//MOD_COLLISION_GENERAL,
	WP_NONE_GENERAL,//MOD_VEH_EXPLOSION_GENERAL,
	WP_NONE_GENERAL,//MOD_TEAM_CHANGE_GENERAL,

	//q3
	WP_SHOTGUN_GENERAL,//MOD_SHOTGUN_GENERAL,
	WP_GAUNTLET_GENERAL,//MOD_GAUNTLET_GENERAL,
	WP_MACHINEGUN_GENERAL,//MOD_MACHINEGUN_GENERAL,
	WP_GRENADE_LAUNCHER_GENERAL,//MOD_GRENADE_GENERAL,
	WP_GRENADE_LAUNCHER_GENERAL,//MOD_GRENADE_SPLASH_GENERAL,
	WP_PLASMAGUN_GENERAL,//MOD_PLASMA_GENERAL,
	WP_PLASMAGUN_GENERAL,//MOD_PLASMA_SPLASH_GENERAL,
	WP_RAILGUN_GENERAL,//MOD_RAILGUN_GENERAL,
	WP_LIGHTNING_GENERAL,//MOD_LIGHTNING_GENERAL,
	WP_BFG_GENERAL,//MOD_BFG_GENERAL,
	WP_BFG_GENERAL,//MOD_BFG_SPLASH_GENERAL,
	// 
	//#ifdef MISSIONPACK
	WP_NAILGUN_GENERAL,//MOD_NAIL_GENERAL,
	WP_CHAINGUN_GENERAL,//MOD_CHAINGUN_GENERAL,
	WP_PROX_LAUNCHER_GENERAL, //MOD_PROXIMITY_MINE_GENERAL,
	WP_NONE_GENERAL,//MOD_KAMIKAZE_GENERAL,
	WP_NONE_GENERAL,//MOD_JUICED_GENERAL,
	//#endif

	WP_GRAPPLING_HOOK_GENERAL,//MOD_GRAPPLE_GENERAL,
};

typedef enum {
	Q_BR,
	Q_R,
	Q_TR,
	Q_T,
	Q_TL,
	Q_L,
	Q_BL,
	Q_B,
	Q_NUM_QUADS
} saberQuadrant_t;

typedef enum {
	BLK_NO,
	BLK_TIGHT,		// Block only attacks and shots around the saber itself, a bbox of around 12x12x12
	BLK_WIDE		// Block all attacks in an area around the player in a rough arc of 180 degrees
} saberBlockType_t;

#define SETANIM_TORSO 1
#define SETANIM_LEGS  2
#define SETANIM_BOTH  SETANIM_TORSO|SETANIM_LEGS//3

#define SETANIM_FLAG_NORMAL		0//Only set if timer is 0
#define SETANIM_FLAG_OVERRIDE	1//Override previous
#define SETANIM_FLAG_HOLD		2//Set the new timer
#define SETANIM_FLAG_RESTART	4//Allow restarting the anim if playing the same one (weapon fires)
#define SETANIM_FLAG_HOLDLESS	8//Set the new timer

// Silly, but I'm replacing these macros so they are shorter!
#define AFLAG_IDLE	(SETANIM_FLAG_NORMAL)
#define AFLAG_ACTIVE (/*SETANIM_FLAG_OVERRIDE | */SETANIM_FLAG_HOLD | SETANIM_FLAG_HOLDLESS)
#define AFLAG_WAIT (SETANIM_FLAG_HOLD | SETANIM_FLAG_HOLDLESS)
#define AFLAG_FINISH (SETANIM_FLAG_HOLD)


std::map <int, std::string>  saberMoveNames_general
{
	// Invalid, or saber not armed
  std::make_pair(LS_INVALID_GENERAL, "_INVALID"),
  std::make_pair(LS_NONE_GENERAL, "_WEIRD"),

  // General movements with saber
  std::make_pair(LS_READY_GENERAL, "_IDLE"),
  std::make_pair(LS_DRAW_GENERAL, "_DRAW"),
  std::make_pair(LS_PUTAWAY_GENERAL, "_PUTAWAY"),

  // Attacks
  std::make_pair(LS_A_TL2BR_GENERAL, ""),//4
  std::make_pair(LS_A_L2R_GENERAL, ""),
  std::make_pair(LS_A_BL2TR_GENERAL, ""),
  std::make_pair(LS_A_BR2TL_GENERAL, ""),
  std::make_pair(LS_A_R2L_GENERAL, ""),
  std::make_pair(LS_A_TR2BL_GENERAL, ""),
  std::make_pair(LS_A_T2B_GENERAL, ""),
  std::make_pair(LS_A_BACKSTAB_GENERAL, "_BLUBS"),
  std::make_pair(LS_A_BACK_GENERAL, "_BS"),
  std::make_pair(LS_A_BACK_CR_GENERAL, "_DBS"),

  std::make_pair(LS_ROLL_STAB_GENERAL, "_ROLLSTAB"), // JKA

  std::make_pair(LS_A_LUNGE_GENERAL, "_UPCUT"),
  std::make_pair(LS_A_JUMP_T__B__GENERAL, "_DFA"),
  std::make_pair(LS_A_FLIP_STAB_GENERAL, "_YDFA"),
  std::make_pair(LS_A_FLIP_SLASH_GENERAL, "_YDFA"),

  // JKA
	std::make_pair(LS_JUMPATTACK_DUAL_GENERAL,"_BUTTERFLYDUAL"), // Flip forward attack
	std::make_pair(LS_JUMPATTACK_ARIAL_LEFT_GENERAL,"_CARTWHEEL"),
	std::make_pair(LS_JUMPATTACK_ARIAL_RIGHT_GENERAL,"_CARTWHEEL"),
	std::make_pair(LS_JUMPATTACK_CART_LEFT_GENERAL,"_CARTWHEEL"),
	std::make_pair(LS_JUMPATTACK_CART_RIGHT_GENERAL,"_CARTWHEEL"),
	std::make_pair(LS_JUMPATTACK_STAFF_LEFT_GENERAL,"_BUTTERFLYSTAFF"), // Official butterfly but sabermoveData calls it dual jump attack staff(?!)
	std::make_pair(LS_JUMPATTACK_STAFF_RIGHT_GENERAL,"_BUTTERFLYSTAFF"),
	std::make_pair(LS_BUTTERFLY_LEFT_GENERAL,"_BUTTERFLYSTAFF2"), // Not the official butterfly but actually named butterfly.. wtf
	std::make_pair(LS_BUTTERFLY_RIGHT_GENERAL,"_BUTTERFLYSTAFF2"),
	std::make_pair(LS_A_BACKFLIP_ATK_GENERAL,"_BFATK"),
	std::make_pair(LS_SPINATTACK_DUAL_GENERAL,"_KATADUAL"), // Dual spin attack
	std::make_pair(LS_SPINATTACK_GENERAL,"_KATASTAFF2"), // Saber staff twirl
	std::make_pair(LS_LEAP_ATTACK_GENERAL,"_LONGLEAP"), // idk wtf this is
	std::make_pair(LS_SWOOP_ATTACK_RIGHT_GENERAL,"_SWOOP"), // Idk if this is an actual attack. The animation is a guy sitting and swooping.. ?!?
	std::make_pair(LS_SWOOP_ATTACK_LEFT_GENERAL,"_SWOOP"),	// Ooh. It might be if sitting on an animal or in a vehicle otherwise? Oh are they called "swoop bikes" ?
	std::make_pair(LS_TAUNTAUN_ATTACK_RIGHT_GENERAL,"_TAUNTAUN"), // thes are also sitting... hmm. sitting on a tauntaun? 
	std::make_pair(LS_TAUNTAUN_ATTACK_LEFT_GENERAL,"_TAUNTAUN"),
	std::make_pair(LS_KICK_F_GENERAL,"_KICKFRONT"),
	std::make_pair(LS_KICK_B_GENERAL,"_KICKBACK"),
	std::make_pair(LS_KICK_R_GENERAL,"_KICKSIDE"), // what difference does it make...
	std::make_pair(LS_KICK_L_GENERAL,"_KICKSIDE"),
	std::make_pair(LS_KICK_S_GENERAL,"_KICKSPIN"), // I havent investigated this too deeply. Idk how to do it
	std::make_pair(LS_KICK_BF_GENERAL,"_KICKFRONTBACK"),
	std::make_pair(LS_KICK_RL_GENERAL,"_KICKBOTHSIDES"),
	std::make_pair(LS_KICK_F_AIR_GENERAL,"_KICKFRONTAIR"),
	std::make_pair(LS_KICK_B_AIR_GENERAL,"_KICKBACKAIR"),
	std::make_pair(LS_KICK_R_AIR_GENERAL,"_KICKSIDEAIR"),
	std::make_pair(LS_KICK_L_AIR_GENERAL,"_KICKSIDEAIR"),
	std::make_pair(LS_STABDOWN_GENERAL,"_STABGROUND"),
	std::make_pair(LS_STABDOWN_STAFF_GENERAL,"_STABGROUNDSTAFF"),
	std::make_pair(LS_STABDOWN_DUAL_GENERAL,"_STABGROUNDDUAL"),
	std::make_pair(LS_DUAL_SPIN_PROTECT_GENERAL,"_DUALBARRIER"),	// Dual saber barrier (spinning sabers)
	std::make_pair(LS_STAFF_SOULCAL_GENERAL,"_KATASTAFF"),
	std::make_pair(LS_A1_SPECIAL_GENERAL,"_KATABLUE"), // Fast attack kata
	std::make_pair(LS_A2_SPECIAL_GENERAL,"_KATAYEL"),
	std::make_pair(LS_A3_SPECIAL_GENERAL,"_KATARED"),
	std::make_pair(LS_UPSIDE_DOWN_ATTACK_GENERAL,"_FLIPATK"), // Can't find info on this. Animation looks like a vampire hanging upside down and wiggling a saber downwards
	std::make_pair(LS_PULL_ATTACK_STAB_GENERAL,"_PULLSTAB"),	// Can't find info on this either. 
	std::make_pair(LS_PULL_ATTACK_SWING_GENERAL,"_PULLSWING"),	// Some kind of animation that pulls someone in and stabs? Idk if its actually usable or how...
	std::make_pair(LS_SPINATTACK_ALORA_GENERAL,"_ALORA"), // "Alora Spin slash"? No info on it either idk. Might just all be single player stuff
	std::make_pair(LS_DUAL_FB_GENERAL,"_DUALSTABFB"), // Dual stab front back
	std::make_pair(LS_DUAL_LR_GENERAL,"_DUALSTABLR"), // dual stab left right
	std::make_pair(LS_HILT_BASH_GENERAL,"_HILTBASH"), // Staff handle bashed into face (like darth maul i guess?)
	// JKA end

  //starts
  std::make_pair(LS_S_TL2BR_GENERAL, ""),//26
  std::make_pair(LS_S_L2R_GENERAL, ""),
  std::make_pair(LS_S_BL2TR_GENERAL, ""),//# Start of attack chaining to SLASH LR2UL
  std::make_pair(LS_S_BR2TL_GENERAL, ""),//# Start of attack chaining to SLASH LR2UL
  std::make_pair(LS_S_R2L_GENERAL, ""),
  std::make_pair(LS_S_TR2BL_GENERAL, ""),
  std::make_pair(LS_S_T2B_GENERAL, ""),

  //returns
  std::make_pair(LS_R_TL2BR_GENERAL, ""),//33
  std::make_pair(LS_R_L2R_GENERAL, ""),
  std::make_pair(LS_R_BL2TR_GENERAL, ""),
  std::make_pair(LS_R_BR2TL_GENERAL, ""),
  std::make_pair(LS_R_R2L_GENERAL, ""),
  std::make_pair(LS_R_TR2BL_GENERAL, ""),
  std::make_pair(LS_R_T2B_GENERAL, ""),

  //transitions
  std::make_pair(LS_T1_BR__R_GENERAL, ""),//40
  std::make_pair(LS_T1_BR_TR_GENERAL, ""),
  std::make_pair(LS_T1_BR_T__GENERAL, ""),
  std::make_pair(LS_T1_BR_TL_GENERAL, ""),
  std::make_pair(LS_T1_BR__L_GENERAL, ""),
  std::make_pair(LS_T1_BR_BL_GENERAL, ""),
  std::make_pair(LS_T1__R_BR_GENERAL, ""),//46
  std::make_pair(LS_T1__R_TR_GENERAL, ""),
  std::make_pair(LS_T1__R_T__GENERAL, ""),
  std::make_pair(LS_T1__R_TL_GENERAL, ""),
  std::make_pair(LS_T1__R__L_GENERAL, ""),
  std::make_pair(LS_T1__R_BL_GENERAL, ""),
  std::make_pair(LS_T1_TR_BR_GENERAL, ""),//52
  std::make_pair(LS_T1_TR__R_GENERAL, ""),
  std::make_pair(LS_T1_TR_T__GENERAL, ""),
  std::make_pair(LS_T1_TR_TL_GENERAL, ""),
  std::make_pair(LS_T1_TR__L_GENERAL, ""),
  std::make_pair(LS_T1_TR_BL_GENERAL, ""),
  std::make_pair(LS_T1_T__BR_GENERAL, ""),//58
  std::make_pair(LS_T1_T___R_GENERAL, ""),
  std::make_pair(LS_T1_T__TR_GENERAL, ""),
  std::make_pair(LS_T1_T__TL_GENERAL, ""),
  std::make_pair(LS_T1_T___L_GENERAL, ""),
  std::make_pair(LS_T1_T__BL_GENERAL, ""),
  std::make_pair(LS_T1_TL_BR_GENERAL, ""),//64
  std::make_pair(LS_T1_TL__R_GENERAL, ""),
  std::make_pair(LS_T1_TL_TR_GENERAL, ""),
  std::make_pair(LS_T1_TL_T__GENERAL, ""),
  std::make_pair(LS_T1_TL__L_GENERAL, ""),
  std::make_pair(LS_T1_TL_BL_GENERAL, ""),
  std::make_pair(LS_T1__L_BR_GENERAL, ""),//70
  std::make_pair(LS_T1__L__R_GENERAL, ""),
  std::make_pair(LS_T1__L_TR_GENERAL, ""),
  std::make_pair(LS_T1__L_T__GENERAL, ""),
  std::make_pair(LS_T1__L_TL_GENERAL, ""),
  std::make_pair(LS_T1__L_BL_GENERAL, ""),
  std::make_pair(LS_T1_BL_BR_GENERAL, ""),//76
  std::make_pair(LS_T1_BL__R_GENERAL, ""),
  std::make_pair(LS_T1_BL_TR_GENERAL, ""),
  std::make_pair(LS_T1_BL_T__GENERAL, ""),
  std::make_pair(LS_T1_BL_TL_GENERAL, ""),
  std::make_pair(LS_T1_BL__L_GENERAL, ""),

  //Bounces
  std::make_pair(LS_B1_BR_GENERAL, "_BOUNCE"),
  std::make_pair(LS_B1__R_GENERAL, "_BOUNCE"),
  std::make_pair(LS_B1_TR_GENERAL, "_BOUNCE"),
  std::make_pair(LS_B1_T__GENERAL, "_BOUNCE"),
  std::make_pair(LS_B1_TL_GENERAL, "_BOUNCE"),
  std::make_pair(LS_B1__L_GENERAL, "_BOUNCE"),
  std::make_pair(LS_B1_BL_GENERAL, "_BOUNCE"),

  //Deflected attacks
  std::make_pair(LS_D1_BR_GENERAL, "_DEFLECT"),
  std::make_pair(LS_D1__R_GENERAL, "_DEFLECT"),
  std::make_pair(LS_D1_TR_GENERAL, "_DEFLECT"),
  std::make_pair(LS_D1_T__GENERAL, "_DEFLECT"),
  std::make_pair(LS_D1_TL_GENERAL, "_DEFLECT"),
  std::make_pair(LS_D1__L_GENERAL, "_DEFLECT"),
  std::make_pair(LS_D1_BL_GENERAL, "_DEFLECT"),
  std::make_pair(LS_D1_B__GENERAL, "_DEFLECT"),

  //Reflected attacks
  std::make_pair(LS_V1_BR_GENERAL, "_REFLECT"),
  std::make_pair(LS_V1__R_GENERAL, "_REFLECT"),
  std::make_pair(LS_V1_TR_GENERAL, "_REFLECT"),
  std::make_pair(LS_V1_T__GENERAL, "_REFLECT"),
  std::make_pair(LS_V1_TL_GENERAL, "_REFLECT"),
  std::make_pair(LS_V1__L_GENERAL, "_REFLECT"),
  std::make_pair(LS_V1_BL_GENERAL, "_REFLECT"),
  std::make_pair(LS_V1_B__GENERAL, "_REFLECT"),

  // Broken parries
  std::make_pair(LS_H1_T__GENERAL, "_BPARRY"),//
  std::make_pair(LS_H1_TR_GENERAL, "_BPARRY"),
  std::make_pair(LS_H1_TL_GENERAL, "_BPARRY"),
  std::make_pair(LS_H1_BR_GENERAL, "_BPARRY"),
  std::make_pair(LS_H1_B__GENERAL, "_BPARRY"),
  std::make_pair(LS_H1_BL_GENERAL, "_BPARRY"),

  // Knockaways
  std::make_pair(LS_K1_T__GENERAL, "_KNOCKAWAY"),//
  std::make_pair(LS_K1_TR_GENERAL, "_KNOCKAWAY"),
  std::make_pair(LS_K1_TL_GENERAL, "_KNOCKAWAY"),
  std::make_pair(LS_K1_BR_GENERAL, "_KNOCKAWAY"),
  std::make_pair(LS_K1_BL_GENERAL, "_KNOCKAWAY"),

  // Parries
  std::make_pair(LS_PARRY_UP_GENERAL, "_PARRY"),//
  std::make_pair(LS_PARRY_UR_GENERAL, "_PARRY"),
  std::make_pair(LS_PARRY_UL_GENERAL, "_PARRY"),
  std::make_pair(LS_PARRY_LR_GENERAL, "_PARRY"),
  std::make_pair(LS_PARRY_LL_GENERAL, "_PARRY"),

  // Projectile Reflections
  std::make_pair(LS_REFLECT_UP_GENERAL, "_PREFLECT"),//
  std::make_pair(LS_REFLECT_UR_GENERAL, "_PREFLECT"),
  std::make_pair(LS_REFLECT_UL_GENERAL, "_PREFLECT"),
  std::make_pair(LS_REFLECT_LR_GENERAL, "_PREFLECT"),
  std::make_pair(LS_REFLECT_LL_GENERAL, "_PREFLECT"),

  //std::make_pair(LS_MOVE_MAX, "")//
};

std::map <int, std::string>  saberStyleNames
{
	std::make_pair(FORCE_LEVEL_0, "_UNKST"),
	std::make_pair(FORCE_LEVEL_1, "_BLU"),
	std::make_pair(FORCE_LEVEL_2, "_YEL"),
	std::make_pair(FORCE_LEVEL_3, "_RED"),

	// New in jka: but the values of the former ones have stayed the same so we dont have to worry its ok.
	std::make_pair(SS_DESANN, "_DESANN"), // Is there a better name?
	std::make_pair(SS_TAVION, "_TAVION"), // Is there a better name?
	std::make_pair(SS_DUAL, "_DUAL"),
	std::make_pair(SS_STAFF, "_STAFF"),
	std::make_pair(SS_NUM_SABER_STYLES, ""),
	//std::make_pair(NUM_FORCE_POWER_LEVELS, "")
};



/*
===============
LerpAngle

===============
*/
float LerpAngle(float from, float to, float frac) {
	float	a;

	if (to - from > 180) {
		to -= 360;
	}
	if (to - from < -180) {
		to += 360;
	}
	a = from + frac * (to - from);

	return a;
}









std::vector<std::string> splitString(std::string input, std::string separator, bool trim, bool allowEmpty) {
	std::vector<std::string> retVal;
	int position = 0;
	size_t foundPos = 0;
	size_t lastFoundPos = -1;

	while (std::string::npos != (foundPos = input.find(separator, lastFoundPos + 1))) {
		std::string newString = input.substr(lastFoundPos + 1, foundPos - lastFoundPos - 1);
		if (trim) {
			size_t tmpLocation = newString.find_last_not_of(" ");
			if (tmpLocation != std::string::npos && tmpLocation < (newString.size() - 1)) {
				newString.erase(tmpLocation + 1);
			}
			newString.erase(0, (std::max)((int)newString.find_first_not_of(" "), 0));
		}
		if (allowEmpty || newString.size() > 0)
			retVal.push_back(newString);
		lastFoundPos = foundPos;
	}
	if (lastFoundPos != input.size() - 1) {
		std::string newString = input.substr(lastFoundPos + 1, input.size() - lastFoundPos - 1);
		if (trim) {
			size_t tmpLocation = newString.find_last_not_of(" ");
			if (tmpLocation != std::string::npos && tmpLocation < (newString.size() - 1)) {
				newString.erase(tmpLocation + 1);
			}
			newString.erase(0, (std::max)((int)newString.find_first_not_of(" "), 0));
		}
		if (allowEmpty || newString.size() > 0)
			retVal.push_back(newString);
	}
	return retVal;
}


char tolowerSignSafe(char in) {
	return (char)tolower((unsigned char)in);
}




/*
================
BG_EvaluateTrajectory

================
*/
void BG_EvaluateTrajectory(const trajectory_t* tr, int atTime, vec3_t result) {
	float		deltaTime;
	float		phase;

	switch (tr->trType) {
	case TR_STATIONARY:
	case TR_INTERPOLATE:
		VectorCopy(tr->trBase, result);
		break;
	case TR_LINEAR:
		deltaTime = (atTime - tr->trTime) * 0.001;	// milliseconds to seconds
		VectorMA(tr->trBase, deltaTime, tr->trDelta, result);
		break;
	case TR_SINE:
		deltaTime = (atTime - tr->trTime) / (float)tr->trDuration;
		phase = sin(deltaTime * M_PI * 2);
		VectorMA(tr->trBase, phase, tr->trDelta, result);
		break;
	case TR_LINEAR_STOP:
		if (atTime > tr->trTime + tr->trDuration) {
			atTime = tr->trTime + tr->trDuration;
		}
		deltaTime = (atTime - tr->trTime) * 0.001;	// milliseconds to seconds
		if (deltaTime < 0) {
			deltaTime = 0;
		}
		VectorMA(tr->trBase, deltaTime, tr->trDelta, result);
		break;
	case TR_GRAVITY:
		deltaTime = (atTime - tr->trTime) * 0.001;	// milliseconds to seconds
		VectorMA(tr->trBase, deltaTime, tr->trDelta, result);
		result[2] -= 0.5 * DEFAULT_GRAVITY * deltaTime * deltaTime;		// FIXME: local gravity...
		break;
	default:
#ifdef QAGAME
		Com_Error(ERR_DROP, "BG_EvaluateTrajectory: [GAME SIDE] unknown trType: %i", tr->trType);
#else
		Com_Error(ERR_DROP, "BG_EvaluateTrajectory: [CLIENTGAME SIDE] unknown trType: %i", tr->trType);
#endif
		break;
	}
}

/*
================
BG_EvaluateTrajectoryDelta

For determining velocity at a given time
================
*/
void BG_EvaluateTrajectoryDelta(const trajectory_t* tr, int atTime, vec3_t result) {
	float	deltaTime;
	float	phase;

	switch (tr->trType) {
	case TR_STATIONARY:
	case TR_INTERPOLATE:
		VectorClear(result);
		break;
	case TR_LINEAR:
		VectorCopy(tr->trDelta, result);
		break;
	case TR_SINE:
		deltaTime = (atTime - tr->trTime) / (float)tr->trDuration;
		phase = cos(deltaTime * M_PI * 2);	// derivative of sin = cos
		phase *= 0.5;
		VectorScale(tr->trDelta, phase, result);
		break;
	case TR_LINEAR_STOP:
		if (atTime > tr->trTime + tr->trDuration) {
			VectorClear(result);
			return;
		}
		VectorCopy(tr->trDelta, result);
		break;
	case TR_GRAVITY:
		deltaTime = (atTime - tr->trTime) * 0.001;	// milliseconds to seconds
		VectorCopy(tr->trDelta, result);
		result[2] -= DEFAULT_GRAVITY * deltaTime;		// FIXME: local gravity...
		break;
	default:
		Com_Error(ERR_DROP, "BG_EvaluateTrajectoryDelta: unknown trType: %i", tr->trTime);
		break;
	}
}



float VectorDistance(vec3_t v1, vec3_t v2)
{
	vec3_t dir;

	VectorSubtract(v2, v1, dir);
	return VectorLength(dir);
}



#define Q_COLOR_ESCAPE	'^'
// you MUST have the last bit on here about colour strings being less than 7 or taiwanese strings register as colour!!!!
//#define Q_IsColorString(p)	( p && *(p) == Q_COLOR_ESCAPE && *((p)+1) && *((p)+1) != Q_COLOR_ESCAPE && *((p)+1) <= '7' && *((p)+1) >= '0' )
// Correct version of the above for Q_StripColor
#define Q_IsColorStringExt(p)	((p) && *(p) == Q_COLOR_ESCAPE && *((p)+1) && isdigit(*((p)+1))) // ^[0-9]

// from eternaljk2mv:
#define Q_IsColorString(p)	( p && *(p) == Q_COLOR_ESCAPE && *((p)+1) <= '7' && *((p)+1) >= '0' )
#define Q_IsColorString_1_02(p)	( p && *(p) == Q_COLOR_ESCAPE && *((p)+1) && *((p)+1) != Q_COLOR_ESCAPE ) // 1.02 ColorStrings
#define Q_IsColorString_Extended(p) Q_IsColorString_1_02(p)

#define Q_IsColorStringNT(p)	( p && *(p) == Q_COLOR_ESCAPE && *((p)+1) && *((p)+1) != Q_COLOR_ESCAPE && *((p)+1) <= 0x7F && *((p)+1) >= 0x00 )
#define ColorIndexNT(c)			( (c) & 127 )

#define Q_IsColorStringHex(p) (Q_IsColorStringHexY((p))) || (Q_IsColorStringHexy((p))) || (Q_IsColorStringHexX((p))) || (Q_IsColorStringHexx((p)) )
#define Q_IsColorStringHexY(p) ((p)+8) && (p) && *(p)=='Y' && Q_IsHex((p+1)) && Q_IsHex((p+2)) && Q_IsHex((p+3)) && Q_IsHex((p+4)) && Q_IsHex((p+5)) && Q_IsHex((p+6)) && Q_IsHex((p+7)) && Q_IsHex((p+8))
#define Q_IsColorStringHexy(p) ((p)+4) && (p) && *(p)=='y' && Q_IsHex((p+1)) && Q_IsHex((p+2)) && Q_IsHex((p+3)) && Q_IsHex((p+4))
#define Q_IsColorStringHexX(p) ((p)+6) && (p) && *(p)=='X' && Q_IsHex((p+1)) && Q_IsHex((p+2)) && Q_IsHex((p+3)) && Q_IsHex((p+4)) && Q_IsHex((p+5)) && Q_IsHex((p+6))
#define Q_IsColorStringHexx(p) ((p)+3) && (p) && *(p)=='x' && Q_IsHex((p+1)) && Q_IsHex((p+2)) && Q_IsHex((p+3))

#define Q_IsHex(p) ((p) && ((*(p) >= '0' && *(p) <= '9') || (*(p) >= 'a' && *(p) <= 'f') || (*(p) >= 'A' && *(p) <= 'F')))


qboolean Q_parseColorHex(const char* p, float* color, int* skipCount) {
	char c = *p++;
	int i;
	int val;

	qboolean doWrite = qtrue;
	if (!color || !(color + 3)) {
		doWrite = qfalse;
	}

	*skipCount = 0; // We update it only if successful. If not successful, we want the string to be parsed normally.

	int countToParse = 8;
	qboolean halfPrecision = qfalse;
	if (c == 'Y') {
		countToParse = 8;
	}
	else if (c == 'y') {
		countToParse = 4;
		halfPrecision = qtrue;
	}
	else if (c == 'X') {
		countToParse = 6;
		if (doWrite) color[3] = 1.0f; // Z and z don't contain alpha.
	}
	else if (c == 'x') {
		countToParse = 3;
		if (doWrite) color[3] = 1.0f;
		halfPrecision = qtrue;
	}

	int presumableSkipCount = countToParse + 1; // skip count will be set to this if successful.

	for (i = 0; i < countToParse; i++) {
		int readHex;
		c = p[i];
		if (c >= '0' && c <= '9') {
			readHex = c - '0';
		}
		else if (c >= 'a' && c <= 'f') {
			readHex = 0xa + c - 'a';
		}
		else if (c >= 'A' && c <= 'F') {
			readHex = 0xa + c - 'A';
		}
		else {
			if (color) {
				color[0] = color[1] = color[2] = color[3] = 1.0f;
			}
			return qfalse;
		}
		if (doWrite) {

			if (halfPrecision) { // Single digit per value.
				val = readHex;
				color[i] = val * (1 / 15.0f);
			}
			else {
				if (i & 1) {
					val |= readHex;
					color[i >> 1] = val * (1 / 255.0f);
				}
				else {
					val = readHex << 4;
				}
			}
		}

	}

	*skipCount = presumableSkipCount;
	return qtrue;

}

std::string Q_StripColorAll(std::string string) {
	const char* sourceCString = string.c_str();
	int stringLen = strlen(sourceCString);
	char* cString = new char[stringLen + 1];
	strcpy_s(cString, stringLen + 1, sourceCString);
	Q_StripColorAll(cString);
	std::string colorStripped = cString;
	return colorStripped;
}

void Q_StripColorAll(char* text) {
	char* read;
	char* write;

	read = write = text;
	while (*read) {
		if (Q_IsColorStringHex(read + 1)) {
			int skipCount = 0;
			Q_parseColorHex(read + 1, 0, &skipCount);
			read += 1 + skipCount;
		}
		else if (Q_IsColorStringExt(read)) {
			read += 2;
		}
		else if (Q_IsColorStringNT(read)) {
			read += 2;
		}
		else {
			// Avoid writing the same data over itself
			if (write != read) {
				*write = *read;
			}
			write++;
			read++;
		}
	}
	if (write < read) {
		// Add trailing NUL byte if string has shortened
		*write = '\0';
	}
}





/*QUAKED item_***** ( 0 0 0 ) (-16 -16 -16) (16 16 16) suspended
DO NOT USE THIS CLASS, IT JUST HOLDS GENERAL INFORMATION.
The suspended flag will allow items to hang in the air, otherwise they are dropped to the next surface.

If an item is the target of another entity, it will not spawn in until fired.

An item fires all of its targets when it is picked up.  If the toucher can't carry it, the targets won't be fired.

"notfree" if set to 1, don't spawn in free for all games
"notteam" if set to 1, don't spawn in team games
"notsingle" if set to 1, don't spawn in single player games
"wait"	override the default wait before respawning.  -1 = never respawn automatically, which can be used with targeted spawning.
"random" random number of plus or minus seconds varied from the respawn time
"count" override quantity or duration on most items.
*/

/*QUAKED misc_shield_floor_unit (1 0 0) (-16 -16 0) (16 16 40)
#MODELNAME="/models/items/a_shield_converter.md3"
Gives shield energy when used.

"count" - max charge value (default 50)
"chargerate" - rechage 1 point every this many milliseconds (default 3000)
*/

#if 0
gitem_t	bg_itemlist_UNUSED[] =
{
	{
		NULL,				// classname	
		NULL,				// pickup_sound
		{	NULL,			// world_model[0]
			NULL,			// world_model[1]
			0, 0} ,			// world_model[2],[3]
		NULL,				// view_model
		/* icon */		NULL,		// icon
		/* pickup */	//NULL,		// pickup_name
				0,					// quantity
				(itemType_t)0,					// giType (IT_*)
				0,					// giTag
				/* precache */ "",			// precaches
				/* sounds */ ""				// sounds
					},	// leave index 0 alone

					//
					// Pickups
					//

				/*QUAKED item_shield_sm_instant (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
				Instant shield pickup, restores 25
				*/
					{
						"item_shield_sm_instant",
						"sound/player/pickupshield.wav",
						{ "models/map_objects/mp/psd_sm.md3",
						0, 0, 0},
						/* view */		NULL,
						/* icon */		"gfx/mp/small_shield",
						/* pickup *///	"Shield Small",
								25,
								IT_ARMOR,
								1, //special for shield - max on pickup is maxhealth*tag, thus small shield goes up to 100 shield
								/* precache */ "",
								/* sounds */ ""
									},

	/*QUAKED item_shield_lrg_instant (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
	Instant shield pickup, restores 100
	*/
		{
			"item_shield_lrg_instant",
			"sound/player/pickupshield.wav",
			{ "models/map_objects/mp/psd.md3",
			0, 0, 0},
			/* view */		NULL,
			/* icon */		"gfx/mp/large_shield",
			/* pickup *///	"Shield Large",
					100,
					IT_ARMOR,
					2, //special for shield - max on pickup is maxhealth*tag, thus large shield goes up to 200 shield
					/* precache */ "",
					/* sounds */ ""
						},

	/*QUAKED item_medpak_instant (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
	Instant medpack pickup, heals 25
	*/
		{
			"item_medpak_instant",
			"sound/player/pickuphealth.wav",
			{ "models/map_objects/mp/medpac.md3",
			0, 0, 0 },
			/* view */		NULL,
			/* icon */		"gfx/hud/i_icon_medkit",
			/* pickup *///	"Medpack",
					25,
					IT_HEALTH,
					0,
					/* precache */ "",
					/* sounds */ ""
						},


	//
	// ITEMS
	//

/*QUAKED item_seeker (.3 .3 1) (-8 -8 -0) (8 8 16) suspended
30 seconds of seeker drone
*/
	{
		"item_seeker",
		"sound/weapons/w_pkup.wav",
		{ "models/items/remote.md3",
		0, 0, 0} ,
		/* view */		NULL,
		/* icon */		"gfx/hud/i_icon_seeker",
		/* pickup *///	"Seeker Drone",
				120,
				IT_HOLDABLE,
				HI_SEEKER,
				/* precache */ "",
				/* sounds */ ""
					},

	/*QUAKED item_shield (.3 .3 1) (-8 -8 -0) (8 8 16) suspended
	Portable shield
	*/
		{
			"item_shield",
			"sound/weapons/w_pkup.wav",
			{ "models/map_objects/mp/shield.md3",
			0, 0, 0} ,
			/* view */		NULL,
			/* icon */		"gfx/hud/i_icon_shieldwall",
			/* pickup *///	"Forcefield",
					120,
					IT_HOLDABLE,
					HI_SHIELD,
					/* precache */ "",
					/* sounds */ "sound/weapons/detpack/stick.wav sound/movers/doors/forcefield_on.wav sound/movers/doors/forcefield_off.wav sound/movers/doors/forcefield_lp.wav sound/effects/bumpfield.wav",
						},

						/*QUAKED item_medpac (.3 .3 1) (-8 -8 -0) (8 8 16) suspended
						Bacta canister pickup, heals 25 on use
						*/
							{
								"item_medpac",	//should be item_bacta
								"sound/weapons/w_pkup.wav",
								{ "models/map_objects/mp/bacta.md3",
								0, 0, 0} ,
								/* view */		NULL,
								/* icon */		"gfx/hud/i_icon_bacta",
								/* pickup *///	"Bacta Canister",
										25,
										IT_HOLDABLE,
										HI_MEDPAC,
										/* precache */ "",
										/* sounds */ ""
											},

	/*QUAKED item_datapad (.3 .3 1) (-8 -8 -0) (8 8 16) suspended
	Do not place this.
	*/
		{
			"item_datapad",
			"sound/weapons/w_pkup.wav",
			{ "models/items/datapad.md3",
			0, 0, 0} ,
			/* view */		NULL,
			/* icon */		NULL,
			/* pickup *///	"Datapad",
					1,
					IT_HOLDABLE,
					HI_DATAPAD,
					/* precache */ "",
					/* sounds */ ""
						},

	/*QUAKED item_binoculars (.3 .3 1) (-8 -8 -0) (8 8 16) suspended
	These will be standard equipment on the player - DO NOT PLACE
	*/
		{
			"item_binoculars",
			"sound/weapons/w_pkup.wav",
			{ "models/items/binoculars.md3",
			0, 0, 0} ,
			/* view */		NULL,
			/* icon */		"gfx/hud/i_icon_zoom",
			/* pickup *///	"Binoculars",
					60,
					IT_HOLDABLE,
					HI_BINOCULARS,
					/* precache */ "",
					/* sounds */ ""
						},

	/*QUAKED item_sentry_gun (.3 .3 1) (-8 -8 -0) (8 8 16) suspended
	Sentry gun inventory pickup.
	*/
		{
			"item_sentry_gun",
			"sound/weapons/w_pkup.wav",
			{ "models/items/psgun.glm",
			0, 0, 0} ,
			/* view */		NULL,
			/* icon */		"gfx/hud/i_icon_sentrygun",
			/* pickup *///	"Sentry Gun",
					120,
					IT_HOLDABLE,
					HI_SENTRY_GUN,
					/* precache */ "",
					/* sounds */ ""
						},

	/*QUAKED item_force_enlighten_light (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
	Adds one rank to all Force powers temporarily. Only light jedi can use.
	*/
		{
			"item_force_enlighten_light",
			"sound/player/enlightenment.wav",
			{ "models/map_objects/mp/jedi_enlightenment.md3",
			0, 0, 0} ,
			/* view */		NULL,
			/* icon */		"gfx/hud/mpi_jlight",
			/* pickup *///	"Light Force Enlightenment",
					25,
					IT_POWERUP,
					PW_FORCE_ENLIGHTENED_LIGHT,
					/* precache */ "",
					/* sounds */ ""
						},

	/*QUAKED item_force_enlighten_dark (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
	Adds one rank to all Force powers temporarily. Only dark jedi can use.
	*/
		{
			"item_force_enlighten_dark",
			"sound/player/enlightenment.wav",
			{ "models/map_objects/mp/dk_enlightenment.md3",
			0, 0, 0} ,
			/* view */		NULL,
			/* icon */		"gfx/hud/mpi_dklight",
			/* pickup *///	"Dark Force Enlightenment",
					25,
					IT_POWERUP,
					PW_FORCE_ENLIGHTENED_DARK,
					/* precache */ "",
					/* sounds */ ""
						},

	/*QUAKED item_force_boon (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
	Unlimited Force Pool for a short time.
	*/
		{
			"item_force_boon",
			"sound/player/boon.wav",
			{ "models/map_objects/mp/force_boon.md3",
			0, 0, 0} ,
			/* view */		NULL,
			/* icon */		"gfx/hud/mpi_fboon",
			/* pickup *///	"Force Boon",
					25,
					IT_POWERUP,
					PW_FORCE_BOON,
					/* precache */ "",
					/* sounds */ ""
						},

	/*QUAKED item_ysalimari (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
	A small lizard carried on the player, which prevents the possessor from using any Force power.  However, he is unaffected by any Force power.
	*/
		{
			"item_ysalimari",
			"sound/player/ysalimari.wav",
			{ "models/map_objects/mp/ysalimari.md3",
			0, 0, 0} ,
			/* view */		NULL,
			/* icon */		"gfx/hud/mpi_ysamari",
			/* pickup *///	"Ysalamiri",
					25,
					IT_POWERUP,
					PW_YSALAMIRI,
					/* precache */ "",
					/* sounds */ ""
						},

	//
	// WEAPONS 
	//

/*QUAKED weapon_stun_baton (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
Don't place this
*/
	{
		"weapon_stun_baton",
		"sound/weapons/w_pkup.wav",
		{ "models/weapons2/stun_baton/baton_w.glm",
		0, 0, 0},
		/* view */		"models/weapons2/stun_baton/baton.md3",
		/* icon */		"gfx/hud/w_icon_stunbaton",
		/* pickup *///	"Stun Baton",
				100,
				IT_WEAPON,
				WP_STUN_BATON_JK2,
				/* precache */ "",
				/* sounds */ ""
					},

	/*QUAKED weapon_saber (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
	Don't place this
	*/
		{
			"weapon_saber",
			"sound/weapons/w_pkup.wav",
			{ "models/weapons2/saber/saber_w.glm",
			0, 0, 0},
			/* view */		"models/weapons2/saber/saber_w.md3",
			/* icon */		"gfx/hud/w_icon_lightsaber",
			/* pickup *///	"Lightsaber",
					100,
					IT_WEAPON,
					WP_SABER_JK2,
					/* precache */ "",
					/* sounds */ ""
						},

	/*QUAKED weapon_bryar_pistol (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
	Don't place this
	*/
		{
			"weapon_bryar_pistol",
			"sound/weapons/w_pkup.wav",
			{ "models/weapons2/briar_pistol/briar_pistol_w.glm",
			0, 0, 0},
			/* view */		"models/weapons2/briar_pistol/briar_pistol.md3",
			/* icon */		"gfx/hud/w_icon_rifle",
			/* pickup *///	"Bryar Pistol",
					100,
					IT_WEAPON,
					WP_BRYAR_PISTOL_JK2,
					/* precache */ "",
					/* sounds */ ""
						},

	/*QUAKED weapon_blaster (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
	*/
		{
			"weapon_blaster",
			"sound/weapons/w_pkup.wav",
			{ "models/weapons2/blaster_r/blaster_w.glm",
			0, 0, 0},
			/* view */		"models/weapons2/blaster_r/blaster.md3",
			/* icon */		"gfx/hud/w_icon_blaster",
			/* pickup *///	"E11 Blaster Rifle",
					100,
					IT_WEAPON,
					WP_BLASTER_JK2,
					/* precache */ "",
					/* sounds */ ""
						},

	/*QUAKED weapon_disruptor (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
	*/
		{
			"weapon_disruptor",
			"sound/weapons/w_pkup.wav",
			{ "models/weapons2/disruptor/disruptor_w.glm",
			0, 0, 0},
			/* view */		"models/weapons2/disruptor/disruptor.md3",
			/* icon */		"gfx/hud/w_icon_disruptor",
			/* pickup *///	"Tenloss Disruptor Rifle",
					100,
					IT_WEAPON,
					WP_DISRUPTOR_JK2,
					/* precache */ "",
					/* sounds */ ""
						},

	/*QUAKED weapon_bowcaster (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
	*/
		{
			"weapon_bowcaster",
			"sound/weapons/w_pkup.wav",
			{ "models/weapons2/bowcaster/bowcaster_w.glm",
			0, 0, 0},
			/* view */		"models/weapons2/bowcaster/bowcaster.md3",
			/* icon */		"gfx/hud/w_icon_bowcaster",
			/* pickup *///	"Wookiee Bowcaster",
					100,
					IT_WEAPON,
					WP_BOWCASTER_JK2,
					/* precache */ "",
					/* sounds */ ""
						},

	/*QUAKED weapon_repeater (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
	*/
		{
			"weapon_repeater",
			"sound/weapons/w_pkup.wav",
			{ "models/weapons2/heavy_repeater/heavy_repeater_w.glm",
			0, 0, 0},
			/* view */		"models/weapons2/heavy_repeater/heavy_repeater.md3",
			/* icon */		"gfx/hud/w_icon_repeater",
			/* pickup *///	"Imperial Heavy Repeater",
					100,
					IT_WEAPON,
					WP_REPEATER_JK2,
					/* precache */ "",
					/* sounds */ ""
						},

	/*QUAKED weapon_demp2 (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
	NOTENOTE This weapon is not yet complete.  Don't place it.
	*/
		{
			"weapon_demp2",
			"sound/weapons/w_pkup.wav",
			{ "models/weapons2/demp2/demp2_w.glm",
			0, 0, 0},
			/* view */		"models/weapons2/demp2/demp2.md3",
			/* icon */		"gfx/hud/w_icon_demp2",
			/* pickup *///	"DEMP2",
					100,
					IT_WEAPON,
					WP_DEMP2_JK2,
					/* precache */ "",
					/* sounds */ ""
						},

	/*QUAKED weapon_flechette (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
	*/
		{
			"weapon_flechette",
			"sound/weapons/w_pkup.wav",
			{ "models/weapons2/golan_arms/golan_arms_w.glm",
			0, 0, 0},
			/* view */		"models/weapons2/golan_arms/golan_arms.md3",
			/* icon */		"gfx/hud/w_icon_flechette",
			/* pickup *///	"Golan Arms Flechette",
					100,
					IT_WEAPON,
					WP_FLECHETTE_JK2,
					/* precache */ "",
					/* sounds */ ""
						},

	/*QUAKED weapon_rocket_launcher (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
	*/
		{
			"weapon_rocket_launcher",
			"sound/weapons/w_pkup.wav",
			{ "models/weapons2/merr_sonn/merr_sonn_w.glm",
			0, 0, 0},
			/* view */		"models/weapons2/merr_sonn/merr_sonn.md3",
			/* icon */		"gfx/hud/w_icon_merrsonn",
			/* pickup *///	"Merr-Sonn Missile System",
					3,
					IT_WEAPON,
					WP_ROCKET_LAUNCHER_JK2,
					/* precache */ "",
					/* sounds */ ""
						},

	/*QUAKED ammo_thermal (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
	*/
		{
			"ammo_thermal",
			"sound/weapons/w_pkup.wav",
			{ "models/weapons2/thermal/thermal_pu.md3",
			"models/weapons2/thermal/thermal_w.glm", 0, 0},
			/* view */		"models/weapons2/thermal/thermal.md3",
			/* icon */		"gfx/hud/w_icon_thermal",
			/* pickup *///	"Thermal Detonators",
					4,
					IT_AMMO,
					AMMO_THERMAL,
					/* precache */ "",
					/* sounds */ ""
						},

	/*QUAKED ammo_tripmine (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
	*/
		{
			"ammo_tripmine",
			"sound/weapons/w_pkup.wav",
			{ "models/weapons2/laser_trap/laser_trap_pu.md3",
			"models/weapons2/laser_trap/laser_trap_w.glm", 0, 0},
			/* view */		"models/weapons2/laser_trap/laser_trap.md3",
			/* icon */		"gfx/hud/w_icon_tripmine",
			/* pickup *///	"Trip Mines",
					3,
					IT_AMMO,
					AMMO_TRIPMINE,
					/* precache */ "",
					/* sounds */ ""
						},

	/*QUAKED ammo_detpack (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
	*/
		{
			"ammo_detpack",
			"sound/weapons/w_pkup.wav",
			{ "models/weapons2/detpack/det_pack_pu.md3", "models/weapons2/detpack/det_pack_proj.glm", "models/weapons2/detpack/det_pack_w.glm", 0},
			/* view */		"models/weapons2/detpack/det_pack.md3",
			/* icon */		"gfx/hud/w_icon_detpack",
			/* pickup *///	"Det Packs",
					3,
					IT_AMMO,
					AMMO_DETPACK,
					/* precache */ "",
					/* sounds */ ""
						},

	/*QUAKED weapon_thermal (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
	*/
		{
			"weapon_thermal",
			"sound/weapons/w_pkup.wav",
			{ "models/weapons2/thermal/thermal_w.glm", "models/weapons2/thermal/thermal_pu.md3",
			0, 0 },
			/* view */		"models/weapons2/thermal/thermal.md3",
			/* icon */		"gfx/hud/w_icon_thermal",
			/* pickup *///	"Thermal Detonator",
					4,
					IT_WEAPON,
					WP_THERMAL_JK2,
					/* precache */ "",
					/* sounds */ ""
						},

	/*QUAKED weapon_trip_mine (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
	*/
		{
			"weapon_trip_mine",
			"sound/weapons/w_pkup.wav",
			{ "models/weapons2/laser_trap/laser_trap_w.glm", "models/weapons2/laser_trap/laser_trap_pu.md3",
			0, 0},
			/* view */		"models/weapons2/laser_trap/laser_trap.md3",
			/* icon */		"gfx/hud/w_icon_tripmine",
			/* pickup *///	"Trip Mine",
					3,
					IT_WEAPON,
					WP_TRIP_MINE_JK2,
					/* precache */ "",
					/* sounds */ ""
						},

	/*QUAKED weapon_det_pack (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
	*/
		{
			"weapon_det_pack",
			"sound/weapons/w_pkup.wav",
			{ "models/weapons2/detpack/det_pack_proj.glm", "models/weapons2/detpack/det_pack_pu.md3", "models/weapons2/detpack/det_pack_w.glm", 0},
			/* view */		"models/weapons2/detpack/det_pack.md3",
			/* icon */		"gfx/hud/w_icon_detpack",
			/* pickup *///	"Det Pack",
					3,
					IT_WEAPON,
					WP_DET_PACK_JK2,
					/* precache */ "",
					/* sounds */ ""
						},

	/*QUAKED weapon_emplaced (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
	*/
		{
			"weapon_emplaced",
			"sound/weapons/w_pkup.wav",
			{ "models/weapons2/blaster_r/blaster_w.glm",
			0, 0, 0},
			/* view */		"models/weapons2/blaster_r/blaster.md3",
			/* icon */		"gfx/hud/w_icon_blaster",
			/* pickup *///	"Emplaced Gun",
					50,
					IT_WEAPON,
					WP_EMPLACED_GUN_JK2,
					/* precache */ "",
					/* sounds */ ""
						},


	//NOTE: This is to keep things from messing up because the turret weapon type isn't real
		{
			"weapon_turretwp",
			"sound/weapons/w_pkup.wav",
			{ "models/weapons2/blaster_r/blaster_w.glm",
			0, 0, 0},
			/* view */		"models/weapons2/blaster_r/blaster.md3",
			/* icon */		"gfx/hud/w_icon_blaster",
			/* pickup *///	"Turret Gun",
					50,
					IT_WEAPON,
					WP_TURRET_JK2,
					/* precache */ "",
					/* sounds */ ""
						},

	//
	// AMMO ITEMS
	//

/*QUAKED ammo_force (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
Don't place this
*/
	{
		"ammo_force",
		"sound/player/pickupenergy.wav",
		{ "models/items/energy_cell.md3",
		0, 0, 0},
		/* view */		NULL,
		/* icon */		"gfx/hud/w_icon_blaster",
		/* pickup *///	"Force??",
				100,
				IT_AMMO,
				AMMO_FORCE,
				/* precache */ "",
				/* sounds */ ""
					},

	/*QUAKED ammo_blaster (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
	Ammo for the Bryar and Blaster pistols.
	*/
		{
			"ammo_blaster",
			"sound/player/pickupenergy.wav",
			{ "models/items/energy_cell.md3",
			0, 0, 0},
			/* view */		NULL,
			/* icon */		"gfx/hud/i_icon_battery",
			/* pickup *///	"Blaster Pack",
					100,
					IT_AMMO,
					AMMO_BLASTER,
					/* precache */ "",
					/* sounds */ ""
						},

	/*QUAKED ammo_powercell (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
	Ammo for Tenloss Disruptor, Wookie Bowcaster, and the Destructive Electro Magnetic Pulse (demp2 ) guns
	*/
		{
			"ammo_powercell",
			"sound/player/pickupenergy.wav",
			{ "models/items/power_cell.md3",
			0, 0, 0},
			/* view */		NULL,
			/* icon */		"gfx/mp/ammo_power_cell",
			/* pickup *///	"Power Cell",
					100,
					IT_AMMO,
					AMMO_POWERCELL,
					/* precache */ "",
					/* sounds */ ""
						},

	/*QUAKED ammo_metallic_bolts (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
	Ammo for Imperial Heavy Repeater and the Golan Arms Flechette
	*/
		{
			"ammo_metallic_bolts",
			"sound/player/pickupenergy.wav",
			{ "models/items/metallic_bolts.md3",
			0, 0, 0},
			/* view */		NULL,
			/* icon */		"gfx/mp/ammo_metallic_bolts",
			/* pickup *///	"Metallic Bolts",
					100,
					IT_AMMO,
					AMMO_METAL_BOLTS,
					/* precache */ "",
					/* sounds */ ""
						},

	/*QUAKED ammo_rockets (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
	Ammo for Merr-Sonn portable missile launcher
	*/
		{
			"ammo_rockets",
			"sound/player/pickupenergy.wav",
			{ "models/items/rockets.md3",
			0, 0, 0},
			/* view */		NULL,
			/* icon */		"gfx/mp/ammo_rockets",
			/* pickup *///	"Rockets",
					3,
					IT_AMMO,
					AMMO_ROCKETS,
					/* precache */ "",
					/* sounds */ ""
						},


	//
	// POWERUP ITEMS
	//
/*QUAKED team_CTF_redflag (1 0 0) (-16 -16 -16) (16 16 16)
Only in CTF games
*/
	{
		"team_CTF_redflag",
		NULL,
		{ "models/flags/r_flag.md3",
		"models/flags/r_flag_ysal.md3", 0, 0 },
		/* view */		NULL,
		/* icon */		"gfx/hud/mpi_rflag",
		/* pickup *///	"Red Flag",
				0,
				IT_TEAM,
				PW_REDFLAG,
				/* precache */ "",
				/* sounds */ ""
					},

	/*QUAKED team_CTF_blueflag (0 0 1) (-16 -16 -16) (16 16 16)
	Only in CTF games
	*/
		{
			"team_CTF_blueflag",
			NULL,
			{ "models/flags/b_flag.md3",
			"models/flags/b_flag_ysal.md3", 0, 0 },
			/* view */		NULL,
			/* icon */		"gfx/hud/mpi_bflag",
			/* pickup *///	"Blue Flag",
					0,
					IT_TEAM,
					PW_BLUEFLAG,
					/* precache */ "",
					/* sounds */ ""
						},

	//
	// PERSISTANT POWERUP ITEMS
	//

	/*QUAKED team_CTF_neutralflag (0 0 1) (-16 -16 -16) (16 16 16)
Only in One Flag CTF games
*/
	{
		"team_CTF_neutralflag",
		NULL,
		{ "models/flags/n_flag.md3",
		0, 0, 0 },
		/* view */		NULL,
		/* icon */		"icons/iconf_neutral1",
		/* pickup *///	"Neutral Flag",
				0,
				IT_TEAM,
				PW_NEUTRALFLAG,
				/* precache */ "",
				/* sounds */ ""
					},

					{
						"item_redcube",
						"sound/player/pickupenergy.wav",
						{ "models/powerups/orb/r_orb.md3",
						0, 0, 0 },
						/* view */		NULL,
						/* icon */		"icons/iconh_rorb",
						/* pickup *///	"Red Cube",
								0,
								IT_TEAM,
								0,
								/* precache */ "",
								/* sounds */ ""
									},

									{
										"item_bluecube",
										"sound/player/pickupenergy.wav",
										{ "models/powerups/orb/b_orb.md3",
										0, 0, 0 },
										/* view */		NULL,
										/* icon */		"icons/iconh_borb",
										/* pickup *///	"Blue Cube",
												0,
												IT_TEAM,
												0,
												/* precache */ "",
												/* sounds */ ""
													},

	// end of list marker
	{NULL}
};

int		bg_numItems = sizeof(bg_itemlist) / sizeof(bg_itemlist[0]) - 1;
#endif


int forceMasteryPoints[NUM_FORCE_MASTERY_LEVELS] =
{
	0,		// FORCE_MASTERY_UNINITIATED,
	5,		// FORCE_MASTERY_INITIATE,
	10,		// FORCE_MASTERY_PADAWAN,
	20,		// FORCE_MASTERY_JEDI,
	30,		// FORCE_MASTERY_JEDI_GUARDIAN,
	50,		// FORCE_MASTERY_JEDI_ADEPT,
	75,		// FORCE_MASTERY_JEDI_KNIGHT,
	100		// FORCE_MASTERY_JEDI_MASTER,
};

int bgForcePowerCost[NUM_FORCE_POWERS][NUM_FORCE_POWER_LEVELS] = //0 == neutral
{
	{	0,	2,	4,	6	},	// Heal			// FP_HEAL
	{	0,	0,	2,	6	},	// Jump			//FP_LEVITATION,//hold/duration
	{	0,	2,	4,	6	},	// Speed		//FP_SPEED,//duration
	{	0,	1,	3,	6	},	// Push			//FP_PUSH,//hold/duration
	{	0,	1,	3,	6	},	// Pull			//FP_PULL,//hold/duration
	{	0,	4,	6,	8	},	// Mind Trick	//FP_TELEPATHY,//instant
	{	0,	1,	3,	6	},	// Grip			//FP_GRIP,//hold/duration
	{	0,	2,	5,	8	},	// Lightning	//FP_LIGHTNING,//hold/duration
	{	0,	4,	6,	8	},	// Dark Rage	//FP_RAGE,//duration
	{	0,	2,	5,	8	},	// Protection	//FP_PROTECT,//duration
	{	0,	1,	3,	6	},	// Absorb		//FP_ABSORB,//duration
	{	0,	1,	3,	6	},	// Team Heal	//FP_TEAM_HEAL,//instant
	{	0,	1,	3,	6	},	// Team Force	//FP_TEAM_FORCE,//instant
	{	0,	2,	4,	6	},	// Drain		//FP_DRAIN,//hold/duration
	{	0,	2,	5,	8	},	// Sight		//FP_SEE,//duration
	{	0,	1,	5,	8	},	// Saber Attack	//FP_SABERATTACK,
	{	0,	1,	5,	8	},	// Saber Defend	//FP_SABERDEFEND,
	{	0,	4,	6,	8	}	// Saber Throw	//FP_SABERTHROW,
	//NUM_FORCE_POWERS
};

const float forceJumpHeight[NUM_FORCE_POWER_LEVELS] =
{
	32,//normal jump (+stepheight+crouchdiff = 66)
	96,//(+stepheight+crouchdiff = 130)
	192,//(+stepheight+crouchdiff = 226)
	384//(+stepheight+crouchdiff = 418)
};

const float forceJumpStrength[NUM_FORCE_POWER_LEVELS] =
{
	JUMP_VELOCITY,//normal jump
	420,
	590,
	840
};


static vec3_t	bytedirs[NUMVERTEXNORMALS] =
{
{-0.525731f, 0.000000f, 0.850651f}, {-0.442863f, 0.238856f, 0.864188f},
{-0.295242f, 0.000000f, 0.955423f}, {-0.309017f, 0.500000f, 0.809017f},
{-0.162460f, 0.262866f, 0.951056f}, {0.000000f, 0.000000f, 1.000000f},
{0.000000f, 0.850651f, 0.525731f}, {-0.147621f, 0.716567f, 0.681718f},
{0.147621f, 0.716567f, 0.681718f}, {0.000000f, 0.525731f, 0.850651f},
{0.309017f, 0.500000f, 0.809017f}, {0.525731f, 0.000000f, 0.850651f},
{0.295242f, 0.000000f, 0.955423f}, {0.442863f, 0.238856f, 0.864188f},
{0.162460f, 0.262866f, 0.951056f}, {-0.681718f, 0.147621f, 0.716567f},
{-0.809017f, 0.309017f, 0.500000f},{-0.587785f, 0.425325f, 0.688191f},
{-0.850651f, 0.525731f, 0.000000f},{-0.864188f, 0.442863f, 0.238856f},
{-0.716567f, 0.681718f, 0.147621f},{-0.688191f, 0.587785f, 0.425325f},
{-0.500000f, 0.809017f, 0.309017f}, {-0.238856f, 0.864188f, 0.442863f},
{-0.425325f, 0.688191f, 0.587785f}, {-0.716567f, 0.681718f, -0.147621f},
{-0.500000f, 0.809017f, -0.309017f}, {-0.525731f, 0.850651f, 0.000000f},
{0.000000f, 0.850651f, -0.525731f}, {-0.238856f, 0.864188f, -0.442863f},
{0.000000f, 0.955423f, -0.295242f}, {-0.262866f, 0.951056f, -0.162460f},
{0.000000f, 1.000000f, 0.000000f}, {0.000000f, 0.955423f, 0.295242f},
{-0.262866f, 0.951056f, 0.162460f}, {0.238856f, 0.864188f, 0.442863f},
{0.262866f, 0.951056f, 0.162460f}, {0.500000f, 0.809017f, 0.309017f},
{0.238856f, 0.864188f, -0.442863f},{0.262866f, 0.951056f, -0.162460f},
{0.500000f, 0.809017f, -0.309017f},{0.850651f, 0.525731f, 0.000000f},
{0.716567f, 0.681718f, 0.147621f}, {0.716567f, 0.681718f, -0.147621f},
{0.525731f, 0.850651f, 0.000000f}, {0.425325f, 0.688191f, 0.587785f},
{0.864188f, 0.442863f, 0.238856f}, {0.688191f, 0.587785f, 0.425325f},
{0.809017f, 0.309017f, 0.500000f}, {0.681718f, 0.147621f, 0.716567f},
{0.587785f, 0.425325f, 0.688191f}, {0.955423f, 0.295242f, 0.000000f},
{1.000000f, 0.000000f, 0.000000f}, {0.951056f, 0.162460f, 0.262866f},
{0.850651f, -0.525731f, 0.000000f},{0.955423f, -0.295242f, 0.000000f},
{0.864188f, -0.442863f, 0.238856f}, {0.951056f, -0.162460f, 0.262866f},
{0.809017f, -0.309017f, 0.500000f}, {0.681718f, -0.147621f, 0.716567f},
{0.850651f, 0.000000f, 0.525731f}, {0.864188f, 0.442863f, -0.238856f},
{0.809017f, 0.309017f, -0.500000f}, {0.951056f, 0.162460f, -0.262866f},
{0.525731f, 0.000000f, -0.850651f}, {0.681718f, 0.147621f, -0.716567f},
{0.681718f, -0.147621f, -0.716567f},{0.850651f, 0.000000f, -0.525731f},
{0.809017f, -0.309017f, -0.500000f}, {0.864188f, -0.442863f, -0.238856f},
{0.951056f, -0.162460f, -0.262866f}, {0.147621f, 0.716567f, -0.681718f},
{0.309017f, 0.500000f, -0.809017f}, {0.425325f, 0.688191f, -0.587785f},
{0.442863f, 0.238856f, -0.864188f}, {0.587785f, 0.425325f, -0.688191f},
{0.688191f, 0.587785f, -0.425325f}, {-0.147621f, 0.716567f, -0.681718f},
{-0.309017f, 0.500000f, -0.809017f}, {0.000000f, 0.525731f, -0.850651f},
{-0.525731f, 0.000000f, -0.850651f}, {-0.442863f, 0.238856f, -0.864188f},
{-0.295242f, 0.000000f, -0.955423f}, {-0.162460f, 0.262866f, -0.951056f},
{0.000000f, 0.000000f, -1.000000f}, {0.295242f, 0.000000f, -0.955423f},
{0.162460f, 0.262866f, -0.951056f}, {-0.442863f, -0.238856f, -0.864188f},
{-0.309017f, -0.500000f, -0.809017f}, {-0.162460f, -0.262866f, -0.951056f},
{0.000000f, -0.850651f, -0.525731f}, {-0.147621f, -0.716567f, -0.681718f},
{0.147621f, -0.716567f, -0.681718f}, {0.000000f, -0.525731f, -0.850651f},
{0.309017f, -0.500000f, -0.809017f}, {0.442863f, -0.238856f, -0.864188f},
{0.162460f, -0.262866f, -0.951056f}, {0.238856f, -0.864188f, -0.442863f},
{0.500000f, -0.809017f, -0.309017f}, {0.425325f, -0.688191f, -0.587785f},
{0.716567f, -0.681718f, -0.147621f}, {0.688191f, -0.587785f, -0.425325f},
{0.587785f, -0.425325f, -0.688191f}, {0.000000f, -0.955423f, -0.295242f},
{0.000000f, -1.000000f, 0.000000f}, {0.262866f, -0.951056f, -0.162460f},
{0.000000f, -0.850651f, 0.525731f}, {0.000000f, -0.955423f, 0.295242f},
{0.238856f, -0.864188f, 0.442863f}, {0.262866f, -0.951056f, 0.162460f},
{0.500000f, -0.809017f, 0.309017f}, {0.716567f, -0.681718f, 0.147621f},
{0.525731f, -0.850651f, 0.000000f}, {-0.238856f, -0.864188f, -0.442863f},
{-0.500000f, -0.809017f, -0.309017f}, {-0.262866f, -0.951056f, -0.162460f},
{-0.850651f, -0.525731f, 0.000000f}, {-0.716567f, -0.681718f, -0.147621f},
{-0.716567f, -0.681718f, 0.147621f}, {-0.525731f, -0.850651f, 0.000000f},
{-0.500000f, -0.809017f, 0.309017f}, {-0.238856f, -0.864188f, 0.442863f},
{-0.262866f, -0.951056f, 0.162460f}, {-0.864188f, -0.442863f, 0.238856f},
{-0.809017f, -0.309017f, 0.500000f}, {-0.688191f, -0.587785f, 0.425325f},
{-0.681718f, -0.147621f, 0.716567f}, {-0.442863f, -0.238856f, 0.864188f},
{-0.587785f, -0.425325f, 0.688191f}, {-0.309017f, -0.500000f, 0.809017f},
{-0.147621f, -0.716567f, 0.681718f}, {-0.425325f, -0.688191f, 0.587785f},
{-0.162460f, -0.262866f, 0.951056f}, {0.442863f, -0.238856f, 0.864188f},
{0.162460f, -0.262866f, 0.951056f}, {0.309017f, -0.500000f, 0.809017f},
{0.147621f, -0.716567f, 0.681718f}, {0.000000f, -0.525731f, 0.850651f},
{0.425325f, -0.688191f, 0.587785f}, {0.587785f, -0.425325f, 0.688191f},
{0.688191f, -0.587785f, 0.425325f}, {-0.955423f, 0.295242f, 0.000000f},
{-0.951056f, 0.162460f, 0.262866f}, {-1.000000f, 0.000000f, 0.000000f},
{-0.850651f, 0.000000f, 0.525731f}, {-0.955423f, -0.295242f, 0.000000f},
{-0.951056f, -0.162460f, 0.262866f}, {-0.864188f, 0.442863f, -0.238856f},
{-0.951056f, 0.162460f, -0.262866f}, {-0.809017f, 0.309017f, -0.500000f},
{-0.864188f, -0.442863f, -0.238856f}, {-0.951056f, -0.162460f, -0.262866f},
{-0.809017f, -0.309017f, -0.500000f}, {-0.681718f, 0.147621f, -0.716567f},
{-0.681718f, -0.147621f, -0.716567f}, {-0.850651f, 0.000000f, -0.525731f},
{-0.688191f, 0.587785f, -0.425325f}, {-0.587785f, 0.425325f, -0.688191f},
{-0.425325f, 0.688191f, -0.587785f}, {-0.425325f, -0.688191f, -0.587785f},
{-0.587785f, -0.425325f, -0.688191f}, {-0.688191f, -0.587785f, -0.425325f}
};

void ByteToDir(int b, vec3_t dir) {
	if (b < 0 || b >= NUMVERTEXNORMALS) {
		VectorCopy(vec3_origin, dir);
		return;
	}
	VectorCopy(bytedirs[b], dir);
}

void vectoangles(const vec3_t value1, vec3_t angles) {
	float	forward;
	float	yaw, pitch;

	if (value1[1] == 0 && value1[0] == 0) {
		yaw = 0;
		if (value1[2] > 0) {
			pitch = 90;
		}
		else {
			pitch = 270;
		}
	}
	else {
		if (value1[0]) {
			yaw = (atan2(value1[1], value1[0]) * 180 / M_PI);
		}
		else if (value1[1] > 0) {
			yaw = 90;
		}
		else {
			yaw = 270;
		}
		if (yaw < 0) {
			yaw += 360;
		}

		forward = sqrt(value1[0] * value1[0] + value1[1] * value1[1]);
		pitch = (atan2(value1[2], forward) * 180 / M_PI);
		if (pitch < 0) {
			pitch += 360;
		}
	}

	angles[PITCH] = -pitch;
	angles[YAW] = yaw;
	angles[ROLL] = 0;
}


void AngleVectors(const vec3_t angles, vec3_t forward, vec3_t right, vec3_t up) {
	float		angle;
	static float		sr, sp, sy, cr, cp, cy;
	// static to help MS compiler fp bugs

	angle = angles[YAW] * (M_PI * 2 / 360);
	sy = sin(angle);
	cy = cos(angle);
	angle = angles[PITCH] * (M_PI * 2 / 360);
	sp = sin(angle);
	cp = cos(angle);
	angle = angles[ROLL] * (M_PI * 2 / 360);
	sr = sin(angle);
	cr = cos(angle);

	if (forward)
	{
		forward[0] = cp * cy;
		forward[1] = cp * sy;
		forward[2] = -sp;
	}
	if (right)
	{
		right[0] = (-1 * sr * sp * cy + -1 * cr * -sy);
		right[1] = (-1 * sr * sp * sy + -1 * cr * cy);
		right[2] = -1 * sr * cp;
	}
	if (up)
	{
		up[0] = (cr * sp * cy + -sr * -sy);
		up[1] = (cr * sp * sy + -sr * cy);
		up[2] = cr * cp;
	}
}

/*
=================
AngleSubtract

Always returns a value from -180 to 180
=================
*/
float	AngleSubtract(float a1, float a2) {
	float	a;

	a = a1 - a2;

	// Improved variant. Same results for most values but it's more correct for extremely high values (and more performant as well I guess)
	// The reason I do this: Some demos end up having (or being read as having) nonsensically high  float values.
	// This results in the old code entering an endless loop because subtracting 360 no longer does  anything  to float  values that are that  high.
	a = fmodf(a, 360.0f);
	if (a > 180) {
		a -= 360;
	}
	if (a < -180) {
		a += 360;
	}
#if 0
	assert(fabs(a) < 3600);
	while (a > 180) {
		a -= 360;
	}
	while (a < -180) {
		a += 360;
	}
#endif
	return a;
}


void AnglesSubtract(vec3_t v1, vec3_t v2, vec3_t v3) {
	v3[0] = AngleSubtract(v1[0], v2[0]);
	v3[1] = AngleSubtract(v1[1], v2[1]);
	v3[2] = AngleSubtract(v1[2], v2[2]);
}





















































qboolean CL_ServerVersionIs103(const char* versionstr) {
	return strstr(versionstr, "v1.03") ? qtrue : qfalse;
}





constexpr int strlenConstExpr(const char* txt) {
	int count = 0;
	while (*txt != 0) {
		count++;
		txt++;
	}
	return count;
}








qboolean demoCutInitClearGamestate(clientConnection_t* clcCut, clientActive_t* clCut, int serverCommandSequence, int clientNum, int checksumFeed) {
	int				i;
	entityState_t* es;
	int				newnum;
	entityState_t	nullstate;
	int				cmd;
	char* s;
	clcCut->connectPacketCount = 0;
	Com_Memset(clCut, 0, sizeof(*clCut));
	clcCut->serverCommandSequence = serverCommandSequence;
	clCut->gameState.dataCount = 1;
	clcCut->clientNum = clientNum;
	clcCut->checksumFeed = checksumFeed;
	return qtrue;
}

std::string makeConfigStringCommand(int index, std::string value) {
	std::stringstream ss;
	ss << "cs " << index << " " << value;
	return ss.str();
}





void retimeEntity(entityState_t* entity, double newServerTime, double newDemoTime) {
	vec3_t newPos;
	BG_EvaluateTrajectory(&entity->pos, newServerTime, newPos);
	VectorCopy(newPos, entity->pos.trBase);
	BG_EvaluateTrajectoryDelta(&entity->pos, newServerTime, newPos);
	VectorCopy(newPos, entity->pos.trDelta);
	BG_EvaluateTrajectory(&entity->apos, newServerTime, newPos);
	VectorCopy(newPos, entity->apos.trBase);
	BG_EvaluateTrajectoryDelta(&entity->apos, newServerTime, newPos);
	VectorCopy(newPos, entity->apos.trDelta);
	entity->pos.trTime = newDemoTime;
	entity->apos.trTime = newDemoTime;
}





/*
=================
IntegerToBoundingBox (MOHAA)
=================
*/
#define BBOX_XBITS 9
#define BBOX_YBITS 8
#define BBOX_ZBOTTOMBITS 5
#define BBOX_ZTOPBITS 9
#define BBOX_MAX_X ( 1 << BBOX_XBITS )
#define BBOX_MAX_Y ( 1 << BBOX_YBITS )
#define BBOX_MAX_BOTTOM_Z ( 1 << ( BBOX_ZBOTTOMBITS - 1 ) )
#define BBOX_REALMAX_BOTTOM_Z ( 1 << BBOX_ZBOTTOMBITS )
#define BBOX_MAX_TOP_Z ( 1 << BBOX_ZTOPBITS )
void IntegerToBoundingBox(int num, vec3_t mins, vec3_t maxs)
{
	int x, y, zd, zu;

	x = num & (BBOX_MAX_X - 1);
	y = (num >> (BBOX_XBITS)) & (BBOX_MAX_Y - 1);
	zd = (num >> (BBOX_XBITS + BBOX_YBITS)) & (BBOX_REALMAX_BOTTOM_Z - 1);
	zd -= BBOX_MAX_BOTTOM_Z;
	zu = (num >> (BBOX_XBITS + BBOX_YBITS + BBOX_ZBOTTOMBITS)) & (BBOX_MAX_TOP_Z - 1);

	mins[0] = -x;
	mins[1] = -y;
	mins[2] = zd;

	maxs[0] = x;
	maxs[1] = y;
	maxs[2] = zu;
}

#ifdef DEBUG
#define DEBUG_MOHAA_STRINGMATCH
//#define DEBUG_MOHAA_STRINGMATCH_FULL
#endif






















/*
============
FS_ReadFile

Filename are relative to the quake search path
a null buffer will just return the file length without loading
============
*/
int FS_ReadFile(const char* qpath, void** buffer, qboolean skipJKA) {
	fileHandle_t	h;
	byte* buf;
	int				len;


	if (!qpath || !qpath[0]) {
		Com_Error(ERR_FATAL, "FS_ReadFile with empty name");
	}

	buf = NULL;	// quiet compiler warning


	// look for it in the filesystem or pack files
	len = FS_FOpenFileRead(qpath, &h, qfalse,qfalse);
	if (h == 0) {
		if (buffer) {
			*buffer = NULL;
		}
		return -1;
	}

	if (!buffer) {
		FS_FCloseFile(h);
		return len;
	}

	buf = (byte*)malloc(len + 1);
	buf[len] = '\0';	// because we're not calling Z_Malloc with optional trailing 'bZeroIt' bool
	*buffer = buf;

	//	Z_Label(buf, qpath);

	FS_Read(buf, len, h);

	// guarantee that it will have a trailing 0 for string operations
	buf[len] = 0;
	FS_FCloseFile(h);

	// if we are journalling and it is a config file, write it to the journal file
	return len;
}


/*
============
FS_WriteFile

Filename are reletive to the quake search path
============
*/
void FS_WriteFile(const char* qpath, const void* buffer, int size) {
	fileHandle_t f;


	if (!qpath || !buffer) {
		Com_Error(ERR_FATAL, "FS_WriteFile: NULL parameter");
	}

	f = FS_FOpenFileWrite(qpath, FILECOMPRESSION_NONE);
	if (!f) {
		Com_Printf("Failed to open %s\n", qpath);
		return;
	}

	FS_Write(buffer, size, f);

	FS_FCloseFile(f);
}



