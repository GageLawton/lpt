#include "aircraft.h"
#include "map.h"
#include <SDL2/SDL.h>
#include <cstdio>
#include <cmath>

void aircraft_draw(const Aircraft* ac)
{
    if (!ac->position_valid) return;

    SDL_Renderer* r = map_renderer();
    int           x, y;
    map_project(ac->lat, ac->lon, &x, &y);

    // Filled circle radius 4 in #00ff88
    SDL_SetRenderDrawColor(r, 0x00, 0xFF, 0x88, 0xFF);
    for (int dy = -4; dy <= 4; dy++)
        for (int dx = -4; dx <= 4; dx++)
            if (dx * dx + dy * dy <= 16) SDL_RenderDrawPoint(r, x + dx, y + dy);

    // Callsign label 6 px to the right — requires TTF font wired up in map_init;
    // see issue #13 for full SDL2_ttf integration
    if (ac->callsign[0]) {
        // placeholder: white tick mark at label origin until font is wired up
        SDL_SetRenderDrawColor(r, 0xFF, 0xFF, 0xFF, 0xFF);
        SDL_RenderDrawLine(r, x + 6, y - 4, x + 6, y + 4);
    }

    // Altitude indicator in grey below the dot
    char alt_str[16];
    snprintf(alt_str, sizeof(alt_str), "%d ft", ac->altitude_ft);
    (void)alt_str; // rendered once TTF is wired up
    SDL_SetRenderDrawColor(r, 0x88, 0x88, 0x88, 0xFF);
    SDL_RenderDrawPoint(r, x + 6, y + 10);
}

void aircraft_draw_vector(const Aircraft* ac)
{
    if (!ac->position_valid || ac->groundspeed_kt <= 0) return;

    SDL_Renderer* r = map_renderer();
    int           x, y;
    map_project(ac->lat, ac->lon, &x, &y);

    float len = fminf(ac->groundspeed_kt, 60.0f);
    float rad = ac->heading_deg * 3.14159265f / 180.0f;
    int   x2  = x + (int)(len * sinf(rad));
    int   y2  = y - (int)(len * cosf(rad));

    SDL_SetRenderDrawColor(r, 0x00, 0xFF, 0x88, 0x99);
    SDL_RenderDrawLine(r, x, y, x2, y2);
}
