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
    int SampleCount;
    int SamplesPerSecond;
};

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
    int XOffset, int YOffset,
    game_sound_output *SoundOutput);

#define HANDMADE_H
#endif
