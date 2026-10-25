# The Last Watch

Birinci şahıs, hikaye odaklı gerilim/gizem oyunu. Unreal Engine 5.7, C++.
Tasarımın tamamı `GDD.md` içinde — her özellikten önce ilgili bölümü oku ve ona uy.

## Geliştirici hakkında

- Geliştirici Unreal editörünü, Blueprint'i ve C++'ı **bilmiyor**. Prompt verir, oyunu oynar, geri bildirim verir.
- Geliştiriciyle **Türkçe** konuş. Oyun içi metinler ve seslendirme **İngilizce**.
- Editörde elle yapılması zorunlu bir iş varsa (ör. Fab'dan asset eklemek) bunu **adım adım, hangi menüye tıklanacağını yazarak** anlat.
- Her işin sonunda oyunu nasıl açıp neyi test edeceğini kısaca söyle.

## Kurallar

- **Blueprint yazma.** Bütün oyun mantığı C++. Mevcut şablon Blueprint'leri (`Content/FirstPerson/Blueprints`, `Content/Variant_Horror/Blueprints`) sadece C++ sınıflarının alt sınıfı olarak, input/mesh referansları için kullanılabilir.
- Sahne/harita kurulumu ve asset oluşturma **Python editör betikleriyle** (`Scripts/Editor/*.py`) veya C++ ile yapılır; elle editör işi istenmez.
- Diyaloglar, notlar, telsiz konuşmaları kodda yazılmaz; **veri dosyalarında** (`Content/Data/` altında JSON/CSV → DataTable) tutulur. Çeviri (TR altyazı) buna bağlı.
- Fab'dan gelen asset'leri değiştirme; yollarıyla referans ver.
- Küçük adımlarla ilerle: bir özellik → derle → çalıştır → log kontrol → commit.
- `Variant_Shooter` oyunda kullanılmayacak; silinebilir ama önce geliştiriciye sor.

## Komutlar

- Derleme (editör **kapalıyken**): `powershell -File Scripts/build.ps1`
- Oyunu editörsüz çalıştır: `powershell -File Scripts/play.ps1` (isteğe bağlı `-Map /Game/...`)
- Editör: `C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe`
- Editör Python betiği çalıştır (editör kapalıyken): `powershell -File Scripts/run_python.ps1 Scripts/Editor/<betik>.py` — dosya/doku içe aktaran betikler `-Editor` ister (komut satırı modunda içe aktarma çöker).
- Ekran görüntüsü al (HUD dahil): `powershell -File Scripts/screenshot.ps1 -Commands "DebugOpenNote elias_01" -Out Saved\Screenshots\x.png` — sonucu Read ile incele. Görsel değişiklikleri böyle doğrula.
- Gerçek tuş testi: `powershell -File Scripts/test_input.ps1 -Commands "DebugTeleport ..." -Keys "SPACE;WAIT:2;WDOWN;WAIT:10;WUP"` — oyunu açar, oyuncu hazır olunca pencereye tuş mesajı gönderir (SPACE, E, TAB, C, WDOWN/WUP, CTRLDOWN/CTRLUP, WAIT:s). `tlw.LogPawn 1` her kareyi (`Pawn x= y= z= ... crouched=`), basılan tuşları ve **ilerleyemeyince önündeki engeli** (`Blocked by ... (mesh ...)`) log'a yazar. Hareketle ilgili her düzeltmeyi bununla doğrula; ekran görüntüsü yetmez.
- Loglar: `Saved/Logs/TheLastWatch.log`

## Hareket

Şablon karakter zıplama ve eğilmeyi kapalı getiriyor; `LastWatchPlayerController::OnPossess` açar. Space = zıpla, Ctrl basılı = eğil, C = eğilme aç/kapa. Yürünebilir eğim 50° (`WalkableSlope`). Görünmez duvar şikâyetinde önce eğime bak (`island_shape.height` türevi), sonra `Blocked by` loguna. Çim/çalı/çakıl çarpışmasız; kayalık ve iskele "complex as simple". Fener merdiveni için oyunda `DebugStairHeadroom 200`.

## Geliştirici konsol komutları (oyunda ` tuşu)

`DebugOpenNote <id>`, `DebugGiveItem <id>`, `DebugInventory`, `DebugRadio <kHz>`, `DebugSetNight <1-5>`, `DebugTeleport X Y Z Yaw Pitch` (uçma moduna geçer; kamera açısı için; bir şeyin içine denk gelirse sessizce başarısız olur ve oyuncu başlangıçta kalır), `DebugLamp 0|1`, `DebugDumpLights <cm>`, `DebugShotAfter <s>`, `tlw.Language en|tr`, `tlw.ReloadData` (JSON'ları oyunu kapatmadan yeniden okur).

Ekran görüntüsünde `viewmode unlit` / `viewmode lit_detaillighting` komutları renk mi ışık mı sorunu olduğunu ayırmak için işe yarar.

## Oyun sistemleri (`Source/TheLastWatch/Game/`)

- `GameDataSubsystem` — `Content/Data/*.json` okur (Notes, Items, Radio, UI). Metinler `{ "en": ..., "tr": ... }`.
- `Interactables` — `AInteractableActor` tabanı + `ANotePickup`, `AItemPickup`, `AKeeperDoor`, `ARadioSet`. Geçici görünüm: tintlenmiş motor küpü.
- `LastWatchPlayerController` — odak (line trace), E etkileşimi, modlar (Explore/Reading/Radio/Inventory), girdiler kodda oluşturulur.
- `LastWatchHUD` — tüm arayüz Slate ile C++'ta.
- `LastWatchGameMode` — şablonun `BP_HorrorCharacter`'ını kullanır, `CurrentNight` tutar.
- `LighthouseBeam` — dönen iki spot ışık + lamba parıltısı, `SetLit()` ile söner.
- `FootstepComponent` — ayak altındaki mesh'in asset yoluna göre ayak sesi (kulübe=ahşap, santral=beton, ada=toprak).
- Test haritası: `/Game/Maps/Dev/L_SystemsTest`, `Scripts/Editor/build_systems_test_map.py` ile üretilir.

## Ada (Skerrow Rock) — `/Game/Maps/L_Skerrow`, varsayılan harita

Harita tamamen betiklerle üretilir; elle düzenleme yapma, betiği değiştirip yeniden çalıştır:

1. `Scripts/Editor/import_assets.py -Editor` — `Imports/unpacked` içindeki fener ve kayalık FBX'lerini içe aktarır, `M_ScanPBR` ana malzemesini kurar.
2. `Scripts/Editor/build_island_terrain.py -Editor` — `island_shape.py`'deki yükseklik fonksiyonundan arazi mesh'i, toprak yolu maskesi (`T_DirtMask`), `M_IslandGround`, `M_Ocean`.
3. `Scripts/Editor/build_skerrow_map.py` — haritayı kurar: kayalıklar, deniz, fener+ışık, kulübe ve baraka (kulübe paketinin showcase haritasından kopyalanır, kapılar `KeeperDoor` ile değiştirilir), mobilya, etkileşimli objeler, iskele, gece gökyüzü, sis, sesler.

Fener: `Scripts/Editor/build_lighthouse.py` (2'den sonra, 3'ten önce) — dış modeli 2.2x ölçekler, kapı/kaide/balkon zeminini keser; beton kabuk, 94 basamaklı spiral merdiven, döşemeler ve lamba odası cam çarpışması üretir (`/Game/Lighthouse`). Parçalar aynı çerçevede: orijin = kule tabanı, +Y = kapı.

Işık kuralı: pozlama ay ışığına göre sabit (EV100 -1.9). ~1 lüksü geçen her ışık beyaza patlar; iç mekân lambaları 10–70 lm. Fener huzmeleri hacimsel sisi yaktığı için lamba odasının dışından başlar. C++ aktör bileşen değerlerini Python'dan değiştirme (yeniden kurulumda sıfırlanır), aktöre UPROPERTY ekle. Işık şüphesinde `DebugDumpLights <yarıçap>`.

`island_shape.py` adanın tek kaynağı: boyut, bina yerleri (`PADS`), toprak yol (`PATH`), iniş koyu. Bir şeyin yerini değiştirmek için oradan değiştir, 2 ve 3'ü yeniden çalıştır.

Malzeme betiklerinde: `ComponentMask` kanallarını her zaman `set_mask()` ile açıkça ver (varsayılanlar kapalı değil); GeometryScript köşe renkleri statik mesh'e geçmiyor, maske için doku kullan.

Sığınak: `Scripts/Editor/build_bunker.py` (haritadan önce) — VESPER istasyonu, adanın 300 m üstünde (yükseklik sisinin dışında), kalıntıdaki kapaktan ışınlanarak girilir. Harita kurulum sırası: terrain → lighthouse → bunker → skerrow_map.

## Hikaye (5 gece)

- Senaryo tamamen `Content/Data/Nights.json`: her gece `on_start` eylemleri + sıralı `objectives` (`complete_on` olayı) + `events` (o gece her zaman tepki; son gecedeki seçim). Belgeler `Notes.json`, telsiz `Radio.json` (`nights` alanı), eşyalar `Items.json`, istemler `UI.json`.
- `UStorySubsystem` (`Game/StorySubsystem.*`) geceyi başlatır, olayları dinler: `read:<not>`, `take:<eşya>`, `task:<id>`, `enter:<alan>`, `radio:<yayın>`, `use:<kapı>`, `delay:<sn>`. Eylemler: subtitle, title, toast, fade_out/in, show/hide (etiket), lamp_on/off, give/remove, sound, teleport (etiket), storm, fog, ambience (`etiket=ses`), notify, next_night, ending, quit.
- Haritadaki karşılıkları `build_skerrow_map.py` sonundaki "story" bölümünde: `AStoryTask` (görünmez kutu, basılı tutma, gerekli eşya; yalnızca hikaye onu beklerken kullanılabilir), `AStoryVolume`, `ATeleportDoor`, `ATargetPoint` etiketleri, `AStormController`. Aktör etiketi `night:3-5` → sadece o gecelerde görünür.
- Yeni gece adımı eklemek: JSON'a objective yaz → gerekiyorsa haritaya task/volume ekle (id'ler aynı) → `DebugAutoplay 5` ile tüm akışı log'dan doğrula (`Story:` satırları) → kritik etkileşimi `test_input.ps1` ile gerçek tuşla dene.
- Hata ayıklama: `DebugSetNight n`, `DebugSkipObjective`, `DebugStoryEvent <olay>`, `DebugAutoplay <sn>`. `DebugTeleport` başarısız olursa log'da `Teleport blocked by ... (mesh ...)` yazar.

## Üçüncü parti asset'ler

Fab paketleri git'te değil (bkz. `CREDITS.md`). Yeni bir paket eklendiğinde `Scripts/Editor/asset_inventory.py` çalıştır, `Saved/asset_inventory.txt` içinde mesh adları ve boyutları çıkar. Fener modeli CC-BY: jenerikte DylanHayes'e atıf zorunlu.

## Proje yapısı

- `Source/TheLastWatch/` — C++ kaynak. Şablon: `TheLastWatchCharacter` (FPS karakter), `Variant_Horror/` (el feneri + stamina'lı sprint, oyunumuzun başlangıç noktası).
- `Content/` — asset'ler (.uasset, ikili; metin olarak düzenlenemez).
- `Config/` — ini ayarları (varsayılan harita, game mode).
- `Scripts/` — derleme/çalıştırma betikleri ve editör Python betikleri.
