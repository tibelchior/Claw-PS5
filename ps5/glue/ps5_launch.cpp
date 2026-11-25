// Launch glue for the installed title. The console mounts the title folder as /app0:
//   /app0/Claw/  the PC game's files (CLAW.REZ, ASSETS.ZIP, fonts), read only
//   /app0/data/              everything the port writes: ini files, claw.log (the one log: launcher, engine, renderer,
//                            presenter, controller, sound; the previous run is claw.prev.log), saves, cache
// If /app0/data cannot be written, /data/claw is used instead. The engine runs with that folder's System/ as its working
// directory; its ini Paths point at the read-only game folder (see tools/container/title.sh).
#include <cerrno>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <exception>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>
#include <cctype>
#include <system_error>

#include <SDL.h>

#include <fcntl.h>
#include <pthread.h>
#include <ucontext.h>
#include <sys/stat.h>
#include <unistd.h>

extern "C" int __real_main(int argc, char **argv);
extern "C" int sceSystemServiceLoadExec(const char *path, char *const argv[]);
extern "C" void ut_ps5_log(const char *text);
extern "C" void ut_ps5_input_start(void);
extern "C" const char *ut_ps5_game_dir(void);

namespace {
std::string g_data = "/app0/data";

bool writable(const std::string &dir) {
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    const std::string probe = dir + "/.write-test";
    if (FILE *f = std::fopen(probe.c_str(), "w")) {
        std::fclose(f);
        std::remove(probe.c_str());
        return true;
    }
    return false;
}


// Sets key=value in [section] of an ini when the key is missing or empty (a value the player set is kept).
void ini_default(const std::string &path, const char *section, const char *key, const char *value, bool force = false) {
    std::string text;
    if (FILE *f = std::fopen(path.c_str(), "rb")) {
        char buf[8192];
        size_t n;
        while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) text.append(buf, n);
        std::fclose(f);
    }
    const bool crlf = text.find("\r\n") != std::string::npos;
    std::vector<std::string> lines;
    for (size_t pos = 0; pos < text.size();) {
        size_t end = text.find('\n', pos);
        if (end == std::string::npos) end = text.size();
        std::string line = text.substr(pos, end - pos);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(line);
        pos = end + 1;
    }
    const auto lower = [](std::string v) { for (char &ch : v) ch = char(std::tolower((unsigned char)ch)); return v; };
    const auto trim = [](std::string v) {
        while (!v.empty() && (v.back() == ' ' || v.back() == '\t')) v.pop_back();
        size_t a = 0;
        while (a < v.size() && (v[a] == ' ' || v[a] == '\t')) a++;
        return v.substr(a);
    };
    const std::string header = lower(std::string("[") + section + "]");
    const std::string prefix = lower(std::string(key) + "=");
    const std::string entry = std::string(key) + "=" + value;
    size_t at = lines.size();
    bool changed = false;
    for (size_t i = 0; i < lines.size(); i++)
        if (lower(trim(lines[i])) == header) { at = i; break; }
    if (at == lines.size()) {
        if (!lines.empty() && !lines.back().empty()) lines.push_back("");
        lines.push_back(std::string("[") + section + "]");
        lines.push_back(entry);
        changed = true;
    } else {
        size_t j = at + 1;
        bool found = false;
        for (; j < lines.size() && trim(lines[j]).rfind("[", 0) != 0; j++) {
            const std::string t = lower(trim(lines[j]));
            if (t.rfind(prefix, 0) == 0) {
                found = true;
                if (force || trim(lines[j].substr(lines[j].find('=') + 1)).empty()) { if (lines[j] != entry) { lines[j] = entry; changed = true; } }
                break;
            }
        }
        if (!found) { lines.insert(lines.begin() + long(at) + 1, entry); changed = true; }
    }
    if (!changed) return;
    std::string out;
    for (const std::string &l : lines) out += l + (crlf ? "\r\n" : "\n");
    if (FILE *f = std::fopen(path.c_str(), "wb")) {
        std::fwrite(out.data(), 1, out.size(), f);
        std::fclose(f);
        std::fprintf(stderr, "[ps5-launch] ini: set [%s] %s\n", section, entry.c_str());
    } else {
        std::fprintf(stderr, "[ps5-launch] ERROR: cannot write %s: errno %d\n", path.c_str(), errno);
    }
}

int g_copied = 0, g_copy_failed = 0;

// Plain stdio copy: std::filesystem::copy_file needs system calls the console may not offer.
void copy_if_missing(const std::string &from, const std::string &to) {
    std::error_code ec;
    if (std::filesystem::exists(to, ec)) return;
    FILE *in = std::fopen(from.c_str(), "rb");
    if (!in) {
        std::fprintf(stderr, "[ps5-launch] ERROR: cannot read %s: errno %d\n", from.c_str(), errno);
        g_copy_failed++;
        return;
    }
    FILE *out = std::fopen(to.c_str(), "wb");
    if (!out) {
        std::fprintf(stderr, "[ps5-launch] ERROR: cannot write %s: errno %d\n", to.c_str(), errno);
        std::fclose(in);
        g_copy_failed++;
        return;
    }
    char buf[65536];
    size_t n;
    bool ok = true;
    while ((n = std::fread(buf, 1, sizeof buf, in)) > 0)
        if (std::fwrite(buf, 1, n, out) != n) { ok = false; break; }
    std::fclose(in);
    if (std::fclose(out) != 0) ok = false;
    if (ok) g_copied++;
    else {
        std::fprintf(stderr, "[ps5-launch] ERROR: copy to %s failed: errno %d\n", to.c_str(), errno);
        std::remove(to.c_str());
        g_copy_failed++;
    }
}

// One log for everything. stdout and stderr (engine log lines, glue notes, library messages) are
// appended to <data>/claw.log from the first instruction on; the previous run is kept as claw.prev.log.
void open_log() {
    const std::string log = g_data + "/claw.log";
    std::rename(log.c_str(), (g_data + "/claw.prev.log").c_str());
    // A title's stderr is not a file descriptor to dup2 onto (that left the log empty): reopen the
    // streams themselves on the file, line buffered so a crash keeps everything before it.
    if (!std::freopen(log.c_str(), "a", stderr)) return;
    setvbuf(stderr, nullptr, _IOLBF, 0);
    if (std::freopen(log.c_str(), "a", stdout)) setvbuf(stdout, nullptr, _IOLBF, 0);
    setenv("CLAW_LOG_STDERR", "1", 1);
}

void crash(int sig, siginfo_t *info, void *ctx) {
    char text[256];
    const ucontext_t *uc = static_cast<const ucontext_t *>(ctx);
    // rip - base of a known function = offset to look up in the linked eboot.elf (nm / addr2line).
    const int n = std::snprintf(text, sizeof text,
        "\n[ps5-launch] CRASH: signal %d, fault address %p, code %d, rip %p, rsp %p, ref (ut_ps5_game_dir) %p\n", sig,
        info ? info->si_addr : nullptr, info ? info->si_code : 0, uc ? (void *)uc->uc_mcontext.mc_rip : nullptr,
        uc ? (void *)uc->uc_mcontext.mc_rsp : nullptr, (void *)&ut_ps5_game_dir);
    if (n > 0) (void)!write(fileno(stderr), text, size_t(n));
    // Words at the top of the stack: the return address of the bad call is among the first (look them up
    // with nm / addr2line in the linked eboot.elf; code lives at 0x400000 and up).
    if (uc) {
        const unsigned long *sp = reinterpret_cast<const unsigned long *>(uc->uc_mcontext.mc_rsp);
        for (int i = 0; i < 48; i++) {
            const int m = std::snprintf(text, sizeof text, "[ps5-launch] stack[%d] %lx\n", i, sp[i]);
            if (m > 0) (void)!write(fileno(stderr), text, size_t(m));
        }
    }
    signal(sig, SIG_DFL);
    raise(sig);
}

void install_crash_handlers() {
    struct sigaction sa = {};
    sa.sa_sigaction = crash;
    sa.sa_flags = SA_SIGINFO;
    for (int sig : {SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT}) sigaction(sig, &sa, nullptr);
    std::set_terminate([] {
        std::fprintf(stderr, "[ps5-launch] CRASH: uncaught C++ exception, terminating\n");
        std::abort();
    });
}
}  // namespace

// The console's getcwd() jumps to a bad address (crash at the engine's first call), and chdir() into the
// writable folder is refused. The title has no working directory to report, so the link wraps getcwd
// (--wrap=getcwd): it answers with the engine's base directory (data folder + /System).
extern "C" char *__wrap_getcwd(char *buf, size_t size) {
    std::string dir = g_data + "/System";
    if (!buf) {
        buf = static_cast<char *>(std::malloc(dir.size() + 1));
        if (!buf) return nullptr;
    } else if (size < dir.size() + 1) {
        errno = ERANGE;
        return nullptr;
    }
    std::memcpy(buf, dir.c_str(), dir.size() + 1);
    return buf;
}

extern "C" const char *ut_ps5_game_dir(void) { return "/app0/Claw"; }
extern "C" const char *ut_ps5_data_dir(void) { return g_data.c_str(); }

// OpenClaw ends with exit(0) from inside its main loop teardown (and exit(1) on fatal errors): ending the process that way makes
// the console show an error screen. Every exit, from anywhere, asks the system to close the title instead (like a normal quit).
extern "C" void __wrap_exit(int code) {
    ut_ps5_log("[ps5-launch] exit requested, asking the system to close the title");
    std::fflush(nullptr);
    usleep(300 * 1000);
    const int rc = sceSystemServiceLoadExec("exit", nullptr);
    std::fprintf(stderr, "[ps5] the system did not close the title: %d\n", rc);
    usleep(1500 * 1000);
    _exit(code);
}

// The title's entry point (app_crt.cpp calls "main", the link wraps it).
extern "C" int __wrap_main(int argc, char **argv) {
    if (!writable(g_data)) g_data = "/data/claw";
    std::error_code ec;
    std::filesystem::create_directories(g_data, ec);
    open_log();
    install_crash_handlers();
    const std::time_t now = std::time(nullptr);
    std::fprintf(stderr, "[ps5-launch] Captain Claw (OpenClaw) PS5 starting, %s", std::ctime(&now));
    std::fprintf(stderr, "[ps5-launch] data folder %s, game folder %s\n", g_data.c_str(), ut_ps5_game_dir());
    for (const char *f : {"CLAW.REZ", "ASSETS.ZIP", "clacon.ttf", "console02.tga"})
        std::fprintf(stderr, "[ps5-launch] /app0/Claw/%s %s\n", f,
                     std::filesystem::exists(std::string("/app0/Claw/") + f, ec) ? "present" : "MISSING");
    // OpenClaw reads <HOME>/.config/openclaw/config.xml (and writes its saves next to it) when there is no config.xml in the
    // working directory. The console refuses chdir, so HOME points into the writable data folder; the first-run config
    // shipped in the title (port/config.xml: AssetsFolder /app0/Claw/, software renderer, 16:9) is copied there.
    const std::string home = g_data + "/home";
    const std::string cfgdir = home + "/.config/openclaw";
    std::filesystem::create_directories(cfgdir, ec);
    std::filesystem::create_directories(g_data + "/tmp", ec);
    // The game never writes its config (options pages are not implemented upstream), so the shipped one always wins.
    std::remove((cfgdir + "/config.xml").c_str());
    copy_if_missing("/app0/port/config.xml", cfgdir + "/config.xml");
    copy_if_missing("/app0/Claw/SAVES.XML", cfgdir + "/SAVES.XML");
    // Test helper: <data>/debug.txt. "level N" starts straight in level N (no menu); every other line is an OpenClaw console
    // command run at level start (invincible on, infinitejump on, catnip, firesword, teleport X Y, ...). Delete the file to play normally.
    {
        const std::string dbg = g_data + "/debug.txt";
        std::string level, commands;
        if (FILE *f = std::fopen(dbg.c_str(), "r")) {
            char line[256];
            while (std::fgets(line, sizeof line, f)) {
                std::string l(line);
                while (!l.empty() && (l.back() == '\n' || l.back() == '\r' || l.back() == ' ')) l.pop_back();
                if (l.empty() || l[0] == '#') continue;
                if (l.compare(0, 6, "level ") == 0) level = l.substr(6);
                else commands += l + "\n";
            }
            std::fclose(f);
            std::fprintf(stderr, "[ps5-launch] debug.txt: level '%s', %zu bytes of commands\n", level.c_str(), commands.size());
        }
        // There is no level 14 in OpenClaw: asking for it shows the ending videos (movies.txt "end" lines) and the menu.
        if (level == "14") {
            std::fprintf(stderr, "[ps5-launch] debug.txt: level 14 = play the ending videos\n");
            setenv("CLAW_PLAY_ENDING", "1", 1);
            level.clear();
            commands.clear();
        }
        if (!level.empty()) {
            std::string cfg;
            if (FILE *f = std::fopen((cfgdir + "/config.xml").c_str(), "rb")) {
                char buf[4096]; size_t n;
                while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) cfg.append(buf, n);
                std::fclose(f);
            }
            auto set = [&](const char *open, const char *close, const std::string &value) {
                const size_t a = cfg.find(open);
                if (a == std::string::npos) return;
                const size_t b = cfg.find(close, a);
                if (b == std::string::npos) return;
                cfg.replace(a + std::strlen(open), b - a - std::strlen(open), value);
            };
            set("<SkipMenu>", "</SkipMenu>", "true");
            set("<SkipMenuToLevel>", "</SkipMenuToLevel>", level);
            set("<LoadAllLevelSaves>", "</LoadAllLevelSaves>", "true");   // saves for levels not unlocked yet (else the level has no start data)
            if (FILE *f = std::fopen((cfgdir + "/config.xml").c_str(), "wb")) { std::fwrite(cfg.data(), 1, cfg.size(), f); std::fclose(f); }
        }
        if (!commands.empty()) {
            const std::string cmdfile = g_data + "/debug_commands.txt";
            if (FILE *f = std::fopen(cmdfile.c_str(), "wb")) { std::fwrite(commands.data(), 1, commands.size(), f); std::fclose(f); }
            setenv("CLAW_STARTUP_COMMANDS", cmdfile.c_str(), 1);
        }
    }
    setenv("HOME", home.c_str(), 1);
    // The console has no window system: SDL's dummy drivers, with frames and sound routed by the patched SDL
    // (ps5_present.cpp, ps5_io.cpp). Set as hints: SDL keeps its own copy of the environment.
    SDL_SetHint(SDL_HINT_VIDEODRIVER, "dummy");
    SDL_SetHint(SDL_HINT_AUDIODRIVER, "dummy");
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
    if (std::filesystem::exists(g_data + "/quiet", ec)) setenv("CLAW_QUIET", "1", 1);
    ut_ps5_input_start();
    std::fprintf(stderr, "[ps5-launch] entering the engine\n");
    const int code = __real_main(argc, argv);
    // Ending the process from inside makes the console report an error; asking the system to close the title
    // is how a game quits.
    ut_ps5_log("[ps5-launch] game finished, asking the system to close the title");
    std::fflush(nullptr);
    usleep(500 * 1000);
    const int rc = sceSystemServiceLoadExec("exit", nullptr);
    std::fprintf(stderr, "[ps5] the system did not close the title: %d\n", rc);
    usleep(1500 * 1000);
    _exit(code);
}
