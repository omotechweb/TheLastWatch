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
- Komut satırı editör / Python: `UnrealEditor-Cmd.exe TheLastWatch.uproject -run=pythonscript -script=<yol>` (Python Editor Script Plugin açık olmalı)
- Loglar: `Saved/Logs/TheLastWatch.log`

## Proje yapısı

- `Source/TheLastWatch/` — C++ kaynak. Şablon: `TheLastWatchCharacter` (FPS karakter), `Variant_Horror/` (el feneri + stamina'lı sprint, oyunumuzun başlangıç noktası).
- `Content/` — asset'ler (.uasset, ikili; metin olarak düzenlenemez).
- `Config/` — ini ayarları (varsayılan harita, game mode).
- `Scripts/` — derleme/çalıştırma betikleri ve editör Python betikleri.
