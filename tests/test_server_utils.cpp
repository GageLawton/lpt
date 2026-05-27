#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>

#include "../src/web/server.cpp"

int main()
{
    assert(json_escape("hello") == "hello");
    assert(json_escape("say \"hi\"") == "say \\\"hi\\\"");
    assert(json_escape("a\\b") == "a\\\\b");
    assert(json_escape("") == "");

    Aircraft ac{};
    ac.icao = 0xABCDEF;
    std::strncpy(ac.callsign, "A\"B\\C", sizeof(ac.callsign) - 1);
    std::string aircraft_json = aircraft_to_json(&ac);
    assert(aircraft_json.find("\"cs\":\"A\\\"B\\\\C\"") != std::string::npos);

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
