#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
#include <algorithm>
#include <vector>
#include <sstream>
#include <random>
#include <array>
#include <cmath>

uint64_t lastTick = 0;
SDL_Cursor *cursorHand = nullptr;
SDL_Cursor *cursorPointer = nullptr;

enum GameState
{
    HOME_SCREEN,
    PLAYING,
    QUESTIONS,
    SHOP,
    GAME_END
};

struct Box
{
    float x;
    float y;
    float width;
    float height;
};

struct Question
{
    Question()
        : question(""), answers({"", "", "", ""}), correct({}) {}

    Question(std::string _question, std::array<std::string, 4> _answers, std::array<bool, 4> _correct)
        : question(_question), answers(_answers), correct(_correct) {}
    std::string question;
    std::array<std::string, 4> answers;
    std::array<bool, 4> correct;

    bool operator==(const Question &) const = default;
};

class Kit
{
public:
    std::vector<Question> questions;

    Question get_random_question()
    {
        if (questions.empty())
            return Question{};

        if (bag.empty())
            bag = questions;
        std::uniform_int_distribution range{0, static_cast<int>(bag.size()) - 1};
        int randomIndex = range(gen);
        Question choice = bag.at(randomIndex);
        bag[randomIndex] = bag.back();
        bag.pop_back();
        return choice;
    }

    Kit &operator=(Kit &other)
    {
        if (this->questions != other.questions)
        {
            other.questions = this->questions;
        }
        return *this;
    }

private:
    std::vector<Question> bag;
    std::random_device rd{};
    std::mt19937 gen{rd()};
};

Kit basicKit;

struct AppData
{
    SDL_Window *window = nullptr;
    SDL_Renderer *renderer = nullptr;
    GameState currentGameState = PLAYING;
    std::vector<Box> platforms = {{0, 0.5f, 1, 0.1f}};
    float gravity = 1.95f;
    float camX = 0;
    float camY = 0;
    float playerEnergy = 1500;
    float playerX = 0;
    float playerY = 0;
    float velocityX = 0;
    float velocityY = 0;
    float playerWidth = 0.0875f;
    float playerHeight = 0.175f;
    float playerSpeed = 6.5f;
    float playerJumpHeight = 1.05f;
    float maxRunSpeed = 2.75f;
    bool running = true;
    bool onGround = false;
    bool jumpKeyWasPressed = false;
    int jumpsRemaining = 2;
    float maxFallSpeed = 1;
    std::vector<float> checkpointX = {0};
    std::vector<float> checkpointY = {0};
    int checkpointIndex = 0;
    float stepThreshold = 0.01;
    TTF_Font *font = TTF_OpenFont("OpenSans-SemiBold.ttf", 35);
    int frameCount = 0;
    float frameTimer = 0;
    bool showHitboxes = true;
    SDL_Color backgroundColor = SDL_Color{58, 161, 247, 255};
    Kit activeKit;
    Question currentQuestion;
};

class TextElement
{
public:
    TextElement()
        : font(nullptr), renderer(nullptr),
          texture(nullptr), rect{0, 0, 0, 0} {}
    TextElement(TTF_Font *font, SDL_Renderer *renderer)
        : font(font), renderer(renderer), texture(nullptr), rect{0, 0, 0, 0} {}
    ~TextElement()
    {
        cleanup();
    }

    void init(TTF_Font *newFont, SDL_Renderer *newRenderer)
    {
        cleanup();
        font = newFont;
        renderer = newRenderer;
    }

    void set_text(const std::string &text, SDL_Color color, int x, int y, bool solid = false)
    {
        cleanup();
        SDL_Surface *surf;
        if (solid)
        {
            surf = TTF_RenderText_Solid(font, text.c_str(), color);
        }
        else
        {
            surf = TTF_RenderText_Blended(font, text.c_str(), color);
        }
        if (!surf)
            return;
        texture = SDL_CreateTextureFromSurface(renderer, surf);
        if (texture)
        {
            rect.x = x;
            rect.y = y;
            rect.w = surf->w;
            rect.h = surf->h;
        }
        SDL_FreeSurface(surf);
    }

    void set_text_right(const std::string &text, SDL_Color color, int rightEdge, int y, bool solid = false)
    {
        set_text(text, color, 0, y, solid);

        if (texture)
            rect.x = rightEdge - rect.w - 5;
    }

    void render_text()
    {
        if (texture)
        {
            SDL_RenderCopy(renderer, texture, nullptr, &rect);
        }
    }

    SDL_Rect *get_rect()
    {
        return &rect;
    }

private:
    void cleanup()
    {
        if (texture)
        {
            SDL_DestroyTexture(texture);
            texture = nullptr;
        }
    }

    TTF_Font *font;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    SDL_Rect rect;
};

TextElement fpsCounter;
TextElement energyCounter;
TextElement questionText;

class Button
{
public:
    bool pressed = false;
    bool hovering = false;
    Button()
        : txt(TextElement{}), renderer(nullptr), rect(SDL_Rect{0, 0, 1, 1}) {}

    Button(TTF_Font *font, SDL_Renderer *renderer)
        : txt(TextElement{font, renderer}), renderer(renderer), rect(SDL_Rect{0, 0, 1, 1}) {}

    void init(TTF_Font *font, SDL_Renderer *renderer)
    {
        cleanup();
        this->renderer = renderer;
        txt.init(font, renderer);
    }

    void set_button(std::string text, SDL_Color color, int _x, int _y, int width, int height)
    {
        rect = {
            rect.x = _x,
            rect.y = _y,
            rect.w = width,
            rect.h = height};

        txt.set_text(text, color, _x + rect.w / 2, _y + rect.h / 2);
        SDL_Rect *textRect = txt.get_rect();
        textRect->x -= textRect->w / 2;
        textRect->y -= textRect->h / 2;
    }

    void update(float deltaTime)
    {
        int mouseX, mouseY;
        Uint32 mouseButtons = SDL_GetMouseState(&mouseX, &mouseY);

        SDL_Point mouse = {mouseX, mouseY};

        hovering = SDL_PointInRect(&mouse, &rect);
        pressed =
            (mouseButtons & SDL_BUTTON(SDL_BUTTON_LEFT)) &&
            hovering;

        // extra polish
        if (hovering)
        {
            SDL_SetCursor(cursorHand);
            colorOffset += (-50 - colorOffset) * 7 * deltaTime;
        }
        else
        {
            SDL_SetCursor(cursorPointer);
            colorOffset += -colorOffset * 5 * deltaTime;
        }
    }

    void render_button()
    {
        int intOffset = std::round(colorOffset);
        SDL_SetRenderDrawColor(
            renderer,
            std::clamp(44 + intOffset, 0, 255),
            std::clamp(187 + intOffset, 0, 255),
            std::clamp(255 + intOffset, 0, 255),
            255);
        SDL_RenderFillRect(renderer, &rect);
        txt.render_text();
    }

    void cleanup()
    {
        txt.init(nullptr, nullptr);
    }

private:
    TextElement txt;
    SDL_Renderer *renderer;
    SDL_Rect rect;
    float colorOffset = 0;
};

Button questionButton;
Button quizButton1;
Button quizButton2;
Button quizButton3;
Button quizButton4;

class World
{
public:
    std::vector<Box> platforms;

    float spawnX = 0;
    float spawnY = 0;

    std::vector<float> checkpointsX = {0};
    std::vector<float> checkpointsY = {0};

    void Load(AppData *app)
    {
        app->platforms = platforms;
        app->playerX = spawnX;
        app->playerY = spawnY;
        app->camX = spawnX;
        app->camY = spawnY;
        app->checkpointX = checkpointsX;
        app->checkpointY = checkpointsY;
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

void draw_texture(Box box, AppData *app, SDL_Texture *texture)
{
    int width, height;
    SDL_GetWindowSizeInPixels(app->window, &width, &height);
    int renderWidth = std::round(box.width * width);
    int renderHeight = std::round(box.height * height);
    int renderX = std::round(box.x * width) - (renderWidth / 2);
    int renderY = std::round(box.y * height) - (renderHeight / 2);

    SDL_Rect rect = {renderX, renderY, renderWidth, renderHeight};

    SDL_RenderCopy(app->renderer, texture, nullptr, &rect);
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

void draw_texture_world(Box box, AppData *app, SDL_Texture *texture)
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
    draw_texture(box, app, texture);
}

void render_game(AppData *app)
{
    const SDL_Color &bg = app->backgroundColor;
    SDL_SetRenderDrawColor(app->renderer, bg.r, bg.g, bg.b, bg.a);
    SDL_RenderClear(app->renderer);

    if (app->showHitboxes)
    {
        // render player
        draw_box_world(Box{app->playerX, app->playerY, app->playerWidth, app->playerHeight}, app, 255, 0, 0);

        // render platform
        for (Box platform : app->platforms)
            draw_box_world(platform, app, 255, 255, 255);
    }

    fpsCounter.render_text();
    energyCounter.render_text();
    questionButton.render_button();

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
            if (std::abs(overlap.y) <= app->stepThreshold)
            {
                app->playerY += overlap.y;
                continue;
            }
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
            if (app->velocityY > 0) // makes sure it's the ground, not ceiling
            {
                app->onGround = true;
            }
            app->velocityY = 0;
        }
    }

    // --- 3. APPLY SINGLE-FRAME FRICTION & LIMITERS ---
    app->velocityX -= (app->velocityX * 8.0f) * deltaTime;
    app->velocityX = std::clamp(app->velocityX, -app->maxRunSpeed, app->maxRunSpeed);
    app->velocityY -= (app->velocityY * 1.0f) * deltaTime;
    app->velocityY = std::clamp(app->velocityY, -2.5f, app->maxFallSpeed);
}

void handle_inputs(AppData *app, float deltaTime)
{
    if (app->playerEnergy < 1)
        return;
    const uint8_t *keystate = SDL_GetKeyboardState(nullptr);

    float movement = ((keystate[SDL_SCANCODE_D] || keystate[SDL_SCANCODE_RIGHT]) -
                      (keystate[SDL_SCANCODE_A] || keystate[SDL_SCANCODE_LEFT])) *
                     app->playerSpeed * deltaTime;
    app->velocityX += movement;
    if (std::abs(movement) > 0)
    {
        app->playerEnergy -= 125 * deltaTime;
        std::stringstream ss;
        ss << "Energy: " << static_cast<uint64_t>(std::round(app->playerEnergy));
        int w;
        SDL_GetWindowSizeInPixels(app->window, &w, nullptr);
        energyCounter.set_text_right(ss.str(), SDL_Color{255, 255, 255, 255}, w, 5);
    }

    bool jumpKeyPressed =
        keystate[SDL_SCANCODE_W] ||
        keystate[SDL_SCANCODE_UP] ||
        keystate[SDL_SCANCODE_SPACE];

    if (app->onGround)
    {
        app->jumpsRemaining = 2;
    }
    else if (app->jumpsRemaining > 1)
    {
        app->jumpsRemaining = 1;
    }

    // Detect a new press, not a held key
    if (jumpKeyPressed && !app->jumpKeyWasPressed)
    {
        if (app->jumpsRemaining > 0)
        {
            app->velocityY = -app->playerJumpHeight;
            app->jumpsRemaining--;
            app->playerEnergy -= 15;
            std::stringstream ss;
            ss << "Energy: " << static_cast<uint64_t>(std::round(app->playerEnergy));
            int w;
            SDL_GetWindowSizeInPixels(app->window, &w, nullptr);
            energyCounter.set_text_right(ss.str(), SDL_Color{255, 255, 255, 255}, w, 5);
        }
    }

    app->jumpKeyWasPressed = jumpKeyPressed;
}

// executes functions for game every frame
void game_tick(AppData *app)
{
    uint64_t currentTick = SDL_GetTicks64();
    float deltaTime = (currentTick - lastTick) / 1000.0f; // convert to seconds
    lastTick = currentTick;
    app->frameTimer += deltaTime;

    // cap deltatime to prevent outrageous valuess
    deltaTime = std::min(deltaTime, 0.05f);

    // display fps
    if (app->frameTimer >= 0.5f)
    {
        std::stringstream ss;
        ss << "FPS: " << std::round(app->frameCount / app->frameTimer);
        fpsCounter.set_text(ss.str(), SDL_Color{255, 255, 255, 255}, 5, 5);
        app->frameCount = 0;
        app->frameTimer = 0;
    }
    else
    {
        app->frameCount += 1;
    }

    questionButton.update(deltaTime);
    if (questionButton.pressed)
    {
        app->currentGameState = QUESTIONS;
        app->currentQuestion = app->activeKit.get_random_question();
        return;
    }
    handle_inputs(app, deltaTime);
    run_physics(app, deltaTime);
    // detect player death
    if (app->playerY > app->checkpointY[app->checkpointIndex] + 1)
    {
        // implement checkpoint code later, maybe add a force death
        app->playerX = app->checkpointX[app->checkpointIndex];
        app->playerY = app->checkpointY[app->checkpointIndex];
    }
    // let camera catch up
    app->camX += (app->playerX - app->camX) * 6.5f * deltaTime;
    app->camY += (app->playerY - app->camY) * 6.5f * deltaTime;
    app->camY = std::min(app->camY, app->checkpointY[app->checkpointIndex]);
    render_game(app);
}

void render_quiz(AppData *app)
{
    SDL_Color &bg = app->backgroundColor;
    SDL_SetRenderDrawColor(app->renderer, bg.r, bg.g, bg.b, bg.a);
    SDL_RenderClear(app->renderer);
    questionText.render_text();
    quizButton1.render_button();
    quizButton2.render_button();
    quizButton3.render_button();
    quizButton4.render_button();
    SDL_RenderPresent(app->renderer);
}

void quiz_tick(AppData *app)
{
    uint64_t currentTick = SDL_GetTicks64();
    float deltaTime = (currentTick - lastTick) / 1000.0f; // convert to seconds
    lastTick = currentTick;
    app->frameTimer += deltaTime;

    quizButton1.update(deltaTime);
    quizButton2.update(deltaTime);
    quizButton3.update(deltaTime);
    quizButton4.update(deltaTime);
    render_quiz(app);
}

void shutdown(AppData *app)
{
    fpsCounter.init(nullptr, nullptr);
    energyCounter.init(nullptr, nullptr);
    questionText.init(nullptr, nullptr);
    questionButton.cleanup();
    quizButton1.cleanup();
    quizButton2.cleanup();
    quizButton3.cleanup();
    quizButton4.cleanup();
    SDL_DestroyRenderer(app->renderer);
    SDL_DestroyWindow(app->window);
    TTF_CloseFont(app->font);
    SDL_FreeCursor(cursorHand);
    SDL_FreeCursor(cursorPointer);
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
}

void tick(void *arg)
{
    AppData *app = static_cast<AppData *>(arg);
    // check our events
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        if (event.type == SDL_QUIT)
        {
            app->running = false;
#ifdef __EMSCRIPTEN__
            emscripten_cancel_main_loop();
            shutdown(app);
#endif
            return;
        }
        if (event.type == SDL_WINDOWEVENT &&
            event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) // reposition the UI elements so it doesn't get cooked
        {
            std::stringstream ss;
            ss << "Energy: " << app->playerEnergy;
            int w, h;
            SDL_GetWindowSizeInPixels(app->window, &w, &h);
            energyCounter.set_text_right(ss.str(), SDL_Color{255, 255, 255, 255}, w, 5);
            questionButton.set_button("Questions", SDL_Color{255, 255, 255, 255}, 5, h - 68, 200, 63);
            int centerX = w / 2;
            int centerY = h / 2 + 100;
            quizButton1.set_button("Option 1", SDL_Color{255, 255, 255, 255}, centerX - 205, centerY - 105, 200, 100);
            quizButton2.set_button("Option 2", SDL_Color{255, 255, 255, 255}, centerX + 10, centerY - 105, 200, 100);
            quizButton3.set_button("Option 3", SDL_Color{255, 255, 255, 255}, centerX - 205, centerY + 10, 200, 100);
            quizButton4.set_button("Option 4", SDL_Color{255, 255, 255, 255}, centerX + 10, centerY + 10, 200, 100);
        }
    }

    GameState &state = app->currentGameState;
    if (state == PLAYING)
    {
        game_tick(app);
    }
    else if (state == QUESTIONS)
    {
        quiz_tick(app);
    }
}

int main()
{
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    IMG_Init(IMG_INIT_PNG);

    AppData app;
    // creates the game window and renderer
    app.window = SDL_CreateWindow("Graoud", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    app.renderer = SDL_CreateRenderer(app.window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    // make a basic world
    World basicWorld;
    basicWorld.platforms = {{0, 0.5f, 1, 0.1f}, {0, 0.495f, 0.4f, 0.1f}, {1.5f, 0.45f, 1, 0.1f}};
    basicWorld.spawnY = 0.325f;
    basicWorld.checkpointsY = {0.325f};
    basicWorld.Load(&app);

    // make a basic kit
    basicKit.questions = {
        {"blank1", {"blank", "blank1", "blank1", "blank1"}, {true, true, true, true}},
        {"blank2", {"blank2", "blank2", "blank2", "blank2"}, {true, true, true, true}}};
    app.activeKit = basicKit;

    // initialize UI
    cursorHand = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_HAND);
    cursorPointer = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
    fpsCounter.init(app.font, app.renderer);
    energyCounter.init(app.font, app.renderer);
    questionText.init(app.font, app.renderer);
    std::stringstream ss;
    ss << "Energy: " << app.playerEnergy;
    int w, h;
    SDL_GetWindowSizeInPixels(app.window, &w, &h);
    energyCounter.set_text_right(ss.str(), SDL_Color{255, 255, 255, 255}, w, 5);
    questionButton.init(app.font, app.renderer);
    questionButton.set_button("Questions", SDL_Color{255, 255, 255, 255}, 5, h - 68, 200, 63);
    quizButton1.init(app.font, app.renderer);
    quizButton2.init(app.font, app.renderer);
    quizButton3.init(app.font, app.renderer);
    quizButton4.init(app.font, app.renderer);
    int centerX = w / 2;
    int centerY = h / 2 + 100;
    quizButton1.set_button("Option 1", SDL_Color{255, 255, 255, 255}, centerX - 205, centerY - 105, 200, 100);
    quizButton2.set_button("Option 2", SDL_Color{255, 255, 255, 255}, centerX + 10, centerY - 105, 200, 100);
    quizButton3.set_button("Option 3", SDL_Color{255, 255, 255, 255}, centerX - 205, centerY + 10, 200, 100);
    quizButton4.set_button("Option 4", SDL_Color{255, 255, 255, 255}, centerX + 10, centerY + 10, 200, 100);

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop_arg(tick, &app, 0, true);
#else
    while (app.running)
    {
        tick(&app);
    }
#endif

    // cleans up data to prevent data leaks
    shutdown(&app);

    return 0;
}
