/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2009 John Fitzgibbons and others
Copyright (C) 2007-2008 Kristian Duske
Copyright (C) 2010-2014 QuakeSpasm developers

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#ifndef __GLQUAKE_H
#define __GLQUAKE_H

#include <windows.h>

void GL_BeginRendering(int* x, int* y, int* width, int* height);
void GL_EndRendering(void);


void GL_Init(HWND hwnd, HINSTANCE hinstance, int width, int height);
void* GL_LoadDXRMesh(msurface_t* surfaces, int numSurfaces);

void GL_Upload32(unsigned* data, int textureId, int width, int height, qboolean mipmap, qboolean alpha);

void GL_Set2D (void);

void GL_DumpSceneVertexes(void);

int VID_GetCurrentWidth(void);
int VID_GetCurrentHeight(void);

extern const char* gl_vendor;
extern const char* gl_renderer;
extern const char* gl_version;
extern int gl_version_major;
extern int gl_version_minor;
extern const char* gl_extensions;
extern char* gl_extensions_nice;

extern	int glx, gly, glwidth, glheight;

#define	GL_UNUSED_TEXTURE	(~(GLuint)0)

// r_local.h -- private refresh defs

#define ALIAS_BASE_SIZE_RATIO		(1.0 / 11.0)
					// normalizing factor so player model works out to about
					//  1 pixel per triangle
#define	MAX_LBM_HEIGHT		480

#define TILE_SIZE		128		// size of textures generated by R_GenTiledSurf

#define SKYSHIFT		7
#define	SKYSIZE			(1 << SKYSHIFT)
#define SKYMASK			(SKYSIZE - 1)

#define BACKFACE_EPSILON	0.01

void BuildSurfaceDisplayList(msurface_t* fa);
void GL_FinishDXRLoading(void);
void GL_Render(float x, float y, float z, float* viewAngles);

extern int g_width, g_height;
extern int r_finishDXRInit;

extern qmodel_t* currentmodel;
extern mvertex_t* r_pcurrentvertbase;
extern medge_t* r_pcurrentedges;
extern int* r_currentpsurfedges;

void mult_matrix_vector(float* p, const float* a, const float* b);
void mult_matrix_matrix(float* p, const float* a, const float* b);
void inverse(const float* m, float* inv);
void create_view_matrix(float* matrix, float* vieworg, float* viewangles);
void create_orthographic_matrix(float matrix[16], float xmin, float xmax, float ymin, float ymax, float znear, float zfar);
void create_projection_matrix(float matrix[16], float znear, float zfar, float fov_x, float fov_y);

void* GL_LoadDXRAliasMesh(const char* name, int numVertexes, trivertx_t* vertexes, int numTris, mtriangle_t* triangles, stvert_t* stverts);
void create_entity_matrix(float matrix[16], entity_t* e, qboolean enable_left_hand);
void create_brush_matrix(float matrix[16], entity_t* e, qboolean enable_left_hand);

void GL_BlitUIImage(int texnum, int srcx, int srcy, int destx, int desty);
void GL_BlitUIImageUV(int texnum, float u, float v, int destx, int desty, int w, int h);
void GL_BlitUIImageUVNoScale(int texnum, float u, float v, int destx, int desty, int w, int h);
void GL_RegisterWorldLight(entity_t* ent, float x, float y, float z, float radius);

void GL_SetUICanvas(float x, float y, float width, float height);

void R_TimeRefresh_f (void);
void R_ReadPointFile_f (void);
texture_t *R_TextureAnimation (texture_t *base, int frame);

typedef struct surfcache_s
{
	struct surfcache_s	*next;
	struct surfcache_s 	**owner;		// NULL is an empty chunk of memory
	int			lightadj[MAXLIGHTMAPS]; // checked for strobe flush
	int			dlight;
	int			size;		// including header
	unsigned		width;
	unsigned		height;		// DEBUG only needed for debug
	float			mipscale;
	struct texture_s	*texture;	// checked for animating textures
	byte			data[4];	// width*height elements
} surfcache_t;


typedef struct
{
	pixel_t		*surfdat;	// destination for generated surface
	int		rowbytes;	// destination logical width in bytes
	msurface_t	*surf;		// description for surface to generate
	fixed8_t	lightadj[MAXLIGHTMAPS];
							// adjust for lightmap levels for dynamic lighting
	texture_t	*texture;	// corrected for animating textures
	int		surfmip;	// mipmapped ratio of surface texels / world pixels
	int		surfwidth;	// in mipmapped texels
	int		surfheight;	// in mipmapped texels
} drawsurf_t;


typedef enum {
	pt_static, pt_grav, pt_slowgrav, pt_fire, pt_explode, pt_explode2, pt_blob, pt_blob2
} ptype_t;

// !!! if this is changed, it must be changed in d_ifacea.h too !!!
typedef struct particle_s
{
// driver-usable fields
	vec3_t		org;
	float		color;
// drivers never touch the following fields
	struct particle_s	*next;
	vec3_t		vel;
	float		ramp;
	float		die;
	ptype_t		type;
} particle_t;


//====================================================

extern	qboolean	r_cache_thrash;		// compatability
extern	vec3_t		modelorg, r_entorigin;
extern	entity_t	*currententity;
extern	int		r_visframecount;	// ??? what difs?
extern	int		r_framecount;
extern	mplane_t	frustum[4];

//
// view origin
//
extern	vec3_t	vup;
extern	vec3_t	vpn;
extern	vec3_t	vright;
extern	vec3_t	r_origin;

//
// screen size info
//
extern	refdef_t	r_refdef;
extern	mleaf_t		*r_viewleaf, *r_oldviewleaf;
extern	int		d_lightstylevalue[256];	// 8.8 fraction of base light value

extern	cvar_t	r_norefresh;
extern	cvar_t	r_drawentities;
extern	cvar_t	r_drawworld;
extern	cvar_t	r_drawviewmodel;
extern	cvar_t	r_speeds;
extern	cvar_t	r_pos;
extern	cvar_t	r_waterwarp;
extern	cvar_t	r_fullbright;
extern	cvar_t	r_lightmap;
extern	cvar_t	r_shadows;
extern	cvar_t	r_wateralpha;
extern	cvar_t	r_lavaalpha;
extern	cvar_t	r_telealpha;
extern	cvar_t	r_slimealpha;
extern	cvar_t	r_dynamic;
extern	cvar_t	r_novis;
extern	cvar_t	r_scale;

extern	cvar_t	gl_clear;
extern	cvar_t	gl_cull;
extern	cvar_t	gl_smoothmodels;
extern	cvar_t	gl_affinemodels;
extern	cvar_t	gl_polyblend;
extern	cvar_t	gl_flashblend;
extern	cvar_t	gl_nocolors;

extern	cvar_t	gl_playermip;

extern	cvar_t	gl_subdivide_size;
extern	float	load_subdivide_size; //johnfitz -- remember what subdivide_size value was when this map was loaded

extern int		gl_stencilbits;

// Multitexture
extern	qboolean	mtexenabled;
extern	qboolean	gl_mtexable;


//johnfitz -- anisotropic filtering
#define	GL_TEXTURE_MAX_ANISOTROPY_EXT		0x84FE
#define	GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT	0x84FF
extern	float		gl_max_anisotropy;
extern	qboolean	gl_anisotropy_able;

//ericw -- GLSL

// SDL 1.2 has a bug where it doesn't provide these typedefs on OS X!
extern	qboolean	gl_glsl_able;
extern	qboolean	gl_glsl_gamma_able;
extern	qboolean	gl_glsl_alias_able;
// ericw --

//ericw -- NPOT texture support
extern	qboolean	gl_texture_NPOT;

//johnfitz -- polygon offset
#define OFFSET_BMODEL 1
#define OFFSET_NONE 0
#define OFFSET_DECAL -1
#define OFFSET_FOG -2
#define OFFSET_SHOWTRIS -3
void GL_PolygonOffset (int);

//johnfitz -- GL_EXT_texture_env_combine
//the values for GL_ARB_ are identical
#define GL_COMBINE_EXT		0x8570
#define GL_COMBINE_RGB_EXT	0x8571
#define GL_COMBINE_ALPHA_EXT	0x8572
#define GL_RGB_SCALE_EXT	0x8573
#define GL_CONSTANT_EXT		0x8576
#define GL_PRIMARY_COLOR_EXT	0x8577
#define GL_PREVIOUS_EXT		0x8578
#define GL_SOURCE0_RGB_EXT	0x8580
#define GL_SOURCE1_RGB_EXT	0x8581
#define GL_SOURCE0_ALPHA_EXT	0x8588
#define GL_SOURCE1_ALPHA_EXT	0x8589
extern qboolean gl_texture_env_combine;
extern qboolean gl_texture_env_add; // for GL_EXT_texture_env_add

//johnfitz -- rendering statistics
extern int rs_brushpolys, rs_aliaspolys, rs_skypolys, rs_particles, rs_fogpolys;
extern int rs_dynamiclightmaps, rs_brushpasses, rs_aliaspasses, rs_skypasses;
extern float rs_megatexels;

//johnfitz -- track developer statistics that vary every frame
extern cvar_t devstats;
typedef struct {
	int		packetsize;
	int		edicts;
	int		visedicts;
	int		efrags;
	int		tempents;
	int		beams;
	int		dlights;
} devstats_t;
extern devstats_t dev_stats, dev_peakstats;

//ohnfitz -- reduce overflow warning spam
typedef struct {
	double	packetsize;
	double	efrags;
	double	beams;
	double	varstring;
} overflowtimes_t;
extern overflowtimes_t dev_overflows; //this stores the last time overflow messages were displayed, not the last time overflows occured
#define CONSOLE_RESPAM_TIME 3 // seconds between repeated warning messages

//johnfitz -- moved here from r_brush.c
extern int gl_lightmap_format, lightmap_bytes;

#define LMBLOCK_WIDTH	256	//FIXME: make dynamic. if we have a decent card there's no real reason not to use 4k or 16k (assuming there's no lightstyles/dynamics that need uploading...)
#define LMBLOCK_HEIGHT	256 //Alternatively, use texture arrays, which would avoid the need to switch textures as often.

typedef struct glRect_s {
	unsigned short l,t,w,h;
} glRect_t;
struct lightmap_s
{
	gltexture_t *texture;
	glpoly_t	*polys;
	qboolean	modified;
	glRect_t	rectchange;

	// the lightmap texture data needs to be kept in
	// main memory so texsubimage can update properly
	byte		*data;//[4*LMBLOCK_WIDTH*LMBLOCK_HEIGHT];
};
extern struct lightmap_s *lightmap;
extern int lightmap_count;	//allocated lightmaps

extern int gl_warpimagesize; //johnfitz -- for water warp

extern qboolean r_drawflat_cheatsafe, r_fullbright_cheatsafe, r_lightmap_cheatsafe, r_drawworld_cheatsafe; //johnfitz

typedef struct glsl_attrib_binding_s {
	const char *name;
	//GLuint attrib;
} glsl_attrib_binding_t;

extern float	map_wateralpha, map_lavaalpha, map_telealpha, map_slimealpha; //ericw

//johnfitz -- fog functions called from outside gl_fog.c
void Fog_ParseServerMessage (void);
float *Fog_GetColor (void);
float Fog_GetDensity (void);
void Fog_EnableGFog (void);
void Fog_DisableGFog (void);
void Fog_StartAdditive (void);
void Fog_StopAdditive (void);
void Fog_SetupFrame (void);
void Fog_NewMap (void);
void Fog_Init (void);
void Fog_SetupState (void);

void R_NewGame (void);

void R_AnimateLight (void);
void R_MarkSurfaces (void);
void R_CullSurfaces (void);
qboolean R_CullBox (vec3_t emins, vec3_t emaxs);
void R_StoreEfrags (efrag_t **ppefrag);
qboolean R_CullModelForEntity (entity_t *e);
void R_RotateForEntity (vec3_t origin, vec3_t angles);
void R_MarkLights (dlight_t *light, int num, mnode_t *node);

void R_InitParticles (void);
void R_DrawParticles (void);
void CL_RunParticles (void);
void R_ClearParticles (void);

void R_TranslatePlayerSkin (int playernum);
void R_TranslateNewPlayerSkin (int playernum); //johnfitz -- this handles cases when the actual texture changes
void R_UpdateWarpTextures (void);

void R_DrawWorld (void);
void R_DrawAliasModel (entity_t *e);
void R_DrawBrushModel (entity_t *e);
void R_DrawSpriteModel (entity_t *e);

void R_DrawTextureChains_Water (qmodel_t *model, entity_t *ent, texchain_t chain);

void R_RenderDlights (void);
void GL_BuildLightmaps (void);
void GL_DeleteBModelVertexBuffer (void);
void GL_BuildBModelVertexBuffer (void);
void GLMesh_LoadVertexBuffers (void);
void GLMesh_DeleteVertexBuffers (void);
void R_RebuildAllLightmaps (void);

int R_LightPoint (vec3_t p);

void GL_SubdivideSurface (msurface_t *fa);
void R_BuildLightMap (msurface_t *surf, byte *dest, int stride);
void R_RenderDynamicLightmaps (msurface_t *fa);
void R_UploadLightmaps (void);

void R_DrawWorld_ShowTris (void);
void R_DrawBrushModel_ShowTris (entity_t *e);
void R_DrawAliasModel_ShowTris (entity_t *e);
void R_DrawParticles_ShowTris (void);

void GLWorld_CreateShaders (void);
void GLAlias_CreateShaders (void);
void GL_DrawAliasShadow (entity_t *e);
void DrawGLTriangleFan (glpoly_t *p);
void DrawGLPoly (glpoly_t *p);
void DrawWaterPoly (glpoly_t *p);
void GL_MakeAliasModelDisplayLists (qmodel_t *m, aliashdr_t *hdr);

void Sky_Init (void);
void Sky_DrawSky (void);
void Sky_NewMap (void);
void Sky_LoadTexture (texture_t *mt);
void Sky_LoadSkyBox (const char *name);

void TexMgr_RecalcWarpImageSize (void);

void R_ClearTextureChains (qmodel_t *mod, texchain_t chain);
void R_ChainSurface (msurface_t *surf, texchain_t chain);
void R_DrawTextureChains (qmodel_t *model, entity_t *ent, texchain_t chain);
void R_DrawWorld_Water (void);

//void GL_BindBuffer (GLenum target, GLuint buffer);
void GL_ClearBufferBindings ();

void GLSLGamma_DeleteTexture (void);
void GLSLGamma_GammaCorrect (void);

void R_ScaleView_DeleteTexture (void);

float GL_WaterAlphaForSurface (msurface_t *fa);

#endif	/* __GLQUAKE_H */

