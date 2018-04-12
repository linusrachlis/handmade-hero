struct game_offscreen_buffer
{
    void *Memory;
    int Width;
    int Height;
    int BytesPerPixel;
    int Pitch;
};

struct velocity
{
    int X;
    int Y;
};

struct box
{
    int Top;
    int Left;
    int Width;
    int Height;
    velocity Velocity;

    int Bottom()
    {
        return Top + Height;
    }

    int Right()
    {
        return Left + Width;
    }
};

// Pixel structure in register: xx RR GG BB
global_variable const uint32_t White = (255 << 16) | (255 << 8) | 255;
global_variable const uint32_t Black = 0;

global_variable box Box = {};

void GameSetup()
{
    Box.Top = 100;
    Box.Left = 100;
    Box.Width = 50;
    Box.Height = 50;
    Box.Velocity.X = 1;
    Box.Velocity.Y = 1;
}

internal void
GameUpdateAndRender(game_offscreen_buffer *Buffer)
{
    //
    // NOTE: Update
    //

    if (Box.Velocity.X > 0) {
        // Going right
        if ((Buffer->Width - Box.Right()) < Box.Velocity.X)
        {
            // Bounce
            Box.Velocity.X *= -1;
        }
        else
        {
            Box.Left += Box.Velocity.X;
        }
    }
    else if (Box.Velocity.X < 0)
    {
        // Going left
        if (Box.Left < -Box.Velocity.X)
        {
            // Bounce
            Box.Velocity.X *= -1;
        }
        else
        {
            Box.Left += Box.Velocity.X;
        }
    }

    if (Box.Velocity.Y > 0) {
        // Going down
        if ((Buffer->Height - Box.Bottom()) < Box.Velocity.Y)
        {
            // Bounce
            Box.Velocity.Y *= -1;
        }
        else
        {
            Box.Top += Box.Velocity.Y;
        }
    }
    else if (Box.Velocity.Y < 0)
    {
        // Going up
        if (Box.Top < -Box.Velocity.Y)
        {
            // Bounce
            Box.Velocity.Y *= -1;
        }
        else
        {
            Box.Top += Box.Velocity.Y;
        }
    }

    //
    // NOTE: Render
    //

    // Note: Row has to be a 1-byte pointer because we use the Pitch to advance it, and
    // Pitch is a number of bytes.
    uint8_t *Row = (uint8_t *)Buffer->Memory;

    for (int Y = 0; Y < Buffer->Height; Y++)
    {
        uint32_t *Pixel = (uint32_t *)Row;

        for (int X = 0; X < Buffer->Width; X++)
        {
            if (X >= Box.Left && X < Box.Right() &&
                Y >= Box.Top && Y < Box.Bottom())
            {
                // Within the box
                *Pixel = White;
            }
            else
            {
                *Pixel = Black;
            }

            // Advance to write next pixel
            Pixel++;
        }

        // Advance row pointer by number of bytes per row
        Row += Buffer->Pitch;
    }
}
