// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 2020-2021 by Jaime Ita Passos.
// Copyright (C) 1998-2000 by DooM Legacy Team.
// Copyright (C) 1998-2021 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file r_gles1.c
/// \brief OpenGL ES 1.1 API for Sonic Robo Blast 2

#include <stdarg.h>
#include <math.h>

#include "r_gles.h"
#include "../r_opengl/r_vbo.h"

#if defined (HWRENDER) && !defined (NOROPENGL)

static const GLfloat white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

boolean GLBackend_LoadFunctions(void)
{
	GLExtension_multitexture = true;
	GLExtension_vertex_buffer_object = true;
	GLExtension_texture_filter_anisotropic = true;

	GLBackend_LoadExtraFunctions();

	GETOPENGLFUNC(ClearDepthf)
	GETOPENGLFUNC(DepthRangef)

	GETOPENGLFUNC(Color4f)
	GETOPENGLFUNC(VertexPointer)
	GETOPENGLFUNC(NormalPointer)
	GETOPENGLFUNC(TexCoordPointer)
	GETOPENGLFUNC(ColorPointer)
	GETOPENGLFUNC(DrawArrays)
	GETOPENGLFUNC(DrawElements)
	GETOPENGLFUNC(EnableClientState)
	GETOPENGLFUNC(DisableClientState)

	GETOPENGLFUNC(TexEnvi)

	if (!GLBackend_LoadLegacyFunctions())
		return false;

	return true;
}

boolean GLBackend_LoadExtraFunctions(void)
{
	GETOPENGLFUNCTRY(GenerateMipmap)
	if (pglGenerateMipmap)
		MipmapSupported = GL_TRUE;

	GLExtension_LoadFunctions();

	return true;
}

#undef GETOPENGLFUNC

static void SetShader(int type)
{
	(void)type;
}

static boolean CompileShaders(void)
{
	return false;
}

static void SetShaderInfo(hwdshaderinfo_t info, INT32 value)
{
	(void)info;
	(void)value;
}

static void LoadCustomShader(int number, char *shader, size_t size, boolean fragment)
{
	(void)number;
	(void)shader;
	(void)size;
	(void)fragment;
}

static void UnSetShader(void) {}
static void CleanShaders(void) {}

// -----------------+
// SetNoTexture     : Disable texture
// -----------------+
static void SetNoTexture(void)
{
	GLTexture_Disable();
}

static void GLPerspective(GLfloat fovy, GLfloat aspect)
{
	GLfloat m[4][4] =
	{
		{ 1.0f, 0.0f, 0.0f, 0.0f},
		{ 0.0f, 1.0f, 0.0f, 0.0f},
		{ 0.0f, 0.0f, 1.0f,-1.0f},
		{ 0.0f, 0.0f, 0.0f, 0.0f},
	};
	const GLfloat zNear = near_clipping_plane;
	const GLfloat zFar = FAR_CLIPPING_PLANE;
	const GLfloat radians = (GLfloat)(fovy / 2.0f * M_PIl / 180.0f);
	const GLfloat sine = (GLfloat)sin(radians);
	const GLfloat deltaZ = zFar - zNear;
	GLfloat cotangent;

	if ((fabsf((float)deltaZ) < 1.0E-36f) || fpclassify(sine) == FP_ZERO || fpclassify(aspect) == FP_ZERO)
	{
		return;
	}
	cotangent = cosf(radians) / sine;

	m[0][0] = cotangent / aspect;
	m[1][1] = cotangent;
	m[2][2] = -(zFar + zNear) / deltaZ;
	m[3][2] = -2.0f * zNear * zFar / deltaZ;

	gl_MultMatrixf(&m[0][0]);
}

static void FlushScreenTextures(void)
{
	GLTexture_FlushScreen();
}

// -----------------+
// SetModelView     :
// -----------------+
static void SetModelView(INT32 w, INT32 h)
{
	// The screen textures need to be flushed if the width or height change so that they be remade for the correct size
	if (screen_width != w || screen_height != h)
		FlushScreenTextures();

	screen_width = w;
	screen_height = h;

	gl_Viewport(0, 0, w, h);

	gl_MatrixMode(GL_PROJECTION);
	gl_LoadIdentity();

	gl_MatrixMode(GL_MODELVIEW);
	gl_LoadIdentity();

	GLPerspective(fov, ASPECT_RATIO);
}


// -----------------+
// SetBlend         : Set blend modes
// -----------------+
static void SetBlend(FBITFIELD PolyFlags)
{
	GLBackend_SetBlend(PolyFlags);
}


// -----------------+
// SetStates        : Set permanent states
// -----------------+
static void SetStates(void)
{
#ifdef GL_LIGHT_MODEL_AMBIENT
	GLfloat LightDiffuse[] = {1.0f, 1.0f, 1.0f, 1.0f};
#endif

	gl_EnableClientState(GL_VERTEX_ARRAY); // We always use this one

	gl_ShadeModel(GL_SMOOTH);      // iterate vertice colors

	gl_Enable(GL_TEXTURE_2D);      // two-dimensional texturing
	gl_TexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	gl_AlphaFunc(GL_NOTEQUAL, 0.0f);
	gl_Enable(GL_BLEND);           // enable color blending

	gl_ColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

	gl_Enable(GL_DEPTH_TEST);    // check the depth buffer
	gl_DepthMask(GL_TRUE);             // enable writing to depth buffer
	gl_ClearDepthf(1.0f);
	gl_DepthRangef(0.0f, 1.0f);
	gl_DepthFunc(GL_LEQUAL);

	// this set CurrentPolyFlags to the acctual configuration
	CurrentPolyFlags = 0xffffffff;
	SetBlend(0);

	tex_downloaded = 0;
	SetNoTexture();

	gl_PolygonOffset(-1.0f, -1.0f);

	// Lighting for models
#ifdef GL_LIGHT_MODEL_AMBIENT
	gl_LightModelfv(GL_LIGHT_MODEL_AMBIENT, LightDiffuse);
	gl_Enable(GL_LIGHT0);
#endif

	// bp : when no t&l :)
	gl_LoadIdentity();
	gl_Scalef(1.0f, 1.0f, -1.0f);
}

// -----------------+
// DeleteTexture    : Deletes a texture from the GPU and frees its data
// -----------------+
static void DeleteTexture(GLMipmap_t *pTexInfo)
{
	FTextureInfo *head = TexCacheHead;

	if (!pTexInfo)
		return;
	else if (pTexInfo->downloaded)
		gl_DeleteTextures(1, (GLuint *)&pTexInfo->downloaded);

	while (head)
	{
		if (head->downloaded == pTexInfo->downloaded)
		{
			if (head->next)
				head->next->prev = head->prev;
			else // no next -> tail is being deleted -> update TexCacheTail
				TexCacheTail = head->prev;
			if (head->prev)
				head->prev->next = head->next;
			else // no prev -> head is being deleted -> update TexCacheHead
				TexCacheHead = head->next;
			free(head);
			break;
		}

		head = head->next;
	}

	pTexInfo->downloaded = 0;
}


// -----------------+
// Init             : Initialise the OpenGL ES interface API
// Returns          :
// -----------------+
static boolean Init(void)
{
	return GLBackend_Init();
}


// -----------------+
// RecreateContext  : Clears textures, buffer objects, and recompiles shaders.
// Returns          :
// -----------------+
static void RecreateContext(void)
{
	GLBackend_RecreateContext();
}


// -----------------+
// SetPalette       : Sets the current palette.
// Returns          :
// -----------------+
static void SetPalette(RGBA_t *palette)
{
	GLBackend_SetPalette(palette);
}


// -----------------+
// ClearMipMapCache : Flush OpenGL textures from memory
// -----------------+
static void ClearMipMapCache(void)
{
	GLTexture_Flush();
}


// -----------------+
// ReadRect         : Read a rectangle region of the truecolor framebuffer
//                  : store pixels as RGBA8888
// Returns          : RGBA8888 pixel array stored in dst_data
// -----------------+
static void ReadRect(INT32 x, INT32 y, INT32 width, INT32 height, INT32 dst_stride, UINT32 *dst_data)
{
	(void)dst_stride;
	GLBackend_ReadRectRGBA(x, y, width, height, dst_data);
}


// -----------------+
// GClipRect        : Defines the 2D hardware clipping window
// -----------------+
static void GClipRect(INT32 minx, INT32 miny, INT32 maxx, INT32 maxy, float nearclip)
{
	gl_Viewport(minx, screen_height-maxy, maxx-minx, maxy-miny);
	near_clipping_plane = nearclip;

	gl_MatrixMode(GL_PROJECTION);
	gl_LoadIdentity();
	GLPerspective(fov, ASPECT_RATIO);
	gl_MatrixMode(GL_MODELVIEW);
}


// -----------------+
// ClearBuffer      : Clear the color/alpha/depth buffer(s)
// -----------------+
static void ClearBuffer(FBOOLEAN ColorMask, FBOOLEAN DepthMask, FRGBAFloat *ClearColor)
{
	GLbitfield ClearMask = 0;

	if (ColorMask)
	{
		if (ClearColor)
			gl_ClearColor(ClearColor->red,
			              ClearColor->green,
			              ClearColor->blue,
			              ClearColor->alpha);
		ClearMask |= GL_COLOR_BUFFER_BIT;
	}
	if (DepthMask)
	{
		gl_ClearDepthf(1.0f);     //Hurdler: all that are permanen states
		gl_DepthRangef(0.0f, 1.0f);
		gl_DepthFunc(GL_LEQUAL);
		ClearMask |= GL_DEPTH_BUFFER_BIT;
	}

	SetBlend(DepthMask ? PF_Occlude | CurrentPolyFlags : CurrentPolyFlags&~PF_Occlude);

	gl_Clear(ClearMask);
	gl_EnableClientState(GL_TEXTURE_COORD_ARRAY); // We mostly use this one
}


// -----------------+
// HWRAPI Draw2DLine: Render a 2D line
// -----------------+
static void Draw2DLine(F2DCoord *v1, F2DCoord *v2, RGBA_t Color)
{
	GLfloat fcolor[4];
	GLfloat p[12];
	GLfloat dx, dy;
	GLfloat angle;

	gl_Disable(GL_TEXTURE_2D);

	// This is the preferred, 'modern' way of rendering lines -- creating a polygon.
	if (fabsf(v2->x - v1->x) > FLT_EPSILON)
		angle = (float)atan((v2->y-v1->y)/(v2->x-v1->x));
	else
		angle = (float)N_PI_DEMI;
	dx = (float)sin(angle) / (float)screen_width;
	dy = (float)cos(angle) / (float)screen_height;

	p[0] = v1->x - dx;  p[1] = -(v1->y + dy); p[2] = 1;
	p[3] = v2->x - dx;  p[4] = -(v2->y + dy); p[5] = 1;
	p[6] = v2->x + dx;  p[7] = -(v2->y - dy); p[8] = 1;
	p[9] = v1->x + dx;  p[10] = -(v1->y - dy); p[11] = 1;

	fcolor[0] = (Color.s.red/255.0f);
	fcolor[1] = (Color.s.green/255.0f);
	fcolor[2] = (Color.s.blue/255.0f);
	fcolor[3] = (Color.s.alpha/255.0f);

	gl_DisableClientState(GL_TEXTURE_COORD_ARRAY);
	gl_Color4f(fcolor[0], fcolor[1], fcolor[2], fcolor[3]);
	gl_VertexPointer(3, GL_FLOAT, 0, p);
	gl_DrawArrays(GL_TRIANGLE_FAN, 0, 4);

	gl_EnableClientState(GL_TEXTURE_COORD_ARRAY);
	gl_Enable(GL_TEXTURE_2D);
}

static void SetClamp(UINT32 clamp)
{
	gl_TexParameteri(GL_TEXTURE_2D, (GLenum)clamp, GL_CLAMP_TO_EDGE);
}

// -----------------+
// UpdateTexture    : Updates the texture data.
// -----------------+
static void UpdateTexture(GLMipmap_t *pTexInfo)
{
	// Upload a texture
	GLuint num = pTexInfo->downloaded;
	boolean update = true;

	INT32 w = pTexInfo->width, h = pTexInfo->height;
	INT32 i, j;

	const GLubyte *pImgData = (const GLubyte *)pTexInfo->data;
	const GLvoid *ptex = NULL;
	RGBA_t *tex = NULL;

	// Generate a new texture name.
	if (!num)
	{
		gl_GenTextures(1, &num);
		pTexInfo->downloaded = num;
		update = false;
	}

	if (pTexInfo->format == GL_TEXFMT_P_8 || pTexInfo->format == GL_TEXFMT_AP_88)
	{
		GLTexture_AllocBuffer(pTexInfo);
		ptex = tex = TextureBuffer;

		for (j = 0; j < h; j++)
		{
			for (i = 0; i < w; i++)
			{
				if ((*pImgData == HWR_PATCHES_CHROMAKEY_COLORINDEX) &&
					(pTexInfo->flags & TF_CHROMAKEYED))
				{
					tex[w*j+i].s.red   = 0;
					tex[w*j+i].s.green = 0;
					tex[w*j+i].s.blue  = 0;
					tex[w*j+i].s.alpha = 0;
					pTexInfo->flags |= TF_TRANSPARENT; // there is a hole in it
				}
				else
				{
					tex[w*j+i].s.red   = myPaletteData[*pImgData].s.red;
					tex[w*j+i].s.green = myPaletteData[*pImgData].s.green;
					tex[w*j+i].s.blue  = myPaletteData[*pImgData].s.blue;
					tex[w*j+i].s.alpha = myPaletteData[*pImgData].s.alpha;
				}

				pImgData++;

				if (pTexInfo->format == GL_TEXFMT_AP_88)
				{
					if (!(pTexInfo->flags & TF_CHROMAKEYED))
						tex[w*j+i].s.alpha = *pImgData;
					pImgData++;
				}

			}
		}
	}
	else if (pTexInfo->format == GL_TEXFMT_RGBA)
	{
		// Directly upload the texture data
		ptex = pImgData;
	}
	else if (pTexInfo->format == GL_TEXFMT_ALPHA_INTENSITY_88)
	{
		GLTexture_AllocBuffer(pTexInfo);
		ptex = tex = TextureBuffer;

		for (j = 0; j < h; j++)
		{
			for (i = 0; i < w; i++)
			{
				tex[w*j+i].s.red   = *pImgData;
				tex[w*j+i].s.green = *pImgData;
				tex[w*j+i].s.blue  = *pImgData;
				pImgData++;
				tex[w*j+i].s.alpha = *pImgData;
				pImgData++;
			}
		}
	}
	else if (pTexInfo->format == GL_TEXFMT_ALPHA_8) // Used for fade masks
	{
		GLTexture_AllocBuffer(pTexInfo);
		ptex = tex = TextureBuffer;

		for (j = 0; j < h; j++)
		{
			for (i = 0; i < w; i++)
			{
				tex[w*j+i].s.red   = 255; // 255 because the fade mask is modulated with the screen texture, so alpha affects it while the colours don't
				tex[w*j+i].s.green = 255;
				tex[w*j+i].s.blue  = 255;
				tex[w*j+i].s.alpha = *pImgData;
				pImgData++;
			}
		}
	}

	gl_BindTexture(GL_TEXTURE_2D, num);
	tex_downloaded = num;

	// disable texture filtering on any texture that has holes so there's no dumb borders or blending issues
	if (pTexInfo->flags & TF_TRANSPARENT)
	{
		gl_TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		gl_TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	}
	else
	{
		gl_TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter);
		gl_TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);
	}

	if (update)
		gl_TexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, ptex);
	else
		gl_TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, ptex);

	if (MipmapEnabled)
		gl_GenerateMipmap(GL_TEXTURE_2D);

	if (pTexInfo->flags & TF_WRAPX)
		gl_TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	else
		SetClamp(GL_TEXTURE_WRAP_S);

	if (pTexInfo->flags & TF_WRAPY)
		gl_TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	else
		SetClamp(GL_TEXTURE_WRAP_T);

	if (GLExtension_texture_filter_anisotropic)
		gl_TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, anisotropic_filter);
}

// -----------------+
// SetTexture       : The mipmap becomes the current texture source
// -----------------+
static void SetTexture(GLMipmap_t *pTexInfo)
{
	if (!pTexInfo)
	{
		SetNoTexture();
		return;
	}
	else if (pTexInfo->downloaded)
	{
		if (pTexInfo->downloaded != tex_downloaded)
		{
			gl_BindTexture(GL_TEXTURE_2D, pTexInfo->downloaded);
			tex_downloaded = pTexInfo->downloaded;
		}
	}
	else
	{
		FTextureInfo *newTex = calloc(1, sizeof (*newTex));

		UpdateTexture(pTexInfo);

		newTex->texture = pTexInfo;
		newTex->downloaded = (UINT32)pTexInfo->downloaded;
		newTex->width = (UINT32)pTexInfo->width;
		newTex->height = (UINT32)pTexInfo->height;
		newTex->format = (UINT32)pTexInfo->format;

		// insertion at the tail
		if (TexCacheTail)
		{
			newTex->prev = TexCacheTail;
			TexCacheTail->next = newTex;
			TexCacheTail = newTex;
		}
		else // initialization of the linked list
			TexCacheTail = TexCacheHead = newTex;
	}
}

// code that is common between DrawPolygon and DrawIndexedTriangles
// the corona thing is there too, i have no idea if that stuff works with DrawIndexedTriangles and batching
static void PreparePolygon(FSurfaceInfo *pSurf, FOutVector *pOutVerts, FBITFIELD PolyFlags)
{
	static GLRGBAFloat poly = {1.0f, 1.0f, 1.0f, 1.0f};
	static GLRGBAFloat tint = {1.0f, 1.0f, 1.0f, 1.0f};
	static GLRGBAFloat fade = {1.0f, 1.0f, 1.0f, 1.0f};

	if (PolyFlags & PF_Corona)
		PolyFlags &= ~(PF_NoDepthTest|PF_Corona);

	SetBlend(PolyFlags);

	// PolyColor
	if (pSurf)
	{
		float red = (pSurf->PolyColor.s.red/255.0f);
		float green = (pSurf->PolyColor.s.green/255.0f);
		float blue = (pSurf->PolyColor.s.blue/255.0f);
		float alpha = (pSurf->PolyColor.s.alpha/255.0f);

		// If Modulated, mix the surface colour to the texture
		if (CurrentPolyFlags & PF_Modulated)
			gl_Color4f(red, green, blue, alpha);

		// If the surface is either modulated or colormapped, or both
		if (CurrentPolyFlags & (PF_Modulated | PF_ColorMapped))
		{
			poly.red   = red;
			poly.green = green;
			poly.blue  = blue;
			poly.alpha = alpha;
		}

		// Only if the surface is colormapped
		if (CurrentPolyFlags & PF_ColorMapped)
		{
			tint.red   = (pSurf->TintColor.s.red/255.0f);
			tint.green = (pSurf->TintColor.s.green/255.0f);
			tint.blue  = (pSurf->TintColor.s.blue/255.0f);
			tint.alpha = (pSurf->TintColor.s.alpha/255.0f);

			fade.red   = (pSurf->FadeColor.s.red/255.0f);
			fade.green = (pSurf->FadeColor.s.green/255.0f);
			fade.blue  = (pSurf->FadeColor.s.blue/255.0f);
			fade.alpha = (pSurf->FadeColor.s.alpha/255.0f);
		}
	}
}

// -----------------+
// DrawPolygon      : Render a polygon, set the texture, set render mode
// -----------------+
static void DrawPolygon(FSurfaceInfo *pSurf, FOutVector *pOutVerts, FUINT iNumPts, FBITFIELD PolyFlags)
{
	PreparePolygon(pSurf, pOutVerts, PolyFlags);

	gl_VertexPointer(3, GL_FLOAT, sizeof(FOutVector), &pOutVerts[0].x);
	gl_TexCoordPointer(2, GL_FLOAT, sizeof(FOutVector), &pOutVerts[0].s);
	gl_DrawArrays(GL_TRIANGLE_FAN, 0, iNumPts);

	if (PolyFlags & PF_RemoveYWrap)
		gl_TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	if (PolyFlags & PF_ForceWrapX)
		SetClamp(GL_TEXTURE_WRAP_S);

	if (PolyFlags & PF_ForceWrapY)
		SetClamp(GL_TEXTURE_WRAP_T);
}

static void DrawPolygonShader(FSurfaceInfo *pSurf, FOutVector *pOutVerts, FUINT iNumPts, FBITFIELD PolyFlags, INT32 shader)
{
	(void)shader;
	DrawPolygon(pSurf, pOutVerts, iNumPts, PolyFlags);
}

static void DrawIndexedTriangles(FSurfaceInfo *pSurf, FOutVector *pOutVerts, FUINT iNumPts, FBITFIELD PolyFlags, UINT32 *IndexArray)
{
	PreparePolygon(pSurf, pOutVerts, PolyFlags);

	gl_VertexPointer(3, GL_FLOAT, sizeof(FOutVector), &pOutVerts[0].x);
	gl_TexCoordPointer(2, GL_FLOAT, sizeof(FOutVector), &pOutVerts[0].s);
	gl_DrawElements(GL_TRIANGLES, iNumPts, GL_UNSIGNED_INT, IndexArray);

	// the DrawPolygon variant of this has some code about polyflags and wrapping here but havent noticed any problems from omitting it?
}

static void RenderSkyDome(gl_sky_t *sky)
{
	int i, j;

	// Build the sky dome! Yes!
	if (sky->rebuild)
	{
		// delete VBO when already exists
		if (GLExtension_vertex_buffer_object)
		{
			if (sky->vbo)
				gl_DeleteBuffers(1, &sky->vbo);
		}

		if (GLExtension_vertex_buffer_object)
		{
			// generate a new VBO and get the associated ID
			gl_GenBuffers(1, &sky->vbo);

			// bind VBO in order to use
			gl_BindBuffer(GL_ARRAY_BUFFER, sky->vbo);

			// upload data to VBO
			gl_BufferData(GL_ARRAY_BUFFER, sky->vertex_count * sizeof(sky->data[0]), sky->data, GL_STATIC_DRAW);
		}

		sky->rebuild = false;
	}

	// bind VBO in order to use
	if (GLExtension_vertex_buffer_object)
		gl_BindBuffer(GL_ARRAY_BUFFER, sky->vbo);

	// activate and specify pointers to arrays
	gl_VertexPointer(3, GL_FLOAT, sizeof(sky->data[0]), sky_vbo_x);
	gl_TexCoordPointer(2, GL_FLOAT, sizeof(sky->data[0]), sky_vbo_u);
	gl_ColorPointer(4, GL_UNSIGNED_BYTE, sizeof(sky->data[0]), sky_vbo_r);

	// activate color arrays
	gl_EnableClientState(GL_COLOR_ARRAY);

	// set transforms
	gl_Scalef(1.0f, (float)sky->height / 200.0f, 1.0f);
	gl_Rotatef(270.0f, 0.0f, 1.0f, 0.0f);

	for (j = 0; j < 2; j++)
	{
		for (i = 0; i < sky->loopcount; i++)
		{
			gl_skyloopdef_t *loop = &sky->loops[i];
			unsigned int mode = 0;

			if (j == 0 ? loop->use_texture : !loop->use_texture)
				continue;

			switch (loop->mode)
			{
				case HWD_SKYLOOP_FAN:
					mode = GL_TRIANGLE_FAN;
					break;
				case HWD_SKYLOOP_STRIP:
					mode = GL_TRIANGLE_STRIP;
					break;
				default:
					continue;
			}

			gl_DrawArrays(mode, loop->vertexindex, loop->vertexcount);
		}
	}

	gl_Scalef(1.0f, 1.0f, 1.0f);
	gl_Color4f(white[0], white[1], white[2], white[3]);

	// bind with 0, so, switch back to normal pointer operation
	if (GLExtension_vertex_buffer_object)
		gl_BindBuffer(GL_ARRAY_BUFFER, 0);

	// deactivate color array
	gl_DisableClientState(GL_COLOR_ARRAY);
}

static void SetSpecialState(hwdspecialstate_t IdState, INT32 Value)
{
	switch (IdState)
	{
		case HWD_SET_MODEL_LIGHTING:
			model_lighting = Value;
			break;

		case HWD_SET_TEXTUREFILTERMODE:
			GLTexture_SetFilterMode(Value);
			GLTexture_Flush(); //??? if we want to change filter mode by texture, remove this
			break;

		case HWD_SET_TEXTUREANISOTROPICMODE:
			if (GLExtension_texture_filter_anisotropic)
			{
				anisotropic_filter = min(Value, maximumAnisotropy);
				GLTexture_Flush(); //??? if we want to change filter mode by texture, remove this
			}
			break;

		case HWD_SET_DITHER:
			if (Value)
				gl_Enable(GL_DITHER);
			else
				gl_Disable(GL_DITHER);

		default:
			break;
	}
}

static void CreateModelVBOs(model_t *model)
{
	GLModel_GenerateVBOs(model);
}

static void DeleteModelVBOs(model_t *model)
{
	GLModel_DeleteVBOs(model);
}

static void DeleteModelData(void)
{
	GLBackend_DeleteModelData();
}

#define BUFFER_OFFSET(i) ((char*)NULL + (i))

// -----------------+
// HWRAPI DrawModel : Draw a model
// -----------------+
static void DrawModel(model_t *model, INT32 frameIndex, float duration, float tics, INT32 nextFrameIndex, FTransform *pos, float scale, UINT8 flipped, UINT8 hflipped, FSurfaceInfo *Surface)
{
	static GLRGBAFloat poly = {0,0,0,0};
	static GLRGBAFloat tint = {0,0,0,0};
	static GLRGBAFloat fade = {0,0,0,0};

	float pol = 0.0f;
	float scalex, scaley, scalez;

	boolean useTinyFrames;
	boolean useVBO = true;

	FBITFIELD flags;
	int i;

	// Because otherwise, scaling the screen negatively vertically breaks the lighting
	GLfloat LightPos[4] = {0.0f, 1.0f, 0.0f, 0.0f};
#ifdef GL_LIGHT_MODEL_AMBIENT
	float ambient[4];
	float diffuse[4];
#endif

	// Affect input model scaling
	scale *= 0.5f;
	scalex = scale;
	scaley = scale;
	scalez = scale;

	if (duration > 0.0 && tics >= 0.0) // don't interpolate if instantaneous or infinite in length
	{
		float newtime = (duration - tics); // + 1;

		pol = newtime / duration;

		if (pol > 1.0f)
			pol = 1.0f;

		if (pol < 0.0f)
			pol = 0.0f;
	}

	poly.red    = (Surface->PolyColor.s.red/255.0f);
	poly.green  = (Surface->PolyColor.s.green/255.0f);
	poly.blue   = (Surface->PolyColor.s.blue/255.0f);
	poly.alpha  = (Surface->PolyColor.s.alpha/255.0f);

#ifdef GL_LIGHT_MODEL_AMBIENT
	if (model_lighting)
	{
		ambient[0] = poly.red;
		ambient[1] = poly.green;
		ambient[2] = poly.blue;
		ambient[3] = poly.alpha;

		diffuse[0] = poly.red;
		diffuse[1] = poly.green;
		diffuse[2] = poly.blue;
		diffuse[3] = poly.alpha;

		if (ambient[0] > 0.75f)
			ambient[0] = 0.75f;
		if (ambient[1] > 0.75f)
			ambient[1] = 0.75f;
		if (ambient[2] > 0.75f)
			ambient[2] = 0.75f;

		gl_Lightfv(GL_LIGHT0, GL_POSITION, LightPos);
		gl_ShadeModel(GL_SMOOTH);

		gl_Enable(GL_LIGHTING);
		gl_Materialfv(GL_FRONT_AND_BACK, GL_AMBIENT, ambient);
		gl_Materialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, diffuse);
	}
#endif
	else
		gl_Color4f(poly.red, poly.green, poly.blue, poly.alpha);

	tint.red   = (Surface->TintColor.s.red/255.0f);
	tint.green = (Surface->TintColor.s.green/255.0f);
	tint.blue  = (Surface->TintColor.s.blue/255.0f);
	tint.alpha = (Surface->TintColor.s.alpha/255.0f);

	fade.red   = (Surface->FadeColor.s.red/255.0f);
	fade.green = (Surface->FadeColor.s.green/255.0f);
	fade.blue  = (Surface->FadeColor.s.blue/255.0f);
	fade.alpha = (Surface->FadeColor.s.alpha/255.0f);

	gl_Enable(GL_CULL_FACE);
	gl_Enable(GL_NORMALIZE);

#ifdef USE_FTRANSFORM_MIRROR
	// flipped is if the object is vertically flipped
	// hflipped is if the object is horizontally flipped
	// pos->flip is if the screen is flipped vertically
	// pos->mirror is if the screen is flipped horizontally
	// XOR all the flips together to figure out what culling to use!
	{
		boolean reversecull = (flipped ^ hflipped ^ pos->flip ^ pos->mirror);
		if (reversecull)
			gl_CullFace(GL_FRONT);
		else
			gl_CullFace(GL_BACK);
	}
#else
	// pos->flip is if the screen is flipped too
	if (flipped ^ hflipped ^ pos->flip) // If one or three of these are active, but not two, invert the model's culling
	{
		gl_CullFace(GL_FRONT);
	}
	else
	{
		gl_CullFace(GL_BACK);
	}
#endif

	gl_PushMatrix(); // should be the same as glLoadIdentity
	//Hurdler: now it seems to work
	gl_Translatef(pos->x, pos->z, pos->y);
	if (flipped)
		scaley = -scaley;
	if (hflipped)
		scalez = -scalez;

#ifdef USE_FTRANSFORM_ANGLEZ
	gl_Rotatef(pos->anglez, 0.0f, 0.0f, -1.0f); // rotate by slope from Kart
#endif
	gl_Rotatef(pos->angley, 0.0f, -1.0f, 0.0f);
	gl_Rotatef(pos->anglex, 1.0f, 0.0f, 0.0f);

	if (pos->roll)
	{
		float roll = (1.0f * pos->rollflip);
		gl_Translatef(pos->centerx, pos->centery, 0);
		if (pos->rotaxis == 2) // Z
			gl_Rotatef(pos->rollangle, 0.0f, 0.0f, roll);
		else if (pos->rotaxis == 1) // Y
			gl_Rotatef(pos->rollangle, 0.0f, roll, 0.0f);
		else // X
			gl_Rotatef(pos->rollangle, roll, 0.0f, 0.0f);
		gl_Translatef(-pos->centerx, -pos->centery, 0);
	}

	gl_Scalef(scalex, scaley, scalez);

	useTinyFrames = model->meshes[0].tinyframes != NULL;

	if (useTinyFrames)
		gl_Scalef(1 / 64.0f, 1 / 64.0f, 1 / 64.0f);

	// Don't use the VBO if it does not have the correct texture coordinates.
	// (Can happen when model uses a sprite as a texture and the sprite changes)
	// Comparing floats with the != operator here should be okay because they
	// are just copies of glpatches' max_s and max_t values.
	// Instead of the != operator, memcmp is used to avoid a compiler warning.
	if (memcmp(&(model->vbo_max_s), &(model->max_s), sizeof(model->max_s)) != 0 ||
		memcmp(&(model->vbo_max_t), &(model->max_t), sizeof(model->max_t)) != 0)
		useVBO = false;

	gl_EnableClientState(GL_NORMAL_ARRAY);

	for (i = 0; i < model->numMeshes; i++)
	{
		mesh_t *mesh = &model->meshes[i];

		if (useTinyFrames)
		{
			tinyframe_t *frame = &mesh->tinyframes[frameIndex % mesh->numFrames];
			tinyframe_t *nextframe = NULL;

			if (nextFrameIndex != -1)
				nextframe = &mesh->tinyframes[nextFrameIndex % mesh->numFrames];

			if (!nextframe || fpclassify(pol) == FP_ZERO)
			{
				if (useVBO)
				{
					gl_BindBuffer(GL_ARRAY_BUFFER, frame->vboID);
					gl_VertexPointer(3, GL_SHORT, sizeof(vbotiny_t), BUFFER_OFFSET(0));
					gl_NormalPointer(GL_BYTE, sizeof(vbotiny_t), BUFFER_OFFSET(sizeof(short)*3));
					gl_TexCoordPointer(2, GL_FLOAT, sizeof(vbotiny_t), BUFFER_OFFSET(sizeof(short) * 3 + sizeof(char) * 6));

					gl_DrawElements(GL_TRIANGLES, mesh->numTriangles * 3, GL_UNSIGNED_SHORT, mesh->indices);
					gl_BindBuffer(GL_ARRAY_BUFFER, 0);
				}
				else
				{
					gl_VertexPointer(3, GL_SHORT, 0, frame->vertices);
					gl_NormalPointer(GL_BYTE, 0, frame->normals);
					gl_TexCoordPointer(2, GL_FLOAT, 0, mesh->uvs);
					gl_DrawElements(GL_TRIANGLES, mesh->numTriangles * 3, GL_UNSIGNED_SHORT, mesh->indices);
				}
			}
			else
			{
				short *vertPtr;
				char *normPtr;
				int j;

				// Dangit, I soooo want to do this in a GLSL shader...
				GLModel_AllocLerpTinyBuffer(mesh->numVertices * sizeof(short) * 3);
				vertPtr = vertTinyBuffer;
				normPtr = normTinyBuffer;
				j = 0;

				for (j = 0; j < mesh->numVertices * 3; j++)
				{
					// Interpolate
					*vertPtr++ = (short)(frame->vertices[j] + (pol * (nextframe->vertices[j] - frame->vertices[j])));
					*normPtr++ = (char)(frame->normals[j] + (pol * (nextframe->normals[j] - frame->normals[j])));
				}

				gl_VertexPointer(3, GL_SHORT, 0, vertTinyBuffer);
				gl_NormalPointer(GL_BYTE, 0, normTinyBuffer);
				gl_TexCoordPointer(2, GL_FLOAT, 0, mesh->uvs);
				gl_DrawElements(GL_TRIANGLES, mesh->numTriangles * 3, GL_UNSIGNED_SHORT, mesh->indices);
			}
		}
		else
		{
			mdlframe_t *frame = &mesh->frames[frameIndex % mesh->numFrames];
			mdlframe_t *nextframe = NULL;

			if (nextFrameIndex != -1)
				nextframe = &mesh->frames[nextFrameIndex % mesh->numFrames];

			if (!nextframe || fpclassify(pol) == FP_ZERO)
			{
				if (useVBO)
				{
					// Zoom! Take advantage of just shoving the entire arrays to the GPU.
					gl_BindBuffer(GL_ARRAY_BUFFER, frame->vboID);
					gl_VertexPointer(3, GL_FLOAT, sizeof(vbo64_t), BUFFER_OFFSET(0));
					gl_NormalPointer(GL_FLOAT, sizeof(vbo64_t), BUFFER_OFFSET(sizeof(float) * 3));
					gl_TexCoordPointer(2, GL_FLOAT, sizeof(vbo64_t), BUFFER_OFFSET(sizeof(float) * 6));

					gl_DrawArrays(GL_TRIANGLES, 0, mesh->numTriangles * 3);
					gl_BindBuffer(GL_ARRAY_BUFFER, 0);
				}
				else
				{
					gl_VertexPointer(3, GL_FLOAT, 0, frame->vertices);
					gl_NormalPointer(GL_FLOAT, 0, frame->normals);
					gl_TexCoordPointer(2, GL_FLOAT, 0, mesh->uvs);
					gl_DrawArrays(GL_TRIANGLES, 0, mesh->numTriangles * 3);
				}
			}
			else
			{
				float *vertPtr;
				float *normPtr;
				int j = 0;

				// Dangit, I soooo want to do this in a GLSL shader...
				GLModel_AllocLerpBuffer(mesh->numVertices * sizeof(float) * 3);
				vertPtr = vertBuffer;
				normPtr = normBuffer;
				//int j = 0;

				for (j = 0; j < mesh->numVertices * 3; j++)
				{
					// Interpolate
					*vertPtr++ = frame->vertices[j] + (pol * (nextframe->vertices[j] - frame->vertices[j]));
					*normPtr++ = frame->normals[j] + (pol * (nextframe->normals[j] - frame->normals[j]));
				}

				gl_VertexPointer(3, GL_FLOAT, 0, vertBuffer);
				gl_NormalPointer(GL_FLOAT, 0, normBuffer);
				gl_TexCoordPointer(2, GL_FLOAT, 0, mesh->uvs);
				gl_DrawArrays(GL_TRIANGLES, 0, mesh->numVertices);
			}
		}
	}

	gl_DisableClientState(GL_NORMAL_ARRAY);

	gl_PopMatrix(); // should be the same as glLoadIdentity
	gl_Disable(GL_CULL_FACE);
	gl_Disable(GL_NORMALIZE);

#ifdef GL_LIGHT_MODEL_AMBIENT
	if (model_lighting)
	{
		gl_Disable(GL_LIGHTING);
		gl_ShadeModel(GL_FLAT);
	}
#endif
}

// -----------------+
// SetTransform     :
// -----------------+
static void SetTransform(FTransform *stransform)
{
	static boolean special_splitscreen;
	boolean shearing = false;
	float used_fov;

	gl_LoadIdentity();

	if (stransform)
	{
		used_fov = stransform->fovxangle;
#ifdef USE_FTRANSFORM_MIRROR
		// mirroring from Kart
		if (stransform->mirror)
			gl_Scalef(-stransform->scalex, stransform->scaley, -stransform->scalez);
		else
#endif
		if (stransform->flip)
			gl_Scalef(stransform->scalex, -stransform->scaley, -stransform->scalez);
		else
			gl_Scalef(stransform->scalex, stransform->scaley, -stransform->scalez);

		if (stransform->roll)
			gl_Rotatef(stransform->rollangle, 0.0f, 0.0f, 1.0f);
		gl_Rotatef(stransform->anglex       , 1.0f, 0.0f, 0.0f);
		gl_Rotatef(stransform->angley+270.0f, 0.0f, 1.0f, 0.0f);
		gl_Translatef(-stransform->x, -stransform->z, -stransform->y);

		special_splitscreen = stransform->splitscreen;
		shearing = stransform->shearing;
	}
	else
	{
		used_fov = fov;
		gl_Scalef(1.0f, 1.0f, -1.0f);
	}

	gl_MatrixMode(GL_PROJECTION);
	gl_LoadIdentity();

	// Simulate Software's y-shearing
	// https://zdoom.org/wiki/Y-shearing
	if (shearing)
	{
		float fdy = stransform->viewaiming * 2;
		gl_Translatef(0.0f, -fdy/BASEVIDHEIGHT, 0.0f);
	}

	if (special_splitscreen)
	{
		used_fov = (float)(atan(tan(used_fov*M_PI/360)*0.8)*360/M_PI);
		GLPerspective(used_fov, 2*ASPECT_RATIO);
	}
	else
		GLPerspective(used_fov, ASPECT_RATIO);

	gl_MatrixMode(GL_MODELVIEW);
}

static INT32 GetTextureUsed(void)
{
	return GLTexture_GetMemoryUsage(TexCacheHead);
}

static void PostImgRedraw(float points[SCREENVERTS][SCREENVERTS][2])
{
	INT32 x, y;
	float float_x, float_y, float_nextx, float_nexty;
	float xfix, yfix;
	INT32 texsize = 512;

	const float blackBack[16] =
	{
		-16.0f, -16.0f, 6.0f,
		-16.0f, 16.0f, 6.0f,
		16.0f, 16.0f, 6.0f,
		16.0f, -16.0f, 6.0f
	};

	// look for power of two that is large enough for the screen
	while (texsize < screen_width || texsize < screen_height)
		texsize <<= 1;

	// X/Y stretch fix for all resolutions(!)
	xfix = (float)(texsize)/((float)((screen_width)/(float)(SCREENVERTS-1)));
	yfix = (float)(texsize)/((float)((screen_height)/(float)(SCREENVERTS-1)));

	gl_Disable(GL_DEPTH_TEST);
	gl_Disable(GL_BLEND);

	// const float blackBack[16]

	// Draw a black square behind the screen texture,
	// so nothing shows through the edges
	gl_Color4f(white[0], white[1], white[2], white[3]);

	gl_VertexPointer(3, GL_FLOAT, 0, blackBack);
	gl_DrawArrays(GL_TRIANGLE_FAN, 0, 4);

	for(x=0;x<SCREENVERTS-1;x++)
	{
		for(y=0;y<SCREENVERTS-1;y++)
		{
			float stCoords[8];
			float vertCoords[12];

			// Used for texture coordinates
			// Annoying magic numbers to scale the square texture to
			// a non-square screen..
			float_x = (float)(x/(xfix));
			float_y = (float)(y/(yfix));
			float_nextx = (float)(x+1)/(xfix);
			float_nexty = (float)(y+1)/(yfix);

			// float stCoords[8];
			stCoords[0] = float_x;
			stCoords[1] = float_y;
			stCoords[2] = float_x;
			stCoords[3] = float_nexty;
			stCoords[4] = float_nextx;
			stCoords[5] = float_nexty;
			stCoords[6] = float_nextx;
			stCoords[7] = float_y;

			gl_TexCoordPointer(2, GL_FLOAT, 0, stCoords);

			// float vertCoords[12];
			vertCoords[0] = points[x][y][0];
			vertCoords[1] = points[x][y][1];
			vertCoords[2] = 4.4f;
			vertCoords[3] = points[x][y + 1][0];
			vertCoords[4] = points[x][y + 1][1];
			vertCoords[5] = 4.4f;
			vertCoords[6] = points[x + 1][y + 1][0];
			vertCoords[7] = points[x + 1][y + 1][1];
			vertCoords[8] = 4.4f;
			vertCoords[9] = points[x + 1][y][0];
			vertCoords[10] = points[x + 1][y][1];
			vertCoords[11] = 4.4f;

			gl_VertexPointer(3, GL_FLOAT, 0, vertCoords);

			gl_DrawArrays(GL_TRIANGLE_FAN, 0, 4);
		}
	}

	gl_Enable(GL_DEPTH_TEST);
	gl_Enable(GL_BLEND);
}

// Create Screen to fade from
static void StartScreenWipe(void)
{
	INT32 texsize = 512;
	boolean firstTime = (startScreenWipe == 0);

	// look for power of two that is large enough for the screen
	while (texsize < screen_width || texsize < screen_height)
		texsize <<= 1;

	// Create screen texture
	if (firstTime)
		gl_GenTextures(1, &startScreenWipe);
	gl_BindTexture(GL_TEXTURE_2D, startScreenWipe);

	if (firstTime)
	{
		gl_TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		gl_TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		SetClamp(GL_TEXTURE_WRAP_S);
		SetClamp(GL_TEXTURE_WRAP_T);
		gl_CopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, texsize, texsize, 0);
	}
	else
		gl_CopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, texsize, texsize);

	tex_downloaded = startScreenWipe;
}

// Create Screen to fade to
static void EndScreenWipe(void)
{
	INT32 texsize = 512;
	boolean firstTime = (endScreenWipe == 0);

	// look for power of two that is large enough for the screen
	while (texsize < screen_width || texsize < screen_height)
		texsize <<= 1;

	// Create screen texture
	if (firstTime)
		gl_GenTextures(1, &endScreenWipe);
	gl_BindTexture(GL_TEXTURE_2D, endScreenWipe);

	if (firstTime)
	{
		gl_TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		gl_TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		SetClamp(GL_TEXTURE_WRAP_S);
		SetClamp(GL_TEXTURE_WRAP_T);
		gl_CopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, texsize, texsize, 0);
	}
	else
		gl_CopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, texsize, texsize);

	tex_downloaded = endScreenWipe;
}


// Draw the last scene under the intermission
static void DrawIntermissionBG(void)
{
	INT32 texsize = 512;
	float xfix, yfix;

	const float screenVerts[12] =
	{
		-1.0f, -1.0f, 1.0f,
		-1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, -1.0f, 1.0f
	};

	float fix[8];

	// look for power of two that is large enough for the screen
	while (texsize < screen_width || texsize < screen_height)
		texsize <<= 1;

	xfix = 1/((float)(texsize)/((float)((screen_width))));
	yfix = 1/((float)(texsize)/((float)((screen_height))));

	// const float screenVerts[12]

	// float fix[8];
	fix[0] = 0.0f;
	fix[1] = 0.0f;
	fix[2] = 0.0f;
	fix[3] = yfix;
	fix[4] = xfix;
	fix[5] = yfix;
	fix[6] = xfix;
	fix[7] = 0.0f;

	gl_Clear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

	gl_BindTexture(GL_TEXTURE_2D, screentexture);
	gl_Color4f(white[0], white[1], white[2], white[3]);

	gl_TexCoordPointer(2, GL_FLOAT, 0, fix);
	gl_VertexPointer(3, GL_FLOAT, 0, screenVerts);
	gl_DrawArrays(GL_TRIANGLE_FAN, 0, 4);

	tex_downloaded = screentexture;
}

// Do screen fades!
static void DoWipe(void)
{
	INT32 texsize = 512;
	float xfix, yfix;

	INT32 fademaskdownloaded = tex_downloaded; // the fade mask that has been set

	const float screenVerts[12] =
	{
		-1.0f, -1.0f, 1.0f,
		-1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, -1.0f, 1.0f
	};

	float fix[8];

	const float defaultST[8] =
	{
		0.0f, 1.0f,
		0.0f, 0.0f,
		1.0f, 0.0f,
		1.0f, 1.0f
	};

	if (!GLExtension_multitexture)
		return;

	// look for power of two that is large enough for the screen
	while (texsize < screen_width || texsize < screen_height)
		texsize <<= 1;

	xfix = 1/((float)(texsize)/((float)((screen_width))));
	yfix = 1/((float)(texsize)/((float)((screen_height))));

	// const float screenVerts[12]

	// float fix[8];
	fix[0] = 0.0f;
	fix[1] = 0.0f;
	fix[2] = 0.0f;
	fix[3] = yfix;
	fix[4] = xfix;
	fix[5] = yfix;
	fix[6] = xfix;
	fix[7] = 0.0f;

	gl_Clear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

	SetBlend(PF_Modulated|PF_NoDepthTest);
	gl_Enable(GL_TEXTURE_2D);

	// Draw the original screen
	gl_BindTexture(GL_TEXTURE_2D, startScreenWipe);
	gl_Color4f(white[0], white[1], white[2], white[3]);
	gl_TexCoordPointer(2, GL_FLOAT, 0, fix);
	gl_VertexPointer(3, GL_FLOAT, 0, screenVerts);
	gl_DrawArrays(GL_TRIANGLE_FAN, 0, 4);

	SetBlend(PF_Modulated|PF_Translucent|PF_NoDepthTest);

	// Draw the end screen that fades in
	gl_ActiveTexture(GL_TEXTURE0);
	gl_Enable(GL_TEXTURE_2D);
	gl_BindTexture(GL_TEXTURE_2D, endScreenWipe);
	gl_TexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	gl_ActiveTexture(GL_TEXTURE1);
	gl_Enable(GL_TEXTURE_2D);
	gl_BindTexture(GL_TEXTURE_2D, fademaskdownloaded);

	gl_TexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	// const float defaultST[8]

	gl_ClientActiveTexture(GL_TEXTURE0);
	gl_TexCoordPointer(2, GL_FLOAT, 0, fix);
	gl_VertexPointer(3, GL_FLOAT, 0, screenVerts);
	gl_ClientActiveTexture(GL_TEXTURE1);
	gl_EnableClientState(GL_TEXTURE_COORD_ARRAY);
	gl_TexCoordPointer(2, GL_FLOAT, 0, defaultST);
	gl_DrawArrays(GL_TRIANGLE_FAN, 0, 4);

	gl_Disable(GL_TEXTURE_2D); // disable the texture in the 2nd texture unit
	gl_DisableClientState(GL_TEXTURE_COORD_ARRAY);

	gl_ActiveTexture(GL_TEXTURE0);
	gl_ClientActiveTexture(GL_TEXTURE0);
	tex_downloaded = endScreenWipe;
}

static void DoScreenWipe(void)
{
	DoWipe();
}

static void DoTintedWipe(boolean istowhite, boolean isfadingin)
{
	(void)istowhite;
	(void)isfadingin;
	DoWipe();
}

// Create a texture from the screen.
static void MakeScreenTexture(void)
{
	INT32 texsize = 512;
	boolean firstTime = (screentexture == 0);

	// look for power of two that is large enough for the screen
	while (texsize < screen_width || texsize < screen_height)
		texsize <<= 1;

	// Create screen texture
	if (firstTime)
		gl_GenTextures(1, &screentexture);
	gl_BindTexture(GL_TEXTURE_2D, screentexture);

	if (firstTime)
	{
		gl_TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		gl_TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		SetClamp(GL_TEXTURE_WRAP_S);
		SetClamp(GL_TEXTURE_WRAP_T);
		gl_CopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, texsize, texsize, 0);
	}
	else
		gl_CopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, texsize, texsize);

	tex_downloaded = screentexture;
}

static void MakeFinalScreenTexture(void)
{
	INT32 texsize = 512;
	boolean firstTime = (finalScreenTexture == 0);

	// look for power of two that is large enough for the screen
	while (texsize < screen_width || texsize < screen_height)
		texsize <<= 1;

	// Create screen texture
	if (firstTime)
		gl_GenTextures(1, &finalScreenTexture);
	gl_BindTexture(GL_TEXTURE_2D, finalScreenTexture);

	if (firstTime)
	{
		gl_TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		gl_TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		SetClamp(GL_TEXTURE_WRAP_S);
		SetClamp(GL_TEXTURE_WRAP_T);
		gl_CopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, texsize, texsize, 0);
	}
	else
		gl_CopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, texsize, texsize);

	tex_downloaded = finalScreenTexture;
}

static void DrawFinalScreenTexture(int width, int height)
{
	float xfix, yfix;
	float origaspect, newaspect;
	float xoff = 1, yoff = 1; // xoffset and yoffset for the polygon to have black bars around the screen
	FRGBAFloat clearColour;
	INT32 texsize = 512;

	float off[12];
	float fix[8];

	// look for power of two that is large enough for the screen
	while (texsize < screen_width || texsize < screen_height)
		texsize <<= 1;

	xfix = 1/((float)(texsize)/((float)((screen_width))));
	yfix = 1/((float)(texsize)/((float)((screen_height))));

	origaspect = (float)screen_width / screen_height;
	newaspect = (float)width / height;
	if (origaspect < newaspect)
	{
		xoff = origaspect / newaspect;
		yoff = 1;
	}
	else if (origaspect > newaspect)
	{
		xoff = 1;
		yoff = newaspect / origaspect;
	}

	// float off[12];
	off[0] = -xoff;
	off[1] = -yoff;
	off[2] = 1.0f;
	off[3] = -xoff;
	off[4] = yoff;
	off[5] = 1.0f;
	off[6] = xoff;
	off[7] = yoff;
	off[8] = 1.0f;
	off[9] = xoff;
	off[10] = -yoff;
	off[11] = 1.0f;

	// float fix[8];
	fix[0] = 0.0f;
	fix[1] = 0.0f;
	fix[2] = 0.0f;
	fix[3] = yfix;
	fix[4] = xfix;
	fix[5] = yfix;
	fix[6] = xfix;
	fix[7] = 0.0f;

	gl_Viewport(0, 0, width, height);

	clearColour.red = clearColour.green = clearColour.blue = 0;
	clearColour.alpha = 1;
	ClearBuffer(true, false, &clearColour);
	gl_BindTexture(GL_TEXTURE_2D, finalScreenTexture);

	gl_Color4f(white[0], white[1], white[2], white[3]);

	gl_TexCoordPointer(2, GL_FLOAT, 0, fix);
	gl_VertexPointer(3, GL_FLOAT, 0, off);

	gl_DrawArrays(GL_TRIANGLE_FAN, 0, 4);
	tex_downloaded = finalScreenTexture;
}

struct hwdriver_s GPU_API_OpenGLES = {
#define DEF(func) func,
	HWR_API_FUNCTIONS(DEF)
#undef DEF
};

#endif //HWRENDER
