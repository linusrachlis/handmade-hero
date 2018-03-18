#include <windows.h>
#include <stdint.h>

#define internal static
#define local_persist static
#define global_variable static

global_variable bool Running = false;

global_variable BITMAPINFO BitmapInfo;
global_variable void *BitmapMemory;
global_variable int BitmapWidth;
global_variable int BitmapHeight;
global_variable int BytesPerPixel = 4;
global_variable uint8_t RedValue = 0;

internal void RenderWeirdGradient(int XOffset, int YOffset)
{
    // Pitch = byte offset from one row to the next
    int Pitch = BitmapWidth * BytesPerPixel;
    // Create pointer to the ... first 8 bits? lower 8 bits? TODO
    uint8_t *Row = (uint8_t *)BitmapMemory;

    for (int Y = 0; Y < BitmapHeight; Y++)
    {
        uint32_t *Pixel = (uint32_t *)Row;

        for (int X = 0; X < BitmapWidth; X++)
        {
            uint8_t Red = RedValue;
            uint8_t Green = (Y + YOffset);
            uint8_t Blue = (X + XOffset);
            // Pixel structure in register: xx RR GG BB
            *Pixel = (Red << 16) | (Green << 8) | Blue;
            // Advance to write next pixel
            Pixel++;
        }

        // Advance row pointer by number of bytes per row
        Row += Pitch;
    }

    RedValue++;
}

internal void Win32ResizeDIBSection(int Width, int Height)
{
    if (BitmapMemory)
    {
        VirtualFree(BitmapMemory, 0, MEM_RELEASE);
    }

    BitmapWidth = Width;
    BitmapHeight = Height;

    BitmapInfo.bmiHeader.biSize = sizeof(BitmapInfo.bmiHeader);
    BitmapInfo.bmiHeader.biWidth = BitmapWidth;
    BitmapInfo.bmiHeader.biHeight = -BitmapHeight;
    BitmapInfo.bmiHeader.biPlanes = 1;
    BitmapInfo.bmiHeader.biBitCount = 32;
    BitmapInfo.bmiHeader.biCompression = BI_RGB;

    int BitmapMemorySize = BitmapWidth * BitmapHeight * BytesPerPixel;
    BitmapMemory = VirtualAlloc(0, BitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);
}

internal void Win32UpdateWindow(
    HDC DeviceContext,
    RECT *ClientRect,
    int X,
    int Y,
    int Width,
    int Height)
{
    // TODO for now, ignoring the desired repainting rectangle, and just repainting the
    // whole window every time. If we stick with this way, then stop passing X,Y,Width,Height.
    int WindowWidth = ClientRect->right - ClientRect->left;
    int WindowHeight = ClientRect->bottom - ClientRect->top;
    StretchDIBits(DeviceContext,
        /*
        X, Y, Width, Height,
        X, Y, Width, Height,
        */
        0, 0, BitmapWidth, BitmapHeight,
        0, 0, WindowWidth, WindowHeight,
        BitmapMemory,
        &BitmapInfo,
        DIB_RGB_COLORS, SRCCOPY);
}

LRESULT CALLBACK Win32MainWindowCallback(
    HWND   Window,
    UINT   Message,
    WPARAM WParam,
    LPARAM LParam)
{
    LRESULT Result = 0;

    switch (Message)
    {
        case WM_SIZE:
        {
            OutputDebugStringA("WM_SIZE\n");
            RECT ClientRect;
            GetClientRect(Window, &ClientRect);
            int Width = ClientRect.right - ClientRect.left;
            int Height = ClientRect.bottom - ClientRect.top;
            Win32ResizeDIBSection(Width, Height);
        } break;

        case WM_CLOSE:
        {
            OutputDebugStringA("WM_CLOSE\n");
            Running = false;
        } break;

        case WM_ACTIVATEAPP:
        {
            OutputDebugStringA("WM_ACTIVATEAPP\n");
        } break;

        case WM_DESTROY:
        {
            OutputDebugStringA("WM_DESTROY\n");
            Running = false;
        } break;

        case WM_PAINT:
        {
            PAINTSTRUCT Paint;
            HDC DeviceContext = BeginPaint(Window, &Paint);
            int X = Paint.rcPaint.left;
            int Y = Paint.rcPaint.top;
            int Width = Paint.rcPaint.right - Paint.rcPaint.left;
            int Height = Paint.rcPaint.bottom - Paint.rcPaint.top;

            RECT ClientRect;
            GetClientRect(Window, &ClientRect);

            Win32UpdateWindow(DeviceContext, &ClientRect, X, Y, Width, Height);
            EndPaint(Window, &Paint);
        } break;

        default:
        {
            Result = DefWindowProc(Window, Message, WParam, LParam);
        } break;
    }

    return(Result);
}

int CALLBACK WinMain(
    HINSTANCE Instance,
    HINSTANCE PrevInstance,
    LPSTR     CmdLine,
    int       ShowCode)
{
    WNDCLASS WindowClass = {};

    WindowClass.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    WindowClass.lpfnWndProc = Win32MainWindowCallback;
    WindowClass.hInstance = Instance;
    // WindowClass.hIcon;
    WindowClass.lpszClassName = "HandmadeHeroWindowClass";

    if (RegisterClass(&WindowClass)) {
        HWND Window = CreateWindowEx(
            0,
            WindowClass.lpszClassName,
            "Handmade Hero",
            WS_OVERLAPPEDWINDOW | WS_VISIBLE,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            0,
            0,
            Instance,
            0);

        if (Window)
        {
            Running = true;
            int XOffset = 0;
            int YOffset = 0;

            while (Running)
            {
                MSG Message;
                while (PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
                {
                    if (Message.message == WM_QUIT)
                    {
                        Running = false;
                    }

                    TranslateMessage(&Message);
                    DispatchMessage(&Message);
                }

                HDC DeviceContext = GetDC(Window);
                RECT ClientRect;
                GetClientRect(Window, &ClientRect);
                int Width = ClientRect.right - ClientRect.left;
                int Height = ClientRect.bottom - ClientRect.top;

                RenderWeirdGradient(XOffset, YOffset);
                Win32UpdateWindow(DeviceContext, &ClientRect, 0, 0, Width, Height);
                ReleaseDC(Window, DeviceContext);

                XOffset++;
                YOffset++;
            }
        } else {
            // TODO logging
        }
    } else {
        // TODO logging
    }

    return(0);
}
