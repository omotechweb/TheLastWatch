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
- Editör Python betiği çalıştır (editör kapalıyken): `powershell -File Scripts/run_python.ps1 Scripts/Editor/<betik>.py`
- Ekran görüntüsü al (HUD dahil): `powershell -File Scripts/screenshot.ps1 -Commands "DebugOpenNote elias_01" -Out Saved\Screenshots\x.png` — sonucu Read ile incele. Görsel değişiklikleri böyle doğrula.
- Loglar: `Saved/Logs/TheLastWatch.log`

## Geliştirici konsol komutları (oyunda ` tuşu)

`DebugOpenNote <id>`, `DebugGiveItem <id>`, `DebugInventory`, `DebugRadio <kHz>`, `DebugSetNight <1-5>`, `tlw.Language en|tr`, `tlw.ReloadData` (JSON'ları oyunu kapatmadan yeniden okur).

## Oyun sistemleri (`Source/TheLastWatch/Game/`)

- `GameDataSubsystem` — `Content/Data/*.json` okur (Notes, Items, Radio, UI). Metinler `{ "en": ..., "tr": ... }`.
- `Interactables` — `AInteractableActor` tabanı + `ANotePickup`, `AItemPickup`, `AKeeperDoor`, `ARadioSet`. Geçici görünüm: tintlenmiş motor küpü.
- `LastWatchPlayerController` — odak (line trace), E etkileşimi, modlar (Explore/Reading/Radio/Inventory), girdiler kodda oluşturulur.
- `LastWatchHUD` — tüm arayüz Slate ile C++'ta.
- `LastWatchGameMode` — şablonun `BP_HorrorCharacter`'ını kullanır, `CurrentNight` tutar.
- Test haritası: `/Game/Maps/Dev/L_SystemsTest`, `Scripts/Editor/build_systems_test_map.py` ile üretilir (elle düzenleme; betiği değiştirip yeniden çalıştır).

## Proje yapısı

- `Source/TheLastWatch/` — C++ kaynak. Şablon: `TheLastWatchCharacter` (FPS karakter), `Variant_Horror/` (el feneri + stamina'lı sprint, oyunumuzun başlangıç noktası).
- `Content/` — asset'ler (.uasset, ikili; metin olarak düzenlenemez).
- `Config/` — ini ayarları (varsayılan harita, game mode).
- `Scripts/` — derleme/çalıştırma betikleri ve editör Python betikleri.
