# LAN Transfer

Aynı ağdaki (aynı Wi-Fi veya kablolu ağ) bilgisayarlar arasında **sürükle-bırak ile dosya ve klasör aktarımı** yapan macOS uygulaması. İnternet, hesap ya da bulut gerekmez; dosyalar doğrudan bir bilgisayardan diğerine gider.

![Simge](icon/icon_512.png)

---

## Özellikler

### Aktarım
- **Sürükle-bırak:** Dosya ve klasörleri pencerenin herhangi bir yerine bırakın.
- **Klasör desteği:** Klasörler, içindeki tüm alt klasör ve dosyalarla birlikte, yapısı korunarak gönderilir.
- **Çoklu seçim:** Aynı anda istediğiniz kadar dosya ve klasör gönderebilirsiniz.
- **Ekle… düğmesi:** Sürüklemek yerine dosya/klasör seçme penceresiyle de ekleyebilirsiniz.
- **Kuyruk yönetimi:** Bırakılan öğeler simgeleri ve boyutlarıyla listelenir; üzerine gelince ✕ ile tek tek, **Temizle** ile hepsi birden kaldırılır.
- **Başarılı gönderimden sonra** kuyruk otomatik temizlenir.
- **Büyük dosyalar:** Boyut sınırı yoktur; dosyalar parça parça aktarılır.

### Hız ve ilerleme
- İlerleme çubuğu (hem gönderirken hem alırken)
- **Anlık hız** (ör. `12.4 MB/s`)
- **Kalan süre** (ör. `kalan 0:42`)
- **Aktarılan / toplam miktar** (ör. `450 MB / 1.2 GB`)
- Aktarım bitince **özet:** toplam boyut, geçen süre, ortalama hız

### Cihaz bulma
- Ağdaki LAN Transfer çalıştıran bilgisayarlar **otomatik olarak** birkaç saniye içinde listelenir.
- Listedeki **ilk cihaz varsayılan olarak seçilidir**; başka bir cihaza tıklayarak değiştirebilirsiniz.
- Kapanan cihazlar ~8 saniye içinde listeden kalkar.
- Modem yayın paketlerini engellese bile cihazlar bulunur (ağdaki adreslere doğrudan sorgu gönderilir).
- Bir cihazın diğerini görmesi yeterlidir; karşılıklı cevap sayesinde iki taraf da birbirini listeler.
- Cihaz listede görünmezse **IP adresini elle** girerek gönderebilirsiniz.

### Alma
- Gelen dosyalar **onay gerektirmeden** `İndirilenler/Transfer` klasörüne kaydedilir.
- Aynı isimde dosya/klasör varsa üzerine yazılmaz; `dosya (1).txt` gibi yeni bir adla kaydedilir.
- Dosya geldiğinde ses çalar ve Dock simgesi dikkat çeker.
- **Gelenler** düğmesi kayıt klasörünü Finder'da açar.

### Arka planda çalışma
- **Menü çubuğu simgesi (⇄):** Aktarım sırasında yanında yüzde görünür.
- Menüden: durum bilgisi, pencereyi göster, gelenler klasörünü aç, başlangıçta çalıştır, çık.
- Pencereyi kapatmak uygulamayı kapatmaz; arka planda dosya almaya devam eder.
- **Başlangıçta otomatik çalışma:** İlk açılışta etkinleşir; pencerenin üstündeki anahtardan veya menüden kapatılabilir.

---

## Kullanım

### İlk kurulum (her bilgisayarda bir kez)
1. `LANTransfer.app` dosyasını **Uygulamalar** klasörüne kopyalayın ve açın.
2. macOS **"Yerel ağdaki aygıtları bulmak istiyor"** diye sorarsa **İzin Ver** deyin.
   - Yanlışlıkla reddettiyseniz: **Sistem Ayarları → Gizlilik ve Güvenlik → Yerel Ağ → LANTransfer**'i açın ve uygulamayı yeniden başlatın.
3. "Giriş öğesi eklendi" bildirimi çıkabilir; bu, uygulamanın açılışta başlaması içindir.

### Dosya göndermek
1. İki bilgisayarda da LAN Transfer'in açık olduğundan emin olun (menü çubuğunda ⇄ simgesi).
2. Pencereyi açın (menü çubuğu simgesi → **LAN Transfer'i Göster**).
3. Dosya ve klasörleri pencereye **sürükleyip bırakın**.
4. Soldaki listeden hedef bilgisayarı seçin (ilk cihaz zaten seçilidir).
5. **Gönder → CİHAZ-ADI** düğmesine basın (veya **Enter**).
6. Altta hız ve kalan süreyi izleyin; bitince "✓ gönderildi" yazar.

### Dosya almak
Hiçbir şey yapmanıza gerek yok. Uygulama açık (veya menü çubuğunda) olduğu sürece gelen dosyalar **İndirilenler → Transfer** klasörüne kaydedilir.

---

## Sorun giderme

| Sorun | Çözüm |
|---|---|
| Cihazlar birbirini görmüyor | İki bilgisayarın **aynı ağda** olduğunu kontrol edin. Mac'te **Yerel Ağ** iznini açın. Olmazsa karşı tarafın IP adresini elle girin. |
| Gönderiyor ama karşı taraf almıyor | Alıcıdaki **güvenlik yazılımı** (ör. ESET, Little Snitch, LuLu) veya güvenlik duvarı gelen bağlantıları engelliyor olabilir. LAN Transfer'e izin verin ya da ağı "Ev/Güvenilir" olarak işaretleyin. |
| VPN açıkken çalışmıyor | VPN (ör. AdGuard VPN) yerel ağ trafiğini etkileyebilir. VPN ayarlarında yerel ağı hariç tutun veya geçici olarak kapatın. |
| Misafir / kafe ağında çalışmıyor | Bu ağlarda genelde **istemci izolasyonu** açıktır ve cihazlar birbirine bağlanamaz. Ev/ofis ağını kullanın. |
| "Yerel ağ erişimi engelli" uyarısı | **Sistem Ayarları → Gizlilik ve Güvenlik → Yerel Ağ → LANTransfer**'i açın. |
| "Port açılamadı" hatası | Uygulamanın başka bir kopyası açık olabilir. Menüden **Çık** deyip tekrar açın. |
| "… adresine bağlanılamadı" | Karşı bilgisayarın güvenlik duvarı bağlantıyı engelliyor. Karşı taraf Windows ise aşağıdaki **Windows güvenlik duvarı izni** adımlarını uygulayın. |

Gerekli portlar: **UDP 50000** (cihaz bulma), **TCP 50001** (aktarım).

### Windows güvenlik duvarı izni
Mac'ten Windows'a gönderirken **"… adresine bağlanılamadı"** hatası alıyorsanız Windows güvenlik duvarı gelen bağlantıları sessizce engelliyordur. Windows bilgisayarda:

1. LAN Transfer'in **açık** olduğundan emin olun.
2. **Ayarlar → Ağ ve İnternet → Wi-Fi → bağlı ağ → Ağ profili türü: Özel** yapın.
3. Hâlâ olmuyorsa güvenlik duvarına izin kuralı ekleyin. Başlat menüsünde **PowerShell**'i arayın, **Yönetici olarak çalıştır** deyin ve şu iki satırı yapıştırın:
   ```powershell
   New-NetFirewallRule -DisplayName "LAN Transfer TCP" -Direction Inbound -Protocol TCP -LocalPort 50001 -Action Allow -Profile Any
   New-NetFirewallRule -DisplayName "LAN Transfer UDP" -Direction Inbound -Protocol UDP -LocalPort 50000 -Action Allow -Profile Any
   ```
   Bu kurallar, ağ türü ne olursa olsun bu iki porttan gelen bağlantılara izin verir. Yalnızca güvendiğiniz ağlarda kullandığınız bir bilgisayarda yapın.
4. Windows'ta ESET gibi ayrı bir antivirüs/güvenlik duvarı varsa onda da LAN Transfer'e izin verin.

Kuralları daha sonra kaldırmak için (Yönetici PowerShell):
```powershell
Remove-NetFirewallRule -DisplayName "LAN Transfer TCP"
Remove-NetFirewallRule -DisplayName "LAN Transfer UDP"
```

---

## Güvenlik notu
Aktarım **şifrelenmez** ve gelen dosyalar **onaysız** kabul edilir. Uygulamayı yalnızca güvendiğiniz ev veya ofis ağında kullanın; ortak ağlarda (kafe, otel, havalimanı) açık bırakmayın.

---

## Geliştiriciler için

### Gereksinimler
- macOS 13 (Ventura) veya üzeri
- Xcode veya Xcode Command Line Tools (Swift 5.9+)

### Derleme ve kurulum
```bash
cd LANTransfer-Mac/mac
./build_app.sh                                   # mac/LANTransfer.app oluşturur
ditto LANTransfer.app /Applications/LANTransfer.app
```

### Proje yapısı
```
mac/
  Package.swift                  Swift paketi
  build_app.sh                   .app paketi oluşturma, simge ve imzalama
  AppIcon.icns                   uygulama simgesi
  Sources/LANTransfer/
    App.swift                    arayüz, menü çubuğu, başlangıç ayarı
    TransferModel.swift          cihaz bulma, gönderme, alma
    Net.swift                    soket yardımcıları, protokol, hız ölçer
icon/
  make_icon.swift                simgeyi çizen betik (swift make_icon.swift)
  icon_512.png                   512×512 simge
```

### Protokol
Tüm sayılar big-endian.

- **Keşif (UDP 50000):** `"LNT2"` + tür (`A` = duyuru, `R` = cevap) + cihaz adı (UTF-8). Duyuru 2 saniyede bir genel yayına, alt ağ yayınına ve (3 turda bir) alt ağdaki tüm adreslere gönderilir; duyuru alan cihaz göndericiye `R` ile cevap verir.
- **Aktarım (TCP 50001):**
  `"LNT2"` | u16 ad uzunluğu | ad | u32 dosya sayısı |
  her dosya için: u16 yol uzunluğu | yol (`/` ayraçlı, UTF-8) | u64 boyut |
  ardından dosya içerikleri sırayla. Alıcı tümünü kaydettikten sonra `"OK"` gönderir.
- Alıcı yoldaki `..` ve boş bileşenleri atar; dosyalar yalnızca kayıt klasörünün içine yazılabilir.
