#include "common.h"
#define cimg_display 0
#include "../ext/CImg.h"

// project for just playing around and testing concepts

#define DEG2RAD( a ) ( (a) * (float) ( M_PI / 180.0 ) )
#define RAD2DEG( a ) ( (a) * (float) ( 180.0 / M_PI ) )

bool r_newDLights = false;
int overbrightBits = 1;
float r_ambientScale = 0.6;
float r_directedScale = 1.0;
int deluxemode = 0;

vec_t VectorNormalize2(const vec3_t v, vec3_t out) {
	float	length, ilength;

	length = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
	length = sqrtf(length);

	if (length)
	{
#ifndef Q3_VM // bk0101022 - FPE related
		//	  assert( ((Q_fabs(v[0])!=0.0f) || (Q_fabs(v[1])!=0.0f) || (Q_fabs(v[2])!=0.0f)) );
#endif
		ilength = 1 / length;
		out[0] = v[0] * ilength;
		out[1] = v[1] * ilength;
		out[2] = v[2] * ilength;
	}
	else {
#ifndef Q3_VM // bk0101022 - FPE related
		//	  assert( ((Q_fabs(v[0])==0.0f) && (Q_fabs(v[1])==0.0f) && (Q_fabs(v[2])==0.0f)) );
#endif
		VectorClear(out);
	}

	return length;

}
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

	vec3_t		lightGridStep;

	mgrid_t			*lightGridData;
	word		*lightGridArray;
	int			numGridArrayElements;
	bmodel_t	bmodels[1];
} world_t;


void R_LoadEntities( lump_t *l, world_t* w, byte* fileBase ) {
	const char *p;
	char *token, *s;
	char keyname[MAX_TOKEN_CHARS];
	char value[MAX_TOKEN_CHARS];

	w->lightGridSize[0] = 64;
	w->lightGridSize[1] = 64;
	w->lightGridSize[2] = 128;

	p = (const char *)(fileBase + l->fileofs);

	// store for reference by the cgame
	char* entityString = (char *)calloc( l->filelen + 1, 1 );
	Q_strncpyz(entityString, l->filelen + 1, p, l->filelen + 1);
	//strcpy( entityString, p );
	entityString[l->filelen] = '\0';
	const char* entityParsePoint = (const char *) entityString;

	token = COM_ParseExt( &p, qtrue );
	if (!*token || *token != '{') {
		return;
	}
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
void R_LoadLightGrid(lump_t* l, world_t* w, byte* fileBase) {
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

	if (r_newDLights)
	{
		// Precalc soe data to speed up R_SetupEntityLightingGrid
		w->lightGridStep[0] = 1;
		w->lightGridStep[1] = w->lightGridBounds[0];
		w->lightGridStep[2] = w->lightGridBounds[0] * w->lightGridBounds[1];

		for (i = 0; i < 8; i++)
		{
			w->lightGridOffsets[i] = 0;

			if (i & 1)
			{
				w->lightGridOffsets[i] += w->lightGridStep[0];
			}
			if (i & 2)
			{
				w->lightGridOffsets[i] += w->lightGridStep[1];
			}
			if (i & 4)
			{
				w->lightGridOffsets[i] += w->lightGridStep[2];
			}
		}
	}
}

void R_LoadLightGridArray( lump_t *l, world_t* w, byte* fileBase) {

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

typedef byte color4ub_t[4];
color4ub_t	styleColors[MAX_LIGHT_STYLES] = { 0 };
#define	FOG_TABLE_SIZE		256
#define FUNCTABLE_SIZE		1024
#define FUNCTABLE_SIZE2		10
#define FUNCTABLE_MASK		(FUNCTABLE_SIZE-1)
float					sinTable[FUNCTABLE_SIZE];

static void R_SetupEntityLightingGrid( trRefEntity_t *ent, world_t* world ) {
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

		pos[0] = std::clamp(0, world->lightGridBounds[0] - 1, pos[0]);
		pos[1] = std::clamp(0, world->lightGridBounds[1] - 1, pos[1]);
		pos[2] = std::clamp(0, world->lightGridBounds[2] - 1, pos[2]);

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
				continue;	// ignore samples in walls
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
				continue;	// ignore samples in walls
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

int R_LightDirForPoint(vec3_t point, vec3_t lightDir, vec3_t normal, float* directionality, world_t* world, float normalDotRestrict, float normalDotRestrictLow)
{
	trRefEntity_t ent;
	float dot;

	if (world->lightGridData == NULL)
		return qfalse;

	Com_Memset(&ent, 0, sizeof(ent));
	VectorCopy(point, ent.e.origin);
	R_SetupEntityLightingGrid(&ent, world);

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
	byte* buffer;
	byte* startMarker;

	if (argc <= 2) return 1;

	const char* fileName = argv[1];
	const char* fileNameOut = argv[2];

	resamplingMode_t mode = LINEAR_OVERBRIGHT;

	bool debugLD = false;
	if (argc > 3) {
		if (!stricmp(argv[3],"lin")) {
			mode = LINEAR;
		}
		else if (!stricmp(argv[3],"raw")) {
			mode = RAW;
		}
		else if (!stricmp(argv[3],"olin")) {
			mode = LINEAR_OVERBRIGHT;
		}
		else if (!stricmp(argv[3],"debug")) {
			debugLD = true;
		}
	}
	
	switch (mode) {
		case LINEAR:
			std::cout << "Using linear pixel blending mode.\n";
			break;
		case RAW:
			std::cout << "Using raw pixel blending mode.\n";
			break;
		case LINEAR_OVERBRIGHT:
			std::cout << "Using overbright-linear pixel blending mode (default).\n";
			break;
	}
	std::cout << "Use third parameter 'lin' for linear, 'raw' for raw and 'olin' for overbright-linear mode. Linear is best for games without overbright bits (jka), overbright-linear is best for games with overbright bits (jk2).\n";

	int inputFileLength = FS_ReadFile(fileName, (void**)&buffer, qfalse);
	if (!buffer) {
		Com_Printf("RE_LoadWorldMap: %s not found", fileName);
		return 1;
	}

	//startMarker = (unsigned char*)malloc(sizeof(dheader_t));

	header = (dheader_t*)buffer;
	byte* fileBase = (byte*)header;

	i = LittleLong(header->version);
	if (i != BSP_VERSION) {
		Com_Printf("RE_LoadWorldMap: %s has wrong version number (%i should be %i)",
			fileName, i, BSP_VERSION);
		return 1;
	}

	int lmSize = (LIGHTMAP_WIDTH * LIGHTMAP_HEIGHT * 3);
	int numLightmaps, countOutputLightmaps;
	byte* outputLightmaps;
	byte* outputLightmapUsageInfo;
	{
		lump_t* l = &header->lumps[LUMP_LIGHTMAPS];
		int len = l->filelen;
		if (!len) {
			return 1;
		}
		byte* buf = fileBase + l->fileofs;
		
		numLightmaps = len / (LIGHTMAP_WIDTH * LIGHTMAP_HEIGHT * 3);

		countOutputLightmaps = numLightmaps * 2;

		
		int lmbufsize = (LIGHTMAP_WIDTH * LIGHTMAP_HEIGHT * 3) * countOutputLightmaps;


		// replace file buffer with an appropriately sized one
		byte* tmpBufPtr = (byte*)calloc(inputFileLength + lmbufsize,1); // we make room for the new lightmaps at the end since we can't put them where the old ones are
		memcpy(tmpBufPtr, buffer, inputFileLength); // copy the old file there
		free(buffer);
		buffer = tmpBufPtr; // replacing old file buffer with new one and reiniting the pointers. this will 100% blow up in my face later someday.
		header = (dheader_t*)buffer;
		fileBase = (byte*)header;
		l = &header->lumps[LUMP_LIGHTMAPS];
		buf = fileBase + l->fileofs;

		outputLightmapUsageInfo = (byte*)calloc(lmbufsize, 1);
		outputLightmaps = fileBase + inputFileLength;
		memset(outputLightmaps, 0, lmbufsize);
		for (int i = 0; i < numLightmaps; i++) {
			byte* srcLm = buf + lmSize * i;
			byte* outLm = outputLightmaps + lmSize * i * 2;

			memcpy(outLm, srcLm,lmSize);
		}

		// Null the old lump and overwrite it with reduced size one.
		Com_Memset(buf, 0, l->filelen);
		l->fileofs = inputFileLength; // starts at the previous end of the file
		l->filelen = lmbufsize;
		inputFileLength += lmbufsize;

		/*
		int outBufSize = countOutputLightmaps * (LIGHTMAP_WIDTH * LIGHTMAP_HEIGHT * 3);
		byte* outBuf = (byte*)malloc(outBufSize);

		for (int i = 0; i < numLightmaps; i++) {
			byte* start = buf + i * (LIGHTMAP_WIDTH * LIGHTMAP_HEIGHT * 3);
			int targetLightmap = i / 4;
			int subLightmapnum = i % 4;

			int targetXOffset = (subLightmapnum % 2) * LIGHTMAP_WIDTH / 2;
			int targetYOffset = (subLightmapnum / 2) * LIGHTMAP_HEIGHT / 2;

			byte* outStart = outBuf + targetLightmap * (LIGHTMAP_WIDTH * LIGHTMAP_HEIGHT * 3);
			for (int x = 0; x < LIGHTMAP_WIDTH; x+=2) {
				for (int y = 0; y < LIGHTMAP_HEIGHT; y+=2) {
					byte* pixelPos = start + y * (LIGHTMAP_WIDTH * 3) + x * 3;
					int r, g, b;
					if (mode == RAW) {
						r = (pixelPos[0] + pixelPos[3] + pixelPos[LIGHTMAP_WIDTH * 3] + pixelPos[LIGHTMAP_WIDTH * 3 + 3]) / 4; // Averaging the values of 4 pixels for one target pixel.
						pixelPos++;
						g = (pixelPos[0] + pixelPos[3] + pixelPos[LIGHTMAP_WIDTH * 3] + pixelPos[LIGHTMAP_WIDTH * 3 + 3]) / 4; // Averaging the values of 4 pixels for one target pixel.
						pixelPos++;
						b = (pixelPos[0] + pixelPos[3] + pixelPos[LIGHTMAP_WIDTH * 3] + pixelPos[LIGHTMAP_WIDTH * 3 + 3]) / 4; // Averaging the values of 4 pixels for one target pixel.
					}
					else if (mode == LINEAR) {
						r = fromLinear((toLinear(pixelPos[0],false) + toLinear(pixelPos[3], false) + toLinear(pixelPos[LIGHTMAP_WIDTH * 3], false) + toLinear(pixelPos[LIGHTMAP_WIDTH * 3 + 3], false)) / 4.f,false); // Averaging the 4 linearized float values for one target pixel
						pixelPos++;
						g = fromLinear((toLinear(pixelPos[0], false) + toLinear(pixelPos[3], false) + toLinear(pixelPos[LIGHTMAP_WIDTH * 3], false) + toLinear(pixelPos[LIGHTMAP_WIDTH * 3 + 3], false)) / 4.f, false); // Averaging the 4 linearized float values for one target pixel
						pixelPos++;
						b = fromLinear((toLinear(pixelPos[0], false) + toLinear(pixelPos[3], false) + toLinear(pixelPos[LIGHTMAP_WIDTH * 3], false) + toLinear(pixelPos[LIGHTMAP_WIDTH * 3 + 3], false)) / 4.f, false); // Averaging the 4 linearized float values for one target pixel
					}
					else if (mode == LINEAR_OVERBRIGHT) {
						r = fromLinear((toLinear(pixelPos[0], true) + toLinear(pixelPos[3], true) + toLinear(pixelPos[LIGHTMAP_WIDTH * 3], true) + toLinear(pixelPos[LIGHTMAP_WIDTH * 3 + 3], true)) / 4.f,true); // Averaging the 4 linearized float values for one target pixel
						pixelPos++;
						g = fromLinear((toLinear(pixelPos[0], true) + toLinear(pixelPos[3], true) + toLinear(pixelPos[LIGHTMAP_WIDTH * 3], true) + toLinear(pixelPos[LIGHTMAP_WIDTH * 3 + 3], true)) / 4.f, true); // Averaging the 4 linearized float values for one target pixel
						pixelPos++;
						b = fromLinear((toLinear(pixelPos[0], true) + toLinear(pixelPos[3], true) + toLinear(pixelPos[LIGHTMAP_WIDTH * 3], true) + toLinear(pixelPos[LIGHTMAP_WIDTH * 3 + 3], true)) / 4.f, true); // Averaging the 4 linearized float values for one target pixel
					}

					int targetX = targetXOffset + x / 2;
					int targetY = targetYOffset + y / 2;
					byte* outPixelPos = outStart + targetY * (LIGHTMAP_WIDTH * 3) + targetX * 3;
					outPixelPos[0] = r;
					outPixelPos[1] = g;
					outPixelPos[2] = b;
				}
			}
		}
		
		// Null the old lump and overwrite it with reduced size one.
		Com_Memset(buf, 0, l->filelen);
		Com_Memcpy(buf, outBuf, outBufSize);
		header->lumps[LUMP_LIGHTMAPS].filelen = outBufSize;
		*/

	}


	{
		lump_t* l = &header->lumps[LUMP_SURFACES];
		lump_t* lV = &header->lumps[LUMP_DRAWVERTS];
		lump_t* lS = &header->lumps[LUMP_SHADERS];
		lump_t* lI = &header->lumps[LUMP_DRAWINDEXES];
		lump_t* lm = &header->lumps[LUMP_MODELS];
		lump_t* llG = &header->lumps[LUMP_LIGHTGRID];
		lump_t* llA = &header->lumps[LUMP_LIGHTARRAY];
		lump_t* lE = &header->lumps[LUMP_ENTITIES];
		int len = l->filelen;
		int lenV = lV->filelen;
		int lenI = lI->filelen;
		int lenS = lS->filelen;
		int lenM = lm->filelen;
		if (!len || !lenV || !lenI|| !lenS || !lenM) {
			return 1;
		}
		byte* buf = fileBase + l->fileofs;
		byte* bufV = fileBase + lV->fileofs;
		byte* bufI = fileBase + lI->fileofs;
		byte* bufS = fileBase + lS->fileofs;
		byte* bufM = fileBase + lm->fileofs;
		dsurface_t* surfAsArray = (dsurface_t*)buf;
		mapVert_t* vertAsArray = (mapVert_t*)bufV;
		dshader_t* shadersAsArray = (dshader_t*)bufS;
		dmodel_t* submodelsAsArray = (dmodel_t*)bufM;

		world_t world{ 0 };
		// get world bounds for lightgrid
		for (int j = 0; j < 3; j++) {
			world.bmodels[0].bounds[0][j] = submodelsAsArray->mins[j];
			world.bmodels[0].bounds[1][j] = submodelsAsArray->maxs[j];
		}
		R_InitFunctionTables();
		R_LoadEntities(lE,&world,fileBase);
		R_LoadLightGrid(llG, &world,fileBase);
		R_LoadLightGridArray(llA, &world,fileBase);

		int* indexesAsArray = (int*)bufI;

		int numSurfaces = len / sizeof(dsurface_t);

		int tries = 0, missesXYZ = 0, missesST =0, missesXYZReal = 0, missesSTReal =0;
		vec3_t pixelXYZ[128*128]{ 0 };
		for (int i = 0; i < numSurfaces; i++) {
			dsurface_t* surf = &surfAsArray[i];
			dshader_t* shader = shadersAsArray + surf->shaderNum;
			for (int l = 0; l < MAXLIGHTMAPS; l++) {
				int lightmapNumOriginal = surf->lightmapNum[l];
				if (lightmapNumOriginal < 0) continue;
				//int targetLightmap = lightmapNumOriginal / 4;
				//int subLightmapnum = lightmapNumOriginal % 4;
				//int targetXOffset = (subLightmapnum % 2) * LIGHTMAP_WIDTH / 2;
				//int targetYOffset = (subLightmapnum / 2) * LIGHTMAP_HEIGHT / 2;
				//float targetXOffsetV = (float)(subLightmapnum % 2) * 1.f / 2.f;
				//float targetYOffsetV = (float)(subLightmapnum / 2) * 1.f / 2.f;

				surf->lightmapNum[l] = lightmapNumOriginal * 2;
			}
			if (surf->surfaceType != MST_PLANAR) {
				continue;
			}
			for (int l = 0; l < MAXLIGHTMAPS; l++) {
				int lightmapNumOriginal = surf->lightmapNum[l];
				if (lightmapNumOriginal < 0) continue;
				//int targetLightmap = lightmapNumOriginal / 4;
				//int subLightmapnum = lightmapNumOriginal % 4;
				//int targetXOffset = (subLightmapnum % 2) * LIGHTMAP_WIDTH / 2;
				//int targetYOffset = (subLightmapnum / 2) * LIGHTMAP_HEIGHT / 2;
				//float targetXOffsetV = (float)(subLightmapnum % 2) * 1.f / 2.f;
				//float targetYOffsetV = (float)(subLightmapnum / 2) * 1.f / 2.f;

				//surf->lightmapNum[l] = lightmapNumOriginal*2;
				//int targetX = targetXOffset + surf->lightmapX[l] / 2;
				//int targetY = targetYOffset + surf->lightmapY[l] / 2;
				//surf->lightmapX[l] = targetX;
				//surf->lightmapY[l] = targetY;
				//surf->lightmapWidth = surf->lightmapWidth / 2;
				//surf->lightmapHeight = surf->lightmapHeight / 2;

				//for (int v = 0; v < surf->numVerts; v++) {
				for (int idx = 0; idx < surf->numIndexes; idx++) {
					//mapVert_t* vert = &vertAsArray[surf->firstVert + v];
					//vert->lightmap[l][0] = targetXOffsetV + vert->lightmap[l][0] / 2.0f;
					//vert->lightmap[l][1] = targetYOffsetV + vert->lightmap[l][1] / 2.0f;
					if (((idx + 1) % 3)) {
						continue;
					}
					;
					mapVert_t* vert[3] = {
						&vertAsArray[surf->firstVert + indexesAsArray[surf->firstIndex + idx - 2]],
						&vertAsArray[surf->firstVert + indexesAsArray[surf->firstIndex + idx - 1]],
						&vertAsArray[surf->firstVert + indexesAsArray[surf->firstIndex + idx]],
					};

					vec3_t calcedNormal,fullSizeNormal;
					vec3_t side1, side2;
					VectorSubtract(vert[2]->xyz, vert[1]->xyz, side1);
					VectorSubtract(vert[2]->xyz, vert[0]->xyz, side2);
					CrossProduct(side1, side2, fullSizeNormal);
					VectorCopy(fullSizeNormal, calcedNormal);
					float triangleSize = 0.5f*VectorNormalize(calcedNormal);

					byte triangleSizeSqrtIndicator = triangleSize == 0 ? 0 : 0.1f*sqrtf(triangleSize);

					// barycentric matrix stuff
					float triangleSizeyTimes2Squared = DotProduct(fullSizeNormal, fullSizeNormal);
					vec3_t helper1, helper2;
					float scaler = 1.0f / triangleSizeyTimes2Squared;
					CrossProduct(side1,fullSizeNormal,helper1);
					VectorScale(helper1, scaler, helper1);
					CrossProduct(fullSizeNormal,side2,helper2);
					VectorScale(helper2, scaler, helper2);
					float baryCentricMatrix[16] = {
						-helper1[0]-helper2[0], -helper1[1]-helper2[1], -helper1[2]-helper2[2], 1.0f + DotProduct(helper1,vert[2]->xyz) + DotProduct(helper2,vert[2]->xyz),
						helper1[0],helper1[1],helper1[2], -DotProduct(helper1,vert[2]->xyz),
						helper2[0],helper2[1],helper2[2], -DotProduct(helper2,vert[2]->xyz),
						0,0,0,1
					};

					vec3_t bary[3];
					for (int j = 0; j < 3; j++) {
						applyMatrix(vert[j]->xyz, baryCentricMatrix,bary[j]);
					}


					float uvTransformMatrix[16] = { 0 };
					float uvTransformMatrixInverted[16] = { 0 };
					float uvTransformMatrixPseudoInverted[16] = { 0 };

					//vec3_t uvTransformMatrix[2];
					//uvTransformMatrix[3] = uvTransformMatrix[7] = uvTransformMatrix[11] = uvTransformMatrix[15] = 1.0f;
					uvTransformMatrix[15] = 1.0f;
					makeUVTransformationMatrixSafe(vert[0]->xyz, vert[0]->lightmap[l], vert[1]->xyz, vert[1]->lightmap[l], vert[2]->xyz, vert[2]->lightmap[l], calcedNormal, uvTransformMatrix);

					memcpy(uvTransformMatrixPseudoInverted, uvTransformMatrix, sizeof(uvTransformMatrixPseudoInverted));


					cimg_library::CImg<float> A(4, 4);
					for (int k = 0; k < 16; k++) {
						A[k] = uvTransformMatrixPseudoInverted[k];
					}
					//cimg_library::CImg<float> pseudo_inv2= ((A * A.get_transpose()).invert()) * A.get_transpose();
					cimg_library::CImg<float> pseudo_inv2= A.invert();
					for (int k = 0; k < 16; k++) {
						uvTransformMatrixPseudoInverted[k] = pseudo_inv2[k];
					}

					//pinv(uvTransformMatrixPseudoInverted, 4, 4);
					__gluInvertMatrixfRowMajor(uvTransformMatrix, uvTransformMatrixInverted);

					bool good = true;

					float planedist;
					for (int j = 0; j < 3;j++) {
						vec3_t st = { 0 }, stOriginal = { 0 };
						vec3_t xyz, xyz2, xyzOriginal;
						stOriginal[0] = vert[j]->lightmap[l][0];
						stOriginal[1] = vert[j]->lightmap[l][1];

						applyMatrix(vert[j]->xyz, uvTransformMatrix, st);
						//st[0] = DotProduct(vert[j]->xyz,&uvTransformMatrix[0]);
						//st[1] = DotProduct(vert[j]->xyz,&uvTransformMatrix[4]);

						planedist = DotProduct(calcedNormal,vert[j]->xyz);
						stOriginal[2] = planedist;
						VectorCopy(vert[j]->xyz, xyzOriginal);

						applyMatrix(stOriginal, uvTransformMatrixInverted, xyz);
						//xyz[0] = DotProduct(stOriginal, &uvTransformMatrixInverted[0]);
						//xyz[1] = DotProduct(stOriginal, &uvTransformMatrixInverted[4]);
						//xyz[2] = DotProduct(stOriginal, &uvTransformMatrixInverted[8]);
						
						//xyz2[0] = DotProduct(stOriginal, &uvTransformMatrixPseudoInverted[0]);
						//xyz2[1] = DotProduct(stOriginal, &uvTransformMatrixPseudoInverted[4]);
						//xyz2[2] = DotProduct(stOriginal, &uvTransformMatrixPseudoInverted[8]);

						tries++;
						float stdist = sqrtf((st[0] - stOriginal[0]) * (st[0] - stOriginal[0]) + (st[1] - stOriginal[1]) * (st[1] - stOriginal[1]));
						float xyzdist = VectorDistance(xyz,xyzOriginal);
						if (stdist > 0.2f) {
							missesST++;
							good = false;
							if (triangleSize >= 100.0) {
								missesSTReal++;
								//Com_Printf("st;  original: %.3f %.3f, matrix result: %.3f %.3f\n", stOriginal[0], stOriginal[1], st[0], st[1]);
							}
						}
						if (xyzdist > 200.0f) {
							missesXYZ++;
							good = false;
							if (triangleSize >= 100.0) {
								missesXYZReal++;
								//Com_Printf("xyz; original: %.3f %.3f %.3f, matrix result: %.3f %.3f %.3f\n", xyzOriginal[0], xyzOriginal[1], xyzOriginal[2], xyz[0], xyz[1], xyz[2]);
							}
						}


					}

					if (!good && triangleSize > 400.0f) {
						Com_Printf("bad big tri :( %f ( %f %f %f ) ( %f %f %f ) ( %f %f %f )\n", triangleSize,
							vert[0]->xyz[0],vert[0]->xyz[1],vert[0]->xyz[1],
							vert[1]->xyz[0],vert[1]->xyz[1],vert[1]->xyz[1],
							vert[2]->xyz[0],vert[2]->xyz[1],vert[2]->xyz[1]
						);
					}

					if (good || debugLD) 
					do {
						vec2_t uvMax, uvMin;
						uvMin[0] = std::min(std::min(vert[0]->lightmap[l][0], vert[1]->lightmap[l][0]), vert[2]->lightmap[l][0]);
						uvMin[1] = std::min(std::min(vert[0]->lightmap[l][1], vert[1]->lightmap[l][1]), vert[2]->lightmap[l][1]);
						uvMax[0] = std::max(std::max(vert[0]->lightmap[l][0], vert[1]->lightmap[l][0]), vert[2]->lightmap[l][0]);
						uvMax[1] = std::max(std::max(vert[0]->lightmap[l][1], vert[1]->lightmap[l][1]), vert[2]->lightmap[l][1]);
						vec2i_t lmMin, lmMax;
						lmMin[0] = std::clamp(uvMin[0] * 128.0f, 0.0f, 128.0f-1.0f);
						lmMin[1] = std::clamp(uvMin[1] * 128.0f, 0.0f, 128.0f-1.0f);
						lmMax[0] = std::ceil(std::clamp(uvMax[0] * 128.0f, 0.0f, 128.0f-1.0f)) + 0.5f;
						lmMax[1] = std::ceil(std::clamp(uvMax[1] * 128.0f, 0.0f, 128.0f-1.0f)) + 0.5f;

						int lmOffset = debugLD ? 0 : 1;
						byte* lm = outputLightmapUsageInfo + lmSize * (lightmapNumOriginal + lmOffset);
						byte* lmReal = outputLightmaps + lmSize * (lightmapNumOriginal + lmOffset);
#define ACCURATE_CLIPPING 1
#if ACCURATE_CLIPPING
						for (int x = lmMin[0]; x <= lmMax[0]; x++) {
							for (int y = lmMin[1]; y <= lmMax[1]; y++) {
								if (debugLD && !good && !lm[y * 128 * 3 + x * 3 + 2]) {
									lmReal[y * 128 * 3 + x * 3 + 0] = 0;
									lmReal[y * 128 * 3 + x * 3 + 1] = 0;
									lmReal[y * 128 * 3 + x * 3 + 2] = triangleSizeSqrtIndicator;
									continue;
								}

								vec3_t pixelUv;
								vec3_t xyz, bary;
								pixelUv[0] = ((float)x + 0.5f) / 128.0f;
								pixelUv[1] = ((float)y + 0.5f) / 128.0f;
								pixelUv[2] = planedist;

								applyMatrix(pixelUv, uvTransformMatrixInverted, xyz);
								VectorCopy(xyz, pixelXYZ[y * 128 + x]);
								applyMatrix(xyz, baryCentricMatrix, bary);


								bool insideTriangle = bary[0] >= 0.0 && bary[1] >= 0.0 && bary[2] >= 0.0;


								const float uvHalfPixOffset = -0.5f / 128.0f;
								const vec3_t cornerUvOffsets[4] = {
									{-uvHalfPixOffset,-uvHalfPixOffset,0.0f},
									{uvHalfPixOffset,-uvHalfPixOffset,0.0f},
									{uvHalfPixOffset,uvHalfPixOffset,0.0f},
									{-uvHalfPixOffset,uvHalfPixOffset,0.0f},
								};
								vec3_t tmp, tmp2;
								vec3_t cornerBary[4];
								if (!insideTriangle) {
									// center not inside triangle. check the 4 corners of the pixel and save them
									for (int corner = 0; corner < 4; corner++) {
										VectorAdd(pixelUv, cornerUvOffsets[corner], tmp);
										applyMatrix(tmp, uvTransformMatrixInverted, tmp2);
										applyMatrix(tmp2, baryCentricMatrix, cornerBary[corner]);

										if (cornerBary[corner][0] < 0.0 || cornerBary[corner][1] < 0.0 || cornerBary[corner][2] < 0.0) {
											continue;
										}
										insideTriangle = true;
									}
								}

								if (!insideTriangle) {
									const int pixelBordersCorners[4][2] = {
										{0,1},
										{1,2},
										{2,3},
										{3,0},
									};
									// corners werent inside the triangle either. so check each pixel border
									// for intersection with a barycentric edge, then lerp towards that edge and check if its
									// inside the triangle.
									// this will not cover the whole triangle being inside the pixel and off-center (fuck it who cares)
									for (int border = 0; border < 4 && !insideTriangle; border++) {
										vec_t* point1 = cornerBary[pixelBordersCorners[border][0]];
										vec_t* point2 = cornerBary[pixelBordersCorners[border][1]];

										for (int dim = 0; dim < 3; dim++) {
											if (point1[dim] >= 0 != point2[dim] >= 0) {
												float lerp = point1[dim] / (point1[dim] - point2[dim]);
												vec3_t lerpBary;
												VectorLerp(lerp,point1,point2,lerpBary);
												if (point1[(dim+1)%3] >= 0 && point2[(dim + 1) % 3] >= 0) {
													insideTriangle = true;
													break;
												}
											}
										}
									}
								}

								if (!insideTriangle) {
									continue;
								}



								lm[y * 128 * 3 + x * 3 + 1] = 255;
							}
						}
						if (!good) {
							break;
						}
#else
						for (int x = lmMin[0]; x <= lmMax[0];x++) {
							for (int y = lmMin[1]; y <= lmMax[1]; y++) {
								if (debugLD && !good && !lm[y * 128 * 3 + x * 3 + 2]) {
									lmReal[y * 128 * 3 + x * 3 + 0] = 0;
									lmReal[y * 128 * 3 + x * 3 + 1] = 0;
									lmReal[y * 128 * 3 + x * 3 + 2] = triangleSizeSqrtIndicator;
									continue;
								}

								vec3_t pixelUv;
								vec3_t xyz, bary;
								pixelUv[0] = ((float)x + 0.5f) / 128.0f;
								pixelUv[1] = ((float)y + 0.5f) / 128.0f;
								pixelUv[2] = planedist;

								applyMatrix(pixelUv, uvTransformMatrixInverted, xyz);
								VectorCopy(xyz, pixelXYZ[y * 128 + x]);
								applyMatrix(xyz, baryCentricMatrix, bary);

								if (bary[0] < 0.0 || bary[1] < 0.0 || bary[2] < 0.0) {
									continue;
								} 
								lm[y * 128 * 3 + x * 3] = 255;
							}
						}
						if (!good) {
							break;
						}
						for (int x = lmMin[0]; x <= lmMax[0]; x++) {
							for (int y = lmMin[1]; y <= lmMax[1]; y++) {

								for (int xx = std::max(x,0); xx < std::min(127,lmMax[0]+1); xx++) {
									for (int yy = std::max(lmMin[1], 0); yy < std::min(127, lmMax[1]); yy++) {
										if (lm[yy * 128 * 3 + xx * 3]) {
											lm[y * 128 * 3 + x * 3 + 1] = 255;
										}
									}
								}
							}
						}
#endif
						for (int x = lmMin[0]; x <= lmMax[0]; x++) {
							for (int y = lmMin[1]; y <= lmMax[1]; y++) {
								if (!lm[y * 128 * 3 + x * 3 + 1] || lm[y * 128 * 3 + x * 3 + 2] > triangleSizeSqrtIndicator) {
									// pixel already written to by a bigger triangle (is that a good criterion?) or not inside triangle
									continue;
								}
								vec3_t sampleLocation;
								vec3_t lightDir;

								VectorCopy(pixelXYZ[y * 128 + x], sampleLocation);
								// push the sample location so it's definitely above the surface.
								float height = DotProduct(sampleLocation, calcedNormal) - planedist;
								float newHeight = height;
								if (height < 0) {
									if (height < -100) {
										Com_Printf("very far off :( %.3f\n",height);
									}
									VectorMA(sampleLocation, -height + 1.0f, calcedNormal, sampleLocation); 
									newHeight = DotProduct(sampleLocation, calcedNormal) - planedist;
								}
								R_LightDirForPoint(sampleLocation, lightDir, calcedNormal, NULL, &world, 0.2, 0.0 );
								VectorNormalize(lightDir);

								if (!lightDir[0] && !lightDir[1] && !lightDir[2]) {
									Com_Printf("dead lights :(\n");
								}

								vec3_t surfaceLightDir;
								if(deluxemode == 1){ // tangentspace shizzle
									// as in netradiant custom
									const vec3_t g_vector3_axis_x{ 1, 0, 0 };
									const vec3_t g_vector3_axis_y{ 0, 1, 0 };
									const vec3_t g_vector3_axis_z{ 0, 0, 1 };
									vec3_t myTangent{ 0 }, myBinormal{ 0 };
									if (calcedNormal[0] == 0 && calcedNormal[1] == 0.0f) {
										if (calcedNormal[2] == 1.0f) {
											VectorCopy(g_vector3_axis_x,myTangent);
											VectorCopy(g_vector3_axis_y, myBinormal);
										}
										else if (calcedNormal[2] == -1.0f) {
											VectorScale(g_vector3_axis_x,-1.0f, myTangent);
											VectorCopy(g_vector3_axis_y, myBinormal);
										}
									}
									else {
										CrossProduct(calcedNormal, g_vector3_axis_z,myTangent);
										VectorNormalize(myTangent);
										CrossProduct(myTangent, calcedNormal, myBinormal);
										VectorNormalize(myBinormal);
									}

									float somedot = DotProduct(myTangent, calcedNormal);
									VectorMA(myTangent,-somedot,calcedNormal,myTangent);
									somedot = DotProduct(myBinormal, calcedNormal);
									VectorMA(myBinormal,-somedot,calcedNormal, myBinormal);

									VectorNormalize(myTangent);
									VectorNormalize(myBinormal);

									if (calcedNormal[0] > 0 || calcedNormal[1] < 0 || calcedNormal[2] < 0) {
										VectorNegate(myTangent, myTangent);
									}
									surfaceLightDir[0] = DotProduct(lightDir, myTangent);
									surfaceLightDir[1] = DotProduct(lightDir, myBinormal);
									surfaceLightDir[2] = DotProduct(lightDir, calcedNormal);
								}
								else {
									VectorCopy(lightDir, surfaceLightDir);
								}

								lmReal[y * 128 * 3 + x * 3 + 0] = std::clamp((surfaceLightDir[0]*0.5f+0.5f)*256.0f,0.0f,255.0f);
								lmReal[y * 128 * 3 + x * 3 + 1] = std::clamp((surfaceLightDir[1]*0.5f+0.5f)*256.0f,0.0f,255.0f);
								lmReal[y * 128 * 3 + x * 3 + 2] = std::clamp((surfaceLightDir[2]*0.5f+0.5f)*256.0f,0.0f,255.0f);
								lm[y * 128 * 3 + x * 3 + 2] = triangleSizeSqrtIndicator;
							}
						}
					} while (0);

				}
			}
		}
	
		Com_Printf("tries: %d, misses(ST): %d, misses(XYZ): %d\n",tries,missesST,missesXYZ);
		Com_Printf("misses(STReal): %d, misses(XYZReal): %d\n",missesSTReal,missesXYZReal);
	}

	FS_WriteFile(fileNameOut, buffer, inputFileLength);


}