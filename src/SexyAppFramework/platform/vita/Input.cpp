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

#include <psp2/ctrl.h>
#include <psp2/apputil.h>
#include <psp2/system_param.h>

#include <SDL.h>

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "SexyAppBase.h"
#include "graphics/GLInterface.h"
#include "graphics/GLImage.h"
#include "widget/WidgetManager.h"
#include "misc/KeyCodes.h"
#include "VitaPlatform.h"

using namespace Sexy;

namespace
{

// How far the pointer travels per second at full stick deflection, measured in
// screen widths.
constexpr float STICK_SPEED = 1.4f;
constexpr float STICK_SPEED_PRECISE = 0.35f; // right stick, for placing plants
constexpr float STICK_DEAD_ZONE = 0.22f;

struct VitaInput
{
	unsigned int mPrevButtons;
	float mCursorX;
	float mCursorY;
	int mLastSentX;
	int mLastSentY;
	int mMouseDownButton;  // 0 = up, 1 = left, -1 = right
	bool mTouchDown;
	bool mCircleIsEnter;   // JP systems swap the roles of Cross and Circle
	uint32_t mLastPollTick;
};

VitaInput gInput = { 0, 0.0f, 0.0f, -1, -1, 0, false, false, 0 };

// Raw stick byte (0..255, centred on 128) to a -1..1 axis with a dead zone and
// a squared response, which keeps small deflections usable for fine aiming.
float StickAxis(unsigned char theRaw)
{
	float aValue = (static_cast<float>(theRaw) - 128.0f) / 127.0f;
	if (aValue > 1.0f)
		aValue = 1.0f;
	else if (aValue < -1.0f)
		aValue = -1.0f;

	float aMagnitude = std::fabs(aValue);
	if (aMagnitude <= STICK_DEAD_ZONE)
		return 0.0f;

	aMagnitude = (aMagnitude - STICK_DEAD_ZONE) / (1.0f - STICK_DEAD_ZONE);
	aMagnitude *= aMagnitude;
	return aValue < 0.0f ? -aMagnitude : aMagnitude;
}

// Push the pointer to an absolute position in app coordinates. theFromPointer
// tells the app whether to paint the virtual pointer: touch input hides it
// again so the finger is the only thing pointing at the screen.
void MoveCursor(SexyAppBase* theApp, float theX, float theY, bool theFromPointer)
{
	const float aMaxX = static_cast<float>(theApp->mWidth - 1);
	const float aMaxY = static_cast<float>(theApp->mHeight - 1);

	gInput.mCursorX = theX < 0.0f ? 0.0f : (theX > aMaxX ? aMaxX : theX);
	gInput.mCursorY = theY < 0.0f ? 0.0f : (theY > aMaxY ? aMaxY : theY);

	const int aX = static_cast<int>(gInput.mCursorX);
	const int aY = static_cast<int>(gInput.mCursorY);

	const bool aVisibilityChanged = theApp->mSoftwareCursorEnabled != theFromPointer;
	theApp->mSoftwareCursorEnabled = theFromPointer;

	if (aX == gInput.mLastSentX && aY == gInput.mLastSentY && !aVisibilityChanged)
		return;

	gInput.mLastSentX = aX;
	gInput.mLastSentY = aY;

	theApp->mLastUserInputTick = theApp->mLastTimerTime;
	theApp->mWidgetManager->MouseMove(aX, aY);

	// The pointer is painted on top of the finished frame, so the frame has to
	// be redrawn whenever it moves or is shown/hidden.
	if (theFromPointer || aVisibilityChanged)
		theApp->mWidgetManager->MarkAllDirty();
}

void PressMouse(SexyAppBase* theApp, int theButton)
{
	if (gInput.mMouseDownButton != 0)
		return;

	gInput.mMouseDownButton = theButton;
	theApp->mLastUserInputTick = theApp->mLastTimerTime;
	theApp->mWidgetManager->MouseDown(gInput.mLastSentX, gInput.mLastSentY, theButton);
}

void ReleaseMouse(SexyAppBase* theApp, int theButton)
{
	if (gInput.mMouseDownButton != theButton)
		return;

	gInput.mMouseDownButton = 0;
	theApp->mLastUserInputTick = theApp->mLastTimerTime;
	theApp->mWidgetManager->MouseUp(gInput.mLastSentX, gInput.mLastSentY, theButton);
}

// Run a controller action that the app resolves to a screen position: picking a
// seed packet, grabbing the shovel, hopping to the next lawn square.
void RunControllerAction(SexyAppBase* theApp, ControllerAction theAction)
{
	int aX = gInput.mLastSentX;
	int aY = gInput.mLastSentY;
	bool aClick = false;

	if (!theApp->ResolveControllerAction(theAction, aX, aY, aClick))
	{
		// Off the lawn the app has nothing to snap to, so let the D-pad nudge
		// the pointer instead of doing nothing at all.
		const int aNudge = theApp->mWidth / 24;
		switch (theAction)
		{
			case CONTROLLER_ACTION_CELL_LEFT:  aX -= aNudge; break;
			case CONTROLLER_ACTION_CELL_RIGHT: aX += aNudge; break;
			case CONTROLLER_ACTION_CELL_UP:    aY -= aNudge; break;
			case CONTROLLER_ACTION_CELL_DOWN:  aY += aNudge; break;
			default: return;
		}
	}

	MoveCursor(theApp, static_cast<float>(aX), static_cast<float>(aY), true);

	if (aClick && gInput.mMouseDownButton == 0)
	{
		theApp->mWidgetManager->MouseDown(gInput.mLastSentX, gInput.mLastSentY, 1);
		theApp->mWidgetManager->MouseUp(gInput.mLastSentX, gInput.mLastSentY, 1);
	}
}

struct ButtonKeyBinding
{
	unsigned int mButton;
	KeyCode mKeyCode;
};

// Buttons that stand in for a keyboard key. The D-pad doubles as a list
// navigator -- the almanac and the achievements screen scroll on the arrow
// keys -- while also hopping the pointer between lawn squares, see
// kButtonActions below.
const ButtonKeyBinding kButtonKeys[] = {
	{ SCE_CTRL_START,    KEYCODE_SPACE  },  // pause
	{ SCE_CTRL_UP,       KEYCODE_UP     },
	{ SCE_CTRL_DOWN,     KEYCODE_DOWN   },
	{ SCE_CTRL_LEFT,     KEYCODE_LEFT   },
	{ SCE_CTRL_RIGHT,    KEYCODE_RIGHT  },
};

struct ButtonActionBinding
{
	unsigned int mButton;
	ControllerAction mAction;
};

const ButtonActionBinding kButtonActions[] = {
	{ SCE_CTRL_LTRIGGER, CONTROLLER_ACTION_PREV_SLOT  },
	{ SCE_CTRL_RTRIGGER, CONTROLLER_ACTION_NEXT_SLOT  },
	{ SCE_CTRL_SQUARE,   CONTROLLER_ACTION_SHOVEL     },
	{ SCE_CTRL_UP,       CONTROLLER_ACTION_CELL_UP    },
	{ SCE_CTRL_DOWN,     CONTROLLER_ACTION_CELL_DOWN  },
	{ SCE_CTRL_LEFT,     CONTROLLER_ACTION_CELL_LEFT  },
	{ SCE_CTRL_RIGHT,    CONTROLLER_ACTION_CELL_RIGHT },
};

int AxisOffset(unsigned char theRaw)
{
	return std::abs(static_cast<int>(theRaw) - 128);
}

void PollController(SexyAppBase* theApp)
{
	SceCtrlData aPad;
	std::memset(&aPad, 0, sizeof(aPad));
	aPad.lx = aPad.ly = aPad.rx = aPad.ry = 128;
	if (sceCtrlPeekBufferPositive(0, &aPad, 1) < 0)
		return;

	// On PlayStation TV the handheld port stays silent and the pad shows up on
	// port 1 instead, so fold that in and let one build cover both.
	SceCtrlData aExtPad;
	std::memset(&aExtPad, 0, sizeof(aExtPad));
	aExtPad.lx = aExtPad.ly = aExtPad.rx = aExtPad.ry = 128;
	if (sceCtrlPeekBufferPositive(1, &aExtPad, 1) >= 0)
	{
		aPad.buttons |= aExtPad.buttons;
		if (AxisOffset(aExtPad.lx) > AxisOffset(aPad.lx)) aPad.lx = aExtPad.lx;
		if (AxisOffset(aExtPad.ly) > AxisOffset(aPad.ly)) aPad.ly = aExtPad.ly;
		if (AxisOffset(aExtPad.rx) > AxisOffset(aPad.rx)) aPad.rx = aExtPad.rx;
		if (AxisOffset(aExtPad.ry) > AxisOffset(aPad.ry)) aPad.ry = aExtPad.ry;
	}

	const unsigned int aButtons = aPad.buttons;
	const unsigned int aPressed = aButtons & ~gInput.mPrevButtons;
	const unsigned int aReleased = ~aButtons & gInput.mPrevButtons;
	gInput.mPrevButtons = aButtons;

	// ProcessDeferredMessages is called repeatedly within a frame, so integrate
	// stick motion against wall-clock time rather than once per call.
	const uint32_t aNow = SDL_GetTicks();
	uint32_t aElapsed = aNow - gInput.mLastPollTick;
	if (aElapsed > 100)  // a long stall should not teleport the pointer
		aElapsed = 100;
	gInput.mLastPollTick = aNow;

	const float aDeltaTime = aElapsed / 1000.0f;
	const float aWidth = static_cast<float>(theApp->mWidth);

	const float aMoveX = (StickAxis(aPad.lx) * STICK_SPEED
		+ StickAxis(aPad.rx) * STICK_SPEED_PRECISE) * aWidth * aDeltaTime;
	const float aMoveY = (StickAxis(aPad.ly) * STICK_SPEED
		+ StickAxis(aPad.ry) * STICK_SPEED_PRECISE) * aWidth * aDeltaTime;

	if (aMoveX != 0.0f || aMoveY != 0.0f)
		MoveCursor(theApp, gInput.mCursorX + aMoveX, gInput.mCursorY + aMoveY, true);

	if (aPressed == 0 && aReleased == 0)
		return;

	theApp->mLastUserInputTick = theApp->mLastTimerTime;

	const unsigned int aClickButton = gInput.mCircleIsEnter ? SCE_CTRL_CIRCLE : SCE_CTRL_CROSS;
	const unsigned int aCancelButton = gInput.mCircleIsEnter ? SCE_CTRL_CROSS : SCE_CTRL_CIRCLE;

	if (aPressed & aClickButton)
	{
		// A press has to land where the pointer is even if the pointer has not
		// moved yet, so make sure the app has seen a position first.
		MoveCursor(theApp, gInput.mCursorX, gInput.mCursorY, true);
		PressMouse(theApp, 1);
	}
	if (aReleased & aClickButton)
		ReleaseMouse(theApp, 1);

	// Put back whatever is on the cursor, the same as a right click on desktop.
	if (aPressed & SCE_CTRL_SELECT)
	{
		MoveCursor(theApp, gInput.mCursorX, gInput.mCursorY, true);
		PressMouse(theApp, -1);
	}
	if (aReleased & SCE_CTRL_SELECT)
		ReleaseMouse(theApp, -1);

	if (aPressed & aCancelButton)
		theApp->mWidgetManager->KeyDown(KEYCODE_ESCAPE);
	if (aReleased & aCancelButton)
		theApp->mWidgetManager->KeyUp(KEYCODE_ESCAPE);

	if (aPressed & SCE_CTRL_TRIANGLE)
		theApp->mWidgetManager->KeyDown(KEYCODE_RETURN);
	if (aReleased & SCE_CTRL_TRIANGLE)
		theApp->mWidgetManager->KeyUp(KEYCODE_RETURN);

	for (const ButtonKeyBinding& aBinding : kButtonKeys)
	{
		if (aPressed & aBinding.mButton)
			theApp->mWidgetManager->KeyDown(aBinding.mKeyCode);
		if (aReleased & aBinding.mButton)
			theApp->mWidgetManager->KeyUp(aBinding.mKeyCode);
	}

	for (const ButtonActionBinding& aBinding : kButtonActions)
	{
		if (aPressed & aBinding.mButton)
			RunControllerAction(theApp, aBinding.mAction);
	}
}

} // namespace

void SexyAppBase::InitInput()
{
	SDL_Init(SDL_INIT_EVENTS);

	sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG_WIDE);
	sceCtrlSetSamplingModeExt(SCE_CTRL_MODE_ANALOG_WIDE);

	// Japanese systems confirm with Circle and cancel with Cross. Honour the
	// system setting so the buttons read the way the owner expects.
	SceAppUtilInitParam anInitParam;
	SceAppUtilBootParam aBootParam;
	std::memset(&anInitParam, 0, sizeof(anInitParam));
	std::memset(&aBootParam, 0, sizeof(aBootParam));
	sceAppUtilInit(&anInitParam, &aBootParam);

	int anEnterButton = 0;
	if (sceAppUtilSystemParamGetInt(SCE_SYSTEM_PARAM_ID_ENTER_BUTTON, &anEnterButton) >= 0)
		gInput.mCircleIsEnter = (anEnterButton == SCE_SYSTEM_PARAM_ENTER_BUTTON_CIRCLE);

	gInput.mCursorX = mWidth * 0.5f;
	gInput.mCursorY = mHeight * 0.5f;
	gInput.mLastPollTick = SDL_GetTicks();

	mSoftwareCursorEnabled = true;
	mMouseIn = true;
}

bool SexyAppBase::StartTextInput(std::string& theInput)
{
	(void)theInput;
	// SDL drives the system IME here and reports whatever it collects as
	// ordinary SDL_TEXTINPUT events, so there is nothing to hand back.
	SDL_StartTextInput();
	return false;
}

void SexyAppBase::StopTextInput()
{
	SDL_StopTextInput();
}

bool SexyAppBase::ProcessDeferredMessages(bool singleMessage)
{
	(void)singleMessage;

	PollController(this);

	SDL_Event event;
	if (SDL_PollEvent(&event))
	{
		switch (event.type)
		{
			case SDL_QUIT:
				CloseRequestAsync();
				break;

			case SDL_APP_WILLENTERBACKGROUND:
				mMinimized = true;
				RehupFocus();
				break;

			case SDL_APP_DIDENTERFOREGROUND:
				mMinimized = false;
				RehupFocus();
				mWidgetManager->MarkAllDirty();
				break;

			case SDL_FINGERDOWN:
			case SDL_FINGERMOTION:
			case SDL_FINGERUP:
			{
				// Touch device 1 is the front panel. The rear panel is switched
				// off in MakeWindow, but filter on it anyway.
				if (event.tfinger.touchId != (SDL_TouchID)1)
					break;

				int aX = static_cast<int>(event.tfinger.x * VITA_SCREEN_WIDTH);
				int aY = static_cast<int>(event.tfinger.y * VITA_SCREEN_HEIGHT);
				mWidgetManager->RemapMouse(aX, aY);

				MoveCursor(this, static_cast<float>(aX), static_cast<float>(aY), false);

				if (event.type == SDL_FINGERUP)
				{
					ReleaseMouse(this, 1);
					gInput.mTouchDown = false;
				}
				else if (event.type == SDL_FINGERDOWN && !gInput.mTouchDown)
				{
					gInput.mTouchDown = true;
					PressMouse(this, 1);
				}
				break;
			}

			case SDL_KEYDOWN:
				mLastUserInputTick = mLastTimerTime;
				switch (event.key.keysym.sym)
				{
					case SDLK_RETURN:
					case SDLK_KP_ENTER:  mWidgetManager->KeyDown(KEYCODE_RETURN); break;
					case SDLK_BACKSPACE: mWidgetManager->KeyDown(KEYCODE_BACK); break;
					case SDLK_ESCAPE:    mWidgetManager->KeyDown(KEYCODE_ESCAPE); break;
					case SDLK_DELETE:    mWidgetManager->KeyDown(KEYCODE_DELETE); break;
					case SDLK_LEFT:      mWidgetManager->KeyDown(KEYCODE_LEFT); break;
					case SDLK_RIGHT:     mWidgetManager->KeyDown(KEYCODE_RIGHT); break;
					case SDLK_UP:        mWidgetManager->KeyDown(KEYCODE_UP); break;
					case SDLK_DOWN:      mWidgetManager->KeyDown(KEYCODE_DOWN); break;
					case SDLK_HOME:      mWidgetManager->KeyDown(KEYCODE_HOME); break;
					case SDLK_END:       mWidgetManager->KeyDown(KEYCODE_END); break;
					default: break;
				}
				break;

			case SDL_TEXTINPUT:
				mLastUserInputTick = mLastTimerTime;
				// The IME hands back UTF-8; the widget layer takes one byte at a
				// time, which covers the ASCII a profile name is made of.
				for (int i = 0; event.text.text[i] != 0; i++)
					mWidgetManager->KeyChar(event.text.text[i]);
				break;
		}
	}

	return SDL_HasEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
}
