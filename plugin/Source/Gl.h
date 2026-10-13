/* SeriousPortal - the OpenGL calls, in a file of their own (the engine's headers and <GL/gl.h> don't mix). Plain
 * arrays in and out. The caller is on Sam's rendering thread with Sam's context current. */
#ifndef SERIOUSPORTAL_GL_H
#define SERIOUSPORTAL_GL_H

namespace gl {

// Functions loaded and the two textures made for the current context.
bool Ready(void);
// Copy a part of the framebuffer (x, y from the bottom left) into texture i.
void CopyToTexture(int i, int x, int y, int w, int h);
// Draw with this projection (column-major, world to clip) and depth range; depth tested, not written, blended.
void Begin(const float afProjection[16], float fDepthNear, float fDepthFar, bool bDepthTest);
void End(void);
// A triangle fan: texture i (-1: none) with projective coordinates stq (3 a vertex), or colours rgba (4 a vertex).
void Fan(int iTexture, const float *afXYZ, const float *afSTQ, const float *afRGBA, int ctVertices);
// A triangle strip in colours.
void Strip(const float *afXYZ, const float *afRGBA, int ctVertices);

}; // namespace

#endif
