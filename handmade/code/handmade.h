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

/*
TODO: ultimately, this proc needs 4 things from the platform layer:
- user input
- graphics buffer to fill
- sound buffer to fill
- timing information

For now it's just handling the graphics part.
*/
internal void
GameUpdateAndRender(game_offscreen_buffer *Buffer, int XOffset, int YOffset);

#define HANDMADE_H
#endif
