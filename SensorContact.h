// SensorContact.h
#ifndef SENSOR_CONTACT_H
#define SENSOR_CONTACT_H

#include "Device.h"

class SensorContact : public Device {
private:
  String topicSuffix;
  int pin;
  bool lastState = false;
  bool stateChanged = false;

  /**
   * Anti-rebond logiciel : un contact mécanique (ILS/reed switch) "rebondit"
   * électriquement pendant quelques millisecondes à chaque changement d'état.
   * Sans ça, une seule ouverture de porte peut générer une rafale de faux
   * messages ON/OFF sur MQTT. On n'accepte un changement que s'il reste
   * stable pendant `debounceMs`.
   */
  bool candidateState = false;
  unsigned long candidateSince = 0;
  static const unsigned long debounceMs = 50;

public:
  SensorContact(const String& piece, const String& appareil, int gpioPin)
    : topicSuffix(piece + "/" + appareil), pin(gpioPin) {}

  String getTopicSuffix() const override { return topicSuffix; }

  /**
   * IMPORTANT (spécifique ESP32) : n'utilise JAMAIS les broches 34-39 pour ce
   * capteur — elles n'ont AUCUNE résistance de tirage interne sur le silicium
   * de l'ESP32 (contrairement à un PIR ou au MQ2, dont le signal est piloté
   * activement par le module et non "tiré" par une résistance). Avec
   * INPUT_PULLUP sur ces broches, l'entrée resterait flottante et donnerait
   * des lectures aléatoires. Choisis une broche parmi 0-27/32-33.
   */
  void begin() override { pinMode(pin, INPUT_PULLUP); }

  bool isSensor() const override { return true; }

  bool hasNewReading() override {
    // Avec INPUT_PULLUP : contact fermé (aimant présent, porte fermée) = LOW.
    // Contact ouvert (porte ouverte) = HIGH (tiré par la résistance interne).
    bool reading = digitalRead(pin) == HIGH;
    unsigned long now = millis();

    if(reading != candidateState){
      // Nouvel état détecté : on démarre (ou redémarre) le chronomètre de stabilité.
      candidateState = reading;
      candidateSince = now;
    }

    if(candidateState != lastState && (now - candidateSince) >= debounceMs){
      // L'état candidat est resté stable assez longtemps : on le valide vraiment.
      lastState = candidateState;
      stateChanged = true;
      return true;
    }
    return false;
  }

  String readStatus() override {
    stateChanged = false;
    return lastState ? "1" : "0";   // "1" = ouvert, "0" = fermé
  }
};

#endif