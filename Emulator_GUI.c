#include "Bus.h"
#include "Emulator_GUI.h"

#define PLAY_BUTTON_ID 0x0002
#define STEP_BUTTON_ID 0x0003
#define PAUSE_BUTTON_ID 0x0004
#define STOP_BUTTON_ID 0x0005
#define STEP_PPU_BUTTON_ID 0x000b
#define STEP_FRAME_BUTTON_ID 0x000d
#define BREAKPOINT_EDIT_ID 0x0006
#define BREAKPOINT_SET_ID 0x0007
#define BREAKPOINT_CLEAR_ID 0x0008
#define RAMVIEW_EDIT_ID 0x0009
#define RAMVIEW_SET_ID 0x000a
#define RAMVIEW_BOX_ID 0x000c
#define INSERT_CART_ID 2
#define SAVE_CART_ID 3
#ifdef RGB
#undef RGB
#endif
#define RGB(r,g,b)          ((COLORREF)(((BYTE)(b)|((WORD)((BYTE)(g))<<8))|(((DWORD)(BYTE)(r))<<16)))

static FLOAT dpiX, dpiY;
static RECT frame = {0};
static UINT insert_cart_id;
HINSTANCE main_instance = NULL;
BOOL bPause = FALSE;
BOOL bStop = FALSE;
uint16_t breakpoint_addr = 0x0000;
uint16_t ram_view_addr = 0x0000;
uint32_t* screen_buffer = NULL;
struct cart_stats curr_cart_stats = { .nTitle = 17, .title = {0} };

// Play thread ID
DWORD dwPlayThread = 0x0000;

int WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow)
{
    main_instance = hInstance;

    // Main window class
	const wchar_t MAIN_CLASS_NAME[] = L"Gameboy Emulator";

    WNDCLASS mwc = {
        mwc.style = 0,
        mwc.lpfnWndProc = MainWindowProc,
        mwc.cbClsExtra = 0,
        mwc.cbWndExtra = 0,
	    mwc.hInstance = hInstance,
        mwc.hIcon = 0,
        mwc.hCursor = LoadCursor(NULL, IDC_ARROW),
        mwc.hbrBackground = 0,
        mwc.lpszMenuName = NULL,
	    mwc.lpszClassName = MAIN_CLASS_NAME };

	RegisterClass(&mwc);

    // DPI awareness
    SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);

    HDC screen = GetDC(0);
    dpiX = (FLOAT)(GetDeviceCaps(screen, LOGPIXELSX));
    dpiY = (FLOAT)(GetDeviceCaps(screen, LOGPIXELSY));
    ReleaseDC(0, screen);

    // Create menu
    HMENU main_menu = CreateMenu();
    HMENU file_menu = CreatePopupMenu();

    AppendMenu(
        file_menu,
        MF_STRING|MF_ENABLED,
        INSERT_CART_ID,
        L"Insert Cartridge");
    AppendMenu(
        file_menu,
        MF_DISABLED|MF_STRING,
        SAVE_CART_ID,
        L"Save Cartridge"
    );
    AppendMenu(
        main_menu,
        MF_ENABLED|MF_STRING|MF_POPUP,
        file_menu,
        L"File"
    );
    


    insert_cart_id = GetMenuItemID(main_menu, 0);
    HWND hwnd = CreateWindowEx(
        0,                              // Optional window styles.
        MAIN_CLASS_NAME,                     // Window class
        L"Ben's GameBoy Emulator",    // Window text
        WS_CLIPCHILDREN|WS_OVERLAPPEDWINDOW|WS_MAXIMIZE,            // Window style

        // Size and position
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,

        NULL,       // Parent window    
        main_menu,       // Menu
        hInstance,  // Instance handle
        NULL        // Additional application data
    );

    if (hwnd == NULL)
    {
        wchar_t errormsg[20];
        StringCchPrintf(errormsg, 20, L"%d", GetLastError());
        MessageBox(NULL, errormsg, NULL, MB_OK);
        return 0;
    }

    // Create buttons
    INITCOMMONCONTROLSEX icc = {sizeof (INITCOMMONCONTROLSEX), ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);

    HWND hwndButtonStep = CreateWindow(
        L"BUTTON",  // Predefined class; Unicode assumed 
        L"Step",      // Button text 
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | BS_FLAT | WS_DISABLED,  // Styles 
        800,         // x position 
        30,         // y position 
        60,        // Button width
        60,        // Button height
        hwnd,     // Parent window
        STEP_BUTTON_ID,       // Button ID
        (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
        NULL);      // Pointer not needed.

    HWND hwndButtonPlay = CreateWindow(
        L"BUTTON",  // Predefined class; Unicode assumed 
        L"Play",      // Button text 
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | BS_FLAT | WS_DISABLED,  // Styles 
        900,         // x position 
        30,         // y position 
        60,        // Button width
        60,        // Button height
        hwnd,     // Parent window
        PLAY_BUTTON_ID,       // Button ID
        (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
        NULL);      // Pointer not needed.

    HWND hwndButtonStop = CreateWindow(
        L"BUTTON",  // Predefined class; Unicode assumed 
        L"Stop",      // Button text 
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | BS_FLAT | WS_DISABLED,  // Styles 
        1370,         // x position 
        30,         // y position 
        60,        // Button width
        60,        // Button height
        hwnd,     // Parent window
        STOP_BUTTON_ID,       // Button ID
        (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
        NULL);      // Pointer not needed.

    HWND hwndButtonPause = CreateWindow(
        L"BUTTON",  // Predefined class; Unicode assumed 
        L"Pause",      // Button text 
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | BS_FLAT | WS_DISABLED,  // Styles 
        1000,         // x position 
        30,         // y position 
        60,        // Button width
        60,        // Button height
        hwnd,     // Parent window
        PAUSE_BUTTON_ID,       // Button ID
        (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
        NULL);      // Pointer not needed.
    HWND hwndButtonStepPPU = CreateWindow(
        L"BUTTON",  // Predefined class; Unicode assumed 
        L"Step PPU",      // Button text 
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | BS_FLAT | WS_DISABLED,  // Styles 
        1240,         // x position 
        30,         // y position 
        100,        // Button width
        60,        // Button height
        hwnd,     // Parent window
        STEP_PPU_BUTTON_ID,       // Button ID
        (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
        NULL);      // Pointer not needed.
    HWND hwndButtonStepFrame = CreateWindow(
        L"BUTTON",  // Predefined class; Unicode assumed 
        L"Step Frame",      // Button text 
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | BS_FLAT | WS_DISABLED,  // Styles 
        1100,         // x position 
        30,         // y position 
        100,        // Button width
        60,        // Button height
        hwnd,     // Parent window
        STEP_FRAME_BUTTON_ID,       // Button ID
        (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
        NULL);      // Pointer not needed.


    // Create breakpoint input
    HWND hwndEditBreakpoint = CreateWindow(
        L"EDIT",  // Predefined class; Unicode assumed 
        NULL,      // Button text 
        ES_LEFT | WS_VISIBLE | WS_CHILD | WS_BORDER,  // Styles 
        800,         // x position 
        305,         // y position 
        60,        // Edit width
        26,        // Edit height
        hwnd,     // Parent window
        BREAKPOINT_EDIT_ID,       // Edit ID
        (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
        NULL);      // Pointer not needed.
    HWND hwndButtonBreakSet = CreateWindow(
        L"BUTTON",  // Predefined class; Unicode assumed 
        L"Set",      // Button text 
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | BS_FLAT,  // Styles 
        870,         // x position 
        305,         // y position 
        60,        // Button width
        26,        // Button height
        hwnd,     // Parent window
        BREAKPOINT_SET_ID,       // Button ID
        (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
        NULL);      // Pointer not needed.
    HWND hwndButtonBreakClear = CreateWindow(
        L"BUTTON",  // Predefined class; Unicode assumed 
        L"Clear",      // Button text 
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | BS_FLAT,  // Styles 
        940,         // x position 
        305,         // y position 
        60,        // Button width
        26,        // Button height
        hwnd,     // Parent window
        BREAKPOINT_CLEAR_ID,       // Button ID
        (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
        NULL);      // Pointer not needed.

    // Create RAM view input and button
    HWND hwndEditRamview = CreateWindow(
        L"EDIT",  // Predefined class; Unicode assumed 
        NULL,      // Button text 
        ES_LEFT | WS_VISIBLE | WS_CHILD | WS_BORDER,  // Styles 
        950,         // x position 
        395,         // y position 
        60,        // Edit width
        26,        // Edit height
        hwnd,     // Parent window
        RAMVIEW_EDIT_ID,       // Edit ID
        (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
        NULL);      // Pointer not needed.
    HWND hwndButtonRamviewSet = CreateWindow(
        L"BUTTON",  // Predefined class; Unicode assumed 
        L"Set",      // Button text 
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | BS_FLAT,  // Styles 
        1020,         // x position 
        395,         // y position 
        60,        // Button width
        26,        // Button height
        hwnd,     // Parent window
        RAMVIEW_SET_ID,       // Button ID
        (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
        NULL);      // Pointer not needed.
    HWND hwndRamview = CreateWindow(
        L"EDIT",  // Predefined class; Unicode assumed 
        NULL,      // Button text 
        ES_LEFT | WS_VISIBLE | WS_CHILD | ES_READONLY | ES_MULTILINE,  // Styles 
        800,         // x position 
        425,         // y position 
        438,        // Edit width
        192,        // Edit height
        hwnd,     // Parent window
        RAMVIEW_BOX_ID,       // Edit ID
        (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
        NULL);      // Pointer not needed.

    // Setup Ramview font
    HFONT monofont = CreateFontW(
        16,  // Height
        0,  // Width
        0,  // Escapement
        0,  // Orientation
        FW_DONTCARE, // Font weight
        FALSE,  // Italic
        FALSE,  // Underline
        FALSE,  // Strike out
        DEFAULT_CHARSET,    // Character set
        OUT_DEFAULT_PRECIS, // Precision
        CLIP_DEFAULT_PRECIS,    // Clipping precision
        DEFAULT_QUALITY,    // Quality
        DEFAULT_PITCH | FF_MODERN,    // Pitch and font family
        L"Courier New"    // Typeface name
    );

    // Send text to Ramview
    SetWindowFont(hwndRamview, monofont, TRUE);

    

    ShowWindow(hwnd, SW_MAXIMIZE);
    UpdateWindow(hwnd);

    // Start the message loop. 
    BOOL bRet;
    MSG msg;

    while ((bRet = GetMessage(&msg, NULL, 0, 0)) != 0)
    {
        if (bRet == -1)
        {
            // handle the error and possibly exit
        }
        else
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    // Return the exit code to the system. 

    return msg.wParam;
}

LRESULT CALLBACK MainWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
        case WM_CREATE:
            game_reset();
            screen_buffer = gameboy_get_screen();
            return 0;
        case WM_PAINT:
        {
           
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            // All painting occurs here, between BeginPaint and EndPaint.

            
            
            FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + 1));
            RenderLCDScreen(hwnd, screen_buffer, 4, 100, 30);
            HBRUSH frame_color = CreateSolidBrush(RGB(40,40,40));
            FrameRect(hdc, &frame, frame_color);
            DeleteObject(frame_color);
            UpdateInfo(hwnd);

            // Cartridge stats
            displayCartStats(hwnd);
            
            // Draw Joypad buttons
#define SCL 40
#define X 200
#define Y 680
            POINT plus_points[12] = {
                {X + 0 * SCL, Y + 1 * SCL},
                {X + 1 * SCL, Y + 1 * SCL},
                {X + 1 * SCL, Y + 0 * SCL},
                {X + 2 * SCL, Y + 0 * SCL},
                {X + 2 * SCL, Y + 1 * SCL},
                {X + 3 * SCL, Y + 1 * SCL},
                {X + 3 * SCL, Y + 2 * SCL},
                {X + 2 * SCL, Y + 2 * SCL},
                {X + 2 * SCL, Y + 3 * SCL},
                {X + 1 * SCL, Y + 3 * SCL},
                {X + 1 * SCL, Y + 2 * SCL},
                {X + 0 * SCL, Y + 2 * SCL},
            };
            HRGN direction_buttons = CreatePolygonRgn(plus_points, 12, WINDING);
            HBRUSH frame_brush = GetStockObject(GRAY_BRUSH);
            FrameRgn(hdc, direction_buttons, frame_brush, 2,2);

            // Draw A and B buttons
            LOGBRUSH br = {BS_SOLID, (COLORREF)RGB(130,130,130), NULL};
            HPEN pen = ExtCreatePen(PS_GEOMETRIC|PS_SOLID, 2, &br, 0, NULL);
            SelectObject(hdc, (HGDIOBJ)pen);
#define SCL 60
#define X 490
#define Y 720
            Ellipse(hdc, X, Y, X + SCL, Y + SCL);

#define SCL 60
#define X 570
#define Y 680
            Ellipse(hdc, X, Y, X + SCL, Y + SCL);

            // Draw Select and Start buttons
            int prev_gm = GetGraphicsMode(hdc);
            XFORM prev_xfm;
            GetWorldTransform(hdc, &prev_xfm);
            double factor = (2.0f * 3.1416f) / 360.0f;
            double rot = -25.0f * factor;

            // Create a matrix for the transform we want (read the docs for details)
            XFORM xfm = { 0.0f };
            xfm.eM11 = (float)cos(rot);
            xfm.eM12 = (float)sin(rot);
            xfm.eM21 = (float)-sin(rot);
            xfm.eM22 = (float)cos(rot);

            SetGraphicsMode(hdc, GM_ADVANCED);
            SetWorldTransform(hdc, &xfm);
            
#define X 310
#define Y 860
#define SCLX 60
#define SCLY 15
            POINT prev_org;
            SetWindowOrgEx(hdc, -X, -Y, &prev_org);
            RoundRect(hdc, 0, 0, SCLX, SCLY, SCLY, SCLY);

#define X 315
#define Y 880
            SetWindowOrgEx(hdc, -X, -Y, &prev_org);
            WCHAR select_str[10] = { L'S', L'E', L'L', L'E', L'C', L'T', L'\0'};
            ExtTextOutW(
                hdc,
                0,    // X
                0,     // Y
                ETO_NUMERICSLOCAL,   // Flags
                NULL,   // Rect
                select_str,
                _countof(select_str),
                NULL);

#define X 400
#define Y 860
            SetWindowOrgEx(hdc, -X, -Y, &prev_org);
            RoundRect(hdc, 0, 0, SCLX, SCLY, SCLY, SCLY);

#define X 410
#define Y 880
            SetWindowOrgEx(hdc, -X, -Y, &prev_org);
            WCHAR start_str[10] = { L'S', L'T', L'A', L'R', L'T', L'\0' };
            ExtTextOutW(
                hdc,
                0,    // X
                0,     // Y
                ETO_NUMERICSLOCAL,   // Flags
                NULL,   // Rect
                start_str,
                _countof(start_str),
                NULL);

#define X 534
#define Y 790
            SetWindowOrgEx(hdc, -X, -Y, &prev_org);
            WCHAR b_str[10] = { L'B', L'\0' };
            ExtTextOutW(
                hdc,
                0,    // X
                0,     // Y
                ETO_NUMERICSLOCAL,   // Flags
                NULL,   // Rect
                b_str,
                _countof(b_str),
                NULL);

#define X 615
#define Y 750
            SetWindowOrgEx(hdc, -X, -Y, &prev_org);
            WCHAR a_str[10] = { L'A', L'\0' };
            ExtTextOutW(
                hdc,
                0,    // X
                0,     // Y
                ETO_NUMERICSLOCAL,   // Flags
                NULL,   // Rect
                a_str,
                _countof(a_str),
                NULL);

            SetWindowOrgEx(hdc, prev_org.x, prev_org.y, NULL);
            SetWorldTransform(hdc, &prev_xfm);
            SetGraphicsMode(hdc, prev_gm);

            DeleteObject(pen);
            
            EndPaint(hwnd, &ps);
            hdc = NULL;
            
            return 0;
        }
        case WM_COMMAND:
        {
            if (HIWORD(wParam) == 0 && lParam == 0)
            {
                switch (LOWORD(wParam)) {
                    case INSERT_CART_ID:
                    {
                        // Insert cart
                        WCHAR filename[MAX_PATH] = {0};
                        OPENFILENAME filenameinfo = {
                            sizeof(OPENFILENAME),
                            hwnd,
                            NULL,
                            NULL,
                            NULL,
                            NULL,
                            0,
                            filename,
                            MAX_PATH,
                            NULL,
                            0,
                            NULL,
                            NULL,
                            OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST,
                            0,
                            0,
                            NULL,
                            NULL,
                            NULL,
                            NULL,
                            NULL,
                            NULL,
                            0
                        };
                        if (!GetOpenFileName(&filenameinfo))
                            break;
                        char asciiname[MAX_PATH];
                        size_t result;
                        wcstombs_s(&result, asciiname, MAX_PATH, filename, _TRUNCATE);
                        if (!bPause)
                        {
                            bStop = true;
                            game_pause(hwnd);
                            Sleep(100);
                            game_reset();
                        }
                        gameboy_cart_unload();
                        uint8_t load_err = gameboy_cart_load(asciiname);
                        if (load_err == 0)
                        {
                            HWND hwPlayButton = GetDlgItem(hwnd, PLAY_BUTTON_ID);
                            HWND hwStepButton = GetDlgItem(hwnd, STEP_BUTTON_ID);
                            HWND hwStepPPUButton = GetDlgItem(hwnd, STEP_PPU_BUTTON_ID);
                            HWND hwStopButton = GetDlgItem(hwnd, STOP_BUTTON_ID);
                            HWND hwStepFrameButton = GetDlgItem(hwnd, STEP_FRAME_BUTTON_ID);
                            EnableWindow(hwPlayButton, TRUE);
                            EnableWindow(hwStepButton, TRUE);
                            EnableWindow(hwStepPPUButton, TRUE);
                            EnableWindow(hwStopButton, TRUE);
                            EnableWindow(hwStepFrameButton, TRUE);
                            gameboy_cartridge_stats(curr_cart_stats.nTitle, curr_cart_stats.title, &curr_cart_stats.type, 
                                &curr_cart_stats.rom_size, &curr_cart_stats.ram_size, &curr_cart_stats.japan, 
                                &curr_cart_stats.header_chck, &curr_cart_stats.global_chck);
                            displayCartStats(hwnd);

                            HMENU menu = GetMenu(hwnd);
                            HMENU file_menu = GetSubMenu(menu, 0);
                            switch (curr_cart_stats.type)
                            {
                            case CART_MBC1_RAM_BATTERY:
                            case CART_MBC2_BATTERY:
                            case CART_ROM_RAM_BATTERY:
                            case CART_MMM01_RAM_BATTERY:
                            case CART_MBC3_TIMER_RAM_BATTERY:
                            case CART_MBC3_RAM_BATTERY:
                            case CART_MBC5_RAM_BATTERY:
                            case CART_MBC5_RUMBLE_RAM_BATTERY:
                            case CART_MBC7_SENSOR_RUMBLE_RAM_BATTERY:
                            case CART_HUC1_RAM_BATTERY:
                                // You can save
                            {
                                EnableMenuItem(file_menu, SAVE_CART_ID, MF_ENABLED | MF_BYCOMMAND);
                                break;
                            }
                            default:
                            {
                                EnableMenuItem(file_menu, SAVE_CART_ID, MF_DISABLED | MF_BYCOMMAND);
                            }
                            }
                            
                        }
                        else
                        {
                            // Reset buttons
                            HWND hwPlayButton = GetDlgItem(hwnd, PLAY_BUTTON_ID);
                            HWND hwStepButton = GetDlgItem(hwnd, STEP_BUTTON_ID);
                            HWND hwPauseButton = GetDlgItem(hwnd, PAUSE_BUTTON_ID);
                            HWND hwStepPPUButton = GetDlgItem(hwnd, STEP_PPU_BUTTON_ID);
                            HWND hwStopButton = GetDlgItem(hwnd, STOP_BUTTON_ID);
                            HWND hwStepFrameButton = GetDlgItem(hwnd, STEP_FRAME_BUTTON_ID);
                            EnableWindow(hwPlayButton, FALSE);
                            EnableWindow(hwStepButton, FALSE);
                            EnableWindow(hwPauseButton, FALSE);
                            EnableWindow(hwStepPPUButton, FALSE);
                            EnableWindow(hwStopButton, FALSE);
                            EnableWindow(hwStepFrameButton, FALSE);

                            // Display error message
                            WCHAR cart_error_strings[9][100] = {
                                L"Error: cartridge is loaded already", 
                                L"Error: opening file", 
                                L"Error: file is too small", 
                                L"Error: not enough memory", 
                                L"Error: reading file", 
                                L"Error: ROM size incompatible with this cart type", 
                                L"Error: RAM size incompatible with this cart type", 
                                L"Error: file size incompatible with this cart type", 
                                L"Error: cartridge not supported"
                            };

                            WCHAR error_msg[100] = { 0 };

                            if (load_err < _countof(cart_error_strings))
                                wcscpy_s(error_msg, _countof(error_msg), cart_error_strings[load_err]);
                            else
                                wcscpy_s(error_msg, _countof(error_msg), L"Error");
                            MessageBoxW(hwnd, error_msg, L"Error loading cartridge", MB_OK|MB_ICONERROR);
                        }
                        break;
                    }
                    case SAVE_CART_ID:
                    {
                       // Save cartridge to file
                        char filename[MAX_PATH] = { 0 };
                        char exts[] = "Gameboy ROMs\0*.gb\0\0";
                        OPENFILENAMEA filenameinfo = {
                            sizeof(OPENFILENAMEA),
                            hwnd,
                            NULL,
                            exts,
                            NULL,
                            NULL,
                            0,
                            filename,
                            MAX_PATH,
                            NULL,
                            0,
                            NULL,
                            NULL,
                            OFN_OVERWRITEPROMPT,
                            0,
                            0,
                            "gb",
                            NULL,
                            NULL,
                            NULL,
                            NULL,
                            NULL,
                            0
                        };
                        if (!GetSaveFileNameA(&filenameinfo))
                            break;
                        if (gameboy_cartridge_save(filename) != 0)
                        {
                            // Display error
                            MessageBoxW(hwnd, L"Error saving cartridge to file", L"Error", MB_OK | MB_ICONERROR);
                        }
                    }
                }
            }
            else if (HIWORD(wParam) == BN_CLICKED)
            {
                switch (LOWORD(wParam)) // Button ID
                {
                    case STEP_BUTTON_ID:
                    {
                        UpdateLCD(hwnd);
                        UpdateInfo(hwnd);
                        RenderLCDScreen(hwnd, screen_buffer, 4, 100, 30);
                        
                        break;
                    }
                    case STEP_PPU_BUTTON_ID:
                    {
                        gameboy_clock();
                        UpdateInfo(hwnd);
                        RenderLCDScreen(hwnd, screen_buffer, 4, 100, 30);

                        break;
                    }
                    case PLAY_BUTTON_ID:
                    {
                        bPause = FALSE;
                        bStop = FALSE;

                        // Disable Play and Step buttons
                        HWND hwPlayButton = GetDlgItem(hwnd, PLAY_BUTTON_ID);
                        HWND hwStepButton = GetDlgItem(hwnd, STEP_BUTTON_ID);
                        HWND hwStepPPUButton = GetDlgItem(hwnd, STEP_PPU_BUTTON_ID);
                        HWND hwStepFrameButton = GetDlgItem(hwnd, STEP_FRAME_BUTTON_ID);
                        EnableWindow(hwPlayButton, FALSE);
                        EnableWindow(hwStepButton, FALSE);
                        EnableWindow(hwStepPPUButton, FALSE);
                        EnableWindow(hwStepFrameButton, FALSE);

                        // Enable Stop and Pause buttons
                        HWND hwStopButton = GetDlgItem(hwnd, STOP_BUTTON_ID);
                        HWND hwPauseButton = GetDlgItem(hwnd, PAUSE_BUTTON_ID);
                        EnableWindow(hwStopButton, TRUE);
                        EnableWindow(hwPauseButton, TRUE);

                        CreateThread(NULL,
                            0,
                            game_play,
                            hwnd,
                            0,
                            &dwPlayThread);


                        break;
                    }
                    case STEP_FRAME_BUTTON_ID:
                    {
                        bPause = FALSE;
                        bStop = FALSE;

                        // Disable Play and Step buttons
                        HWND hwPlayButton = GetDlgItem(hwnd, PLAY_BUTTON_ID);
                        HWND hwStepButton = GetDlgItem(hwnd, STEP_BUTTON_ID);
                        HWND hwStepPPUButton = GetDlgItem(hwnd, STEP_PPU_BUTTON_ID);
                        HWND hwStepFrameButton = GetDlgItem(hwnd, STEP_FRAME_BUTTON_ID);
                        EnableWindow(hwPlayButton, FALSE);
                        EnableWindow(hwStepButton, FALSE);
                        EnableWindow(hwStepPPUButton, FALSE);
                        EnableWindow(hwStepFrameButton, FALSE);

                        // Enable Stop and Pause buttons
                        HWND hwStopButton = GetDlgItem(hwnd, STOP_BUTTON_ID);
                        HWND hwPauseButton = GetDlgItem(hwnd, PAUSE_BUTTON_ID);
                        EnableWindow(hwStopButton, TRUE);
                        EnableWindow(hwPauseButton, TRUE);

                        CreateThread(NULL,
                            0,
                            game_step_frame,
                            hwnd,
                            0,
                            NULL);

                        break;
                    }
                    case STOP_BUTTON_ID:
                    {
                        // Stop
                        bStop = TRUE;
                        if (!bPause)
                            game_pause(hwnd);
                        else
                        {
                            game_reset();
                            UpdateInfo(hwnd);
                            RenderLCDScreen(hwnd, screen_buffer, 4, 100, 30);
                        }
                        

                        break;
                    }
                    case PAUSE_BUTTON_ID:
                    {
                        game_pause(hwnd);

                        break;
                    }
                    case BREAKPOINT_SET_ID:
                    {
                        // Set the breakpoint address
                        HWND hwndBreakpointEdit = GetDlgItem(hwnd, BREAKPOINT_EDIT_ID);
                        WCHAR addr_text[20] = { 0 };
                        addr_text[0] = 20; // Set the first word to the size of the buffer
                        SendMessageW(hwndBreakpointEdit, EM_GETLINE, 0, addr_text);
                        breakpoint_addr = (uint16_t)wcstol(addr_text, NULL, 16);

                        UpdateInfo(hwnd);
                        break;
                    }
                    case BREAKPOINT_CLEAR_ID:
                    {
                        // Set the breakpoint address to 0x0000
                        breakpoint_addr = 0x0000;

                        UpdateInfo(hwnd);
                        break;
                    }
                    case RAMVIEW_SET_ID:
                    {
                        // Set the RAM view address
                        HWND hwndRamviewEdit = GetDlgItem(hwnd, RAMVIEW_EDIT_ID);
                        WCHAR addr_text[20] = { 0 };
                        addr_text[0] = 20; // Set the first word to the size of the buffer
                        SendMessageW(hwndRamviewEdit, EM_GETLINE, 0, addr_text);
                        ram_view_addr = (uint16_t)wcstol(addr_text, NULL, 16);

                        UpdateInfo(hwnd);
                        break;
                    }
                }
            }
            return 0;
        }
        case WM_LBUTTONDOWN:
        {
            SetFocus(hwnd);
            break;
        }
        case WM_KEYDOWN:
        {
            if (!(lParam & 0x40000000) && !bPause && !bStop)
            {
                // Keys used:
                // A - B Button
                // S - A Button
                // Shift - Select Button
                // Enter - Start Button
                // Directional keys are mapped to directional keys
                HDC hdc = GetDC(hwnd);
                HPEN pen = CreatePen(PS_SOLID, 2, (COLORREF)RGB(130, 130, 130));
                HBRUSH brush = CreateSolidBrush((COLORREF)RGB(220, 220, 220));
                SelectObject(hdc, (HGDIOBJ)brush);
                SelectObject(hdc, (HGDIOBJ)pen);

                int prev_gm = GetGraphicsMode(hdc);
                XFORM prev_xfm;
                GetWorldTransform(hdc, &prev_xfm);
                double factor = (2.0f * 3.1416f) / 360.0f;
                double rot = -25.0f * factor;

                // Create a matrix for the transform we want (read the docs for details)
                XFORM xfm = { 0.0f };
                xfm.eM11 = (float)cos(rot);
                xfm.eM12 = (float)sin(rot);
                xfm.eM21 = (float)-sin(rot);
                xfm.eM22 = (float)cos(rot);

                POINT prev_org;
                GetWindowOrgEx(hdc, &prev_org);
                switch (wParam)
                {
                    case VK_UP:
                        gameboy_joypad_input(GB_JOYPAD_UP, true);
#define SCL 36
#define X 206
#define Y 682
                        BeginPath(hdc);
                        MoveToEx(hdc, X+SCL, Y, NULL);
                        LineTo(hdc, X+2*SCL, Y);
                        LineTo(hdc, X+2*SCL, Y+SCL+4);
                        LineTo(hdc, X+1.5*SCL, Y+1.5*SCL+4);
                        LineTo(hdc, X+SCL, Y+SCL+4);
                        EndPath(hdc);
                        FillPath(hdc);
                        break;
                    case VK_DOWN:
                        gameboy_joypad_input(GB_JOYPAD_DOWN, true);

                        BeginPath(hdc);
                        MoveToEx(hdc, X+SCL, Y+3*SCL+8, NULL);
                        LineTo(hdc, X+2*SCL, Y+3*SCL+8);
                        LineTo(hdc, X+2*SCL, Y +2*SCL+4);
                        LineTo(hdc, X+1.5*SCL, Y+1.5*SCL+4);
                        LineTo(hdc, X+SCL, Y+2*SCL+4);
                        EndPath(hdc);
                        FillPath(hdc);
                        break;
                    case VK_LEFT:
                        gameboy_joypad_input(GB_JOYPAD_LEFT, true);

                        BeginPath(hdc);
                        MoveToEx(hdc, X-4 , Y +SCL+4, NULL);
                        LineTo(hdc, X+SCL, Y+SCL+4);
                        LineTo(hdc, X + 1.5 * SCL, Y + 1.5 * SCL + 4);
                        LineTo(hdc, X+SCL, Y+2*SCL+4);
                        LineTo(hdc, X-4, Y+2*SCL+4);
                        EndPath(hdc);
                        FillPath(hdc);
                        break;
                    case VK_RIGHT:
                        gameboy_joypad_input(GB_JOYPAD_RIGHT, true);

                        BeginPath(hdc);
                        MoveToEx(hdc, X+2*SCL, Y+SCL+4, NULL);
                        LineTo(hdc, X + 1.5 * SCL, Y + 1.5 * SCL + 4);
                        LineTo(hdc, X+2*SCL, Y+2*SCL+4);
                        LineTo(hdc, X+3*SCL+4, Y+2*SCL+4);
                        LineTo(hdc, X+3*SCL+4, Y+SCL+4);
                        EndPath(hdc);
                        FillPath(hdc);
                        break;
                    case VK_SHIFT:
                        gameboy_joypad_input(GB_JOYPAD_SELECT, true);
#define X 310
#define Y 860
#define SCLX 60
#define SCLY 15
                        SetGraphicsMode(hdc, GM_ADVANCED);
                        SetWorldTransform(hdc, &xfm);
                        SetWindowOrgEx(hdc, -X, -Y, NULL);
                        RoundRect(hdc, 0, 0, SCLX, SCLY, SCLY, SCLY);
                        SetWindowOrgEx(hdc, prev_org.x, prev_org.y, NULL);
                        SetGraphicsMode(hdc, prev_gm);
                        SetWorldTransform(hdc, &prev_xfm);
                        break;
                    case VK_RETURN:
                        gameboy_joypad_input(GB_JOYPAD_START, true);
#define X 400
#define Y 860
                        SetGraphicsMode(hdc, GM_ADVANCED);
                        SetWorldTransform(hdc, &xfm);
                        SetWindowOrgEx(hdc, -X, -Y, NULL);
                        RoundRect(hdc, 0, 0, SCLX, SCLY, SCLY, SCLY);
                        SetWindowOrgEx(hdc, prev_org.x, prev_org.y, NULL);
                        SetGraphicsMode(hdc, prev_gm);
                        SetWorldTransform(hdc, &prev_xfm);
                        break;
                    case 'A':
                        gameboy_joypad_input(GB_JOYPAD_B, true);
#define SCL 60
#define X 490
#define Y 720
                        Ellipse(hdc, X, Y, X + SCL, Y + SCL);
                        break;
                    case 'S':
                        gameboy_joypad_input(GB_JOYPAD_A, true);
#define SCL 60
#define X 570
#define Y 680
                        Ellipse(hdc, X, Y, X + SCL, Y + SCL);
                        break;
                }
                DeleteObject(brush);
                DeleteObject(pen);
                ReleaseDC(hwnd, hdc);
            }
            break;
        }
        case WM_KEYUP:
        {
            if (!bPause && !bStop)
            {
                HDC hdc = GetDC(hwnd);
                HPEN pen = CreatePen(PS_SOLID, 2, (COLORREF)RGB(130, 130, 130));
                HBRUSH brush = CreateSolidBrush((COLORREF)RGB(255, 255, 255));
                SelectObject(hdc, (HGDIOBJ)brush);
                SelectObject(hdc, (HGDIOBJ)pen);

                int prev_gm = GetGraphicsMode(hdc);
                XFORM prev_xfm;
                GetWorldTransform(hdc, &prev_xfm);
                double factor = (2.0f * 3.1416f) / 360.0f;
                double rot = -25.0f * factor;

                // Create a matrix for the transform we want (read the docs for details)
                XFORM xfm = { 0.0f };
                xfm.eM11 = (float)cos(rot);
                xfm.eM12 = (float)sin(rot);
                xfm.eM21 = (float)-sin(rot);
                xfm.eM22 = (float)cos(rot);

                POINT prev_org;
                GetWindowOrgEx(hdc, &prev_org);
                switch (wParam)
                {
                case VK_UP:
                    gameboy_joypad_input(GB_JOYPAD_UP, false);
#define SCL 36
#define X 206
#define Y 682
                    BeginPath(hdc);
                    MoveToEx(hdc, X + SCL, Y, NULL);
                    LineTo(hdc, X + 2 * SCL, Y);
                    LineTo(hdc, X + 2 * SCL, Y + SCL + 4);
                    LineTo(hdc, X + 1.5 * SCL, Y + 1.5 * SCL + 4);
                    LineTo(hdc, X + SCL, Y + SCL + 4);
                    EndPath(hdc);
                    FillPath(hdc);
                    break;
                case VK_DOWN:
                    gameboy_joypad_input(GB_JOYPAD_DOWN, false);
                    BeginPath(hdc);
                    MoveToEx(hdc, X + SCL, Y + 3 * SCL + 8, NULL);
                    LineTo(hdc, X + 2 * SCL, Y + 3 * SCL + 8);
                    LineTo(hdc, X + 2 * SCL, Y + 2 * SCL + 4);
                    LineTo(hdc, X + 1.5 * SCL, Y + 1.5 * SCL + 4);
                    LineTo(hdc, X + SCL, Y + 2 * SCL + 4);
                    EndPath(hdc);
                    FillPath(hdc);
                    break;
                case VK_LEFT:
                    gameboy_joypad_input(GB_JOYPAD_LEFT, false);
                    BeginPath(hdc);
                    MoveToEx(hdc, X - 4, Y + SCL + 4, NULL);
                    LineTo(hdc, X + SCL, Y + SCL + 4);
                    LineTo(hdc, X + 1.5 * SCL, Y + 1.5 * SCL + 4);
                    LineTo(hdc, X + SCL, Y + 2 * SCL + 4);
                    LineTo(hdc, X - 4, Y + 2 * SCL + 4);
                    EndPath(hdc);
                    FillPath(hdc);
                    break;
                case VK_RIGHT:
                    gameboy_joypad_input(GB_JOYPAD_RIGHT, false);
                    BeginPath(hdc);
                    MoveToEx(hdc, X + 2 * SCL, Y + SCL + 4, NULL);
                    LineTo(hdc, X + 1.5 * SCL, Y + 1.5 * SCL + 4);
                    LineTo(hdc, X + 2 * SCL, Y + 2 * SCL + 4);
                    LineTo(hdc, X + 3 * SCL + 4, Y + 2 * SCL + 4);
                    LineTo(hdc, X + 3 * SCL + 4, Y + SCL + 4);
                    EndPath(hdc);
                    FillPath(hdc);
                    break;
                case VK_SHIFT:
                    gameboy_joypad_input(GB_JOYPAD_SELECT, false);
#define X 310
#define Y 860
#define SCLX 60
#define SCLY 15
                    SetGraphicsMode(hdc, GM_ADVANCED);
                    SetWorldTransform(hdc, &xfm);
                    SetWindowOrgEx(hdc, -X, -Y, NULL);
                    RoundRect(hdc, 0, 0, SCLX, SCLY, SCLY, SCLY);
                    SetWindowOrgEx(hdc, prev_org.x, prev_org.y, NULL);
                    SetGraphicsMode(hdc, prev_gm);
                    SetWorldTransform(hdc, &prev_xfm);
                    break;
                case VK_RETURN:
                    gameboy_joypad_input(GB_JOYPAD_START, false);
#define X 400
#define Y 860
                    SetGraphicsMode(hdc, GM_ADVANCED);
                    SetWorldTransform(hdc, &xfm);
                    SetWindowOrgEx(hdc, -X, -Y, NULL);
                    RoundRect(hdc, 0, 0, SCLX, SCLY, SCLY, SCLY);
                    SetWindowOrgEx(hdc, prev_org.x, prev_org.y, NULL);
                    SetGraphicsMode(hdc, prev_gm);
                    SetWorldTransform(hdc, &prev_xfm);
                    break;
                case 'A':
                    gameboy_joypad_input(GB_JOYPAD_B, false);
#define SCL 60
#define X 490
#define Y 720
                    Ellipse(hdc, X, Y, X + SCL, Y + SCL);
                    break;
                case 'S':
                    gameboy_joypad_input(GB_JOYPAD_A, false);
#define SCL 60
#define X 570
#define Y 680
                    Ellipse(hdc, X, Y, X + SCL, Y + SCL);
                    break;
                }
                DeleteObject(brush);
                DeleteObject(pen);
                ReleaseDC(hwnd, hdc);
            }
            break;
        }
        case WM_DESTROY:
        {
            bPause = true;
            bStop = true;
            // Perform cleanup tasks.
            gameboy_cart_unload();
            //LCD_free();
            HWND hwRamview = GetDlgItem(hwnd, RAMVIEW_BOX_ID);
            HFONT monofont = GetWindowFont(hwRamview);
            DeleteObject(monofont);

            //gameboy_reset(false);
            PostQuitMessage(0);
            return 0;
        }
        
    }
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int RenderLCDScreen(HWND hwnd, void* screen_buffer, int nScale, int xOffset, int yOffset)
{
    HDC hdc = GetDC(hwnd);
    // Creating temp bitmap
    HBITMAP map = CreateBitmap(160, // width
        144, // height
        1, // Color Planes, unfortanutelly don't know what is it actually. Let it be 1
        8 * 4, // Size of memory for one pixel in bits (in win32 4 bytes = 4*8 bits)
        (void*)screen_buffer); // pointer to array

    // Temp HDC to copy picture
    HDC src = CreateCompatibleDC(hdc); // hdc - Device context for window, I've got earlier with GetDC(hWnd) or GetDC(NULL);
    SelectObject(src, map); // Inserting picture into our temp HDC
    // Copy image from temp HDC to window
    StretchBlt(hdc, // Destination
        xOffset,  // x and
        yOffset,  // y - upper-left corner of place, where we'd like to copy
        160 * nScale, // Width
        144 * nScale, // Height
        src, // source
        0,   // x and
        0,   // y of upper left corner  of part of the source, from where we'd like to copy
        160,
        144,
        SRCCOPY); // Defined DWORD to juct copy pixels. Watch more on msdn;
    frame.left = xOffset - 1;
    frame.top = yOffset - 1;
    frame.right = xOffset + 160 * nScale + 1;
    frame.bottom = yOffset + 144 * nScale + 1;

    ReleaseDC(hwnd, hdc);
    DeleteDC(src); // Deleting temp HDC
    DeleteObject(map);
    return 0;
}

void game_pause(HWND hwnd)
{
    bPause = TRUE;
    

    // Disable Stop and Pause buttons
    HWND hwPauseButton = GetDlgItem(hwnd, PAUSE_BUTTON_ID);
    EnableWindow(hwPauseButton, FALSE);

    // Enable Play and Step buttons
    HWND hwPlayButton = GetDlgItem(hwnd, PLAY_BUTTON_ID);
    HWND hwStepButton = GetDlgItem(hwnd, STEP_BUTTON_ID);
    HWND hwStepPPUButton = GetDlgItem(hwnd, STEP_PPU_BUTTON_ID);
    HWND hwStepFrameButton = GetDlgItem(hwnd, STEP_FRAME_BUTTON_ID);
    EnableWindow(hwPlayButton, TRUE);
    EnableWindow(hwStepButton, TRUE);
    EnableWindow(hwStepPPUButton, TRUE);
    EnableWindow(hwStepFrameButton, TRUE);

    UpdateInfo(hwnd);
    RenderLCDScreen(hwnd, screen_buffer, 4, 100, 30);
    
}

void UpdateLCD(HWND hwnd) 
{
    uint8_t cycles_left = -1;
    do
    {
        gameboy_mclock();
        gameboy_cpu_stats(
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            &cycles_left,
            NULL,
            NULL,
            NULL,
            NULL);
    } 
    while (cycles_left != 0);
    
    //RenderLCDScreen(hwnd, 4, 100, 30);
}

void UpdateInfo(HWND hwnd)
{
    HDC hdc = GetDC(hwnd);

    // Select font
    HFONT default_font = GetStockFont(DEFAULT_GUI_FONT);
    SelectFont(hdc, default_font);

    // Clock Count
    WCHAR clock_text[50] = { 0 };
    StringCchPrintfW(clock_text, 50, L"Clock cycles: %I64d\n", clock_count);

    // CPU Stats
    WCHAR opcode_text[80] = { 0 };
    WCHAR regs_text[80] = { 0 };
    WCHAR regs_16_text[80] = { 0 };
    WCHAR fetched_text[80] = { 0 };
    WCHAR flags_text[80] = { 0 };
    WCHAR instruction_text[80] = { 0 };
    WCHAR disassembly_text[80] = { 0 };
    uint8_t curr_op = 0;
    uint16_t curr_af = 0, curr_bc = 0, curr_de = 0, curr_hl = 0, curr_sp = 0, curr_pc = 0;
    uint8_t curr_fetched, curr_cb_opcode;
    uint16_t curr_fetched_16, curr_stackbase;
    gameboy_cpu_stats(
        &curr_af,
        &curr_bc,
        &curr_de,
        &curr_hl,
        &curr_sp,
        &curr_pc,
        NULL,
        &curr_op,
        NULL,
        &curr_fetched,
        &curr_fetched_16,
        &curr_cb_opcode,
        &curr_stackbase
    );
    gameboy_cpu_disassemble_inst(curr_pc, disassembly_text, 150);
    StringCchPrintfW(opcode_text, _countof(opcode_text), L"Current executing opcode: 0x%02X, current 0xCB opcode: 0x%02X", curr_op, curr_cb_opcode);
    StringCchPrintfW(regs_16_text, _countof(regs_16_text), L"AF: 0x%04X, BC: 0x%04X, DE: 0x%04X, HL: 0x%04X, SP: 0x%04X, PC: 0x%04X", 
        curr_af, curr_bc, curr_de, curr_hl, curr_sp, curr_pc);
    StringCchPrintfW(regs_text, _countof(regs_text), L"A: 0x%02X, B: 0x%02X, C: 0x%02X, D: 0x%02X, E: 0x%02X, H: 0x%02X, L: 0x%02X, F: 0x%02X", 
        curr_af >> 8, curr_bc >> 8, curr_bc & 0xff, curr_de >> 8, curr_de & 0xff, curr_hl >> 8, 
        curr_hl & 0xff, curr_af & 0xff);
    StringCchPrintfW(flags_text, _countof(flags_text), L"Flags: Zero: %d, Subtract: %d, Half-Carry: %d, Carry: %d", 
        curr_af & (1 << 7) ? 1 : 0, curr_af & (1 << 6) ? 1 : 0, curr_af & (1 << 5) ? 1 : 0, curr_af & (1 << 4) ? 1 : 0);
    StringCchPrintfW(fetched_text, _countof(fetched_text), L"Fetched: 0x%02X, Fetched-16: 0x%04X", 
        curr_fetched, curr_fetched_16);
    StringCchPrintfW(instruction_text, _countof(instruction_text), L"Next instruction: %s", disassembly_text);

    ExtTextOutW(
        hdc,
        800,    // X
        100,     // Y
        ETO_NUMERICSLOCAL,   // Flags
        NULL,   // Rect
        clock_text,
        _countof(clock_text),
        NULL);
    ExtTextOutW(
        hdc,
        800,    // X
        120,     // Y
        ETO_NUMERICSLOCAL,   // Flags
        NULL,   // Rect
        opcode_text,
        _countof(opcode_text),
        NULL);
    ExtTextOutW(
        hdc,
        800,    // X
        140,     // Y
        ETO_NUMERICSLOCAL,   // Flags
        NULL,   // Rect
        regs_16_text,
        _countof(regs_16_text),
        NULL);
    ExtTextOutW(
        hdc,
        800,    // X
        160,     // Y
        ETO_NUMERICSLOCAL,   // Flags
        NULL,   // Rect
        regs_text,
        _countof(regs_text),
        NULL);
    ExtTextOutW(
        hdc,
        800,    // X
        180,     // Y
        ETO_NUMERICSLOCAL,   // Flags
        NULL,   // Rect
        flags_text,
        _countof(flags_text),
        NULL);
    ExtTextOutW(
        hdc,
        800,    // X
        200,     // Y
        ETO_NUMERICSLOCAL,   // Flags
        NULL,   // Rect
        fetched_text,
        _countof(fetched_text),
        NULL);
    ExtTextOutW(
        hdc,
        800,    // X
        220,     // Y
        ETO_NUMERICSLOCAL,   // Flags
        NULL,   // Rect
        instruction_text,
        _countof(instruction_text),
        NULL);


    // Add breakpoint text
    WCHAR breakpoint_text[50] = { 0 };
    WCHAR breakpoint_addr_text[80] = { 0 };
    wcscpy_s(breakpoint_text, 50, L"Set breakpoint at address:");
    if (breakpoint_addr != 0x0000)
        StringCchPrintfW(breakpoint_addr_text, 80, L"Breakpoint address: $%04x", breakpoint_addr);
    else
        wcscpy_s(breakpoint_addr_text, 80, L"Breakpoint address: $------");
    ExtTextOutW(
        hdc,
        800,    // X
        280,     // Y
        ETO_NUMERICSLOCAL,   // Flags
        NULL,   // Rect
        breakpoint_text,
        _countof(breakpoint_text),
        NULL);
    ExtTextOutW(
        hdc,
        800,    // X
        335,     // Y
        ETO_NUMERICSLOCAL,   // Flags
        NULL,   // Rect
        breakpoint_addr_text,
        _countof(breakpoint_addr_text),
        NULL);

    // Add stack visualization
    #define MAX_STACK_ELEMENTS 16
    uint8_t temp = 0x00;
    uint16_t stack_element = 0x0000;
    WCHAR element_text[50] = { 0 };
    int nElements = min(((curr_stackbase - curr_sp) / 2), MAX_STACK_ELEMENTS);
    for (int i = 0; i < nElements; i++)
    {
        temp = bus_read(curr_stackbase - 1 - 2*i, DEV_CPU);
        stack_element = temp << 8 | (uint16_t)bus_read(curr_stackbase - 2 - 2*i, DEV_CPU);

        StringCchPrintfW(element_text, 50, L"%02d 0x%04x $%04x", i, stack_element, curr_stackbase - 1 - 2*i);
        ExtTextOutW(
            hdc,
            1300,    // X
            255 + 25*i,     // Y
            ETO_NUMERICSLOCAL,   // Flags
            NULL,   // Rect
            element_text,
            _countof(element_text),
            NULL);
    }
    // Delete all former elements
    RECT delete_rect = {1300, 255+25*nElements, 1420, 250+25*16};
    HBRUSH delete_color = CreateSolidBrush(RGB(255, 255, 255));
    FillRect(hdc, &delete_rect, delete_color);
    DeleteObject(delete_color);

    // Create frame
    RECT stack_frame = {1290, 250, 1430, 655};
    HBRUSH stack_frame_color = CreateSolidBrush(RGB(40, 40, 40));
    FrameRect(hdc, &stack_frame, stack_frame_color);
    DeleteObject(stack_frame_color);

    // Add text
    WCHAR stack_title_text[] = L"The Stack";
    WCHAR stack_text[] = L"value   addr";
    ExtTextOutW(
        hdc,
        1315,    // X
        660,     // Y
        ETO_NUMERICSLOCAL,   // Flags
        NULL,   // Rect
        stack_title_text,
        _countof(stack_title_text),
        NULL);
    ExtTextOutW(
        hdc,
        1325,    // X
        230,     // Y
        ETO_NUMERICSLOCAL,   // Flags
        NULL,   // Rect
        stack_text,
        _countof(stack_text),
        NULL);

    // RAM view
    // Ram starts at ram_view address
    // Hex dump format:
    // 0000  00 11 22 33 44 55 66 77  88 99 aa bb cc dd ee ff
#define RAM_ROWS 12
#define a ram_view_row_bytes
    WCHAR ram_view_text[80*RAM_ROWS] = { 0 };
    for (int i = 0; i < RAM_ROWS; i++)
    {
        WCHAR ram_view_row_text[80] = { 0 };
        uint8_t a[16] = { 0 };
        for (int j = 0; j < 16; j++)
        {
            a[j] = bus_read(ram_view_addr+i*16+j, DEV_CPU);
        }
        StringCchPrintfW(ram_view_row_text, 80, L"%04x  %02x %02x %02x %02x %02x %02x %02x %02x  %02x %02x %02x %02x %02x %02x %02x %02x\r\n",
            (uint16_t)(ram_view_addr+i*16), a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8], a[9], a[10], a[11], a[12], a[13], a[14], a[15]);

        wcscat_s(ram_view_text, 80*RAM_ROWS, ram_view_row_text);
    }
   
    HWND hwRamview = GetDlgItem(hwnd, RAMVIEW_BOX_ID);
    SetWindowText(hwRamview, ram_view_text);
    
    // Add text
    WCHAR ram_view_title_text[] = L"RAM View";
    ExtTextOutW(
        hdc,
        980,    // X
        370,     // Y
        ETO_NUMERICSLOCAL,   // Flags
        NULL,   // Rect
        ram_view_title_text,
        _countof(ram_view_title_text),
        NULL);


    // PPU stats
    uint8_t curr_lcdc, curr_stat, curr_scy, curr_scx, curr_ly, curr_lyc, curr_bgp,
        curr_obp0, curr_obp1, curr_wy, curr_wx;
    gameboy_ppu_stats(
        &curr_lcdc,
        &curr_stat,
        &curr_scy,
        &curr_scx,
        &curr_ly,
        &curr_lyc,
        &curr_bgp,
        &curr_obp0,
        &curr_obp1,
        &curr_wy,
        &curr_wx
    );
    WCHAR ppu_stats_text[200] = { 0 };
    RECT ppu_stats_box = {1200, 200, 1300, 600};
    StringCchPrintfW(ppu_stats_text, 200, L"LCDC: $%02X\r\nSTAT: $%02X\r\nSCY: $%02X\r\nSCX: $%02X\r\nLY: $%02X\r\nLYC: $%02X\r\nBGP: $%02X\r\nOBP0: $%02X\r\nOBP1: $%02X\r\nWY: $%02X\r\nWX: $%02X", 
        curr_lcdc, curr_stat, curr_scy, curr_scx, curr_ly, curr_lyc, curr_bgp, curr_obp0, curr_obp1, 
        curr_wy, curr_wx);
    DrawTextW(
        hdc,
        ppu_stats_text,
        -1,
        &ppu_stats_box,
        DT_LEFT
        );

    ReleaseDC(hwnd, hdc);
}

void displayCartStats(HWND hwnd)
{
    HDC hdc = GetDC(hwnd);

    // Select font
    HFONT default_font = GetStockFont(DEFAULT_GUI_FONT);
    SelectFont(hdc, default_font);

    // Title
    WCHAR game_title[20] = { 0 };
    mbstowcs_s(NULL, game_title, _countof(game_title), curr_cart_stats.title, _countof(curr_cart_stats.title));
    WCHAR game_title_text[40] = { 0 };
    wcscpy_s(game_title_text, _countof(game_title_text), L"Game title: ");
    wcscat_s(game_title_text, _countof(game_title_text), game_title);
    ExtTextOutW(
        hdc,
        1460,    // X
        100,     // Y
        ETO_NUMERICSLOCAL,   // Flags
        NULL,   // Rect
        game_title_text,
        _countof(game_title_text),
        NULL);
    
    // Type
    WCHAR cart_type[60] = { 0 };
    switch (curr_cart_stats.type)
    {
        case CART_ROM_ONLY:
            wcscpy_s(cart_type, _countof(cart_type), L"ROM ONLY");
            break;
        case CART_MBC1:
            wcscpy_s(cart_type, _countof(cart_type), L"MBC1");
            break;
        case CART_MBC1_RAM:
            wcscpy_s(cart_type, _countof(cart_type), L"MBC1+RAM");
            break;
        case CART_MBC1_RAM_BATTERY:
            wcscpy_s(cart_type, _countof(cart_type), L"MBC1+RAM+BATTERY");
            break;
        case CART_MBC2:
            wcscpy_s(cart_type, _countof(cart_type), L"MBC2");
            break;
        case CART_MBC2_BATTERY:
            wcscpy_s(cart_type, _countof(cart_type), L"MBC2+BATTERY");
            break;
        case CART_ROM_RAM:
            wcscpy_s(cart_type, _countof(cart_type), L"ROM+RAM");
            break;
        case CART_ROM_RAM_BATTERY:
            wcscpy_s(cart_type, _countof(cart_type), L"ROM+RAM+BATTERY");
            break;
        case CART_MMM01:
            wcscpy_s(cart_type, _countof(cart_type), L"MMM01");
            break;
        case CART_MMM01_RAM:
            wcscpy_s(cart_type, _countof(cart_type), L"MMM01+RAM");
            break;
        case CART_MMM01_RAM_BATTERY:
            wcscpy_s(cart_type, _countof(cart_type), L"MMM01+RAM+BATTERY");
            break;
        case CART_MBC3_TIMER_BATTERY:
            wcscpy_s(cart_type, _countof(cart_type), L"MBC3+TIMER+BATTERY");
            break;
        case CART_MBC3_TIMER_RAM_BATTERY:
            wcscpy_s(cart_type, _countof(cart_type), L"MBC3+TIMER+RAM+BATTERY");
            break;
        case CART_MBC3:
            wcscpy_s(cart_type, _countof(cart_type), L"MBC3");
            break;
        case CART_MBC3_RAM:
            wcscpy_s(cart_type, _countof(cart_type), L"MBC3+RAM");
            break;
        case CART_MBC3_RAM_BATTERY:
            wcscpy_s(cart_type, _countof(cart_type), L"MBC3+RAM+BATTERY");
            break;
        case CART_MBC5:
            wcscpy_s(cart_type, _countof(cart_type), L"MBC5");
            break;
        case CART_MBC5_RAM:
            wcscpy_s(cart_type, _countof(cart_type), L"MBC5+RAM");
            break;
        case CART_MBC5_RAM_BATTERY:
            wcscpy_s(cart_type, _countof(cart_type), L"MBC5+RAM+BATTERY");
            break;
        case CART_MBC5_RUMBLE:
            wcscpy_s(cart_type, _countof(cart_type), L"MBC5+RUMBLE");
            break;
        case CART_MBC5_RUMBLE_RAM:
            wcscpy_s(cart_type, _countof(cart_type), L"MBC5+RUMBLE+RAM");
            break;
        case CART_MBC5_RUMBLE_RAM_BATTERY:
            wcscpy_s(cart_type, _countof(cart_type), L"MBC5+RUMBLE+RAM+BATTERY");
            break;
        case CART_MBC6:
            wcscpy_s(cart_type, _countof(cart_type), L"MBC6");
            break;
        case CART_MBC7_SENSOR_RUMBLE_RAM_BATTERY:
            wcscpy_s(cart_type, _countof(cart_type), L"MBC7+SENSOR+RUMBLE+RAM+BATTERY");
            break;
        case CART_POCKET_CAMERA:
            wcscpy_s(cart_type, _countof(cart_type), L"POCKET CAMERA");
            break;
        case CART_BANDAI_TAMA5:
            wcscpy_s(cart_type, _countof(cart_type), L"BANDAI TAMA5");
            break;
        case CART_HUC3:
            wcscpy_s(cart_type, _countof(cart_type), L"HUC3");
            break;
        case CART_HUC1_RAM_BATTERY:
            wcscpy_s(cart_type, _countof(cart_type), L"HUC1+RAM+BATTERY");
            break;
        default:
            wcscpy_s(cart_type, _countof(cart_type), L"UNKNOWN");
            break;
    }
    WCHAR cart_type_text[40] = { 0 };
    wcscpy_s(cart_type_text, _countof(cart_type_text), L"Cartridge type: ");
    wcscat_s(cart_type_text, _countof(cart_type_text), cart_type);
    ExtTextOutW(
        hdc,
        1460,    // X
        120,     // Y
        ETO_NUMERICSLOCAL,   // Flags
        NULL,   // Rect
        cart_type_text,
        _countof(cart_type_text),
        NULL);

    // ROM size
    WCHAR rom_size[10] = { 0 };
    switch (curr_cart_stats.rom_size)
    {
        case CART_ROM_32K:
            wcscpy_s(rom_size, _countof(rom_size), L"32Kb");
            break;
        case CART_ROM_64K:
            wcscpy_s(rom_size, _countof(rom_size), L"64Kb");
            break;
        case CART_ROM_128K:
            wcscpy_s(rom_size, _countof(rom_size), L"128Kb");
            break;
        case CART_ROM_256K:
            wcscpy_s(rom_size, _countof(rom_size), L"256Kb");
            break;
        case CART_ROM_512K:
            wcscpy_s(rom_size, _countof(rom_size), L"512Kb");
            break;
        case CART_ROM_1M:
            wcscpy_s(rom_size, _countof(rom_size), L"1Mb");
            break;
        case CART_ROM_2M:
            wcscpy_s(rom_size, _countof(rom_size), L"2Mb");
            break;
        case CART_ROM_4M:
            wcscpy_s(rom_size, _countof(rom_size), L"4Mb");
            break;
        case CART_ROM_8M:
            wcscpy_s(rom_size, _countof(rom_size), L"8Mb");
            break;
        case CART_ROM_1_1M:
            wcscpy_s(rom_size, _countof(rom_size), L"1.1Mb");
            break;
        case CART_ROM_1_2M:
            wcscpy_s(rom_size, _countof(rom_size), L"1.2Mb");
            break;
        case CART_ROM_1_5M:
            wcscpy_s(rom_size, _countof(rom_size), L"1.5Mb");
            break;
        default:
            wcscpy_s(rom_size, _countof(rom_size), L"Unknown");
            break;
    }
    WCHAR rom_size_text[40] = { 0 };
    wcscpy_s(rom_size_text, _countof(rom_size_text), L"ROM size: ");
    wcscat_s(rom_size_text, _countof(rom_size_text), rom_size);
    ExtTextOutW(
        hdc,
        1460,    // X
        140,     // Y
        ETO_NUMERICSLOCAL,   // Flags
        NULL,   // Rect
        rom_size_text,
        _countof(rom_size_text),
        NULL);

    // RAM size
    WCHAR ram_size[10] = { 0 };
    switch (curr_cart_stats.ram_size)
    {
        case CART_RAM_NONE:
            wcscpy_s(ram_size, _countof(ram_size), L"None");
            break;
        case CART_RAM_2K:
            wcscpy_s(ram_size, _countof(ram_size), L"2Kb");
            break;
        case CART_RAM_8K:
            wcscpy_s(ram_size, _countof(ram_size), L"8Kb");
            break;
        case CART_RAM_32K:
            wcscpy_s(ram_size, _countof(ram_size), L"32Kb");
            break;
        case CART_RAM_128K:
            wcscpy_s(ram_size, _countof(ram_size), L"128Kb");
            break;
        case CART_RAM_64K:
            wcscpy_s(ram_size, _countof(ram_size), L"64Kb");
            break;
        default:
            wcscpy_s(ram_size, _countof(ram_size), L"Unknown");
            break;
    }
    WCHAR ram_size_text[40] = { 0 };
    wcscpy_s(ram_size_text, _countof(ram_size_text), L"RAM size: ");
    wcscat_s(ram_size_text, _countof(ram_size_text), ram_size);
    ExtTextOutW(
        hdc,
        1460,    // X
        160,     // Y
        ETO_NUMERICSLOCAL,   // Flags
        NULL,   // Rect
        ram_size_text,
        _countof(ram_size_text),
        NULL);

    // Japan
    WCHAR japan_text[40] = { 0 };
    if (curr_cart_stats.japan)
        wcscpy_s(japan_text, _countof(japan_text), L"Destination: Non-Japanese");
    else
        wcscpy_s(japan_text, _countof(japan_text), L"Destination: Japan");
    ExtTextOutW(
        hdc,
        1460,    // X
        180,     // Y
        ETO_NUMERICSLOCAL,   // Flags
        NULL,   // Rect
        japan_text,
        _countof(japan_text),
        NULL);

    // Header checksum
    WCHAR header_chck_text[40] = { 0 };
    if (curr_cart_stats.header_chck)
        wcscpy_s(header_chck_text, _countof(header_chck_text), L"Header checksum: Valid");
    else
        wcscpy_s(header_chck_text, _countof(header_chck_text), L"Header checksum: Invalid");
    ExtTextOutW(
        hdc,
        1460,    // X
        200,     // Y
        ETO_NUMERICSLOCAL,   // Flags
        NULL,   // Rect
        header_chck_text,
        _countof(header_chck_text),
        NULL);

    // Global checksum
    WCHAR global_chck_text[40] = { 0 };
    if (curr_cart_stats.global_chck)
        wcscpy_s(global_chck_text, _countof(global_chck_text), L"Global checksum: Valid");
    else
        wcscpy_s(global_chck_text, _countof(global_chck_text), L"Global checksum: Invalid");
    ExtTextOutW(
        hdc,
        1460,    // X
        220,     // Y
        ETO_NUMERICSLOCAL,   // Flags
        NULL,   // Rect
        global_chck_text,
        _countof(global_chck_text),
        NULL);
    
    ReleaseDC(hwnd, hdc);
}

DWORD WINAPI game_play(HWND hwnd)
{
    // Create timer
    HANDLE hTimer = CreateWaitableTimer(NULL, TRUE, NULL);
    if (hTimer == NULL)
    {
        bPause = TRUE;
        bStop = TRUE;

        // Reset

        game_pause(hwnd);
        return;
    }
    
    // Set timer
    LARGE_INTEGER liDueTime;
    liDueTime.QuadPart = -100000LL;
    SetWaitableTimer(hTimer, &liDueTime, 0, NULL, NULL, FALSE);
    

    uint16_t current_pc = 0x0000;
    uint8_t curr_stat = 0x02;
    while (!bPause && ((current_pc != breakpoint_addr) || current_pc == 0))
    {
        UpdateLCD(hwnd);
        gameboy_cpu_stats(
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            &current_pc,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL
        );

        // Get PPU stats
        uint8_t past_stat = curr_stat;
        gameboy_ppu_stats(
            NULL,
            &curr_stat,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL
        );

        // When screen buffer is full, display frame and wait for 1/5 sec to pass since the time captured
        if (((curr_stat & 0x03) == 1) && ((past_stat & 0x03) != 1)) 
        {
            RenderLCDScreen(hwnd, screen_buffer, 4, 100, 30);
            WaitForSingleObject(hTimer, 17);
            SetWaitableTimer(hTimer, &liDueTime, 0, NULL, NULL, FALSE);
        }
        
    }
    CloseHandle(hTimer);
    if (bStop)
    {
        // Reset
        game_reset();
        bStop = FALSE;
    }
    game_pause(hwnd);
    UpdateInfo(hwnd);
    return 0;
}

DWORD WINAPI game_step_frame(HWND hwnd)
{
    uint16_t current_pc = 0x0000;
    uint8_t curr_stat = 0x01;
    while (!bPause && ((current_pc != breakpoint_addr) || current_pc == 0))
    {
        gameboy_clock();
        gameboy_cpu_stats(
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            &current_pc,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL
        );

        // Get PPU stats
        uint8_t past_stat = curr_stat;
        gameboy_ppu_stats(
            NULL,
            &curr_stat,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL
        );

        // When screen buffer is full, display frame and wait for 1/5 sec to pass since the time captured
        if (((curr_stat & 0x03) == 1) && ((past_stat & 0x03) != 1))
        {
            RenderLCDScreen(hwnd, screen_buffer, 4, 100, 30);
            break;
        }

    }
    game_pause(hwnd);
    UpdateInfo(hwnd);
    return 0;
}

void game_reset()
{
    if (gameboy_reset(false) != 0)
    {
        MessageBoxW(NULL, L"Error - could not reset", NULL, MB_OK);
    }
}