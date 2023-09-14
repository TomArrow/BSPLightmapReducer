#include "common.h"


int main(int argc, char** argv) {
	int			i;
	dheader_t* header;
	byte* buffer;
	byte* startMarker;

	if (argc <= 2) return 1;

	const char* fileName = argv[1];
	const char* fileNameOut = argv[2];

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
					int r = (pixelPos[0]+pixelPos[3]+pixelPos[LIGHTMAP_WIDTH * 3]+pixelPos[LIGHTMAP_WIDTH * 3+3])/4; // Averaging the values of 4 pixels for one target pixel.
					pixelPos++;
					int g = (pixelPos[0]+pixelPos[3]+pixelPos[LIGHTMAP_WIDTH * 3]+pixelPos[LIGHTMAP_WIDTH * 3+3])/4; // Averaging the values of 4 pixels for one target pixel.
					pixelPos++;
					int b = (pixelPos[0]+pixelPos[3]+pixelPos[LIGHTMAP_WIDTH * 3]+pixelPos[LIGHTMAP_WIDTH * 3+3])/4; // Averaging the values of 4 pixels for one target pixel.

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