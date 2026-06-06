#include "ui/file_picker.h"
#include <cstdio>
#include <memory>
#include <array>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commdlg.h>
#else
#include <cstdlib>
#endif

namespace ghostpad::ui {

#ifndef _WIN32
static std::string execCommand(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    FILE* pipe = popen(cmd, "r");
    if (!pipe) {
        return "";
    }
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }
    pclose(pipe);
    if (!result.empty() && result.back() == '\n') {
        result.pop_back();
    }
    return result;
}
#endif

std::string pickFile(const std::string& title, const std::string& filter_desc, const std::string& filter_ext) {
#ifdef _WIN32
    OPENFILENAMEA ofn;
    char szFile[260] = {0};

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    
    // Construct the filter string in the format "Description\0*.ext\0All Files\0*.*\0"
    std::string filter_str = filter_desc + " (" + filter_ext + ")\0" + filter_ext + "\0All Files (*.*)\0*.*\0";
    ofn.lpstrFilter = filter_str.c_str();
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.lpstrTitle = title.c_str();
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameA(&ofn) == TRUE) {
        return ofn.lpstrFile;
    }
    return "";
#elif defined(__APPLE__)
    (void)filter_desc;
    (void)filter_ext;
    // Use AppleScript to pick a file
    std::string cmd = "osascript -e 'POSIX path of (choose file with prompt \"" + title + "\")' 2>/dev/null";
    return execCommand(cmd.c_str());
#else
    (void)filter_desc;
    (void)filter_ext;
    // Fallback for generic Linux / other systems (zenity if available, or empty)
    std::string cmd = "zenity --file-selection --title=\"" + title + "\" 2>/dev/null";
    return execCommand(cmd.c_str());
#endif
}

} // namespace ghostpad::ui
