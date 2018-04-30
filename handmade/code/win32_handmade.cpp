#include <stdint.h>
#include <math.h>

#define internal static
#define local_persist static
#define global_variable static
#define CONTROL_SPEED 5

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
#include <stdio.h>

typedef HRESULT WINAPI dsound_create_func(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter);

#include "win32_handmade.h"

global_variable bool GlobalRunning = false;
global_variable win32_offscreen_buffer GlobalBackbuffer;
global_variable LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;
global_variable int64 GlobalPerfCountPerSecond;

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

        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP:
        {
            Assert(!"Key message sent to main window callback!");
        } break;

        default:
        {
            Result = DefWindowProc(Window, Message, WParam, LParam);
        } break;
    }

    return(Result);
}

void Win32InitDSound(HWND Window, int32 SamplesPerSecond, int32 BufferSize)
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
        uint8 *SampleDest = (uint8 *)Region1;
        for (DWORD ByteIndex = 0; ByteIndex < Region1Size; ByteIndex++)
        {
            *SampleDest = 0;
            SampleDest++;
        }

        SampleDest = (uint8 *)Region2;
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

internal void
Win32ProcessKeyboardMessage(game_button_state *NewState, bool IsDown)
{
    Assert(NewState->EndedDown != IsDown);
    NewState->EndedDown = IsDown;
    NewState->HalfTransitionCount++;
}

internal void
Win32ProcessPendingMessages(game_controller_input *KeyboardController)
{
    MSG Message;
    while (PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
    {
        switch (Message.message)
        {
            case WM_QUIT:
            {
                GlobalRunning = false;
            } break;

            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
            case WM_KEYDOWN:
            case WM_KEYUP:
            {
                uint32 VKCode = (uint32)Message.wParam;
                bool WasDown = ((Message.lParam & (1 << 30)) != 0);
                bool IsDown = ((Message.lParam & (1 << 31)) == 0);

                if (WasDown != IsDown)
                {
                    switch (VKCode)
                    {
                        case 'W':
                        case VK_UP:
                        {
                            Win32ProcessKeyboardMessage(&KeyboardController->Up, IsDown);
                        } break;

                        case 'A':
                        case VK_LEFT:
                        {
                            Win32ProcessKeyboardMessage(&KeyboardController->Left, IsDown);
                        } break;

                        case 'S':
                        case VK_DOWN:
                        {
                            Win32ProcessKeyboardMessage(&KeyboardController->Down, IsDown);
                        } break;

                        case 'D':
                        case VK_RIGHT:
                        {
                            Win32ProcessKeyboardMessage(&KeyboardController->Right, IsDown);
                        } break;

                        case 'Q':
                        {
                            Win32ProcessKeyboardMessage(&KeyboardController->LeftShoulder, IsDown);
                        } break;

                        case 'E':
                        {
                            Win32ProcessKeyboardMessage(&KeyboardController->RightShoulder, IsDown);
                        } break;

                        case VK_ESCAPE:
                        {
                            GlobalRunning = false;
                        } break;

                        case VK_SPACE:
                        default:
                        break;
                    }
                }

                bool AltKeyWasDown = (Message.lParam & (1 << 29)) != 0;
                if ((VKCode == VK_F4) && AltKeyWasDown)
                {
                    GlobalRunning = false;
                }
            } break;

            default:
            {
                TranslateMessage(&Message);
                DispatchMessage(&Message);
            } break;
        }
    }
}

inline LARGE_INTEGER
Win32GetPerfCount()
{
    LARGE_INTEGER PerfCount;
    QueryPerformanceCounter(&PerfCount);
    return PerfCount;
}

inline float
Win32GetSecondsElapsed(LARGE_INTEGER Start, LARGE_INTEGER End)
{
    float Result = (float)(End.QuadPart - Start.QuadPart) / (float)GlobalPerfCountPerSecond;
    return Result;
}

int CALLBACK WinMain(
    HINSTANCE Instance,
    HINSTANCE PrevInstance,
    LPSTR     CmdLine,
    int       ShowCode)
{
    LARGE_INTEGER PerfCountPerSecondResult;
    QueryPerformanceFrequency(&PerfCountPerSecondResult);
    GlobalPerfCountPerSecond = PerfCountPerSecondResult.QuadPart;

    Win32ResizeDIBSection(&GlobalBackbuffer, 1066, 600);

    WNDCLASS WindowClass = {};

    WindowClass.style = CS_HREDRAW | CS_VREDRAW;
    WindowClass.lpfnWndProc = Win32MainWindowCallback;
    WindowClass.hInstance = Instance;
    // WindowClass.hIcon;
    WindowClass.lpszClassName = "HandmadeHeroWindowClass";

    // TODO how to query on windows?
    int MonitorRefreshHz = 60;
    int GameUpdateHz = MonitorRefreshHz / 2;
    float TargetSecondsPerFrame = 1.0f / (float)GameUpdateHz;

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

            game_input Input[2] = {};
            game_input *NewInput = &Input[0];
            game_input *OldInput = &Input[1];

            // Sound test
            win32_sound_output SoundOutput = {};
            SoundOutput.SamplesPerSecond = 48000;
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

            LARGE_INTEGER LastPerfCount = Win32GetPerfCount();
            uint64 LastCycleCount = __rdtsc();

            while (GlobalRunning)
            {
                // TODO zeroing macro
                // TODO don't zero everything
                game_controller_input *OldKeyboardController = &OldInput->Controllers[0];
                game_controller_input *NewKeyboardController = &NewInput->Controllers[0];
                *NewKeyboardController = {};

                for (int ButtonIndex = 0;
                    ButtonIndex < ArrayCount(OldKeyboardController->Buttons);
                    ButtonIndex++)
                {
                    // Copy EndedDown state from old controller so will persist from the last frame,
                    // unless windows is about to tell us it changed.
                    NewKeyboardController->Buttons[ButtonIndex].EndedDown =
                        OldKeyboardController->Buttons[ButtonIndex].EndedDown;
                }

                Win32ProcessPendingMessages(NewKeyboardController);

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
                GameUpdateAndRender(&GameGraphicsBuffer, Input, &GameSoundOutput);

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

                LARGE_INTEGER EndPerfCount = Win32GetPerfCount();
                double SecondsElapsed = Win32GetSecondsElapsed(LastPerfCount, EndPerfCount);

                while (SecondsElapsed < TargetSecondsPerFrame)
                {
                    // TODO don't melt CPU
                    EndPerfCount = Win32GetPerfCount();
                    SecondsElapsed = Win32GetSecondsElapsed(LastPerfCount, EndPerfCount);
                }

                uint64 EndCycleCount = __rdtsc();
                int64 ElapsedCycleCount = EndCycleCount - LastCycleCount;
                double MCyclesPerFrame = (double)ElapsedCycleCount / (1000.0f * 1000.0f);

                double FramesPerSecond = 1.0f / SecondsElapsed;

                char Buffer[256];
                sprintf_s(Buffer, sizeof(Buffer),
                    "%.02fms/f,  %.02ff/s,  %.02fmc/f\n",
                    SecondsElapsed * 1000.0f, FramesPerSecond, MCyclesPerFrame);
                OutputDebugStringA(Buffer);

                LastPerfCount = EndPerfCount;
                LastCycleCount = EndCycleCount;

                game_input *Temp = NewInput;
                NewInput = OldInput;
                OldInput = Temp;
            }
        } else {
            // TODO logging
        }
    } else {
        // TODO logging
    }

    return(0);
}
