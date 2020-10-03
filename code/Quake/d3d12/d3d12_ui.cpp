// d3d12_ui.cpp
//

#include "d3d12_local.h"
#include "nv_helpers_dx12/BottomLevelASGenerator.h"
#include "nv_helpers_dx12/TopLevelASGenerator.h"
#include <vector>

#define MAX_LOADED_TEXTURES			10000

extern byte* uiTextureBuffer;

struct glTexture_t {
	int width;
	int height;
	byte* data;

	glTexture_t() {
		width = -1;
		height = -1;
		data = NULL;
	}
};



glTexture_t textures[MAX_LOADED_TEXTURES];

/*
==============
R_CopyImage
==============
*/
void R_CopyImage(byte* source, int sourceX, int sourceY, int sourceWidth, byte* dest, int destX, int destY, int destWidth, int width, int height)
{
	for (int y = 0; y < height; y++)
	{
		int _x = 0;
		int _y = y * 4;
		int destPos = (destWidth * (_y + (destY * 4))) + (_x + (destX * 4));
		int sourcePos = (sourceWidth * (_y + (sourceY * 4))) + (_x + (sourceX * 4));

		memcpy(&dest[destPos], &source[sourcePos], width * 4);		
	}
}

/*
=================
GL_BlitUIImage
=================
*/
void GL_BlitUIImage(int texnum, int srcx, int srcy, int destx, int desty) {
	byte* src = textures[texnum].data;
	int width = textures[texnum].width; 
	int height = textures[texnum].height;
	if (destx < 0 || desty < 0 || destx + width > g_width || desty + height > g_height)
		return;

	R_CopyImage(src, srcx, srcy, width, (byte *)uiTextureBuffer, destx, desty, g_width, width, height);
}

/*
=================
GL_BlitUIImage
=================
*/
void GL_BlitUIImageUV(int texnum, float u, float v, int destx, int desty, int w, int h) {
	if (destx < 0 || desty < 0 || destx + w > g_width || desty + h > g_height)
		return;

	byte* src = textures[texnum].data;
	int width = textures[texnum].width;
	int height = textures[texnum].height;
	R_CopyImage(src, u * width, v * height, width, (byte*)uiTextureBuffer, destx, desty, g_width, w, h);
}

/*
===============
GL_Upload32
===============
*/
void GL_Upload32(unsigned* data, int textureId, int width, int height, qboolean mipmap, qboolean alpha)
{
	textures[textureId].width = width;
	textures[textureId].height = height;

	if (textures[textureId].data != NULL)
		delete textures[textureId].data;

	textures[textureId].data = new byte[width * height * 4];
	memcpy(textures[textureId].data, data, width * height * 4);
}