# Guida Operativa per l'Agente AI su phiverb

La tua missione è migliorare il progetto `phiverb` seguendo un processo rigoroso per garantire stabilità e qualità.

## Workflow Obbligatorio

Devi seguire questo ciclo per ogni singola modifica che apporti.

1.  **Configura l'Ambiente**: Prima di iniziare, esegui sempre `source setup_env.sh` con le opzioni necessarie (es. `--metal`).
    ```bash
    source setup_env.sh --metal
    ```

2.  **Modifica il Codice**: Applica le modifiche richieste al codice sorgente.

3.  **Valida il Tuo Lavoro**: Esegui lo script `dev_cycle.sh`. Questo script compilerà, testerà e validerà automaticamente il tuo lavoro.
    ```bash
    bash dev_cycle.sh "Il tuo messaggio di commit qui" "percorso/opzionale/alla/scena.obj"
    ```

4.  **Analizza il Risultato**:
    *   **Se lo script `dev_cycle.sh` fallisce**, il tuo lavoro non è valido. Analizza i log e l'output per trovare la causa del fallimento, correggi il problema e riesegui lo script fino al successo. **NON PROCEDERE CON UN COMMIT**.
    *   **Se lo script `dev_cycle.sh` ha successo**, le tue modifiche sono state validate. Ora puoi procedere con il commit.

## Risorse Documentali

Per comprendere il contesto e i dettagli tecnici, consulta questi documenti:

-   `docs/01_PROJECT_GOALS.md`: Gli obiettivi principali del progetto (cosa fare).
-   `docs/02_WORKFLOW_GUIDE.md`: Spiegazione dettagliata del `dev_cycle.sh` (come fare).
-   `docs/03_TECHNICAL_REFERENCE/`: Dettagli tecnici su build, logging e architettura.
-   `docs/ARCHEOLOGY/`: Contesto storico per capire il *perché* di certe scelte ed evitare di ripetere errori passati.