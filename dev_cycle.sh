#!/bin/bash
#
# dev_cycle.sh - Esegue un ciclo completo di build, test e validazione.
# Progettato per essere usato da sviluppatori e agenti AI per garantire la qualità.
#
# USO:
#   bash dev_cycle.sh "Messaggio di commit descrittivo" [percorso/scena_di_test.obj]
#

# --- Validazione Argomenti ---
COMMIT_MSG=$1
SCENE_FILE=$2

if [ -z "$COMMIT_MSG" ]; then
    echo "ERRORE: Il messaggio di commit è obbligatorio."
    echo "Uso: $0 \"Messaggio di commit\" [percorso/scena.obj]"
    exit 1
fi

# Assicurarsi che l'ambiente sia configurato
if [ -z "$WAYVERB_LOG_DIR" ]; then
    echo "ATTENZIONE: Ambiente non configurato. Eseguo 'source setup_env.sh' con i default."
    source setup_env.sh
fi

echo ">>> Inizio Ciclo di Sviluppo per: \"$COMMIT_MSG\" <<<"

# --- FASE 1: Compilazione ---
echo
echo "--- [1/4] Compilazione Progetto ---"
# NOTA: Assumiamo che il progetto Xcode sia configurato per generare la 'build_id'
# Inserire qui il comando di build specifico (es. xcodebuild o cmake+make)
# Esempio:
xcodebuild -project wayverb.xcodeproj -scheme wayverb-app -configuration Release || {
    echo "ERRORE: Compilazione fallita."
    exit 1
}
echo "Compilazione completata."

# --- FASE 2: Test di Regressione ---
echo
echo "--- [2/4] Esecuzione Test di Regressione Headless ---"
# NOTA: Questo script deve esistere ed essere eseguibile.
# Deve restituire un codice di uscita diverso da 0 in caso di fallimento.
if [ -x "bin/apple_silicon_regression" ]; then
    if [ -n "$SCENE_FILE" ]; then
        bin/apple_silicon_regression --scene "$SCENE_FILE" || {
            echo "ERRORE: Test di regressione fallito sulla scena specifica."
            exit 1
        }
    else
        bin/apple_silicon_regression || {
            echo "ERRORE: Test di regressione standard fallito."
            exit 1
        }
    fi
    echo "Test di regressione superati."
else
    echo "ATTENZIONE: Script di regressione 'bin/apple_silicon_regression' non trovato. Salto il test."
fi

# --- FASE 3: Esecuzione e Raccolta Log ---
echo
echo "--- [3/4] Esecuzione Controllata con Logging ---"
LOG_FILENAME="phiverb-run-$(date +%Y%m%d-%H%M%S).log"
LOG_FILEPATH="$WAYVERB_LOG_DIR/$LOG_FILENAME"

if [ -x "tools/run_wayverb.sh" ]; then
    echo "Avvio di run_wayverb.sh, log in: $LOG_FILEPATH"
    # Esegue in background, attende e poi termina per un test controllato
tools/run_wayverb.sh > "$LOG_FILEPATH" 2>&1 &
    APP_PID=$!
    sleep 10 # Lascia l'app girare per 10 secondi
    
    if kill -0 $APP_PID > /dev/null 2>&1; then
        echo "Terminazione controllata dell'applicazione..."
        kill $APP_PID
    else
        echo "ATTENZIONE: L'applicazione è terminata inaspettatamente prima dei 10 secondi."
    fi
    wait $APP_PID
else
    echo "ERRORE: 'tools/run_wayverb.sh' non trovato. Impossibile eseguire."
    exit 1
fi

# --- FASE 4: Analisi del Log ---
echo
echo "--- [4/4] Analisi Automatica del Log ---"
# NOTA: Questo script deve esistere.
if [ -x "scripts/monitor_app_log.sh" ]; then
    SUMMARY_FILE="${LOG_FILEPATH}.summary"
scripts/monitor_app_log.sh "$LOG_FILEPATH" > "$SUMMARY_FILE"
    
echo "Riepilogo analisi log creato in: $SUMMARY_FILE"
    # Controlla se il summary contiene errori critici
    if grep -qE "FATAL|ERROR|CRITICAL" "$SUMMARY_FILE"; then
        echo "ERRORE: Rilevati problemi critici nel log:"
        cat "$SUMMARY_FILE"
        exit 1
    fi
    echo "Analisi del log non ha rilevato errori critici."
else
    echo "ATTENZIONE: 'scripts/monitor_app_log.sh' non trovato. Salto analisi log."
fi

echo
echo ">>> Ciclo di Sviluppo COMPLETATO con successo. Le modifiche sono pronte per il commit. <<<"
exit 0
