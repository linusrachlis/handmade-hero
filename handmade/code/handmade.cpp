struct game_offscreen_buffer
{
    void *Memory;
    int Width;
    int Height;
    int BytesPerPixel;
    int Pitch;
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

global_variable int GameWidth;
global_variable int GameHeight;

// Pixel structure in register: xx RR GG BB
global_variable const uint32_t White = (255 << 16) | (255 << 8) | 255;
global_variable const uint32_t Black = 0;

global_variable puck Puck;
global_variable box LeftPaddle;
global_variable box RightPaddle;
global_variable const int NumBoxesToRender = 3;
global_variable box *BoxesToRender[NumBoxesToRender];

internal void GameSetup(int Width, int Height)
{
    GameWidth = Width;
    GameHeight = Height;

    // TODO: register boxes to be rendered

    Puck.Box.Top = 100;
    Puck.Box.Left = 100;
    Puck.Box.Width = 50;
    Puck.Box.Height = 50;
    Puck.Velocity.X = 3;
    Puck.Velocity.Y = 3;

    LeftPaddle.Top = 0;
    LeftPaddle.Left = 0;
    LeftPaddle.Width = 50;
    LeftPaddle.Height = 200;

    RightPaddle.Top = 0;
    RightPaddle.Width = 50;
    RightPaddle.Height = 200;
    RightPaddle.Left = GameWidth - RightPaddle.Width;

    BoxesToRender[0] = &Puck.Box;
    BoxesToRender[1] = &LeftPaddle;
    BoxesToRender[2] = &RightPaddle;
}

internal void
GameUpdateAndRender(game_offscreen_buffer *Buffer)
{
    //
    // NOTE: Update
    //

    if (Puck.Velocity.X > 0) {
        // Going right
        if ((GameWidth - Puck.Box.Right()) < Puck.Velocity.X)
        {
            // Bounce
            Puck.Velocity.X *= -1;
        }
        else
        {
            Puck.Box.Left += Puck.Velocity.X;
        }
    }
    else if (Puck.Velocity.X < 0)
    {
        // Going left
        if (Puck.Box.Left < -Puck.Velocity.X)
        {
            // Bounce
            Puck.Velocity.X *= -1;
        }
        else
        {
            Puck.Box.Left += Puck.Velocity.X;
        }
    }

    if (Puck.Velocity.Y > 0) {
        // Going down
        if ((GameHeight - Puck.Box.Bottom()) < Puck.Velocity.Y)
        {
            // Bounce
            Puck.Velocity.Y *= -1;
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
        }
        else
        {
            Puck.Box.Top += Puck.Velocity.Y;
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

            *Pixel = PixelIsOccupied ? White : Black;

            // Advance to write next pixel
            Pixel++;
        }

        // Advance row pointer by number of bytes per row
        Row += Buffer->Pitch;
    }
}
