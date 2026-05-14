/*
 * Tiny in-engine episode picker — a cross-platform replacement for the
 * old Win32-only AP Keen Launcher.exe. Uses SDL_ShowMessageBox, which
 * renders a native modal dialog on Windows/macOS/Linux. On builds
 * without SDL (dos/null) this is a no-op and the engine falls back to
 * the existing auto-detection.
 */

#include "ap_picker.h"

#include <stddef.h>

#ifdef WITH_SDL

#if WITH_SDL == 3
#include <SDL3/SDL.h>
#else
#include "SDL.h"
#endif

#define AP_PICKER_MAX 8

static const char *ap_picker_label(const CK_EpisodeDef *ep)
{
	switch (ep->ep)
	{
	case EP_CK4: return "Keen 4: Secret of the Oracle";
	case EP_CK5: return "Keen 5: The Armageddon Machine";
	case EP_CK6: return "Keen 6: Aliens Ate My Babysitter";
	default:     return "Unknown episode";
	}
}

CK_EpisodeDef *AP_Picker_PickEpisode(CK_EpisodeDef **episodes)
{
	if (!episodes) return NULL;

	SDL_MessageBoxButtonData buttons[AP_PICKER_MAX];
	CK_EpisodeDef *choices[AP_PICKER_MAX];
	int count = 0;

	for (int i = 0; episodes[i] && count < AP_PICKER_MAX; ++i)
	{
		if (!episodes[i]->isPresent()) continue;
		choices[count] = episodes[i];
		buttons[count].flags = 0;
		buttons[count].buttonid = count;
		buttons[count].text = ap_picker_label(episodes[i]);
		count++;
	}
	if (count < 2) return NULL;

	buttons[0].flags = SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT;
	buttons[count - 1].flags |= SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;

	/* SDL_ShowMessageBox requires the video subsystem. The engine
	 * initialises it later in VL_Impl_GetBackend(); SDL refcounts
	 * subsystems, so initialising it here is safe either way. */
	bool ownInit = false;
	if (!SDL_WasInit(SDL_INIT_VIDEO))
	{
#if WITH_SDL == 3
		if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) return NULL;
#else
		if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) return NULL;
#endif
		ownInit = true;
	}

	SDL_MessageBoxData data;
	data.flags = SDL_MESSAGEBOX_INFORMATION
#ifdef SDL_MESSAGEBOX_BUTTONS_LEFT_TO_RIGHT
		| SDL_MESSAGEBOX_BUTTONS_LEFT_TO_RIGHT
#endif
		;
	data.window = NULL;
	data.title = "Omnispeak AP";
	data.message = "Which Commander Keen episode would you like to play?";
	data.numbuttons = count;
	data.buttons = buttons;
	data.colorScheme = NULL;

	int picked = -1;
#if WITH_SDL == 3
	bool shown = SDL_ShowMessageBox(&data, &picked);
#else
	bool shown = (SDL_ShowMessageBox(&data, &picked) == 0);
#endif

	/* Leave the video subsystem initialised even if we set it up — the
	 * engine is about to need it. SDL_Quit() at shutdown cleans up. */
	(void)ownInit;

	if (!shown || picked < 0 || picked >= count) return NULL;
	return choices[picked];
}

#else /* !WITH_SDL */

CK_EpisodeDef *AP_Picker_PickEpisode(CK_EpisodeDef **episodes)
{
	(void)episodes;
	return NULL;
}

#endif
