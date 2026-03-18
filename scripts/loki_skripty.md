# LOKI — Referencia skriptov

`build.sh` | `clean.sh` | `run.sh` | `test.sh`

---

## build.sh — konfigurácia a kompilácia

Spúšťa `cmake` konfiguráciu a `mingw32-make`. Vždy celý cmake configure od začiatku — pomalý, ale spoľahlivý. Používaj keď meníš `CMakeLists`, pridávaš knižnicu alebo robíš prvý build.

```
./scripts/build.sh [debug|release] [--tests] [--copy-dlls]
```

| Príkaz | Kedy použiť |
|---|---|
| `./scripts/build.sh` | Štandardný debug build bez testov |
| `./scripts/build.sh --tests --copy-dlls` | Prvý build s testami (kopíruje DLL-ky) |
| `./scripts/build.sh release` | Release build pred publishovaním |
| `./scripts/build.sh debug --tests` | Rebuild s testami, keď DLL-ky už existujú |

`--copy-dlls` je potrebný po prvom builde a po zmene toolchainu — kopíruje
`libgcc_s_seh-1.dll`, `libstdc++-6.dll` a `libwinpthread-1.dll` do
`build/debug/apps/loki/` a `build/debug/tests/`.

---

## run.sh — rýchle spustenie aplikácie

Inkrementálny build (len zmenené súbory) + okamžité spustenie `loki.exe`.
`cmake` sa nespúšťa znovu — rýchle. Automaticky skopíruje DLL-ky ak chýbajú.

```
./scripts/run.sh [config]
```

| Príkaz | Kedy použiť |
|---|---|
| `./scripts/run.sh` | Bežná práca — uprav kód, spusti |
| `./scripts/run.sh config/iny.json` | Iný konfiguračný súbor |

---

## clean.sh — vymazanie build adresárov

Maže `build/debug/` a/alebo `build/release/`. Príkaz `rebuild` zavolá `build.sh` automaticky.

```
./scripts/clean.sh [debug|release|all|rebuild] [--tests]
```

| Príkaz | Kedy použiť |
|---|---|
| `./scripts/clean.sh` | Vymaž oba buildy (pred commitom, pri divných chybách) |
| `./scripts/clean.sh debug` | Vymaž len debug |
| `./scripts/clean.sh rebuild` | Čistý rebuild bez testov |
| `./scripts/clean.sh rebuild --tests` | Čistý rebuild aj s testami |

---

## test.sh — spustenie testovej sady

Inkrementálny build, DLL check, `ctest`. Ak `build/debug/tests/` neexistuje,
automaticky zavolá `build.sh --tests --copy-dlls`.

```
./scripts/test.sh [--rebuild] [--filter <pat>] [--verbose] [--list]
```

| Príkaz | Kedy použiť |
|---|---|
| `./scripts/test.sh` | Spusti všetky testy (inkrementálny build) |
| `./scripts/test.sh --rebuild` | Čistý rebuild + všetky testy |
| `./scripts/test.sh --filter exceptions` | Len testy obsahujúce `exceptions` v názve |
| `./scripts/test.sh --filter stats --verbose` | Stats testy s plným výpisom každej assertcie |
| `./scripts/test.sh --list` | Zobraz zoznam všetkých testov bez spustenia |

---

## Typický pracovný postup

**Prvý setup projektu**
```bash
./scripts/build.sh debug --copy-dlls
```

**Bežná práca (upravuješ kód)**
```bash
./scripts/run.sh
```

**Práca na testoch**
```bash
./scripts/test.sh
./scripts/test.sh --filter timeStamp
```

**Niečo sa pokazilo — čistý začiatok**
```bash
./scripts/clean.sh rebuild
```

**Čistý rebuild aj s testami**
```bash
./scripts/clean.sh rebuild --tests
```

---

## Poznámky

| Téma | Poznámka |
|---|---|
| Kde spúšťať | Všetky skripty vždy z koreňa repozitára (kde leží `CMakeLists.txt`). |
| DLL-ky | Treba skopírovať po prvom builde a po zmene toolchainu. `build.sh --copy-dlls` to robí automaticky. |
| cmake vs make | `build.sh` spúšťa `cmake` zakaždým (pomalé). `run.sh` a `test.sh` volajú len `make` (rýchle). |
| Testy v release | Testy sa buildujú len v debug. Release build `--tests` nepodporuje. |
| Nové testy | Pridaj `.cpp` do `tests/unit/<modul>/` a zaregistruj v `tests/CMakeLists.txt` cez `loki_add_test_exe()`. |
