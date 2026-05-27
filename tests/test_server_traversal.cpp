// Integration test for the static file handler's path traversal guard.
// Spins up a real httplib::Server on localhost, issues crafted GET requests,
// and asserts the correct HTTP status codes.

#include <cassert>
#include <cstdio>
#include <string>
#include <thread>
#include "httplib.h"

static const int PORT = 19876;

static void register_handlers(httplib::Server& svr)
{
    // Mirror the static file handler from src/web/server.cpp
    svr.Get(R"(/(.+))", [](const httplib::Request& req, httplib::Response& res) {
        std::string path = std::string("web/") + req.matches[1].str();
        if (path.find("..") != std::string::npos) {
            res.status = 400;
            return;
        }
        res.set_content("ok", "text/plain");
    });
}

int main()
{
    httplib::Server svr;
    register_handlers(svr);

    // Bind first so wait_until_ready() is race-free
    assert(svr.bind_to_port("127.0.0.1", PORT));
    std::thread server_thread([&]() { svr.listen_after_bind(); });
    svr.wait_until_ready();

    httplib::Client cli("127.0.0.1", PORT);
    cli.set_connection_timeout(2);
    cli.set_read_timeout(2);

    // 1. Normal path — must return 200
    {
        auto r = cli.Get("/index.html");
        assert(r && r->status == 200);
    }

    // 2. Simple traversal — must return 400
    {
        auto r = cli.Get("/../../etc/passwd");
        assert(r && r->status == 400);
    }

    // 3. Embedded traversal — must return 400
    {
        auto r = cli.Get("/assets/../../../etc/shadow");
        assert(r && r->status == 400);
    }

    // 4. Dot-dot at end of segment — must return 400
    {
        auto r = cli.Get("/web/..");
        assert(r && r->status == 400);
    }

    svr.stop();
    server_thread.join();

    printf("test_server_traversal: all tests passed\n");
    return 0;
}
