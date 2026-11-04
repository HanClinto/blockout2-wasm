/*
  glcompat.h: the OpenGL 1.x calls BlockOut II makes that Emscripten's legacy GL emulation lacks.
  Force-included before every game source file in the web build (emcc -include web/glcompat.h).

  - Display lists (glGenLists, glNewList, glCallList...): recorded here and replayed through the emulation's
    immediate mode. The game's lists only hold glBegin/glEnd with vertices, normals, texture coordinates and
    colors, which is all the recorder keeps.
  - glMaterialfv(GL_EMISSION) isn't emulated: folded into the ambient term instead, which is exact for this game
    (its global ambient light is white and LIGHT0 has no ambient, so emission and material ambient add the same way).
  - glMaterialf(GL_SHININESS) goes to glMaterialfv; glHint(GL_PERSPECTIVE_CORRECTION_HINT) is dropped (WebGL has
    no such hint and would flag an error, which the game treats as fatal).
  - glTexImage2D with an OpenGL 1.0 component count (3, 4) as the internal format: WebGL wants GL_RGB/GL_RGBA, the
    same as the data's format.
  - gluLookAt and gluPerspective done here: Emscripten's gluLookAt (3.1.74) passes its arguments to gl-matrix's
    lookAt in the wrong order and leaves the matrix unchanged, so the game's camera faced away from everything; both
    of its GLU calls also replace the matrix where GLU multiplies onto it.
  - glRasterPos2i + glDrawPixels (the pit's level gauge) become a textured quad in window coordinates.

  This program is free software; you can redistribute it and/or modify it under the terms of the GNU General
  Public License as published by the Free Software Foundation; either version 2 of the License, or (at your
  option) any later version.
*/
#ifndef BLOCKOUT_GLCOMPAT_H
#define BLOCKOUT_GLCOMPAT_H

#include <SDL_opengl.h>
#include <GL/glu.h>

#ifdef __cplusplus
extern "C" {
#endif

GLuint bo_glGenLists(GLsizei range);
void   bo_glNewList(GLuint list, GLenum mode);
void   bo_glEndList(void);
void   bo_glCallList(GLuint list);
void   bo_glDeleteLists(GLuint list, GLsizei range);
void   bo_glBegin(GLenum mode);
void   bo_glEnd(void);
void   bo_glVertex3f(GLfloat x, GLfloat y, GLfloat z);
void   bo_glNormal3f(GLfloat x, GLfloat y, GLfloat z);
void   bo_glTexCoord2f(GLfloat s, GLfloat t);
void   bo_glColor3f(GLfloat r, GLfloat g, GLfloat b);
void   bo_glColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a);
void   bo_glMaterialfv(GLenum face, GLenum pname, const GLfloat *params);
void   bo_glMaterialf(GLenum face, GLenum pname, GLfloat param);
void   bo_glHint(GLenum target, GLenum mode);
void   bo_glRasterPos2i(GLint x, GLint y);
void   bo_glDrawPixels(GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels);
void   bo_gluLookAt(GLdouble ex, GLdouble ey, GLdouble ez, GLdouble cx, GLdouble cy, GLdouble cz,
                    GLdouble ux, GLdouble uy, GLdouble uz);
void   bo_gluPerspective(GLdouble fovy, GLdouble aspect, GLdouble zNear, GLdouble zFar);
void   bo_glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border,
                       GLenum format, GLenum type, const void *pixels);

#ifdef __cplusplus
}
#endif

#ifndef BLOCKOUT_GLCOMPAT_IMPL
#define glGenLists     bo_glGenLists
#define glNewList      bo_glNewList
#define glEndList      bo_glEndList
#define glCallList     bo_glCallList
#define glDeleteLists  bo_glDeleteLists
#define glBegin        bo_glBegin
#define glEnd          bo_glEnd
#define glVertex3f     bo_glVertex3f
#define glNormal3f     bo_glNormal3f
#define glTexCoord2f   bo_glTexCoord2f
#define glColor3f      bo_glColor3f
#define glColor4f      bo_glColor4f
#define glMaterialfv   bo_glMaterialfv
#define glMaterialf    bo_glMaterialf
#define glHint         bo_glHint
#define glRasterPos2i  bo_glRasterPos2i
#define glDrawPixels   bo_glDrawPixels
#define glTexImage2D   bo_glTexImage2D
#define gluLookAt      bo_gluLookAt
#define gluPerspective bo_gluPerspective
#endif

#endif
