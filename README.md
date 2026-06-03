# WS2812 RGB Sequenzer

Ein ESP32-basierter RGB-LED-Sequenzer fuer adressierbare LED-Streifen wie WS2812, WS2812B, SK6812 oder WS2815. Sequenzen werden ueber eine Weboberflaeche erstellt, live auf dem LED-Streifen getestet, im ESP32 gespeichert und laufen danach eigenstaendig ohne Browser oder externe Steuerung weiter.

## Funktionen

- Grafische Weboberflaeche direkt vom ESP32
- Anzahl der LEDs einstellbar
- Mehrere Sequenzen als Playlist
- Schritte benennen, bearbeiten, kopieren, einfuegen, verschieben und loeschen
- Pro Schritt beliebige LEDs mit individuellen Farben
- Effektgruppen pro Schritt, z. B. Feuer, Gewitter, Regenbogen, Schweißerlicht und Fotografen-Blitz
- Effektgeschwindigkeit pro Effekt
- Effektfarbe fuer einfarbige Effekte
- Live-Vorschau waehrend der Bearbeitung
- Lokale Browser-Simulation ohne ESP32

## Benoetigte Hardware

Pflicht:

- 1x ESP32 Dev Board, z. B. ESP32 DevKit V1 oder ESP32-WROOM-32
- 1x adressierbarer RGB-LED-Streifen
  - 5 V: WS2812B/SK6812
  - 12 V: WS2815 empfohlen
- Passendes Netzteil fuer den LED-Streifen
- Step-Down-Wandler auf 5 V fuer den ESP32, falls das Projekt aus 12 V versorgt wird
- 1x 330-Ohm-Widerstand in Reihe zur Datenleitung
- Gemeinsame Masseverbindung zwischen Netzteil, LED-Streifen und ESP32
- Kabel, Klemmen oder Loetmaterial

Empfohlen:

- 1x 1000-uF-Elko zwischen Plus und GND am Anfang des LED-Streifens
- Logikpegelwandler 3,3 V auf 5 V fuer die Datenleitung, besonders bei langen Leitungen oder 5-V-Streifen
- Sicherung passend zum maximalen Strom des LED-Streifens
- Einspeisung an mehreren Stellen bei laengeren Streifen

## Verdrahtung

Typischer Aufbau:

```text
Netzteil +  --------------------  +V LED-Streifen
Netzteil GND -------------------  GND LED-Streifen
Netzteil GND -------------------  ESP32 GND

ESP32 GPIO 16 -- 330 Ohm -------  DIN LED-Streifen

12 V Netzteil -> Step-Down 5 V -- ESP32 5V/VIN
```

Wichtig: Den ESP32 nicht direkt mit 12 V versorgen. Bei 12-V-LED-Streifen wie WS2815 bekommt nur der Streifen 12 V. Der ESP32 braucht 5 V ueber USB oder einen Step-Down-Wandler.

Siehe auch: `docs/ws2812_ws2815_wiring.svg`

## Software

Arduino-Bibliothek:

```text
Adafruit NeoPixel
```

Sketch:

```text
arduino/ESP32_RGB_Strip_Sequencer/ESP32_RGB_Strip_Sequencer.ino
```

Standard-Datenpin im Sketch:

```cpp
const uint8_t DATA_PIN = 16;
```

## Nutzung

1. `Adafruit NeoPixel` in der Arduino IDE installieren.
2. Sketch `arduino/ESP32_RGB_Strip_Sequencer/ESP32_RGB_Strip_Sequencer.ino` oeffnen.
3. ESP32-Board auswaehlen und Sketch hochladen.
4. Mit WLAN `RGB-Sequencer` verbinden, Passwort `12345678`.
5. Browser oeffnen: `http://192.168.4.1`.
6. LED-Anzahl einstellen, Sequenzen und Schritte erstellen.
7. `Alle Sequenzen speichern` druecken.

Nach dem Speichern laeuft die Playlist eigenstaendig weiter.

## Simulation

Die Simulation laeuft ohne ESP32 direkt im Browser:

```text
simulation/rgb_strip_simulation.html
```

Sie dient zum Ausprobieren der Bedienoberflaeche, Sequenzen, Farben und Effekte.

## Effekte

Enthalten sind unter anderem:

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
- Ozean
- Wald
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

## Hinweise zur Stromversorgung

RGB-LED-Streifen koennen sehr viel Strom brauchen. Als grobe Obergrenze gilt bei vielen RGB-LEDs:

```text
maximaler Strom = LED-Anzahl * ca. 60 mA
```

Beispiel: 100 LEDs koennen theoretisch bis zu etwa 6 A benoetigen. In echten Effekten ist es oft weniger, aber das Netzteil sollte ausreichend dimensioniert sein.
