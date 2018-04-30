#if !defined(HANDMADE_H)

#if HANDMADE_SLOW
#define Assert(Expression) if (!(Expression)) {*(int *)0 = 0;}
#else
#define Assert(Expression)
#endif

#define Kilobytes(Number) ((Number) * 1024)
#define Megabytes(Number) (Kilobytes(Number) * 1024)
#define Gigabytes(Number) (Megabytes(Number) * 1024)
#define Terabytes(Number) (Gigabytes(Number) * 1024)

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

#define Pi32 3.1415926535897932384626433832795f

/*
TODO: services the platform layer provides to the game
*/

struct game_button_state
{
    int HalfTransitionCount;
    bool EndedDown;
};

struct game_controller_input
{
    bool IsAnalog;

    float StartX;
    float StartY;

    float MinX;
    float MinY;

    float MaxX;
    float MaxY;

    float EndX;
    float EndY;

    union
    {
        game_button_state Buttons[6];
        struct
        {
            game_button_state Up;
            game_button_state Down;
            game_button_state Left;
            game_button_state Right;
            game_button_state LeftShoulder;
            game_button_state RightShoulder;
        };
    };
};

struct game_input
{
    // TODO(casey): Insert clock values here.
    // TODO game pads -- I don't have any so I didn't write the code for them
    game_controller_input Controllers[5];
};

struct game_memory
{
    bool IsInitialized;

    uint64 PermanentStorageSize;
    void *PermanentStorage; // NOTE(casey): REQUIRED to be cleared to zero at startup

    uint64 TransientStorageSize;
    void *TransientStorage; // NOTE(casey): REQUIRED to be cleared to zero at startup
};

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

#define HANDMADE_H
#endif
