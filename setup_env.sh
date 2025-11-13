#!/bin/bash
#
# setup_env.sh - Configura l'ambiente di sviluppo per phiverb (Wayverb).
#
# USO:
#   source setup_env.sh [opzioni]
#
# OPZIONI:
#   --metal         : Abilita il backend Metal (imposta WAYVERB_METAL=1).
#   --force-opencl  : Forza l'uso di OpenCL (default).
#   --no-viz        : Disabilita la visualizzazione per test headless.
#   --viz-decimate <N> : Imposta il fattore di decimazione della visualizzazione.
#

# --- Impostazioni di Default ---
export WAYVERB_LOG_DIR="${PWD}/build/logs"
export WAYVERB_METAL="force-opencl" # Default sicuro che garantisce la funzionalità
export WAYVERB_DISABLE_VIZ="0"
export WAYVERB_VIZ_DECIMATE="1"

# --- Parsing degli Argomenti ---
while [[ "$#" -gt 0 ]]; do
    case $1 in
        --metal) export WAYVERB_METAL="1"; echo "INFO: Backend Metal abilitato.";;
        --force-opencl) export WAYVERB_METAL="force-opencl"; echo "INFO: Backend OpenCL forzato.";;
        --no-viz) export WAYVERB_DISABLE_VIZ="1"; echo "INFO: Visualizzazione disabilitata.";;
        --viz-decimate)
            if [[ -n "$2" && "$2" != --* ]]; then
                export WAYVERB_VIZ_DECIMATE="$2"
                echo "INFO: Decimazione visualizzazione impostata a $2."
                shift
            else
                echo "ERRORE: --viz-decimate richiede un valore numerico." >&2
                return 1
            fi
            ;;
        *) echo "ERRORE: Parametro non riconosciuto: $1" >&2; return 1;;
    esac
    shift
done

# --- Verifica e Creazione Directory di Log ---
if [ ! -d "$WAYVERB_LOG_DIR" ]; then
    echo "INFO: Creazione directory di log in: $WAYVERB_LOG_DIR"
    mkdir -p "$WAYVERB_LOG_DIR"
fi

echo "--- Ambiente phiverb configurato ---"
echo "  Backend: ${WAYVERB_METAL}"
echo "  Log Dir: ${WAYVERB_LOG_DIR}"
echo "------------------------------------"