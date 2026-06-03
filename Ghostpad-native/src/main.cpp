// Ghostpad Native - PS5 Remote Controller
// Copyright (c) 2026  seregonwar
// Based on original Ghostpad by stonedmodder  
// Licensed under the GNU General Public License v3.0. See LICENSE file for details.

#include "ui/app.h"
#include <cstdio>
#include <cstdlib>
#include <sys/stat.h>
#include <unistd.h>
#include <libgen.h>
#include <string>
#include <GLFW/glfw3.h>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

static std::string getDefaultDataDir() {
    const char* home = getenv("HOME");
    if (!home) home = getenv("USERPROFILE");
    if (!home) home = ".";

    std::string dir = std::string(home) + "/.ghostpad";
    mkdir(dir.c_str(), 0755);
    return dir;
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    std::string data_dir = getDefaultDataDir();
    fprintf(stderr, "Ghostpad Native - Data directory: %s\n", data_dir.c_str());

    ghostpad::App app(data_dir);
    app.init();

    double last_time = glfwGetTime();

    while (!app.should_close) {
        double current_time = glfwGetTime();
        double dt = current_time - last_time;
        last_time = current_time;

        // Cap dt to avoid spiral of death
        if (dt > 0.1) dt = 0.1;
        if (dt <= 0.0) dt = 1.0 / 60.0;

        app.update(dt);
        app.render();
    }

    app.shutdown();
    return 0;
}
