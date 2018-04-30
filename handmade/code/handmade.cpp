#include "handmade.h"

global_variable int GlobalXOffset = 0;
global_variable int GlobalYOffset = 0;
global_variable int GlobalToneHz = 256;

internal void
GameFillSoundBuffer(game_sound_output *SoundOutput, int ToneHz)
{
    local_persist float tSine = 0;
    int ToneAmplitude = 3000;
    int SamplesPerCycle = SoundOutput->SamplesPerSecond / ToneHz;

    int16_t *SampleOut = (int16_t *)SoundOutput->Buffer;
    for (int SampleIndex = 0; SampleIndex < SoundOutput->SampleCount; SampleIndex++)
    {
        int16_t SampleValue = (int16_t)(sinf(tSine) * ToneAmplitude);

        *SampleOut = SampleValue;
        SampleOut++;
        *SampleOut = SampleValue;
        SampleOut++;

        float TwoPi = (2.0f * Pi32);
        tSine += TwoPi / (float)SamplesPerCycle;
        if (tSine > TwoPi)
        {
            // Keep tSine within 2Pi, because that's the range that matters, and if it
            // gets too big the pitch starts changing (I think due to floating point error).
            tSine -= TwoPi;
        }
    }
}

internal void
RenderWeirdGradient(game_offscreen_buffer *Buffer, int XOffset, int YOffset)
{
    // Note: Row has to be a 1-byte pointer because we use the Pitch to advance it, and
    // Pitch is a number of bytes.
    uint8_t *Row = (uint8_t *)Buffer->Memory;

    for (int Y = 0; Y < Buffer->Height; Y++)
    {
        uint32_t *Pixel = (uint32_t *)Row;

        for (int X = 0; X < Buffer->Width; X++)
        {
            uint8 Green = (uint8)(Y + YOffset);
            uint8 Blue = (uint8)(X + XOffset);
            // Pixel structure in register: xx RR GG BB
            *Pixel = (Green << 8) | Blue;
            // Advance to write next pixel
            Pixel++;
        }

        // Advance row pointer by number of bytes per row
        Row += Buffer->Pitch;
    }
}

internal void
GameUpdateAndRender(
    game_offscreen_buffer *GraphicsBuffer,
    game_input *Input,
    game_sound_output *SoundOutput)
{
    game_controller_input *Keyboard = &Input->Controllers[0];

    if (Keyboard->Left.EndedDown)
    {
        GlobalXOffset--;
    }
    if (Keyboard->Right.EndedDown)
    {
        GlobalXOffset++;
    }
    if (Keyboard->Up.EndedDown)
    {
        GlobalYOffset--;
        GlobalToneHz++;
    }
    if (Keyboard->Down.EndedDown)
    {
        GlobalYOffset++;
        if (GlobalToneHz > 1)
        {
            GlobalToneHz--;
        }
    }

    GameFillSoundBuffer(SoundOutput, GlobalToneHz);
    RenderWeirdGradient(GraphicsBuffer, GlobalXOffset, GlobalYOffset);
}
