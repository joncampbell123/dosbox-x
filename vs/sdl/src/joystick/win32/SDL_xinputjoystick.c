/*
	SDL - Simple DirectMedia Layer
	Copyright (C) 1997-2012 Sam Lantinga

	This library is free software; you can redistribute it and/or
	modify it under the terms of the GNU Lesser General Public
	License as published by the Free Software Foundation; either
	version 2.1 of the License, or (at your option) any later version.

	This library is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	Lesser General Public License for more details.

	You should have received a copy of the GNU Lesser General Public
	License along with this library; if not, write to the Free Software
	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

	Sam Lantinga
	slouken@libsdl.org
*/
#include "SDL_config.h"

#ifdef SDL_JOYSTICK_XINPUT

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "Xinput.h"
#pragma comment(lib, "Xinput9_1_0.lib")

#include "SDL_events.h"
#include "SDL_joystick.h"
#include "../SDL_sysjoystick.h"
#include "../SDL_joystick_c.h"

#define MAX_JOYSTICKS	16
#define MAX_AXES	6	/* each joystick can have up to 6 axes */
#define MAX_BUTTONS	32	/* and 32 buttons                      */
#define AXIS_MIN	-32768  /* minimum value for axis coordinate */
#define AXIS_MAX	32767   /* maximum value for axis coordinate */
/* limit axis to 256 possible positions to filter out noise */
#define JOY_AXIS_THRESHOLD      (((AXIS_MAX)-(AXIS_MIN))/256)
#define JOY_BUTTON_FLAG(n)	(1<<n)


/* array to hold joystick ID values */
static UINT SYS_JoystickID[MAX_JOYSTICKS];
static XINPUT_CAPABILITIES SYS_Joystick[MAX_JOYSTICKS];
static char* SYS_JoystickName[MAX_JOYSTICKS];

/* The private structure used to keep track of a joystick */
struct joystick_hwdata
{
	/* joystick ID */
	UINT id;

	/* values used to translate device-specific coordinates into
	   SDL-standard ranges */
	struct _transaxis
	{
		int offset;
		float scale;
	} transaxis[6];
};

/* Convert a win32 Multimedia API return code to a text message */
static void SetMMerror(char* function, int code);

static char* GetJoystickName(int index)
{
	char* name = (char*)SDL_malloc(256);
	SDL_snprintf(name, 256, "XInput Controller %d", index);
	return name;
}

/* Function to scan the system for joysticks.
 * This function should set SDL_numjoysticks to the number of available
 * joysticks.  Joystick 0 should be the system default joystick.
 * It should return 0, or -1 on an unrecoverable fatal error.
 */
int SDL_SYS_JoystickInit(void)
{
	int i;
	int maxdevs;
	int numdevs;
	XINPUT_CAPABILITIES caps;
	DWORD result;

	/* Reset the joystick ID & name mapping tables */
	for (i = 0; i < MAX_JOYSTICKS; ++i)
	{
		SYS_JoystickID[i] = 0;
		SYS_JoystickName[i] = NULL;
	}

	/* Loop over all potential joystick devices */
	numdevs = 0;
	maxdevs = XUSER_MAX_COUNT;
	for (i = 0; i < maxdevs && numdevs < MAX_JOYSTICKS; ++i)
	{
		ZeroMemory(&caps, sizeof(XINPUT_CAPABILITIES));
		result = XInputGetCapabilities(i, XINPUT_FLAG_GAMEPAD, &caps);
		if (result == ERROR_SUCCESS)
		{
			SYS_JoystickID[numdevs] = i;
			SYS_Joystick[numdevs] = caps;
			SYS_JoystickName[numdevs] = GetJoystickName(i);
			numdevs++;
		}
	}

	return numdevs;
}

/* Function to get the device-dependent name of a joystick */
const char* SDL_SYS_JoystickName(int index)
{
	if (SYS_JoystickName[index] != NULL)
		return (SYS_JoystickName[index]);

	return "XInput Controller (default)";
}

/* Function to open a joystick for use.
   The joystick to open is specified by the index field of the joystick.
   This should fill the nbuttons and naxes fields of the joystick structure.
   It returns 0, or -1 if there is an error.
 */
int SDL_SYS_JoystickOpen(SDL_Joystick* joystick)
{
	int index, i;
	int axis_min[MAX_AXES], axis_max[MAX_AXES];

	index = joystick->index;
	axis_min[0] = -32768;
	axis_max[0] = 32767;
	axis_min[1] = -32768;
	axis_max[1] = 32767;
	axis_min[2] = -32768;
	axis_max[2] = 32767;
	axis_min[3] = -32768;
	axis_max[3] = 32767;
	axis_min[4] = -32768; // see SDL_SYS_JoystickUpdate on why this
	axis_max[4] = 32767;
	axis_min[5] = -32768;
	axis_max[5] = 32767;

	/* allocate memory for system specific hardware data */
	joystick->hwdata = (struct joystick_hwdata *)SDL_malloc(sizeof(*joystick->hwdata));
	if (joystick->hwdata == NULL)
	{
		SDL_OutOfMemory();
		return (-1);
	}
	SDL_memset(joystick->hwdata, 0, sizeof(*joystick->hwdata));

	/* set hardware data */
	joystick->hwdata->id = SYS_JoystickID[index];
	for (i = 0; i < MAX_AXES; ++i)
	{
		joystick->hwdata->transaxis[i].offset = AXIS_MIN - axis_min[i];
		joystick->hwdata->transaxis[i].scale = (float)(AXIS_MAX - AXIS_MIN) / (axis_max[i] - axis_min[i]);
	}

	/* fill nbuttons, naxes, and nhats fields */
	int buttons, axes, hats;

	switch (SYS_Joystick[index].SubType)
	{
	case XINPUT_DEVSUBTYPE_GAMEPAD:
		buttons = 10;
		axes = 6;
		hats = 1;
		break;
	default:
		buttons = 0;
		axes = 0;
		hats = 0;
		break;
	}

	joystick->nbuttons = buttons;
	joystick->naxes = axes;
	joystick->nhats = hats;

	return 0;
}

static Uint8 TranslatePOV(DWORD value)
{
	Uint8 pov = SDL_HAT_CENTERED;

	if (value & XINPUT_GAMEPAD_DPAD_UP)
		pov |= SDL_HAT_UP;

	if (value & XINPUT_GAMEPAD_DPAD_RIGHT)
		pov |= SDL_HAT_RIGHT;

	if (value & XINPUT_GAMEPAD_DPAD_DOWN)
		pov |= SDL_HAT_DOWN;

	if (value & XINPUT_GAMEPAD_DPAD_LEFT)
		pov |= SDL_HAT_LEFT;

	return (pov);
}

/* Function to update the state of a joystick - called as a device poll.
 * This function shouldn't update the joystick structure directly,
 * but instead should call SDL_PrivateJoystick*() to deliver events
 * and update joystick device state.
 */
void SDL_SYS_JoystickUpdate(SDL_Joystick* joystick)
{
	DWORD result;
	int i;
	SHORT pos[MAX_AXES];
	struct _transaxis* transaxis;
	int value, change;

	XINPUT_STATE state;
	ZeroMemory(&state, sizeof(XINPUT_STATE));
	result = XInputGetState(joystick->hwdata->id, &state);
	if (result != ERROR_SUCCESS)
	{
		return;
	}

	pos[0] = state.Gamepad.sThumbLX;
	pos[1] = state.Gamepad.sThumbLY;
	pos[2] = state.Gamepad.sThumbRX;
	pos[3] = state.Gamepad.sThumbRY;
	// rescale these twos as formula below fails otherwise
	pos[4] = state.Gamepad.bLeftTrigger * 65535 / 255 - 32768;
	pos[5] = state.Gamepad.bRightTrigger * 65535 / 255 - 32768;

	transaxis = joystick->hwdata->transaxis;
	for (i = 0; i < joystick->naxes; i++)
	{
		value = (int)(((float)pos[i] + transaxis[i].offset) * transaxis[i].scale);
		change = (value - joystick->axes[i]);
		if ((change < -JOY_AXIS_THRESHOLD) || (change > JOY_AXIS_THRESHOLD))
		{
			SDL_PrivateJoystickAxis(joystick, (Uint8)i, (Sint16)value);
		}
	}

	/* joystick button events */
	for (i = 0; i < joystick->nbuttons; ++i)
	{
		int button;
		switch (i)
		{
		case 0:
			button = XINPUT_GAMEPAD_A;
			break;
		case 1:
			button = XINPUT_GAMEPAD_B;
			break;
		case 2:
			button = XINPUT_GAMEPAD_X;
			break;
		case 3:
			button = XINPUT_GAMEPAD_Y;
			break;
		case 4:
			button = XINPUT_GAMEPAD_LEFT_SHOULDER;
			break;
		case 5:
			button = XINPUT_GAMEPAD_RIGHT_SHOULDER;
			break;
		case 6:
			button = XINPUT_GAMEPAD_BACK;
			break;
		case 7:
			button = XINPUT_GAMEPAD_START;
			break;
		case 8:
			button = XINPUT_GAMEPAD_LEFT_THUMB;
			break;
		case 9:
			button = XINPUT_GAMEPAD_RIGHT_THUMB;
			break;
		default:
			button = 0;
		}

		if (state.Gamepad.wButtons & button)
		{
			if (!joystick->buttons[i])
			{
				SDL_PrivateJoystickButton(joystick, (Uint8)i, SDL_PRESSED);
			}
		}
		else
		{
			if (joystick->buttons[i])
			{
				SDL_PrivateJoystickButton(joystick, (Uint8)i, SDL_RELEASED);
			}
		}
	}

	const Uint8 pov = TranslatePOV(state.Gamepad.wButtons);
	if (pov != joystick->hats[0])
	{
		SDL_PrivateJoystickHat(joystick, 0, pov);
	}
}

/* Function to close a joystick after use */
void SDL_SYS_JoystickClose(SDL_Joystick* joystick)
{
	if (joystick->hwdata != NULL)
	{
		/* free system specific hardware data */
		SDL_free(joystick->hwdata);
		joystick->hwdata = NULL;
	}
}

/* Function to perform any system-specific joystick related cleanup */
void SDL_SYS_JoystickQuit(void)
{
	int i;
	for (i = 0; i < MAX_JOYSTICKS; i++)
	{
		if (SYS_JoystickName[i] != NULL)
		{
			SDL_free(SYS_JoystickName[i]);
			SYS_JoystickName[i] = NULL;
		}
	}
}

#endif /* SDL_JOYSTICK_XINPUT */
