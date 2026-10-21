// SPDX-License-Identifier: GPL-2.0-or-later
#include "bbport_settings.h"
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <windows.h>

// AMD ML-FG is the third frame generation backend (fg_backend=fsr401, 2x only). A failed probe
// keeps the saved choice: the presenter uses FSR 3.1 for that session only.
int main() {
    using namespace BbSettings;
    wchar_t path[MAX_PATH]{};
    assert(GetTempFileNameW(std::filesystem::temp_directory_path().c_str(), L"bbf", 0, path));
    _wputenv_s(L"BB_CONFIG", path);
    for (const char* env : {"BB_FG_BACKEND", "BB_FRAME_GENERATION", "BB_REFLEX"})
        _putenv_s(env, "");
    {
        std::ofstream file{std::filesystem::path(path)};
        file << "frame_generation=4x\nfg_backend=fsr401\n";
    }
    auto& s = Get();
    Load();
    assert(s.fg_backend == FgFsr401 && s.frame_generation == 2); // ML-FG makes one frame
    assert(std::string(FgBackendName(FgFsr401)) == "fsr401");
    for (const char* reason : {"ML-FG file missing: test.dll", "ML-FG support check failed.",
                              "ML-FG requires Windows 11.", "ML-FG context creation failed."}) {
        ConfigureFg401Support(false, reason);
        assert(!s.fg401_supported && std::string(s.fg401_problem.load()) == reason);
        assert(s.fg_backend == FgFsr401);
        Save();
        std::ifstream file{std::filesystem::path(path)};
        const std::string text((std::istreambuf_iterator<char>(file)), {});
        assert(text.find("fg_backend=fsr401\n") != std::string::npos);
        assert(text.find("frame_generation=2x\n") != std::string::npos);
        Load();
        assert(s.fg_backend == FgFsr401 && s.frame_generation == 2);
    }
    ConfigureFg401Support(true, nullptr);
    assert(s.fg401_supported && !s.fg401_problem.load());
    // A choice made in the menu applies on the next start.
    s.fg_backend = FgDlss;
    assert(FrameGenNeedsRestart());
    s.fg_backend = FgFsr401;
    assert(!FrameGenNeedsRestart());
    _putenv_s("BB_FG_BACKEND", "2");
    Load();
    assert(s.fg_backend == FgFsr401);
    _putenv_s("BB_FG_BACKEND", "fsr");
    Load();
    assert(s.fg_backend == FgFsr);
    _putenv_s("BB_FG_BACKEND", "");
    std::filesystem::remove(path);
    std::puts("ML-FG backend survives probe failures, saves and restarts; 2x only PASS");
}
