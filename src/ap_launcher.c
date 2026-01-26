#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define ID_BTN_EP4 1001
#define ID_BTN_EP5 1002

void launch_episode(int episode)
{
    STARTUPINFOA si = {0};
    PROCESS_INFORMATION pi = {0};
    char cmd[64];

    si.cb = sizeof(si);
    wsprintfA(cmd, "omnispeak.exe /Episode %d", episode);

    CreateProcessA(
        NULL, cmd,
        NULL, NULL,
        FALSE, 0,
        NULL, NULL,
        &si, &pi
    );

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case ID_BTN_EP4:
            launch_episode(4);
            break;

        case ID_BTN_EP5:
            launch_episode(5);
            break;
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(
    HINSTANCE hInst,
    HINSTANCE hPrev,
    LPSTR lpCmdLine,
    int nCmdShow)
{
    WNDCLASSEXA wc = {0};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.lpszClassName = "AP Keen Launcher";
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);

    RegisterClassExA(&wc);

    HWND hwnd = CreateWindowA(
        "AP Keen Launcher",
        "AP Keen Launcher",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT,
        350, 180,
        NULL, NULL, hInst, NULL
    );

    CreateWindowA(
        "BUTTON", "Episode 4",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        20, 20, 300, 40,
        hwnd, (HMENU)ID_BTN_EP4, hInst, NULL
    );

    CreateWindowA(
        "BUTTON", "Episode 5",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        20, 80, 300, 40,
        hwnd, (HMENU)ID_BTN_EP5, hInst, NULL
    );

    ShowWindow(hwnd, nCmdShow);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}