// Nintendo Wii U port (Syobon Action U) of the OpenSyobonAction SDL shim.
//
// The original code targeted SDL 1.2: a small software framebuffer
// ("screen") plus SDL_gfx surface primitives, SDL_image, SDL_mixer and
// SDL_ttf. devkitPro only ships SDL2 for Wii U, so this file keeps the
// exact same software-rendering model (the game still blits into an
// SDL_Surface) and adds:
//
//   * the SDL_gfx surface primitives (SDL2_gfx is renderer-only),
//   * a PresentFrame() that uploads the framebuffer to a streaming
//     texture and shows it on the TV and the GamePad.  The playfield is
//     widescreen (748x420), which fills a 16:9 TV with no letterboxing.
//   * Wii U controller -> virtual key mapping (GamePad, Pro Controller,
//     Classic Controller, Wii Remote + Nunchuk),
//   * resource path resolution that works from a .wuhb content mount and
//     from an SD-card /apps/ installation.
#include "DxLib.h"

#include <locale.h>
#include <map>

// Defined in main.h / main.cpp; used by the alpha-blended watermark bar.
extern Uint32 gfxcolor;

SDL_Joystick *joystick = NULL;

// ---------------------------------------------------------------------------
// Framebuffer / presentation
// ---------------------------------------------------------------------------

// The game's native framebuffer (widescreen).  Declared in DxLib.h so the
// game code and the platform layer agree on the size.
#define GAME_W SCR_W
#define GAME_H SCR_H

SDL_Surface *screen = NULL;

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture *frameTex = NULL;
static SDL_Rect gameDest = { 0, 0, GAME_W, GAME_H };

// Wii U native TV mode (16:9).  The window is created at the TV's actual
// display mode so the GX2 color buffer matches the scan buffer exactly
// (no hardware rescale of the pixel art).  Fallbacks in case the mode
// cannot be queried.
#define WIIU_FALLBACK_W 1280
#define WIIU_FALLBACK_H 720

static int dispW = WIIU_FALLBACK_W;
static int dispH = WIIU_FALLBACK_H;

static void ComputeGameDest()
{
    float sx = (float)dispW / (float)GAME_W;
    float sy = (float)dispH / (float)GAME_H;
    float s = (sx < sy) ? sx : sy;

    int w = (int)(GAME_W * s + 0.5f);
    int h = (int)(GAME_H * s + 0.5f);

    gameDest.w = w;
    gameDest.h = h;
    gameDest.x = (dispW - w) / 2;
    gameDest.y = (dispH - h) / 2;
}

// Upload the software framebuffer and present it to both displays.
void PresentFrame()
{
    if (!renderer || !frameTex || !screen)
        return;

    SDL_UpdateTexture(frameTex, NULL, screen->pixels, screen->pitch);

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, frameTex, NULL, &gameDest);
    SDL_RenderPresent(renderer);
}

// ---------------------------------------------------------------------------
// Resource path resolution
// ---------------------------------------------------------------------------

static std::string g_prefix;
static bool g_prefixKnown = false;

std::string ResolvePath(const char *rel)
{
    if (g_prefixKnown)
        return g_prefix + rel;

    // Probe, in order of likelihood, the places the game content can live.
    //
    //  * /vol/content      - the .wuhb bundle content mount (Aroma/WUHB)
    //  * fs:/vol/content   - same mount accessed through the "fs" devoptab
    //  * fs:/vol/external01 - the SD card root
    //  * ""                - the current working directory (dev/test)
    const char *candidates[] = {
        "/vol/content/",
        "fs:/vol/content/",
        "",
        "fs:/vol/external01/wiiu/apps/SyobonActionU/",
        "/vol/external01/wiiu/apps/SyobonActionU/",
    };

    for (unsigned i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
        std::string p = std::string(candidates[i]) + rel;
        FILE *f = fopen(p.c_str(), "rb");
        if (f) {
            fclose(f);
            g_prefix = candidates[i];
            g_prefixKnown = true;
            return p;
        }
    }

    // A loose .rpx launched from the SD card keeps its resources next to
    // the executable, so try the directory SDL reports for the app too.
    char *base = SDL_GetBasePath();
    if (base) {
        const char *subs[] = { "", "content/", "../content/" };
        for (unsigned i = 0; i < sizeof(subs) / sizeof(subs[0]); i++) {
            std::string p = std::string(base) + subs[i] + rel;
            FILE *f = fopen(p.c_str(), "rb");
            if (f) {
                fclose(f);
                g_prefix = std::string(base) + subs[i];
                g_prefixKnown = true;
                SDL_free(base);
                return p;
            }
        }
        SDL_free(base);
    }

    // Nothing found: keep the plain relative path so the error message
    // from the loader is meaningful.
    g_prefix = "";
    g_prefixKnown = true;
    return std::string(rel);
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

// Keyboard state and virtual gamepad state are kept separate so that a
// neutral GamePad never clears a held keyboard key.
static bool keysHeld[SDL_NUM_SCANCODES];
static bool padHeld[SDL_NUM_SCANCODES];
static bool ex = false;

// Open joysticks (GamePad, Pro Controller, Classic Controller, Wii
// Remote, Nunchuk ...).  The Wii U SDL2 driver exposes them all.
#define MAX_JOYSTICKS 8
static SDL_Joystick *joysticks[MAX_JOYSTICKS];
static int numJoysticks = 0;
static int lastDeviceCount = -1;

static int ScanCodeOf(int key)
{
    SDL_Scancode sc = SDL_GetScancodeFromKey((SDL_Keycode)key);
    if ((int)sc < 0 || (int)sc >= SDL_NUM_SCANCODES)
        return -1;
    return (int)sc;
}

static void CloseJoysticks()
{
    for (int i = 0; i < numJoysticks; i++) {
        if (joysticks[i])
            SDL_JoystickClose(joysticks[i]);
        joysticks[i] = NULL;
    }
    numJoysticks = 0;
    joystick = NULL;
}

static void RefreshJoysticks()
{
    int count = SDL_NumJoysticks();

    CloseJoysticks();

    for (int i = 0; i < count && i < MAX_JOYSTICKS; i++) {
        SDL_Joystick *js = SDL_JoystickOpen(i);
        if (js)
            joysticks[numJoysticks++] = js;
    }

    joystick = (numJoysticks > 0) ? joysticks[0] : NULL;
    lastDeviceCount = count;
}

// Wii U button indices are identical across the GamePad, Pro
// Controller, Classic Controller and Wii Remote layouts used by the
// devkitPro SDL2 joystick driver:
//   0=A 1=B 2=X 3=Y 6=L 7=R 8=ZL 9=ZR 10=+ 11=- 12=Left 13=Up 14=Right 15=Down
#define PAD_BTN_A 0
#define PAD_BTN_B 1
#define PAD_BTN_PLUS 10
#define PAD_BTN_MINUS 11
#define PAD_BTN_LEFT 12
#define PAD_BTN_UP 13
#define PAD_BTN_RIGHT 14
#define PAD_BTN_DOWN 15

static void SetPadKey(int key, bool on)
{
    int sc = ScanCodeOf(key);
    if (sc >= 0)
        padHeld[sc] = on;
}

static void PollGamepad()
{
    if (SDL_NumJoysticks() != lastDeviceCount)
        RefreshJoysticks();

    for (int i = 0; i < numJoysticks; i++) {
        SDL_Joystick *js = joysticks[i];
        if (!js)
            continue;

        // D-pad (digital).
        bool left = SDL_JoystickGetButton(js, PAD_BTN_LEFT) != 0;
        bool right = SDL_JoystickGetButton(js, PAD_BTN_RIGHT) != 0;
        bool up = SDL_JoystickGetButton(js, PAD_BTN_UP) != 0;
        bool down = SDL_JoystickGetButton(js, PAD_BTN_DOWN) != 0;

        // Left analogue stick.
        Sint16 ax = SDL_JoystickGetAxis(js, 0);
        Sint16 ay = SDL_JoystickGetAxis(js, 1);
        if (ax < -8000)
            left = true;
        if (ax > 8000)
            right = true;
        if (ay < -8000)
            up = true;
        if (ay > 8000)
            down = true;

        if (left)
            SetPadKey(KEY_INPUT_LEFT, true);
        if (right)
            SetPadKey(KEY_INPUT_RIGHT, true);
        if (up)
            SetPadKey(KEY_INPUT_UP, true);
        if (down)
            SetPadKey(KEY_INPUT_DOWN, true);

        // A and B both jump, exactly like Z / Up on the keyboard.
        if (SDL_JoystickGetButton(js, PAD_BTN_A) ||
            SDL_JoystickGetButton(js, PAD_BTN_B))
            SetPadKey(KEY_INPUT_Z, true);

        // START returns to the title screen ("menu").
        if (SDL_JoystickGetButton(js, PAD_BTN_PLUS))
            SetPadKey(KEY_INPUT_F1, true);
    }
}

// Jump helper used by the original gameplay code.
int JoystickJumpPressed()
{
    for (int i = 0; i < numJoysticks; i++) {
        SDL_Joystick *js = joysticks[i];
        if (!js)
            continue;
        if (SDL_JoystickGetButton(js, JOYSTICK_JUMP) ||
            SDL_JoystickGetButton(js, PAD_BTN_A) ||
            SDL_JoystickGetButton(js, PAD_BTN_B))
            return 1;
    }
    return 0;
}

void UpdateKeys()
{
    SDL_Event event;

    // Virtual gamepad state is rebuilt every frame; a single-frame
    // touchscreen tap sets it for exactly one frame.
    memset(padHeld, 0, sizeof(padHeld));
    bool touchTap = false;

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_KEYDOWN:
            if (event.key.repeat == 0) {
                int sc = event.key.keysym.scancode;
                if (sc >= 0 && sc < SDL_NUM_SCANCODES)
                    keysHeld[sc] = true;
            }
            break;
        case SDL_KEYUP: {
            int sc = event.key.keysym.scancode;
            if (sc >= 0 && sc < SDL_NUM_SCANCODES)
                keysHeld[sc] = false;
            break;
        }
        case SDL_FINGERDOWN:
            // Tap the GamePad screen to start / confirm, like Z.
            touchTap = true;
            break;
        case SDL_QUIT:
            ex = true;
            break;
        case SDL_JOYDEVICEADDED:
        case SDL_JOYDEVICEREMOVED:
            lastDeviceCount = -1;   // force a rescan next poll
            break;
        default:
            break;
        }
    }

    SDL_JoystickUpdate();
    PollGamepad();

    if (touchTap)
        SetPadKey(KEY_INPUT_Z, true);
}

byte ProcessMessage()
{
    return ex;
}

byte CheckHitKey(int key)
{
    int sc = ScanCodeOf(key);
    if (sc < 0)
        return 0;
    // The original shim also treated ';' as a jump key; keep that alias.
    if (key == SDLK_z) {
        int semi = ScanCodeOf(SDLK_SEMICOLON);
        if (semi >= 0 && keysHeld[semi])
            return 1;
    }
    return (keysHeld[sc] || padHeld[sc]) ? 1 : 0;
}

byte WaitKey()
{
    SDL_Event event;
    while (true) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_KEYDOWN)
                return 1;
            if (event.type == SDL_FINGERDOWN)
                return 1;
            // Never hang if the user leaves via HOME / the OS asks us
            // to quit while a message is waiting to be dismissed.
            if (event.type == SDL_QUIT) {
                ex = true;
                return 1;
            }
        }
        // Keep the GamePad responsive while waiting.
        SDL_JoystickUpdate();
        for (int i = 0; i < numJoysticks; i++) {
            SDL_Joystick *js = joysticks[i];
            if (!js)
                continue;
            if (SDL_JoystickGetButton(js, PAD_BTN_A) ||
                SDL_JoystickGetButton(js, PAD_BTN_B) ||
                SDL_JoystickGetButton(js, PAD_BTN_PLUS))
                return 1;
        }
        // Keep the picture alive: the game loop is blocked here, so the
        // last completed frame must be re-presented each tick.
        PresentFrame();
        SDL_Delay(16);
    }
}

// ---------------------------------------------------------------------------
// Initialisation / shutdown
// ---------------------------------------------------------------------------

bool sound = true;
void deinit();

int DxLib_Init()
{
    atexit(deinit);

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK |
                 SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) < 0) {
        fprintf(stderr, "Unable to init SDL: %s\n", SDL_GetError());
        return -1;
    }

    // Crisp nearest-neighbour scaling for the pixel art.
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");

    // Match the TV's real display mode (480p / 720p / 1080p) so the GX2
    // colour buffer equals the scan buffer and the artwork is never
    // resampled in hardware.
    int displayIndex = 0;
    SDL_DisplayMode dm;
    if (SDL_GetCurrentDisplayMode(displayIndex, &dm) == 0 &&
        dm.w > 0 && dm.h > 0) {
        dispW = dm.w;
        dispH = dm.h;
    }
    fprintf(stderr, "SDL: display %dx%d\n", dispW, dispH);

    window = SDL_CreateWindow("Syobon Action U",
                              SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED,
                              dispW, dispH, 0);
    if (!window) {
        fprintf(stderr, "Unable to create window: %s\n", SDL_GetError());
        SDL_Quit();
        return -1;
    }

    // The Wii U renderer drives GX2 directly.  Request vsync for tear-free
    // presentation; the game paces itself to 30 fps with wait2(), and wait2
    // only delays the remainder of the frame budget, so vsync does not
    // double the frame time.
    renderer = SDL_CreateRenderer(window, -1,
                                  SDL_RENDERER_ACCELERATED |
                                  SDL_RENDERER_PRESENTVSYNC);
    if (!renderer)
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer)
        renderer = SDL_CreateRenderer(window, -1, 0);
    if (!renderer) {
        fprintf(stderr, "Unable to create renderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    // The original game framebuffer: a plain 32-bit software surface.
    screen = SDL_CreateRGBSurfaceWithFormat(0, GAME_W, GAME_H, 32,
                                            SDL_PIXELFORMAT_ARGB8888);
    if (!screen) {
        fprintf(stderr, "Unable to create framebuffer: %s\n", SDL_GetError());
        return -1;
    }

    frameTex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                 SDL_TEXTUREACCESS_STREAMING, GAME_W, GAME_H);
    if (!frameTex) {
        fprintf(stderr, "Unable to create frame texture: %s\n", SDL_GetError());
        return -1;
    }
    SDL_SetTextureBlendMode(frameTex, SDL_BLENDMODE_NONE);

    ComputeGameDest();

    if (IMG_Init(IMG_INIT_PNG) != IMG_INIT_PNG) {
        fprintf(stderr, "Unable to init SDL_image: %s\n", IMG_GetError());
        return -1;
    }

    if (TTF_Init() == -1) {
        fprintf(stderr, "Unable to init SDL_ttf: %s\n", TTF_GetError());
        return -1;
    }

    // Audio Rate, Audio Format, Audio Channels, Audio Buffers
    //
    // 1024 samples @ 22050 Hz is the configuration verified on hardware;
    // do not change it without re-testing.
#define AUDIO_CHANNELS 2
    if (sound && Mix_OpenAudio(22050, AUDIO_S16SYS, AUDIO_CHANNELS, 1024)) {
        fprintf(stderr, "Unable to init SDL_mixer: %s\n", Mix_GetError());
        sound = false;
    }

    for (int i = 0; i < SDL_NUM_SCANCODES; i++) {
        keysHeld[i] = false;
        padHeld[i] = false;
    }
    for (int i = 0; i < FONT_MAX; i++)
        font[i] = NULL;

    RefreshJoysticks();

    srand(time(NULL));

    return 0;
}

// ---------------------------------------------------------------------------
// Fonts and text
// ---------------------------------------------------------------------------

byte fontsize = 0;
TTF_Font *font[FONT_MAX];

void SetFontSize(byte size)
{
    // font[] is indexed by pixel size, so clamp to the table.
    if (size >= FONT_MAX)
        size = FONT_MAX - 1;
    fontsize = size;
    if (font[size] == NULL) {
        std::string path = ResolvePath("res/sazanami-gothic.ttf");
        font[size] = TTF_OpenFont(path.c_str(), size);
        if (font[size] == NULL) {
            printf("Unable to load font: %s\n", TTF_GetError());
            exit(1);
        }
    }
}

byte GetFontSize()
{
    return fontsize;
}

byte fontType = DX_FONTTYPE_NORMAL;

void ChangeFontType(byte type)
{
    fontType = type;
}

void DrawString(int a, int b, const char *x, Uint32 c)
{
    if (!x || !*x)
        return;
    if (!font[fontsize])
        return;

    SDL_Color color = { (Uint8)(c >> 16), (Uint8)(c >> 8), (Uint8)c, 255 };
    SDL_Surface *rendered = TTF_RenderUTF8_Solid(font[fontsize], x, color);
    if (!rendered)
        return;

    if (fontType == DX_FONTTYPE_EDGE) {
        SDL_Color blk = { 0, 0, 0, 255 };
        SDL_Surface *shadow = TTF_RenderUTF8_Solid(font[fontsize], x, blk);
        if (shadow) {
            DrawGraphZ(a - 1, b - 1, shadow);
            DrawGraphZ(a, b - 1, shadow);
            DrawGraphZ(a + 1, b - 1, shadow);
            DrawGraphZ(a - 1, b, shadow);
            DrawGraphZ(a + 1, b, shadow);
            DrawGraphZ(a - 1, b + 1, shadow);
            DrawGraphZ(a, b + 1, shadow);
            DrawGraphZ(a + 1, b + 1, shadow);
            SDL_FreeSurface(shadow);
        }
    }
    DrawGraphZ(a, b, rendered);
    SDL_FreeSurface(rendered);
}

void DrawFormatString(int a, int b, Uint32 color, const char *str, ...)
{
    if (!str)
        return;
    // The original used a fixed strlen+16 buffer with vsprintf, which can
    // overflow for long numbers. Size it dynamically and bound the write.
    va_list args;
    va_start(args, str);
    int need = vsnprintf(NULL, 0, str, args);
    va_end(args);
    if (need < 0)
        return;

    char *newstr = new char[need + 1];
    va_start(args, str);
    vsnprintf(newstr, need + 1, str, args);
    va_end(args);

    DrawString(a, b, newstr, color);
    delete[] newstr;
}

// ---------------------------------------------------------------------------
// Sprite drawing
// ---------------------------------------------------------------------------

// Mirrored / rotated sprite cache.  The original implementation called
// zoomSurface/rotozoomSurface for every mirrored sprite every frame;
// caching keeps the identical output without the per-frame allocation.
static std::map<SDL_Surface *, SDL_Surface *> flipCache;
static std::map<SDL_Surface *, SDL_Surface *> rotCache;

static SDL_Surface *GetFlipped(SDL_Surface *mx)
{
    std::map<SDL_Surface *, SDL_Surface *>::iterator it = flipCache.find(mx);
    if (it != flipCache.end())
        return it->second;

    SDL_Surface *flipped = zoomSurface(mx, -1, 1, 0);
    if (flipped)
        SDL_SetColorKey(flipped, SDL_TRUE,
                        SDL_MapRGB(flipped->format, 9 * 16 + 9, 255, 255));
    flipCache[mx] = flipped;
    return flipped;
}

static SDL_Surface *GetRotated(SDL_Surface *mx)
{
    std::map<SDL_Surface *, SDL_Surface *>::iterator it = rotCache.find(mx);
    if (it != rotCache.end())
        return it->second;

    SDL_Surface *rot = rotozoomSurface(mx, 180, 1, 0);
    if (rot)
        SDL_SetColorKey(rot, SDL_TRUE,
                        SDL_MapRGB(rot->format, 9 * 16 + 9, 255, 255));
    rotCache[mx] = rot;
    return rot;
}

void DrawGraphZ(int a, int b, SDL_Surface * mx)
{
    if (mx && screen) {
        SDL_Rect offset;
        offset.x = a;
        offset.y = b;
        offset.w = mx->w;
        offset.h = mx->h;
        SDL_BlitSurface(mx, NULL, screen, &offset);
    }
}

void DrawTurnGraphZ(int a, int b, SDL_Surface * mx)
{
    if (mx && screen) {
        SDL_Surface *flipped = GetFlipped(mx);
        if (!flipped)
            return;
        SDL_Rect offset;
        offset.x = a;
        offset.y = b;
        offset.w = flipped->w;
        offset.h = flipped->h;
        SDL_BlitSurface(flipped, NULL, screen, &offset);
    }
}

void DrawVertTurnGraph(int a, int b, SDL_Surface * mx)
{
    if (mx && screen) {
        SDL_Surface *rot = GetRotated(mx);
        if (!rot)
            return;
        SDL_Rect offset;
        offset.x = a - mx->w / 2;
        offset.y = b - mx->h / 2;
        offset.w = rot->w;
        offset.h = rot->h;
        SDL_BlitSurface(rot, NULL, screen, &offset);
    }
}

SDL_Surface *DerivationGraph(int srcx, int srcy, int width, int height,
			     SDL_Surface * src)
{
    if (!src)
        return NULL;

    SDL_Surface *img =
        SDL_CreateRGBSurfaceWithFormat(0, width, height, 32,
                                       SDL_PIXELFORMAT_ARGB8888);
    if (!img)
        return NULL;

    SDL_Rect offset;
    offset.x = srcx;
    offset.y = srcy;
    offset.w = width;
    offset.h = height;

    SDL_BlitSurface(src, &offset, img, NULL);
    SDL_SetColorKey(img, SDL_TRUE,
		    SDL_MapRGB(img->format, 9 * 16 + 9, 255, 255));
    return img;
}

//Noticably different than the original
SDL_Surface *LoadGraph(const char *filename)
{
    std::string path = ResolvePath(filename);
    SDL_Surface *image = IMG_Load(path.c_str());

    if (image)
        return image;

    fprintf(stderr, "Error: Unable to load %s: %s\n", path.c_str(),
            IMG_GetError());
    exit(1);
}

// ---------------------------------------------------------------------------
// Sound
// ---------------------------------------------------------------------------

void PlaySoundMem(Mix_Chunk * s, int l)
{
    if (sound)
        Mix_PlayChannel(-1, s, l);
}

void StopSoundMem(Mix_Chunk * s)
{
    // SDL_mixer stops a chunk by channel; find the channel playing it.
    if (!s)
        return;
    int channels = Mix_AllocateChannels(-1);
    for (int i = 0; i < channels; i++) {
        if (Mix_GetChunk(i) == s) {
            Mix_HaltChannel(i);
            return;
        }
    }
}

Mix_Chunk *LoadSoundMem(const char *f)
{
    if (!sound)
        return NULL;

    std::string path = ResolvePath(f);
    Mix_Chunk *s = Mix_LoadWAV(path.c_str());
    if (s)
        return s;
    fprintf(stderr, "Error: Unable to load sound %s: %s\n", path.c_str(),
            Mix_GetError());
    return NULL;
}

Mix_Music *LoadMusicMem(const char *f)
{
    if (!sound)
        return NULL;

    std::string path = ResolvePath(f);
    Mix_Music *m = Mix_LoadMUS(path.c_str());
    if (m)
        return m;
    fprintf(stderr, "Error: Unable to load music %s: %s\n", path.c_str(),
            Mix_GetError());
    return NULL;
}

// ---------------------------------------------------------------------------
// Shutdown helpers (called from the game's deinit())
// ---------------------------------------------------------------------------

void ShutdownJoysticks()
{
    CloseJoysticks();
}

void ShutdownVideo()
{
    if (frameTex) {
        SDL_DestroyTexture(frameTex);
        frameTex = NULL;
    }
    if (renderer) {
        SDL_DestroyRenderer(renderer);
        renderer = NULL;
    }
    if (window) {
        SDL_DestroyWindow(window);
        window = NULL;
    }

    flipCache.clear();
    rotCache.clear();
}

// Alpha-blended filled rectangle (used for the watermark backing bar so
// the text stays readable over the busy ground tiles).
void fillrectAlpha(int a, int b, int c, int d, int alpha)
{
    if (!screen)
        return;
    if (alpha < 0) alpha = 0;
    if (alpha > 255) alpha = 255;

    int x0 = a, y0 = b, x1 = a + c - 1, y1 = b + d - 1;
    if (x1 < x0) { int t = x0; x0 = x1; x1 = t; }
    if (y1 < y0) { int t = y0; y0 = y1; y1 = t; }
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 >= screen->w) x1 = screen->w - 1;
    if (y1 >= screen->h) y1 = screen->h - 1;

    Uint8 r = (Uint8)((gfxcolor >> 24) & 0xFF);
    Uint8 g = (Uint8)((gfxcolor >> 16) & 0xFF);
    Uint8 bl = (Uint8)((gfxcolor >> 8) & 0xFF);

    for (int y = y0; y <= y1; y++) {
        Uint32 *row = (Uint32 *)((Uint8 *)screen->pixels + y * screen->pitch);
        for (int x = x0; x <= x1; x++) {
            Uint32 src = row[x];
            Uint8 sr = (Uint8)((src >> 16) & 0xFF);
            Uint8 sg = (Uint8)((src >> 8) & 0xFF);
            Uint8 sb = (Uint8)(src & 0xFF);
            Uint8 nr = (Uint8)((r * alpha + sr * (255 - alpha)) / 255);
            Uint8 ng = (Uint8)((g * alpha + sg * (255 - alpha)) / 255);
            Uint8 nb = (Uint8)((bl * alpha + sb * (255 - alpha)) / 255);
            row[x] = 0xFF000000u | ((Uint32)nr << 16) |
                     ((Uint32)ng << 8) | nb;
        }
    }
}

// ---------------------------------------------------------------------------
// SDL_gfx surface primitives (SDL 1.2 semantics)
//
// Colour is packed 0xRRGGBBAA, matching setcolor()'s gfxcolor and the
// original SDL_gfx.  The "Color" variants write opaque pixels.
// ---------------------------------------------------------------------------

static inline Uint32 MapRGBOpaque(Uint8 r, Uint8 g, Uint8 b)
{
    const SDL_PixelFormat *fmt = screen->format;
    // Must include the alpha channel, exactly like SDL_MapRGB(): the
    // framebuffer is ARGB8888 and the Wii U renderer honours the alpha
    // byte.  Without the Amask these pixels get alpha 0 and the shapes
    // (pipes, ground, goal) vanish into the black letterbox backdrop.
    return (Uint32)(((r >> fmt->Rloss) << fmt->Rshift) |
                    ((g >> fmt->Gloss) << fmt->Gshift) |
                    ((b >> fmt->Bloss) << fmt->Bshift) |
                    fmt->Amask);
}

static inline Uint32 GfxPixel(Uint32 color)
{
    return MapRGBOpaque((Uint8)(color >> 24), (Uint8)(color >> 16),
                        (Uint8)(color >> 8));
}

static inline void PutPixel(SDL_Surface *dst, int x, int y, Uint32 pixel)
{
    if (x < 0 || y < 0 || x >= dst->w || y >= dst->h)
        return;
    ((Uint32 *)((Uint8 *)dst->pixels + y * dst->pitch))[x] = pixel;
}

int pixelColor(SDL_Surface * dst, Sint16 x, Sint16 y, Uint32 color)
{
    if (!dst)
        return -1;
    PutPixel(dst, x, y, GfxPixel(color));
    return 0;
}

int lineColor(SDL_Surface * dst, Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2,
	      Uint32 color)
{
    if (!dst)
        return -1;

    Uint32 pixel = GfxPixel(color);

    int dx = (x2 > x1) ? (x2 - x1) : (x1 - x2);
    int dy = (y2 > y1) ? (y2 - y1) : (y1 - y2);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;
    int x = x1, y = y1;

    while (true) {
        PutPixel(dst, x, y, pixel);
        if (x == x2 && y == y2)
            break;
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x += sx;
        }
        if (e2 < dx) {
            err += dx;
            y += sy;
        }
    }
    return 0;
}

int rectangleColor(SDL_Surface * dst, Sint16 x1, Sint16 y1, Sint16 x2,
		   Sint16 y2, Uint32 color)
{
    if (!dst)
        return -1;
    if (x1 > x2) {
        Sint16 t = x1; x1 = x2; x2 = t;
    }
    if (y1 > y2) {
        Sint16 t = y1; y1 = y2; y2 = t;
    }
    lineColor(dst, x1, y1, x2, y1, color);
    lineColor(dst, x1, y2, x2, y2, color);
    lineColor(dst, x1, y1, x1, y2, color);
    lineColor(dst, x2, y1, x2, y2, color);
    return 0;
}

int boxColor(SDL_Surface * dst, Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2,
	     Uint32 color)
{
    if (!dst)
        return -1;
    if (x1 > x2) {
        Sint16 t = x1; x1 = x2; x2 = t;
    }
    if (y1 > y2) {
        Sint16 t = y1; y1 = y2; y2 = t;
    }
    if (x2 < 0 || y2 < 0 || x1 >= dst->w || y1 >= dst->h)
        return 0;

    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 >= dst->w) x2 = dst->w - 1;
    if (y2 >= dst->h) y2 = dst->h - 1;

    SDL_Rect r;
    r.x = x1;
    r.y = y1;
    r.w = x2 - x1 + 1;
    r.h = y2 - y1 + 1;
    SDL_FillRect(dst, &r, GfxPixel(color));
    return 0;
}

// Midpoint ellipse outline (SDL_gfx semantics: rx/ry are the radii).
int ellipseColor(SDL_Surface * dst, Sint16 x, Sint16 y, Sint16 rx,
		 Sint16 ry, Uint32 color)
{
    if (!dst || rx < 0 || ry < 0)
        return -1;
    if (rx == 0 || ry == 0)
        return pixelColor(dst, x, y, color);

    Uint32 pixel = GfxPixel(color);
    long rx2 = (long)rx * rx;
    long ry2 = (long)ry * ry;
    long fx = 0, fy = ry;
    long p = ry2 - rx2 * ry + rx2 / 4;

    while (fx * ry2 < fy * rx2) {
        PutPixel(dst, x + fx, y + fy, pixel);
        PutPixel(dst, x - fx, y + fy, pixel);
        PutPixel(dst, x + fx, y - fy, pixel);
        PutPixel(dst, x - fx, y - fy, pixel);
        fx++;
        if (p < 0) {
            p += 2 * ry2 * fx + ry2;
        } else {
            fy--;
            p += 2 * ry2 * fx - 2 * rx2 * fy + ry2;
        }
    }

    fx = rx;
    fy = 0;
    p = rx2 - ry2 * rx + ry2 / 4;
    while (fx * ry2 >= fy * rx2) {
        PutPixel(dst, x + fx, y + fy, pixel);
        PutPixel(dst, x - fx, y + fy, pixel);
        PutPixel(dst, x + fx, y - fy, pixel);
        PutPixel(dst, x - fx, y - fy, pixel);
        fy++;
        if (p < 0) {
            p += 2 * rx2 * fy + rx2;
        } else {
            fx--;
            p += 2 * rx2 * fy - 2 * ry2 * fx + rx2;
        }
    }
    return 0;
}

// Filled ellipse via horizontal spans.
int filledEllipseColor(SDL_Surface * dst, Sint16 x, Sint16 y, Sint16 rx,
		       Sint16 ry, Uint32 color)
{
    if (!dst || rx < 0 || ry < 0)
        return -1;
    if (rx == 0 || ry == 0)
        return pixelColor(dst, x, y, color);

    Uint32 pixel = GfxPixel(color);
    long rx2 = (long)rx * rx;
    long ry2 = (long)ry * ry;

    for (int dy = -ry; dy <= ry; dy++) {
        long t = ry2 - (long)dy * dy;
        if (t < 0)
            continue;
        // half width = rx * sqrt(t) / ry
        double hw = (double)rx * sqrt((double)t) / (double)ry;
        int w = (int)(hw + 0.5);
        int px = x - w;
        int py = y + dy;
        if (py < 0 || py >= dst->h)
            continue;
        if (px + 2 * w < 0 || px >= dst->w)
            continue;
        SDL_Rect r;
        r.x = px;
        r.y = py;
        r.w = 2 * w + 1;
        r.h = 1;
        if (r.x < 0) {
            r.w += r.x;
            r.x = 0;
        }
        if (r.x + r.w > dst->w)
            r.w = dst->w - r.x;
        if (r.w > 0)
            SDL_FillRect(dst, &r, pixel);
    }
    (void)rx2;
    return 0;
}
