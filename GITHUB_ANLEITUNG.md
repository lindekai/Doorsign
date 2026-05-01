# GitHub-Anleitung für DoorSign
## Schritt-für-Schritt für dein erstes Git-Projekt

---

## Was du brauchst

- Einen kostenlosen GitHub-Account
- Git installiert auf deinem Computer
- Die 13 Projektdateien aus dem DoorSign-Ordner

---

## Teil 1 — Git installieren

### Windows
1. Gehe zu https://git-scm.com/download/win
2. Lade den Installer herunter und starte ihn
3. Alle Optionen auf Standard lassen → einfach immer „Next" klicken
4. Nach der Installation: Rechtsklick auf den Desktop → du siehst jetzt „Git Bash"

### macOS
1. Öffne das Terminal (Spotlight → „Terminal")
2. Tippe: `git --version`
3. Falls nicht installiert, erscheint automatisch ein Dialog zur Installation → bestätigen

### Linux (Ubuntu/Debian)
```bash
sudo apt update && sudo apt install git
```

---

## Teil 2 — GitHub-Account erstellen

1. Gehe zu https://github.com
2. Klicke auf **„Sign up"**
3. E-Mail-Adresse, Passwort und Benutzername eingeben
4. E-Mail bestätigen (Link im Postfach)
5. Fertig — du bist jetzt auf GitHub angemeldet

---

## Teil 3 — Git einmalig konfigurieren

Öffne das Terminal (macOS/Linux) oder **Git Bash** (Windows).

Trage deinen Namen und deine E-Mail ein — diese erscheinen später
bei jedem Commit als Autor:

```bash
git config --global user.name "Dein Name"
git config --global user.email "deine@email.de"
```

Prüfen ob es geklappt hat:
```bash
git config --list
```
Du solltest `user.name` und `user.email` in der Ausgabe sehen.

---

## Teil 4 — Repository auf GitHub anlegen

1. Melde dich auf https://github.com an
2. Klicke oben rechts auf das **„+"** Symbol → **„New repository"**
3. Fülle das Formular aus:

   | Feld | Wert |
   |---|---|
   | Repository name | `doorsign` |
   | Description | `Digitales Türschild für Konferenzräume — ESP32 + E-Ink` |
   | Visibility | **Private** (empfohlen, da WLAN-Passwörter im Code!) |
   | Initialize with README | **NICHT** ankreuzen |
   | Add .gitignore | **NICHT** ankreuzen |
   | Add license | nach Wunsch (z.B. MIT) |

4. Klicke auf **„Create repository"**

5. Du siehst jetzt eine leere Seite mit einer URL, z.B.:
   ```
   https://github.com/DEIN-USERNAME/doorsign
   ```
   Diese URL brauchst du gleich. Lass das Browser-Fenster offen.

---

## Teil 5 — .gitignore anlegen (wichtig!)

Bevor du die Dateien hochlädst, lege eine `.gitignore`-Datei an.
Diese verhindert, dass unnötige Dateien (z.B. temporäre Build-Dateien)
in Git landen.

Erstelle im DoorSign-Ordner eine neue Datei mit dem Namen `.gitignore`
(kein Dateiname davor, nur die Endung!) mit folgendem Inhalt:

```
# Arduino Build-Ordner
build/
*.bin
*.elf
*.map

# macOS
.DS_Store

# Windows
Thumbs.db
desktop.ini

# VSCode
.vscode/

# Arduino IDE temporäre Dateien
*.tmp

# WICHTIG: Lokale Konfiguration mit echten Passwörtern
# Falls du eine separate config_local.h nutzt, hier eintragen:
# config_local.h
```

**Hinweis zu Passwörtern:** Deine WLAN-Zugangsdaten stehen in `config.h`.
Da das Repository auf **Private** gesetzt ist, ist das für den internen
Gebrauch in Ordnung. Würdest du es jemals auf Public stellen, müsstest du
die Passwörter vorher entfernen.

---

## Teil 6 — Lokales Git-Repository anlegen und Dateien hochladen

Öffne Terminal / Git Bash und navigiere in deinen DoorSign-Ordner:

```bash
# Beispiel Windows (passe den Pfad an):
cd C:\Users\DeinName\Documents\DoorSign

# Beispiel macOS/Linux:
cd ~/Documents/DoorSign
```

Jetzt führe diese Befehle **der Reihe nach** aus:

### Schritt 1 — Git im Ordner initialisieren
```bash
git init
```
Ausgabe: `Initialized empty Git repository in .../DoorSign/.git/`

### Schritt 2 — Alle Dateien für den ersten Commit vorbereiten
```bash
git add .
```
(Der Punkt bedeutet: „alle Dateien in diesem Ordner")

### Schritt 3 — Prüfen was hinzugefügt wurde
```bash
git status
```
Du solltest alle 13 Dateien grün in der Liste sehen.

### Schritt 4 — Ersten Commit erstellen
```bash
git commit -m "Initial commit: DoorSign ESP32 E-Ink Projekt"
```
Ausgabe: eine Liste aller hinzugefügten Dateien.

### Schritt 5 — Hauptbranch auf „main" setzen
```bash
git branch -M main
```

### Schritt 6 — Mit GitHub-Repository verbinden
Ersetze `DEIN-USERNAME` mit deinem GitHub-Benutzernamen:
```bash
git remote add origin https://github.com/DEIN-USERNAME/doorsign.git
```

### Schritt 7 — Dateien hochladen (Push)
```bash
git push -u origin main
```
Git fragt nach deinem GitHub-Benutzernamen und Passwort.

> **Hinweis:** GitHub akzeptiert seit 2021 kein normales Passwort mehr
> beim Push. Du brauchst einen „Personal Access Token".
> Wie du ihn erstellst, steht im nächsten Abschnitt.

---

## Teil 7 — Personal Access Token erstellen (für den Push)

1. Gehe zu https://github.com/settings/tokens
2. Klicke auf **„Generate new token"** → **„Generate new token (classic)"**
3. Fülle aus:
   - **Note:** `doorsign-push`
   - **Expiration:** 90 days (oder „No expiration" für dauerhaft)
   - **Scopes:** Haken bei **`repo`** setzen (gibt vollen Zugriff auf deine Repos)
4. Klicke **„Generate token"**
5. Den Token **sofort kopieren** — er wird danach nicht mehr angezeigt!
   Er sieht so aus: `ghp_xxxxxxxxxxxxxxxxxxxxxxxxxxxx`

Beim nächsten `git push`:
- **Username:** dein GitHub-Benutzername
- **Password:** den kopierten Token einfügen (nicht dein GitHub-Passwort!)

---

## Teil 8 — Überprüfen ob alles hochgeladen wurde

1. Gehe zu `https://github.com/DEIN-USERNAME/doorsign`
2. Du solltest jetzt alle 13 Dateien sehen:
   - `DoorSign.ino`
   - `config.h`
   - `Logger.h`
   - `WifiManager.h` / `.cpp`
   - `TimeManager.h` / `.cpp`
   - `ImageManager.h` / `.cpp`
   - `DisplayManager.h` / `.cpp`
   - `StateMachine.h` / `.cpp`
   - `.gitignore`

---

## Teil 9 — Änderungen später hochladen

Wenn du später Code änderst, sind es immer dieselben 3 Schritte:

```bash
# 1. Geänderte Dateien zum Commit hinzufügen
git add .

# 2. Commit mit beschreibender Nachricht erstellen
git commit -m "Fix: WLAN-Reconnect verbessert"

# 3. Hochladen
git push
```

Nützliche Befehle zum Nachschlagen:

```bash
# Was hat sich geändert?
git status

# Alle bisherigen Commits anzeigen
git log --oneline

# Unterschiede einer Datei anzeigen
git diff config.h
```

---

## Teil 10 — Zweites Gerät (DoorSign-Beta) vorbereiten

Da beide Geräte dieselbe Firmware nutzen und nur `config.h` unterschiedlich
ist, gibt es zwei sinnvolle Wege:

### Option A — Branch pro Gerät (empfohlen)
```bash
# Branch für Gerät 2 anlegen
git checkout -b device/beta

# config.h öffnen und Gerät-2-Block einkommentieren
# ... Datei bearbeiten ...

git add config.h
git commit -m "Config: DoorSign-Beta (Konferenzraum Beta)"
git push -u origin device/beta
```

Zum Flashen von Gerät 1: `git checkout main`
Zum Flashen von Gerät 2: `git checkout device/beta`

### Option B — Zwei config-Dateien
Benenne die Dateien `config_alpha.h` und `config_beta.h`,
und kopiere vor dem Flashen die gewünschte nach `config.h`.

---

## Zusammenfassung der wichtigsten Befehle

```
git init              → Repository neu anlegen
git add .             → Alle Änderungen vormerken
git commit -m "..."   → Änderungen als Version speichern
git push              → Auf GitHub hochladen
git pull              → Von GitHub herunterladen
git status            → Was hat sich geändert?
git log --oneline     → Versionshistorie anzeigen
git checkout -b NAME  → Neuen Branch anlegen
```

---

*Bei Fragen oder Fehlermeldungen: die genaue Fehlermeldung aus dem Terminal
kopieren — damit lässt sich jedes Problem schnell lösen.*
