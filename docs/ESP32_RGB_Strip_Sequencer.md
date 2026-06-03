# ESP32 RGB-LED-Streifen Sequencer

Diese Version ist fuer einen adressierbaren RGB-LED-Streifen gedacht, bei dem jede LED einzeln angesprochen werden kann, z. B. WS2812, WS2815 oder kompatibel.

Bei einem 12-V-Projekt ist ein WS2815-Streifen praktisch, weil der Streifen mit 12 V versorgt wird. Der ESP32 braucht trotzdem 5 V oder USB.

## Wichtig: Streifen-Typ

- Adressierbarer RGB-Streifen: meist 3 Leitungen `+12V`, `GND`, `DIN`. Diese Firmware passt.
- Einfacher 12-V-RGB-Streifen: meist 4 Leitungen `+12V`, `R`, `G`, `B`. Damit kann der ganze Streifen nur eine Farbe gleichzeitig haben. Die Anzahl einzelner LEDs ist dann nicht einzeln steuerbar.

## Verdrahtung

```text
12 V Netzteil +  --------------------  +12V LED-Streifen
12 V Netzteil GND -------------------  GND LED-Streifen
12 V Netzteil GND -------------------  ESP32 GND

ESP32 GPIO 16 -- 330 Ohm ------------  DIN LED-Streifen

12 V -> Step-Down 5 V ---------------  ESP32 5V/VIN
```

Empfehlung:

- 330-Ohm-Widerstand in Reihe zur Datenleitung.
- 1000-uF-Elko zwischen +12 V und GND am Anfang des Streifens.
- Bei langen Streifen 12 V an mehreren Stellen einspeisen.
- GND von Netzteil, ESP32 und Streifen muss verbunden sein.

## Arduino-Bibliothek

Installiere in der Arduino IDE:

```text
Adafruit NeoPixel
```

## Nutzung

1. Sketch `ESP32_RGB_Strip_Sequencer.ino` auf den ESP32 laden.
2. Mit WLAN `RGB-Sequencer` verbinden, Passwort `12345678`.
3. Browser öffnen: `http://192.168.4.1`.
4. Anzahl der LEDs eintragen.
5. Sequenz benennen, Farbe wählen und LEDs im Raster anklicken.
6. Eine geänderte Farbe verändert bestehende LEDs nicht. Die Farbe wird erst gesetzt, wenn eine LED angeklickt wird.
7. Klick auf eine LED setzt die aktuelle Farbe. Klick auf dieselbe LED mit derselben Farbe schaltet sie aus.
8. Optional im Bereich `Effekt fuer LED-Gruppe` einen LED-Bereich eintragen, z. B. `1-20`, einen Effekt und dessen Geschwindigkeit wählen und den Effekt hinzufügen.
   Geschwindigkeit `1` ist normal, `0.5` langsamer, `2` doppelt so schnell.
   Die Effektfarbe wird bei einfarbigen Effekten verwendet, z. B. Funkeln, Komet, Theater-Lauflicht, Pulsieren, Atmen, Meteor, Sternfunkeln, Scanner und Herzschlag.
9. Verfügbare Effekte: Feuerflackern, Gewitter, Regenbogen, Schweisserlicht, Fotografen-Blitz, Polizei-Blitzer, Funkeln, Komet, Theater-Lauflicht, Pulsieren, Atmen, Lava, Ozean, Wald, Meteor, Sternfunkeln, Kerze, Sonnenaufgang, Sonnenuntergang, Scanner, Konfetti, Aurora, Giftgruen, Herzschlag.
10. Schrittdauer setzen und `Schritt speichern` drücken.
11. Weitere Schritte oder Sequenzen anlegen.
12. `Alle Sequenzen speichern` drücken.

Während der Bearbeitung leuchtet der echte Streifen live. Nach dem Speichern läuft die Playlist eigenständig weiter, auch wenn Browser oder Handy getrennt werden.

## Grenzen

Voreingestellt sind maximal 300 LEDs, 30 Sequenzen und 240 Schritte. Das kann im Sketch über `MAX_LEDS`, `MAX_SEQUENCES` und `MAX_STEPS` angepasst werden.
