/*
  glcompat.cpp: see glcompat.h.

  This program is free software; you can redistribute it and/or modify it under the terms of the GNU General
  Public License as published by the Free Software Foundation; either version 2 of the License, or (at your
  option) any later version.
*/
#define BLOCKOUT_GLCOMPAT_IMPL
#include "glcompat.h"
#include <map>
#include <vector>
#include <stdio.h>
#include <string.h>
#include <math.h>

namespace {

enum OpKind { OP_BEGIN, OP_END, OP_VERTEX, OP_NORMAL, OP_TEXCOORD, OP_COLOR };

struct Op {
  OpKind kind;
  GLenum mode;
  GLfloat v[4];
};

std::map<GLuint, std::vector<Op> > lists;
GLuint nextList = 1;
GLuint recording = 0;   // the list being compiled, 0 if none
GLenum recordMode = 0;  // GL_COMPILE or GL_COMPILE_AND_EXECUTE

bool executing() { return recording == 0 || recordMode == GL_COMPILE_AND_EXECUTE; }

void record(OpKind kind, GLenum mode, GLfloat a, GLfloat b, GLfloat c, GLfloat d) {
  if (!recording) return;
  Op op;
  op.kind = kind;
  op.mode = mode;
  op.v[0] = a; op.v[1] = b; op.v[2] = c; op.v[3] = d;
  lists[recording].push_back(op);
}

// Material: emission folded into ambient (see glcompat.h)
GLfloat matAmbient[4]  = { 0.2f, 0.2f, 0.2f, 1.0f };
GLfloat matEmission[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

void applyAmbient(GLenum face) {
  GLfloat sum[4] = { matAmbient[0] + matEmission[0], matAmbient[1] + matEmission[1],
                     matAmbient[2] + matEmission[2], matAmbient[3] };
  glMaterialfv(face, GL_AMBIENT, sum);
}

// glRasterPos2i, in window coordinates
GLfloat rasterX = 0, rasterY = 0;
bool rasterValid = false;
GLuint pixelTexture = 0;
std::vector<unsigned char> pixelBytes;

void transform(const GLfloat *m, const GLfloat *in, GLfloat *out) {
  for (int r = 0; r < 4; r++)
    out[r] = m[r] * in[0] + m[4 + r] * in[1] + m[8 + r] * in[2] + m[12 + r] * in[3];
}

}  // namespace

extern "C" {

GLuint bo_glGenLists(GLsizei range) {
  GLuint first = nextList;
  for (GLsizei i = 0; i < range; i++) lists[nextList++].clear();
  return first;
}

void bo_glNewList(GLuint list, GLenum mode) {
  recording = list;
  recordMode = mode;
  lists[list].clear();
}

void bo_glEndList(void) { recording = 0; }

void bo_glCallList(GLuint list) {
  std::map<GLuint, std::vector<Op> >::iterator it = lists.find(list);
  if (it == lists.end()) return;
  const std::vector<Op> &ops = it->second;
  for (size_t i = 0; i < ops.size(); i++) {
    const Op &op = ops[i];
    switch (op.kind) {
      case OP_BEGIN:    glBegin(op.mode); break;
      case OP_END:      glEnd(); break;
      case OP_VERTEX:   glVertex3f(op.v[0], op.v[1], op.v[2]); break;
      case OP_NORMAL:   glNormal3f(op.v[0], op.v[1], op.v[2]); break;
      case OP_TEXCOORD: glTexCoord2f(op.v[0], op.v[1]); break;
      case OP_COLOR:    glColor4f(op.v[0], op.v[1], op.v[2], op.v[3]); break;
    }
  }
}

void bo_glDeleteLists(GLuint list, GLsizei range) {
  for (GLsizei i = 0; i < range; i++) lists.erase(list + i);
}

void bo_glBegin(GLenum mode) {
  record(OP_BEGIN, mode, 0, 0, 0, 0);
  if (executing()) glBegin(mode);
}

void bo_glEnd(void) {
  record(OP_END, 0, 0, 0, 0, 0);
  if (executing()) glEnd();
}

void bo_glVertex3f(GLfloat x, GLfloat y, GLfloat z) {
  record(OP_VERTEX, 0, x, y, z, 1);
  if (executing()) glVertex3f(x, y, z);
}

void bo_glNormal3f(GLfloat x, GLfloat y, GLfloat z) {
  record(OP_NORMAL, 0, x, y, z, 0);
  if (executing()) glNormal3f(x, y, z);
}

void bo_glTexCoord2f(GLfloat s, GLfloat t) {
  record(OP_TEXCOORD, 0, s, t, 0, 0);
  if (executing()) glTexCoord2f(s, t);
}

void bo_glColor3f(GLfloat r, GLfloat g, GLfloat b) { bo_glColor4f(r, g, b, 1.0f); }

void bo_glColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a) {
  record(OP_COLOR, 0, r, g, b, a);
  if (executing()) glColor4f(r, g, b, a);
}

void bo_glMaterialfv(GLenum face, GLenum pname, const GLfloat *params) {
  if (recording) fprintf(stderr, "glcompat: glMaterialfv inside a display list isn't recorded\n");
  switch (pname) {
    case GL_AMBIENT:
      memcpy(matAmbient, params, sizeof(matAmbient));
      applyAmbient(face);
      break;
    case GL_EMISSION:
      memcpy(matEmission, params, sizeof(matEmission));
      applyAmbient(face);
      break;
    case GL_AMBIENT_AND_DIFFUSE:
      memcpy(matAmbient, params, sizeof(matAmbient));
      applyAmbient(face);
      glMaterialfv(face, GL_DIFFUSE, params);
      break;
    default:
      glMaterialfv(face, pname, params);
  }
}

void bo_glMaterialf(GLenum face, GLenum pname, GLfloat param) {
  GLfloat v[4] = { param, 0, 0, 0 };
  glMaterialfv(face, pname, v);
}

void bo_glHint(GLenum target, GLenum mode) {
  if (target == GL_PERSPECTIVE_CORRECTION_HINT) return;
  glHint(target, mode);
}

void bo_glRasterPos2i(GLint x, GLint y) {
  GLfloat mv[16], pr[16], in[4] = { (GLfloat)x, (GLfloat)y, 0, 1 }, eye[4], clip[4];
  GLint vp[4];
  glGetFloatv(GL_MODELVIEW_MATRIX, mv);
  glGetFloatv(GL_PROJECTION_MATRIX, pr);
  glGetIntegerv(GL_VIEWPORT, vp);
  transform(mv, in, eye);
  transform(pr, eye, clip);
  rasterValid = clip[3] != 0;
  if (!rasterValid) return;
  rasterX = vp[0] + (clip[0] / clip[3] + 1) * 0.5f * vp[2];
  rasterY = vp[1] + (clip[1] / clip[3] + 1) * 0.5f * vp[3];
}

// Bottom row first, as glDrawPixels takes them; drawn with the current blend state, no lighting or depth.
void bo_glDrawPixels(GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels) {
  if (!rasterValid || format != GL_RGBA || width <= 0 || height <= 0) return;
  size_t n = (size_t)width * height * 4;
  pixelBytes.resize(n);
  if (type == GL_FLOAT) {
    const GLfloat *src = (const GLfloat *)pixels;
    for (size_t i = 0; i < n; i++) {
      GLfloat f = src[i] < 0 ? 0 : (src[i] > 1 ? 1 : src[i]);
      pixelBytes[i] = (unsigned char)(f * 255.0f + 0.5f);
    }
  } else if (type == GL_UNSIGNED_BYTE) {
    memcpy(&pixelBytes[0], pixels, n);
  } else {
    return;
  }

  GLint vp[4], oldTexture = 0;
  glGetIntegerv(GL_VIEWPORT, vp);
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldTexture);
  GLboolean hadTexture = glIsEnabled(GL_TEXTURE_2D);

  if (!pixelTexture) glGenTextures(1, &pixelTexture);
  glBindTexture(GL_TEXTURE_2D, pixelTexture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, &pixelBytes[0]);
  glEnable(GL_TEXTURE_2D);

  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  glOrtho(vp[0], vp[0] + vp[2], vp[1], vp[1] + vp[3], -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();

  GLfloat x0 = rasterX, y0 = rasterY, x1 = rasterX + width, y1 = rasterY + height;
  glColor4f(1, 1, 1, 1);
  glBegin(GL_QUADS);
  glTexCoord2f(0, 0); glVertex3f(x0, y0, 0);
  glTexCoord2f(1, 0); glVertex3f(x1, y0, 0);
  glTexCoord2f(1, 1); glVertex3f(x1, y1, 0);
  glTexCoord2f(0, 1); glVertex3f(x0, y1, 0);
  glEnd();

  glPopMatrix();
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);

  glBindTexture(GL_TEXTURE_2D, oldTexture);
  if (!hadTexture) glDisable(GL_TEXTURE_2D);
}

void bo_glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border,
                     GLenum format, GLenum type, const void *pixels) {
  glTexImage2D(target, level, (GLint)format, width, height, border, format, type, pixels);
}

// As GLU does it (Mesa's), multiplied onto the current matrix
void bo_gluLookAt(GLdouble ex, GLdouble ey, GLdouble ez, GLdouble cx, GLdouble cy, GLdouble cz,
                  GLdouble ux, GLdouble uy, GLdouble uz) {
  double f[3] = { cx - ex, cy - ey, cz - ez };
  double n = sqrt(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]);
  if (n == 0) return;
  f[0] /= n; f[1] /= n; f[2] /= n;
  double s[3] = { f[1] * uz - f[2] * uy, f[2] * ux - f[0] * uz, f[0] * uy - f[1] * ux };
  n = sqrt(s[0] * s[0] + s[1] * s[1] + s[2] * s[2]);
  if (n == 0) return;
  s[0] /= n; s[1] /= n; s[2] /= n;
  double u[3] = { s[1] * f[2] - s[2] * f[1], s[2] * f[0] - s[0] * f[2], s[0] * f[1] - s[1] * f[0] };
  GLfloat m[16] = {
    (GLfloat)s[0], (GLfloat)u[0], (GLfloat)-f[0], 0,
    (GLfloat)s[1], (GLfloat)u[1], (GLfloat)-f[1], 0,
    (GLfloat)s[2], (GLfloat)u[2], (GLfloat)-f[2], 0,
    0, 0, 0, 1 };
  glMultMatrixf(m);
  glTranslatef((GLfloat)-ex, (GLfloat)-ey, (GLfloat)-ez);
}

void bo_gluPerspective(GLdouble fovy, GLdouble aspect, GLdouble zNear, GLdouble zFar) {
  double rad = fovy / 2 * M_PI / 180;
  double sine = sin(rad), dz = zFar - zNear;
  if (dz == 0 || sine == 0 || aspect == 0) return;
  double cot = cos(rad) / sine;
  GLfloat m[16] = {
    (GLfloat)(cot / aspect), 0, 0, 0,
    0, (GLfloat)cot, 0, 0,
    0, 0, (GLfloat)(-(zFar + zNear) / dz), -1,
    0, 0, (GLfloat)(-2 * zNear * zFar / dz), 0 };
  glMultMatrixf(m);
}

}  // extern "C"
