#if !defined(HANDMADE_H)

/*
TODO: services the platform layer provides to the game
*/

/*
NOTE: services the game provides to the platform layer
*/

struct game_offscreen_buffer
{
    void *Memory;
    int Width;
    int Height;
    int BytesPerPixel;
    int Pitch;
};

struct game_sound_output
{
    int16_t *Buffer;
    int SampleCount; // this is the platform asking for X samples
    int SamplesPerSecond;
};

struct game_input
{
    bool MovingUp;
    bool MovingDown;
};

struct box
{
    int Top;
    int Left;
    int Width;
    int Height;

    int Bottom()
    {
        return Top + Height;
    }

    int Right()
    {
        return Left + Width;
    }
};

struct velocity
{
    int X;
    int Y;
};

struct puck
{
    box Box;
    velocity Velocity;
};

internal void GameSetup(int Width, int Height);

internal void
GameFillSoundBuffer(game_sound_output *SoundOutput, int ToneHz);

/*
TODO: ultimately, this proc needs 4 things from the platform layer:
- user input
- graphics buffer to fill
- sound buffer to fill
- timing information
*/
internal void
GameUpdateAndRender(
    game_offscreen_buffer *Buffer,
    game_input LeftInput,
    game_input RightInput,
    game_sound_output *SoundOutput, int ToneHz);

#define HANDMADE_H
#endif
