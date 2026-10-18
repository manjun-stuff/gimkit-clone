#include <SDL2/SDL.h>
#include <emscripten.h>
#include <iostream>
#include <algorithm>

uint64_t lastTick = 0;

enum GameState
{
    HOME_SCREEN,
    PLAYING,
    QUESTIONS,
    SHOP
};

struct AppData
{
    SDL_Window *window = nullptr;
    SDL_Renderer *renderer = nullptr;
    GameState currentGameState = PLAYING;
    float camX = 0;
    float camY = 0;
    int playerMoney = 0;
    float playerX = 0;
    float playerY = 0;
    float velocityX = 0;
    float velocityY = 0;
    float playerWidth = 0.05f;
    float playerHeight = 0.1f;
    float playerSpeed = 5;
    bool running = true;
};

struct Vector2
{
    float x;
    float y;
};

bool aabb_circle(Vector2 pos, Vector2 size, Vector2 circleCenter, float circleRadius)
{
    // localize coords so boxcenter is at 0, 0
    Vector2 localCircle = {circleCenter.x - pos.x, circleCenter.y - pos.y};
    // rotate the circle coords so the box is axis aligned
    Vector2 clampedCircle = {std::clamp(localCircle.x, -size.x / 2, size.x / 2), std::clamp(localCircle.y, -size.y / 2, size.y / 2)};

    if (std::hypot(clampedCircle.x - localCircle.x, clampedCircle.y - localCircle.y) < circleRadius)
    {
        return true;
    }
    return false;
}

bool bounding_box(Vector2 a1, Vector2 a2, Vector2 a3, Vector2 a4, Vector2 b1, Vector2 b2, Vector2 b3, Vector2 b4)
{
    // obb collision detection, can't do simple AABB because the boxes are rotated, also can't simply check corners because of rotation and collisions that dont involve corners, so i have to result to some complex math.
    return false;
}

void draw_box(Vector2 uv, Vector2 boxSize, AppData *app, int r, int g, int b)
{
    int width, height;
    SDL_GetWindowSizeInPixels(app->window, &width, &height);
    int renderWidth = std::round(boxSize.x * width);
    int renderHeight = std::round(boxSize.y * height);
    int renderX = std::round(uv.x * width) - (renderWidth / 2);
    int renderY = std::round(uv.y * height) - (renderHeight / 2);

    SDL_Rect box = {renderX, renderY, renderWidth, renderHeight};

    SDL_SetRenderDrawColor(app->renderer, r, g, b, 255);
    SDL_RenderFillRect(app->renderer, &box);
}

void draw_box_world(Vector2 world, Vector2 boxSize, AppData *app, int r, int g, int b)
{
    // subtract cam coords so we can get the camera's viewpoint
    float localX = world.x - app->camX;
    float localY = world.y - app->camY;

    // acquire aspect ratio
    int width, height;
    SDL_GetWindowSizeInPixels(app->window, &width, &height);
    // float aspect = static_cast<float>(width) / height;

    Vector2 pixel;

    // fix the x coords
    pixel.x = localX * height;

    // modify the local y position so it expands to screen
    pixel.y = localY * height;

    Vector2 uv = {pixel.x / width + 0.5f, pixel.y / height + 0.5f};

    // modify box size x to fit
    boxSize.x = boxSize.x * static_cast<float>(height) / width;

    // render
    draw_box(uv, boxSize, app, r, g, b);
}

void render_frame(AppData *app)
{
    SDL_SetRenderDrawColor(app->renderer, 0, 0, 0, 255);
    SDL_RenderClear(app->renderer);

    draw_box_world(Vector2{0.0f, 0.0f}, Vector2{1.0f, 1.0f}, app, 255, 0, 0);

    SDL_RenderPresent(app->renderer);
}

// executes functions for game every frame
void tick(void *arg)
{
    uint64_t currentTick = SDL_GetTicks64();
    uint64_t deltaTime = currentTick - lastTick;
    lastTick = currentTick;

    // get the data for our application
    AppData *app = static_cast<AppData *>(arg);
    // check our events
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        if (event.type == SDL_QUIT)
        {
            app->running = false;
            emscripten_cancel_main_loop();
            return;
        }
    }

    render_frame(app);
}

int main()
{
    SDL_Init(SDL_INIT_VIDEO);
    AppData app;
    // creates the game window and renderer
    app.window = SDL_CreateWindow("Gamkat", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    app.renderer = SDL_CreateRenderer(app.window, -1, SDL_RENDERER_ACCELERATED);

    emscripten_set_main_loop_arg(tick, &app, 0, true);

    // cleans up data to prevent data leaks
    SDL_DestroyWindow(app.window);
    SDL_DestroyRenderer(app.renderer);
    SDL_Quit();

    return 0;
}
