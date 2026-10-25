# The Last Watch — Oyun Tasarım Belgesi (GDD)

> Sürüm 0.1 · Taslak
> Tür: Birinci şahıs hikaye odaklı gerilim / gizem
> Motor: Unreal Engine 5 (C++, First Person şablonu)
> Dil: İngilizce (ses + metin), Türkçe altyazı
> Hedef süre: 60–90 dakika, 5 gece

---

## 1. Tek cümlelik özet

1974'te Kuzey Atlantik'teki ıssız bir kaya adada yeni fener bekçisi olarak, kaybolan selefinin izini sürerken fenerin gemileri değil, denizin altındaki bir şeyi uzak tutmak için var olduğunu keşfedersin.

## 2. Tasarım ilkeleri

- **Atmosfer > aksiyon.** Gerilim ışık, ses, sis ve sessizlikten doğar; dövüş yok.
- **Ekranda NPC yok.** Bütün karakterler telsiz, ses kaydı, not ve günlük üzerinden anlatılır.
- **Çevresel anlatım.** Ada her gece biraz değişir; oyuncu hikayeyi mekândan okur.
- **Küçük ama cilalı.** 5 gece, sınırlı sayıda mekân, her biri özenle işlenmiş.
- **Belirsizlik.** Oyuncu sonuna kadar "mantıklı bir açıklama var mı?" diye sorgulamalı.

## 3. Mekân ve dönem

- **Skerrow Rock:** Kuzey Atlantik'te, haritalarda zor görünen hayali bir kaya ada. İskoçya–Norveç arası bir hava; gerçek bir ülkeye bağlı değil.
- **Yıl:** 1974. Soğuk Savaş dönemi; analog teknoloji (telsiz, makaralı kaset, mekanik saatler, gaz lambaları).
- **Hava:** Gri gökyüzü, sürekli rüzgâr, yoğun sis, geceleri yağmur. Gece 4–5'te fırtına.

## 4. Karakterler

| Karakter | Rol | Nasıl görünür |
|---|---|---|
| **Oyuncu (Keeper)** | Yeni atanan fener bekçisi. İsimsiz, yüzsüz. | Birinci şahıs |
| **Margaret** | Anakara sahil güvenliği operatörü. Sıcak, güven veren ses. Aslında Ministry için çalışıyor; görevi bekçiyi bilgisiz ve işinin başında tutmak. | Telsiz |
| **Elias Marr** | Önceki bekçi, eski telsiz operatörü. Sayıları çözdü, gerçeği öğrendi, bir gece feneri söndürdü ve kayboldu. | Günlükler, notlar, Gece 4'ten itibaren telsiz |
| **Sayı yayını** | Tanımsız frekansta sayı okuyan mekanik bir ses. | Telsiz |
| **"O" / The Choir** | Derinlerdeki varlık. Asla tam görülmez. | Hidrofon kayıtları, sisteki ışıklar, ayak izleri |

## 5. Arka plan (oyuncu parça parça öğrenir)

1. **1800'ler:** Skerrow'da gemiler sürekli batar; fener inşa edilir. Denizciler arasında "kayalıkların altındaki koro" efsanesi anlatılır.
2. **1950'ler:** "The Ministry" (adı asla açıkça verilmeyen bir devlet birimi), Sovyet denizaltılarını dinlemek için fenerin altına gizli bir hidrofon istasyonu kurar — **PROJECT VESPER**.
3. **Keşif:** Hidrofonlar denizaltı değil, derinden gelen ritmik bir ses kaydeder. Fenerin belirli dönüş ritmi bu sesi susturur; fener aslında bir **mühürdür**.
4. **Sayı yayınları:** Casus mesajı değil; Ministry'nin bekçiye ilettiği fener zamanlama kodlarıdır.
5. **Bekçiler:** Ministry bekçileri gerçeği bilmeden işe alır. Önceki bekçilerin çoğu "kaybolmuş" olarak kayıtlıdır.
6. **Elias:** Sayıları çözdü, gerçeği anladı, feneri söndürdü — ve kayboldu.

## 6. Temel oynanış döngüsü (her gece)

1. **Alacakaranlık — Keşif:** Adayı gez, Elias'ın günlük sayfalarını, eşyaları ve ipuçlarını bul. Her gece yeni bir alan açılır veya mevcut alanlar değişir.
2. **Feneri hazırla — Rutin görevler:**
   - Yakıt tankını doldur
   - Merceği temizle
   - Dönme mekanizmasının saat zembereğini kur
3. **Gece — Nöbet:**
   - Margaret ile telsiz kontrolü (diyalog seçenekleri sınırlı)
   - Hava kayıt defterini doldur
   - Feneri ayakta tut; aksaklıklara müdahale et (sönme, mekanizma arızası, tuhaf olaylar)
4. **Sabah — Kapanış:** Bekçi günlüğüne yaz (otomatik özet), bir sonraki geceye geçiş.

### Mekanikler

- **Etkileşim:** Bakılan nesneyi vurgulama + E tuşu ile etkileşim (al, aç, oku, çevir, dinle).
- **Belge okuyucu:** Günlük sayfaları, dosyalar, notlar tam ekran okunur; envantere kaydedilir.
- **Telsiz:** Frekans çevirme (düğme), konuşma seçme. Belirli frekanslar hikaye ilerledikçe açılır.
- **Kaset çalar:** Hidrofon kasetlerini dinleme (Gece 4).
- **Fener sistemi:** Yakıt seviyesi, mercek temizliği, zemberek gerginliği. Herhangi biri düşerse ışık zayıflar / söner.
- **Fener (el lambası):** Oyuncunun taşıdığı gaz lambası veya el feneri; sınırlı pil/yakıt yok, sadece atmosfer için.
- **Kayıt:** Her gecenin başında otomatik kayıt.
- **Dövüş yok, ölüm yok** (gerilim tamamen psikolojik). İsteğe bağlı: Gece 5'te başarısızlık durumu.

## 7. Gece gece hikaye akışı

### Gece 1 — Varış
- Tekne oyuncuyu iskeleye bırakır, uzaklaşır.
- Kulübe, fener kulesi ve telsiz barakası erişilebilir.
- Margaret ilk telsiz görüşmesinde rutini anlatır (eğitim bölümü).
- **Bulgu:** İlk günlük sayfası — *"Never miss the 3 AM check."*
- **Tuhaflık:** Telsizde bir an sayı okuyan bir ses, sonra parazit.
- **Gece sonu:** Sisin içinde, gemi olmaması gereken bir noktada bir ışık.

### Gece 2 — İzler
- **Yeni alan:** Kayıkhane.
- **Bulgu:** Elias'ın sayıları çözmeye çalıştığı not defteri.
- **Olay:** Gece fener söner; mekanizmanın kurcalandığı anlaşılır. Oyuncu tamir eder.
- **Margaret:** Elias sorulunca konuyu değiştirir.

### Gece 3 — Sığınak (Soğuk Savaş katmanı)
- **Bulgu:** Telsiz barakasının altında kilitli bir kapak; anahtar adanın başka bir yerinde.
- **Yeni alan:** Yeraltı sığınağı — hidrofon cihazları, PROJECT VESPER dosyaları, Sovyet denizaltı raporları.
- **Çözüm:** Sayılar = fener zamanlama kodları.
- **Ton:** Her şeyin mantıklı (askeri/politik) bir açıklaması var gibi görünür.
- **Margaret:** Sesi değişir; oyuncunun aşağı indiğini biliyor.

### Gece 4 — Zemin kayıyor (doğaüstü katman)
- **Bulgu:** Hidrofon kasetleri — sesler makine değil.
- **Olay:** Telsizde Elias'ın sesi: *"Turn it off. They're lying to you. I'm alive — I'm out on the rocks."*
- **Çevre:** Denizden kulübeye uzanan ıslak ayak izleri.
- **Bulgu:** Eski bekçilerin listesi; hepsi "missing" olarak işaretli.
- **Hava:** Fırtına başlar.

### Gece 5 — Karar
- Fırtına zirvede, fener sönmek üzere; adada yollar kısmen kapanmış.
- Margaret yalvarır: *"Keep it lit."* Elias yalvarır: *"Turn it off."*
- Belirsizlik: Elias'ın sesi gerçekten Elias mı, yoksa "o" mu?
- Oyuncu fener odasına tırmanır ve kararını verir.

## 8. Sonlar

### Son A — Işığı yanık tut
Sabah olur. Margaret: *"Welcome to the service, Keeper."*
Son sahne: Oyuncu bir sonraki bekçi için günlüğe yazar — *"Never miss the 3 AM check."*
Hayatta kaldın ama artık adanın esirisin.

### Son B — Işığı söndür
Sisin içindeki ışıklar yüzlerce olur; bir şey sudan yükselir. Ekran kararır.
Telsizden son ses: *"Skerrow, respond... Skerrow?"*
Belirsiz, korkutucu bir son.

## 9. Mekânlar

| Mekân | Açıldığı gece | İçerik |
|---|---|---|
| İskele | 1 | Başlangıç noktası, tekne |
| Bekçi kulübesi | 1 | Yatak, masa, günlük, soba; oyuncunun "evi" |
| Fener kulesi | 1 | Merdivenler, yakıt odası, mekanizma katı, fener/mercek odası |
| Telsiz barakası | 1 | Telsiz, kayıt defteri; altında kilitli kapak |
| Kayalıklar / patika | 1 | Adayı dolaşan yol, manzara noktaları |
| Kayıkhane | 2 | Elias'ın not defteri, alet |
| Yeraltı sığınağı | 3 | Hidrofon istasyonu, dosyalar, kasetler |
| Deniz mağarası (isteğe bağlı) | 4–5 | Ayak izlerinin kaynağı, "o"ya en yakın nokta |

## 10. Görsel ve ses yönü

- **Görsel:** Gerçekçi ama soğuk; mavi-gri palet. Tek sıcak renk fenerin ışığı ve gaz lambası. Volumetrik sis, dönen fener ışığı huzmesi sisin içinde görünmeli.
- **Işık:** Lumen. Gece sahneleri çok karanlık ama okunabilir.
- **Ses:** Rüzgâr, dalga, gıcırdayan ahşap, telsiz paraziti ön planda. Müzik minimal; sadece kritik anlarda drone/ambient.
- **Seslendirme:** İngilizce. Seçenekler: yapay zekâ ses üretimi veya gönüllü seslendirme. Tüm satırlar altyazılı (EN + TR).

## 11. Asset planı (Fab)

Oyuncu (geliştirici) Fab'dan indirip projeye ekler; Claude kodda kullanır.

- Kıyı / kayalık ortam paketi (kayalar, çimen, deniz kıyısı)
- Deniz fener modeli (iç mekânlı tercihen)
- Eski ahşap kulübe / iskele / kayıkhane
- 1970'ler dönem eşyaları (telsiz, kaset çalar, lambalar, mobilya)
- Askeri sığınak / bunker iç mekân paketi
- Su: UE Water plugin
- Ses efekt paketleri: rüzgâr, deniz, ahşap, telsiz paraziti

## 12. Teknik kurallar (Claude Code için)

- **Blueprint kullanılmaz;** tüm oyun mantığı C++ ile yazılır.
- Proje **komut satırından derlenir;** derleme hataları Claude tarafından düzeltilir.
- Sahne/harita kurulumu **Python editör betikleri** veya C++ ile yapılır.
- Asset'ler `Content/` altında yolu ile referans verilir; asset'ler değiştirilmez.
- Diyaloglar ve metinler kodun içine gömülmez; **veri dosyalarında** (DataTable / JSON) tutulur, böylece çeviri kolay olur.
- Her özellik bitince oyun çalıştırılır ve log kontrol edilir.
- Geliştirici editör bilmez; editörde elle yapılması zorunlu bir iş varsa adım adım Türkçe anlatılır.

## 13. Geliştirme yol haritası

| Aşama | Hedef |
|---|---|
| **0. Kurulum** | First Person C++ projesi, git, CLAUDE.md, derleme/çalıştırma betikleri |
| **1. Temel sistemler** | Etkileşim sistemi, belge okuyucu, envanter, telsiz altyapısı, kayıt |
| **2. Ada blockout** | Basit şekillerle ada ve mekânların yerleşimi; yürüyüp gezilebilir |
| **3. Fener sistemi** | Yakıt, mercek, zemberek; dönen ışık huzmesi |
| **4. Gece 1 (dikey dilim)** | Gece 1 baştan sona oynanabilir, gerçek asset'ler, ses, ışık |
| **5. Gece 2–5** | Kalan geceler, olaylar, sığınak, fırtına |
| **6. Sonlar** | Karar anı, iki son, jenerik |
| **7. Cila** | Ses, ışık, menüler, ayarlar, altyazılar, performans |
| **8. Yayın hazırlığı** | Steam sayfası, fragman, demo (Gece 1) |

## 14. Açık sorular

- Oyunun nihai adı (*The Last Watch* / *Hollow Light* / *Skerrow*)
- Gece 5'te başarısızlık durumu olacak mı?
- Seslendirme yöntemi
- Deniz mağarası dahil edilecek mi?
