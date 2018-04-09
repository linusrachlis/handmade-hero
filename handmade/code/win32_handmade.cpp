#include <windows.h>
#include <stdint.h>
#include <dsound.h>

#define internal static
#define local_persist static
#define global_variable static
#define CONTROL_SPEED 5

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

internal void RenderWeirdGradient(
    win32_offscreen_buffer *Buffer,
    int XOffset, int YOffset)
{
    // Note: Row has to be a 1-byte pointer because we use the Pitch to advance it, and
    // Pitch is a number of bytes.
    uint8_t *Row = (uint8_t *)Buffer->Memory;

    for (int Y = 0; Y < Buffer->Height; Y++)
    {
        uint32_t *Pixel = (uint32_t *)Row;

        for (int X = 0; X < Buffer->Width; X++)
        {
            uint8_t Green = (Y + YOffset);
            uint8_t Blue = (X + XOffset);
            // Pixel structure in register: xx RR GG BB
            *Pixel = (Green << 8) | Blue;
            // Advance to write next pixel
            Pixel++;
        }

        // Advance row pointer by number of bytes per row
        Row += Buffer->Pitch;
    }
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
    Win32ResizeDIBSection(&GlobalBackbuffer, 1066, 600);

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
            int XOffset = 0;
            int YOffset = 0;

            // Sound test
            int SamplesPerSecond = 48000;
            int ToneHz = 504;
            int16_t ToneAmplitude = 3000; // Max is ~32K (2^16), but we don't want to go deaf
            int BytesPerSample = sizeof(int16_t) * 2;
            int SecondaryBufferSize = BytesPerSample * SamplesPerSecond; // 1-second buffer
            int SamplesPerCycle = SamplesPerSecond / ToneHz;
            int SamplesPerHalfCycle = SamplesPerCycle / 2;
         
            Win32InitDSound(Window, SamplesPerSecond, SecondaryBufferSize);
            uint32_t RunningSampleIndex = 0;
            bool SoundIsPlaying = false;

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

                HDC DeviceContext = GetDC(Window);
                win32_window_dimension Dimension = Win32GetWindowDimension(Window);
                RenderWeirdGradient(&GlobalBackbuffer, XOffset, YOffset);

                DWORD PlayCursor;
                if (SUCCEEDED(GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, 0)))
                {
                    DWORD ByteToLock = (RunningSampleIndex * BytesPerSample) % SecondaryBufferSize;
                    DWORD BytesToWrite;
                    if (ByteToLock == PlayCursor)
                    {
                        // NOTE: Is this what happens the very first time?
                        BytesToWrite = SecondaryBufferSize;
                    }
                    else if (ByteToLock > PlayCursor)
                    {
                        BytesToWrite = SecondaryBufferSize - ByteToLock;
                        BytesToWrite += PlayCursor;
                    }
                    else
                    {
                        BytesToWrite = PlayCursor - ByteToLock;
                    }

                    VOID *Region1;
                    DWORD Region1Size;
                    VOID *Region2;
                    DWORD Region2Size;

                    if (SUCCEEDED(GlobalSecondaryBuffer->Lock(
                        ByteToLock, BytesToWrite,
                        &Region1, &Region1Size,
                        &Region2, &Region2Size,
                        0)))
                    {
                        // TODO: assert valid size for regions

                        // Fill in region 1
                        DWORD Region1SampleCount = Region1Size / BytesPerSample;
                        int16_t *SampleOut = (int16_t *)Region1;
                        for (DWORD SampleIndex = 0; SampleIndex < Region1SampleCount; SampleIndex++)
                        {
                            bool WaveIsUp = (RunningSampleIndex / SamplesPerHalfCycle) % 2;
                            int16_t SampleValue = WaveIsUp ? ToneAmplitude : -ToneAmplitude;
                            *SampleOut = SampleValue;
                            SampleOut++;
                            *SampleOut = SampleValue;
                            SampleOut++;
                            RunningSampleIndex++;
                        }
                        // Fill in region 2 of buffer
                        DWORD Region2SampleCount = Region2Size / BytesPerSample;
                        SampleOut = (int16_t *)Region2;
                        for (DWORD SampleIndex = 0; SampleIndex < Region2SampleCount; SampleIndex++)
                        {
                            bool WaveIsUp = (RunningSampleIndex / SamplesPerHalfCycle) % 2;
                            int16_t SampleValue = WaveIsUp ? ToneAmplitude : -ToneAmplitude;
                            *SampleOut = SampleValue;
                            SampleOut++;
                            *SampleOut = SampleValue;
                            SampleOut++;
                            RunningSampleIndex++;
                        }

                        GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
                    }
                    else
                    {
                        OutputDebugStringA("Failed to lock buffer\n");
                    }
                }

                if (!SoundIsPlaying)
                {
                    if (!SUCCEEDED(GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING)))
                    {
                        OutputDebugStringA("Failed to start playing\n");
                    }
                    SoundIsPlaying = true;
                }

                Win32PaintBufferToWindow(
                    &GlobalBackbuffer, DeviceContext,
                    Dimension.Width, Dimension.Height);
                ReleaseDC(Window, DeviceContext);

                XOffset++;
                YOffset++;

                if (GoingUp)
                {
                    YOffset -= CONTROL_SPEED;
                }
                if (GoingDown)
                {
                    YOffset += CONTROL_SPEED;
                }
                if (GoingLeft)
                {
                    XOffset -= CONTROL_SPEED;
                }
                if (GoingRight)
                {
                    XOffset += CONTROL_SPEED;
                }
            }
        } else {
            // TODO logging
        }
    } else {
        // TODO logging
    }

    return(0);
}
