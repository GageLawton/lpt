#pragma once
#include <cstdint>
#include <SDL2/SDL.h>

// Initialize SDL2 window and renderer
SDL_Renderer* map_renderer();

// Returns 0 on success
int map_init(int width, int height, double center_lat, double center_lon);

// Convert lat/lon to screen pixel coordinates
void map_project(double lat, double lon, int* x, int* y);

// Draw the map background (grid or tile)
void map_draw_background();

// Draw range rings at intervals of ring_spacing_nm nautical miles
void map_draw_range_rings(float ring_spacing_nm);

// Present the rendered frame and handle window events
// Returns false if the user closed the window
bool map_present();

// Tear down SDL2
void map_close();
