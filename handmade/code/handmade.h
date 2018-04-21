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

typedef int16 GameSoundWaveFunc(int, int);

#define HANDMADE_H
#endif
