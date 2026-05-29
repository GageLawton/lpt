#include "map.h"
#include <cmath>
#include <cstdio>

static SDL_Window*   s_window     = nullptr;
static SDL_Renderer* s_renderer   = nullptr;
static int           s_width      = 0;
static int           s_height     = 0;
static double        s_clat       = 0.0;
static double        s_clon       = 0.0;
static double        s_px_per_deg = 100.0;

SDL_Renderer* map_renderer()
{
    return s_renderer;
}

int map_init(int w, int h, double clat, double clon)
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "[map] SDL_Init: %s\n", SDL_GetError());
        return -1;
    }
    s_window = SDL_CreateWindow("lpt \xe2\x80\x94 ADS-B Plane Tracker", SDL_WINDOWPOS_CENTERED,
                                SDL_WINDOWPOS_CENTERED, w, h, SDL_WINDOW_SHOWN);
    if (!s_window) return -1;

    s_renderer
        = SDL_CreateRenderer(s_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!s_renderer) {
        SDL_DestroyWindow(s_window);
        s_window = nullptr;
        return -1;
    }

    s_width      = w;
    s_height     = h;
    s_clat       = clat;
    s_clon       = clon;
    s_px_per_deg = h / 10.0; // show ~10 degrees of latitude
    return 0;
}

void map_project(double lat, double lon, int* x, int* y)
{
    *x = s_width / 2 + (int)((lon - s_clon) * s_px_per_deg);
    *y = s_height / 2 - (int)((lat - s_clat) * s_px_per_deg);
}

void map_draw_background()
{
    SDL_SetRenderDrawColor(s_renderer, 0x1a, 0x1a, 0x2e, 0xFF);
    SDL_RenderClear(s_renderer);

    SDL_SetRenderDrawColor(s_renderer, 0x30, 0x30, 0x50, 0xFF);

    double lon0 = s_clon - s_width / (2.0 * s_px_per_deg) - 1.0;
    double lon1 = s_clon + s_width / (2.0 * s_px_per_deg) + 1.0;
    for (double lon = floor(lon0); lon <= lon1; lon += 1.0) {
        int x = s_width / 2 + (int)((lon - s_clon) * s_px_per_deg);
        SDL_RenderDrawLine(s_renderer, x, 0, x, s_height);
    }

    double lat0 = s_clat - s_height / (2.0 * s_px_per_deg) - 1.0;
    double lat1 = s_clat + s_height / (2.0 * s_px_per_deg) + 1.0;
    for (double lat = floor(lat0); lat <= lat1; lat += 1.0) {
        int y = s_height / 2 - (int)((lat - s_clat) * s_px_per_deg);
        SDL_RenderDrawLine(s_renderer, 0, y, s_width, y);
    }
}

void map_draw_range_rings(float ring_spacing_nm)
{
    SDL_SetRenderDrawColor(s_renderer, 0x40, 0x40, 0x60, 0xFF);
    int cx = s_width / 2;
    int cy = s_height / 2;

    for (int ring = 1; ring <= 3; ring++) {
        double radius_deg = ring * ring_spacing_nm / 60.0;
        int    r_px       = (int)(radius_deg * s_px_per_deg);
        for (int deg = 0; deg < 360; deg++) {
            double a0 = deg * M_PI / 180.0;
            double a1 = (deg + 1) * M_PI / 180.0;
            SDL_RenderDrawLine(s_renderer, cx + (int)(r_px * cos(a0)), cy + (int)(r_px * sin(a0)),
                               cx + (int)(r_px * cos(a1)), cy + (int)(r_px * sin(a1)));
        }
    }
}

bool map_present()
{
    SDL_RenderPresent(s_renderer);
    SDL_Event ev;
    while (SDL_PollEvent(&ev))
        if (ev.type == SDL_QUIT) return false;
    return true;
}

void map_close()
{
    if (s_renderer) {
        SDL_DestroyRenderer(s_renderer);
        s_renderer = nullptr;
    }
    if (s_window) {
        SDL_DestroyWindow(s_window);
        s_window = nullptr;
    }
    SDL_Quit();
}
