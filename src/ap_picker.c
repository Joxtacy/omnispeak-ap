/*
 * Tiny in-engine episode picker — a cross-platform replacement for the
 * old Win32-only AP Keen Launcher.exe.
 *
 * On Windows we drive comctl32's TaskDialogIndirect for a themed dialog
 * with command-link buttons (the plain SDL_ShowMessageBox path is the
 * legacy Win32 MessageBox, which sizes its width to the message text
 * and truncates long button labels).
 *
 * On macOS/Linux we use SDL_ShowMessageBox, which already renders a
 * native dialog. On builds without SDL (dos/null) this is a no-op and
 * the engine falls back to the existing auto-detection.
 */

#include "ap_picker.h"

#include <stddef.h>

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

#ifdef _WIN32

#include <windows.h>
#include <commctrl.h>
#include <stdlib.h>
#include <string.h>

typedef HRESULT (WINAPI *AP_TaskDialogIndirectFn)(
	const TASKDIALOGCONFIG *, int *, int *, BOOL *);

static wchar_t *ap_picker_widen(const char *s)
{
	int len = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
	if (len <= 0) return NULL;
	wchar_t *w = (wchar_t *)malloc((size_t)len * sizeof(wchar_t));
	if (!w) return NULL;
	MultiByteToWideChar(CP_UTF8, 0, s, -1, w, len);
	return w;
}

/*
 * Returns the picked episode, or NULL if the user cancelled. Returns
 * the sentinel (CK_EpisodeDef *)-1 if TaskDialogIndirect is unavailable
 * (no v6 comctl32) — the caller should then fall through to the SDL
 * path. Any other failure also yields the sentinel.
 */
#define AP_PICKER_WIN32_FALLBACK ((CK_EpisodeDef *)-1)

static CK_EpisodeDef *AP_Picker_PickEpisode_Win32(CK_EpisodeDef **episodes)
{
	HMODULE comctl32 = LoadLibraryW(L"comctl32.dll");
	if (!comctl32) return AP_PICKER_WIN32_FALLBACK;
	AP_TaskDialogIndirectFn tdi =
		(AP_TaskDialogIndirectFn)GetProcAddress(comctl32, "TaskDialogIndirect");
	if (!tdi)
	{
		FreeLibrary(comctl32);
		return AP_PICKER_WIN32_FALLBACK;
	}

	TASKDIALOG_BUTTON buttons[AP_PICKER_MAX];
	wchar_t *labels[AP_PICKER_MAX] = { 0 };
	CK_EpisodeDef *choices[AP_PICKER_MAX] = { 0 };
	int count = 0;

	for (int i = 0; episodes[i] && count < AP_PICKER_MAX; ++i)
	{
		if (!episodes[i]->isPresent()) continue;
		labels[count] = ap_picker_widen(ap_picker_label(episodes[i]));
		if (!labels[count]) continue;
		buttons[count].nButtonID = 100 + count;
		buttons[count].pszButtonText = labels[count];
		choices[count] = episodes[i];
		count++;
	}

	CK_EpisodeDef *picked_ep = NULL;
	if (count >= 2)
	{
		TASKDIALOGCONFIG cfg;
		memset(&cfg, 0, sizeof(cfg));
		cfg.cbSize = sizeof(cfg);
		cfg.hwndParent = NULL;
		cfg.dwFlags = TDF_USE_COMMAND_LINKS | TDF_ALLOW_DIALOG_CANCELLATION;
		cfg.dwCommonButtons = TDCBF_CANCEL_BUTTON;
		cfg.pszWindowTitle = L"Omnispeak AP";
		cfg.pszMainIcon = TD_INFORMATION_ICON;
		cfg.pszMainInstruction =
			L"Which Commander Keen episode would you like to play?";
		cfg.cButtons = (UINT)count;
		cfg.pButtons = buttons;
		cfg.nDefaultButton = 100;

		int picked_id = 0;
		HRESULT hr = tdi(&cfg, &picked_id, NULL, NULL);
		if (SUCCEEDED(hr))
		{
			int idx = picked_id - 100;
			if (idx >= 0 && idx < count) picked_ep = choices[idx];
		}
	}

	for (int i = 0; i < count; ++i) free(labels[i]);
	FreeLibrary(comctl32);
	return picked_ep;
}

#endif /* _WIN32 */

#ifdef WITH_SDL

#if WITH_SDL == 3
#include <SDL3/SDL.h>
#else
#include "SDL.h"
#endif

#ifdef __APPLE__
#include <stdio.h>
#include <string.h>

/* osascript "choose from list" gives us a native, fully keyboard-driven
 * list dialog (arrows, type-ahead, Return, Esc) without dragging in any
 * Cocoa / Objective-C plumbing. NSAlert via SDL_ShowMessageBox only
 * honours keyboard navigation when the user has Full Keyboard Access
 * turned on system-wide, so we prefer this. */
static CK_EpisodeDef *AP_Picker_PickEpisode_Mac(CK_EpisodeDef **episodes)
{
	CK_EpisodeDef *choices[AP_PICKER_MAX];
	const char *labels[AP_PICKER_MAX];
	int count = 0;
	for (int i = 0; episodes[i] && count < AP_PICKER_MAX; ++i)
	{
		if (!episodes[i]->isPresent()) continue;
		choices[count] = episodes[i];
		labels[count] = ap_picker_label(episodes[i]);
		count++;
	}
	if (count < 2) return NULL;

	char cmd[2048];
	int n = snprintf(cmd, sizeof(cmd),
		"osascript -e 'choose from list {");
	for (int i = 0; i < count; ++i)
	{
		n += snprintf(cmd + n, sizeof(cmd) - n,
			"%s\"%s\"", i ? "," : "", labels[i]);
	}
	snprintf(cmd + n, sizeof(cmd) - n,
		"} with title \"Omnispeak AP\""
		" with prompt \"Which Commander Keen episode would you like to play?\""
		" default items {\"%s\"}'",
		labels[0]);

	FILE *p = popen(cmd, "r");
	if (!p) return NULL;
	char out[256] = {0};
	if (!fgets(out, sizeof(out), p))
	{
		pclose(p);
		return NULL;
	}
	pclose(p);

	size_t L = strlen(out);
	while (L && (out[L - 1] == '\n' || out[L - 1] == '\r')) out[--L] = 0;
	if (!L || !strcmp(out, "false")) return NULL;

	for (int i = 0; i < count; ++i)
		if (!strcmp(out, labels[i])) return choices[i];
	return NULL;
}
#endif /* __APPLE__ */

CK_EpisodeDef *AP_Picker_PickEpisode(CK_EpisodeDef **episodes)
{
	if (!episodes) return NULL;

#ifdef _WIN32
	CK_EpisodeDef *win = AP_Picker_PickEpisode_Win32(episodes);
	if (win != AP_PICKER_WIN32_FALLBACK) return win;
	/* TaskDialogIndirect not available (no v6 comctl32) — fall through
	 * to the SDL message box. */
#endif

#ifdef __APPLE__
	return AP_Picker_PickEpisode_Mac(episodes);
#endif

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
#ifdef _WIN32
	if (!episodes) return NULL;
	CK_EpisodeDef *win = AP_Picker_PickEpisode_Win32(episodes);
	if (win != AP_PICKER_WIN32_FALLBACK) return win;
	return NULL;
#else
	(void)episodes;
	return NULL;
#endif
}

#endif
