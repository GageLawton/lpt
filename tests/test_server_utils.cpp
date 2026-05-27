#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>

static std::string json_escape(const std::string& s)
{
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == '"')       out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else                out += c;
    }
    return out;
}

static std::string mime_for(const std::string& path)
{
    auto ends = [&](const char* s) {
        size_t pl = path.size(), sl = strlen(s);
        return pl >= sl && path.compare(pl - sl, sl, s) == 0;
    };
    if (ends(".html")) return "text/html";
    if (ends(".css"))  return "text/css";
    if (ends(".js"))   return "application/javascript";
    if (ends(".jsx"))  return "application/javascript";
    return "application/octet-stream";
}

int main()
{
    assert(json_escape("hello") == "hello");
    assert(json_escape("say \"hi\"") == "say \\\"hi\\\"");
    assert(json_escape("a\\b") == "a\\\\b");
    assert(json_escape("") == "");

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
