# WS2812 RGB Sequenzer

Ein webbasierter RGB-LED-Sequenzer fuer **Wemos D1 mini / ESP8266** und **WS2812B** LED-Streifen. Sequenzen werden ueber eine Weboberflaeche erstellt, live auf dem LED-Streifen getestet, im Wemos gespeichert und laufen danach eigenstaendig ohne Browser oder externe Steuerung weiter.

## Funktionen

- Grafische Weboberflaeche direkt vom Wemos D1 mini
- Anzahl der LEDs einstellbar, begrenzt auf maximal 50 LEDs fuer stabileres WLAN auf dem ESP8266
- Mehrere Sequenzen als Playlist
- Sequenzen koennen aktiviert oder deaktiviert werden
- Schritte bearbeiten, kopieren, einfuegen, verschieben und loeschen
- Schritte koennen benannt und umbenannt werden
- Optional zufaellige Schrittdauer von 0 bis zu einer einstellbaren Obergrenze in 0,1-Sekunden-Schritten
- Pro Schritt beliebige LEDs mit individuellen Farben
- Effektgruppen pro Schritt, z. B. Feuer, Gewitter, Regenbogen, Schweißerlicht und Fotografen-Blitz
- Effektgeschwindigkeit pro Effekt auf einer Skala von 0 bis 100: 0 ist sehr langsam, 50 Standard, 100 sehr schnell
- Effektfarbe fuer einfarbige Effekte
- Live-Vorschau waehrend der Bearbeitung: im Stop-Modus wird jeder geladene oder geaenderte Schritt direkt auf den echten LEDs angezeigt
- Dauerhafte Speicherung im Flash per LittleFS
- Automatischer Start der gespeicherten Playlist nach Stromausfall oder Neustart
- Demo_Sequenz wird beim Start automatisch ueberschrieben: LED 1 zeigt Weiss, Grundfarben und danach alle Effekte je 7 Sekunden, dazwischen jeweils 2 Sekunden Pause
- WLAN-Name und WLAN-Kennwort in der Bedienoberflaeche aenderbar
- Lokale Browser-Simulation ohne Hardware

## Benoetigte Hardware

Pflicht:

- 1x Wemos D1 mini oder kompatibles ESP8266-Board
- 1x WS2812B LED-Streifen mit 5 V
- 1x 12-V-Netzteil als Eingang, falls dein Projekt mit 12 V versorgt wird
- 1x Stepdown-Wandler 12 V auf 5 V, mindestens 3 A fuer 20 LEDs plus Wemos
- 1x 330-Ohm-Widerstand in Reihe zur Datenleitung
- Gemeinsame Masseverbindung zwischen Stepdown, Wemos und LED-Streifen
- Kabel, Klemmen oder Loetmaterial

Empfohlen:

- 1x 1000-uF-Elko zwischen 5 V und GND am Anfang des LED-Streifens
- 1x 74AHCT125 oder 74HCT125 als Pegelwandler von 3,3 V auf 5 V
- 1x 100-nF-Kondensator nahe am Pegelwandler zwischen VCC und GND
- Sicherung passend zum maximalen Strom des LED-Streifens

## Verdrahtung

Direkte Minimalverdrahtung:

```text
12 V Netzteil +  -> Stepdown IN+
12 V Netzteil -  -> Stepdown IN-

Stepdown OUT+ 5V -> Wemos 5V
Stepdown OUT+ 5V -> WS2812B 5V

Stepdown OUT- GND -> Wemos G
Stepdown OUT- GND -> WS2812B GND

Wemos D4 -> 330 Ohm -> WS2812B DIN
```

Sauberer mit Pegelwandler:

```text
Wemos D4 -> 74AHCT125 IN
74AHCT125 OUT -> 330 Ohm -> WS2812B DIN
74AHCT125 VCC -> 5V
74AHCT125 GND -> GND
74AHCT125 /OE -> GND
```

Siehe Schaltplan:

```text
docs/wemos_d1_mini_ws2812b_wiring.svg
```

## Software

Arduino-Bibliotheken:

```text
Adafruit NeoPixel
LittleFS ist im ESP8266-Arduino-Core enthalten
```

Arduino-Board:

```text
LOLIN(WEMOS) D1 R2 & mini
```

Sketch:

```text
arduino/Wemos_D1_Mini_RGB_Sequencer/Wemos_D1_Mini_RGB_Sequencer.ino
```

Eigene Variante mit `Fade-In` und `Fade-Out`, ohne den Hauptsketch zu ersetzen:

```text
arduino/Wemos_D1_Mini_RGB_Sequencer_Fade/Wemos_D1_Mini_RGB_Sequencer_Fade.ino
```

In dieser Variante dauern `Fade-In` und `Fade-Out` jeweils genau so lange wie die
eingestellte Schrittdauer.

Standard-Datenpin:

```cpp
const uint8_t DATA_PIN = D4;
```

## Nutzung

1. ESP8266-Boardpaket in der Arduino IDE installieren.
2. `Adafruit NeoPixel` installieren.
3. Sketch `arduino/Wemos_D1_Mini_RGB_Sequencer/Wemos_D1_Mini_RGB_Sequencer.ino` oeffnen.
4. Board `LOLIN(WEMOS) D1 R2 & mini` auswaehlen.
5. Sketch hochladen.
6. Mit WLAN `RGB-Sequencer` verbinden, Passwort `12345678`.
7. Browser oeffnen: bevorzugt `http://rgb-sequencer.local`.
8. Falls das nicht klappt: `http://192.168.178.99`.

Der Wemos beantwortet ausserdem DNS-Anfragen im eigenen WLAN und zeigt fuer unbekannte
Adressen die Bedienoberflaeche. Auf vielen Smartphones erscheint dadurch nach dem
Verbinden automatisch ein Anmelde-/Portal-Fenster. Auf Windows kann das wegen fester
IP-/DNS-Einstellungen ausbleiben; dann die Adresse oben manuell oeffnen.

Der Name und das Kennwort des eigenen WLANs koennen in der Bedienoberflaeche unter
`Name / WLAN` und `Kennwort` geaendert werden. Das Kennwort muss mindestens 8 Zeichen
lang sein. Nach `Alle Sequenzen speichern` startet der Access Point kurz neu und
erscheint danach mit den neuen Zugangsdaten.
9. LED-Anzahl einstellen, Sequenzen und Schritte erstellen.
10. `Alle Sequenzen speichern` druecken.

Nach dem Speichern laeuft die Playlist eigenstaendig weiter. Nach einem Neustart oder
Stromausfall startet der Wemos die gespeicherte Playlist automatisch wieder.

`Stop` haelt die automatische Playlist an und schaltet in den Bearbeitungsmodus. In
diesem Zustand wird der aktuell ausgewaehlte Schritt dauerhaft live auf den echten
LEDs angezeigt. Wenn du eine LED-Farbe, einen Effekt oder den Schritt wechselst, wird
das sofort am LED-Streifen sichtbar. Mit `Abspielen` startet die normale Playlist
wieder ab Anfang.

## Simulation

Die Simulation laeuft ohne Wemos direkt im Browser:

```text
simulation/rgb_strip_simulation.html
```

Sie dient zum Ausprobieren der Bedienoberflaeche, Sequenzen, Farben und Effekte.

## Effekte

Enthalten sind:

- Feuerflackern
- Gewitter
- Regenbogen
- Schweißerlicht
- Fotografen-Blitz
- Polizei-Blitzer
- Funkeln
- Komet
- Theater-Lauflicht
- Pulsieren
- Atmen
- Lava
- Zufall An/Aus
- Zufall Farbe
- Zufall An/Aus + Farbe
- Meteor
- Sternfunkeln
- Kerze
- Sonnenaufgang
- Sonnenuntergang
- Scanner
- Konfetti
- Aurora
- Giftgruen
- Herzschlag

## Stromversorgung

WS2812B-LEDs koennen bei voller Helligkeit und Weiss bis zu ca. 60 mA pro LED benoetigen.

```text
maximaler Strom = LED-Anzahl * ca. 60 mA + Wemos-Reserve
```

Beispiel fuer 20 LEDs:

```text
20 * 0,06 A = 1,2 A
plus Wemos ca. 0,1-0,3 A
```

Ein Stepdown mit **5 V / 3 A** ist dafuer passend. Vor dem Anschliessen immer mit dem Multimeter kontrollieren, dass wirklich etwa **5,0 V** am Ausgang anliegen.
