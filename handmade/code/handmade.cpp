#include "handmade.h"

/*
TODO
- paddle shouldn't be able to leave the screen
- sound effects
- paddle hit angling
- platform timing (avoid just doing as many FPS as possible)
- bug: paddle hit still counts if puck is past the edge of the paddle, as long as
  the paddle _touches_ the puck before the puck touches the edge of the screen.
  more realistic would be that the puck just bounces off the end of the paddle
  before reaching the screen edge.
- AI?
*/

global_variable int GameWidth;
global_variable int GameHeight;
global_variable bool Victory = false;
global_variable const int PaddleSpeed = 2;
global_variable const int PuckSpeed = 4;

// Pixel structure in register: xx RR GG BB
global_variable const uint32 White = (255 << 16) | (255 << 8) | 255;
global_variable const uint32 Red = (255 << 16);
global_variable const uint32 Black = 0;
global_variable uint8 XOffset = 0;
global_variable uint8 YOffset = 0;

global_variable puck Puck;
global_variable box LeftPaddle;
global_variable box RightPaddle;
global_variable const int NumBoxesToRender = 3;
global_variable box *BoxesToRender[NumBoxesToRender];

global_variable const float GlobalSfxLengthSeconds = 0.2f;
global_variable const int GlobalVictoryToneHz = 800;
global_variable const int GlobalWallToneHz = 40;
global_variable const int GlobalLeftPaddleToneHz = 200;
global_variable const int GlobalRightPaddleToneHz = 400;
global_variable int GlobalSfxRemainingSamples = 0;
global_variable int GlobalSfxCurrentToneHz;

internal void GameSetup(int Width, int Height)
{
    GameWidth = Width;
    GameHeight = Height;

    Puck.Box.Top = 100;
    Puck.Box.Left = 100;
    Puck.Box.Width = 50;
    Puck.Box.Height = 50;
    Puck.Velocity.X = PuckSpeed;
    Puck.Velocity.Y = PuckSpeed;

    LeftPaddle.Top = 0;
    LeftPaddle.Left = 0;
    LeftPaddle.Width = 50;
    LeftPaddle.Height = 150;

    RightPaddle.Top = 0;
    RightPaddle.Width = 50;
    RightPaddle.Height = 150;
    RightPaddle.Left = GameWidth - RightPaddle.Width;

    // Register boxes for the renderer
    BoxesToRender[0] = &Puck.Box;
    BoxesToRender[1] = &LeftPaddle;
    BoxesToRender[2] = &RightPaddle;
}

internal void GameStartSound(game_sound_output *SoundOutput, int ToneHz)
{
    // NOTE: if we're in the middle of another sound, this will simply cancel that and
    // start playing the new sound.
    GlobalSfxRemainingSamples = SoundOutput->SamplesPerSecond * GlobalSfxLengthSeconds;
    GlobalSfxCurrentToneHz = ToneHz;
}

internal void GameFillSoundBuffer(game_sound_output *SoundOutput)
{
    int ToneAmplitude = 3000;
    int SamplesPerCycle;
    int SamplesPerHalfCycle;

    if (GlobalSfxRemainingSamples)
    {
        SamplesPerCycle = SoundOutput->SamplesPerSecond / GlobalSfxCurrentToneHz;
        SamplesPerHalfCycle = SamplesPerCycle / 2;
    }

    int16 *SampleOut = (int16 *)SoundOutput->Buffer;

    int SfxSampleCount;
    int SilenceSampleCount;
    if (GlobalSfxRemainingSamples < SoundOutput->SampleCount)
    {
        SfxSampleCount = GlobalSfxRemainingSamples;
        SilenceSampleCount = SoundOutput->SampleCount - GlobalSfxRemainingSamples;
    }
    else
    {
        SfxSampleCount = SoundOutput->SampleCount;
        SilenceSampleCount = 0;
    }

    // Loop for sfx
    for (int SampleIndex = 0; SampleIndex < SfxSampleCount; SampleIndex++)
    {
        int16 SampleValue;
        SampleValue = ((GlobalSfxRemainingSamples % SamplesPerCycle) > SamplesPerHalfCycle) ? ToneAmplitude : 0;

        *SampleOut++ = SampleValue;
        *SampleOut++ = SampleValue;
        GlobalSfxRemainingSamples--;
    }

    // Loop for silence
    for (int SampleIndex = 0; SampleIndex < SilenceSampleCount; SampleIndex++)
    {
        *SampleOut++ = 0;
        *SampleOut++ = 0;
    }
}

internal void
GameUpdateAndRender(
    game_offscreen_buffer *Buffer,
    game_input LeftInput,
    game_input RightInput,
    game_sound_output *SoundOutput, int ToneHz)
{
    //
    // NOTE: Update game state, unless victory has been achieved.
    //

    if (!Victory)
    {
        if (LeftInput.MovingUp)
        {
            LeftPaddle.Top -= PaddleSpeed;
        }

        if (LeftInput.MovingDown)
        {
            LeftPaddle.Top += PaddleSpeed;
        }

        if (RightInput.MovingUp)
        {
            RightPaddle.Top -= PaddleSpeed;
        }

        if (RightInput.MovingDown)
        {
            RightPaddle.Top += PaddleSpeed;
        }

        // X collision detection is only about paddles, and interacts with
        // victory conditions.
        if (Puck.Velocity.X > 0) {
            // Going right
            if (((RightPaddle.Left - Puck.Box.Right()) < Puck.Velocity.X) &&
                (Puck.Box.Bottom() > RightPaddle.Top) &&
                (Puck.Box.Top < RightPaddle.Bottom()))
            {
                // If the paddle is there to block the puck, bounce
                Puck.Velocity.X *= -1;
                GameStartSound(SoundOutput, GlobalRightPaddleToneHz);
            }
            else if (Puck.Box.Right() >= GameWidth)
            {
                Victory = true;
                GameStartSound(SoundOutput, GlobalVictoryToneHz);
            }
            else
            {
                Puck.Box.Left += Puck.Velocity.X;
            }
        }
        else if (Puck.Velocity.X < 0)
        {
            // Going left
            if (((Puck.Box.Left - LeftPaddle.Right()) < Puck.Velocity.X) &&
                (Puck.Box.Bottom() > LeftPaddle.Top) &&
                (Puck.Box.Top < LeftPaddle.Bottom()))
            {
                // If the paddle is there to block the puck, bounce
                Puck.Velocity.X *= -1;
                GameStartSound(SoundOutput, GlobalLeftPaddleToneHz);
            }
            else if (Puck.Box.Left <= 0)
            {
                Victory = true;
                GameStartSound(SoundOutput, GlobalVictoryToneHz);
            }
            else
            {
                Puck.Box.Left += Puck.Velocity.X;
            }
        }

        // Y collision detection is only about floor and ceiling.
        if (Puck.Velocity.Y > 0) {
            // Going down
            if ((GameHeight - Puck.Box.Bottom()) < Puck.Velocity.Y)
            {
                // Bounce
                Puck.Velocity.Y *= -1;
                GameStartSound(SoundOutput, GlobalWallToneHz);
            }
            else
            {
                Puck.Box.Top += Puck.Velocity.Y;
            }
        }
        else if (Puck.Velocity.Y < 0)
        {
            // Going up
            if (Puck.Box.Top < -Puck.Velocity.Y)
            {
                // Bounce
                Puck.Velocity.Y *= -1;
                GameStartSound(SoundOutput, GlobalWallToneHz);
            }
            else
            {
                Puck.Box.Top += Puck.Velocity.Y;
            }
        }
    }

    // If frame has no sound, fill with silence
    GameFillSoundBuffer(SoundOutput);

    //
    // NOTE: Render
    //

    uint32 BoxColour = Victory ? Black : Red;

    // Note: Row has to be a 1-byte pointer because we use the Pitch to advance it, and
    // Pitch is a number of bytes.
    uint8 *Row = (uint8 *)Buffer->Memory;

    for (int Y = 0; Y < Buffer->Height; Y++)
    {
        uint32 *Pixel = (uint32 *)Row;

        for (int X = 0; X < Buffer->Width; X++)
        {
            bool PixelIsOccupied = false;

            for (int BoxIndex = 0; BoxIndex < NumBoxesToRender; BoxIndex++)
            {
                box *CurrentBox = BoxesToRender[BoxIndex];
                if (X >= CurrentBox->Left && X < CurrentBox->Right() &&
                    Y >= CurrentBox->Top && Y < CurrentBox->Bottom())
                {
                    PixelIsOccupied = true;
                    break;
                }
            }

            if (PixelIsOccupied)
            {
                *Pixel = BoxColour;
            }
            else
            {
                // Cast to 8-bit so they roll over at 256
                uint8 Green = Y + YOffset;
                uint8 Blue = X + XOffset;
                // Pixel structure in register: xx RR GG BB
                *Pixel = (Green << 8) | Blue;
            }

            // Advance to write next pixel
            Pixel++;
        }

        // Advance row pointer by number of bytes per row
        Row += Buffer->Pitch;
    }

    XOffset++;
    YOffset++;
}
