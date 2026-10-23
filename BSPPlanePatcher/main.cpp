#include "common.h"
#define cimg_display 0
#include "../ext/CImg.h"
#include "CModel.h"

// project for just playing around and testing concepts

#define DEG2RAD( a ) ( (a) * (float) ( M_PI / 180.0 ) )
#define RAD2DEG( a ) ( (a) * (float) ( 180.0 / M_PI ) )

bool r_newDLights = true;
int overbrightBits = 1;
float r_ambientScale = 0.6;
float r_directedScale = 1.0;
int deluxemode = 0;

bool pvscheck = false;
bool tracecheck = true;

/*
===============
R_ColorShiftLightingBytes

===============
*/
static	void R_ColorShiftLightingBytes( byte in[3])
{
	int		shift=0, r, g, b;

	// should NOT do it if overbrightBits is 0
	//if (tr.overbrightBits)
	//	shift = 1 - tr.overbrightBits;

	if (!shift)
	{
		return;
	}

	// shift the data based on overbright range
	r = in[0] << shift;
	g = in[1] << shift;
	b = in[2] << shift;

	// normalize by color instead of saturating to white
	if ( ( r | g | b ) > 255 ) {
		int		max;

		max = r > g ? r : g;
		max = max > b ? max : b;
		r = r * 255 / max;
		g = g * 255 / max;
		b = b * 255 / max;
	}

	in[0] = r;
	in[1] = g;
	in[2] = b;
}

float Com_Clamp(float min, float max, float value) {
	if (value < min) {
		return min;
	}
	if (value > max) {
		return max;
	}
	return value;
}

int Com_Clampi(int min, int max, int value) {
	if (value < min) {
		return min;
	}
	if (value > max) {
		return max;
	}
	return value;
}

typedef unsigned short		word;

typedef struct {
	vec3_t		bounds[2];		// for culling
	//msurface_t* firstSurface;
	int			numSurfaces;
} bmodel_t;
typedef struct
{
	byte		ambientLight[MAXLIGHTMAPS][3];
	byte		directLight[MAXLIGHTMAPS][3];
	byte		styles[MAXLIGHTMAPS];
	byte		latLong[2];
//	byte		pad[2];								// to align to a cache line
} mgrid_t;
typedef struct {

	vec3_t		lightGridOrigin;
	vec3_t		lightGridSize;
	vec3_t		lightGridInverseSize;
	int			lightGridBounds[3];

	int			lightGridOffsets[8];
	vec3_t		lightGridOffsetsWorld[8];

	vec3_t		lightGridStep;

	mgrid_t			*lightGridData;
	word		*lightGridArray;
	int			numGridArrayElements;
	bmodel_t	bmodels[1];
	
	char*		entityString;
	int			entityStringLen;
} world2_t;


void R_LoadEntities( lump_t *l, world2_t* w, byte* fileBase ) {
	const char *p;
	char *token, *s;
	char keyname[MAX_TOKEN_CHARS];
	char value[MAX_TOKEN_CHARS];

	w->lightGridSize[0] = 64;
	w->lightGridSize[1] = 64;
	w->lightGridSize[2] = 128;

	p = (const char *)(fileBase + l->fileofs);

	const char* extra = "\n\"fakedeluxemap\" \"1\"";
	int extraLen = strlen(extra);

	// store for reference by the cgame
	w->entityStringLen = l->filelen + 1 + extraLen;
	w->entityString = (char *)calloc(w->entityStringLen, 1 );
	Q_strncpyz(w->entityString, l->filelen + 1, p, l->filelen + 1);
	//strcpy( entityString, p );
	w->entityString[l->filelen] = '\0';
	p = (const char *)w->entityString;

	token = COM_ParseExt( &p, qtrue );
	if (!*token || *token != '{') {
		return;
	}

	int lenSoFar = p - w->entityString;
	memmove(w->entityString + lenSoFar + extraLen, w->entityString + lenSoFar, l->filelen + 1 - lenSoFar);
	memcpy(w->entityString + lenSoFar, extra, extraLen);

	// only parse the world spawn
	while ( 1 ) {
		// parse key
		token = COM_ParseExt( &p, qtrue );

		if ( !*token || *token == '}' ) {
			break;
		}
		Q_strncpyz(keyname, sizeof(keyname), token, sizeof(keyname));

		// parse value
		token = COM_ParseExt( &p, qtrue );

		if ( !*token || *token == '}' ) {
			break;
		}
		Q_strncpyz(value, sizeof(value), token, sizeof(value));

		// check for a different grid size
		if (!_stricmp(keyname, "gridsize")) {
			vec3_t gridSize;

			if ( sscanf(value, "%f %f %f", &gridSize[0], &gridSize[1], &gridSize[2]) != 3 ) {
				Com_Printf( "WARNING: Malformed gridsize '%s'\n", value);
			} else {
				w->lightGridSize[0] = gridSize[0];
				w->lightGridSize[1] = gridSize[1];
				w->lightGridSize[2] = gridSize[2];
			}
			continue;
		}

	}
}

/*
================
R_LoadLightGrid

================
*/
void R_LoadLightGrid(lump_t* l, world2_t* w, byte* fileBase) {
	int		i, j;
	vec3_t	maxs;
	float* wMins, * wMaxs;

	w->lightGridInverseSize[0] = 1.0 / w->lightGridSize[0];
	w->lightGridInverseSize[1] = 1.0 / w->lightGridSize[1];
	w->lightGridInverseSize[2] = 1.0 / w->lightGridSize[2];

	wMins = w->bmodels[0].bounds[0];
	wMaxs = w->bmodels[0].bounds[1];

	for (i = 0; i < 3; i++) {
		w->lightGridOrigin[i] = w->lightGridSize[i] * ceilf(wMins[i] / w->lightGridSize[i]);
		maxs[i] = w->lightGridSize[i] * floorf(wMaxs[i] / w->lightGridSize[i]);
		w->lightGridBounds[i] = (maxs[i] - w->lightGridOrigin[i]) / w->lightGridSize[i] + 1;
	}

	int numGridDataElements = l->filelen / sizeof(*w->lightGridData);

	w->lightGridData = (mgrid_t*)calloc(l->filelen, 1); // yes we leak, sue me
	memcpy(w->lightGridData, (void*)(fileBase + l->fileofs), l->filelen);

	// deal with overbright bits
	for (i = 0; i < numGridDataElements; i++)
	{
		for (j = 0; j < MAXLIGHTMAPS; j++)
		{
			R_ColorShiftLightingBytes(w->lightGridData[i].ambientLight[j]);
			R_ColorShiftLightingBytes(w->lightGridData[i].directLight[j]);
		}
	}

	//if (r_newDLights)
	{
		// Precalc soe data to speed up R_SetupEntityLightingGrid
		w->lightGridStep[0] = 1;
		w->lightGridStep[1] = w->lightGridBounds[0];
		w->lightGridStep[2] = w->lightGridBounds[0] * w->lightGridBounds[1];

		for (i = 0; i < 8; i++)
		{
			w->lightGridOffsets[i] = 0;
			VectorClear(w->lightGridOffsetsWorld[i]);

			if (i & 1)
			{
				w->lightGridOffsets[i] += w->lightGridStep[0];
				w->lightGridOffsetsWorld[i][0] += w->lightGridSize[0];
			}
			if (i & 2)
			{
				w->lightGridOffsets[i] += w->lightGridStep[1];
				w->lightGridOffsetsWorld[i][1] += w->lightGridSize[1];
			}
			if (i & 4)
			{
				w->lightGridOffsets[i] += w->lightGridStep[2];
				w->lightGridOffsetsWorld[i][2] += w->lightGridSize[2];
			}
		}
	}
}

void R_LoadLightGridArray( lump_t *l, world2_t* w, byte* fileBase) {

	w->numGridArrayElements = w->lightGridBounds[0] * w->lightGridBounds[1] * w->lightGridBounds[2];

	if ( l->filelen != w->numGridArrayElements * (int)sizeof(*w->lightGridArray) ) {
		Com_Printf( "WARNING: light grid array mismatch\n" );
		w->lightGridData = NULL;
		return;
	}

	w->lightGridArray =(word*) calloc( l->filelen,1 );// yes we leak, sue me
	memcpy( w->lightGridArray, (void *)(fileBase + l->fileofs), l->filelen );
}



typedef struct {
	vec3_t				origin;				// also used as MODEL_BEAM's "from"

} refEntity_t;
typedef struct {

	refEntity_t	e;
	float		axisLength;		// compensate for non-normalized axis

	qboolean	needDlights;	// true for bmodels that touch a dlight
	qboolean	lightingCalculated;
	vec3_t		lightDir;		// normalized direction towards light
	vec3_t		ambientLight;	// color normalized to 0-255
	int			ambientLightInt;	// 32 bit rgba packed
	vec3_t		directedLight;
	float		directionality;
	qboolean	intShaderTime;
} trRefEntity_t;

inline void VectorScaleVector(const vec3_t a, const vec3_t b, vec3_t out)
{
	out[0] = a[0] * b[0];
	out[1] = a[1] * b[1];
	out[2] = a[2] * b[2];
}
int wallsamples[4] = { 0 };

static bool R_LightGridCheckGridPointValidity(CModel* cm, int i, vec3_t origin, int pos[3], world2_t* world) {
	if (cm && (pvscheck || tracecheck)) {
		int j;
		vec3_t posHere;
		for (j = 0; j < 3; j++) {
			posHere[j] = world->lightGridOrigin[j] + pos[j] * world->lightGridSize[j] + world->lightGridOffsetsWorld[i][j];
		}
		if (tracecheck) {
			trace_t trace;
			const float maxDeviationRadiant = std::max(sqrtf(3.0f * (18.0f * 18.0f)), 0.5f * VectorLength(world->lightGridSize)); // netradiant-custom nudges half the gridsize, gtkradiant 9 and 18 units, so we can be off by up to this amount, and the gridpoint can still contain valid info for the current location
			// note this logic most likely does not 100% perfectly account for all scenarios, its an approximation of mine.
			//if (VectorDistance(ent->e.origin, posHere) > maxDeviationRadiant) {
			cm->CM_BoxTrace(&trace, origin, posHere, NULL, NULL, 0, MASK_PLAYERSOLID, qfalse);
			if (!trace.allsolid && !trace.startsolid && trace.fraction < 1.0f) {

				// don't have clear line of sight. 
				// check whether the gridpoint is in valid space. if not, it may have nudged things.
				trace_t trace2;
				cm->CM_BoxTrace(&trace2, posHere, posHere, NULL, NULL, 0, MASK_PLAYERSOLID, qfalse);
				if (trace2.startsolid || trace2.allsolid) {

					float remainingDistance = VectorDistance(trace.endpos, posHere);
					if (remainingDistance > maxDeviationRadiant) {
						wallsamples[3]++;
						// ignore samples in walls #2
						return qfalse;
					}
				}
				else {
					wallsamples[3]++;
					return qfalse;
				}
			}
			//}
		}
		if (pvscheck) {
			if (!cm->R_inPVS(origin, posHere))
			{
				wallsamples[1]++;
				// ignore samples in walls #2
				return false;
			}
			if (!cm->R_inPVS(posHere, posHere)) // this never really triggers tbh, seems q3map2 is smart enough to do it itself.
			{
				wallsamples[2]++;
				// ignore samples in walls #2
				return false;
			}
		}
	}
	return true;
}

typedef byte color4ub_t[4];
color4ub_t	styleColors[MAX_LIGHT_STYLES] = { 0 };
#define	FOG_TABLE_SIZE		256
#define FUNCTABLE_SIZE		1024
#define FUNCTABLE_SIZE2		10
#define FUNCTABLE_MASK		(FUNCTABLE_SIZE-1)
float					sinTable[FUNCTABLE_SIZE];


static void R_SetupEntityLightingGrid( trRefEntity_t *ent, world2_t* world, CModel* cm ) {
	vec3_t			lightOrigin;
	int				pos[3];
	int				i, j;
	float			frac[3];
	int				gridStep[3];
	vec3_t			direction;
	float			totalFactor;
	unsigned short	*startGridPos;

	if (r_newDLights)
	{
		vec3_t v, invfrac;
		float fraction[8];

		VectorCopy(ent->e.origin, lightOrigin);
		VectorSubtract( lightOrigin, world->lightGridOrigin, lightOrigin );
		VectorScaleVector( lightOrigin, world->lightGridInverseSize, v );

		pos[0] = (int)floorf(v[0]);
		pos[1] = (int)floorf(v[1]);
		pos[2] = (int)floorf(v[2]);

		frac[0] = v[0] - (float)pos[0];
		frac[1] = v[1] - (float)pos[1];
		frac[2] = v[2] - (float)pos[2];

		invfrac[0] = 1.0f - frac[0];
		invfrac[1] = 1.0f - frac[1];
		invfrac[2] = 1.0f - frac[2];

		fraction[0] = invfrac[0] * invfrac[1] * invfrac[2];
		fraction[1] = frac[0] * invfrac[1] * invfrac[2];
		fraction[2] = invfrac[0] * frac[1] * invfrac[2];
		fraction[3] = frac[0] * frac[1] * invfrac[2];
		fraction[4] = invfrac[0] * invfrac[1] * frac[2];
		fraction[5] = frac[0] * invfrac[1] * frac[2];
		fraction[6] = invfrac[0] * frac[1] * frac[2];
		fraction[7] = frac[0] * frac[1] * frac[2];

		pos[0] = Com_Clampi(0, world->lightGridBounds[0] - 1, pos[0]);
		pos[1] = Com_Clampi(0, world->lightGridBounds[1] - 1, pos[1]);
		pos[2] = Com_Clampi(0, world->lightGridBounds[2] - 1, pos[2]);

		VectorClear( ent->ambientLight );
		VectorClear( ent->directedLight );
		VectorClear( direction );

		// trilerp the light value
		/*
		startGridPos = world->lightGridArray + (pos[0] * world->lightGridStep[0]) + (pos[1] * world->lightGridStep[1]) + (pos[2] * world->lightGridStep[2]);
		*/
		startGridPos = world->lightGridArray + (int)((pos[0] * world->lightGridStep[0])) + (int)((pos[1] * world->lightGridStep[1])) + (int)((pos[2] * world->lightGridStep[2]));

		totalFactor = 0;
		for ( i = 0 ; i < 8 ; i++ )
		{
			float			factor;
			mgrid_t			*data;
			unsigned short	*gridPos;
			int				lat, lng;
			vec3_t			normal;

			gridPos = startGridPos + world->lightGridOffsets[i];

			if (gridPos >= world->lightGridArray + world->numGridArrayElements)
			{
				//we've gone off the array somehow
				continue;
			}

			data = world->lightGridData + *gridPos;
			if ( data->styles[0] == LS_LSNONE )
			{
				wallsamples[0]++;
				continue;	// ignore samples in walls
			}

			if (!R_LightGridCheckGridPointValidity(cm, i, ent->e.origin, pos, world)) {
				continue;
			}

			factor = fraction[i];
			totalFactor += factor;

			for(j = 0; j < MAXLIGHTMAPS; j++)
			{
				if (data->styles[j] != LS_LSNONE)
				{
					const byte	style = data->styles[j];

					ent->ambientLight[0] += factor * data->ambientLight[j][0] * styleColors[style][0] / 255.0f;
					ent->ambientLight[1] += factor * data->ambientLight[j][1] * styleColors[style][1] / 255.0f;
					ent->ambientLight[2] += factor * data->ambientLight[j][2] * styleColors[style][2] / 255.0f;

					ent->directedLight[0] += factor * data->directLight[j][0] * styleColors[style][0] / 255.0f;
					ent->directedLight[1] += factor * data->directLight[j][1] * styleColors[style][1] / 255.0f;
					ent->directedLight[2] += factor * data->directLight[j][2] * styleColors[style][2] / 255.0f;
				}
				else
				{
					break;
				}
			}

			lat = data->latLong[1] << 2;
			lng = data->latLong[0] << 2;

			// decode X as cos( lat ) * sin( long )
			// decode Y as sin( lat ) * sin( long )
			// decode Z as cos( long )

			normal[0] = sinTable[(lat + (FUNCTABLE_SIZE / 4)) & FUNCTABLE_MASK] * sinTable[lng];
			normal[1] = sinTable[lat] * sinTable[lng];
			normal[2] = sinTable[(lng + (FUNCTABLE_SIZE / 4)) & FUNCTABLE_MASK];

			VectorMA( direction, factor, normal, direction );
		}

		if ( totalFactor > 0 && totalFactor < 0.99f )
		{
			totalFactor = 1.0f / totalFactor;
			VectorScale( ent->ambientLight, totalFactor, ent->ambientLight );
			VectorScale( ent->directedLight, totalFactor, ent->directedLight );
		}

		VectorScale( ent->ambientLight, r_ambientScale, ent->ambientLight );
		VectorScale( ent->directedLight, r_directedScale, ent->directedLight );
		VectorNormalize2( direction, ent->lightDir );
	}
	else
	{

		VectorCopy(ent->e.origin, lightOrigin);

		VectorSubtract( lightOrigin, world->lightGridOrigin, lightOrigin );
		for ( i = 0 ; i < 3 ; i++ ) {
			float	v;

			v = lightOrigin[i]*world->lightGridInverseSize[i];
			pos[i] = floorf( v );
			frac[i] = v - pos[i];
			if ( pos[i] < 0 ) {
				pos[i] = 0;
			} else if ( pos[i] >= world->lightGridBounds[i] - 1 ) {
				pos[i] = world->lightGridBounds[i] - 1;
			}
		}

		VectorClear( ent->ambientLight );
		VectorClear( ent->directedLight );
		VectorClear( direction );

		// trilerp the light value
		gridStep[0] = 1;
		gridStep[1] = world->lightGridBounds[0];
		gridStep[2] = world->lightGridBounds[0] * world->lightGridBounds[1];
		startGridPos = world->lightGridArray + (pos[0] * gridStep[0] + pos[1] * gridStep[1] + pos[2] * gridStep[2]);

		totalFactor = 0;
		for ( i = 0 ; i < 8 ; i++ ) {
			float			factor;
			mgrid_t			*data;
			unsigned short	*gridPos;
			int				lat, lng;
			vec3_t			normal;

			factor = 1.0;
			gridPos = startGridPos;
			for ( j = 0 ; j < 3 ; j++ ) {
				if ( i & (1<<j) ) {
					if ( pos[j] + 1 > world->lightGridBounds[j] - 1 ) {
						break; // ignore values outside lightgrid
					}
					factor *= frac[j];
					gridPos += gridStep[j];
				} else {
					factor *= (1.0f - frac[j]);
				}
			}

			if (j != 3)
			{
				continue;
			}
			if (gridPos >= world->lightGridArray + world->numGridArrayElements)
			{//we've gone off the array somehow
				continue;
			}
			data = world->lightGridData + *gridPos;
			if ( data->styles[0] == LS_LSNONE )
			{
				wallsamples[0]++;
				continue;	// ignore samples in walls
			}


			if (!R_LightGridCheckGridPointValidity(cm, i, ent->e.origin, pos, world)) {
				continue;
			}

			totalFactor += factor;

			for(j=0;j<MAXLIGHTMAPS;j++)
			{
				if (data->styles[j] != LS_LSNONE)
				{
					const byte	style= data->styles[j];

					ent->ambientLight[0] += factor * data->ambientLight[j][0] * styleColors[style][0] / 255.0f;
					ent->ambientLight[1] += factor * data->ambientLight[j][1] * styleColors[style][1] / 255.0f;
					ent->ambientLight[2] += factor * data->ambientLight[j][2] * styleColors[style][2] / 255.0f;

					ent->directedLight[0] += factor * data->directLight[j][0] * styleColors[style][0] / 255.0f;
					ent->directedLight[1] += factor * data->directLight[j][1] * styleColors[style][1] / 255.0f;
					ent->directedLight[2] += factor * data->directLight[j][2] * styleColors[style][2] / 255.0f;
				}
				else
				{
					break;
				}
			}

			lat = data->latLong[1];
			lng = data->latLong[0];
			lat *= (FUNCTABLE_SIZE/256);
			lng *= (FUNCTABLE_SIZE/256);

			// decode X as cos( lat ) * sin( long )
			// decode Y as sin( lat ) * sin( long )
			// decode Z as cos( long )

			normal[0] = sinTable[(lat + (FUNCTABLE_SIZE / 4)) & FUNCTABLE_MASK] * sinTable[lng];
			normal[1] = sinTable[lat] * sinTable[lng];
			normal[2] = sinTable[(lng + (FUNCTABLE_SIZE / 4)) & FUNCTABLE_MASK];

			VectorMA( direction, factor, normal, direction );
		}

		if ( totalFactor > 0 && totalFactor < 0.99f )
		{
			totalFactor = 1.0f / totalFactor;
			VectorScale( ent->ambientLight, totalFactor, ent->ambientLight );
			VectorScale( ent->directedLight, totalFactor, ent->directedLight );
		}

		VectorScale( ent->ambientLight, r_ambientScale, ent->ambientLight );
		VectorScale( ent->directedLight, r_directedScale, ent->directedLight );

		VectorNormalize2( direction, ent->lightDir );
	}
}

static vec_t VectorLengthSquared( const vec3_t v ) {
	return (v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
}

int R_LightDirForPoint(vec3_t point, vec3_t lightDir, vec3_t normal, float* directionality, world2_t* world, float normalDotRestrict, float normalDotRestrictLow, CModel* cm)
{
	trRefEntity_t ent;
	float dot;

	if (world->lightGridData == NULL)
		return qfalse;

	Com_Memset(&ent, 0, sizeof(ent));
	VectorCopy(point, ent.e.origin);
	R_SetupEntityLightingGrid(&ent, world, cm);

	dot = DotProduct(ent.lightDir, normal);
	if (VectorLengthSquared(normal) == 0.0f || dot > normalDotRestrict) {
		VectorCopy(ent.lightDir, lightDir);
		if (directionality) {
			*directionality = ent.directionality;
		}
	}
	else {
		dot = (dot - normalDotRestrictLow)/(normalDotRestrict- normalDotRestrictLow);
		if (dot > 0.0f) {
			VectorScale(normal,(1.0f- dot), lightDir);
			VectorMA(lightDir, dot, ent.lightDir, lightDir);
		}
		else {
			VectorCopy(normal, lightDir);
		}
		if (directionality) {
			*directionality = 0;
		}
	}

	return qtrue;
}

void R_InitFunctionTables() {
	//
	// init function tables
	//
	for ( int i = 0; i < FUNCTABLE_SIZE; i++ )
	{
		sinTable[i]		= sinf( DEG2RAD( i * 360.0f / ( ( float ) ( FUNCTABLE_SIZE - 1 ) ) ) );
		
	}
}


enum resamplingMode_t {
	RAW,
	LINEAR,
	LINEAR_OVERBRIGHT
};

static inline float linearizeSRGB(float n)
{
	return (n > 0.04045f ? (float)powf((n + 0.055) / 1.055, 2.4) : n / 12.92f);
}
static inline float delinearizeSRGB(float n)
{
	return n > 0.0031308f ? 1.055f * (float)powf(n, 1 / 2.4) - 0.055f : 12.92f * n;
}

inline float toLinear(byte value,bool overBright) {
	return overBright ? linearizeSRGB((float)value*2.f/255.f) : linearizeSRGB((float)value / 255.f);
}
inline byte fromLinear(float value,bool overBright) {
	return (byte)std::max(std::min((overBright ? delinearizeSRGB(value)*255.0f/2.0f : delinearizeSRGB(value) * 255.0f),255.f),0.0f);
}

int main(int argc, char** argv) {
	int			i;
	dheader_t* header;
	dheader_t* header2;
	byte* buffer;
	byte* buffer2;

	float dotTolerance = 0.99f;
	float distTolerance = 0.1f;

	if (argc < 4) { 

		std::cout << "Usage: BSP file, reference planes BSP file, and output BSP file. Optional: plane tolerances (dot & dist, default 0.99 and 0.1).\n";
		return 1; 
	}

	const char* fileName = argv[1];
	const char* fileName2 = argv[2];
	const char* fileNameOut = argv[3];

	if (argc > 4) {
		dotTolerance = atof(argv[4]);
	}
	if (argc > 5) {
		distTolerance = atof(argv[5]);
	}

	std::cout << "Usage: BSP file, reference planes BSP file, and output BSP file. Optional: plane tolerances (dot & dist, default 0.99 and 0.1).\n";

	int inputFileLength = FS_ReadFile(fileName, (void**)&buffer, qfalse);
	if (!buffer) {
		Com_Printf("RE_LoadWorldMap: %s not found", fileName);
		return 1;
	}

	int inputFileLength2 = FS_ReadFile(fileName2, (void**)&buffer2, qfalse);
	if (!buffer2) {
		Com_Printf("RE_LoadWorldMap (2): %s not found", fileName2);
		return 1;
	}


	header = (dheader_t*)buffer;
	byte* fileBase = (byte*)header;

	header2 = (dheader_t*)buffer2;
	byte* fileBase2 = (byte*)header2;

	i = LittleLong(header->version);
	if (i != BSP_VERSION) {
		Com_Printf("RE_LoadWorldMap: %s has wrong version number (%i should be %i)",
			fileName, i, BSP_VERSION);
		return 1;
	}
	i = LittleLong(header2->version);
	if (i != BSP_VERSION) {
		Com_Printf("RE_LoadWorldMap (2): %s has wrong version number (%i should be %i)",
			fileName, i, BSP_VERSION);
		return 1;
	}

	// patch planes
	{
		lump_t* l = &header->lumps[LUMP_PLANES];
		int len = l->filelen;
		if (!len) {
			return 1;
		}
		byte* buf = fileBase + l->fileofs;
		dplane_t* in = (dplane_t*)buf;
		int count = l->filelen / sizeof(*in);

		lump_t* l2 = &header2->lumps[LUMP_PLANES];
		int len2 = l2->filelen;
		if (!len2) {
			return 1;
		}
		byte* buf2 = fileBase2 + l2->fileofs;
		dplane_t* in2 = (dplane_t*)buf2;
		int count2 = l2->filelen / sizeof(*in2);

		std::cout << "Planes (BSP): " << count << ", Planes (reference BSP): " << count2 << "\n";

		int patched = 0;
		int identical = 0;
		int nomatch = 0;

		double diffTolerance = std::abs(1.0 - dotTolerance);



		for (i = 0; i < count; i++, in++) {
			int closestMatch = -1;
#if 1
			double bestDiff = 0;
			double bestDist = 0;
			in2 = (dplane_t*)buf2;
			int candidates = 0;
			for (int j = 0; j < count2; j++, in2++) {
				double diff = (in->normal[0] - in2->normal[0])* (in->normal[0] - in2->normal[0])
					+ (in->normal[1] - in2->normal[1]) * (in->normal[1] - in2->normal[1])
					+ (in->normal[2] - in2->normal[2]) * (in->normal[2] - in2->normal[2])
					;
				diff = !diff ? 0 : sqrt(diff/3.0);
				double dist = std::abs((double)in->dist - (double)in2->dist);
				if (diff <= diffTolerance && dist <= distTolerance) {
					candidates++;
					// might be a match
					if (closestMatch == -1 || diff < bestDiff || diff == bestDiff && dist < bestDist) {
						closestMatch = j;
						bestDiff = diff;
						bestDist = dist;
					}
				}
			}
			if (!candidates) {
				nomatch++;
				continue;
			}
			if (candidates > 1) {
				//std::cerr << "Warning: " << candidates << " candidates for plane. Picked best with " << bestDiff << " " << bestDist << ".\n";
			}
#else // dotproduct calculation introduces imprecision.
			float bestDot = 0;
			float bestDist = 0;
			in2 = (dplane_t*)buf2;
			int candidates = 0;
			for (int j = 0; j < count2; j++,in2++) {
				float dot = DotProduct(in->normal,in2->normal);
				float dist = std::abs(in->dist-in2->dist);
				if (dot >= dotTolerance && dist <= distTolerance) {
					dot = std::abs(1.0f-dot);
					candidates++;
					// might be a match
					if (closestMatch == -1 || dot < bestDot || dot == bestDot && dist < bestDist) {
						closestMatch = j;
						bestDot = dot;
						bestDist = dist;
					}
				}
			}
			if (!candidates) {
				nomatch++;
				continue;
			}
			if (candidates > 1) {
				std::cerr << "Warning: " << candidates << " candidates for plane. Picked best with " << bestDot << " " << bestDist << ".\n";
			}
#endif
			in2 = (dplane_t*)buf2 + closestMatch;

			if (in->normal[0] == in2->normal[0] && in->normal[1] == in2->normal[1] && in->normal[2] == in2->normal[2] && in->dist == in2->dist) {
				identical++;
				continue;
			}

			*in = *in2;
			patched++;
		}

		std::cout << "Patched: " << patched << ", identical: " << identical << ", no match: " << nomatch << "\n";


	}

	// patch brushes
	{
		lump_t* l = &header->lumps[LUMP_BRUSHES];
		int len = l->filelen;
		if (!len) {
			return 1;
		}
		byte* buf = fileBase + l->fileofs;
		dbrush_t* in = (dbrush_t*)buf;
		int count = l->filelen / sizeof(*in);

		lump_t* l2 = &header2->lumps[LUMP_BRUSHES];
		int len2 = l2->filelen;
		if (!len2) {
			return 1;
		}
		byte* buf2 = fileBase2 + l2->fileofs;
		dbrush_t* in2 = (dbrush_t*)buf2;
		int count2 = l2->filelen / sizeof(*in2);

		int patched = 0;
		int identical = 0;
		int nomatch = 0;
		/*
		for (i = 0; i < count; i++, in++) {
			int closestMatch = -1;
			float bestDot = 0;
			float bestDist = 0;
			in2 = (dbrush_t*)buf2;
			int candidates = 0;
			for (int j = 0; j < count2; j++, in2++) {
				float dot = DotProduct(in->normal, in2->normal);
				float dist = std::abs(in->dist - in2->dist);
				if (dot >= dotTolerance && dist <= distTolerance) {
					candidates++;
					// might be a match
					if (closestMatch == -1 || dot > bestDot || dot == bestDot && dist < bestDist) {
						closestMatch = j;
						bestDot = dot;
						bestDist = dist;
					}
				}
			}
			if (!candidates) {
				nomatch++;
				continue;
			}
			if (candidates > 1) {
				std::cerr << "Warning: " << candidates << " candidates for plane. Picked best with " << bestDot << " " << bestDist << ".\n";
			}
			in2 = (dbrush_t*)buf2 + closestMatch;

			if (in->normal[0] == in2->normal[0] && in->normal[1] == in2->normal[1] && in->normal[2] == in2->normal[2] && in->dist == in2->dist) {
				identical++;
				continue;
			}

			*in = *in2;
			patched++;
		}*/

		std::cout << "Patched: " << patched << ", identical: " << identical << ", no match: " << nomatch << "\n";


	}

	FS_WriteFile(fileNameOut, buffer, inputFileLength);
}