# Incremental Encoder — Pulse Sayma, Yön ve RPM Hesaplama

[![CI](https://github.com/Southpaw52/stm32-encoder-rpm/actions/workflows/ci.yml/badge.svg)](https://github.com/Southpaw52/stm32-encoder-rpm/actions/workflows/ci.yml)

STM32F407VG (STM32F4DISCOVERY) üzerinde A/B kanallı artımlı (incremental) encoder sinyallerini
donanımsal **Encoder Interface Mode** ile sayan, her **1 saniyede** bir donanım timer'ı ile hız ve
yönü güncelleyen ve sonuçları **UART** üzerinden gönderen STM32 HAL tabanlı firmware.

| | |
|---|---|
| Hedef | STM32F4DISCOVERY (MB997), STM32F407VGT6, 8 MHz HSE |
| Yazılım | STM32CubeMX 6.17.0, STM32Cube FW_F4 V1.28.3 (HAL), CMake + Ninja |
| Derleyici | GNU Tools for STM32 13.3.rel1 (`arm-none-eabi-gcc` 13.3.1) |
| Host testleri | Herhangi bir C11 derleyicisi (doğrulama: GCC 16.2.0, MinGW-w64) |
| Sürüm | `1.0.0` |

---

## 1. Gereksinim → Çözüm Eşlemesi

| # | Gereksinim | Çözüm | Kaynak |
|---|---|---|---|
| G1 | A/B sinyallerinin okunması ve pulse sayılması | TIM2 Encoder Mode TI12 (x4), 32-bit donanım sayacı; CPU her kenarda müdahale etmez | `encoder_rpm.ioc`, `Core/Src/main.c` (`MX_TIM2_Init`) |
| G2 | Dönüş yönünün belirlenmesi | 1 s penceredeki işaretli net yer değiştirme: `+` → `CW`, `−` → `CCW`, `0` → `STOP` | `App/Src/encoder_math.c` |
| G3 | Tur başına 100 pulse ile RPM | `ENCODER_PPR = 100`, x4 → `ENCODER_COUNTS_PER_REV = 400`; tamsayı aritmetiği ile centi-RPM | `App/Inc/encoder_config.h`, `encoder_rpm_x100()` |
| G4 | Timer ile her 1 s güncelleme | TIM6: 84 MHz / 8400 / 10000 = 1 Hz update kesmesi; pencere süresi açılışta çalışma anında doğrulanır | `MX_TIM6_Init`, `app_init()` |
| G5 | Pulse, yön ve RPM'in UART ile gönderilmesi | USART2 115200 8N1, DMA ile bloklamayan gönderim, tek satırlık metin protokolü | `App/Src/telemetry.c`, `App/Src/app.c` |
| G6 | Motor durunca RPM'in sıfırlanması | Pencere farkı `0` (veya ayarlanabilir eşik altı) ise `dir=STOP rpm=0.00`; motor durduktan sonraki ilk tam pencerede (kenar titreşimi için §5) | `app_run_once()`, testler |
| Kapsam | HAL/LL kaynakları ve çevre birimi konfigürasyonu | CubeMX `.ioc`, üretilmiş HAL init kodu, uygulama modülleri, bu doküman | tüm depo |

---

## 2. Donanım ve Pin Konfigürasyonu

Pin seçimi, kartın kullanıcı kılavuzundaki *"MCU pin description versus board function"* tablosuna
göre yapılmıştır (UM1472 Rev 9). Encoder pinleri, kart üzerinde hiçbir bileşene bağlı olmayan
**Free I/O** pinlerinden seçilmiştir.

| Sinyal | MCU pini | Çevre birimi / AF | GPIO | Kart konnektörü | Not |
|---|---|---|---|---|---|
| Encoder A | **PA15** | TIM2_CH1, AF1 | Alternate function, dahili pull-up | P2 pin 40 | JTDI pini; SWD kullanıldığı için serbest |
| Encoder B | **PA1** | TIM2_CH2, AF1 | Alternate function, dahili pull-up | P1 pin 11 | Free I/O |
| UART TX | **PA2** | USART2_TX, AF7 | AF push-pull | P1 pin 14 | Bilgisayara giden veri |
| UART RX | PA3 | USART2_RX, AF7 | AF | P1 pin 13 | Yapılandırıldı, kullanılmıyor |
| Hata LED'i | PD14 | GPIO çıkış | push-pull | LD5 (kırmızı) | `Error_Handler()` içinde yanar |
| SWD | PA13 / PA14 | SWDIO / SWCLK | — | ST-LINK | Hata ayıklama |
| HSE | PH0 / PH1 | OSC_IN / OSC_OUT | — | X2, 8 MHz | Kristal modu |

**Neden bu pinler?**

- **PA0 + PA1** (TIM2'nin varsayılan encoder pinleri) seçilmedi. Kartta **PA0'a kullanıcı butonu B1**
  bağlıdır. Butonun RC devresi encoder sinyalini bozar.
- **PB3** (TIM2_CH2'nin diğer seçeneği) seçilmedi. Kartta PB3, SB12 köprüsü üzerinden
  **ST-LINK SWO** hattına bağlıdır.
- **PA5** (TIM2_CH1 seçeneği) seçilmedi. Kartta bu pin ivmeölçerin SPI saat hattıdır.
- **PA15 (CH1) + PA1 (CH2)**: Kart üzerinde hiçbir bileşene bağlı değildir ve aynı AF1 grubundadır.

**UART bağlantısı:** Kılavuza göre kart üzerindeki ST-LINK/V2-A'nın Virtual COM Port hatları
USART2'ye **bağlı değildir**. Bilgisayara bağlanmanın iki yolu vardır:
1. **USB-TTL (3,3 V) dönüştürücü:** dönüştürücünün RX ucu → PA2 (P1-14), GND → GND.
2. Kılavuzdaki Şekil 7'de gösterildiği gibi ST-LINK VCP uçlarına iki kablo lehimlemek.

**Encoder bağlantısı:** A → PA15, B → PA1, GND → GND. Dahili pull-up'lar (~40 kΩ) açık kollektörlü
(open-collector) encoder çıkışları için yeterlidir. Uzun kablolarda harici 2,2–4,7 kΩ pull-up önerilir.
Encoder 3,3 V lojik seviyesiyle sürülmelidir. 5 V çıkışlı encoder'lar seviye dönüştürücü ile bağlanmalıdır.

---

## 3. Saat Ağacı ve Timer Hesapları

### 3.1 Saat kaynağı: neden HSE kristali?

RPM, *"1 saniyedeki sayım"* olarak ölçüldüğü için **pencere süresindeki hata doğrudan RPM hatasıdır.**

- Dahili HSI osilatörünün fabrika kalibrasyonu yaklaşık **±1 %** mertebesindedir. Bu da RPM'de ±1 %
  hata demektir; örneğin 3000 RPM'de ±30 RPM.
- 8 MHz HSE kristali ise tipik olarak onlarca ppm (milyonda bir) doğruluktadır. RPM ölçüm hatasına
  katkısı ihmal edilebilir düzeydedir.

### 3.2 Sistem frekansı: neden 84 MHz (168 MHz değil)?

Pulse sayımını CPU değil, TIM2 donanımı yapar. CPU saniyede bir kez birkaç tamsayı işlemi ve bir DMA
başlatması yapar. 168 MHz'in getireceği ek işlem gücü bu uygulamada kullanılmaz. **84 MHz**
seçimiyle:

- Timer ve UART saatleri 168 MHz konfigürasyonuyla aynı kalır (APB1 = 42 MHz, APB1 timer = 84 MHz).
- Dinamik güç tüketimi yaklaşık yarıya iner.
- Flash bekleme süresi 5'ten 2'ye düşer (2,7–3,6 V aralığında 60–90 MHz için 2 WS).

| Parametre | Değer | Hesap |
|---|---|---|
| HSE | 8 MHz | X2 kristali |
| PLL (M / N / P / Q) | 8 / 336 / 4 / 7 | VCO girişi 1 MHz, VCO 336 MHz |
| SYSCLK = HCLK | **84 MHz** | 336 / 4 |
| APB1 (PCLK1) | 42 MHz | HCLK / 2 (APB1 için üst sınır 42 MHz) |
| APB1 timer saati (TIM2, TIM6) | **84 MHz** | APB1 bölücüsü ≠ 1 olduğu için ×2 |
| APB2 (PCLK2) | 84 MHz | HCLK / 1 |
| Flash gecikmesi | 2 WS | `FLASH_LATENCY_2` |

### 3.3 TIM6: 1 saniyelik ölçüm penceresi

```
f_update = f_TIM / ((PSC + 1) × (ARR + 1)) = 84 000 000 / (8400 × 10 000) = 1,000 Hz  → T = 1,000 s (tam)
```

`PSC = 8399` sayacı 10 kHz'e (0,1 ms çözünürlük) indirir, `ARR = 9999` 10 000 adımda taşar.
Sonuç tam olarak 1 s'dir, yuvarlama hatası yoktur. `app_init()` bu eşitliği açılışta **çalışma anında**
doğrular. Saat ağacı CubeMX'te değiştirilip pencere güncellenmezse `Error_Handler()` çağrılır.

TIM6, yalnızca zaman tabanı üretmek için tasarlanmış bir *basic timer*'dır. Giriş ve çıkış kanalı
yoktur, bu yüzden pin veya diğer timer kaynaklarıyla çakışmaz.

### 3.4 TIM2: Encoder Interface Mode

| Parametre | Değer | Gerekçe |
|---|---|---|
| Mod | `TIM_ENCODERMODE_TI12` | A ve B'nin her iki kenarı sayılır → **x4** çözünürlük |
| Sayaç | 32 bit, `ARR = 0xFFFFFFFF` | Taşma neredeyse hiç görülmez; görülürse de yazılım doğru hesaplar (§6) |
| Prescaler | 0 | Her kenar 1 sayım |
| Giriş filtresi | `ICxF = 15` (f_DTS/32, N = 8) | 84 MHz / 32 = 2,625 MHz örnekleme × 8 örnek ≈ **3,05 µs**'den kısa darbeler reddedilir |
| Polarite | Rising (yön tanımı §5'te) | |

**Filtre seçiminin sınırı:** Her kuadratür durumu en az ~3,05 µs sürmelidir. Bu da yaklaşık 300 000
sayım/s demektir; 400 sayım/tur ile yaklaşık 45 000 RPM eder. Bu değer tipik motor hızlarının çok
üzerindedir. Bu nedenle en yüksek gürültü bağışıklığını veren filtre değeri seçilmiştir.

### 3.5 USART2

| Parametre | Değer |
|---|---|
| Format | 115200 baud, 8 veri biti, parite yok, 1 stop biti, akış kontrolü yok |
| Gönderim | DMA1 Stream6 Channel4, normal mod, byte genişliği |
| Baud hatası | PCLK1 = 42 MHz, oversampling 16 → `USARTDIV = 22 + 12/16 = 22,75` → 115 385 baud, **+0,16 %** (izin verilen sınır ±2 % mertebesindedir) |

### 3.6 NVIC önceliklendirmesi

| Kesme | Öncelik | Gerekçe |
|---|---|---|
| TIM6_DAC_IRQn | 5 | Ölçüm anı, uygulamanın en kritik kesmesi; UART kesmeleri bunu geciktiremez |
| DMA1_Stream6_IRQn | 6 | UART gönderim tamamlanması; zaman kritik değil |
| USART2_IRQn | 6 | DMA sonrası TC (gönderim bitti) olayı |
| SysTick | 15 | HAL zaman tabanı, en düşük öncelik |

Öncelik grubu `NVIC_PRIORITYGROUP_4`'tür (4 bit preemption, 0 bit sub-priority).

---

## 4. Yazılım Mimarisi

```
                ┌──────────────── donanım ────────────────┐
 Encoder A/B ──►│ TIM2 (x4 sayaç, 32-bit)                 │
                │ TIM6 (1 Hz update) ──► TIM6_DAC_IRQn ─┐ │
                └───────────────────────────────────────┼─┘
                                                        ▼
                     ISR bağlamı:  HAL_TIM_PeriodElapsedCallback()
                                   └─ speed_monitor_on_window_elapsed(TIM2->CNT)
                                      (fark + birikim + sıra no; bölme/biçimleme YOK)
                                                        │  paylaşılan örnek (volatile)
                                                        ▼
                     Ana döngü:    app_run_once()
                                   ├─ speed_monitor_fetch()      ← kritik bölge (PRIMASK)
                                   ├─ encoder_direction_from_delta()
                                   ├─ encoder_rpm_x100()
                                   ├─ telemetry_format_sample()
                                   ├─ HAL_UART_Transmit_DMA()    ← bloklamaz
                                   └─ __WFI()                    ← bir sonraki kesmeye kadar uyku
```

| Modül | Sorumluluk | HAL bağımlılığı | Host'ta test edilir |
|---|---|---|---|
| `encoder_config.h` | Tüm sabitler (PPR, x4, pencere, eşik, tampon boyutu) ve derleme zamanı kontrolleri | yok | ✓ |
| `encoder_math.c/.h` | Taşmaya dayanıklı fark, RPM, yön, zaman tabanı doğrulaması | yok | ✓ |
| `speed_monitor.c/.h` | ISR (üretici) ↔ ana döngü (tüketici) örnek aktarımı | yok | ✓ |
| `telemetry.c/.h` | UART satırı biçimlendirme (printf kullanılmaz, int64 destekli) | yok | ✓ |
| `critical_section.h` | PRIMASK kaydet/kapat/geri yükle; host'ta boş | CMSIS (hedefte) | ✓ |
| `app.c/.h` | HAL bağlantısı: timer başlatma, callback'ler, DMA gönderimi | **yalnız bu modül** | — |
| `Core/` | CubeMX tarafından üretilen init kodu; yalnız `USER CODE` blokları düzenlendi | HAL | — |

**Tasarım ilkesi:** Hesap ve biçimleme mantığı donanımdan tamamen ayrıdır. HAL'e dokunan tek dosya
`app.c`'dir. Bu sayede ölçüm mantığının tamamı gerçek donanım olmadan bilgisayarda test edilebilir.

### 4.1 Kesme güvenliği

- **ISR minimum iş yapar.** Sayacı okur, farkı alır, toplama ekler, sıra numarasını artırır ve
  "hazır" bayrağını kurar. 64-bit bölme, biçimleme ve UART işlemleri ana döngüdedir.
- **Kesme içinde UART yoktur.** Gönderim ana döngüden DMA ile başlatılır. CPU bu sırada beklemez.
- **Paylaşılan veri:** Örnek; 32-bit sıra numarası, 64-bit toplam ve 32-bit farktan oluşur.
  Cortex-M4'te 64-bit erişim atomik olmadığı için tüketici örneği kısa bir **PRIMASK kritik
  bölgesinde** kopyalar. Bu bölge birkaç komut sürer. Önceki PRIMASK değeri geri yüklendiği için iç
  içe kullanıma dayanıklıdır (interrupt'ları koşulsuz açmaz).
- **Tek üretici, tek tüketici:** Tek çekirdekte TIM6 ISR ana döngüyü kesebilir, ama tersi olamaz.
  Bu yüzden yalnız tüketici tarafının korunması yeterlidir.
- **DMA tampon sahipliği:** `uart_tx_busy` bayrağı DMA tamamlanana kadar tamponun yeniden
  yazılmasını engeller. Gönderim sürerken yeni örnek **tüketilmez**. UART'a hiç yazılamayan
  pencereler (gönderim meşgulken atlananlar veya DMA başlatılamadığı için gönderilemeyenler) bir
  sonraki satırda `missed=N` olarak bildirilir. Toplam pulse bilgisi ISR'de birikmeye devam ettiği için **hiçbir
  sayım kaybolmaz**.
- **Ölçüm anının doğruluğu:** `TIM2->CNT` tek bir hizalı 32-bit okumadır. Kesme gecikmesindeki
  birkaç mikrosaniyelik oynama, 1 s'lik pencerede ppm (milyonda bir) mertebesinde etki yapar.
- **İlk pencere:** Başlangıç sayaç değeri, TIM6 başlatılmadan hemen önce okunur. Böylece ilk
  ölçüm de tam olarak 1 s'lik hareketi kapsar.

### 4.2 Hata yönetimi

- Tüm HAL çağrılarının dönüş değeri kontrol edilir. Başlatma hataları `Error_Handler()`'a gider:
  kesmeler kapatılır ve kırmızı LED (LD5) yanar.
- Çalışma sırasındaki UART başlatma ve callback hataları, hata ayıklayıcıdan izlenebilen sayaçlara
  (`statistics`) yazılır. Sistem çalışmaya devam eder.
- Biçimlendirici, satır tampona sığmazsa **sessizce kırpmaz**: boş satır ve `0` döndürür.
  En uzun olası satır derleme zamanında hesaplanıp `_Static_assert` ile doğrulanır.

---

## 5. Ölçüm Tanımları

| Terim | Tanım |
|---|---|
| **PPR** (pulse per revolution) | Bir kanalın (A veya B) bir turda ürettiği darbe sayısı = **100** (proje gereksinimi) |
| **CPR** (count per revolution) | x4 kuadratür çözümleme ile turda sayım = 100 × 4 = **400** |
| `pulses` | Açılıştan bu yana **işaretli toplam sayım** (x4 sayım biriminde). 64-bit, pratikte taşmaz |
| `delta` | Son 1 s penceresindeki **işaretli net sayım değişimi** |
| `dir` | `CW`: sayaç artıyor; `CCW`: sayaç azalıyor; `STOP`: \|delta\| ≤ eşik |
| `rpm` | Hızın büyüklüğü, 2 ondalık basamak. Yön ayrı alanda verilir |

**RPM formülü:**

```
RPM = (delta / CPR) × (60 000 ms / pencere_ms) = delta × 60 / 400 = delta × 0,15     (pencere = 1000 ms)
```

- **Çözünürlük:** 1 sayım = **0,15 RPM**. Ölçüm ±1 sayım kuantalama belirsizliği taşır.
- **Tamsayı aritmetiği:** Hesap `rpm × 100` (centi-RPM) cinsinden 64-bit ara değerle yapılır.
  400 CPR ve 1000 ms ile sonuç **tam** çıkar, yuvarlama gerekmez (1 sayım = 15 centi-RPM).
  Farklı CPR değerlerinde sıfırdan uzağa yuvarlanır ve int32'ye doyurulur. Kodda kayan nokta yoktur.
- **Yön tanımı:** `CW` "sayaç artıyor" demektir; bu da A kanalının B'den önde olduğu dönüştür. Fiziksel
  saat yönü encoder'ın montajına bağlıdır. Ters olması istenirse A/B kabloları yer değiştirilir ya da
  CubeMX'te IC1 polaritesi `Falling` yapılır. Yazılımda değişiklik gerekmez.
- **Duruş:** Pencerede sayım değişmezse `delta = 0` olur ve `dir=STOP rpm=0.00` gönderilir. Motor
  durduktan sonraki **ilk tam pencerede** RPM sıfırdır. Durma anı bir pencerenin ortasına denk
  gelirse o pencere kısmi hareketi gösterir; bir sonraki pencere sıfırdır.
- **Durma eşiği:** `SPEED_STOP_THRESHOLD_COUNTS` (varsayılan `0`). Bir kenarın tam üzerinde duran
  encoder ±1 sayım titreyebilir. Eşik örneğin `1` yapılırsa bu titreşim `STOP` olarak raporlanır.
  Toplam pulse yine doğru birikir.

---

## 6. Kenar Durumlar

| Durum | Davranış | Nasıl |
|---|---|---|
| **Sayaç taşması** (0xFFFFFFFF → 0) | Fark doğru hesaplanır | `encoder_count_delta()`: işaretsiz modüler fark → işaretliye **tanımlı** dönüşüm (implementation-defined cast yok) |
| **Negatif yönde taşma** (0 → 0xFFFFFFFF) | Negatif fark doğru | Aynı fonksiyon; testlerde iki yön de var |
| **Ters dönüş** | `delta < 0`, `dir=CCW`, `pulses` azalır, `rpm` pozitif büyüklük | |
| **Pencere içinde yön değiştirme** | Net yer değiştirme ölçülür (örn. +300 −100 → +200 → 30,00 RPM CW) | Ölçüm tanımı gereği |
| **Toplam sayımın 32 biti aşması** | `pulses` 64 bit olduğu için devam eder | Test: 2^32'yi aşan birikim |
| **Bir pencere UART'a yazılamazsa** (gönderim meşgul veya DMA başlatma hatası) | Sonraki satırda `missed=N`; toplam kaybolmaz | Sıra numarası farkı + raporlanmamış pencere sayacı |
| **Pencere süresi yanlış yapılandırılmışsa** | Açılışta `Error_Handler()` | Çalışma anı doğrulaması |
| **İlk ölçüm** | Tam 1 s hareketi kapsar | Başlangıç değeri TIM6 başlatılmadan hemen önce alınır |

Fark hesabının geçerlilik sınırı \|gerçek değişim\| < 2^31 sayım/pencere'dir. Bu yaklaşık 2,1 milyar
sayım/s eder ve fiziksel olarak ulaşılamaz.

---

## 7. UART Protokolü

Her satır ASCII'dir ve `\r\n` ile biter. Açılışta bir kez başlık satırı gönderilir:

```
# encoder_rpm fw=1.0.0 ppr=100 cpr=400 window_ms=1000
```

Ardından her saniye bir ölçüm satırı gelir:

```
t=<pencere no>s pulses=<±toplam> delta=<±fark> dir=<CW|CCW|STOP> rpm=<büyüklük, 2 ondalık>[ missed=<n>]
```

- İşaretli sayılarda pozitif değer `+`, negatif değer `-` ile yazılır, sıfırın işareti yoktur.
- `missed` alanı, önceki satırdan bu yana tamamlanıp UART'a yazılamayan pencere sayısıdır. Yalnız
  sıfırdan büyükse eklenir.
- `t` alanı 1 s'lik pencere sırasıdır (pencere süresine göre saniye), gerçek saat değildir.

### 7.1 Örnek çıktı (host simülasyonundan **üretilmiş** gerçek çıktı)

Aşağıdaki satırlar `Tests/test_scenarios.c` içindeki hareket profilinin firmware'deki aynı
`speed_monitor` → `encoder_math` → `telemetry` zincirinden geçirilmesiyle üretilmiştir
(`build/host/uart_sample_output.txt`). Profil sırasıyla şu evrelerden geçer: hızlanma, sabit hız,
durma, ters dönüş, sayacın 0 noktasından geriye taşması ve tekrar durma.

```
t=1s pulses=+100 delta=+100 dir=CW rpm=15.00
t=2s pulses=+300 delta=+200 dir=CW rpm=30.00
t=3s pulses=+700 delta=+400 dir=CW rpm=60.00
t=4s pulses=+1100 delta=+400 dir=CW rpm=60.00
t=5s pulses=+1100 delta=0 dir=STOP rpm=0.00
t=6s pulses=+700 delta=-400 dir=CCW rpm=60.00
t=7s pulses=+300 delta=-400 dir=CCW rpm=60.00
t=8s pulses=0 delta=-300 dir=CCW rpm=45.00
t=9s pulses=-1 delta=-1 dir=CCW rpm=0.15
t=10s pulses=-400 delta=-399 dir=CCW rpm=59.85
t=11s pulses=-400 delta=0 dir=STOP rpm=0.00
```

`t=9s` satırında ham TIM2 sayacı `0x00000000`'dan `0xFFFFFFFF`'e geriye taşar. Çıktı yine doğrudur:
`delta=-1`. Gönderim kuyruğu meşgulken kaçırılan pencere örneği:

```
t=4s pulses=+1200 delta=0 dir=STOP rpm=0.00 missed=2
```

---

## 8. Derleme ve Çalıştırma

### 8.1 Gereksinimler

- CMake ≥ 3.22 ve Ninja
- `arm-none-eabi-gcc`, PATH'te olmalı (STM32CubeIDE veya STM32CubeCLT ile gelir)
- İsteğe bağlı: STM32CubeMX 6.17 (`.ioc` dosyasını açmak veya kodu yeniden üretmek için)
- Kart için: STM32CubeProgrammer veya ST-LINK uyumlu bir yükleyici

### 8.2 Firmware

```bash
cmake --preset Debug -S .
cmake --build --preset Debug          # çıktı: build/Debug/encoder_rpm.elf

cmake --preset Release -S .
cmake --build --preset Release        # çıktı: build/Release/encoder_rpm.elf
```

Kartı yüklemek için:

```bash
STM32_Programmer_CLI -c port=SWD -w build/Release/encoder_rpm.elf -v -rst
```

VS Code: *CMake Tools* eklentisiyle `Debug` veya `Release` preset'i seçilerek derlenir.
STM32CubeIDE: `encoder_rpm.ioc` açılarak proje içe aktarılabilir.

### 8.3 CubeMX ile yeniden üretim

`encoder_rpm.ioc`, CubeMX 6.17'de açılıp *Generate Code* yapılabilir. Uygulama kodu `App/`
klasöründedir. `Core/` içindeki değişiklikler yalnız `USER CODE` bloklarındadır, bu yüzden yeniden
üretimde korunur.

### 8.4 Seri terminal

115200 8N1 ayarlı herhangi bir terminal kullanılabilir (PuTTY, Tera Term, VS Code Serial Monitor,
`python -m serial.tools.miniterm COMx 115200`).

---

## 9. Testler (donanım gerektirmez)

### 9.1 Çalıştırma

```bash
cmake -S Tests -B build/host -G Ninja -DCMAKE_C_COMPILER=gcc
cmake --build build/host
ctest --test-dir build/host --output-on-failure -V

# İsteğe bağlı: tanımsız davranış denetimi (UBSan, çalışma zamanı kütüphanesi gerektirmez)
cmake -S Tests -B build/ubsan -G Ninja -DCMAKE_C_COMPILER=gcc       "-DCMAKE_C_FLAGS=-O1 -fsanitize=undefined -fsanitize-trap=undefined"
cmake --build build/ubsan && ./build/ubsan/encoder_tests
```

Host testleri firmware'in **aynı kaynak dosyalarını** (`encoder_math.c`, `speed_monitor.c`,
`telemetry.c`) derler. Kopya veya taklit kod kullanılmaz. Derleme şu bayraklarla yapılır:
`-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Wdouble-promotion -Wcast-align
-Wstrict-prototypes -Wmissing-prototypes -Werror`. Dış test kütüphanesi kullanılmaz
(`Tests/test_framework.h`).

### 9.2 Test senaryoları

| Senaryo | Test(ler) | Beklenen sonuç |
|---|---|---|
| **Normal dönüş** | `normal_rotation_400_and_10000_counts` | +400 sayım/s → `60.00` RPM `CW`; +10 000 → `1500.00` RPM |
| **Ters dönüş** | `reverse_rotation_and_stop` | −400 → `60.00` RPM `CCW`, `pulses` negatif |
| **Durma** | `reverse_rotation_and_stop`, profil `t=5s`, `t=11s` | `delta=0` → `dir=STOP rpm=0.00`, toplam korunur |
| **Yön değiştirme** | `direction_change_and_within_window_net_displacement` | CW → CCW geçişi; pencere içi +300/−100 → net +200 → `30.00` RPM CW |
| **Sayaç taşması** | `counter_wrap_both_directions_and_sign_boundary` | `0xFFFFFF00→0x100 = +512`, `0x10→0xFFFFFFF0 = −32`, `0x7FFFFFFF→0x80000000 = +1` |
| **32/64-bit birikim** | `accumulated_position_beyond_int32_and_uint32` | Toplam INT32_MAX ve 2^32'yi aşınca doğru |
| **Kaçırılan pencere** | `missed_windows_keep_total_and_latest_delta`, `busy_consumer_skips_windows_without_losing_position` | `missed=2`, toplam kayıpsız |
| **İlk örnekten önce** | `no_sample_before_first_window_and_single_consumption` | İlk pencere dolmadan örnek yok; her örnek bir kez tüketilir |
| **RPM yuvarlama / doyma** | `rpm_rounding_half_away_from_zero`, `rpm_saturation_zero_guards_and_large_denominator` | 1024 CPR'de sıfırdan uzağa yuvarlama; INT32 sınırları; sıfır payda koruması |
| **Durma eşiği** | `stop_threshold_inclusive_and_int32_min`, `configured_deadband_forces_zero_rpm_but_retains_position` | \|delta\| ≤ eşik → `STOP` |
| **Zaman tabanı** | `timebase_exact_off_by_one_and_overflow`, `apb_clock_div1_and_divided` | 84 MHz / 8400 / 10 000 = 1000 ms doğru; ±1 hatalı değerler reddedilir |
| **UART biçimi** | `sample_exact_specification_lines`, `formatter_*`, `banner_exact_and_failure_is_empty` | Birebir satır eşleşmesi, int64 uç değerleri, her tampon boyutunda kırpmasız davranış |
| **Uçtan uca profil** | `motion_profile_accelerate_constant_stop_reverse_wrap_stop` | §7.1'deki 11 satır birebir |

### 9.3 Sonuç

```
Summary: 23 passed, 0 failed
100% tests passed out of 1
```

---

## 10. Doğrulama Kapsamı

| Doğrulama | Yöntem | Sonuç |
|---|---|---|
| Ölçüm mantığı (fark, RPM, yön, durma, taşma, biçim) | 23 host testi, firmware ile aynı kaynak dosyalar, GCC 16.2.0, `-Werror` | **23 / 23 geçti** |
| Tanımsız davranış (UB) | Aynı test paketi `-fsanitize=undefined -fsanitize-trap=undefined` ile | **23 / 23 geçti**, UB tespit edilmedi |
| Uçtan uca UART çıktısı | Hareket profili simülasyonu → gerçek üretilmiş satırlar (§7.1) | Beklenen satırlarla birebir |
| Firmware derlemesi (Debug, `-O0 -g3`) | `arm-none-eabi-gcc` 13.3.1, uygulama dosyalarında katı uyarı bayrakları | **0 uyarı**; FLASH 19 760 B, RAM 2 072 B |
| Firmware derlemesi (Release) | aynı | **0 uyarı**; FLASH 10 168 B, RAM 2 072 B |
| Çevre birimi konfigürasyonu | STM32CubeMX 6.17.0 ile `.ioc`'tan kod üretimi; saat, PSC/ARR, AF, DMA ve NVIC değerleri üretilmiş kodda tek tek kontrol edildi | Tutarlı |
| Pin seçimi | UM1472 Rev 9 pin tablosu (Free I/O, SB12/SB20 köprüleri) | Çakışma yok |
| Zaman tabanı | Açılışta çalışma anı kontrolü (`encoder_timebase_period_matches`) | Yanlış konfigürasyon `Error_Handler`'a gider |
| Kart üzerinde çalışma | Aşağıdaki §11 prosedürü | Prosedür ve beklenen çıktılar tanımlı |

---

## 11. Donanım Üzerinde Test Prosedürü

**Gerekenler:** STM32F4DISCOVERY, A/B çıkışlı bir encoder (veya iki kanallı fonksiyon jeneratörü
ya da 90° faz farklı kare dalga üretebilen ikinci bir kart), 3,3 V USB-TTL dönüştürücü.

| # | Adım | Beklenen |
|---|---|---|
| 1 | Firmware'i yükle, terminali 115200 8N1 aç, kartı resetle | `# encoder_rpm fw=1.0.0 ppr=100 cpr=400 window_ms=1000` |
| 2 | Encoder sabit dururken bekle | Her saniye `delta=0 dir=STOP rpm=0.00`, `t` 1'er artar |
| 3 | Mili elle tam 1 tur ileri çevir | `pulses` +400 artar (±1), `dir=CW` |
| 4 | Aynı turu geri çevir | `pulses` başlangıca döner, `dir=CCW` |
| 5 | Jeneratör: A ve B'ye 100 Hz, B 90° geride | `delta=+400 dir=CW rpm=60.00` (±1 sayım = ±0,15 RPM) |
| 6 | Jeneratör: B 90° önde (faz ters) | `delta=-400 dir=CCW rpm=60.00` |
| 7 | Jeneratör: 2,5 kHz | `delta=+10000 rpm=1500.00` |
| 8 | Sinyali kes | Bir sonraki tam pencerede `delta=0 dir=STOP rpm=0.00` |
| 9 | Çalışırken dönüş yönünü değiştir | Geçiş penceresinde net değer, sonraki pencerede yeni yön |
| 10 | (İsteğe bağlı) Hata ayıklayıcıda `TIM2->CNT`'yi `0xFFFFFF00` yap, CW döndür | Taşmada `delta` doğru, `pulses` süreklilik bozulmadan devam eder |

**Frekans–RPM ilişkisi:** `RPM = f_A × 60 / PPR`. Örneğin 100 Hz × 60 / 100 = 60 RPM; ayrıca
100 Hz × 4 kenar = 400 sayım/s.

---

## 12. Tasarım Kararları (Özet)

| Karar | Alternatif | Gerekçe |
|---|---|---|
| Donanım encoder modu (TIM2 TI12) | EXTI kesmesiyle yazılım sayımı | Her kenarda CPU yükü yok, kenar kaçırma riski yok, x4 çözünürlük ve donanım filtresi |
| TIM2 (32-bit) | TIM3/TIM4 (16-bit) | 16-bit sayaç 400 CPR'de 163 turda taşar; 32-bit ile taşma pratikte yok, yine de doğru ele alınır |
| TIM6 ile sabit pencere | Kenarlar arası süre ölçümü (period) | Proje gereksinimi "her 1 saniyede" güncelleme; sabit pencere duruşu doğal olarak 0 yapar |
| Ölçümü ISR'de, hesabı ana döngüde | Hepsi ISR'de | ISR kısa ve deterministik kalır; 64-bit bölme ve biçimleme kesme gecikmesini etkilemez |
| UART DMA | Bloklayan `HAL_UART_Transmit`, kesme tabanlı gönderim | CPU ve kesmeler bloklanmaz; ~50 bayt/s için tek DMA aktarımı yeterli |
| Kendi tamsayı biçimlendiricisi | `snprintf` | newlib-nano `%lld` desteklemez; deterministik bellek ve süre; kayan nokta yok |
| 84 MHz, HSE | 168 MHz veya HSI | §3.1–3.2 |
| Hesap mantığı HAL'den bağımsız | Doğrudan HAL içinde | Donanımsız birim testi ve okunabilirlik |

---

## 13. Klasör Yapısı

```
stm32-encoder-rpm/
├── README.md                  bu doküman
├── encoder_rpm.ioc            STM32CubeMX konfigürasyonu
├── CMakeLists.txt             firmware derlemesi (uygulama dosyalarına katı uyarılar)
├── CMakePresets.json          Debug / Release preset'leri
├── App/
│   ├── Inc/                   encoder_config.h, encoder_math.h, speed_monitor.h,
│   │                          telemetry.h, critical_section.h, app.h
│   └── Src/                   encoder_math.c, speed_monitor.c, telemetry.c, app.c
├── Core/                      CubeMX tarafından üretilen kod (main.c, msp, it)
├── Drivers/                   STM32F4 HAL ve CMSIS (FW_F4 V1.28.3)
├── Tests/                     host testleri (CMake, kendi test çatısı)
├── cmake/                     araç zinciri dosyaları
├── startup_stm32f407xx.s
└── STM32F407XX_FLASH.ld
```

## 14. Kaynaklar

- STMicroelectronics, *UM1472 — Discovery kit with STM32F407VG MCU*, Rev 9.
- STMicroelectronics, *RM0090 — STM32F405/415, STM32F407/417, STM32F427/437 and STM32F429/439 reference
  manual* (TIM2–TIM5 encoder interface, basic timers TIM6/TIM7, USART baud rate generation).
- STMicroelectronics, *STM32F407xx datasheet* (saat sınırları, flash bekleme süreleri).


## License

Application code is released under the MIT License (see [LICENSE](LICENSE)). The STM32Cube HAL and CMSIS components under `Drivers/` remain under their own licenses (see the `LICENSE.txt` files in those folders).

## Author

**southpaw** — application development and embedded integration.
See [AUTHORS.md](AUTHORS.md) for third-party attribution.
