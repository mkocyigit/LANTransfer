# LAN Transfer

Aynı yerel ağdaki Windows ve Mac bilgisayarlar arasında internete çıkmadan, hesap açmadan dosya ve klasör aktarımı. İki sürüm aynı protokolü kullanır: Windows ↔ Mac, Windows ↔ Windows, Mac ↔ Mac.

Bu repo **yalnızca kaynak kodu** içerir; hazır `.exe` veya `.app` yoktur. Aşağıdaki adımlarla kendiniz derleyin.

| Klasör | Platform | Ayrıntılar |
|---|---|---|
| [`LANTransfer-Win/`](LANTransfer-Win/) | Windows (C++ / Win32 + Direct2D) | [README](LANTransfer-Win/README.md) |
| [`LANTransfer-Mac/`](LANTransfer-Mac/) | macOS (Swift / SwiftUI) | [README](LANTransfer-Mac/README.md) |

---

## Windows sürümünü derleme

### Gereksinimler
- Windows 10 veya 11 (64‑bit)
- Aşağıdaki derleyicilerden **biri**:
  - **MinGW‑w64** (g++ 9+ ve `windres`), örn. [MSYS2](https://www.msys2.org/) ile: `pacman -S mingw-w64-ucrt-x86_64-gcc`
  - **Visual Studio 2019+ / Build Tools**, "C++ ile masaüstü geliştirme" iş yükü ve Windows 10/11 SDK
- Ek kütüphane gerekmez; kullanılan tüm kütüphaneler (Winsock, IP Helper, Direct2D, DirectWrite, DWM, Shell, COM) Windows SDK / MinGW ile gelir.

### Derleme
```bat
cd LANTransfer-Win
build.bat
```
`build.bat` önce `cl` (Visual Studio) arar, yoksa MinGW'yi kullanır. Visual Studio için komutu **"x64 Native Tools Command Prompt for VS"** içinden çalıştırın.

MinGW ile elle:
```bat
windres app.rc -O coff -o app.res.o
g++ -O2 -std=c++17 -municode -mwindows main.cpp app.res.o -o LANTransfer.exe ^
    -lws2_32 -liphlpapi -lcomctl32 -lole32 -luuid -lshell32 -ladvapi32 -ldwmapi -ld2d1 -ldwrite -static
```
Çıktı: tek parça, kurulum ve DLL gerektirmeyen `LANTransfer.exe`.

---

## macOS sürümünü derleme

### Gereksinimler
- macOS 13 (Ventura) veya üzeri
- Xcode 15+ veya Xcode Command Line Tools (Swift 5.9+): `xcode-select --install`
- Harici bağımlılık yoktur (yalnızca Apple çerçeveleri).

### Derleme
```bash
cd LANTransfer-Mac/mac
chmod +x build_app.sh
./build_app.sh                                    # LANTransfer.app oluşturur (ad-hoc imzalı)
ditto LANTransfer.app /Applications/LANTransfer.app
```
Simgeyi yeniden üretmek isterseniz: `cd LANTransfer-Mac/icon && swift make_icon.swift`

---

## Ağ gereksinimleri
- İki cihaz aynı yerel ağda olmalı (misafir ağı / istemci yalıtımı kapalı).
- Portlar: **UDP 50000** (cihaz bulma), **TCP 50001** (aktarım). Güvenlik duvarında izin verin.
- Aktarım **şifrelenmez**; yalnızca güvendiğiniz ağlarda kullanın.

## Lisans
[Unlicense](LICENSE) — kamu malı. Kopyalayın, değiştirin, satın, dilediğiniz gibi kullanın; izin veya atıf gerekmez.
