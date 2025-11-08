# Policy "Ask‑Then‑Act" (per TUTTI gli agenti)

**Quando devi CHIEDERE (fermarsi e aprire una richiesta):**
- Una decisione progettuale è ambigua (es. formula, convenzione, range ammesso).
- I test falliscono per dati esterni mancanti (materiali, parametri aria, BRDF/impedenze).
- Divergenze numeriche o prestazionali non spiegate (>15% tempo o errore energetico).
- Qualsiasi dubbio che, se “indovinato”, potrebbe rompere RT/QA.

**Cosa devi fornire nella richiesta:**
1) **Background**: 5–10 frasi su stato, obiettivo, cosa hai già provato.
2) **Domanda esatta**: 1–3 domande numerate, chiare e specifiche.
3) **Vincoli**: limiti, range, standard (ISO, ecc.), target prestazioni.
4) **Contesto tecnico**: commit, branch, percorsi file, snippet minimi, log, seed.
5) **Accettazione**: com’è PASS (criterio misurabile).

**Come si chiude:**
- Aggiorna il file in `.codex/NEED-INFO/…md` con sezione `## Resolution` **oppure** rimuovilo dopo aver applicato la soluzione.
- I merge sono sbloccati quando non ci sono file `NEED-INFO` attivi.
