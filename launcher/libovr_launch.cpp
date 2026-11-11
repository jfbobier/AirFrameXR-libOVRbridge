// MIT License. Copyright (c) 2026 Jean-Francois Bobier.
// The patch table (offsets and bytes for Lone Echo II build 0x61578052) comes from le2-mod-engine, MIT, Copyright (c) 2026 kotorvr:
// https://github.com/kotorvr/le2-mod-engine (patches/openxr_runtime.json, perf_streaming.json, perf_streaming_aggressive.json).
//
// libovr_launch: starts a libovr game with the AirFrameXR libOVR bridge in place of the Oculus runtime.
//   libovr_launch.exe [--bridge-dir DIR] [--texture-budget 2|4] <game.exe> [game arguments...]
//
// What it does, in order (nothing is written to the game folder):
//   1. LIBOVR_DLL_DIR = the bridge folder (default: <this exe's folder>\bin): the game's libovr loader looks there first for LibOVRRT64_1.dll.
//   2. The game starts suspended. The loader inside the game refuses a LibOVRRT64_1.dll that has no Oculus signature, so the branch of that check is
//      patched in memory (a 4-byte edit), before the game runs a single instruction. The patch table is keyed on the exe's build (PE timestamp);
//      an exe that is not in the table is started untouched with a warning.
//   3. Optional --texture-budget 2|4: Lone Echo II's texture cache budget (768 MiB / 1.5 GiB / 3 GiB by the Texture quality setting) times 2 or 4,
//      the mip-requirement sweep every frame (4: also mip upgrades after 1 s instead of 5 s). Only for GPUs with 12 GB (x2) or 16 GB (x4) or more.
//   4. The game resumes. The launcher exits with 0 once it is running (2 when it could not start the game).
// Log: libovr_launch.log next to this exe.
#include <windows.h>
#include <winternl.h>
#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <string>
#include <vector>

namespace {
FILE* g_log = nullptr;
void Log(const char* fmt, ...) {
    char line[1024];
    va_list a;
    va_start(a, fmt);
    vsnprintf(line, sizeof(line), fmt, a);
    va_end(a);
    puts(line);
    if (g_log) {
        fprintf(g_log, "%s\n", line);
        fflush(g_log);
    }
}

struct Patch {
    uint32_t rva;
    std::vector<uint8_t> expect, replace;
    const char* what;
};
struct Profile {
    uint32_t timestamp;
    const char* name;
    std::vector<Patch> signature;   // always applied: the libovr loader accepts the unsigned bridge
    std::vector<Patch> budget2;     // --texture-budget 2
    std::vector<Patch> budget4;     // --texture-budget 4
};

const std::vector<Profile>& Profiles() {
    static const std::vector<Profile> p = {
        {0x61578052, "Lone Echo II 1.1.561021.2 (final Oculus build)",
         {{0x117f431, {0x85, 0xc0, 0x75, 0x11}, {0x90, 0x90, 0xeb, 0x11}, "libovr loader: do not refuse a runtime DLL whose signature check failed"}},
         {{0x5998e0, {0xb8, 0x00, 0x00, 0x00, 0x02, 0x48, 0x3b, 0xc8, 0x48, 0x0f, 0x43, 0xc1}, {0x48, 0x8d, 0x04, 0x4d, 0, 0, 0, 0, 0x90, 0x90, 0x90, 0x90}, "texture budget x2"},
          {0x569433, {0xf3, 0x0f, 0x10, 0x15, 0x19, 0xc8, 0xf7, 0x00}, {0xf3, 0x0f, 0x10, 0x15, 0x39, 0x5d, 0xf7, 0x00}, "mip requirements: half the objects per frame"}},
         {{0x5998e0, {0xb8, 0x00, 0x00, 0x00, 0x02, 0x48, 0x3b, 0xc8, 0x48, 0x0f, 0x43, 0xc1}, {0x48, 0x8d, 0x04, 0x8d, 0, 0, 0, 0, 0x90, 0x90, 0x90, 0x90}, "texture budget x4"},
          {0x569433, {0xf3, 0x0f, 0x10, 0x15, 0x19, 0xc8, 0xf7, 0x00}, {0xf3, 0x0f, 0x10, 0x15, 0x3d, 0x5d, 0xf7, 0x00}, "mip requirements: every object every frame"},
          {0x1e54d48, {0x88, 0x13, 0, 0, 0, 0, 0, 0}, {0xe8, 0x03, 0, 0, 0, 0, 0, 0}, "mip upgrades resume after 1 s instead of 5 s"}}},
        {0x59dacac4, "Arktika.1 (D3D11)",
         {{0x86c2bd, {0x74, 0x2c}, {0xeb, 0x2c}, "libovr loader: accept the runtime DLL even when its signature check did not pass (je -> jmp after cmp esi,1)"}},
         {}, {}},
    };
    return p;
}

std::wstring DirOf(const std::wstring& path) {
    size_t k = path.find_last_of(L"\\/");
    return k == std::wstring::npos ? L"." : path.substr(0, k);
}

bool ReadTimestamp(const std::wstring& exe, uint32_t* ts) {
    HANDLE f = CreateFileW(exe.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    uint8_t head[4096];
    DWORD got = 0;
    bool ok = ReadFile(f, head, sizeof(head), &got, nullptr) && got >= sizeof(IMAGE_DOS_HEADER);
    CloseHandle(f);
    if (!ok) return false;
    auto* dos = (IMAGE_DOS_HEADER*)head;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || (size_t)dos->e_lfanew + sizeof(IMAGE_NT_HEADERS64) > got) return false;
    auto* nt = (IMAGE_NT_HEADERS64*)(head + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return false;
    *ts = nt->FileHeader.TimeDateStamp;
    return true;
}

uintptr_t RemoteImageBase(HANDLE process) {
    using NtQueryFn = LONG(NTAPI*)(HANDLE, int, PVOID, ULONG, PULONG);
    auto query = (NtQueryFn)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtQueryInformationProcess");
    if (!query) return 0;
    PROCESS_BASIC_INFORMATION pbi{};
    if (query(process, 0 /*ProcessBasicInformation*/, &pbi, sizeof(pbi), nullptr) != 0 || !pbi.PebBaseAddress) return 0;
    uintptr_t base = 0;
    if (!ReadProcessMemory(process, (uint8_t*)pbi.PebBaseAddress + 0x10 /*PEB.ImageBaseAddress, 64-bit*/, &base, sizeof(base), nullptr)) return 0;
    return base;
}

// 1 applied, 0 the bytes are not what this build has (left alone), -1 error
int Apply(HANDLE process, uintptr_t base, const Patch& p) {
    uint8_t* at = (uint8_t*)base + p.rva;
    std::vector<uint8_t> now(p.expect.size());
    if (!ReadProcessMemory(process, at, now.data(), now.size(), nullptr)) return -1;
    if (now != p.expect) {
        Log("  skipped (bytes differ from the expected ones): %s", p.what);
        return 0;
    }
    DWORD old = 0;
    if (!VirtualProtectEx(process, at, p.replace.size(), PAGE_EXECUTE_READWRITE, &old)) return -1;
    SIZE_T wrote = 0;
    bool ok = WriteProcessMemory(process, at, p.replace.data(), p.replace.size(), &wrote) && wrote == p.replace.size();
    DWORD tmp = 0;
    VirtualProtectEx(process, at, p.replace.size(), old, &tmp);
    FlushInstructionCache(process, at, p.replace.size());
    if (!ok) return -1;
    Log("  applied: %s", p.what);
    return 1;
}
} // namespace

int wmain(int argc, wchar_t** argv) {
    wchar_t self[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, self, MAX_PATH);
    const std::wstring here = DirOf(self);
    _wfopen_s(&g_log, (here + L"\\libovr_launch.log").c_str(), L"w");

    std::wstring bridge = here + L"\\bin";
    int budget = 0, i = 1;
    for (; i < argc; i++) {
        std::wstring a = argv[i];
        if (a == L"--bridge-dir" && i + 1 < argc) bridge = argv[++i];
        else if (a == L"--texture-budget" && i + 1 < argc) budget = _wtoi(argv[++i]);
        else break;
    }
    if (i >= argc || (budget != 0 && budget != 2 && budget != 4)) {
        puts("usage: libovr_launch.exe [--bridge-dir DIR] [--texture-budget 2|4] <game.exe> [game arguments...]");
        return 2;
    }
    const std::wstring exe = argv[i];
    std::wstring cmd = L"\"" + exe + L"\"";
    for (int k = i + 1; k < argc; k++) cmd += L" \"" + std::wstring(argv[k]) + L"\"";

    if (GetFileAttributesW((bridge + L"\\LibOVRRT64_1.dll").c_str()) == INVALID_FILE_ATTRIBUTES) {
        Log("the bridge DLL is missing: %ls\\LibOVRRT64_1.dll", bridge.c_str());
        return 2;
    }
    SetEnvironmentVariableW(L"LIBOVR_DLL_DIR", bridge.c_str());
    Log("LIBOVR_DLL_DIR = %ls", bridge.c_str());

    uint32_t ts = 0;
    const Profile* profile = nullptr;
    if (ReadTimestamp(exe, &ts)) {
        for (const auto& p : Profiles()) if (p.timestamp == ts) profile = &p;
        Log("%ls: PE timestamp 0x%08x%s%s", exe.c_str(), ts, profile ? " = " : " (not in the patch table)", profile ? profile->name : "");
    } else {
        Log("cannot read %ls", exe.c_str());
        return 2;
    }

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(exe.c_str(), &cmd[0], nullptr, nullptr, FALSE, CREATE_SUSPENDED, nullptr, DirOf(exe).c_str(), &si, &pi)) {
        Log("CreateProcess failed (%lu)", GetLastError());
        return 2;
    }
    int rc = 0;
    if (profile) {
        const uintptr_t base = RemoteImageBase(pi.hProcess);
        if (!base) {
            Log("cannot find the game's image base: starting it unpatched");
        } else {
            std::vector<const std::vector<Patch>*> sets = {&profile->signature};
            if (budget == 2) sets.push_back(&profile->budget2);
            if (budget == 4) sets.push_back(&profile->budget4);
            bool signatureOk = true;
            for (size_t s = 0; s < sets.size(); s++)
                for (const auto& p : *sets[s])
                    if (Apply(pi.hProcess, base, p) != 1 && s == 0) signatureOk = false;
            if (!signatureOk) Log("WARNING: the signature check was not patched: the game will probably refuse the bridge DLL");
        }
    } else {
        Log("WARNING: no patch is known for this build: the game's own check will probably refuse the unsigned bridge DLL");
    }
    ResumeThread(pi.hThread);
    Log("game started (pid %lu)", pi.dwProcessId);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return rc;
}
