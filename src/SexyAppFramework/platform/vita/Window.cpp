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

#include <psp2/power.h>

#include <SDL.h>

#include <cstdio>

#include "SexyAppBase.h"
#include "graphics/GLInterface.h"
#include "graphics/GLImage.h"
#include "graphics/GLPlatform.h"
#include "widget/WidgetManager.h"
#include "VitaPlatform.h"

using namespace Sexy;

void SexyAppBase::MakeWindow()
{
	if (!mWindow)
	{
		// The front panel is polled through SDL's touch events by our own input
		// backend, which also drives a virtual pointer for the sticks. Letting
		// SDL synthesize mouse events on top of that would fight with it.
		SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
		// The rear panel is easy to brush against while holding the console, and
		// PvZ has no use for it, so keep it from generating events at all.
		SDL_setenv("VITA_DISABLE_TOUCH_BACK", "1", 1);

		// PvZ pushes a lot of reanimations through the CPU; run the hardware at
		// its documented maximum instead of the conservative boot clocks.
		scePowerSetArmClockFrequency(444);
		scePowerSetBusClockFrequency(222);
		scePowerSetGpuClockFrequency(222);
		scePowerSetGpuXbarClockFrequency(166);

		if (SDL_Init(SDL_INIT_VIDEO) < 0)
		{
			fprintf(stderr, "SDL_Init(SDL_INIT_VIDEO) failed: %s\n", SDL_GetError());
			return;
		}

		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
		SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
		SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
		SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
		SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
		// The renderer is pure 2D: skip the depth/stencil buffers so the driver
		// does not reserve tile memory for them.
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
		SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);

		mWindow = (void*)SDL_CreateWindow(
			mTitle.c_str(),
			SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
			VITA_SCREEN_WIDTH, VITA_SCREEN_HEIGHT,
			SDL_WINDOW_OPENGL);

		if (mWindow)
			mContext = (void*)SDL_GL_CreateContext((SDL_Window*)mWindow);

		if (!mContext)
		{
			// Retry with whatever framebuffer configuration the driver prefers
			// before giving up: an exact RGBA8/no-depth config is a request, not
			// a guarantee.
			if (mWindow) { SDL_DestroyWindow((SDL_Window*)mWindow); mWindow = nullptr; }

			SDL_GL_ResetAttributes();
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

			mWindow = (void*)SDL_CreateWindow(
				mTitle.c_str(),
				SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
				VITA_SCREEN_WIDTH, VITA_SCREEN_HEIGHT,
				SDL_WINDOW_OPENGL);

			if (mWindow)
				mContext = (void*)SDL_GL_CreateContext((SDL_Window*)mWindow);

			if (!mContext)
			{
				if (mWindow) { SDL_DestroyWindow((SDL_Window*)mWindow); mWindow = nullptr; }
				fprintf(stderr,
					"Failed to create an OpenGL ES 2.0 context: %s\n"
					"Check that libgpu_es4_ext.suprx, libIMGEGL.suprx, libGLESv2.suprx and\n"
					"libpvrPSP2_WSEGL.suprx are present in the app's module/ directory and\n"
					"that unsafe homebrew is enabled.\n", SDL_GetError());
				return;
			}
		}

		SDL_GL_SetSwapInterval(1);
	}

	if (mGLInterface == nullptr)
	{
		mGLInterface = new GLInterface(this);
		if (!InitGLInterface())
		{
			delete mGLInterface;
			mGLInterface = nullptr;
			return;
		}
	}

	bool isActive = mActive;
	mActive = true;

	mPhysMinimized = false;
	if (mMinimized)
	{
		if (mMuteOnLostFocus)
			Unmute(true);

		mMinimized = false;
		isActive = mActive; // set this here so we don't call RehupFocus again.
		RehupFocus();
	}

	if (isActive != mActive)
		RehupFocus();

	ReInitImages();

	mWidgetManager->mImage = mGLInterface->GetScreenImage();
	mWidgetManager->MarkAllDirty();

	mGLInterface->UpdateViewport();
	mWidgetManager->Resize(mScreenBounds, mGLInterface->mPresentationRect);
}
