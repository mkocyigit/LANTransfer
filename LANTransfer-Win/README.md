# LAN Transfer

Aynı yerel ağdaki (aynı modem / Wi‑Fi) Windows ve Mac bilgisayarlar arasında, internete çıkmadan ve hesap açmadan dosya ve klasör gönderip almanızı sağlayan küçük bir uygulama.

Windows sürümü tek bir `LANTransfer.exe` dosyasıdır. Kurulum gerektirmez, ek DLL istemez.

---

## Özellikler

### Aktarım
- **Dosya ve klasör gönderme.** Klasörler alt klasörleriyle birlikte, yapısı korunarak gönderilir.
- **Çift yönlü.** Windows ↔ Mac ve Windows ↔ Windows arasında çalışır. İki sürüm aynı protokolü kullanır.
- **Büyük dosya desteği.** Boyut sınırı yoktur. Veri 1 MB'lık parçalar hâlinde, doğrudan iki bilgisayar arasında akar.
- **Teslim onayı.** Alıcı tüm dosyaları diske yazınca "OK" onayı gönderir. Gönderen taraf ancak bu onayı alınca aktarımı başarılı sayar.
- **Güvenli dosya adları.** Gelen dosya adlarındaki geçersiz karakterler temizlenir ve `..` gibi klasör dışına çıkmaya çalışan yollar engellenir.
- **Üzerine yazmaz.** Aynı adda bir dosya ya da klasör varsa yenisi `ad (1).uzantı` biçiminde kaydedilir.

### Cihaz bulma
- **Otomatik keşif.** Ağdaki diğer LAN Transfer'ler birkaç saniye içinde "Cihazlar" listesinde görünür.
- Modem yayın (broadcast) paketlerini engelliyorsa uygulama alt ağdaki her adrese ayrıca sorar, böyle ağlarda da cihazları bulur.
- **Varsayılan seçim.** Listedeki ilk cihaz otomatik seçili gelir.
- **Elle IP.** Cihaz listede görünmüyorsa IP adresini yazarak doğrudan gönderebilirsiniz.

### Hız ve ilerleme
- **Anlık hız** (MB/s), **aktarılan / toplam boyut** ve **kalan süre**, gönderirken de alırken de gösterilir.
- Aktarım bitince toplam boyut, geçen süre ve ortalama hız özetlenir.

### Arayüz
- Direct2D / DirectWrite ile çizilen modern arayüz: yuvarlak köşeli kartlar, başlık bandıyla aynı renkte pencere çubuğu, üzerine gelince renk değiştiren butonlar.
- Cihazlar baş harfli renkli dairelerle, dosyalar kendi Windows simgeleri ve boyutlarıyla listelenir.
- **Sürükle‑bırak.** Dosya ve klasörleri pencerenin herhangi bir yerine bırakabilirsiniz.
- Yüksek DPI (ekran ölçeklendirme) desteği.

### Arka planda çalışma
- **Sistem tepsisi simgesi.** Pencereyi kapatınca uygulama kapanmaz, saatin yanındaki tepsiye iner ve dosya almaya devam eder.
  - Simgeye sol tıklayınca pencere açılır.
  - Sağ tıklayınca menü açılır: Göster, Gelen Klasörü Aç, Windows ile başla, Çıkış.
  - Aktarım sırasında simgenin üzerine gelince yüzde, hız ve kalan süre görünür.
- **Bildirimler.** Dosya geldiğinde, ya da pencere gizliyken gönderim bitince veya başarısız olunca Windows bildirimi çıkar.
- **Windows ile başla.** Açıkken uygulama Windows açılışında pencere göstermeden doğrudan tepsiye başlar.
- **Tek kopya.** Uygulama zaten açıkken tekrar çalıştırırsanız yeni kopya açılmaz, mevcut pencere öne gelir.

---

## Kullanım

### İlk çalıştırma (Windows)
1. `LANTransfer.exe` dosyasını kalıcı bir klasöre koyun, örneğin `C:\Program Files\LANTransfer\` ya da Belgeler.
2. Çift tıklayarak açın.
3. **Windows Güvenlik Duvarı izni sorarsa "İzin ver" deyin.** En azından **Özel ağlar** işaretli olmalıdır. İzin verilmezse diğer cihazlar bu bilgisayarı göremez ve ona dosya gönderemez.
4. İsterseniz pencerenin sağ altındaki **Windows ile başla** düğmesini açın.
5. Tepsi simgesi görünmüyorsa saatin yanındaki **^** okuna tıklayın ve LAN Transfer simgesini görev çubuğuna sürükleyin. Kalıcı olarak göstermek için: **Ayarlar → Kişiselleştirme → Görev çubuğu → Diğer sistem tepsisi simgeleri → LAN Transfer: Açık**.

### Dosya göndermek
1. Karşı bilgisayarda da LAN Transfer'in açık olduğundan emin olun.
2. **Cihazlar** listesinden hedefi seçin. İlk cihaz zaten seçili gelir.
   Listede yoksa **IP** kutusuna karşı bilgisayarın adresini yazın. Bu adres, karşı uygulamanın başlık bandında yazar.
3. Dosya ya da klasörleri pencereye sürükleyin, veya **+ Dosya Ekle** / **+ Klasör Ekle** butonlarını kullanın.
   Yanlış eklediklerinizi seçip **Kaldır**, hepsini silmek için **Temizle**.
4. **Gönder**'e basın. Cihaza çift tıklamak da gönderimi başlatır.

### Dosya almak
Hiçbir şey yapmanız gerekmez. Uygulama açık (ya da tepside) olduğu sürece gelen dosyalar otomatik kaydedilir:

- **Windows:** `İndirilenler\Transfer` (`C:\Users\<kullanıcı>\Downloads\Transfer`)
- **Mac:** `~/Downloads/Transfer`

Klasörü açmak için **Gelen Klasörü Aç** butonunu ya da tepsi menüsünü kullanın.

### Mac tarafında yapılması gerekenler
1. LAN Transfer'i ilk açtığınızda macOS **"yerel ağdaki cihazları bulmak"** için izin ister. **İzin Ver** deyin.
   Kaçırdıysanız: **Sistem Ayarları → Gizlilik ve Güvenlik → Yerel Ağ → LANTransfer: Açık**.
2. Mac'in dosya **alabilmesi** için İndirilenler klasörüne yazma izni gerekir:
   **Sistem Ayarları → Gizlilik ve Güvenlik → Dosyalar ve Klasörler → LANTransfer → İndirilenler Klasörü: Açık**.
   Bu izin yoksa Windows'tan Mac'e gönderim "karşı taraf onay vermedi" hatasıyla biter.
3. macOS güvenlik duvarı açıksa LAN Transfer'in gelen bağlantılara izin verildiğinden emin olun:
   **Sistem Ayarları → Ağ → Güvenlik Duvarı → Seçenekler**.

---

## Sorun giderme

| Belirti | Çözüm |
|---|---|
| Cihaz listede görünmüyor | İki bilgisayar aynı ağda mı? Misafir Wi‑Fi'lar ve bazı modemlerdeki "AP / istemci yalıtımı" cihazların birbirini görmesini engeller. Güvenlik duvarı izinlerini kontrol edin. Gerekirse IP'yi elle yazın. |
| "… adresine bağlanılamadı" | Karşı tarafta uygulama kapalı, ya da güvenlik duvarı 50001 numaralı TCP portunu engelliyor. |
| "Karşı taraf onay vermedi" | Alıcı dosyayı diske yazamadı. Mac'te İndirilenler izni yoktur ya da diskte yer kalmamıştır. Alıcının pencere altındaki durum satırında hatanın nedeni yazar. |
| "Keşif portu (50000) / Aktarım portu (50001) açılamadı" | Uygulamanın başka bir kopyası ya da aynı portu kullanan başka bir program açık. |
| Tepsi simgesi yok | Windows 11 yeni simgeleri **^** okunun içine gizler. Yukarıdaki "İlk çalıştırma" 5. adıma bakın. |

**Kullanılan portlar:** UDP **50000** (cihaz bulma) ve TCP **50001** (aktarım). Güvenlik duvarında elle kural açmanız gerekirse bu iki portu açın.

> **Not:** Aktarım şifrelenmez. Uygulamayı yalnızca güvendiğiniz ev ya da ofis ağlarında kullanın. Açık ve halka açık Wi‑Fi'larda kullanmayın.

---

## Derleme (geliştiriciler için)

Gerekenler: **MinGW‑w64 (g++)** ya da **Visual Studio Build Tools**. `build.bat` hangisinin kurulu olduğunu kendisi anlar.

```bat
cd LANTransfer-Win
build.bat
```

- Visual Studio ile derleyecekseniz `build.bat`'ı **"x64 Native Tools Command Prompt for VS"** içinden çalıştırın.
- MinGW komutu (elle derlemek için):
  ```
  windres app.rc -O coff -o app.res.o
  g++ -O2 -std=c++17 -municode -mwindows main.cpp app.res.o -o LANTransfer.exe ^
      -lws2_32 -liphlpapi -lcomctl32 -lole32 -luuid -lshell32 -ladvapi32 -ldwmapi -ld2d1 -ldwrite -static
  ```

### Dosyalar
| Dosya | Açıklama |
|---|---|
| `main.cpp` | Uygulamanın tamamı (ağ, aktarım, arayüz). Win32 + Direct2D/DirectWrite. |
| `app.rc` | Kaynak dosyası: simge ve manifest. |
| `app.manifest` | Modern Windows kontrolleri (Common Controls 6), DPI farkındalığı, UTF‑8. |
| `app.ico` | Uygulama simgesi. |
| `build.bat` | Derleme betiği. |

Mac sürümünün kaynak kodu ayrı klasördedir (`mac/Sources/LANTransfer/`). Mac'te `build_app.sh` ile derlenir.

### Protokol (özet)
Tüm sayılar big‑endian'dır.

- **Keşif (UDP 50000):** `"LNT2"` + tür (`A` = duyuru, `R` = cevap) + cihaz adı (UTF‑8)
- **Aktarım (TCP 50001):**
  `"LNT2"` · `u16` ad uzunluğu · ad · `u32` dosya sayısı ·
  her dosya için: `u16` yol uzunluğu · yol (`/` ayraçlı, UTF‑8) · `u64` boyut ·
  ardından dosya içerikleri sırayla. Alıcı sonunda `"OK"` gönderir.
