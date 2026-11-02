/* First-launch welcome (desktop), before the game window: where to keep settings and saves
 * (a portable userdata folder beside the executable, or the user profile), then the disc images.
 * The settings wizard follows on the first frame. Drawn with the settings painter in its own
 * window; keyboard, mouse and pad. --setup shows the disc step again. */

enum {
    WELCOME_PANEL_W = 640,
    WELCOME_HEADER_H = 150,
    WELCOME_FOOTER_H = 100,
};

static struct {
    int step;          /* 0 storage, 1 discs */
    int row;
    int full;          /* first launch: the storage step comes first */
    int portable;
    char disc[2][4096];
    int disc_state[2]; /* 0 not set, 1 a matching image, -1 not that disc */
    int changed[2];
    int done, quit, dirty;
    int stick;
    const char *note;
    OverlayLayout layout;
} s_welcome;

static int Welcome_RowCount(void) { return s_welcome.step == 0 ? 3 : 4; }
static int Welcome_DesignHeight(void) {
    return WELCOME_HEADER_H + Welcome_RowCount() * OVERLAY_ROW_H + WELCOME_FOOTER_H;
}

static const char *Welcome_FileName(const char *path) {
    const char *slash = strrchr(path, '/'), *back = strrchr(path, '\\');
    if (back && (!slash || back > slash)) slash = back;
    return slash ? slash + 1 : path;
}

static void Welcome_Row(int row, const char **label, const char **value, Color *valueColor) {
    static char name[2][64];
    *value = "";
    *valueColor = kMuted;
    if (s_welcome.step == 0) {
        static const char *const labels[] = {"Portable", "Standard", "Quit"};
        static const char *const values[] = {"Next to the game", "In your user folder", ""};
        *label = labels[row];
        *value = values[row];
        if (row == (s_welcome.portable ? 0 : 1)) *valueColor = kAccent;
        return;
    }
    if (row < 2) {
        *label = row ? "Disc 2" : "Disc 1";
        if (!s_welcome.disc_state[row]) {
            *value = row ? "Not set (can wait)" : "Not set";
        } else if (s_welcome.disc_state[row] < 0) {
            *value = row ? "Not the USA Disc 2" : "Not the USA Disc 1";
            *valueColor = kDanger;
        } else {
            const char *file = Welcome_FileName(s_welcome.disc[row]);
            size_t length = strlen(file);
            if (length > 36) snprintf(name[row], sizeof(name[row]), "...%s", file + length - 33);
            else snprintf(name[row], sizeof(name[row]), "%s", file);
            *value = name[row];
            *valueColor = kAccent;
        }
        return;
    }
    *label = row == 2 ? "Continue to settings" : s_welcome.full ? "Back" : "Quit";
}

static void Welcome_Help(const char **first, const char **second) {
    *first = *second = "";
    if (s_welcome.note) { *first = s_welcome.note; return; }
    if (s_welcome.step == 0) {
        if (s_welcome.row == 0) {
            *first = "Settings, saves, mods and HD backgrounds go in a userdata folder";
            *second = "beside the game. Copy the whole folder to play on another PC.";
        } else if (s_welcome.row == 1) {
            *first = "Settings, saves, mods and HD backgrounds go in your user profile,";
            *second = "shared by every copy of the game on this PC.";
        } else {
            *first = "Close without changing anything.";
        }
        return;
    }
    if (s_welcome.row < 2) {
        *first = "Choose the Parasite Eve (USA) CUE or BIN image for this disc.";
        *second = s_welcome.portable ? "Keep it inside the game folder so the portable copy finds it."
                                     : "Picking the other disc by mistake puts it in its own slot.";
    } else if (s_welcome.row == 2) {
        *first = "Next: graphics, controls, audio and the Mods menu, then Start game.";
        *second = "Disc 2 can wait; the game asks for it when the story needs it.";
    } else {
        *first = s_welcome.full ? "Back to where settings and saves are kept." : "Close without starting the game.";
    }
}

static void Welcome_Paint(OverlaySurface *surface, const OverlayLayout *layout) {
    HostOverlayState none = {0};
    Painter painter = {surface, &none, layout->scale};
    const float s = layout->scale;
    const int rows = Welcome_RowCount();
    const float footerTop = (float)(WELCOME_HEADER_H + rows * OVERLAY_ROW_H) * s;
    const char *help1, *help2;
    uint8_t *solid;

    memset(surface->pixels, 0, (size_t)surface->w * (size_t)surface->h * 4u);
    PaintFrame(&painter, (float)WELCOME_PANEL_W * s, (float)Welcome_DesignHeight() * s);
    DrawText(surface, (float)OVERLAY_PAD * s, 40.0f * s, 26.0f * s,
             s_welcome.step == 0 ? "Welcome to Parasite Eve" : "Disc images", kText, ALIGN_LEFT);
    DrawText(surface, (float)(WELCOME_PANEL_W - OVERLAY_PAD) * s, 38.0f * s, 14.0f * s,
             !s_welcome.full ? "SETUP" : s_welcome.step == 0 ? "STEP 1 OF 3" : "STEP 2 OF 3", kMuted,
             ALIGN_RIGHT);
    DrawText(surface, (float)OVERLAY_PAD * s, 84.0f * s, 16.0f * s,
             s_welcome.step == 0 ? "A short setup before the first start. Where should the game keep"
                                 : "Point the game at your Parasite Eve (USA) disc images.",
             WithAlpha(kText, 0.82f), ALIGN_LEFT);
    DrawText(surface, (float)OVERLAY_PAD * s, 108.0f * s, 16.0f * s,
             s_welcome.step == 0 ? "your settings, saves and mods?" : "CUE or BIN files, as dumped from your own discs.",
             WithAlpha(kText, 0.82f), ALIGN_LEFT);
    FillRoundRect(surface, (float)OVERLAY_PAD * s, (float)(WELCOME_HEADER_H - 12) * s,
                  (float)(WELCOME_PANEL_W - 2 * OVERLAY_PAD) * s, 1.0f, 0.0f, WithAlpha(kText, 0.10f));

    for (int row = 0; row < rows; row++) {
        const float top = (float)(WELCOME_HEADER_H + row * OVERLAY_ROW_H) * s;
        const float mid = top + (float)OVERLAY_ROW_H * 0.5f * s;
        const int selected = row == s_welcome.row;
        const char *label, *value;
        Color valueColor;
        Welcome_Row(row, &label, &value, &valueColor);
        if (selected) {
            FillRoundRect(surface, (float)(OVERLAY_PAD / 2) * s, top + 3.0f * s,
                          (float)(WELCOME_PANEL_W - OVERLAY_PAD) * s, (float)(OVERLAY_ROW_H - 6) * s, 9.0f * s,
                          WithAlpha(kAccent, 0.16f));
            FillRoundRect(surface, (float)(OVERLAY_PAD / 2) * s + 4.0f * s, mid - 10.0f * s, 3.0f * s,
                          20.0f * s, 1.5f * s, kAccent);
        }
        DrawText(surface, (float)OVERLAY_PAD * s, mid + 6.0f * s, 17.0f * s, label,
                 selected ? kText : WithAlpha(kText, 0.82f), ALIGN_LEFT);
        if (*value)
            DrawText(surface, (float)(WELCOME_PANEL_W - OVERLAY_PAD) * s, mid + 6.0f * s, 15.0f * s, value,
                     valueColor, ALIGN_RIGHT);
    }

    Welcome_Help(&help1, &help2);
    FillRoundRect(surface, (float)OVERLAY_PAD * s, footerTop + 6.0f * s,
                  (float)(WELCOME_PANEL_W - 2 * OVERLAY_PAD) * s, 1.0f, 0.0f, WithAlpha(kText, 0.10f));
    DrawText(surface, (float)OVERLAY_PAD * s, footerTop + 32.0f * s, 14.0f * s, help1,
             s_welcome.note ? kDanger : kMuted, ALIGN_LEFT);
    DrawText(surface, (float)OVERLAY_PAD * s, footerTop + 52.0f * s, 14.0f * s, help2, kMuted, ALIGN_LEFT);
    DrawText(surface, (float)OVERLAY_PAD * s, footerTop + 82.0f * s, 13.0f * s,
             "Up/Down select   Enter / South choose   Esc / East back", WithAlpha(kMuted, 0.7f), ALIGN_LEFT);
    solid = surface->pixels + (size_t)(surface->h - 1) * (size_t)surface->w * 4u;
    solid[3] = 255;
}

static void Welcome_Render(SDL_Renderer *renderer, SDL_Window *window, SDL_Texture **texture, int *texW, int *texH) {
    int w = 0, h = 0;
    OverlayLayout layout;
    SDL_GetWindowSizeInPixels(window, &w, &h);
    if (w <= 0) w = 760;
    if (h <= 0) h = 600;
    {
        const int designH = Welcome_DesignHeight();
        float scale = (float)w * 0.94f / (float)WELCOME_PANEL_W;
        if (scale > (float)h * 0.94f / (float)designH) scale = (float)h * 0.94f / (float)designH;
        if (scale > 2.5f) scale = 2.5f;
        if (scale < 0.5f) scale = 0.5f;
        scale = floorf(scale * 8.0f) / 8.0f;
        layout.scale = scale;
        layout.w = (int)ceilf((float)WELCOME_PANEL_W * scale);
        layout.h = (int)ceilf((float)designH * scale);
        layout.x = (w - layout.w) / 2;
        layout.y = (h - layout.h) / 2;
    }
    s_welcome.layout = layout;
    if (OverlayFontReady() && OverlaySurfaceResize(&s_overlaySurface, layout.w, layout.h)) {
        Welcome_Paint(&s_overlaySurface, &layout);
        if (*texture == NULL || *texW != layout.w || *texH != layout.h) {
            if (*texture != NULL) SDL_DestroyTexture(*texture);
            *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING,
                                         layout.w, layout.h);
            *texW = layout.w;
            *texH = layout.h;
            if (*texture != NULL) SDL_SetTextureBlendMode(*texture, SDL_BLENDMODE_BLEND_PREMULTIPLIED);
        }
        if (*texture != NULL) SDL_UpdateTexture(*texture, NULL, s_overlaySurface.pixels, layout.w * 4);
    }
    SDL_SetRenderDrawColor(renderer, 12, 14, 18, 255);
    SDL_RenderClear(renderer);
    if (*texture != NULL) {
        SDL_FRect dst = {(float)layout.x, (float)layout.y, (float)layout.w, (float)layout.h};
        SDL_RenderTexture(renderer, *texture, NULL, &dst);
    }
    SDL_RenderPresent(renderer);
}

/* Which disc an image is: 1 or 2, 0 when it is not a Parasite Eve (USA) disc. */
static unsigned Welcome_Identify(const char *path) {
    PeDisc disc = {0};
    unsigned found = 0;
    if (!Host_OpenDisc(&disc, path)) return 0;
    if (Asset_ValidatePeImage(&disc, &found)) found = 0;
    pe_disc_close(&disc);
    return found == 1 || found == 2 ? found : 0;
}

static void Welcome_ChooseDisc(int slot) {
    char path[4096];
    s_welcome.note = NULL;
    if (Host_SelectImage((unsigned)slot + 1, path, sizeof(path)) <= 0) return;
    unsigned found = Welcome_Identify(path);
    int target = found ? (int)found - 1 : slot;
    snprintf(s_welcome.disc[target], sizeof(s_welcome.disc[target]), "%s", path);
    s_welcome.disc_state[target] = found ? 1 : -1;
    s_welcome.changed[target] = 1;
    if (found && target != slot) s_welcome.note = target ? "That image is Disc 2, so it went in the Disc 2 slot."
                                                         : "That image is Disc 1, so it went in the Disc 1 slot.";
}

static void Welcome_Accept(void) {
    s_welcome.dirty = 1;
    s_welcome.note = NULL;
    if (s_welcome.step == 0) {
        if (s_welcome.row == 2) { s_welcome.quit = 1; return; }
        s_welcome.portable = s_welcome.row == 0;
        s_welcome.step = 1;
        s_welcome.row = 0;
        return;
    }
    if (s_welcome.row < 2) {
        Welcome_ChooseDisc(s_welcome.row);
        if (s_welcome.row == 0 && s_welcome.disc_state[0] > 0 && s_welcome.disc_state[1] <= 0) s_welcome.row = 1;
        else if (s_welcome.disc_state[0] > 0 && s_welcome.disc_state[1] > 0) s_welcome.row = 2;
    } else if (s_welcome.row == 2) {
        if (s_welcome.disc_state[0] > 0) s_welcome.done = 1;
        else s_welcome.note = "Choose the Disc 1 image first.";
    } else if (s_welcome.full) {
        s_welcome.step = 0;
        s_welcome.row = s_welcome.portable ? 0 : 1;
    } else {
        s_welcome.quit = 1;
    }
}

static void Welcome_Back(void) {
    s_welcome.dirty = 1;
    s_welcome.note = NULL;
    if (s_welcome.step == 1 && s_welcome.full) {
        s_welcome.step = 0;
        s_welcome.row = s_welcome.portable ? 0 : 1;
    } else {
        s_welcome.quit = 1;
    }
}

static void Welcome_Move(int direction) {
    const int rows = Welcome_RowCount();
    s_welcome.row = (s_welcome.row + rows + direction) % rows;
    s_welcome.note = NULL;
    s_welcome.dirty = 1;
}

/* The row under a window point, or -1. */
static int Welcome_HitRow(SDL_Window *window, float x, float y) {
    const float density = SDL_GetWindowPixelDensity(window);
    const OverlayLayout *layout = &s_welcome.layout;
    float dx, dy;
    if (layout->scale <= 0.0f) return -1;
    dx = (x * (density > 0.0f ? density : 1.0f) - (float)layout->x) / layout->scale;
    dy = (y * (density > 0.0f ? density : 1.0f) - (float)layout->y) / layout->scale;
    if (dx < (float)(OVERLAY_PAD / 2) || dx >= (float)(WELCOME_PANEL_W - OVERLAY_PAD / 2)) return -1;
    dy -= (float)WELCOME_HEADER_H;
    if (dy < 0.0f) return -1;
    int row = (int)(dy / (float)OVERLAY_ROW_H);
    return row < Welcome_RowCount() ? row : -1;
}

static void Welcome_Event(const SDL_Event *event, SDL_Window *window) {
    switch (event->type) {
    case SDL_EVENT_QUIT:
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        s_welcome.quit = 1;
        break;
    case SDL_EVENT_WINDOW_EXPOSED:
    case SDL_EVENT_WINDOW_RESIZED:
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        s_welcome.dirty = 1;
        break;
    case SDL_EVENT_GAMEPAD_ADDED:
        SDL_OpenGamepad(event->gdevice.which);
        break;
    case SDL_EVENT_KEY_DOWN:
        switch (event->key.scancode) {
        case SDL_SCANCODE_UP: Welcome_Move(-1); break;
        case SDL_SCANCODE_DOWN: Welcome_Move(1); break;
        case SDL_SCANCODE_RETURN: case SDL_SCANCODE_KP_ENTER: case SDL_SCANCODE_SPACE:
            if (!event->key.repeat) Welcome_Accept();
            break;
        case SDL_SCANCODE_ESCAPE: case SDL_SCANCODE_BACKSPACE:
            if (!event->key.repeat) Welcome_Back();
            break;
        default: break;
        }
        break;
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        switch (event->gbutton.button) {
        case SDL_GAMEPAD_BUTTON_DPAD_UP: Welcome_Move(-1); break;
        case SDL_GAMEPAD_BUTTON_DPAD_DOWN: Welcome_Move(1); break;
        case SDL_GAMEPAD_BUTTON_SOUTH: case SDL_GAMEPAD_BUTTON_START: Welcome_Accept(); break;
        case SDL_GAMEPAD_BUTTON_EAST: Welcome_Back(); break;
        default: break;
        }
        break;
    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
        if (event->gaxis.axis == SDL_GAMEPAD_AXIS_LEFTY) {
            const int value = event->gaxis.value;
            const int next = value > 16000 ? 1 : value < -16000 ? -1 : (value > -8000 && value < 8000) ? 0 : s_welcome.stick;
            if (next != s_welcome.stick) {
                s_welcome.stick = next;
                if (next) Welcome_Move(next);
            }
        }
        break;
    case SDL_EVENT_MOUSE_MOTION: {
        const int row = Welcome_HitRow(window, event->motion.x, event->motion.y);
        if (row >= 0 && row != s_welcome.row) { s_welcome.row = row; s_welcome.note = NULL; s_welcome.dirty = 1; }
        break;
    }
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (event->button.button == SDL_BUTTON_LEFT) {
            const int row = Welcome_HitRow(window, event->button.x, event->button.y);
            if (row >= 0) { s_welcome.row = row; Welcome_Accept(); }
        } else if (event->button.button == SDL_BUTTON_RIGHT) {
            Welcome_Back();
        }
        break;
    default:
        break;
    }
}

/* full: the storage step first (first launch). discs: the current images, or NULL.
 * 1 continue, 0 quit. Portable chosen: the userdata folder is made and portable mode applied. */
static int Host_Welcome(int full, char (*discs)[4096]) {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture = NULL;
    int texW = 0, texH = 0;
    const char *config = SDL_GetEnvironmentVariable(SDL_GetEnvironment(), "PE_CONFIG_HOME");
    const char *base = SDL_GetBasePath();

    memset(&s_welcome, 0, sizeof(s_welcome));
    s_welcome.full = full;
    s_welcome.step = full ? 0 : 1;
    s_welcome.portable = full || (config && base && !SDL_strncasecmp(config, base, strlen(base)));
    s_welcome.dirty = 1;
    for (int i = 0; discs && i < 2; ++i)
        if (*discs[i]) {
            snprintf(s_welcome.disc[i], sizeof(s_welcome.disc[i]), "%s", discs[i]);
            s_welcome.disc_state[i] = 1;
        }
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) return 1;
    window = SDL_CreateWindow("Parasite Eve - Setup", 760, 600, SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_RESIZABLE);
    if (!window) return 1;
    renderer = SDL_CreateRenderer(window, NULL);
    if (!renderer) {
        SDL_DestroyWindow(window);
        return 1;
    }
    {
        int count = 0;
        SDL_JoystickID *pads = SDL_GetGamepads(&count);
        for (int i = 0; pads && i < count; ++i) SDL_OpenGamepad(pads[i]);
        SDL_free(pads);
    }
    while (!s_welcome.done && !s_welcome.quit) {
        SDL_Event event;
        if (s_welcome.dirty) {
            s_welcome.dirty = 0;
            Welcome_Render(renderer, window, &texture, &texW, &texH);
        }
        if (SDL_WaitEventTimeout(&event, 100)) Welcome_Event(&event, window);
    }
    if (texture) SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
    if (s_welcome.quit) return 0;
    if (full && s_welcome.portable && base) {
        char path[4096];
        if (snprintf(path, sizeof(path), "%suserdata", base) < (int)sizeof(path) && SDL_CreateDirectory(path))
            Host_PortableMode();
    }
    return 1;
}

/* The welcome's disc choices into the preferences, relative to the game folder in portable mode. */
static void Host_WelcomeApplyDiscs(void) {
    const char *config = SDL_GetEnvironmentVariable(SDL_GetEnvironment(), "PE_CONFIG_HOME");
    const char *base = SDL_GetBasePath();
    const size_t length = base ? strlen(base) : 0;
    const int portable = config && length && !SDL_strncasecmp(config, base, length);
    if (!s_welcome.done) return;
    for (int i = 0; i < 2; ++i) {
        const char *path = s_welcome.disc[i];
        if (s_welcome.disc_state[i] <= 0 || !s_welcome.changed[i]) continue;
        if (portable && !SDL_strncasecmp(path, base, length)) path += length;
        snprintf(host_options.discs[i], sizeof(host_options.discs[i]), "%s", path);
    }
}

/* No preferences file yet where this launch would keep it (as Host_InitOptions finds it). */
static int Host_FirstLaunch(void) {
    const char *config = SDL_GetEnvironmentVariable(SDL_GetEnvironment(), "PE_CONFIG_HOME");
    char path[4096];
    if (config && *config) {
        snprintf(path, sizeof(path), "%s/parasite-eve.toml", config);
    } else {
#if defined(__linux__) && !defined(__ANDROID__)
        const char *root = SDL_GetEnvironmentVariable(SDL_GetEnvironment(), "XDG_CONFIG_HOME");
        if (root && *root) snprintf(path, sizeof(path), "%s/parasite-eve/parasite-eve.toml", root);
        else if ((root = SDL_GetEnvironmentVariable(SDL_GetEnvironment(), "HOME")))
            snprintf(path, sizeof(path), "%s/.config/parasite-eve/parasite-eve.toml", root);
        else return 0;
#else
        char *pref = SDL_GetPrefPath("", "Parasite Eve");
        if (!pref) return 0;
        snprintf(path, sizeof(path), "%sparasite-eve.toml", pref);
        SDL_free(pref);
#endif
    }
    return !SDL_GetPathInfo(path, NULL);
}
