#ifndef KEYPAD_1X5_H
#define KEYPAD_1X5_H

#include <Arduino.h>

/**
 * Keypad1x5.h
 * ---------------------------------------------------------
 * Reprend exactement la logique de lecture de ton croquis
 * clavier1x5ESP32.ino (5 touches sur une seule broche analogique,
 * via un pont diviseur résistif), transformée en classe réutilisable
 * et non bloquante — plus de `delay()`, plus de variables globales.
 *
 * Touches 1 à 4 = chiffres, touche 5 = OK/Valider (mêmes seuils
 * ADC que ton croquis d'origine).
 * ---------------------------------------------------------
 */

class Keypad1x5 {
private:
  int pin;
  bool keyHeld = false;

public:
  explicit Keypad1x5(int analogPin) : pin(analogPin) {}

  void begin(){
    pinMode(pin, INPUT);
  }

  /**
   * Retourne 0 si aucune touche (ou déjà comptabilisée), sinon 1-5.
   * L'anti-répétition (une seule détection par appui, comme ton
   * `toucheEnfoncee` d'origine) est gérée en interne.
   */
  int readKey(){
    int adc = analogRead(pin);

    if(adc > 3500){
      keyHeld = false;
      return 0;
    }
    if(keyHeld) return 0; // touche déjà maintenue, on ne redétecte pas

    int key = 0;
    if(adc < 400)       key = 1;
    else if(adc < 650)  key = 2;
    else if(adc < 900)  key = 3;
    else if(adc < 1200) key = 4;
    else if(adc < 2200) key = 5; // touche OK

    if(key > 0) keyHeld = true;
    return key;
  }
};

#endif
