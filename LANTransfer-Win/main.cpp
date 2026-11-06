// LAN Transfer - Windows (C++ / Win32)
// Mac sürümüyle aynı protokol. Bkz. mac/Sources/LANTransfer/Net.swift
//
// MSVC:  cl /EHsc /O2 /utf-8 /DUNICODE /D_UNICODE main.cpp /link /SUBSYSTEM:WINDOWS
// MinGW: g++ -O2 -std=c++17 -municode -mwindows main.cpp -o LANTransfer.exe ^
//          -lws2_32 -liphlpapi -lcomctl32 -lole32 -luuid -lshell32 -static

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>
using std::max;
using std::min;
#include <dwmapi.h>
#include <d2d1.h>
#include <dwrite.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

static const char MAGIC[4] = {'L', 'N', 'T', '2'};
static const uint16_t DISCOVERY_PORT = 50000;
static const uint16_t TRANSFER_PORT = 50001;
static const size_t CHUNK = 1 << 20;

enum {
    ID_PEERS = 100, ID_QUEUE, ID_IP, ID_ADDFILE, ID_ADDDIR, ID_REMOVE, ID_CLEAR,
    ID_SEND, ID_OPEN, ID_PROGRESS, ID_STATUS, ID_INFO, ID_LBL_PEERS, ID_LBL_QUEUE, ID_LBL_IP,
    ID_SPEED, ID_AUTOSTART, ID_TRAY_SHOW = 300, ID_TRAY_EXIT, ID_TRAY_AUTOSTART,
};
enum { WM_APP_STATUS = WM_APP + 1, WM_APP_PROGRESS, WM_APP_SENT, WM_APP_RECEIVED, WM_APP_TRAY, WM_APP_SHOW };

// ---------------------------------------------------------------- yardımcılar
static std::string toUtf8(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), &s[0], n, nullptr, nullptr);
    return s;
}
static std::wstring fromUtf8(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &w[0], n);
    return w;
}
static std::wstring ipStr(uint32_t ip) {
    wchar_t b[32];
    swprintf(b, 32, L"%u.%u.%u.%u", ip >> 24 & 255, ip >> 16 & 255, ip >> 8 & 255, ip & 255);
    return b;
}
static bool fileExists(const std::wstring& p) { return GetFileAttributesW(p.c_str()) != INVALID_FILE_ATTRIBUTES; }
static bool isDirectory(const std::wstring& p) {
    DWORD a = GetFileAttributesW(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}
static std::wstring baseName(const std::wstring& p) {
    std::wstring s = p;
    while (!s.empty() && (s.back() == L'\\' || s.back() == L'/')) s.pop_back();
    size_t i = s.find_last_of(L"\\/");
    return i == std::wstring::npos ? s : s.substr(i + 1);
}

// ---------------------------------------------------------------- global durum
static HWND g_hwnd, g_peers, g_queue, g_ip, g_send, g_progress, g_status, g_speed;
static HFONT g_font;
static int g_dpi = 96;
static std::wstring g_name, g_saveDir;
static std::mutex g_mx;
struct PeerInfo { std::wstring name; ULONGLONG seen; };
static std::map<uint32_t, PeerInfo> g_seen;
static std::set<uint32_t> g_ownIps;
static std::vector<uint32_t> g_peerList;    // listbox sırası
static std::vector<std::wstring> g_peerNames;
static std::vector<std::wstring> g_items;   // gönderilecekler
static std::atomic<bool> g_busy{false};

static void postStatus(const std::wstring& s) {
    PostMessageW(g_hwnd, WM_APP_STATUS, 0, (LPARAM) new std::wstring(s));
}
static std::wstring fmtBytes(double b) {
    const wchar_t* u[] = {L"B", L"KB", L"MB", L"GB", L"TB"};
    int i = 0;
    while (b >= 1024 && i < 4) { b /= 1024; i++; }
    wchar_t s[32];
    swprintf(s, 32, i == 0 ? L"%.0f %ls" : b < 10 ? L"%.2f %ls" : b < 100 ? L"%.1f %ls" : L"%.0f %ls", b, u[i]);
    return s;
}
static std::wstring fmtTime(double sec) {
    if (sec < 0 || sec > 360000) return L"—";
    int s = (int)(sec + 0.5);
    wchar_t b[32];
    if (s < 60) swprintf(b, 32, L"%d sn", s);
    else if (s < 3600) swprintf(b, 32, L"%d dk %02d sn", s / 60, s % 60);
    else swprintf(b, 32, L"%d sa %02d dk", s / 3600, s / 60 % 60);
    return b;
}

struct Prog { uint64_t done, total; double speed; bool sending; std::wstring label; };
static void postProgress(uint64_t done, uint64_t total, double speed, bool sending, const std::wstring& s) {
    PostMessageW(g_hwnd, WM_APP_PROGRESS, 0, (LPARAM) new Prog{done, total, speed, sending, s});
}

// Anlık hız: 250 ms'lik dilimlerle ölçülüp yumuşatılır
struct Rate {
    ULONGLONG t0 = GetTickCount64(), lt = t0;
    uint64_t lb = 0;
    double ema = 0;
    double update(uint64_t done) {
        ULONGLONG now = GetTickCount64();
        double dt = (now - lt) / 1000.0;
        if (dt >= 0.25) {
            double inst = (done - lb) / dt;
            ema = ema == 0 ? inst : ema * 0.7 + inst * 0.3;
            lt = now;
            lb = done;
        }
        return ema;
    }
    double avg(uint64_t bytes) const {
        double sec = (GetTickCount64() - t0) / 1000.0;
        return sec > 0.05 ? bytes / sec : (double)bytes;
    }
    std::wstring summary(uint64_t bytes) const {
        double sec = (GetTickCount64() - t0) / 1000.0;
        return fmtBytes((double)bytes) + L"  ·  " + fmtTime(sec) + L"  ·  ort. " +
               fmtBytes(sec > 0.05 ? bytes / sec : (double)bytes) + L"/s";
    }
};

// ---------------------------------------------------------------- ağ
struct LocalIf { uint32_t ip, bcast, mask; };

static std::vector<LocalIf> localInterfaces() {
    std::vector<LocalIf> res;
    ULONG sz = 16 * 1024;
    std::vector<BYTE> buf(sz);
    ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
    ULONG r = GetAdaptersAddresses(AF_INET, flags, nullptr, (IP_ADAPTER_ADDRESSES*)buf.data(), &sz);
    if (r == ERROR_BUFFER_OVERFLOW) {
        buf.resize(sz);
        r = GetAdaptersAddresses(AF_INET, flags, nullptr, (IP_ADAPTER_ADDRESSES*)buf.data(), &sz);
    }
    if (r != NO_ERROR) return res;
    for (auto a = (IP_ADAPTER_ADDRESSES*)buf.data(); a; a = a->Next) {
        if (a->OperStatus != IfOperStatusUp || a->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
        for (auto u = a->FirstUnicastAddress; u; u = u->Next) {
            auto sin = (sockaddr_in*)u->Address.lpSockaddr;
            uint32_t ip = ntohl(sin->sin_addr.s_addr);
            int pl = u->OnLinkPrefixLength;
            if (pl <= 0 || pl >= 32) continue;
            uint32_t mask = 0xFFFFFFFFu << (32 - pl);
            res.push_back({ip, ip | ~mask, mask});
        }
    }
    return res;
}

static bool sendAll(SOCKET s, const char* p, size_t n) {
    while (n) {
        int k = send(s, p, (int)std::min<size_t>(n, 1 << 30), 0);
        if (k <= 0) return false;
        p += k; n -= k;
    }
    return true;
}
static bool recvExact(SOCKET s, char* p, size_t n) {
    while (n) {
        int k = recv(s, p, (int)std::min<size_t>(n, 1 << 30), 0);
        if (k <= 0) return false;
        p += k; n -= k;
    }
    return true;
}
static bool readUInt(SOCKET s, int bytes, uint64_t& v) {
    unsigned char b[8];
    if (!recvExact(s, (char*)b, bytes)) return false;
    v = 0;
    for (int i = 0; i < bytes; i++) v = v << 8 | b[i];
    return true;
}
static bool readString(SOCKET s, std::string& out) {
    uint64_t n;
    if (!readUInt(s, 2, n)) return false;
    out.resize((size_t)n);
    return n == 0 || recvExact(s, &out[0], (size_t)n);
}
static void putBE(std::string& b, uint64_t v, int bytes) {
    for (int i = bytes - 1; i >= 0; i--) b.push_back((char)(v >> (8 * i) & 255));
}
static void putString(std::string& b, const std::string& s) {
    size_t n = std::min<size_t>(s.size(), 65535);
    putBE(b, n, 2);
    b.append(s, 0, n);
}

static SOCKET connectTCP(uint32_t ip, uint16_t port, int timeoutMs = 5000) {
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return s;
    u_long nb = 1;
    ioctlsocket(s, FIONBIO, &nb);
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons(port);
    a.sin_addr.s_addr = htonl(ip);
    connect(s, (sockaddr*)&a, sizeof a);
    fd_set w, e;
    FD_ZERO(&w); FD_SET(s, &w);
    FD_ZERO(&e); FD_SET(s, &e);
    timeval tv{timeoutMs / 1000, (timeoutMs % 1000) * 1000};
    if (select(0, nullptr, &w, &e, &tv) != 1 || !FD_ISSET(s, &w)) {
        closesocket(s);
        return INVALID_SOCKET;
    }
    nb = 0;
    ioctlsocket(s, FIONBIO, &nb);
    return s;
}

// ---------------------------------------------------------------- keşif
// Keşif paketi: "LNT2" + tür ('A' = duyuru, 'R' = cevap) + cihaz adı
static SOCKET g_disc = INVALID_SOCKET;

static std::string discPacket(char kind) {
    std::string p(MAGIC, 4);
    p.push_back(kind);
    return p + toUtf8(g_name);
}

static void sendDisc(const std::string& pkt, uint32_t ip) {
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons(DISCOVERY_PORT);
    a.sin_addr.s_addr = htonl(ip);
    sendto(g_disc, pkt.data(), (int)pkt.size(), 0, (sockaddr*)&a, sizeof a);
}

static void broadcastThread() {
    while (g_disc == INVALID_SOCKET) Sleep(100);
    std::string pkt = discPacket('A');
    for (int round = 0;; round++) {
        auto ifs = localInterfaces();
        std::set<uint32_t> targets = {0xFFFFFFFFu};
        {
            std::lock_guard<std::mutex> lk(g_mx);
            g_ownIps.clear();
            for (auto& i : ifs) { g_ownIps.insert(i.ip); targets.insert(i.bcast); }
        }
        for (uint32_t t : targets) sendDisc(pkt, t);
        // Modem yayınları engelliyorsa: alt ağdaki her adrese doğrudan gönder (3 turda bir)
        if (round % 3 == 0) {
            for (auto& i : ifs) {
                uint32_t hosts = ~i.mask, net = i.ip & i.mask;
                if (hosts <= 1 || hosts > 1023) continue;
                for (uint32_t h = 1; h < hosts; h++)
                    if ((net | h) != i.ip) sendDisc(pkt, net | h);
            }
        }
        Sleep(2000);
    }
}

static void discoveryThread() {
    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    BOOL on = TRUE;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char*)&on, sizeof on);
    setsockopt(s, SOL_SOCKET, SO_BROADCAST, (char*)&on, sizeof on);
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons(DISCOVERY_PORT);
    if (bind(s, (sockaddr*)&a, sizeof a) != 0) {
        postStatus(L"Keşif portu (50000) açılamadı");
        return;
    }
    g_disc = s;
    std::string reply = discPacket('R');
    char buf[1500];
    for (;;) {
        sockaddr_in from{};
        int len = sizeof from;
        int n = recvfrom(s, buf, sizeof buf, 0, (sockaddr*)&from, &len);
        if (n < 5 || memcmp(buf, MAGIC, 4) != 0) continue;
        uint32_t ip = ntohl(from.sin_addr.s_addr);
        char kind = buf[4];
        std::wstring name = fromUtf8(std::string(buf + 5, n - 5));
        bool mine;
        {
            std::lock_guard<std::mutex> lk(g_mx);
            mine = g_ownIps.count(ip) || (ip >> 24) == 127;
            if (!mine) g_seen[ip] = {name, GetTickCount64()};
        }
        if (!mine && kind == 'A') sendDisc(reply, ip);
    }
}

static void refreshPeers() {
    std::vector<std::pair<std::wstring, uint32_t>> list;
    {
        std::lock_guard<std::mutex> lk(g_mx);
        ULONGLONG now = GetTickCount64();
        for (auto it = g_seen.begin(); it != g_seen.end();)
            it = (now - it->second.seen > 8000) ? g_seen.erase(it) : std::next(it);
        for (auto& p : g_seen) list.push_back({p.second.name, p.first});
    }
    std::sort(list.begin(), list.end());
    std::vector<uint32_t> ips;
    for (auto& p : list) ips.push_back(p.second);
    if (ips == g_peerList) return;

    int sel = (int)SendMessageW(g_peers, LB_GETCURSEL, 0, 0);
    uint32_t selIp = (sel >= 0 && sel < (int)g_peerList.size()) ? g_peerList[sel] : 0;
    SendMessageW(g_peers, LB_RESETCONTENT, 0, 0);
    g_peerList = ips;
    g_peerNames.clear();
    for (auto& p : list) g_peerNames.push_back(p.first);
    InvalidateRect(g_hwnd, nullptr, FALSE);  // cihaz sayısı rozeti
    for (size_t i = 0; i < list.size(); i++) {
        SendMessageW(g_peers, LB_ADDSTRING, 0, (LPARAM)list[i].first.c_str());
        if (list[i].second == selIp) SendMessageW(g_peers, LB_SETCURSEL, i, 0);
    }
    // Seçili cihaz yoksa (ilk açılış ya da seçili cihaz kayboldu) ilk sıradakini seç
    if (!list.empty() && SendMessageW(g_peers, LB_GETCURSEL, 0, 0) == LB_ERR)
        SendMessageW(g_peers, LB_SETCURSEL, 0, 0);
}

// ---------------------------------------------------------------- alma
static std::wstring sanitize(std::wstring c) {
    for (auto& ch : c)
        if (ch < 32 || wcschr(L"<>:\"|?*\\", ch)) ch = L'_';
    while (!c.empty() && (c.back() == L' ' || c.back() == L'.')) c.pop_back();
    return c.empty() ? L"_" : c;
}

static std::wstring uniqueName(const std::wstring& name) {
    if (!fileExists(g_saveDir + L"\\" + name)) return name;
    size_t dot = name.find_last_of(L'.');
    std::wstring stem = (dot == std::wstring::npos || dot == 0) ? name : name.substr(0, dot);
    std::wstring ext = (dot == std::wstring::npos || dot == 0) ? L"" : name.substr(dot);
    for (int i = 1;; i++) {
        std::wstring c = stem + L" (" + std::to_wstring(i) + L")" + ext;
        if (!fileExists(g_saveDir + L"\\" + c)) return c;
    }
}

static void receiveThread(SOCKET s) {
    struct Closer { SOCKET s; ~Closer() { closesocket(s); } } closer{s};
    auto fail = [](const wchar_t* m) { postStatus(std::wstring(L"Alma hatası: ") + m); };

    char magic[4];
    if (!recvExact(s, magic, 4) || memcmp(magic, MAGIC, 4) != 0) return;
    std::string senderU;
    uint64_t count;
    if (!readString(s, senderU) || !readUInt(s, 4, count)) return fail(L"başlık okunamadı");
    std::wstring sender = fromUtf8(senderU);

    struct F { std::string path; uint64_t size; };
    std::vector<F> files;
    uint64_t total = 0;
    for (uint64_t i = 0; i < count; i++) {
        F f;
        if (!readString(s, f.path) || !readUInt(s, 8, f.size)) return fail(L"başlık okunamadı");
        total += f.size;
        files.push_back(f);
    }

    std::map<std::wstring, std::wstring> topMap;
    std::vector<char> buf(CHUNK);
    Rate rate;
    uint64_t done = 0;
    ULONGLONG lastUI = 0;
    for (auto& f : files) {
        std::vector<std::wstring> comps;
        std::wstring p = fromUtf8(f.path);
        size_t st = 0;
        while (st <= p.size()) {
            size_t e = p.find(L'/', st);
            if (e == std::wstring::npos) e = p.size();
            std::wstring c = p.substr(st, e - st);
            if (!c.empty() && c != L"." && c != L"..") comps.push_back(sanitize(c));
            st = e + 1;
        }
        if (comps.empty()) comps.push_back(L"dosya");
        if (!topMap.count(comps[0])) topMap[comps[0]] = uniqueName(comps[0]);
        comps[0] = topMap[comps[0]];

        std::wstring dir = g_saveDir;
        for (size_t i = 0; i + 1 < comps.size(); i++) dir += L"\\" + comps[i];
        SHCreateDirectoryExW(nullptr, dir.c_str(), nullptr);
        std::wstring full = dir + L"\\" + comps.back();

        HANDLE h = CreateFileW(full.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE) return fail((L"dosya oluşturulamadı: " + comps.back()).c_str());
        uint64_t rem = f.size;
        while (rem) {
            size_t n = (size_t)std::min<uint64_t>(CHUNK, rem);
            if (!recvExact(s, buf.data(), n)) { CloseHandle(h); return fail(L"bağlantı kesildi"); }
            DWORD w;
            if (!WriteFile(h, buf.data(), (DWORD)n, &w, nullptr) || w != n) {
                CloseHandle(h);
                return fail(L"diske yazılamadı");
            }
            rem -= n; done += n;
            if (GetTickCount64() - lastUI > 100) {
                lastUI = GetTickCount64();
                postProgress(done, total, rate.update(done), false, L"Alınıyor ← " + sender + L": " + comps.back());
            }
        }
        CloseHandle(h);
    }
    sendAll(s, "OK", 2);
    postProgress(total, total, rate.avg(total), false, L"");
    PostMessageW(g_hwnd, WM_APP_RECEIVED, 0,
                 (LPARAM) new std::wstring(L"✓  " + std::to_wstring(files.size()) + L" dosya alındı ← " + sender +
                                           L"    " + rate.summary(total)));
}

static void serverThread() {
    SOCKET srv = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons(TRANSFER_PORT);
    if (bind(srv, (sockaddr*)&a, sizeof a) != 0 || listen(srv, 8) != 0) {
        postStatus(L"Aktarım portu (50001) açılamadı");
        return;
    }
    for (;;) {
        SOCKET c = accept(srv, nullptr, nullptr);
        if (c != INVALID_SOCKET) std::thread(receiveThread, c).detach();
    }
}

// ---------------------------------------------------------------- gönderme
struct SendFile { std::wstring full; std::string rel; uint64_t size; };

static void walk(const std::wstring& dir, const std::wstring& rel, std::vector<SendFile>& out) {
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((dir + L"\\*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        std::wstring n = fd.cFileName;
        if (n == L"." || n == L".." || n == L"desktop.ini" || n == L"Thumbs.db") continue;
        std::wstring full = dir + L"\\" + n, r = rel + L"/" + n;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) walk(full, r, out);
        } else {
            out.push_back({full, toUtf8(r), (uint64_t)fd.nFileSizeHigh << 32 | fd.nFileSizeLow});
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

static void sendThread(std::vector<std::wstring> items, uint32_t ip) {
    auto done = [&](bool ok, const std::wstring& msg) {
        PostMessageW(g_hwnd, WM_APP_SENT, ok, (LPARAM) new std::wstring(msg));
    };
    std::vector<SendFile> files;
    for (auto& it : items) {
        if (isDirectory(it)) walk(it, baseName(it), files);
        else {
            WIN32_FILE_ATTRIBUTE_DATA ad;
            if (GetFileAttributesExW(it.c_str(), GetFileExInfoStandard, &ad))
                files.push_back({it, toUtf8(baseName(it)), (uint64_t)ad.nFileSizeHigh << 32 | ad.nFileSizeLow});
        }
    }
    if (files.empty()) return done(false, L"Gönderilecek dosya bulunamadı");

    std::string hdr(MAGIC, 4);
    putString(hdr, toUtf8(g_name));
    putBE(hdr, files.size(), 4);
    uint64_t total = 0;
    for (auto& f : files) { putString(hdr, f.rel); putBE(hdr, f.size, 8); total += f.size; }

    postStatus(L"Bağlanıyor → " + ipStr(ip) + L"…");
    SOCKET s = connectTCP(ip, TRANSFER_PORT);
    if (s == INVALID_SOCKET) return done(false, L"Gönderme hatası: " + ipStr(ip) + L" adresine bağlanılamadı");
    struct Closer { SOCKET s; ~Closer() { closesocket(s); } } closer{s};
    if (!sendAll(s, hdr.data(), hdr.size())) return done(false, L"Gönderme hatası: bağlantı kesildi");

    std::vector<char> buf(CHUNK);
    uint64_t sent = 0;
    Rate rate;
    ULONGLONG lastUI = 0;
    for (auto& f : files) {
        HANDLE h = CreateFileW(f.full.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                               OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
        if (h == INVALID_HANDLE_VALUE) return done(false, L"Gönderme hatası: açılamadı: " + baseName(f.full));
        uint64_t rem = f.size;
        while (rem) {
            DWORD want = (DWORD)std::min<uint64_t>(CHUNK, rem), got = 0;
            if (!ReadFile(h, buf.data(), want, &got, nullptr) || got == 0) {
                CloseHandle(h);
                return done(false, L"Gönderme hatası: okunamadı: " + baseName(f.full));
            }
            if (!sendAll(s, buf.data(), got)) { CloseHandle(h); return done(false, L"Gönderme hatası: bağlantı kesildi"); }
            rem -= got; sent += got;
            if (GetTickCount64() - lastUI > 100) {
                lastUI = GetTickCount64();
                postProgress(sent, total, rate.update(sent), true, L"Gönderiliyor → " + baseName(f.full));
            }
        }
        CloseHandle(h);
    }
    char ok[2];
    if (!recvExact(s, ok, 2) || memcmp(ok, "OK", 2) != 0) return done(false, L"Gönderme hatası: karşı taraf onay vermedi");
    postProgress(total, total, rate.avg(total), true, L"");
    done(true, L"✓  " + std::to_wstring(files.size()) + L" dosya gönderildi    " + rate.summary(total));
}

// ---------------------------------------------------------------- arayüz
// Tüm özel çizimler Direct2D + DirectWrite ile yapılır.
// Renk paleti
static const COLORREF C_BG = RGB(244, 246, 251), C_CARD = RGB(255, 255, 255), C_BORDER = RGB(228, 232, 240),
                      C_FIELD = RGB(248, 250, 252), C_ACCENT = RGB(79, 70, 229), C_ACCENT_HOT = RGB(99, 102, 241),
                      C_ACCENT_DK = RGB(67, 56, 202), C_ACCENT_LT = RGB(238, 242, 255), C_ACCENT_DIS = RGB(180, 186, 250),
                      C_TEXT = RGB(17, 24, 39), C_MUTED = RGB(107, 114, 128), C_HDR_SUB = RGB(224, 231, 255),
                      C_TRACK = RGB(229, 231, 235), C_HOVER = RGB(243, 244, 246), C_PRESS = RGB(229, 231, 235),
                      C_BLUE = RGB(59, 130, 246), C_SEL_BORDER = RGB(199, 210, 254), C_WHITE = RGB(255, 255, 255);
static HBRUSH g_bgBrush, g_cardBrush, g_fieldBrush;
static HFONT g_fontSm;
static HICON g_iconSm, g_iconLg;
static std::wstring g_infoText;
static NOTIFYICONDATAW g_nid{};
static UINT WM_TASKBARCREATED;
static bool g_closeHintShown = false, g_autoStart = false;
static HWND g_hot;
static RECT g_rcDev, g_rcQueue, g_rcBottom, g_rcIp, g_rcFooter;
static RECT g_rcSend;
struct Stats { std::wstring speed = L"—", done = L"—", eta = L"—"; int prog = 0; };
static Stats g_st;
static std::map<std::wstring, std::wstring> g_itemSub;
static std::map<std::wstring, HICON> g_itemIcon;

static int S(int v) { return MulDiv(v, g_dpi, 96); }
static const int HEADER_H = 84;

// ---- Direct2D / DirectWrite
static ID2D1Factory* g_d2f;
static IDWriteFactory* g_dwf;
static ID2D1DCRenderTarget* g_rt;
static ID2D1SolidColorBrush* g_br;
static ID2D1StrokeStyle* g_dash;
enum TF { TF_BODY, TF_SM, TF_BOLD, TF_H, TF_BIG, TF_TITLE, TF_AVATAR, TF_ICON, TF_COUNT };
static IDWriteTextFormat* g_tf[TF_COUNT];
static IDWriteInlineObject* g_ell[TF_COUNT];

template <class T> static void release(T*& p) { if (p) { p->Release(); p = nullptr; } }

static void d2Init(const wchar_t* body, const wchar_t* disp) {
    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &g_d2f);
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(&g_dwf));
    struct Spec { const wchar_t* face; float pt; DWRITE_FONT_WEIGHT w; };
    const Spec spec[TF_COUNT] = {
        {body, 10, DWRITE_FONT_WEIGHT_NORMAL},   {body, 9, DWRITE_FONT_WEIGHT_NORMAL},
        {body, 10, DWRITE_FONT_WEIGHT_SEMI_BOLD}, {disp, 12, DWRITE_FONT_WEIGHT_SEMI_BOLD},
        {disp, 15, DWRITE_FONT_WEIGHT_SEMI_BOLD}, {disp, 20, DWRITE_FONT_WEIGHT_BOLD},
        {disp, 13, DWRITE_FONT_WEIGHT_SEMI_BOLD}, {disp, 22, DWRITE_FONT_WEIGHT_NORMAL},
    };
    for (int i = 0; i < TF_COUNT; i++) {
        g_dwf->CreateTextFormat(spec[i].face, nullptr, spec[i].w, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                                spec[i].pt * g_dpi / 72.0f, L"tr-tr", &g_tf[i]);
        g_tf[i]->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        g_dwf->CreateEllipsisTrimmingSign(g_tf[i], &g_ell[i]);
        DWRITE_TRIMMING tr{DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0};
        g_tf[i]->SetTrimming(&tr, g_ell[i]);
    }
    D2D1_STROKE_STYLE_PROPERTIES sp = D2D1::StrokeStyleProperties(
        D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_LINE_JOIN_ROUND, 10.0f,
        D2D1_DASH_STYLE_DASH, 0.0f);
    g_d2f->CreateStrokeStyle(sp, nullptr, 0, &g_dash);
}
static void d2Release() {
    release(g_br);
    release(g_rt);
}
static bool d2Begin(HDC dc, const RECT& r) {
    if (!g_rt) {
        // 96 DPI: koordinatlar doğrudan piksel
        auto props = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,
                                                  D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE),
                                                  96.0f, 96.0f);
        if (FAILED(g_d2f->CreateDCRenderTarget(&props, &g_rt))) return false;
        g_rt->CreateSolidColorBrush(D2D1::ColorF(0), &g_br);
        g_rt->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);
    }
    if (FAILED(g_rt->BindDC(dc, &r))) return false;
    g_rt->BeginDraw();
    g_rt->SetTransform(D2D1::Matrix3x2F::Translation((float)-r.left, (float)-r.top));
    return true;
}
static void d2End() {
    if (g_rt->EndDraw() == D2DERR_RECREATE_TARGET) d2Release();
}
static D2D1_COLOR_F col(COLORREF c, float a = 1) {
    return D2D1::ColorF(GetRValue(c) / 255.0f, GetGValue(c) / 255.0f, GetBValue(c) / 255.0f, a);
}
static ID2D1SolidColorBrush* br(COLORREF c, float a = 1) { g_br->SetColor(col(c, a)); return g_br; }
static D2D1_RECT_F rf(const RECT& r) { return D2D1::RectF((float)r.left, (float)r.top, (float)r.right, (float)r.bottom); }

static void fillRect(const RECT& r, COLORREF c) { g_rt->FillRectangle(rf(r), br(c)); }
static void fillRound(const RECT& r, int rad, COLORREF fill, float a = 1, COLORREF border = CLR_INVALID, float ba = 1) {
    D2D1_ROUNDED_RECT rr{rf(r), (float)rad, (float)rad};
    g_rt->FillRoundedRectangle(rr, br(fill, a));
    if (border != CLR_INVALID) {
        D2D1_ROUNDED_RECT b{D2D1::RectF(r.left + 0.5f, r.top + 0.5f, r.right - 0.5f, r.bottom - 0.5f), (float)rad, (float)rad};
        g_rt->DrawRoundedRectangle(b, br(border, ba), 1.0f);
    }
}
static void fillEllipse(float x, float y, float d, COLORREF c) {
    g_rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x + d / 2, y + d / 2), d / 2, d / 2), br(c));
}
// ha: 0 sol, 1 orta, 2 sağ — va: 0 üst, 1 orta, 2 alt
static void text(const std::wstring& s, const RECT& r, TF f, COLORREF c, int ha = 0, int va = 1) {
    IDWriteTextFormat* t = g_tf[f];
    t->SetTextAlignment(ha == 0 ? DWRITE_TEXT_ALIGNMENT_LEADING : ha == 1 ? DWRITE_TEXT_ALIGNMENT_CENTER : DWRITE_TEXT_ALIGNMENT_TRAILING);
    t->SetParagraphAlignment(va == 0 ? DWRITE_PARAGRAPH_ALIGNMENT_NEAR : va == 1 ? DWRITE_PARAGRAPH_ALIGNMENT_CENTER : DWRITE_PARAGRAPH_ALIGNMENT_FAR);
    g_rt->DrawText(s.c_str(), (UINT32)s.size(), t, rf(r), br(c), D2D1_DRAW_TEXT_OPTIONS_CLIP);
}
static float textWidth(const std::wstring& s, TF f) {
    IDWriteTextLayout* l = nullptr;
    g_tf[f]->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    if (FAILED(g_dwf->CreateTextLayout(s.c_str(), (UINT32)s.size(), g_tf[f], 10000, 1000, &l))) return 0;
    DWRITE_TEXT_METRICS m{};
    l->GetMetrics(&m);
    l->Release();
    return m.widthIncludingTrailingWhitespace;
}
static void card(const RECT& r) {
    RECT sh = {r.left, r.top + S(2), r.right, r.bottom + S(2)};
    fillRound(sh, S(14), RGB(15, 23, 42), 0.05f);
    fillRound(r, S(14), C_CARD, 1, C_BORDER);
}
static void pill(const std::wstring& s, int right, int cy) {
    int w = (int)textWidth(s, TF_SM) + S(20);
    RECT r = {right - w, cy - S(11), right, cy + S(11)};
    fillRound(r, S(11), C_ACCENT_LT);
    text(s, r, TF_SM, C_ACCENT, 1);
}

// ---- Windows ile başla
static const wchar_t* RUN_KEY = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static const wchar_t* RUN_VAL = L"LANTransfer";
static bool isAutoStart() {
    DWORD sz = 0;
    return RegGetValueW(HKEY_CURRENT_USER, RUN_KEY, RUN_VAL, RRF_RT_REG_SZ, nullptr, nullptr, &sz) == ERROR_SUCCESS;
}
static void setAutoStart(bool on) {
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, RUN_KEY, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &k, nullptr) != ERROR_SUCCESS) return;
    if (on) {
        wchar_t p[MAX_PATH];
        GetModuleFileNameW(nullptr, p, MAX_PATH);
        std::wstring c = L"\"" + std::wstring(p) + L"\" /tray";
        RegSetValueExW(k, RUN_VAL, 0, REG_SZ, (const BYTE*)c.c_str(), (DWORD)((c.size() + 1) * sizeof(wchar_t)));
    } else {
        RegDeleteValueW(k, RUN_VAL);
    }
    RegCloseKey(k);
    g_autoStart = isAutoStart();
    if (g_hwnd) InvalidateRect(GetDlgItem(g_hwnd, ID_AUTOSTART), nullptr, FALSE);
}

// ---- Tepsi simgesi
static void trayAdd() {
    g_nid.cbSize = sizeof g_nid;
    g_nid.hWnd = g_hwnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_APP_TRAY;
    g_nid.hIcon = g_iconSm;
    lstrcpynW(g_nid.szTip, L"LAN Transfer", ARRAYSIZE(g_nid.szTip));
    Shell_NotifyIconW(NIM_ADD, &g_nid);
}
static void trayTip(const std::wstring& t) {
    g_nid.uFlags = NIF_TIP;
    lstrcpynW(g_nid.szTip, t.c_str(), ARRAYSIZE(g_nid.szTip));
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}
static void trayBalloon(const std::wstring& title, const std::wstring& text) {
    g_nid.uFlags = NIF_INFO;
    lstrcpynW(g_nid.szInfoTitle, title.c_str(), ARRAYSIZE(g_nid.szInfoTitle));
    lstrcpynW(g_nid.szInfo, text.c_str(), ARRAYSIZE(g_nid.szInfo));
    g_nid.dwInfoFlags = NIIF_INFO;
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}
static void showMain() {
    ShowWindow(g_hwnd, IsIconic(g_hwnd) ? SW_RESTORE : SW_SHOW);
    SetForegroundWindow(g_hwnd);
}
static void trayMenu() {
    HMENU m = CreatePopupMenu();
    AppendMenuW(m, MF_STRING, ID_TRAY_SHOW, L"Göster");
    AppendMenuW(m, MF_STRING, ID_OPEN, L"Gelen Klasörü Aç");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING | (isAutoStart() ? MF_CHECKED : 0), ID_TRAY_AUTOSTART, L"Windows ile başla");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, ID_TRAY_EXIT, L"Çıkış");
    SetMenuDefaultItem(m, ID_TRAY_SHOW, FALSE);
    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(g_hwnd);
    TrackPopupMenu(m, TPM_RIGHTBUTTON, pt.x, pt.y, 0, g_hwnd, nullptr);
    PostMessageW(g_hwnd, WM_NULL, 0, 0);
    DestroyMenu(m);
}

// ---- İlerleme / istatistik
static void setProg(int pm) {
    g_st.prog = pm;
    InvalidateRect(GetDlgItem(g_hwnd, ID_PROGRESS), nullptr, FALSE);
}
static void resetStats() {
    g_st = Stats{};
    setProg(0);
    InvalidateRect(g_hwnd, &g_rcBottom, FALSE);
}

// ---- Kuyruk
static void queueChanged() { InvalidateRect(g_hwnd, &g_rcQueue, FALSE); }

static void addItem(const std::wstring& p) {
    if (std::find(g_items.begin(), g_items.end(), p) != g_items.end()) return;
    g_items.push_back(p);
    std::wstring parent = p.substr(0, p.size() - baseName(p).size());
    while (!parent.empty() && (parent.back() == L'\\' || parent.back() == L'/')) parent.pop_back();
    if (isDirectory(p)) {
        g_itemSub[p] = L"Klasör  ·  " + parent;
    } else {
        WIN32_FILE_ATTRIBUTE_DATA ad;
        uint64_t sz = GetFileAttributesExW(p.c_str(), GetFileExInfoStandard, &ad)
                          ? ((uint64_t)ad.nFileSizeHigh << 32 | ad.nFileSizeLow) : 0;
        g_itemSub[p] = fmtBytes((double)sz) + L"  ·  " + parent;
    }
    if (!g_itemIcon.count(p)) {
        SHFILEINFOW sfi{};
        if (SHGetFileInfoW(p.c_str(), 0, &sfi, sizeof sfi, SHGFI_ICON | SHGFI_LARGEICON)) g_itemIcon[p] = sfi.hIcon;
    }
    SendMessageW(g_queue, LB_ADDSTRING, 0, (LPARAM)baseName(p).c_str());
    queueChanged();
}

static void clearQueue() {
    g_items.clear();
    SendMessageW(g_queue, LB_RESETCONTENT, 0, 0);
    for (auto& i : g_itemIcon) DestroyIcon(i.second);
    g_itemIcon.clear();
    g_itemSub.clear();
    queueChanged();
}

static void rebuildQueue() {
    auto items = g_items;
    g_items.clear();
    SendMessageW(g_queue, LB_RESETCONTENT, 0, 0);
    for (auto& p : items) addItem(p);
    queueChanged();
}

static void pickItems(bool folders) {
    IFileOpenDialog* dlg = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&dlg)))) return;
    DWORD opts;
    dlg->GetOptions(&opts);
    dlg->SetOptions(opts | FOS_ALLOWMULTISELECT | FOS_FORCEFILESYSTEM | (folders ? FOS_PICKFOLDERS : 0));
    if (SUCCEEDED(dlg->Show(g_hwnd))) {
        IShellItemArray* arr = nullptr;
        if (SUCCEEDED(dlg->GetResults(&arr))) {
            DWORD n = 0;
            arr->GetCount(&n);
            for (DWORD i = 0; i < n; i++) {
                IShellItem* it;
                if (SUCCEEDED(arr->GetItemAt(i, &it))) {
                    PWSTR path;
                    if (SUCCEEDED(it->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
                        addItem(path);
                        CoTaskMemFree(path);
                    }
                    it->Release();
                }
            }
            arr->Release();
        }
    }
    dlg->Release();
}

static void doSend() {
    if (g_busy) return;
    wchar_t buf[64];
    GetWindowTextW(g_ip, buf, 64);
    std::wstring manual = buf;
    manual.erase(std::remove(manual.begin(), manual.end(), L' '), manual.end());
    uint32_t ip = 0;
    if (!manual.empty()) {
        IN_ADDR a;
        if (InetPtonW(AF_INET, manual.c_str(), &a) != 1) { SetWindowTextW(g_status, L"Geçersiz IP adresi"); return; }
        ip = ntohl(a.s_addr);
    } else {
        int sel = (int)SendMessageW(g_peers, LB_GETCURSEL, 0, 0);
        if (sel < 0 || sel >= (int)g_peerList.size()) {
            SetWindowTextW(g_status, L"Önce listeden bir cihaz seçin veya IP girin");
            return;
        }
        ip = g_peerList[sel];
    }
    if (g_items.empty()) { SetWindowTextW(g_status, L"Gönderilecek dosya yok — sürükleyip bırakın"); return; }
    g_busy = true;
    SetWindowTextW(g_send, L"Gönderiliyor…");
    EnableWindow(g_send, FALSE);
    resetStats();
    std::thread(sendThread, g_items, ip).detach();
}

// ---- Sahibi çizilen kontroller (Direct2D)
static void drawButton(DRAWITEMSTRUCT* d) {
    RECT rc = d->rcItem;
    if (!d2Begin(d->hDC, rc)) return;
    int id = (int)d->CtlID;
    bool dis = d->itemState & ODS_DISABLED, pr = d->itemState & ODS_SELECTED, hot = d->hwndItem == g_hot && !dis;
    wchar_t t[128];
    GetWindowTextW(d->hwndItem, t, 128);
    int w = rc.right - rc.left, h = rc.bottom - rc.top;
    RECT r = rc;

    if (id == ID_OPEN) {
        fillRect(r, C_ACCENT);
        fillRound(r, S(10), C_WHITE, pr ? 0.28f : hot ? 0.20f : 0.12f, C_WHITE, 0.35f);
        text(t, r, TF_BOLD, C_WHITE, 1);
    } else if (id == ID_AUTOSTART) {
        fillRect(r, C_BG);
        int tw = S(40), th = S(22), ty = rc.top + (h - th) / 2;
        RECT tr = {rc.left, ty, rc.left + tw, ty + th};
        fillRound(tr, th / 2, g_autoStart ? (hot ? C_ACCENT_HOT : C_ACCENT) : (hot ? RGB(190, 195, 205) : RGB(209, 213, 219)));
        float kd = (float)(th - S(6));
        float kx = (float)(g_autoStart ? rc.left + tw - S(3) - kd : rc.left + S(3));
        fillEllipse(kx, (float)(ty + S(3)), kd, C_WHITE);
        RECT lr = {rc.left + tw + S(10), rc.top, rc.right, rc.bottom};
        text(t, lr, TF_BODY, C_TEXT);
    } else if (id == ID_SEND) {
        fillRect(r, C_CARD);
        COLORREF c = dis ? C_ACCENT_DIS : pr ? C_ACCENT_DK : hot ? C_ACCENT_HOT : C_ACCENT;
        if (!dis) {
            RECT sh = {rc.left + S(2), rc.top + S(4), rc.right - S(2), rc.bottom};
            fillRound(sh, S(14), C_ACCENT, 0.18f);
        }
        RECT b = {rc.left, rc.top, rc.right, rc.bottom - S(3)};
        fillRound(b, S(14), c);
        text(dis ? std::wstring(t) : std::wstring(t) + L"   →", b, TF_H, C_WHITE, 1);
    } else {
        fillRect(r, C_CARD);
        fillRound(r, S(9), pr ? C_PRESS : hot ? C_HOVER : C_CARD, 1, hot ? RGB(209, 213, 219) : C_BORDER);
        text(t, r, TF_BOLD, dis ? C_MUTED : C_TEXT, 1);
    }
    (void)w;
    d2End();
}

static void drawListItem(DRAWITEMSTRUCT* d, const std::wstring& title, const std::wstring& sub, COLORREF avatar,
                         HICON icon) {
    RECT rc = d->rcItem;
    if (!d2Begin(d->hDC, rc)) return;
    fillRect(rc, C_CARD);
    bool sel = d->itemState & ODS_SELECTED;
    RECT hr = {rc.left + S(2), rc.top + S(2), rc.right - S(2), rc.bottom - S(2)};
    if (sel) fillRound(hr, S(10), C_ACCENT_LT, 1, C_SEL_BORDER);

    int ad = S(36), ax = rc.left + S(12), ay = rc.top + (rc.bottom - rc.top - ad) / 2;
    if (!icon) {
        fillEllipse((float)ax, (float)ay, (float)ad, avatar);
        wchar_t ini[2] = {title.empty() ? L'?' : title[0], 0};
        CharUpperW(ini);
        RECT ar = {ax, ay, ax + ad, ay + ad};
        text(ini, ar, TF_AVATAR, C_WHITE, 1);
    }
    int tx = ax + ad + S(12), mid = (rc.top + rc.bottom) / 2;
    RECT nr = {tx, rc.top + S(4), rc.right - S(10), mid + S(1)};
    text(title, nr, TF_BOLD, sel ? C_ACCENT_DK : C_TEXT, 0, 2);
    RECT sr = {tx, mid + S(2), rc.right - S(10), rc.bottom - S(4)};
    text(sub, sr, TF_SM, C_MUTED, 0, 0);
    d2End();
    if (icon) DrawIconEx(d->hDC, ax + S(2), ay + S(2), icon, ad - S(4), ad - S(4), 0, nullptr, DI_NORMAL);
}

static void drawPeer(DRAWITEMSTRUCT* d) {
    if (d->itemID == (UINT)-1 || d->itemID >= g_peerNames.size()) return;
    const std::wstring& name = g_peerNames[d->itemID];
    static const COLORREF pal[] = {RGB(99, 102, 241), RGB(236, 72, 153), RGB(16, 185, 129),
                                   RGB(245, 158, 11), RGB(59, 130, 246), RGB(139, 92, 246)};
    unsigned hsh = 0;
    for (wchar_t c : name) hsh = hsh * 31 + c;
    drawListItem(d, name, ipStr(g_peerList[d->itemID]), pal[hsh % 6], nullptr);
}

static void drawQueueItem(DRAWITEMSTRUCT* d) {
    if (d->itemID == (UINT)-1 || d->itemID >= g_items.size()) return;
    const std::wstring& p = g_items[d->itemID];
    auto it = g_itemIcon.find(p);
    HICON ic = it != g_itemIcon.end() ? it->second : nullptr;
    drawListItem(d, baseName(p), g_itemSub[p], C_ACCENT, ic ? ic : nullptr);
}

static void drawProgress(DRAWITEMSTRUCT* d) {
    RECT rc = d->rcItem;
    if (!d2Begin(d->hDC, rc)) return;
    fillRect(rc, C_CARD);
    int h = rc.bottom - rc.top;
    fillRound(rc, h / 2, C_TRACK);
    float fw = (float)(rc.right - rc.left) * g_st.prog / 1000.0f;
    if (fw >= h) {
        ID2D1GradientStopCollection* sc = nullptr;
        D2D1_GRADIENT_STOP st[2] = {{0.0f, col(C_ACCENT)}, {1.0f, col(C_BLUE)}};
        g_rt->CreateGradientStopCollection(st, 2, &sc);
        ID2D1LinearGradientBrush* lb = nullptr;
        if (sc) g_rt->CreateLinearGradientBrush(
            D2D1::LinearGradientBrushProperties(D2D1::Point2F((float)rc.left, 0), D2D1::Point2F(rc.left + fw, 0)), sc, &lb);
        if (lb) {
            D2D1_ROUNDED_RECT rr{D2D1::RectF((float)rc.left, (float)rc.top, rc.left + fw, (float)rc.bottom), h / 2.0f, h / 2.0f};
            g_rt->FillRoundedRectangle(rr, lb);
        }
        release(lb);
        release(sc);
    }
    d2End();
}

static void paintEmptyList(HWND h, HDC dc) {
    RECT r;
    GetClientRect(h, &r);
    if (!d2Begin(dc, r)) return;
    fillRect(r, C_CARD);
    bool isPeers = h == g_peers;
    if (!isPeers) {
        D2D1_ROUNDED_RECT rr{D2D1::RectF((float)r.left + S(6), (float)r.top + S(6), (float)r.right - S(6), (float)r.bottom - S(6)),
                             (float)S(12), (float)S(12)};
        g_rt->FillRoundedRectangle(rr, br(C_FIELD));
        g_rt->DrawRoundedRectangle(rr, br(RGB(203, 213, 225)), (float)S(2), g_dash);
    }
    int cy = (r.top + r.bottom) / 2;
    RECT t1 = {r.left + S(10), cy - S(46), r.right - S(10), cy - S(6)};
    RECT t2 = {r.left + S(10), cy - S(4), r.right - S(10), cy + S(18)};
    RECT t3 = {r.left + S(10), cy + S(18), r.right - S(10), cy + S(40)};
    text(isPeers ? L"◌" : L"⇩", t1, TF_ICON, isPeers ? C_MUTED : C_ACCENT, 1, 2);
    text(isPeers ? L"Cihaz aranıyor…" : L"Dosyaları buraya sürükleyip bırakın", t2, TF_BOLD, C_TEXT, 1);
    text(isPeers ? L"Aynı ağdaki cihazlar burada görünür" : L"ya da aşağıdaki butonlarla ekleyin", t3, TF_SM, C_MUTED, 1);
    d2End();
}

// Boş liste mesajı ve buton üzerine gelme efekti
static LRESULT CALLBACK SubProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR kind) {
    if (kind == 1) {
        switch (msg) {
        case WM_MOUSEMOVE:
            if (g_hot != h) {
                HWND old = g_hot;
                g_hot = h;
                if (old) InvalidateRect(old, nullptr, FALSE);
                InvalidateRect(h, nullptr, FALSE);
                TRACKMOUSEEVENT t{sizeof t, TME_LEAVE, h, 0};
                TrackMouseEvent(&t);
            }
            break;
        case WM_MOUSELEAVE:
            if (g_hot == h) { g_hot = nullptr; InvalidateRect(h, nullptr, FALSE); }
            break;
        case WM_SETCURSOR:
            SetCursor(LoadCursor(nullptr, IDC_HAND));
            return TRUE;
        case WM_LBUTTONDBLCLK:
            msg = WM_LBUTTONDOWN;  // hızlı tıklamalar kaybolmasın
            break;
        case WM_ERASEBKGND:
            return 1;
        }
    } else if (kind == 2) {
        if (msg == WM_ERASEBKGND && SendMessageW(h, LB_GETCOUNT, 0, 0) == 0) return 1;
        if (msg == WM_PAINT && SendMessageW(h, LB_GETCOUNT, 0, 0) == 0) {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(h, &ps);
            paintEmptyList(h, dc);
            EndPaint(h, &ps);
            return 0;
        }
        if (msg == WM_SIZE) InvalidateRect(h, nullptr, FALSE);
    }
    return DefSubclassProc(h, msg, wp, lp);
}

// ---- Pencere
static HWND mk(const wchar_t* cls, const wchar_t* text, DWORD style, int id, DWORD ex = 0, HFONT f = nullptr) {
    HWND h = CreateWindowExW(ex, cls, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 0, 0, g_hwnd,
                             (HMENU)(INT_PTR)id, GetModuleHandleW(nullptr), nullptr);
    SendMessageW(h, WM_SETFONT, (WPARAM)(f ? f : g_font), TRUE);
    return h;
}
static HWND mkButton(const wchar_t* text, int id) {
    HWND h = mk(L"BUTTON", text, BS_OWNERDRAW | WS_TABSTOP, id);
    SetWindowSubclass(h, SubProc, 1, 1);
    return h;
}

static void layout(int w, int h) {
    auto mv = [](int id, int x, int y, int cx, int cy) {
        SetWindowPos(GetDlgItem(g_hwnd, id), nullptr, x, y, cx, cy, SWP_NOZORDER | SWP_NOACTIVATE);
    };
    int H = S(HEADER_H), m = S(20), gap = S(16);
    int leftW = S(290), BH = S(132), FH = S(44);

    mv(ID_OPEN, w - m - S(190), (H - S(38)) / 2, S(190), S(38));

    int top = H + m;
    int bottomTop = h - FH - BH;
    int cardsBottom = bottomTop - gap;
    g_rcDev = {m, top, m + leftW, cardsBottom};
    g_rcQueue = {m + leftW + gap, top, w - m, cardsBottom};
    g_rcBottom = {m, bottomTop, w - m, h - FH};
    g_rcFooter = {m, h - FH, w - m, h};

    // Cihazlar kartı
    mv(ID_PEERS, g_rcDev.left + S(8), top + S(50), leftW - S(16), cardsBottom - top - S(50) - S(66));
    g_rcIp = {g_rcDev.left + S(14), cardsBottom - S(54), g_rcDev.right - S(14), cardsBottom - S(14)};
    int eh = S(22);
    mv(ID_IP, g_rcIp.left + S(42), (g_rcIp.top + g_rcIp.bottom - eh) / 2, g_rcIp.right - g_rcIp.left - S(52), eh);

    // Gönderilecekler kartı
    int qx = g_rcQueue.left, qw = g_rcQueue.right - g_rcQueue.left;
    mv(ID_QUEUE, qx + S(8), top + S(50), qw - S(16), cardsBottom - top - S(50) - S(66));
    int by = cardsBottom - S(52), bx = qx + S(14), bh = S(36);
    mv(ID_ADDFILE, bx, by, S(124), bh); bx += S(124) + S(8);
    mv(ID_ADDDIR, bx, by, S(124), bh); bx += S(124) + S(8);
    mv(ID_REMOVE, bx, by, S(92), bh); bx += S(92) + S(8);
    mv(ID_CLEAR, bx, by, S(92), bh);

    // Alt kart
    g_rcSend = {g_rcBottom.left + S(18), bottomTop + (BH - S(58)) / 2, g_rcBottom.left + S(18) + leftW - S(36) + S(10),
                bottomTop + (BH + S(58)) / 2};
    mv(ID_SEND, g_rcSend.left, g_rcSend.top, g_rcSend.right - g_rcSend.left, g_rcSend.bottom - g_rcSend.top);
    int rx = g_rcSend.right + S(28), rw = g_rcBottom.right - S(22) - rx;
    mv(ID_PROGRESS, rx, bottomTop + S(82), rw, S(10));
    mv(ID_STATUS, rx, bottomTop + S(98), rw, S(22));

    // Alt bilgi
    mv(ID_AUTOSTART, w - m - S(190), h - FH + S(8), S(190), FH - S(16));
}

static void paintMain(HDC hdc, int w, int h) {
    RECT all = {0, 0, w, h};
    if (!d2Begin(hdc, all)) return;
    fillRect(all, C_BG);

    // Başlık bandı
    int H = S(HEADER_H);
    RECT hr = {0, 0, w, H};
    fillRect(hr, C_ACCENT);
    RECT t = {S(24), S(12), w - S(230), S(48)};
    text(L"LAN Transfer", t, TF_TITLE, C_WHITE);
    RECT t2 = {S(25), S(48), w - S(230), S(72)};
    text(g_infoText, t2, TF_BODY, C_HDR_SUB);

    // Cihazlar kartı
    card(g_rcDev);
    RECT dt = {g_rcDev.left + S(18), g_rcDev.top + S(14), g_rcDev.right - S(110), g_rcDev.top + S(40)};
    text(L"Cihazlar", dt, TF_H, C_TEXT);
    pill(std::to_wstring(g_peerList.size()) + L" çevrimiçi", g_rcDev.right - S(16), g_rcDev.top + S(27));
    fillRound(g_rcIp, S(9), C_FIELD, 1, C_BORDER);
    RECT il = {g_rcIp.left + S(12), g_rcIp.top, g_rcIp.left + S(40), g_rcIp.bottom};
    text(L"IP", il, TF_BOLD, C_MUTED);

    // Gönderilecekler kartı
    card(g_rcQueue);
    RECT qt = {g_rcQueue.left + S(18), g_rcQueue.top + S(14), g_rcQueue.right - S(120), g_rcQueue.top + S(40)};
    text(L"Gönderilecekler", qt, TF_H, C_TEXT);
    if (!g_items.empty())
        pill(std::to_wstring(g_items.size()) + L" öğe", g_rcQueue.right - S(16), g_rcQueue.top + S(27));

    // Alt kart: istatistikler
    card(g_rcBottom);
    int rx = g_rcSend.right + S(28), rw = g_rcBottom.right - S(22) - rx, cw = rw / 3;
    const wchar_t* labels[] = {L"HIZ", L"AKTARILAN", L"KALAN SÜRE"};
    const std::wstring* vals[] = {&g_st.speed, &g_st.done, &g_st.eta};
    for (int i = 0; i < 3; i++) {
        RECT lr = {rx + i * cw, g_rcBottom.top + S(16), rx + (i + 1) * cw - S(8), g_rcBottom.top + S(34)};
        text(labels[i], lr, TF_SM, C_MUTED);
        RECT vr = {rx + i * cw, g_rcBottom.top + S(34), rx + (i + 1) * cw - S(8), g_rcBottom.top + S(70)};
        text(*vals[i], vr, TF_BIG, C_TEXT);
    }

    // Alt bilgi
    RECT fr = {g_rcFooter.left + S(4), g_rcFooter.top, g_rcFooter.right - S(210), g_rcFooter.bottom};
    text(L"Gelen dosyalar:  " + g_saveDir, fr, TF_SM, C_MUTED);
    d2End();
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (WM_TASKBARCREATED && msg == WM_TASKBARCREATED) { trayAdd(); return 0; }
    switch (msg) {
    case WM_CREATE: {
        g_hwnd = hwnd;
        // Başlık çubuğunu başlık bandıyla aynı renge boya (Windows 11)
        COLORREF cap = C_ACCENT, txt = RGB(255, 255, 255);
        DwmSetWindowAttribute(hwnd, 35 /*DWMWA_CAPTION_COLOR*/, &cap, sizeof cap);
        DwmSetWindowAttribute(hwnd, 36 /*DWMWA_TEXT_COLOR*/, &txt, sizeof txt);
        DwmSetWindowAttribute(hwnd, 34 /*DWMWA_BORDER_COLOR*/, &cap, sizeof cap);

        std::wstring ips;
        for (auto& i : localInterfaces())
            if ((i.ip >> 16) != 0xA9FE) ips += (ips.empty() ? L"" : L"  ·  ") + ipStr(i.ip);
        g_infoText = g_name + L"     " + ips;
        g_autoStart = isAutoStart();

        mkButton(L"Gelen Klasörü Aç", ID_OPEN);
        g_peers = mk(L"LISTBOX", L"", WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS, ID_PEERS);
        g_queue = mk(L"LISTBOX", L"", WS_VSCROLL | LBS_EXTENDEDSEL | LBS_NOINTEGRALHEIGHT | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS, ID_QUEUE);
        SetWindowSubclass(g_peers, SubProc, 2, 2);
        SetWindowSubclass(g_queue, SubProc, 2, 2);
        g_ip = mk(L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, ID_IP);
        SendMessageW(g_ip, EM_SETCUEBANNER, TRUE, (LPARAM)L"elle adres girin (isteğe bağlı)");
        mkButton(L"+  Dosya Ekle", ID_ADDFILE);
        mkButton(L"+  Klasör Ekle", ID_ADDDIR);
        mkButton(L"Kaldır", ID_REMOVE);
        mkButton(L"Temizle", ID_CLEAR);
        g_send = mkButton(L"Gönder", ID_SEND);
        g_progress = mk(L"STATIC", L"", SS_OWNERDRAW, ID_PROGRESS);
        g_status = mk(L"STATIC", L"Hazır", SS_LEFTNOWORDWRAP | SS_PATHELLIPSIS, ID_STATUS, 0, g_fontSm);
        mkButton(L"Windows ile başla", ID_AUTOSTART);
        DragAcceptFiles(hwnd, TRUE);
        SetTimer(hwnd, 1, 1000, nullptr);
        trayAdd();
        return 0;
    }
    case WM_SIZE:
        if (wp != SIZE_MINIMIZED) {
            layout(LOWORD(lp), HIWORD(lp));
            // Boyut değişince her şeyi baştan çiz (eski çizimler ekranda kalmasın)
            RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN);
        }
        return 0;
    case WM_GETMINMAXINFO:
        ((MINMAXINFO*)lp)->ptMinTrackSize = {S(880), S(620)};
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        paintMain(hdc, rc.right, rc.bottom);  // Direct2D kendi arka tamponunu kullanır
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_CTLCOLORSTATIC:
        SetBkColor((HDC)wp, C_CARD);
        SetTextColor((HDC)wp, C_MUTED);
        return (LRESULT)g_cardBrush;
    case WM_CTLCOLOREDIT:
        SetBkColor((HDC)wp, C_FIELD);
        SetTextColor((HDC)wp, C_TEXT);
        return (LRESULT)g_fieldBrush;
    case WM_CTLCOLORLISTBOX:
        return (LRESULT)g_cardBrush;
    case WM_MEASUREITEM: {
        auto mi = (MEASUREITEMSTRUCT*)lp;
        mi->itemHeight = S(56);
        return TRUE;
    }
    case WM_DRAWITEM: {
        auto d = (DRAWITEMSTRUCT*)lp;
        switch (d->CtlType) {
        case ODT_BUTTON: drawButton(d); return TRUE;
        case ODT_LISTBOX: if (d->CtlID == ID_PEERS) drawPeer(d); else drawQueueItem(d); return TRUE;
        case ODT_STATIC: drawProgress(d); return TRUE;
        }
        break;
    }
    case WM_TIMER:
        refreshPeers();
        return 0;
    case WM_DROPFILES: {
        HDROP drop = (HDROP)wp;
        UINT n = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
        for (UINT i = 0; i < n; i++) {
            UINT len = DragQueryFileW(drop, i, nullptr, 0);
            std::wstring p(len, 0);
            DragQueryFileW(drop, i, &p[0], len + 1);
            addItem(p);
        }
        DragFinish(drop);
        return 0;
    }
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case ID_ADDFILE: pickItems(false); break;
        case ID_ADDDIR: pickItems(true); break;
        case ID_CLEAR: if (!g_busy) clearQueue(); break;
        case ID_REMOVE: {
            if (g_busy) break;
            int cnt = (int)SendMessageW(g_queue, LB_GETSELCOUNT, 0, 0);
            if (cnt <= 0) break;
            std::vector<int> sel(cnt);
            SendMessageW(g_queue, LB_GETSELITEMS, cnt, (LPARAM)sel.data());
            std::sort(sel.rbegin(), sel.rend());
            for (int i : sel) if (i < (int)g_items.size()) g_items.erase(g_items.begin() + i);
            rebuildQueue();
            break;
        }
        case ID_SEND: doSend(); break;
        case ID_OPEN: ShellExecuteW(hwnd, L"open", g_saveDir.c_str(), nullptr, nullptr, SW_SHOWNORMAL); break;
        case ID_PEERS:
            if (HIWORD(wp) == LBN_SELCHANGE) SetWindowTextW(g_ip, L"");
            if (HIWORD(wp) == LBN_DBLCLK) doSend();
            break;
        case ID_AUTOSTART:
        case ID_TRAY_AUTOSTART: setAutoStart(!isAutoStart()); break;
        case ID_TRAY_SHOW: showMain(); break;
        case ID_TRAY_EXIT: DestroyWindow(hwnd); break;
        }
        return 0;
    case WM_APP_TRAY:
        switch (LOWORD(lp)) {
        case WM_LBUTTONUP:
        case WM_LBUTTONDBLCLK: showMain(); break;
        case WM_RBUTTONUP:
        case WM_CONTEXTMENU: trayMenu(); break;
        }
        return 0;
    case WM_APP_SHOW:
        showMain();
        return 0;
    case WM_APP_STATUS: {
        auto s = (std::wstring*)lp;
        SetWindowTextW(g_status, s->c_str());
        delete s;
        return 0;
    }
    case WM_APP_PROGRESS: {
        auto p = (Prog*)lp;
        int pm = p->total ? (int)(p->done * 1000 / p->total) : 1000;
        setProg(pm);
        if (!p->label.empty()) SetWindowTextW(g_status, p->label.c_str());
        bool fin = p->done >= p->total;
        std::wstring rate = fmtBytes(p->speed) + L"/s";
        g_st.speed = std::wstring(p->sending ? L"↑ " : L"↓ ") + rate;
        g_st.done = fmtBytes((double)p->done) + L" / " + fmtBytes((double)p->total);
        g_st.eta = fin ? std::wstring(L"Tamamlandı")
                       : p->speed > 0 ? fmtTime((double)(p->total - p->done) / p->speed) : std::wstring(L"…");
        InvalidateRect(hwnd, &g_rcBottom, FALSE);
        if (!fin)
            trayTip(std::wstring(p->sending ? L"Gönderiliyor" : L"Alınıyor") + L"  %" + std::to_wstring(pm / 10) +
                    L"  ·  " + rate + L"  ·  " + g_st.eta);
        delete p;
        return 0;
    }
    case WM_APP_SENT: {
        auto s = (std::wstring*)lp;
        g_busy = false;
        SetWindowTextW(g_send, L"Gönder");
        EnableWindow(g_send, TRUE);
        SetWindowTextW(g_status, s->c_str());
        trayTip(L"LAN Transfer");
        if (wp) {
            clearQueue();
            if (!IsWindowVisible(hwnd)) trayBalloon(L"Gönderim tamamlandı", *s);
        } else {
            resetStats();
            if (IsWindowVisible(hwnd)) MessageBoxW(hwnd, s->c_str(), L"LAN Transfer", MB_ICONERROR);
            else trayBalloon(L"Gönderim başarısız", *s);
        }
        delete s;
        return 0;
    }
    case WM_APP_RECEIVED: {
        auto s = (std::wstring*)lp;
        SetWindowTextW(g_status, s->c_str());
        trayTip(L"LAN Transfer");
        trayBalloon(L"Dosya alındı", *s);
        MessageBeep(MB_ICONASTERISK);
        FLASHWINFO fi{sizeof fi, hwnd, FLASHW_ALL | FLASHW_TIMERNOFG, 3, 0};
        FlashWindowEx(&fi);
        delete s;
        return 0;
    }
    case WM_CLOSE:
        ShowWindow(hwnd, SW_HIDE);
        if (!g_closeHintShown) {
            g_closeHintShown = true;
            trayBalloon(L"LAN Transfer arka planda çalışıyor",
                        L"Dosya almaya devam ediyor. Çıkmak için tepsi simgesine sağ tıklayın.");
        }
        return 0;
    case WM_DESTROY:
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static int CALLBACK fontEnumProc(const LOGFONTW*, const TEXTMETRICW*, DWORD, LPARAM lp) {
    *(bool*)lp = true;
    return 0;
}
static bool fontExists(const wchar_t* face) {
    LOGFONTW lf{};
    lf.lfCharSet = DEFAULT_CHARSET;
    lstrcpynW(lf.lfFaceName, face, LF_FACESIZE);
    bool found = false;
    HDC dc = GetDC(nullptr);
    EnumFontFamiliesExW(dc, &lf, (FONTENUMPROCW)fontEnumProc, (LPARAM)&found, 0);
    ReleaseDC(nullptr, dc);
    return found;
}
static HFONT mkFont(const wchar_t* face, int pt, int weight) {
    LOGFONTW lf{};
    lf.lfHeight = -MulDiv(pt, g_dpi, 72);
    lf.lfWeight = weight;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfQuality = CLEARTYPE_QUALITY;
    lstrcpynW(lf.lfFaceName, face, LF_FACESIZE);
    return CreateFontIndirectW(&lf);
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR cmd, int show) {
    bool startHidden = cmd && wcsstr(cmd, L"/tray");

    // Tek kopya: ikinci açılışta mevcut pencereyi öne getir
    CreateMutexW(nullptr, TRUE, L"LANTransfer_SingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND other = FindWindowW(L"LANTransferWnd", nullptr);
        if (other && !startHidden) PostMessageW(other, WM_APP_SHOW, 0, 0);
        return 0;
    }

    SetProcessDPIAware();
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    INITCOMMONCONTROLSEX icc{sizeof icc, ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&icc);
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    WM_TASKBARCREATED = RegisterWindowMessageW(L"TaskbarCreated");

    HDC dc = GetDC(nullptr);
    g_dpi = GetDeviceCaps(dc, LOGPIXELSX);
    ReleaseDC(nullptr, dc);
    const wchar_t* body = fontExists(L"Segoe UI Variable Text") ? L"Segoe UI Variable Text" : L"Segoe UI";
    const wchar_t* disp = fontExists(L"Segoe UI Variable Display") ? L"Segoe UI Variable Display" : L"Segoe UI";
    g_font = mkFont(body, 10, FW_NORMAL);
    g_fontSm = mkFont(body, 9, FW_NORMAL);
    d2Init(body, disp);
    g_bgBrush = CreateSolidBrush(C_BG);
    g_cardBrush = CreateSolidBrush(C_CARD);
    g_fieldBrush = CreateSolidBrush(C_FIELD);

    wchar_t nm[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD nl = MAX_COMPUTERNAME_LENGTH + 1;
    g_name = GetComputerNameW(nm, &nl) ? nm : L"Windows PC";

    PWSTR dl = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Downloads, 0, nullptr, &dl))) {
        g_saveDir = std::wstring(dl) + L"\\Transfer";
        CoTaskMemFree(dl);
    } else {
        wchar_t prof[MAX_PATH];
        GetEnvironmentVariableW(L"USERPROFILE", prof, MAX_PATH);
        g_saveDir = std::wstring(prof) + L"\\Downloads\\Transfer";
    }
    SHCreateDirectoryExW(nullptr, g_saveDir.c_str(), nullptr);

    // Otomatik başlatma açıksa exe yolu değişmiş olabilir; kaydı güncelle
    if (isAutoStart()) setAutoStart(true);

    g_iconLg = LoadIconW(inst, MAKEINTRESOURCEW(1));
    g_iconSm = (HICON)LoadImageW(inst, MAKEINTRESOURCEW(1), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON),
                                 GetSystemMetrics(SM_CYSMICON), 0);

    WNDCLASSEXW wc{sizeof wc};
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = g_bgBrush;
    wc.hIcon = g_iconLg;
    wc.hIconSm = g_iconSm;
    wc.lpszClassName = L"LANTransferWnd";
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(WS_EX_ACCEPTFILES, wc.lpszClassName, L"LAN Transfer",
                                WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT, S(960), S(700),
                                nullptr, nullptr, inst, nullptr);
    if (!startHidden) ShowWindow(hwnd, show);

    std::thread(broadcastThread).detach();
    std::thread(discoveryThread).detach();
    std::thread(serverThread).detach();

    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0)) {
        if (IsDialogMessageW(hwnd, &m)) continue;
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    WSACleanup();
    return 0;
}
