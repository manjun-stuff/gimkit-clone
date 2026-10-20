#include <SDL2/SDL.h>
#include <emscripten.h>
#include <algorithm>
#include <vector>

uint64_t lastTick = 0;

enum GameState
{
    HOME_SCREEN,
    PLAYING,
    QUESTIONS,
    SHOP
};

struct Box
{
    float x;
    float y;
    float width;
    float height;
};

struct AppData
{
    SDL_Window *window = nullptr;
    SDL_Renderer *renderer = nullptr;
    GameState currentGameState = PLAYING;
    std::vector<Box> platforms = {{0, 0.5f, 1, 0.1f}};
    float gravity = 2.5f;
    float camX = 0;
    float camY = 0;
    int playerMoney = 0;
    float playerX = 0;
    float playerY = 0;
    float velocityX = 0;
    float velocityY = 0;
    float playerWidth = 0.1f;
    float playerHeight = 0.2f;
    float playerSpeed = 7.5f;
    float playerJumpHeight = 1.5f;
    float maxRunSpeed = 7.5f;
    bool running = true;
    bool onGround = false;
};

class World
{
public:
    std::vector<Box> platforms;

    void Load(AppData *app)
    {
        app->platforms = platforms;
    }
};

struct Vector2
{
    float x;
    float y;
};

bool aabb_circle(Box box, Vector2 circleCenter, float circleRadius)
{
    // localize coords so boxcenter is at 0, 0
    Vector2 localCircle = {circleCenter.x - box.x, circleCenter.y - box.y};
    // rotate the circle coords so the box is axis aligned
    Vector2 clampedCircle = {std::clamp(localCircle.x, -box.width / 2, box.width / 2), std::clamp(localCircle.y, -box.height / 2, box.height / 2)};

    if (std::hypot(clampedCircle.x - localCircle.x, clampedCircle.y - localCircle.y) < circleRadius)
    {
        return true;
    }
    return false;
}

bool bounding_box(Box box1, Box box2)
{
    // aabb collision detection
    float xMin = box1.width / 2 + box2.width / 2;
    float yMin = box1.height / 2 + box2.height / 2;
    if (std::abs(box1.x - box2.x) < xMin && std::abs(box1.y - box2.y) < yMin)
    {
        return true;
    }
    return false;
}

Vector2 bounding_box_overlap(Box box1, Box box2)
{
    Vector2 overlap;
    bool right = box2.x > box1.x;
    bool below = box2.y > box1.y;
    float xMin1 = box1.x - box1.width / 2;
    float xMax1 = box1.x + box1.width / 2;
    float xMin2 = box2.x - box2.width / 2;
    float xMax2 = box2.x + box2.width / 2;
    float yMin1 = box1.y - box1.height / 2;
    float yMax1 = box1.y + box1.height / 2;
    float yMin2 = box2.y - box2.height / 2;
    float yMax2 = box2.y + box2.height / 2;
    if (right)
    {
        overlap.x = xMin2 - xMax1;
    }
    else
    {
        overlap.x = xMax2 - xMin1;
    }
    if (below)
    {
        overlap.y = yMin2 - yMax1;
    }
    else
    {
        overlap.y = yMax2 - yMin1;
    }
    return overlap;
}

void draw_box(Box box, AppData *app, int r, int g, int b)
{
    int width, height;
    SDL_GetWindowSizeInPixels(app->window, &width, &height);
    int renderWidth = std::round(box.width * width);
    int renderHeight = std::round(box.height * height);
    int renderX = std::round(box.x * width) - (renderWidth / 2);
    int renderY = std::round(box.y * height) - (renderHeight / 2);

    SDL_Rect rect = {renderX, renderY, renderWidth, renderHeight};

    SDL_SetRenderDrawColor(app->renderer, r, g, b, 255);
    SDL_RenderFillRect(app->renderer, &rect);
}

void draw_box_world(Box box, AppData *app, int r, int g, int b)
{
    // subtract cam coords so we can get the camera's viewpoint
    float localX = box.x - app->camX;
    float localY = box.y - app->camY;

    // acquire aspect ratio
    int width, height;
    SDL_GetWindowSizeInPixels(app->window, &width, &height);
    // float aspect = static_cast<float>(width) / height;

    Vector2 pixel;

    // fix the x coords
    pixel.x = localX * height;

    // modify the local y position so it expands to screen
    pixel.y = localY * height;

    box.x = pixel.x / width + 0.5f;
    box.y = pixel.y / height + 0.5f;

    // modify box size x to fit
    box.width = box.width * static_cast<float>(height) / width;

    // render
    draw_box(box, app, r, g, b);
}

void render_frame(AppData *app)
{
    SDL_SetRenderDrawColor(app->renderer, 0, 0, 0, 255);
    SDL_RenderClear(app->renderer);

    // render player
    draw_box_world(Box{app->playerX, app->playerY, app->playerWidth, app->playerHeight}, app, 255, 0, 0);

    // render platform
    for (Box platform : app->platforms)
        draw_box_world(platform, app, 255, 255, 255);

    SDL_RenderPresent(app->renderer);
}

void run_physics(AppData *app, float deltaTime)
{
    app->onGround = false;

    // Apply environmental gravity once per frame
    app->velocityY += app->gravity * deltaTime;

    // --- 1. RESOLVE HORIZONTAL AXIS ---
    app->playerX += app->velocityX * deltaTime; // Move once

    // Check horizontal collisions against ALL platforms defensively
    for (const Box &platform : app->platforms)
    {
        Box playerBox = {app->playerX, app->playerY, app->playerWidth, app->playerHeight};
        if (bounding_box(playerBox, platform))
        {
            Vector2 overlap = bounding_box_overlap(playerBox, platform);
            app->playerX += overlap.x;
            app->velocityX = 0;
        }
    }

    // --- 2. RESOLVE VERTICAL AXIS ---
    app->playerY += app->velocityY * deltaTime; // Move once

    // Check vertical collisions against ALL platforms defensively
    for (const Box &platform : app->platforms)
    {
        Box playerBox = {app->playerX, app->playerY, app->playerWidth, app->playerHeight};
        if (bounding_box(playerBox, platform))
        {
            Vector2 overlap = bounding_box_overlap(playerBox, platform);
            app->playerY += overlap.y;
            app->velocityY = 0;
            app->onGround = true; // True if resting on at least one platform
        }
    }

    // --- 3. APPLY SINGLE-FRAME FRICTION & LIMITERS ---
    app->velocityX -= (app->velocityX * 8.0f) * deltaTime;
    app->velocityX = std::clamp(app->velocityX, -app->maxRunSpeed, app->maxRunSpeed);
    app->velocityY -= (app->velocityY * 1.0f) * deltaTime;
}

void handle_inputs(AppData *app, float deltaTime)
{
    const uint8_t *keystate = SDL_GetKeyboardState(nullptr);
    // bulletproof, branching-free momentum acceleration
    app->velocityX += ((keystate[SDL_SCANCODE_D] || keystate[SDL_SCANCODE_RIGHT]) - (keystate[SDL_SCANCODE_A] || keystate[SDL_SCANCODE_LEFT])) * app->playerSpeed * deltaTime;
    if ((keystate[SDL_SCANCODE_W] || keystate[SDL_SCANCODE_UP] || keystate[SDL_SCANCODE_SPACE]) && app->onGround)
    {
        app->velocityY -= app->playerJumpHeight;
    }
}

// executes functions for game every frame
void tick(void *arg)
{
    uint64_t currentTick = SDL_GetTicks64();
    float deltaTime = (currentTick - lastTick) / 1000.0f; // convert to seconds
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

    handle_inputs(app, deltaTime);
    run_physics(app, deltaTime);
    // let camera catch up
    app->camX += (app->playerX - app->camX) * 6.5f * deltaTime;
    app->camY += (app->playerY - app->camY) * 6.5f * deltaTime;
    render_frame(app);
}

int main()
{
    SDL_Init(SDL_INIT_VIDEO);
    AppData app;
    // creates the game window and renderer
    app.window = SDL_CreateWindow("Gamkat", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    app.renderer = SDL_CreateRenderer(app.window, -1, SDL_RENDERER_ACCELERATED);

    // make a basic world
    World basicWorld;
    basicWorld.platforms = {{0, 0.5f, 1, 0.1f}, {1.1f, 0.4f, 1, 0.1f}};
    basicWorld.Load(&app);

    emscripten_set_main_loop_arg(tick, &app, 0, true);

    // cleans up data to prevent data leaks
    SDL_DestroyWindow(app.window);
    SDL_DestroyRenderer(app.renderer);
    SDL_Quit();

    return 0;
}
