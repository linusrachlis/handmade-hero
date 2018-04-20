#include "handmade.h"

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

        tSine += (2.0f * Pi32) / (float)SamplesPerCycle;
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

internal void
GameUpdateAndRender(
    game_offscreen_buffer *GraphicsBuffer,
    int XOffset, int YOffset,
    game_sound_output *SoundOutput)
{
    GameFillSoundBuffer(SoundOutput, 256);
    RenderWeirdGradient(GraphicsBuffer, XOffset, YOffset);
}
