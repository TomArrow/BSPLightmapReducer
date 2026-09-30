#include "common.h"
//#define cimg_display 0
//#include "../ext/CImg.h"

// project for just playing around and testing concepts

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
		/*
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
		*/

	}

	{
		lump_t* l = &header->lumps[LUMP_SURFACES];
		lump_t* lV = &header->lumps[LUMP_DRAWVERTS];
		lump_t* lS = &header->lumps[LUMP_SHADERS];
		lump_t* lI = &header->lumps[LUMP_DRAWINDEXES];
		int len = l->filelen;
		int lenV = lV->filelen;
		int lenI = lI->filelen;
		int lenS = lS->filelen;
		if (!len || !lenV || !lenI|| !lenS) {
			return 1;
		}
		byte* buf = fileBase + l->fileofs;
		byte* bufV = fileBase + lV->fileofs;
		byte* bufI = fileBase + lI->fileofs;
		byte* bufS = fileBase + lS->fileofs;
		dsurface_t* surfAsArray = (dsurface_t*)buf;
		mapVert_t* vertAsArray = (mapVert_t*)bufV;
		dshader_t* shadersAsArray = (dshader_t*)bufS;
		int* indexesAsArray = (int*)bufI;

		int numSurfaces = len / sizeof(dsurface_t);

		int tries = 0, missesXYZ = 0, missesST =0, missesXYZReal = 0, missesSTReal =0;
		for (int i = 0; i < numSurfaces; i++) {
			dsurface_t* surf = &surfAsArray[i];
			if (surf->surfaceType != MST_PLANAR) {
				continue;
			}
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

				//surf->lightmapNum[l] = lightmapNumOriginal / 4;
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

					vec3_t calcedNormal;
					vec3_t side1, side2;
					VectorSubtract(vert[2]->xyz, vert[1]->xyz, side1);
					VectorSubtract(vert[2]->xyz, vert[0]->xyz, side2);
					CrossProduct(side1, side2, calcedNormal);
					float triangleSize = 0.5f*VectorNormalize(calcedNormal);

					float uvTransformMatrix[16] = { 0 };
					float uvTransformMatrixInverted[16] = { 0 };
					float uvTransformMatrixPseudoInverted[16] = { 0 };

					//vec3_t uvTransformMatrix[2];
					//uvTransformMatrix[3] = uvTransformMatrix[7] = uvTransformMatrix[11] = uvTransformMatrix[15] = 1.0f;
					uvTransformMatrix[15] = 1.0f;
					makeUVTransformationMatrix(vert[0]->xyz, vert[0]->lightmap[l], vert[1]->xyz, vert[1]->lightmap[l], vert[2]->xyz, vert[2]->lightmap[l], calcedNormal, uvTransformMatrix);

					memcpy(uvTransformMatrixPseudoInverted, uvTransformMatrix, sizeof(uvTransformMatrixPseudoInverted));


					//cimg_library::CImg<float> A(4, 4);
					//for (int k = 0; k < 16; k++) {
					//	A[k] = uvTransformMatrixPseudoInverted[k];
					//}
					////cimg_library::CImg<float> pseudo_inv2= ((A * A.get_transpose()).invert()) * A.get_transpose();
					//cimg_library::CImg<float> pseudo_inv2= A.invert();
					//for (int k = 0; k < 16; k++) {
					//	uvTransformMatrixPseudoInverted[k] = pseudo_inv2[k];
					//}

					//pinv(uvTransformMatrixPseudoInverted, 4, 4);
					__gluInvertMatrixfRowMajor(uvTransformMatrix, uvTransformMatrixInverted);

					for (int j = 0; j < 3;j++) {
						vec3_t st = { 0 }, stOriginal = { 0 };
						float planedist;
						vec3_t xyz, xyz2, xyzOriginal;
						stOriginal[0] = vert[j]->lightmap[l][0];
						stOriginal[1] = vert[j]->lightmap[l][1];
						st[0] = DotProduct(vert[j]->xyz,&uvTransformMatrix[0]);
						st[1] = DotProduct(vert[j]->xyz,&uvTransformMatrix[4]);

						planedist = DotProduct(calcedNormal,vert[j]->xyz);
						stOriginal[2] = planedist;
						VectorCopy(vert[j]->xyz, xyzOriginal);

						xyz[0] = DotProduct(stOriginal, &uvTransformMatrixInverted[0]);
						xyz[1] = DotProduct(stOriginal, &uvTransformMatrixInverted[4]);
						xyz[2] = DotProduct(stOriginal, &uvTransformMatrixInverted[8]);
						
						xyz2[0] = DotProduct(stOriginal, &uvTransformMatrixPseudoInverted[0]);
						xyz2[1] = DotProduct(stOriginal, &uvTransformMatrixPseudoInverted[4]);
						xyz2[2] = DotProduct(stOriginal, &uvTransformMatrixPseudoInverted[8]);

						tries++;
						float stdist = sqrtf((st[0] - stOriginal[0]) * (st[0] - stOriginal[0]) + (st[1] - stOriginal[1]) * (st[1] - stOriginal[1]));
						float xyzdist = VectorDistance(xyz2,xyzOriginal);
						if (stdist > 0.2f) {
							missesST++;
							if (triangleSize >= 100.0) {
								missesSTReal++;
								//Com_Printf("st;  original: %.3f %.3f, matrix result: %.3f %.3f\n", stOriginal[0], stOriginal[1], st[0], st[1]);
							}
						}
						if (xyzdist > 200.0f) {
							missesXYZ++;
							if (triangleSize >= 100.0) {
								missesXYZReal++;
								//Com_Printf("xyz; original: %.3f %.3f %.3f, matrix result: %.3f %.3f %.3f\n", xyzOriginal[0], xyzOriginal[1], xyzOriginal[2], xyz[0], xyz[1], xyz[2]);
							}
						}
					}

				}
			}
		}
	
		Com_Printf("tries: %d, misses(ST): %d, misses(XYZ): %d\n",tries,missesST,missesXYZ);
		Com_Printf("misses(STReal): %d, misses(XYZReal): %d\n",missesSTReal,missesXYZReal);
	}

	FS_WriteFile(fileNameOut, buffer, inputFileLength);


}