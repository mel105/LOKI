# LOKI GNSS Data Downloader

`tools/gnss_download/download.sh` -- shell script for downloading GNSS products
from verified institutional sources. Part of the LOKI framework infrastructure (Faza 0).

---

## Usage

```bash
# Single day
./tools/gnss_download/download.sh --date YYYY-MM-DD [OPTIONS]

# Date range
./tools/gnss_download/download.sh --from YYYY-MM-DD --to YYYY-MM-DD [OPTIONS]
```

### Options

| Option | Description | Default |
|---|---|---|
| `--date YYYY-MM-DD` | Single day download | required |
| `--from / --to` | Date range | required |
| `--station CODE` | 4-letter station code | `GOPE` |
| `--products LIST` | Comma-separated product list | `all` |
| `--output-dir PATH` | Base output directory | `INPUT/GNSS/gnss_data` |
| `--dry-run` | Show what would be downloaded | off |
| `--verbose` | Detailed curl output | off |

### Product identifiers

```
obs, nav, sp3, clk, ionex, bias, dcb, sinex, tropo,
vmf3, egnos, met, antex, tides, misc, era5
```

---

## Examples

```bash
# Dry run -- show what would be downloaded
./tools/gnss_download/download.sh --date 2024-03-15 --dry-run --verbose

# Download all working products for one day
./tools/gnss_download/download.sh --date 2024-03-15 \
  --products antex,nav,obs,sp3,clk,ionex,bias,dcb,vmf3,misc

# Download one week of core products
./tools/gnss_download/download.sh --from 2024-03-15 --to 2024-03-21 \
  --products obs,nav,sp3,clk,ionex

# Different station
./tools/gnss_download/download.sh --date 2024-03-15 --station GRAZ \
  --products obs,nav
```

---

## Output directory structure

```
INPUT/GNSS/gnss_data/
+-- antex/                  -- igs20.atx (one-time download)
+-- bias/                   -- CODE OSB SINEX BIAS
+-- clk/YYYY/DDD/           -- RINEX CLK precise clocks
+-- dcb/YYYY/               -- CODE DCB (P1C1, P1P2)
+-- egnos/YYYY/DDD/         -- EGNOS RINEX B (currently unavailable)
+-- ionex/YYYY/DDD/         -- IONEX TEC maps
+-- met/YYYY/DDD/STATION/   -- RINEX MET meteorological
+-- misc/                   -- EOP, leap-seconds
+-- nav/YYYY/DDD/           -- RINEX NAV broadcast ephemeris
+-- nwm/era5/YYYY/DDD/      -- ERA5 NetCDF (pressure + single levels)
+-- obs/YYYY/DDD/STATION/   -- RINEX OBS (.crx.gz Hatanaka compressed)
+-- sinex/YYYY/DDD/         -- IGS SINEX station coordinates
+-- sp3/YYYY/DDD/           -- SP3 precise ephemeris
+-- tides/                  -- Ocean loading BLQ (manual)
+-- tropo/YYYY/DDD/         -- IGS ZTD SINEX TRO
+-- vmf3/YYYY/              -- VMF3 mapping function (6-hourly)
```

Date subdirectories use **DOY** (Day of Year) convention: `YYYY/DDD/`
e.g. 2024-03-15 -> `2024/075/`

---

## Authentication

| Source | Auth required | Setup |
|---|---|---|
| NASA CDDIS | Yes -- NASA Earthdata | `~/.netrc` with `urs.earthdata.nasa.gov` |
| Copernicus CDS | Yes -- CDS API key | `~/.cdsapirc` with `url:` and `key:` |
| CODE Bern | No -- anonymous FTP | none |
| EUREF/EPN | No -- anonymous FTP | none |
| IGS files.igs.org | No | none |
| TU Wien VMF3 | No | none |
| IERS | No | none |

---

## Product status

### Working

| Product | Source | Format | Notes |
|---|---|---|---|
| ANTEX | files.igs.org | `.atx` | One-time download |
| NAV broadcast | cddis.nasa.gov | `.rnx.gz` | Mixed GNSS (GPS+GAL+GLO+BDS) |
| OBS RINEX3 | epncb.oma.be (EUREF/EPN) | `.crx.gz` | Hatanaka compressed |
| SP3 MGEX | ftp.aiub.unibe.ch (CODE) | `.SP3.gz` | Multi-GNSS precise orbits |
| CLK MGEX | ftp.aiub.unibe.ch (CODE) | `.CLK.gz` | 30s precise clocks |
| IONEX | ftp.aiub.unibe.ch (CODE) | `.ION.gz` | Hourly TEC maps |
| OSB BIAS | ftp.aiub.unibe.ch (CODE) | `.BIA.gz` | SINEX BIAS OSB |
| DCB | ftp.aiub.unibe.ch (CODE) | `.DCB.Z` | P1C1, P1P2 monthly |
| VMF3 | vmf.geo.tuwien.ac.at | plain text | 6-hourly, YYYYMMDD format |
| EOP | datacenter.iers.org | `.csv` | finals2000A |
| Leap seconds | hpiers.obspm.fr | plain text | |

### Not working / pending

| Product | Issue | Notes |
|---|---|---|
| SINEX coordinates | CDDIS OAuth cookie not handled in curl | Needs session cookie fix |
| TROPO IGS ZTD | Same CDDIS OAuth issue | Same fix as SINEX |
| MET RINEX | PECNY FTP denies access, EUREF has no MET | No working source found |
| EGNOS RINEX B | ESA GSSC SFTP port 2200 refused | No public source confirmed |
| ERA5 NWM | Not yet tested | Requires `~/.cdsapirc` setup |

### Manual steps required

| Item | Instructions |
|---|---|
| Ocean loading BLQ | Generate at http://holt.oso.chalmers.se/loading/ -- enter station name + coordinates, select FES2014b, save to `INPUT/GNSS/gnss_data/tides/STATION.blq` |

---

## File format notes

### OBS files (.crx.gz)
EUREF distributes RINEX 3 observations in **Hatanaka compressed** format (`.crx`).
Before parsing, decompress with:
```bash
gunzip file.crx.gz        # -> file.crx
crx2rnx file.crx          # -> file.rnx  (requires RNXCMP tools)
```
The loki_core RINEX parser will handle this transparently via zlib + Hatanaka decoder.

### VMF3 files
Plain text, no compression. Format: `VMF3_YYYYMMDD.HHH`
Four files per day: `.H00`, `.H06`, `.H12`, `.H18`
Contains ah, aw (hydrostatic/wet mapping coefficients) on 1x1 degree global grid.

---

## Known limitations and future plans

### CDDIS authentication
The script uses `~/.netrc` for NASA Earthdata but curl's OAuth redirect handling
requires a persistent cookie jar that is initialized via browser login.
Fix: implement a pre-flight cookie initialization step using curl OAuth flow.

### EGNOS
ESA GSSC previously offered SFTP access on port 2200 with anonymous login,
but this is currently inaccessible. Alternative sources investigated:
- EUREF/EPN: no SBAS data
- BKG Frankfurt: path not found
- CNES SERENAD: not accessible
Action: monitor ESA GSSC for restored access or alternative archive.

### ERA5 (NWM)
Copernicus CDS API v2 supports async job submission.
The script implements submit + poll + download workflow.
Requires `~/.cdsapirc` with valid API key from https://cds.climate.copernicus.eu/how-to-api
ERA5 variables planned: T, q, z (pressure levels) + sp, t2m, tcwv (single levels)
Region: Europe (10W-30E, 35N-70N), 4 times/day (00/06/12/18 UTC)

### MET RINEX
PECNY FTP (primary source for GOPE MET data) denies directory access.
No alternative source found for GOPE meteorological RINEX.
Workaround: use GPT3 empirical model (no download needed, coefficients embedded).

### Multi-station support
Currently station name mapping (`station_rinex3()`) covers only GOPE and TUBO.
Extend `station_rinex3()` with full EUREF/EPN station database for network analysis.

### Database integration
Downloaded raw files are inputs to the loki_core parser + DB pipeline:
```
Raw files -> loki_core parsers -> loki.db -> loki_gnss analysis
```
DB location: `INPUT/GNSS/loki.db` (planned, not yet implemented)
Parsers planned: rinexObsParser, rinexNavParser, sp3Parser, clkParser,
                 ionexParser, vmf3Parser, era5Parser, antexParser

---

## Security

- All downloads use HTTPS or anonymous FTP only
- No third-party mirrors
- `curl --fail` aborts on HTTP 4xx/5xx
- `--connect-timeout 15` prevents hanging on unreachable hosts
- Downloaded file size verified (empty files rejected)
- All paths sanitized (station code: `[A-Z0-9]{4}` only)
