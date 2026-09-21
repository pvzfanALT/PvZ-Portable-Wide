/*
 * Portions of this file are based on the PopCap Games Framework
 * Copyright (C) 2005-2009 PopCap Games, Inc.
 * 
 * Copyright (C) 2026 Zhou Qiankang <wszqkzqk@qq.com>
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later AND LicenseRef-PopCap
 *
 * This file is part of PvZ-Portable.
 *
 * PvZ-Portable is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * PvZ-Portable is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with PvZ-Portable. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef __GLPLATFORM_H__
#define __GLPLATFORM_H__

#ifdef __SWITCH__
#include <switch.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#endif

#ifdef __vita__
// PVR_PSP2 exports the whole GLES 2.0 core from libGLESv2.suprx and ships
// import stubs for it, so the entry points are resolved by the linker and
// there is nothing for a loader like glad to do.  eglGetProcAddress on this
// driver only answers for extensions, which is exactly why glad must not be
// used here.
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#else

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4551) // glad generated code triggers this on MSVC
#endif

#include <glad/gles2.h>

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif // __vita__

#include <SDL.h>

// Shared macro definitions — identical keywords in GLSL ES 1.00 and GLSL 1.20
#define GLSL_VERT_MACROS \
	"#define VERT_IN attribute\n" \
	"#define V2F varying\n"

#define GLSL_FRAG_MACROS \
	"#define V2F varying\n" \
	"#define FRAG_OUT gl_FragColor\n" \
	"#define TEX2D texture2D\n"

// When true, a desktop GL compatibility context is in use and shaders
extern bool gDesktopGLFallback;

inline void PlatformGLInit()
{
#if defined(__vita__)
	// Entry points are linked in directly; nothing to load.
#elif defined(__SWITCH__)
	gladLoadGLES2((GLADloadfunc)eglGetProcAddress);
#else
	gladLoadGLES2((GLADloadfunc)SDL_GL_GetProcAddress);
#endif
}

#endif // __GLPLATFORM_H__
