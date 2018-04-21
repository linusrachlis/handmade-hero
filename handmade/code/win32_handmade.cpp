#include <stdint.h>
#include <stdio.h>

#define internal static
#define local_persist static
#define global_variable static

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

#include "handmade.cpp"

#include <windows.h>
#include <dsound.h>

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
global_variable LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;

// TODO: think about whether this is going against the philosophy of game/platform separation.
// (It's all academic anyway, but learning is the point)
global_variable game_input LeftInput = {};
global_variable game_input RightInput = {};

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
                    /*
                    Controls:
                        Left Player: Q = up, A = down
                        Right Player: P = up, L = down
                    */
                    case 'Q':
                    {
                        LeftInput.MovingUp = IsDown;
                    } break;
                    case 'A':
                    {
                        LeftInput.MovingDown = IsDown;
                    } break;
                    case 'P':
                    {
                        RightInput.MovingUp = IsDown;
                    } break;
                    case 'L':
                    {
                        RightInput.MovingDown = IsDown;
                    } break;
                    // case 'Q':
                    // case 'E':
                    // case VK_ESCAPE:
                    // case VK_SPACE:
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

struct win32_sound_output
{
    int SamplesPerSecond;
    int ToneHz;
    int BytesPerSample;
    int SecondaryBufferSize;
    uint32 RunningSampleIndex;
    int LatencySampleCount;
    int LatencyBytes;
};

internal void
Win32ClearSoundBuffer(win32_sound_output *SoundOutput)
{
    VOID *Region1;
    DWORD Region1Size;
    VOID *Region2;
    DWORD Region2Size;

    if (SUCCEEDED(GlobalSecondaryBuffer->Lock(
        0, SoundOutput->SecondaryBufferSize,
        &Region1, &Region1Size,
        &Region2, &Region2Size,
        0)))
    {
        uint8_t *SampleDest = (uint8_t *)Region1;
        for (DWORD ByteIndex = 0; ByteIndex < Region1Size; ByteIndex++)
        {
            *SampleDest = 0;
            SampleDest++;
        }

        SampleDest = (uint8_t *)Region2;
        for (DWORD ByteIndex = 0; ByteIndex < Region2Size; ByteIndex++)
        {
            *SampleDest = 0;
            SampleDest++;
        }

        GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
    }
}

internal void
Win32FillSoundBuffer(
    win32_sound_output *SoundOutput,
    DWORD ByteToLock, DWORD BytesToWrite,
    game_sound_output *GameSoundOutput)
{
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
        DWORD Region1SampleCount = Region1Size / SoundOutput->BytesPerSample;
        int16 *DestSample = (int16 *)Region1;
        int16 *SourceSample = GameSoundOutput->Buffer;
        for (DWORD SampleIndex = 0;
            SampleIndex < Region1SampleCount;
            SampleIndex++)
        {
            *DestSample++ = *SourceSample++;
            *DestSample++ = *SourceSample++;
            SoundOutput->RunningSampleIndex++;
        }

        // Fill in region 2 of buffer
        DWORD Region2SampleCount = Region2Size / SoundOutput->BytesPerSample;
        DestSample = (int16 *)Region2;
        for (DWORD SampleIndex = 0; SampleIndex < Region2SampleCount; SampleIndex++)
        {
            *DestSample++ = *SourceSample++;
            *DestSample++ = *SourceSample++;
            SoundOutput->RunningSampleIndex++;
        }

        GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
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
            "Best Pong Ever",
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

            int XOffset = 0;
            int YOffset = 0;

            // Sound test
            win32_sound_output SoundOutput = {};
            SoundOutput.SamplesPerSecond = 48000;
            SoundOutput.ToneHz = 256;
            SoundOutput.BytesPerSample = sizeof(int16) * 2;
            SoundOutput.SecondaryBufferSize = SoundOutput.BytesPerSample * SoundOutput.SamplesPerSecond; // 1-second buffer
            SoundOutput.RunningSampleIndex = 0;
            SoundOutput.LatencySampleCount = SoundOutput.SamplesPerSecond / 15;
            SoundOutput.LatencyBytes = SoundOutput.LatencySampleCount * SoundOutput.BytesPerSample;

            Win32InitDSound(Window, SoundOutput.SamplesPerSecond, SoundOutput.SecondaryBufferSize);
            Win32ClearSoundBuffer(&SoundOutput);
            GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

            // Create sound buffer for the game to write into that will always be big enough
            int16 *GameSoundBuffer = (int16 *)VirtualAlloc(
                0, SoundOutput.SecondaryBufferSize,
                MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);

            LARGE_INTEGER LastPerfCount;
            QueryPerformanceCounter(&LastPerfCount);
            uint64_t LastCycleCount = __rdtsc();

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

                // Compute what the sound buffer currently requires from the game
                DWORD ByteToLock = 0;
                DWORD BytesToWrite = 0;
                DWORD PlayCursor = 0;
                DWORD TargetCursor = 0;
                bool SoundIsValid = false;
                if (SUCCEEDED(GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, 0)))
                {
                    ByteToLock = (SoundOutput.RunningSampleIndex * SoundOutput.BytesPerSample) %
                        SoundOutput.SecondaryBufferSize;

                    // TODO The first time through, we end up writing into the part of the buffer
                    // that dsound specifically told us not to. We just start at the beginning of
                    // the buffer and fill up to playcursor + latency ("targetcursor").
                    // SUBSEQUENT times are okay, as long as targetcursor is always ahead of bytetolock.
                    // So our latency has to be big enough to ensure this. Currently 1/15th second,
                    // which is double the length of frame even at our lowest target FPS, so that
                    // seems safe.
                    TargetCursor = (PlayCursor + SoundOutput.LatencyBytes) %
                        SoundOutput.SecondaryBufferSize;
                    if (ByteToLock > TargetCursor)
                    {
                        BytesToWrite = SoundOutput.SecondaryBufferSize - ByteToLock;
                        BytesToWrite += TargetCursor;
                    }
                    else
                    {
                        BytesToWrite = TargetCursor - ByteToLock;
                    }

                    SoundIsValid = true;
                }

                // Prepare bucket for game's sound output
                int GameSampleCountToAskFor = BytesToWrite / SoundOutput.BytesPerSample;
                game_sound_output GameSoundOutput = {};
                GameSoundOutput.Buffer = GameSoundBuffer;
                GameSoundOutput.SampleCount = GameSampleCountToAskFor;
                GameSoundOutput.SamplesPerSecond = SoundOutput.SamplesPerSecond;

                // Prepare bucket for game's graphics output
                game_offscreen_buffer GameGraphicsBuffer = {};
                GameGraphicsBuffer.Memory = GlobalBackbuffer.Memory;
                GameGraphicsBuffer.Width = GlobalBackbuffer.Width;
                GameGraphicsBuffer.Height = GlobalBackbuffer.Height;
                GameGraphicsBuffer.BytesPerPixel = GlobalBackbuffer.BytesPerPixel;
                GameGraphicsBuffer.Pitch = GlobalBackbuffer.Pitch;

                // Ask game for output
                GameUpdateAndRender(
                    &GameGraphicsBuffer,
                    LeftInput, RightInput,
                    &GameSoundOutput, SoundOutput.ToneHz);

                if (SoundIsValid)
                {
                    // Copy game's sound output to DSound buffer
                    Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite, &GameSoundOutput);
                }

                HDC DeviceContext = GetDC(Window);
                win32_window_dimension Dimension = Win32GetWindowDimension(Window);
                Win32PaintBufferToWindow(
                    &GlobalBackbuffer, DeviceContext,
                    Dimension.Width, Dimension.Height);
                ReleaseDC(Window, DeviceContext);

                uint64_t EndCycleCount = __rdtsc();
                int ElapsedCycleCount = EndCycleCount - LastCycleCount;

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
