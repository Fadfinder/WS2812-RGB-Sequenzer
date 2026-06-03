# WS2812 RGB Sequenzer

Ein webbasierter RGB-LED-Sequenzer fuer **Wemos D1 mini / ESP8266** und **WS2812B** LED-Streifen. Sequenzen werden ueber eine Weboberflaeche erstellt, live auf dem LED-Streifen getestet, im Wemos gespeichert und laufen danach eigenstaendig ohne Browser oder externe Steuerung weiter.

## Funktionen

- Grafische Weboberflaeche direkt vom Wemos D1 mini
- Anzahl der LEDs einstellbar
- Mehrere Sequenzen als Playlist
- Schritte bearbeiten, kopieren, einfuegen, verschieben und loeschen
- Pro Schritt beliebige LEDs mit individuellen Farben
- Effektgruppen pro Schritt, z. B. Feuer, Gewitter, Regenbogen, Schweißerlicht und Fotografen-Blitz
- Effektgeschwindigkeit pro Effekt
- Effektfarbe fuer einfarbige Effekte
- Live-Vorschau waehrend der Bearbeitung
- Dauerhafte Speicherung im Flash per LittleFS
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
7. Browser oeffnen: `http://192.168.4.1`.
8. LED-Anzahl einstellen, Sequenzen und Schritte erstellen.
9. `Alle Sequenzen speichern` druecken.

Nach dem Speichern laeuft die Playlist eigenstaendig weiter.

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
