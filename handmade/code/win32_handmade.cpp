#include <stdint.h>

#define internal static
#define local_persist static
#define global_variable static

#include "handmade.cpp"

#include <windows.h>
#include <dsound.h>
#include <math.h>
#include <stdio.h>

typedef HRESULT WINAPI dsound_create_func(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter);

struct win32_offscreen_buffer
{
    BITMAPINFO Info;
    void *Memory;
    int Width;
    int Height;
    int BytesPerPixel;
    int Pitch;
};

struct win32_window_dimension
{
    int Width;
    int Height;
};

global_variable bool GlobalRunning = false;
global_variable win32_offscreen_buffer GlobalBackbuffer;
global_variable bool GoingUp = false;
global_variable bool GoingLeft = false;
global_variable bool GoingDown = false;
global_variable bool GoingRight = false;
global_variable LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;

internal win32_window_dimension Win32GetWindowDimension(HWND Window)
{
    win32_window_dimension Result;

    RECT ClientRect;
    GetClientRect(Window, &ClientRect);
    Result.Width = ClientRect.right - ClientRect.left;
    Result.Height = ClientRect.bottom - ClientRect.top;

    return(Result);
}

internal void Win32ResizeDIBSection(win32_offscreen_buffer *Buffer, int Width, int Height)
{
    if (Buffer->Memory)
    {
        VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
    }

    Buffer->Width = Width;
    Buffer->Height = Height;
    Buffer->BytesPerPixel = 4;

    Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
    Buffer->Info.bmiHeader.biWidth = Buffer->Width;
    Buffer->Info.bmiHeader.biHeight = -Buffer->Height;
    Buffer->Info.bmiHeader.biPlanes = 1;
    Buffer->Info.bmiHeader.biBitCount = 32;
    Buffer->Info.bmiHeader.biCompression = BI_RGB;

    int BitmapMemorySize = Buffer->Width * Buffer->Height * Buffer->BytesPerPixel;
    Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
    Buffer->Pitch = Buffer->Width * Buffer->BytesPerPixel;
}

internal void Win32PaintBufferToWindow(
    win32_offscreen_buffer *Buffer, HDC DeviceContext,
    int WindowWidth, int WindowHeight)
{
    StretchDIBits(DeviceContext,
        0, 0, WindowWidth, WindowHeight,
        0, 0, Buffer->Width, Buffer->Height,
        Buffer->Memory,
        &Buffer->Info,
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
        case WM_CLOSE:
        {
            OutputDebugStringA("WM_CLOSE\n");
            GlobalRunning = false;
        } break;

        case WM_ACTIVATEAPP:
        {
            OutputDebugStringA("WM_ACTIVATEAPP\n");
        } break;

        case WM_DESTROY:
        {
            OutputDebugStringA("WM_DESTROY\n");
            GlobalRunning = false;
        } break;

        case WM_PAINT:
        {
            PAINTSTRUCT Paint;
            HDC DeviceContext = BeginPaint(Window, &Paint);
            win32_window_dimension Dimension = Win32GetWindowDimension(Window);
            Win32PaintBufferToWindow(
                &GlobalBackbuffer, DeviceContext, Dimension.Width, Dimension.Height);
            EndPaint(Window, &Paint);
        } break;

        // TODO Capturing SYSKEY* messages and then not calling DefWindowProc has the effect
        // of disabling shortcuts like Alt-F4 to close the window. Maybe this is what we want?
        // For now, just following Casey.
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP:
        {
            uint32_t VKCode = WParam;
            bool WasDown = ((LParam & (1 << 30)) != 0);
            bool IsDown = ((LParam & (1 << 31)) == 0);

            if (WasDown != IsDown)
            {
                switch (VKCode)
                {
                    case 'W':
                    case VK_UP:
                    {
                        GoingUp = IsDown;
                    } break;
                    case 'A':
                    case VK_LEFT:
                    {
                        GoingLeft = IsDown;
                    } break;
                    case 'S':
                    case VK_DOWN:
                    {
                        GoingDown = IsDown;
                    } break;
                    case 'D':
                    case VK_RIGHT:
                    {
                        GoingRight = IsDown;
                    } break;
                    case 'Q':
                    case 'E':
                    case VK_ESCAPE:
                    case VK_SPACE:
                    default:
                        break;
                }
            }

            bool AltKeyWasDown = (LParam & (1 << 29)) != 0;
            if ((VKCode == VK_F4) && AltKeyWasDown)
            {
                GlobalRunning = false;
            }
        } break;

        default:
        {
            Result = DefWindowProc(Window, Message, WParam, LParam);
        } break;
    }

    return(Result);
}

void Win32InitDSound(HWND Window, int32_t SamplesPerSecond, int32_t BufferSize)
{
    HMODULE DSoundLibrary = LoadLibrary("dsound.dll");

    if (DSoundLibrary)
    {
        dsound_create_func *DirectSoundCreate = (dsound_create_func *)GetProcAddress(DSoundLibrary, "DirectSoundCreate");

        LPDIRECTSOUND DirectSound;
        if (DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0)))
        {
            WAVEFORMATEX WaveFormat = {};
            WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
            WaveFormat.nChannels = 2;
            WaveFormat.nSamplesPerSec = SamplesPerSecond;
            WaveFormat.wBitsPerSample = 16;
            WaveFormat.nBlockAlign = (WaveFormat.wBitsPerSample * WaveFormat.nChannels) / 8;
            WaveFormat.nAvgBytesPerSec = SamplesPerSecond * WaveFormat.nBlockAlign;
            WaveFormat.cbSize = 0;

            if (SUCCEEDED(DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY)))
            {
                DSBUFFERDESC BufferDescription = {};
                BufferDescription.dwSize = sizeof(BufferDescription);
                BufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;

                // NOTE: "Create" a primary buffer
                LPDIRECTSOUNDBUFFER PrimaryBuffer;
                if (SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &PrimaryBuffer, 0)))
                {
                    HRESULT Error = PrimaryBuffer->SetFormat(&WaveFormat);
                    if (SUCCEEDED(Error))
                    {
                        // NOTE: We have finally set the format!
                        OutputDebugStringA("Format was set on primary sound buffer!\n");
                    }
                    else
                    {
                        // TODO: diagnostic
                    }
                }
            }
            else
            {
                // TODO: diagnostic
            }

            DSBUFFERDESC BufferDescription = {};
            BufferDescription.dwSize = sizeof(BufferDescription);
            BufferDescription.dwFlags = 0;
            BufferDescription.dwBufferBytes = BufferSize;
            BufferDescription.lpwfxFormat = &WaveFormat;

            // NOTE: Create secondary buffer
            HRESULT Error = DirectSound->CreateSoundBuffer(&BufferDescription, &GlobalSecondaryBuffer, 0);
            if (SUCCEEDED(Error))
            {
                OutputDebugStringA("Secondary sound buffer created!\n");
            }
            else
            {
                // TODO: diagnostic
            }
        }
    }
    else
    {
        // TODO: diagnostic
    }
}

int CALLBACK WinMain(
    HINSTANCE Instance,
    HINSTANCE PrevInstance,
    LPSTR     CmdLine,
    int       ShowCode)
{
    // LARGE_INTEGER PerfCountPerSecondResult;
    // QueryPerformanceFrequency(&PerfCountPerSecondResult);
    // int64_t PerfCountPerSecond = PerfCountPerSecondResult.QuadPart;

    const int RenderWidth = 1066;
    const int RenderHeight = 600;
    Win32ResizeDIBSection(&GlobalBackbuffer, RenderWidth, RenderHeight);

    WNDCLASS WindowClass = {};

    WindowClass.style = CS_HREDRAW | CS_VREDRAW;
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
            GlobalRunning = true;

            GameSetup(RenderWidth, RenderHeight);

            // Win32InitDSound(Window, SoundOutput.SamplesPerSecond, SoundOutput.SecondaryBufferSize);
            // Win32FillSoundBuffer(&SoundOutput, 0, SoundOutput.LatencyBytes);
            // GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

            // LARGE_INTEGER LastPerfCount;
            // QueryPerformanceCounter(&LastPerfCount);
            // uint64_t LastCycleCount = __rdtsc();

            while (GlobalRunning)
            {
                MSG Message;
                while (PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
                {
                    if (Message.message == WM_QUIT)
                    {
                        GlobalRunning = false;
                    }

                    TranslateMessage(&Message);
                    DispatchMessage(&Message);
                }

                // Call platform-independent code to render weird gradient now
                // (or whatever it wants to render!)
                // TODO: check vars set by key input events and pass data to game layer.
                game_offscreen_buffer GameBuffer = {};
                GameBuffer.Memory = GlobalBackbuffer.Memory;
                GameBuffer.Width = GlobalBackbuffer.Width;
                GameBuffer.Height = GlobalBackbuffer.Height;
                GameBuffer.BytesPerPixel = GlobalBackbuffer.BytesPerPixel;
                GameBuffer.Pitch = GlobalBackbuffer.Pitch;
                GameUpdateAndRender(&GameBuffer);

                // DWORD PlayCursor;
                // if (SUCCEEDED(GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, 0)))
                // {
                //     DWORD ByteToLock = (SoundOutput.RunningSampleIndex * SoundOutput.BytesPerSample) % SoundOutput.SecondaryBufferSize;
                //     DWORD BytesToWrite;
                //     DWORD TargetCursor = (PlayCursor + SoundOutput.LatencyBytes) % SoundOutput.SecondaryBufferSize;

                //     if (ByteToLock > TargetCursor)
                //     {
                //         BytesToWrite = SoundOutput.SecondaryBufferSize - ByteToLock;
                //         BytesToWrite += TargetCursor;
                //     }
                //     else
                //     {
                //         BytesToWrite = TargetCursor - ByteToLock;
                //     }

                //     Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite);
                // }

                HDC DeviceContext = GetDC(Window);
                win32_window_dimension Dimension = Win32GetWindowDimension(Window);
                Win32PaintBufferToWindow(
                    &GlobalBackbuffer, DeviceContext,
                    Dimension.Width, Dimension.Height);
                ReleaseDC(Window, DeviceContext);

                // uint64_t EndCycleCount = __rdtsc();
                // int ElapsedCycleCount = EndCycleCount - LastCycleCount;

                // LARGE_INTEGER EndPerfCount;
                // QueryPerformanceCounter(&EndPerfCount);
                // int64_t ElapsedPerfCount = EndPerfCount.QuadPart - LastPerfCount.QuadPart;
                // int32_t ElapsedMilliseconds = (1000*ElapsedPerfCount) / PerfCountPerSecond;
                // int FramesPerSecond = PerfCountPerSecond / ElapsedPerfCount;
                // uint64_t CyclesPerSecond = ElapsedCycleCount * FramesPerSecond;

                // char Buffer[256];
                // sprintf(Buffer, "Elapsed MS: %d, FPS = %d\n", ElapsedMilliseconds, FramesPerSecond);
                // OutputDebugStringA(Buffer);
                // sprintf(Buffer, "Elapsed cycles: %d, per sec = %I64u\n", ElapsedCycleCount, CyclesPerSecond);
                // OutputDebugStringA(Buffer);

                // LastPerfCount = EndPerfCount;
                // LastCycleCount = EndCycleCount;
            }
        } else {
            // TODO logging
        }
    } else {
        // TODO logging
    }

    return(0);
}
