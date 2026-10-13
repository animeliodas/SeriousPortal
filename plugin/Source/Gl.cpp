/* SeriousPortal - the OpenGL calls. See Gl.h. (No engine headers here.) */
#include <windows.h>
#include <GL/gl.h>
#include "Gl.h"

#define GL_CLAMP_TO_EDGE_ 0x812F
#define GL_TEXTURE0_ 0x84C0
#define GL_ACTIVE_TEXTURE_ 0x84E0
#define GL_CURRENT_PROGRAM_ 0x8B8D

typedef void (APIENTRY *PFN_ActiveTexture)(GLenum);
typedef void (APIENTRY *PFN_UseProgram)(GLuint);

namespace gl {

static PFN_ActiveTexture p_glActiveTexture = NULL;
static PFN_UseProgram p_glUseProgram = NULL;
static HGLRC _hglrc = NULL;
static GLuint _atex[2] = { 0, 0 };
static GLint _iSavedProgram = 0, _iSavedActive = GL_TEXTURE0_;

bool Ready(void) {
  HGLRC hglrc = wglGetCurrentContext();
  if (hglrc == NULL) return false;
  if (hglrc != _hglrc) {
    _hglrc = hglrc;
    p_glActiveTexture = (PFN_ActiveTexture)wglGetProcAddress("glActiveTexture");
    p_glUseProgram = (PFN_UseProgram)wglGetProcAddress("glUseProgram");
    _atex[0] = _atex[1] = 0; // (made in another context: gone)
  }
  if (_atex[0] == 0) {
    glPushAttrib(GL_TEXTURE_BIT);
    glGenTextures(2, _atex);
    for (int i = 0; i < 2; i++) {
      glBindTexture(GL_TEXTURE_2D, _atex[i]);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE_);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE_);
    }
    glPopAttrib();
  }
  return true;
};

void CopyToTexture(int i, int x, int y, int w, int h) {
  GLint iActive = GL_TEXTURE0_;
  glGetIntegerv(GL_ACTIVE_TEXTURE_, &iActive);
  if (p_glActiveTexture != NULL) p_glActiveTexture(GL_TEXTURE0_);
  glPushAttrib(GL_TEXTURE_BIT);
  glBindTexture(GL_TEXTURE_2D, _atex[i & 1]);
  glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, x, y, w, h, 0);
  glPopAttrib();
  if (p_glActiveTexture != NULL) p_glActiveTexture((GLenum)iActive);
};

void Begin(const float afProjection[16], float fDepthNear, float fDepthFar, bool bDepthTest) {
  glGetIntegerv(GL_CURRENT_PROGRAM_, &_iSavedProgram);
  glGetIntegerv(GL_ACTIVE_TEXTURE_, &_iSavedActive);
  glPushAttrib(GL_ALL_ATTRIB_BITS);
  if (p_glUseProgram != NULL) p_glUseProgram(0);
  if (p_glActiveTexture != NULL) {
    for (int u = 3; u >= 0; u--) {
      p_glActiveTexture(GL_TEXTURE0_ + u);
      glDisable(GL_TEXTURE_2D);
    }
  }
  glMatrixMode(GL_TEXTURE);    glPushMatrix(); glLoadIdentity();
  glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadMatrixf(afProjection);
  glMatrixMode(GL_MODELVIEW);  glPushMatrix(); glLoadIdentity();

  glDepthRange(fDepthNear, fDepthFar);
  if (bDepthTest) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);
  glDepthMask(GL_FALSE);
  glDisable(GL_CULL_FACE);
  glDisable(GL_LIGHTING);
  glDisable(GL_FOG);
  glDisable(GL_ALPHA_TEST);
  glDisable(GL_STENCIL_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glShadeModel(GL_SMOOTH);
  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
};

void End(void) {
  glMatrixMode(GL_TEXTURE);    glPopMatrix();
  glMatrixMode(GL_PROJECTION); glPopMatrix();
  glMatrixMode(GL_MODELVIEW);  glPopMatrix();
  glPopAttrib();
  if (p_glActiveTexture != NULL) p_glActiveTexture((GLenum)_iSavedActive);
  if (p_glUseProgram != NULL) p_glUseProgram((GLuint)_iSavedProgram);
};

void Fan(int iTexture, const float *afXYZ, const float *afSTQ, const float *afRGBA, int ctVertices) {
  if (iTexture >= 0) {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, _atex[iTexture & 1]);
    glColor4f(1, 1, 1, 1);
  } else {
    glDisable(GL_TEXTURE_2D);
  }
  glBegin(GL_TRIANGLE_FAN);
  for (int i = 0; i < ctVertices; i++) {
    if (iTexture >= 0) glTexCoord4f(afSTQ[i * 3 + 0], afSTQ[i * 3 + 1], 0.0f, afSTQ[i * 3 + 2]);
    else glColor4fv(afRGBA + i * 4);
    glVertex3fv(afXYZ + i * 3);
  }
  glEnd();
  glDisable(GL_TEXTURE_2D);
};

void Strip(const float *afXYZ, const float *afRGBA, int ctVertices) {
  glDisable(GL_TEXTURE_2D);
  glBegin(GL_TRIANGLE_STRIP);
  for (int i = 0; i < ctVertices; i++) {
    glColor4fv(afRGBA + i * 4);
    glVertex3fv(afXYZ + i * 3);
  }
  glEnd();
};

}; // namespace
