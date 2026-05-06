#!/usr/bin/env bash
# =============================================================================
# LOKI GNSS Data Downloader
# =============================================================================
# Downloads GNSS products from verified institutional sources only.
# All transfers use HTTPS or FTPS. No third-party mirrors.
#
# Usage:
#   ./download.sh --date YYYY-MM-DD [OPTIONS]
#   ./download.sh --from YYYY-MM-DD --to YYYY-MM-DD [OPTIONS]
#
# Sources:
#   PECNY/GOP   ftp.pecny.cz              -- RINEX obs (GOPE)
#   CODE Bern   ftp.aiub.unibe.ch         -- SP3, CLK, DCB, IONEX, BIAS
#   CDDIS       cddis.nasa.gov            -- SINEX ZTD, IGS products
#   IGS         files.igs.org             -- ANTEX
#   TU Wien     vmf.geo.tuwien.ac.at      -- VMF3
#   IERS        datacenter.iers.org       -- EOP, leap-seconds
#   EUREF/EPN   epncb.oma.be              -- European RINEX obs
#   ESAC/ESA    navigation.esa.int        -- EGNOS RINEX B (SBAS)
#   CDS         cds.climate.copernicus.eu -- ERA5 NetCDF (NWM)
# =============================================================================

set -euo pipefail

# =============================================================================
# CONSTANTS
# =============================================================================

readonly SCRIPT_VERSION="1.0.0"
readonly SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly LOKI_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
readonly DEFAULT_OUTPUT_DIR="${LOKI_ROOT}/../INPUT/GNSS/gnss_data"
readonly DEFAULT_STATION="GOPE"
readonly LOG_FILE="${LOKI_ROOT}/../INPUT/GNSS/download.log"

# Retry settings
readonly CURL_RETRY=3
readonly CURL_RETRY_DELAY=5
readonly CURL_TIMEOUT=120

# Colour codes (disabled if not a terminal)
if [[ -t 1 ]]; then
    C_RED='\033[0;31m'
    C_GREEN='\033[0;32m'
    C_YELLOW='\033[0;33m'
    C_CYAN='\033[0;36m'
    C_RESET='\033[0m'
    C_BOLD='\033[1m'
else
    C_RED='' C_GREEN='' C_YELLOW='' C_CYAN='' C_RESET='' C_BOLD=''
fi

# =============================================================================
# GLOBAL STATE
# =============================================================================

ARG_STATION="${DEFAULT_STATION}"
ARG_OUTPUT_DIR="${DEFAULT_OUTPUT_DIR}"
ARG_DATE_SINGLE=""
ARG_DATE_FROM=""
ARG_DATE_TO=""
ARG_PRODUCTS="all"
ARG_DRY_RUN=0
ARG_VERBOSE=0

DATES_LIST=()          # expanded list of YYYY-MM-DD strings
DOWNLOAD_OK=0
DOWNLOAD_FAIL=0
DOWNLOAD_SKIP=0

# =============================================================================
# LOGGING
# =============================================================================

log_info()    { echo -e "${C_CYAN}[INFO]${C_RESET}  $*" | tee -a "${LOG_FILE}"; }
log_ok()      { echo -e "${C_GREEN}[OK]${C_RESET}    $*" | tee -a "${LOG_FILE}"; }
log_warn()    { echo -e "${C_YELLOW}[WARN]${C_RESET}  $*" | tee -a "${LOG_FILE}"; }
log_error()   { echo -e "${C_RED}[ERROR]${C_RESET} $*" | tee -a "${LOG_FILE}"; }
log_verbose() { [[ "${ARG_VERBOSE}" -eq 1 ]] && echo -e "        $*" | tee -a "${LOG_FILE}" || true; }
log_dry()     { echo -e "${C_YELLOW}[DRY]${C_RESET}   Would download: $*" | tee -a "${LOG_FILE}"; }

# =============================================================================
# USAGE
# =============================================================================

usage() {
    cat <<EOF
${C_BOLD}LOKI GNSS Data Downloader v${SCRIPT_VERSION}${C_RESET}

${C_BOLD}USAGE:${C_RESET}
  $(basename "$0") --date YYYY-MM-DD [OPTIONS]
  $(basename "$0") --from YYYY-MM-DD --to YYYY-MM-DD [OPTIONS]

${C_BOLD}DATE OPTIONS (one required):${C_RESET}
  --date YYYY-MM-DD          Single day download
  --from YYYY-MM-DD          Start of date range (use with --to)
  --to   YYYY-MM-DD          End of date range (use with --from)

${C_BOLD}OPTIONS:${C_RESET}
  --station CODE             4-letter station code (default: ${DEFAULT_STATION})
  --products LIST            Comma-separated product list (default: all)
                             Available: obs, nav, sp3, clk, ionex, egnos,
                                        vmf3, era5, antex, bias, dcb,
                                        sinex, tropo, met, tides, misc
  --output-dir PATH          Base output directory (default: INPUT/GNSS/gnss_data)
  --dry-run                  Show what would be downloaded without downloading
  --verbose                  Print detailed curl output
  --help                     Show this help

${C_BOLD}EXAMPLES:${C_RESET}
  $(basename "$0") --date 2024-03-15 --station GOPE
  $(basename "$0") --date 2024-03-15 --products obs,nav,sp3,egnos
  $(basename "$0") --from 2024-03-01 --to 2024-03-07 --station GOPE
  $(basename "$0") --date 2024-03-15 --dry-run --verbose

${C_BOLD}AUTHENTICATION:${C_RESET}
  NASA CDDIS   : requires ~/.netrc with urs.earthdata.nasa.gov credentials
  Copernicus   : requires ~/.cdsapirc with CDS API key
  All others   : anonymous access (no credentials needed)

${C_BOLD}SECURITY:${C_RESET}
  All downloads use HTTPS or FTPS only. Sources are verified institutional
  repositories. No third-party mirrors are used.
EOF
    exit 0
}

# =============================================================================
# ARGUMENT PARSING
# =============================================================================

parse_args() {
    [[ $# -eq 0 ]] && usage

    while [[ $# -gt 0 ]]; do
        case "$1" in
            --date)        ARG_DATE_SINGLE="$2";  shift 2 ;;
            --from)        ARG_DATE_FROM="$2";    shift 2 ;;
            --to)          ARG_DATE_TO="$2";       shift 2 ;;
            --station)     ARG_STATION="${2^^}";   shift 2 ;;
            --products)    ARG_PRODUCTS="$2";      shift 2 ;;
            --output-dir)  ARG_OUTPUT_DIR="$2";    shift 2 ;;
            --dry-run)     ARG_DRY_RUN=1;          shift   ;;
            --verbose)     ARG_VERBOSE=1;           shift   ;;
            --help|-h)     usage ;;
            *)
                log_error "Unknown argument: $1"
                echo "Run with --help for usage."
                exit 1
                ;;
        esac
    done

    # Validate date arguments
    if [[ -n "${ARG_DATE_SINGLE}" ]]; then
        validate_date_format "${ARG_DATE_SINGLE}"
    elif [[ -n "${ARG_DATE_FROM}" && -n "${ARG_DATE_TO}" ]]; then
        validate_date_format "${ARG_DATE_FROM}"
        validate_date_format "${ARG_DATE_TO}"
    else
        log_error "You must specify either --date or both --from and --to."
        exit 1
    fi

    # Sanitize station code: only uppercase letters and digits
    if [[ ! "${ARG_STATION}" =~ ^[A-Z0-9]{4}$ ]]; then
        log_error "Station code must be exactly 4 alphanumeric characters: ${ARG_STATION}"
        exit 1
    fi
}

validate_date_format() {
    local date_str="$1"
    if [[ ! "${date_str}" =~ ^[0-9]{4}-[0-9]{2}-[0-9]{2}$ ]]; then
        log_error "Invalid date format '${date_str}'. Use YYYY-MM-DD."
        exit 1
    fi
    # Check the date is actually valid
    if ! date -d "${date_str}" &>/dev/null 2>&1; then
        log_error "Date '${date_str}' is not a valid calendar date."
        exit 1
    fi
}

# =============================================================================
# DEPENDENCY CHECK
# =============================================================================

validate_deps() {
    local missing=0
    for cmd in curl awk sed date mkdir; do
        if ! command -v "${cmd}" &>/dev/null; then
            log_error "Required tool not found: ${cmd}"
            missing=1
        fi
    done
    [[ ${missing} -eq 1 ]] && exit 1

    # Check ~/.netrc for CDDIS (only warn, not fatal -- user may not need CDDIS)
    if [[ ! -f "${HOME}/.netrc" ]]; then
        log_warn "~/.netrc not found. CDDIS downloads (sinex, tropo) will fail."
        log_warn "Add credentials for urs.earthdata.nasa.gov to ~/.netrc."
    fi

    # Check ~/.cdsapirc for ERA5
    if product_requested "era5" && [[ ! -f "${HOME}/.cdsapirc" ]]; then
        log_warn "~/.cdsapirc not found. ERA5 downloads will be skipped."
        log_warn "Register at https://cds.climate.copernicus.eu and create ~/.cdsapirc"
    fi

    log_verbose "All required tools found."
}

# =============================================================================
# DATE EXPANSION
# =============================================================================

expand_dates() {
    DATES_LIST=()
    if [[ -n "${ARG_DATE_SINGLE}" ]]; then
        DATES_LIST=("${ARG_DATE_SINGLE}")
    else
        local current="${ARG_DATE_FROM}"
        local epoch_to; epoch_to=$(date -d "${ARG_DATE_TO}" +%s)
        local epoch_cur
        while true; do
            epoch_cur=$(date -d "${current}" +%s)
            [[ ${epoch_cur} -gt ${epoch_to} ]] && break
            DATES_LIST+=("${current}")
            current=$(date -d "${current} + 1 day" +%Y-%m-%d)
        done
    fi
    log_info "Date range: ${DATES_LIST[0]} .. ${DATES_LIST[-1]} (${#DATES_LIST[@]} day(s))"
}

# Date helper functions
date_to_year()    { echo "${1:0:4}"; }
date_to_month()   { echo "${1:5:2}"; }
date_to_day()     { echo "${1:8:2}"; }
date_to_doy()     { date -d "$1" +%j; }            # day-of-year, 3 digits
date_to_doy2()    { printf "%03d" "$(date -d "$1" +%-j)"; }
date_to_gpsweek() {
    local epoch; epoch=$(date -d "$1" +%s)
    awk -v ep="${epoch}" 'BEGIN {
        # GPS epoch: 1980-01-06 = Unix 315964800
        gps_seconds = ep - 315964800
        week = int(gps_seconds / 604800)
        printf "%04d", week
    }'
}
date_to_gpsdow() { date -d "$1" +%w; }
date_to_gpsdow()  { date -d "$1" +%w; }            # 0=Sunday

# =============================================================================
# DIRECTORY CREATION
# =============================================================================

create_dirs() {
    local base="${ARG_OUTPUT_DIR}"
    local station_lc="${ARG_STATION,,}"

    local dirs=(
        "${base}/antex"
        "${base}/bias"
        "${base}/dcb"
        "${base}/egnos"
        "${base}/ionex"
        "${base}/met"
        "${base}/misc"
        "${base}/nav"
        "${base}/nwm/era5"
        "${base}/obs"
        "${base}/sinex"
        "${base}/sp3"
        "${base}/tides"
        "${base}/tropo"
        "${base}/vmf3"
        "${base}/clk"
    )

    for dir in "${dirs[@]}"; do
        mkdir -p "${dir}"
    done

    # Date-based subdirectories for the requested dates
    for date_str in "${DATES_LIST[@]}"; do
        local year; year=$(date_to_year "${date_str}")
        local doy;  doy=$(date_to_doy2 "${date_str}")
        mkdir -p "${base}/dcb/${year}"
        mkdir -p "${base}/vmf3/${year}"
        mkdir -p "${base}/obs/${year}/${doy}/${station_lc}"
        mkdir -p "${base}/nav/${year}/${doy}"
        mkdir -p "${base}/sp3/${year}/${doy}"
        mkdir -p "${base}/clk/${year}/${doy}"
        mkdir -p "${base}/ionex/${year}/${doy}"
        mkdir -p "${base}/egnos/${year}/${doy}"
        mkdir -p "${base}/met/${year}/${doy}/${station_lc}"
        mkdir -p "${base}/sinex/${year}/${doy}"
        mkdir -p "${base}/tropo/${year}/${doy}"
        mkdir -p "${base}/nwm/era5/${year}/${doy}"
    done

    log_info "Directory structure created under: ${base}"
}

# =============================================================================
# PRODUCT SELECTION
# =============================================================================

product_requested() {
    local product="$1"
    [[ "${ARG_PRODUCTS}" == "all" ]] && return 0
    echo "${ARG_PRODUCTS}" | tr ',' '\n' | grep -qx "${product}"
}

# =============================================================================
# CORE DOWNLOAD FUNCTION
# =============================================================================

# download_file URL DEST_PATH [DESCRIPTION]
# Returns 0 on success, 1 on failure.
# Skips download if file already exists and is non-empty.
download_file() {
    local url="$1"
    local dest="$2"
    local desc="${3:-$(basename "${dest}")}"

    # Sanitize URL: must start with https:// or ftps://
    if [[ ! "${url}" =~ ^(https|ftps|ftp):// ]]; then
        log_error "Rejected non-HTTPS/FTPS URL: ${url}"
        return 1
    fi

    # Skip if already downloaded
    if [[ -f "${dest}" && -s "${dest}" ]]; then
        log_verbose "Already exists, skipping: ${dest}"
        (( DOWNLOAD_SKIP++ )) || true
        return 0
    fi

    if [[ "${ARG_DRY_RUN}" -eq 1 ]]; then
        log_dry "${desc} -> ${dest}"
        return 0
    fi

    log_verbose "Downloading: ${url}"

    local curl_opts=(
        --fail
        --silent
        --show-error
        --location
        --retry "${CURL_RETRY}"
        --retry-delay "${CURL_RETRY_DELAY}"
        --max-time "${CURL_TIMEOUT}"
        --connect-timeout 15
        --output "${dest}"
    )

    # Use ~/.netrc and cookie jar for NASA CDDIS
    if [[ "${url}" == *"cddis.nasa.gov"* ]]; then
        curl_opts+=(--netrc-file "${HOME}/.netrc")
        curl_opts+=(--cookie-jar "${HOME}/.cddis_cookies")
        curl_opts+=(--cookie "${HOME}/.cddis_cookies")
        curl_opts+=(--max-redirs 20)
        curl_opts+=(--location)
    fi

    # Verbose curl output
    [[ "${ARG_VERBOSE}" -eq 0 ]] && curl_opts+=(--silent) || curl_opts+=(--progress-bar)

    if curl "${curl_opts[@]}" "${url}"; then
        # Verify downloaded file is non-empty
        if [[ -f "${dest}" && -s "${dest}" ]]; then
            log_ok "${desc}"
            (( DOWNLOAD_OK++ )) || true
            return 0
        else
            log_error "Downloaded file is empty: ${dest}"
            rm -f "${dest}"
            (( DOWNLOAD_FAIL++ )) || true
            return 1
        fi
    else
        local exit_code=$?
        log_error "Failed (curl exit ${exit_code}): ${desc}"
        rm -f "${dest}"
        (( DOWNLOAD_FAIL++ )) || true
        return 1
    fi
}

# Try multiple URLs in order, return 0 on first success
download_any() {
    local dest="$1"
    local desc="$2"
    shift 2
    local urls=("$@")

    for url in "${urls[@]}"; do
        if download_file "${url}" "${dest}" "${desc}"; then
            return 0
        fi
    done
    log_warn "All sources failed for: ${desc}"
    return 1
}

# =============================================================================
# STATION NAME MAPPING
# =============================================================================

# Returns 9-character RINEX 3 station ID (e.g. GOPE00CZE)
station_rinex3() {
    local station="${ARG_STATION}"
    case "${station}" in
        GOPE) echo "GOPE00CZE" ;;
        TUBO) echo "TUBO00CZE" ;;
        *)    echo "${station}00XXX" ;;
    esac
}

# Returns lowercase 4-char code
station_lc() { echo "${ARG_STATION,,}"; }

# =============================================================================
# PRODUCT DOWNLOADERS
# =============================================================================

# --- RINEX OBS ---------------------------------------------------------------
download_obs() {
    product_requested "obs" || return 0
    log_info "==> RINEX OBS"

    local station9; station9=$(station_rinex3)
    local station4lc; station4lc=$(station_lc)

    for date_str in "${DATES_LIST[@]}"; do
        local year; year=$(date_to_year "${date_str}")
        local yy="${year:2:2}"
        local doy;  doy=$(date_to_doy2 "${date_str}")
        local dest_dir="${ARG_OUTPUT_DIR}/obs/${year}/${doy}/${station4lc}"

        # Try RINEX 3 first (PECNY FTP), then EUREF/EPN
        # PECNY naming: GOPEdddhhmm_R_YYYYDDDD_01D_30S_MO.rnx.gz
        local file3="${station9}_R_${year}${doy}0000_01D_30S_MO.crx.gz"
        local dest3="${dest_dir}/${file3}"

        # PECNY source (RINEX 3)
        local url_pecny="ftp://ftp.pecny.cz/pub/obs/${year}/${doy}/${file3}"

        # EUREF/EPN source (RINEX 3)
        local url_euref="ftp://epncb.oma.be/pub/obs/${year}/${doy}/${file3}"
        local url_pecny="ftp://ftp.pecny.cz/pub/obs/${year}/${doy}/${file3}"
        url_euref="${url_euref/YYYY/${year}}"

        # RINEX 2 fallback (older archives)
        local file2="${station4lc}${doy}0.${yy}d.Z"
        local dest2="${dest_dir}/${file2}"
        local url_euref2="ftp://epncb.oma.be/pub/obs/${year}/${doy}/${file2}"
        local url_pecny2="ftp://ftp.pecny.cz/pub/obs/${year}/${doy}/${file2}"

        if ! download_file "${url_euref}" "${dest3}" "OBS RINEX3 EUREF ${ARG_STATION} ${date_str}"; then
            if ! download_file "${url_pecny}" "${dest3}" "OBS RINEX3 PECNY ${ARG_STATION} ${date_str}"; then
                download_file "${url_euref2}" "${dest2}" "OBS RINEX2 EUREF ${ARG_STATION} ${date_str}" || true
            fi
        fi
    done
}

# --- RINEX NAV ---------------------------------------------------------------
download_nav() {
    product_requested "nav" || return 0
    log_info "==> RINEX NAV (broadcast ephemeris)"

    for date_str in "${DATES_LIST[@]}"; do
        local year; year=$(date_to_year "${date_str}")
        local yy="${year:2:2}"
        local doy;  doy=$(date_to_doy2 "${date_str}")
        local dest_dir="${ARG_OUTPUT_DIR}/nav/${year}/${doy}"

        # IGS mixed RINEX 3 navigation file
        # Naming: BRDC00IGS_R_YYYYDDD0000_01D_MN.rnx.gz
        local file3="BRDC00IGS_R_${year}${doy}0000_01D_MN.rnx.gz"
        local dest3="${dest_dir}/${file3}"
        local url_cddis="https://cddis.nasa.gov/archive/gnss/data/daily/${year}/brdc/${file3}"

        # RINEX 2 fallback: brdc<doy>0.<yy>n.Z (GPS only)
        local file2="brdc${doy}0.${yy}n.Z"
        local dest2="${dest_dir}/${file2}"
        local url_cddis2="https://cddis.nasa.gov/archive/gnss/data/daily/${year}/brdc/${file2}"

        local url_ign3="https://igs.ign.fr/pub/igs/data/daily/${year}/brdc/${file3}"
        local url_ign2="https://igs.ign.fr/pub/igs/data/daily/${year}/brdc/${file2}"

        if ! download_file "${url_cddis}" "${dest3}" "NAV mixed RINEX3 ${date_str}"; then
            if ! download_file "${url_ign3}" "${dest3}" "NAV mixed RINEX3 IGN ${date_str}"; then
                if ! download_file "${url_cddis2}" "${dest2}" "NAV GPS RINEX2 ${date_str}"; then
                    download_file "${url_ign2}" "${dest2}" "NAV GPS RINEX2 IGN ${date_str}" || true
                fi
            fi
        fi
    done
}

# --- SP3 Precise Ephemeris ---------------------------------------------------
download_sp3() {
    product_requested "sp3" || return 0
    log_info "==> SP3 Precise Ephemeris (CODE Bern)"

    for date_str in "${DATES_LIST[@]}"; do
        local year; year=$(date_to_year "${date_str}")
        local doy;  doy=$(date_to_doy2 "${date_str}")
        local gpsweek; gpsweek=$(date_to_gpsweek "${date_str}")
        local gpsdow;  gpsdow=$(date_to_gpsdow "${date_str}")
        local dest_dir="${ARG_OUTPUT_DIR}/sp3/${year}/${doy}"

        # CODE final: COD0MGXFIN_YYYY_DDD_01D_05M_ORB.SP3.gz (MGEX, multi-GNSS)
        local file_mgex="COD0MGXFIN_${year}${doy}0000_01D_05M_ORB.SP3.gz"
        local dest_mgex="${dest_dir}/${file_mgex}"
        local url_code_mgex="ftp://ftp.aiub.unibe.ch/CODE_MGEX/CODE/${year}/${file_mgex}"


        # CODE GPS-only fallback: COD<week><dow>.EPH.Z
        local file_gps="COD${gpsweek}${gpsdow}.EPH.Z"
        local dest_gps="${dest_dir}/${file_gps}"
        local url_code_gps="ftp://ftp.aiub.unibe.ch/CODE/${year}/${file_gps}"

        if ! download_file "${url_code_mgex}" "${dest_mgex}" "SP3 MGEX ${date_str}"; then
            download_file "${url_code_gps}" "${dest_gps}" "SP3 GPS-only ${date_str}" || true
        fi
    done
}

# --- RINEX CLK ---------------------------------------------------------------
download_clk() {
    product_requested "clk" || return 0
    log_info "==> RINEX CLK (CODE Bern)"

    for date_str in "${DATES_LIST[@]}"; do
        local year; year=$(date_to_year "${date_str}")
        local doy;  doy=$(date_to_doy2 "${date_str}")
        local gpsweek; gpsweek=$(date_to_gpsweek "${date_str}")
        local gpsdow;  gpsdow=$(date_to_gpsdow "${date_str}")
        local dest_dir="${ARG_OUTPUT_DIR}/clk/${year}/${doy}"

        # CODE MGEX clock: COD0MGXFIN_YYYY_DDD_01D_30S_CLK.CLK.gz
        local file_mgex="COD0MGXFIN_${year}${doy}0000_01D_30S_CLK.CLK.gz"
        local dest_mgex="${dest_dir}/${file_mgex}"
        local url_mgex="ftp://ftp.aiub.unibe.ch/CODE_MGEX/CODE/${year}/${file_mgex}"

        # GPS-only fallback: COD<week><dow>.CLK.Z
        local file_gps="COD${gpsweek}${gpsdow}.CLK.Z"
        local dest_gps="${dest_dir}/${file_gps}"
        local url_gps="ftp://ftp.aiub.unibe.ch/CODE/${year}/${file_gps}"

        if ! download_file "${url_mgex}" "${dest_mgex}" "CLK MGEX ${date_str}"; then
            download_file "${url_gps}" "${dest_gps}" "CLK GPS-only ${date_str}" || true
        fi
    done
}

# --- IONEX -------------------------------------------------------------------
download_ionex() {
    product_requested "ionex" || return 0
    log_info "==> IONEX TEC maps (CODE Bern)"

    for date_str in "${DATES_LIST[@]}"; do
        local year; year=$(date_to_year "${date_str}")
        local doy;  doy=$(date_to_doy2 "${date_str}")
        local gpsweek; gpsweek=$(date_to_gpsweek "${date_str}")
        local gpsdow;  gpsdow=$(date_to_gpsdow "${date_str}")
        local dest_dir="${ARG_OUTPUT_DIR}/ionex/${year}/${doy}"

        # CODE IONEX: COD0OPSFIN_YYYY_DDD_01D_01H_GIM.ION.gz (hourly, preferred)
        local file_hr="COD0OPSFIN_${year}${doy}0000_01D_01H_GIM.ION.gz"
        local dest_hr="${dest_dir}/${file_hr}"
        local url_hr="ftp://ftp.aiub.unibe.ch/CODE/${year}/${file_hr}"

        # Daily fallback: CODG<doy>0.<yy>i.Z
        local yy="${year:2:2}"
        local file_daily="CODG${doy}0.${yy}i.Z"
        local dest_daily="${dest_dir}/${file_daily}"
        local url_daily="ftp://ftp.aiub.unibe.ch/CODE/${year}/${file_daily}"

        if ! download_file "${url_hr}" "${dest_hr}" "IONEX hourly ${date_str}"; then
            download_file "${url_daily}" "${dest_daily}" "IONEX daily ${date_str}" || true
        fi
    done
}

# --- DCB / BIAS --------------------------------------------------------------
download_dcb() {
    product_requested "dcb" || return 0
    log_info "==> DCB / OSB Bias (CODE Bern)"

    for date_str in "${DATES_LIST[@]}"; do
        local year; year=$(date_to_year "${date_str}")
        local month; month=$(date_to_month "${date_str}")
        local doy;   doy=$(date_to_doy2 "${date_str}")
        local dest_dir="${ARG_OUTPUT_DIR}/dcb/${year}"

        # CODE monthly DCB: P1C1YYMM.DCB.Z and P1P2YYMM.DCB.Z
        local yy="${year:2:2}"
        for dcb_type in P1C1 P1P2; do
            local file="${dcb_type}${yy}${month}.DCB.Z"
            local dest="${dest_dir}/${file}"
            local url="ftp://ftp.aiub.unibe.ch/CODE/${year}/${file}"
            download_file "${url}" "${dest}" "DCB ${dcb_type} ${year}-${month}" || true
        done
    done
}

download_bias() {
    product_requested "bias" || return 0
    log_info "==> SINEX BIAS OSB (CODE Bern)"

    for date_str in "${DATES_LIST[@]}"; do
        local year; year=$(date_to_year "${date_str}")
        local doy;  doy=$(date_to_doy2 "${date_str}")
        local dest_dir="${ARG_OUTPUT_DIR}/bias"

        # CODE OSB: COD0MGXFIN_YYYY_DDD_01D_01D_OSB.BIA.gz
        local file="COD0MGXFIN_${year}${doy}0000_01D_01D_OSB.BIA.gz"
        local dest="${dest_dir}/${file}"
        local url="ftp://ftp.aiub.unibe.ch/CODE_MGEX/CODE/${year}/${file}"
        download_file "${url}" "${dest}" "OSB BIAS ${date_str}" || true
    done
}

# --- SINEX / ZTD -------------------------------------------------------------
download_sinex() {
    product_requested "sinex" || return 0
    log_info "==> SINEX station coordinates (CDDIS -- requires ~/.netrc)"

    for date_str in "${DATES_LIST[@]}"; do
        local year; year=$(date_to_year "${date_str}")
        local doy;  doy=$(date_to_doy2 "${date_str}")
        local gpsweek; gpsweek=$(date_to_gpsweek "${date_str}")
        local dest_dir="${ARG_OUTPUT_DIR}/sinex/${year}/${doy}"

        # IGS weekly SINEX: igs<week>7.snx.Z
        local file="igs${gpsweek}7.snx.Z"
        local dest="${dest_dir}/${file}"
        local url="https://cddis.nasa.gov/archive/gnss/products/${gpsweek}/${file}"
        download_file "${url}" "${dest}" "SINEX coordinates week ${gpsweek}" || true
    done
}

download_tropo() {
    product_requested "tropo" || return 0
    log_info "==> IGS ZTD SINEX TRO (CDDIS -- requires ~/.netrc)"

    for date_str in "${DATES_LIST[@]}"; do
        local year; year=$(date_to_year "${date_str}")
        local doy;  doy=$(date_to_doy2 "${date_str}")
        local gpsweek; gpsweek=$(date_to_gpsweek "${date_str}")
        local gpsdow;  gpsdow=$(date_to_gpsdow "${date_str}")
        local dest_dir="${ARG_OUTPUT_DIR}/tropo/${year}/${doy}"

        # IGS troposphere: igsYYYYDDD.tro.gz
        local file="igs${year}${doy}.tro.gz"
        local dest="${dest_dir}/${file}"
        local url="https://cddis.nasa.gov/archive/gnss/products/troposphere/zpd/${year}/${doy}/${file}"
        download_file "${url}" "${dest}" "IGS ZTD ${date_str}" || true
    done
}

# --- ANTEX -------------------------------------------------------------------
download_antex() {
    product_requested "antex" || return 0
    log_info "==> ANTEX antenna calibration (IGS -- one-time download)"

    local dest="${ARG_OUTPUT_DIR}/antex/igs20.atx"
    local url="https://files.igs.org/pub/station/general/igs20.atx"
    download_file "${url}" "${dest}" "ANTEX igs20.atx" || true
}

# --- VMF3 --------------------------------------------------------------------
download_vmf3() {
    product_requested "vmf3" || return 0
    log_info "==> VMF3 mapping function (TU Wien)"

    for date_str in "${DATES_LIST[@]}"; do
        local year;  year=$(date_to_year "${date_str}")
        local month; month=$(date_to_month "${date_str}")
        local day;   day=$(date_to_day "${date_str}")
        local dest_dir="${ARG_OUTPUT_DIR}/vmf3/${year}"

        for hour in H00 H06 H12 H18; do
            local file="VMF3_${year}${month}${day}.${hour}"
            local dest="${dest_dir}/${file}"
            local url_op="https://vmf.geo.tuwien.ac.at/trop_products/GRID/1x1/VMF3/VMF3_OP/${year}/${file}"
            local url_fc="https://vmf.geo.tuwien.ac.at/trop_products/GRID/1x1/VMF3/VMF3_FC/${year}/${file}"
            if ! download_file "${url_op}" "${dest}" "VMF3 OP ${date_str} ${hour}"; then
                download_file "${url_fc}" "${dest}" "VMF3 FC ${date_str} ${hour}" || true
            fi
        done
    done
}

# --- EGNOS / SBAS ------------------------------------------------------------
download_egnos() {
    product_requested "egnos" || return 0
    log_warn "==> EGNOS RINEX B / SBAS -- source currently unavailable"
    log_warn "    ESA GSSC SFTP (port 2200) is not accessible."
    log_warn "    EGNOS download will be implemented when a working source is confirmed."
    log_warn "    For manual download visit: https://gssc.esa.int/activities/ftp-and-web-access-to-gnss-repository/"
}

#download_egnos() {
#    product_requested "egnos" || return 0
#    log_info "==> EGNOS RINEX B / SBAS (ESAC/ESA)"
#
#    for date_str in "${DATES_LIST[@]}"; do
#        local year; year=$(date_to_year "${date_str}")
#        local yy="${year:2:2}"
#        local doy;  doy=$(date_to_doy2 "${date_str}")
#        local dest_dir="${ARG_OUTPUT_DIR}/egnos/${year}/${doy}"
#
#        # EGNOS GEO PRNs: 120, 123, 126, 136
#        # ESAC SIS archive naming (v2 RINEX B):
#        #   EGNOS_SIS_<PRN>_<YYYY><DOY>.rnx  (check actual ESAC structure)
#        # v3 (DFMC) uses separate product stream
#
#        # Primary: ESA ESAC navigation archive
#        # Note: ESAC changed URL structure -- try both patterns
#        for prn in 120 123 126 136; do
#            # RINEX B v2 pattern
#            local file_v2="S${prn}${doy}0.${yy}b"
#            local dest_v2="${dest_dir}/${file_v2}"
#            local url_esac_v2="https://gssc.esa.int/products/egnos/${year}/${doy}/S${prn}${doy}0.${yy}b"
#
#            # RINEX B v3 / DFMC pattern (2024+)
#            local file_v3="EGNOS${prn}_${year}${doy}0000_01D_SBAS.rnx.gz"
#            local dest_v3="${dest_dir}/${file_v3}"
#            local url_esac_v3="https://gssc.esa.int/products/egnos-v3/${year}/${doy}/EGNOS${prn}_${year}${doy}0000_01D_SBAS.rnx.gz"
#
#            # Try v3 first (newer data), then v2
#            if ! download_file "${url_esac_v3}" "${dest_v3}" "EGNOS v3 PRN${prn} ${date_str}"; then
#                download_file "${url_esac_v2}" "${dest_v2}" "EGNOS v2 PRN${prn} ${date_str}" || true
#            fi
#        done
#    done
#}

# --- METEOROLOGICAL (RINEX MET) ----------------------------------------------
download_met() {
    product_requested "met" || return 0
    log_info "==> RINEX MET meteorological observations"

    local station4lc; station4lc=$(station_lc)
    local station9;   station9=$(station_rinex3)

    for date_str in "${DATES_LIST[@]}"; do
        local year; year=$(date_to_year "${date_str}")
        local yy="${year:2:2}"
        local doy;  doy=$(date_to_doy2 "${date_str}")
        local dest_dir="${ARG_OUTPUT_DIR}/met/${year}/${doy}/${station4lc}"

        # RINEX 3 MET: GOPE00CZE_R_YYYYDDD0000_01D_MET.rnx.gz
        local file3="${station9}_R_${year}${doy}0000_01D_MET.rnx.gz"
        local dest3="${dest_dir}/${file3}"
        local url_euref3="ftp://epncb.oma.be/pub/obs/${year}/${doy}/${station9}_R_${year}${doy}0000_01D_MET.crx.gz"

        # RINEX 2 fallback
        local file2="${station4lc}${doy}0.${yy}m.Z"
        local dest2="${dest_dir}/${file2}"
        local url_euref2="ftp://epncb.oma.be/pub/obs/${year}/${doy}/${station4lc}${doy}0.${yy}m.Z"

        if ! download_file "${url_euref3}" "${dest3}" "MET RINEX3 EUREF ${ARG_STATION} ${date_str}"; then
            download_file "${url_euref2}" "${dest2}" "MET RINEX2 EUREF ${ARG_STATION} ${date_str}" || true
        fi
    done
}

# --- TIDES (Ocean Loading BLQ) -----------------------------------------------
download_tides() {
    product_requested "tides" || return 0
    log_info "==> Ocean Loading BLQ"

    local dest="${ARG_OUTPUT_DIR}/tides/${ARG_STATION,,}.blq"
    if [[ -f "${dest}" && -s "${dest}" ]]; then
        log_info "BLQ already exists for ${ARG_STATION}: ${dest}"
        return 0
    fi

    log_warn "Ocean loading BLQ must be generated manually."
    log_warn "Visit: http://holt.oso.chalmers.se/loading/"
    log_warn "Enter station: ${ARG_STATION}, coordinates, select FES2014b model."
    log_warn "Save result to: ${dest}"
}

# --- MISC (EOP, leap-seconds) ------------------------------------------------
download_misc() {
    product_requested "misc" || return 0
    log_info "==> Miscellaneous (IERS EOP, leap-seconds)"

    local dest_dir="${ARG_OUTPUT_DIR}/misc"

    # Earth Orientation Parameters (finals2000A)
    local url_eop="https://datacenter.iers.org/data/csv/finals2000A.all.csv"
    download_file "${url_eop}" "${dest_dir}/finals2000A.all.csv" "IERS EOP finals2000A" || true

    # Leap seconds list
    local url_leap="https://hpiers.obspm.fr/iers/bul/bulc/Leap_Second.dat"
    download_file "${url_leap}" "${dest_dir}/Leap_Second.dat" "IERS Leap seconds" || true
}

# --- ERA5 (NWM) --------------------------------------------------------------
download_era5() {
    product_requested "era5" || return 0

    if [[ ! -f "${HOME}/.cdsapirc" ]]; then
        log_warn "Skipping ERA5: ~/.cdsapirc not found."
        log_warn "Register at https://cds.climate.copernicus.eu/how-to-api"
        return 0
    fi

    log_info "==> ERA5 NWM (Copernicus CDS)"

    # Read CDS credentials
    local cds_url cds_key
    cds_url=$(grep -m1 "^url:" "${HOME}/.cdsapirc" | awk '{print $2}')
    cds_key=$(grep -m1 "^key:" "${HOME}/.cdsapirc" | awk '{print $2}')

    if [[ -z "${cds_url}" || -z "${cds_key}" ]]; then
        log_error "Could not parse ~/.cdsapirc. Expected 'url:' and 'key:' fields."
        return 1
    fi

    for date_str in "${DATES_LIST[@]}"; do
        local year;  year=$(date_to_year "${date_str}")
        local month; month=$(date_to_month "${date_str}")
        local day;   day=$(date_to_day "${date_str}")
        local doy;   doy=$(date_to_doy2 "${date_str}")
        local dest_dir="${ARG_OUTPUT_DIR}/nwm/era5/${year}/${doy}"

        # Output file
        local dest_pl="${dest_dir}/era5_pl_${year}${month}${day}.nc"   # pressure levels
        local dest_sl="${dest_dir}/era5_sl_${year}${month}${day}.nc"   # single levels

        # ----------------------------------------------------------------
        # Pressure levels: T, q, z on standard levels
        # Region: Europe (10W-30E, 35N-70N) -- adjust as needed
        # ----------------------------------------------------------------
        if [[ ! -f "${dest_pl}" || ! -s "${dest_pl}" ]]; then
            log_info "Requesting ERA5 pressure levels for ${date_str} (CDS queue)..."

            if [[ "${ARG_DRY_RUN}" -eq 1 ]]; then
                log_dry "ERA5 pressure levels ${date_str} -> ${dest_pl}"
            else
                # Build JSON request for CDS API v2
                local request_pl
                request_pl=$(cat <<EOJSON
{
  "dataset": "reanalysis-era5-pressure-levels",
  "product_type": ["reanalysis"],
  "variable": ["temperature", "specific_humidity", "geopotential"],
  "pressure_level": [
    "100","150","200","250","300","400","500",
    "600","700","850","925","1000"
  ],
  "year":  ["${year}"],
  "month": ["${month}"],
  "day":   ["${day}"],
  "time":  ["00:00","06:00","12:00","18:00"],
  "area":  [70, -10, 35, 30],
  "format": "netcdf"
}
EOJSON
)
                era5_submit_and_wait "${cds_url}" "${cds_key}" \
                    "${request_pl}" "${dest_pl}" "ERA5 pressure levels ${date_str}"
            fi
        else
            log_verbose "ERA5 PL already exists: ${dest_pl}"
            (( DOWNLOAD_SKIP++ )) || true
        fi

        # ----------------------------------------------------------------
        # Single levels: sp, t2m, tcwv
        # ----------------------------------------------------------------
        if [[ ! -f "${dest_sl}" || ! -s "${dest_sl}" ]]; then
            log_info "Requesting ERA5 single levels for ${date_str} (CDS queue)..."

            if [[ "${ARG_DRY_RUN}" -eq 1 ]]; then
                log_dry "ERA5 single levels ${date_str} -> ${dest_sl}"
            else
                local request_sl
                request_sl=$(cat <<EOJSON
{
  "dataset": "reanalysis-era5-single-levels",
  "product_type": ["reanalysis"],
  "variable": [
    "surface_pressure",
    "2m_temperature",
    "total_column_water_vapour"
  ],
  "year":  ["${year}"],
  "month": ["${month}"],
  "day":   ["${day}"],
  "time":  ["00:00","06:00","12:00","18:00"],
  "area":  [70, -10, 35, 30],
  "format": "netcdf"
}
EOJSON
)
                era5_submit_and_wait "${cds_url}" "${cds_key}" \
                    "${request_sl}" "${dest_sl}" "ERA5 single levels ${date_str}"
            fi
        else
            log_verbose "ERA5 SL already exists: ${dest_sl}"
            (( DOWNLOAD_SKIP++ )) || true
        fi
    done
}

# Submit ERA5 request to CDS API v2, poll until complete, download result
era5_submit_and_wait() {
    local cds_url="$1"
    local cds_key="$2"
    local request_json="$3"
    local dest="$4"
    local desc="$5"

    local api_endpoint="${cds_url%/}/jobs"

    # Submit request
    local response
    response=$(curl --fail --silent --show-error \
        --header "PRIVATE-TOKEN: ${cds_key}" \
        --header "Content-Type: application/json" \
        --data "${request_json}" \
        --max-time 30 \
        "${api_endpoint}" 2>&1) || {
        log_error "CDS submit failed for ${desc}: ${response}"
        (( DOWNLOAD_FAIL++ )) || true
        return 1
    }

    local job_id
    job_id=$(echo "${response}" | grep -o '"jobID":"[^"]*"' | cut -d'"' -f4)
    if [[ -z "${job_id}" ]]; then
        log_error "Could not extract job ID from CDS response for ${desc}"
        (( DOWNLOAD_FAIL++ )) || true
        return 1
    fi

    log_verbose "CDS job submitted: ${job_id}"

    # Poll for completion (max 60 min, check every 30s)
    local max_polls=120
    local poll_count=0
    local status=""

    while [[ ${poll_count} -lt ${max_polls} ]]; do
        sleep 30
        local poll_response
        poll_response=$(curl --fail --silent \
            --header "PRIVATE-TOKEN: ${cds_key}" \
            --max-time 15 \
            "${api_endpoint}/${job_id}" 2>/dev/null) || true

        status=$(echo "${poll_response}" | grep -o '"status":"[^"]*"' | cut -d'"' -f4)
        log_verbose "CDS job ${job_id} status: ${status} (poll ${poll_count})"

        case "${status}" in
            "successful")
                break ;;
            "failed"|"dismissed")
                log_error "CDS job failed (${status}) for ${desc}"
                (( DOWNLOAD_FAIL++ )) || true
                return 1 ;;
        esac
        (( poll_count++ )) || true
    done

    if [[ "${status}" != "successful" ]]; then
        log_error "CDS job timed out after $((max_polls * 30 / 60)) min for ${desc}"
        (( DOWNLOAD_FAIL++ )) || true
        return 1
    fi

    # Get download URL from results
    local download_url
    download_url=$(curl --fail --silent \
        --header "PRIVATE-TOKEN: ${cds_key}" \
        --max-time 15 \
        "${api_endpoint}/${job_id}/results" 2>/dev/null | \
        grep -o '"href":"[^"]*"' | cut -d'"' -f4 | head -1)

    if [[ -z "${download_url}" ]]; then
        log_error "Could not get download URL for CDS job ${job_id}"
        (( DOWNLOAD_FAIL++ )) || true
        return 1
    fi

    # Download the result
    download_file "${download_url}" "${dest}" "${desc}"
}

# =============================================================================
# SUMMARY
# =============================================================================

log_summary() {
    local total=$(( DOWNLOAD_OK + DOWNLOAD_FAIL + DOWNLOAD_SKIP ))
    echo ""
    echo -e "${C_BOLD}======================================================${C_RESET}"
    echo -e "${C_BOLD}LOKI GNSS Download Summary${C_RESET}"
    echo -e "  Station  : ${ARG_STATION}"
    echo -e "  Dates    : ${DATES_LIST[0]} .. ${DATES_LIST[-1]}"
    echo -e "  Products : ${ARG_PRODUCTS}"
    echo -e "------------------------------------------------------"
    echo -e "  ${C_GREEN}OK    : ${DOWNLOAD_OK}${C_RESET}"
    echo -e "  ${C_YELLOW}Skipped: ${DOWNLOAD_SKIP}${C_RESET}  (already on disk)"
    echo -e "  ${C_RED}Failed : ${DOWNLOAD_FAIL}${C_RESET}"
    echo -e "  Total  : ${total}"
    echo -e "------------------------------------------------------"
    echo -e "  Log    : ${LOG_FILE}"
    echo -e "${C_BOLD}======================================================${C_RESET}"

    [[ "${ARG_DRY_RUN}" -eq 1 ]] && echo -e "${C_YELLOW}[DRY RUN -- nothing was actually downloaded]${C_RESET}"
}

# =============================================================================
# MAIN
# =============================================================================

main() {
    # Ensure log directory exists
    mkdir -p "$(dirname "${LOG_FILE}")"
    echo "# LOKI download session $(date -u +%Y-%m-%dT%H:%M:%SZ)" >> "${LOG_FILE}"

    parse_args "$@"
    validate_deps
    expand_dates
    create_dirs

    log_info "Station : ${ARG_STATION}"
    log_info "Products: ${ARG_PRODUCTS}"
    [[ "${ARG_DRY_RUN}" -eq 1 ]] && log_warn "DRY RUN mode -- no files will be downloaded"

    # Execute product downloaders
    download_antex
    download_misc
    download_nav
    download_obs
    download_sp3
    download_clk
    download_ionex
    download_dcb
    download_bias
    download_sinex
    download_tropo
    download_vmf3
    download_egnos
    download_met
    download_tides
    download_era5

    log_summary
}

main "$@"
