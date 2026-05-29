#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>

#include "web/server_utils.h"

int main()
{
    assert(json_escape("hello") == "hello");
    assert(json_escape("say \"hi\"") == "say \\\"hi\\\"");
    assert(json_escape("a\\b") == "a\\\\b");
    assert(json_escape("") == "");

    Aircraft ac{};
    ac.icao = 0xABCDEF;
    std::strncpy(ac.callsign, "A\"B\\C", sizeof(ac.callsign) - 1);
    std::string aircraft_json = aircraft_to_json(&ac, 0);
    assert(aircraft_json.find("\"cs\":\"A\\\"B\\\\C\"") != std::string::npos);

    ac.position_valid = true;
    ac.trail[0] = { 37.6213, -122.3790 };
    ac.trail[1] = { 37.6220, -122.3800 };
    ac.trail_len = 2;
    ac.trail_head = 2;
    std::string trail_json = aircraft_to_json(&ac, 0);
    assert(trail_json.find(
        "\"trail\":[{\"lat\":37.621300,\"lon\":-122.379000},"
        "{\"lat\":37.622000,\"lon\":-122.380000}]") != std::string::npos);

    Aircraft ac_empty{};
    ac_empty.icao = 1;
    assert(aircraft_to_json(&ac_empty, 0).find("\"trail\":[") != std::string::npos);

    assert(mime_for("index.html") == "text/html");
    assert(mime_for("style.css") == "text/css");
    assert(mime_for("app.js") == "application/javascript");
    assert(mime_for("app.jsx") == "application/javascript");
    assert(mime_for("data.bin") == "application/octet-stream");
    assert(mime_for("noext") == "application/octet-stream");
    assert(mime_for("web/scope-classic.jsx") == "application/javascript");

    printf("test_server_utils: all tests passed\n");
    return 0;
}
