# Analisi Repository Phiverb/Wayverb

**Data dell'analisi:** 2025-11-10  
**Repository:** https://github.com/Alemusica/phiverb  
**Branch analizzato:** copilot/analyze-repo-structure

---

## 1. SOMMARIO ESECUTIVO

**Wayverb** è un simulatore acustico ibrido per ambienti chiusi che combina metodi geometrici e di modellazione d'onda con accelerazione GPU. Il progetto produce risposte impulsive di stanza (Room Impulse Response - RIR) che possono essere utilizzate con riverberi a convoluzione per creare auralizzazioni realistiche di spazi virtuali.

### Caratteristiche principali:
- **Simulazione ibrida** che combina:
  - Metodi geometrici (image-source e ray-tracing) per alte frequenze
  - Mesh waveguide rettilineo per basse frequenze
- **Accelerazione GPU** tramite OpenCL (attuale) e Metal (in sviluppo per Apple Silicon)
- **Applicazione GUI** costruita con JUCE per configurare ed eseguire simulazioni
- **Strumenti CLI** per test e rendering headless

### Stato attuale:
- ✅ Versione funzionante su macOS con OpenCL
- 🚧 Port Metal per Apple Silicon in corso (branch `feature/metal-apple-silicon`)
- 📊 Circa 5.546 righe di codice C++ in 50 file analizzati
- 🎯 Focus su stabilità, performance e diagnostica

---

## 2. ARCHITETTURA DEL PROGETTO

### 2.1 Struttura delle Directory

```
phiverb/
├── src/                    # Codice sorgente della libreria
│   ├── core/              # Utilità generiche (DSP, strutture dati)
│   ├── raytracer/         # Componenti di acustica geometrica
│   ├── waveguide/         # Simulazione FDTD (Finite Difference Time Domain)
│   ├── combined/          # Integrazione raytracer + waveguide
│   ├── audio_file/        # Wrapper per libsndfile
│   ├── frequency_domain/  # Wrapper per FFTW
│   ├── hrtf/              # Generazione dati HRTF
│   ├── metal/             # Backend Metal (Apple Silicon, sperimentale)
│   └── utilities/         # Utilità autosufficienti
├── wayverb/               # Applicazione GUI (JUCE)
├── bin/                   # Programmi CLI per testing
├── docs/                  # Documentazione generata
├── docs_source/           # Sorgenti documentazione
├── tests/                 # Test scenes
├── scripts/               # Script Python/Octave per prototipazione
├── tools/                 # Tool di build e utilità
└── config/                # Configurazione documentazione
```

### 2.2 Componenti Principali

#### **Image-Source (Geometric Method)**
- **Scopo:** Riflessioni precoci ad alta frequenza
- **Implementazione:** Algoritmo deterministico per riflessioni speculari
- **File chiave:** `src/raytracer/include/raytracer/image_source/`

#### **Stochastic Ray-Tracing (Geometric Method)**
- **Scopo:** Riflessioni tardive ad alta frequenza
- **Implementazione:** Ray-tracing stocastico con BRDF
- **File chiave:** `src/raytracer/include/raytracer/stochastic/`

#### **Rectilinear Waveguide Mesh (Wave-Based Method)**
- **Scopo:** Tutti i contenuti a bassa frequenza
- **Implementazione:** FDTD con accelerazione GPU
- **File chiave:** `src/waveguide/`
- **Backend:** OpenCL (stabile), Metal (in sviluppo)

#### **Combined Engine**
- **Scopo:** Integrazione dei tre metodi
- **Implementazione:** Crossover frequenziale e post-processing
- **File chiave:** `src/combined/`

---

## 3. TECNOLOGIE E DIPENDENZE

### 3.1 Linguaggi
- **C++14/C++17:** Linguaggio principale
- **OpenCL:** Kernels GPU per waveguide (attuale)
- **Metal Shading Language (MSL):** Kernels GPU per Apple Silicon (futuro)
- **Objective-C++:** Wrapper Metal

### 3.2 Librerie Principali
- **JUCE:** Framework per l'applicazione GUI
- **libsndfile:** I/O file audio
- **FFTW:** Trasformate di Fourier
- **Assimp:** Caricamento modelli 3D
- **OpenCL:** Calcolo GPU (macOS)
- **Metal:** Calcolo GPU (Apple Silicon, in sviluppo)

### 3.3 Build System
- **CMake 3.0+:** Build principale
- **Xcode:** Build app GUI macOS
- **Clang (Apple LLVM 8):** Compilatore richiesto

### 3.4 Dipendenze di Sistema (Homebrew)
```bash
brew install cmake autoconf autogen automake libtool pkg-config
```

---

## 4. REQUISITI E PIATTAFORME

### 4.1 Requisiti di Esecuzione
- **Sistema Operativo:** macOS 10.10 o superiore
- **GPU:** Con supporto double-precision
- **Raccomandato:** Mac recente con scheda grafica dedicata

### 4.2 Requisiti di Build
- **macOS:** 10.10+
- **Compilatore:** Clang recente con supporto C++14 e header C++17 sperimentali
- **CMake:** 3.0+
- **Xcode:** Per build GUI

### 4.3 Note su Apple Silicon (M-series)
Il progetto è in fase di port per Apple Silicon:
- **Branch dedicato:** `feature/metal-apple-silicon`
- **Documentazione:** `docs/APPLE_SILICON.md`
- **Status:** Backend Metal in scaffold, fallback a OpenCL funzionante
- **Flag di build:** `WAYVERB_ENABLE_METAL=ON`

---

## 5. WORKFLOW DI BUILD

### 5.1 Build Standard (OpenCL)
```bash
# Via script di build
./build.sh

# O manualmente con CMake
cmake -S . -B build
cmake --build build -j
```

### 5.2 Build Metal (Apple Silicon)
```bash
# Via tool helper
tools/build_metal.sh

# O manualmente
cmake -S . -B build-metal -DWAYVERB_ENABLE_METAL=ON
cmake --build build-metal -j
```

### 5.3 Build GUI (Xcode)
```bash
# Apri il progetto Xcode
open wayverb/Builds/MacOSX/wayverb.xcodeproj

# Oppure usa lo script
./xcproj.sh
```

**Nota:** Il primo build sarà molto lento a causa del download e compilazione delle dipendenze.

---

## 6. TESTING E VALIDAZIONE

### 6.1 Strumenti CLI di Test

Il repository include numerosi tool di test in `bin/`:

- **`apple_silicon_regression`**: Test di regressione veloce per Apple Silicon
  - Supporta override CLI (`--scene`, `--source`, `--receiver`)
  - Fallisce se l'output contiene NaN/Inf o è silenzioso
  
- **`sanitize_mesh`**: Sanitizza modelli OBJ (nessuna semplificazione)

- **`render_binaural`**: Rendering binaurale CLI

- **`boundary_test`**: Test per boundary conditions

- **`waveguide_distance_test`**: Test di distanza waveguide

### 6.2 Test Automatici
```bash
# Enable testing
enable_testing()  # In CMakeLists.txt

# Test suite per componenti
src/raytracer/tests/
src/waveguide/tests/
```

### 6.3 Validazione Mesh 3D

I modelli 3D **devono essere** solidi e watertight:

1. Aprire in SketchUp
2. Select-all e Edit > Make Group
3. Verificare che Entity Info mostri un volume
4. Debuggare con plugin 'Solid Inspector' se necessario

**Importante:** Wayverb interpreta le unità del modello come metri. Esportare in `.obj` quando possibile.

---

## 7. LOGGING E DIAGNOSTICA

### 7.1 Sistema di Logging

Il progetto ha un sistema di logging robusto per diagnostica offline:

```bash
# Avviare app con logs
CL_LOG_ERRORS=stdout \
WAYVERB_LOG_DIR="$(pwd)/build/logs/crash" \
tools/run_wayverb.sh > build/logs/app/wayverb-$(date +%Y%m%d-%H%M%S).log 2>&1 &

# Monitorare log filtrati
scripts/monitor_app_log.sh "$(ls -t build/logs/app/wayverb-*.log | head -1)"
```

**Posizioni log:**
- **App console:** `build/logs/app/wayverb-*.log`
- **Crash logs:** `~/Library/Logs/Wayverb` (o `$WAYVERB_LOG_DIR`)

### 7.2 Build Provenance

Ogni build UI mostra la propria identità nella barra del titolo:
- Versione
- `git describe`
- Branch
- Timestamp

Derivati da `core/build_id.h` / `wayverb/Source/Application.cpp`.

**Pre-build script** scrive automaticamente `wayverb/Source/build_id_override.h`.

---

## 8. PERFORMANCE E OTTIMIZZAZIONE

### 8.1 Variabili di Ambiente per Performance

Il waveguide domina il runtime (milioni di nodi × migliaia di step):

```bash
# Disabilita readback GPU→CPU pressioni nodi
WAYVERB_DISABLE_VIZ=1

# Aggiorna UI ogni N step
WAYVERB_VIZ_DECIMATE=10

# Riduci padding voxel
WAYVERB_VOXEL_PAD=3  # default: 5
```

**Motivo:** Il readback completo del buffer per step blocca la coda con mesh grandi.

### 8.2 Parametri Raccomandati per Scene Pesanti

```
- Cutoff: 500–800 Hz
- Usable portion: 0.60 (clamped)
- Image-source order: 3–4
- Rays: 1e5–2e5
```

La "usable portion" è limitata a `[0.10, 0.60]` per stabilità/performance.

---

## 9. PORT METAL (APPLE SILICON)

### 9.1 Status Attuale

**Branch:** `feature/metal-apple-silicon`

**Implementato:**
- ✅ Scaffold Metal context (`src/metal/metal_context.h`)
- ✅ Traduzione kernels waveguide in MSL
- ✅ Headers condivisi Metal/host
- ✅ Pipeline compilation infrastructure
- 🚧 Runtime execution path (fallback a OpenCL)

**In Sviluppo:**
- Esecuzione end-to-end su Metal
- Performance tuning
- Validazione parità numerica con OpenCL

### 9.2 Flag e Controlli

**Build:**
```bash
-DWAYVERB_ENABLE_METAL=ON
```

**Runtime:**
```bash
WAYVERB_METAL=1              # Usa Metal
WAYVERB_METAL=force-opencl   # Forza fallback OpenCL
```

### 9.3 Piano di Implementazione

**Phase A:** Waveguide kernels → Metal compute pipeline
- Buffer layouts 1:1 mapping
- Kernels MSL (`node_inside`, `node_boundary`, `boundary_*`)
- MTLHeaps privati + triple-buffering
- Command graphs per stage

**Phase B:** Raytracer intersections → Metal
- BVH build su CPU
- Batch rays/intersections su GPU
- Structure-of-arrays per triangoli

**Phase C:** Post-processing → Accelerate/vDSP
- FIR/FFT via vDSP
- HRTF convolution

**Phase D:** Instrumentation
- `os_signpost` + MTLCapture
- Export timings CSV

### 9.4 Obiettivi Performance

Miglioramento atteso: **10–30% wall-time** prima del tuning kernel-level, grazie a:
- Minor overhead CPU submit
- Meno traffico memoria
- Migliore occupancy GPU

---

## 10. STATO SVILUPPO E ROADMAP

### 10.1 Issues Critici (da `todo.md`)

**Raytracer:**
- ☐ Verifica energia ongoing (direction scattering-weighted)
- ☐ Diffuse rain deterministico
- ☐ Validazione early reflections (ISM vs RT)

**Waveguide:**
- ☐ Soft source senza solution growth
- ☐ Solo output ambisonico?
- ☐ Multi-band waveguide NaNs (boundary coeffs)
- ☐ Fix "Tried to read non-existant node" su fractal mesh
- ☐ Completare execution path Metal

**App:**
- ☐ Finire help panel info
- ☐ Benchmark GPU vs CPU performance

### 10.2 Development Log Highlights

**2025-11-08:**
- ✅ Diagnostica waveguide "outside mesh"
- ✅ Geometrie in `geometrie_wayverb/`
- ✅ Tool mesh (`tools/wayverb_mesh.py`, Swift helper)
- ✅ Geometry panel UI

**2025-11-07:**
- ✅ Rilevatori NaN/Inf ricorsivi
- ✅ `bin/apple_silicon_regression` con CLI
- ✅ Auto-logging UI in `build/logs/app/`
- ✅ Direct-path fallback

**2025-11-06:**
- ✅ Metal pipeline scaffolding
- ✅ Port kernels waveguide a Metal
- ✅ Headers condivisi Metal

**2025-11-05:**
- ✅ Metal layout parity scaffold

### 10.3 Action Plan (da `docs/action_plan.md`)

Il repository mantiene un action plan dettagliato in italiano con:

1. **Raytracer** — energia e diffuse rain
2. **Waveguide** — sorgente trasparente e boundary stabili
3. **Metal backend** — parità con OpenCL
4. **Materiali / Output / UI**
5. **QA & Tooling**

Ogni sezione ha checklist dettagliate con criteri di accettazione.

---

## 11. DOCUMENTAZIONE

### 11.1 Documentazione Disponibile

**README principale:** `readme.md`
- Synopsis del progetto
- Note d'uso
- Requisiti
- Istruzioni build
- Struttura progetto

**Documentazione tecnica:** `docs/`
- `APPLE_SILICON.md`: Guida developer Apple Silicon
- `metal_port_plan.md`: Piano port Metal
- `development_log.md`: Log sviluppo cronologico
- `action_plan.md`: Piano azione con checklist
- `AGENT_PROMPTS.md`: Note per agenti AI
- `mesh_tools.md`: Strumenti mesh

**Documentazione sorgente:** `docs_source/`
- Teoria acustica
- Implementazione boundary
- Microphone modeling
- Image source
- Ray tracing
- Waveguide
- Hybrid approach

**HTML generato:** `docs/*.html`
- Documentazione navigabile
- Referenze bibliografiche

### 11.2 AGENTS.md

File di istruzioni operative per assistenti AI che lavorano nel repository:

**Core goals:**
- Mantenere stabilità UI e tool headless
- Priorità a performance Apple Silicon (Metal)
- Preservare OpenCL come fallback stabile
- Log ricchi per triage offline

**Policy chiave:**
- Minimizzare cambiamenti
- Evitare breakage API
- Aggiornare docs con behavior/flag changes
- Feature flags > alterare defaults

---

## 12. WORKFLOW DI SVILUPPO

### 12.1 Stile di Codifica

Da `AGENTS.md`:
- **Cambiamenti minimi** e localizzati
- **Evitare API breakage**
- **Aggiornare documentazione** quando necessario
- **Feature flags** invece di alterare defaults

### 12.2 Validazione

**Prima di commit:**
1. Run linters se disponibili
2. Build progetto
3. Run test suite
4. Verificare logs per errori

**Per performance features:**
- Path loggable per comparare timings
- Opt-in via environment flags
- Profiling con event sampling

**Per fix "All channels are silent":**
1. Lanciare UI via `tools/run_wayverb.sh`
2. Catturare log completo
3. Allegare build label + filtered log
4. Usare `scripts/monitor_app_log.sh`

### 12.3 Tool di Sviluppo

```bash
# Build script principale
./build.sh

# Build Metal
tools/build_metal.sh

# Scrivi build ID header
tools/write_build_id_header.sh

# Compila documentazione
./compile_docs.sh

# Lancia app con logging
tools/run_wayverb.sh

# Monitora log app
scripts/monitor_app_log.sh <logfile>

# Test regressione Apple Silicon
bin/apple_silicon_regression --scene <file.obj>
```

---

## 13. GEOMETRIE E SCENE

### 13.1 Gestione Geometrie

**Directory:** `geometrie_wayverb/` (nota: attualmente non presente ma referenziata in docs)

**Best practices:**
- Committare ogni mesh/preset usato per debug
- Referenziare filename + CLI params nei log
- Usare `--scene geometri_wayverb/<name>.obj` per riproducibilità

### 13.2 Test Scenes

**Directory:** `tests/scenes/`

Scene di test per validazione e regressione.

### 13.3 Tool Geometria (UI)

**Panel sinistro → "geometry":**
- **Analyze:** Report vertici, triangoli, zero-area, duplicati, bordi boundary/non-manifold, flag watertight
- **Sanitize:** Weld/rimuovi degenerati con epsilon (default `1e-6`)

**CLI:** `bin/sanitize_mesh`

**Codice:** `src/core/include/core/geometry_analysis.h`

---

## 14. SICUREZZA E QUALITÀ

### 14.1 Validazione Input

Il progetto è particolarmente attento alla validazione di:
- **File 3D:** Mesh devono essere watertight
- **Parametri fisici:** Guard contro NaN/Inf
- **Buffer GPU:** Guard tags per boundary nodes

### 14.2 Diagnostica Fail-Fast

**NaN/Inf detection:**
- Rilevatori ricorsivi in `core/dsp_vector_ops`
- Logging in `combined::postprocess`
- Report quanti sample NaN/Inf prima del crossover mix
- Fallback sanitizers zero-out non-finite samples

**Boundary validation:**
- Guard-tag infrastructure per ogni boundary node
- Host seed tag deterministico (`node_index ^ 0xA5A5A5A5`)
- OpenCL kernels validano prima di toccare filter state
- CL validator kernel verifica consistency on device startup

### 14.3 Testing Policy

**Da AGENTS.md:**
- Quando si aggiungono performance features, fornire path loggable per comparare timings
- Prima di dichiarare fix per "All channels are silent", eseguire almeno un UI render
- Usare `bin/apple_silicon_regression` per smoke test headless
- Tool fallisce se output contiene NaN/Inf o è silenzioso

---

## 15. EXTERNAL RESEARCH POLICY

**Da AGENTS.md:**

Se sono necessari background/papers/specs aggiuntivi (e.g., Metal Compute Graphs, MPSRayIntersector, waveguide numerics), l'agente può richiedere al maintainer di fetch/riassumere via ChatGPT Pro 5 esterno.

**Meccanismo:**
1. Scrivere nota in chat indirizzata al creator ("@creator")
2. Descrivere info mancante e domande/fonti esatte richieste
3. Attendere risposta prima di procedere quando l'info è blocker-critica

---

## 16. FILE DI CONFIGURAZIONE

### 16.1 CMakeLists.txt

**Principali opzioni:**
```cmake
PROJECT(WAYVERB VERSION 0.1)
option(WAYVERB_ENABLE_METAL "Build experimental Metal backend" OFF)
set(ONLY_BUILD_DOCS false)
```

**Macro iniettate:**
- `WAYVERB_BUILD_GIT_DESC`
- `WAYVERB_BUILD_GIT_BRANCH`
- `WAYVERB_BUILD_TIMESTAMP`

**Per Apple:**
```cmake
if(APPLE)
    add_definitions(-DWAYVERB_FORCE_SINGLE_PRECISION=1)
    if(WAYVERB_ENABLE_METAL)
        add_definitions(-DWAYVERB_ENABLE_METAL=1)
    endif()
endif()
```

### 16.2 .gitignore

Esclude:
- Build artifacts
- Dipendenze compilate
- File temporanei
- IDE-specific files

### 16.3 .clang-format

Formattazione codice C++ consistente.

---

## 17. CONTINUOUS INTEGRATION

### 17.1 Travis CI

**File:** `.travis.yml`

Configurato per:
- Build automatico
- Test suite
- Generazione documentazione

### 17.2 GitHub Actions

**Directory:** `.github/`

Template PR e potenziali workflow CI.

---

## 18. LICENZA

**File:** `LICENSE`

Il software è fornito "as is", senza garanzie di alcun tipo.

Dettagli completi nel file LICENSE del repository.

---

## 19. CONCLUSIONI E RACCOMANDAZIONI

### 19.1 Punti di Forza

✅ **Architettura ben strutturata** con separazione chiara dei componenti  
✅ **Documentazione estensiva** sia tecnica che operativa  
✅ **Sistema di logging robusto** per debugging offline  
✅ **Build provenance** chiara per tracciabilità  
✅ **Test suite** con tool CLI dedicati  
✅ **Attenzione alla qualità** con validazione fail-fast  
✅ **Roadmap chiara** per port Apple Silicon  

### 19.2 Aree di Attenzione

⚠️ **Port Metal in corso** - Attualmente in scaffold, fallback a OpenCL  
⚠️ **Complessità build** - Primo build molto lento per dipendenze  
⚠️ **Platform-specific** - Principalmente macOS, GPU requirements  
⚠️ **Issues aperti** - Diversi item critici in todo.md  

### 19.3 Raccomandazioni per Contribuire

1. **Leggere AGENTS.md** prima di iniziare
2. **Seguire action_plan.md** per task strutturati
3. **Usare tool di logging** forniti per debugging
4. **Mantenere compatibilità** OpenCL durante sviluppo Metal
5. **Testare con scenes watertight** validate
6. **Documentare cambiamenti** in development_log.md

### 19.4 Prossimi Passi Suggeriti

**Short-term:**
1. Completare execution path Metal per waveguide
2. Risolvere "Tried to read non-existant node" su fractal mesh
3. Implementare diffuse rain deterministico
4. Completare help panel UI

**Medium-term:**
1. Validazione parità numerica OpenCL/Metal
2. Performance benchmarking GPU vs CPU
3. Integrazione dataset materiali PTB/openMat
4. Output FOA (First Order Ambisonics)

**Long-term:**
1. Port completo su Metal con tuning performance
2. Supporto real-time simulation
3. Fuzzing per robustezza input
4. Estensione a altre piattaforme (Linux, Windows)

---

## 20. RISORSE UTILI

### 20.1 Link Documentazione

- **Sito progetto:** https://reuk.github.io/wayverb/
- **Repository GitHub:** https://github.com/Alemusica/phiverb
- **README principale:** `readme.md`
- **Guida Apple Silicon:** `docs/APPLE_SILICON.md`
- **Piano sviluppo:** `docs/action_plan.md`

### 20.2 File Chiave da Conoscere

- `AGENTS.md` - Istruzioni operative
- `todo.md` - Issue tracker
- `docs/development_log.md` - Cronologia sviluppo
- `CMakeLists.txt` - Configurazione build
- `build.sh` - Script build principale

### 20.3 Comandi Rapidi

```bash
# Build completo
./build.sh

# Build Metal
tools/build_metal.sh

# Test regressione
bin/apple_silicon_regression --scene test.obj

# App con logging
tools/run_wayverb.sh > build/logs/app/wayverb-$(date +%Y%m%d-%H%M%S).log 2>&1

# Monitora log
scripts/monitor_app_log.sh build/logs/app/wayverb-latest.log
```

---

**Fine dell'analisi**

*Questo documento fornisce una panoramica completa del repository Phiverb/Wayverb al 2025-11-10. Per informazioni aggiornate, consultare sempre la documentazione nel repository.*
