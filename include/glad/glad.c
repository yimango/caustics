#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "glad.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

PFNGLCLEARPROC glad_glClear = NULL;
PFNGLCLEARCOLORPROC glad_glClearColor = NULL;
PFNGLDRAWELEMENTSPROC glad_glDrawElements = NULL;
PFNGLVIEWPORTPROC glad_glViewport = NULL;
PFNGLENABLEPROC glad_glEnable = NULL;
PFNGLDISABLEPROC glad_glDisable = NULL;
PFNGLCULLFACEPROC glad_glCullFace = NULL;
PFNGLBLENDFUNCPROC glad_glBlendFunc = NULL;
PFNGLDEPTHMASKPROC glad_glDepthMask = NULL;
PFNGLDEPTHFUNCPROC glad_glDepthFunc = NULL;
PFNGLDRAWBUFFERSPROC glad_glDrawBuffers = NULL;
PFNGLREADBUFFERPROC glad_glReadBuffer = NULL;
PFNGLDRAWARRAYSPROC glad_glDrawArrays = NULL;
PFNGLREADPIXELSPROC glad_glReadPixels = NULL;
PFNGLPIXELSTOREIPROC glad_glPixelStorei = NULL;
PFNGLGETERRORPROC glad_glGetError = NULL;

PFNGLGENVERTEXARRAYSPROC glGenVertexArrays = NULL;
PFNGLBINDVERTEXARRAYPROC glBindVertexArray = NULL;
PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays = NULL;
PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray = NULL;
PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer = NULL;

PFNGLGENBUFFERSPROC glGenBuffers = NULL;
PFNGLBINDBUFFERPROC glBindBuffer = NULL;
PFNGLBUFFERDATAPROC glBufferData = NULL;
PFNGLBUFFERSUBDATAPROC glBufferSubData = NULL;
PFNGLDELETEBUFFERSPROC glDeleteBuffers = NULL;

PFNGLCREATESHADERPROC glCreateShader = NULL;
PFNGLSHADERSOURCEPROC glShaderSource = NULL;
PFNGLCOMPILESHADERPROC glCompileShader = NULL;
PFNGLGETSHADERIVPROC glGetShaderiv = NULL;
PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog = NULL;
PFNGLDELETESHADERPROC glDeleteShader = NULL;

PFNGLCREATEPROGRAMPROC glCreateProgram = NULL;
PFNGLATTACHSHADERPROC glAttachShader = NULL;
PFNGLLINKPROGRAMPROC glLinkProgram = NULL;
PFNGLGETPROGRAMIVPROC glGetProgramiv = NULL;
PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog = NULL;
PFNGLUSEPROGRAMPROC glUseProgram = NULL;
PFNGLDELETEPROGRAMPROC glDeleteProgram = NULL;

PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation = NULL;
PFNGLUNIFORM1IPROC glUniform1i = NULL;
PFNGLUNIFORM1UIPROC glUniform1ui = NULL;
PFNGLUNIFORM1FPROC glUniform1f = NULL;
PFNGLUNIFORM2FVPROC glUniform2fv = NULL;
PFNGLUNIFORM3FVPROC glUniform3fv = NULL;
PFNGLUNIFORM4FVPROC glUniform4fv = NULL;
PFNGLUNIFORMMATRIX4FVPROC glUniformMatrix4fv = NULL;

PFNGLGENTEXTURESPROC glGenTextures = NULL;
PFNGLBINDTEXTUREPROC glBindTexture = NULL;
PFNGLTEXIMAGE2DPROC glTexImage2D = NULL;
PFNGLTEXSUBIMAGE2DPROC glTexSubImage2D = NULL;
PFNGLTEXPARAMETERIPROC glTexParameteri = NULL;
PFNGLTEXPARAMETERFVPROC glad_glTexParameterfv = NULL;
PFNGLGENERATEMIPMAPPROC glGenerateMipmap = NULL;
PFNGLACTIVETEXTUREPROC glActiveTexture = NULL;
PFNGLDELETETEXTURESPROC glDeleteTextures = NULL;

PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers = NULL;
PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer = NULL;
PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D = NULL;
PFNGLFRAMEBUFFERRENDERBUFFERPROC glFramebufferRenderbuffer = NULL;
PFNGLCHECKFRAMEBUFFERSTATUSPROC glCheckFramebufferStatus = NULL;
PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers = NULL;
PFNGLBLITFRAMEBUFFERPROC glBlitFramebuffer = NULL;

PFNGLGENRENDERBUFFERSPROC glGenRenderbuffers = NULL;
PFNGLBINDRENDERBUFFERPROC glBindRenderbuffer = NULL;
PFNGLRENDERBUFFERSTORAGEPROC glRenderbufferStorage = NULL;
PFNGLDELETERENDERBUFFERSPROC glDeleteRenderbuffers = NULL;

static void* get_proc(GLADloadproc load, const char *name) {
    void *proc = load(name);
    if (!proc) {
        fprintf(stderr, "Failed to load %s\n", name);
    }
    return proc;
}

int gladLoadGLLoader(GLADloadproc load) {
    if (!load) {
        return 0;
    }

    glad_glClear = (PFNGLCLEARPROC)get_proc(load, "glClear");
    glad_glClearColor = (PFNGLCLEARCOLORPROC)get_proc(load, "glClearColor");
    glad_glDrawElements = (PFNGLDRAWELEMENTSPROC)get_proc(load, "glDrawElements");
    glad_glViewport = (PFNGLVIEWPORTPROC)get_proc(load, "glViewport");
    glad_glEnable = (PFNGLENABLEPROC)get_proc(load, "glEnable");
    glad_glDisable = (PFNGLDISABLEPROC)get_proc(load, "glDisable");
    glad_glCullFace = (PFNGLCULLFACEPROC)get_proc(load, "glCullFace");
    glad_glBlendFunc = (PFNGLBLENDFUNCPROC)get_proc(load, "glBlendFunc");
    glad_glDepthMask = (PFNGLDEPTHMASKPROC)get_proc(load, "glDepthMask");
    glad_glDepthFunc = (PFNGLDEPTHFUNCPROC)get_proc(load, "glDepthFunc");
    glad_glDrawBuffers = (PFNGLDRAWBUFFERSPROC)get_proc(load, "glDrawBuffers");
    glad_glReadBuffer = (PFNGLREADBUFFERPROC)get_proc(load, "glReadBuffer");
    glad_glDrawArrays = (PFNGLDRAWARRAYSPROC)get_proc(load, "glDrawArrays");
    glad_glReadPixels = (PFNGLREADPIXELSPROC)get_proc(load, "glReadPixels");
    glad_glPixelStorei = (PFNGLPIXELSTOREIPROC)get_proc(load, "glPixelStorei");
    glad_glGetError = (PFNGLGETERRORPROC)get_proc(load, "glGetError");

    glGenVertexArrays = (PFNGLGENVERTEXARRAYSPROC)get_proc(load, "glGenVertexArrays");
    glBindVertexArray = (PFNGLBINDVERTEXARRAYPROC)get_proc(load, "glBindVertexArray");
    glDeleteVertexArrays = (PFNGLDELETEVERTEXARRAYSPROC)get_proc(load, "glDeleteVertexArrays");
    glEnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC)get_proc(load, "glEnableVertexAttribArray");
    glVertexAttribPointer = (PFNGLVERTEXATTRIBPOINTERPROC)get_proc(load, "glVertexAttribPointer");

    glGenBuffers = (PFNGLGENBUFFERSPROC)get_proc(load, "glGenBuffers");
    glBindBuffer = (PFNGLBINDBUFFERPROC)get_proc(load, "glBindBuffer");
    glBufferData = (PFNGLBUFFERDATAPROC)get_proc(load, "glBufferData");
    glBufferSubData = (PFNGLBUFFERSUBDATAPROC)get_proc(load, "glBufferSubData");
    glDeleteBuffers = (PFNGLDELETEBUFFERSPROC)get_proc(load, "glDeleteBuffers");

    glCreateShader = (PFNGLCREATESHADERPROC)get_proc(load, "glCreateShader");
    glShaderSource = (PFNGLSHADERSOURCEPROC)get_proc(load, "glShaderSource");
    glCompileShader = (PFNGLCOMPILESHADERPROC)get_proc(load, "glCompileShader");
    glGetShaderiv = (PFNGLGETSHADERIVPROC)get_proc(load, "glGetShaderiv");
    glGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC)get_proc(load, "glGetShaderInfoLog");
    glDeleteShader = (PFNGLDELETESHADERPROC)get_proc(load, "glDeleteShader");

    glCreateProgram = (PFNGLCREATEPROGRAMPROC)get_proc(load, "glCreateProgram");
    glAttachShader = (PFNGLATTACHSHADERPROC)get_proc(load, "glAttachShader");
    glLinkProgram = (PFNGLLINKPROGRAMPROC)get_proc(load, "glLinkProgram");
    glGetProgramiv = (PFNGLGETPROGRAMIVPROC)get_proc(load, "glGetProgramiv");
    glGetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC)get_proc(load, "glGetProgramInfoLog");
    glUseProgram = (PFNGLUSEPROGRAMPROC)get_proc(load, "glUseProgram");
    glDeleteProgram = (PFNGLDELETEPROGRAMPROC)get_proc(load, "glDeleteProgram");

    glGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)get_proc(load, "glGetUniformLocation");
    glUniform1i = (PFNGLUNIFORM1IPROC)get_proc(load, "glUniform1i");
    glUniform1ui = (PFNGLUNIFORM1UIPROC)get_proc(load, "glUniform1ui");
    glUniform1f = (PFNGLUNIFORM1FPROC)get_proc(load, "glUniform1f");
    glUniform2fv = (PFNGLUNIFORM2FVPROC)get_proc(load, "glUniform2fv");
    glUniform3fv = (PFNGLUNIFORM3FVPROC)get_proc(load, "glUniform3fv");
    glUniform4fv = (PFNGLUNIFORM4FVPROC)get_proc(load, "glUniform4fv");
    glUniformMatrix4fv = (PFNGLUNIFORMMATRIX4FVPROC)get_proc(load, "glUniformMatrix4fv");

    glGenTextures = (PFNGLGENTEXTURESPROC)get_proc(load, "glGenTextures");
    glBindTexture = (PFNGLBINDTEXTUREPROC)get_proc(load, "glBindTexture");
    glTexImage2D = (PFNGLTEXIMAGE2DPROC)get_proc(load, "glTexImage2D");
    glTexSubImage2D = (PFNGLTEXSUBIMAGE2DPROC)get_proc(load, "glTexSubImage2D");
    glTexParameteri = (PFNGLTEXPARAMETERIPROC)get_proc(load, "glTexParameteri");
    glad_glTexParameterfv = (PFNGLTEXPARAMETERFVPROC)get_proc(load, "glTexParameterfv");
    glGenerateMipmap = (PFNGLGENERATEMIPMAPPROC)get_proc(load, "glGenerateMipmap");
    glActiveTexture = (PFNGLACTIVETEXTUREPROC)get_proc(load, "glActiveTexture");
    glDeleteTextures = (PFNGLDELETETEXTURESPROC)get_proc(load, "glDeleteTextures");

    glGenFramebuffers = (PFNGLGENFRAMEBUFFERSPROC)get_proc(load, "glGenFramebuffers");
    glBindFramebuffer = (PFNGLBINDFRAMEBUFFERPROC)get_proc(load, "glBindFramebuffer");
    glFramebufferTexture2D = (PFNGLFRAMEBUFFERTEXTURE2DPROC)get_proc(load, "glFramebufferTexture2D");
    glFramebufferRenderbuffer = (PFNGLFRAMEBUFFERRENDERBUFFERPROC)get_proc(load, "glFramebufferRenderbuffer");
    glCheckFramebufferStatus = (PFNGLCHECKFRAMEBUFFERSTATUSPROC)get_proc(load, "glCheckFramebufferStatus");
    glDeleteFramebuffers = (PFNGLDELETEFRAMEBUFFERSPROC)get_proc(load, "glDeleteFramebuffers");
    glBlitFramebuffer = (PFNGLBLITFRAMEBUFFERPROC)get_proc(load, "glBlitFramebuffer");

    glGenRenderbuffers = (PFNGLGENRENDERBUFFERSPROC)get_proc(load, "glGenRenderbuffers");
    glBindRenderbuffer = (PFNGLBINDRENDERBUFFERPROC)get_proc(load, "glBindRenderbuffer");
    glRenderbufferStorage = (PFNGLRENDERBUFFERSTORAGEPROC)get_proc(load, "glRenderbufferStorage");
    glDeleteRenderbuffers = (PFNGLDELETERENDERBUFFERSPROC)get_proc(load, "glDeleteRenderbuffers");

    return glad_glClear != NULL && glCreateShader != NULL && glGenFramebuffers != NULL;
}
