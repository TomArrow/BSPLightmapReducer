#include "common.h"


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

	startMarker = (unsigned char*)malloc(sizeof(dheader_t));

	header = (dheader_t*)buffer;
	byte* fileBase = (byte*)header;

	i = LittleLong(header->version);
	if (i != BSP_VERSION) {
		Com_Printf("RE_LoadWorldMap: %s has wrong version number (%i should be %i)",
			fileName, i, BSP_VERSION);
		return 1;
	}

	{
		lump_t* l = &header->lumps[LUMP_LIGHTMAPS];
		int len = l->filelen;
		if (!len) {
			return 1;
		}
		byte* buf = fileBase + l->fileofs;

		int numLightmaps = len / (LIGHTMAP_WIDTH * LIGHTMAP_HEIGHT * 3);

		int countOutputLightmaps = (numLightmaps / 4 * 4) < numLightmaps ? numLightmaps / 4 + 1 : numLightmaps /4;

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


	}

	{
		lump_t* l = &header->lumps[LUMP_SURFACES];
		lump_t* lV = &header->lumps[LUMP_DRAWVERTS];
		int len = l->filelen;
		int lenV = lV->filelen;
		if (!len || !lenV) {
			return 1;
		}
		byte* buf = fileBase + l->fileofs;
		byte* bufV = fileBase + lV->fileofs;
		dsurface_t* surfAsArray = (dsurface_t*)buf;
		mapVert_t* vertAsArray = (mapVert_t*)bufV;

		int numSurfaces = len / sizeof(dsurface_t);

		for (int i = 0; i < numSurfaces; i++) {
			dsurface_t* surf = &surfAsArray[i];
			for (int l = 0; l < MAXLIGHTMAPS; l++) {
				int lightmapNumOriginal = surf->lightmapNum[l];
				if (lightmapNumOriginal < 0) continue;
				int targetLightmap = lightmapNumOriginal / 4;
				int subLightmapnum = lightmapNumOriginal % 4;
				int targetXOffset = (subLightmapnum % 2) * LIGHTMAP_WIDTH / 2;
				int targetYOffset = (subLightmapnum / 2) * LIGHTMAP_HEIGHT / 2;
				float targetXOffsetV = (float)(subLightmapnum % 2) * 1.f / 2.f;
				float targetYOffsetV = (float)(subLightmapnum / 2) * 1.f / 2.f;

				surf->lightmapNum[l] = lightmapNumOriginal / 4;
				int targetX = targetXOffset + surf->lightmapX[l] / 2;
				int targetY = targetYOffset + surf->lightmapY[l] / 2;
				surf->lightmapX[l] = targetX;
				surf->lightmapY[l] = targetY;
				surf->lightmapWidth = surf->lightmapWidth / 2;
				surf->lightmapHeight = surf->lightmapHeight / 2;

				for (int v = 0; v < surf->numVerts; v++) {
					mapVert_t* vert = &vertAsArray[surf->firstVert + v];
					vert->lightmap[l][0] = targetXOffsetV + vert->lightmap[l][0] / 2.0f;
					vert->lightmap[l][1] = targetYOffsetV + vert->lightmap[l][1] / 2.0f;
				}
			}
		}
	}

	FS_WriteFile(fileNameOut, buffer, inputFileLength);


}