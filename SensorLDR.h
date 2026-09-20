#ifndef SENSOR_LDR_H
#define SENSOR_LDR_H

#include "Device.h"

/**
 * SensorLDR.h
 * ---------------------------------------------------------
 * Capteur de luminosité ambiante à base de LDR (photorésistance)
 * montée en pont diviseur de tension avec une résistance fixe.
 *
 * IMPORTANT — sens du seuil à calibrer chez toi, ça dépend du câblage :
 *   - LDR entre VCC et la broche ADC, résistance fixe entre la broche et GND
 *     → plus il fait clair, plus la tension lue est HAUTE (LDR moins résistante)
 *   - LDR entre la broche ADC et GND, résistance fixe entre VCC et la broche
 *     → c'est l'inverse : plus il fait clair, plus la tension lue est BASSE
 *
 * Ce fichier suppose le DEUXIÈME montage (le plus courant dans les tutoriels
 * LDR+ESP32) : valeur ADC BASSE = clair, valeur ADC HAUTE = sombre. Si `isDark()`
 * te donne l'inverse de la réalité chez toi, inverse simplement la comparaison
 * dans `isDark()` ci-dessous, ou permute LDR et résistance fixe sur le montage.
 * Calibre `seuilObscurite` en observant la valeur lue (Serial.println) de jour
 * et de nuit chez toi — les valeurs "typiques" trouvées en ligne ne sont
 * fiables qu'à titre indicatif, ça dépend fortement du modèle de LDR utilisé.
 * ---------------------------------------------------------
 */

class SensorLDR : public Device {
private:
  String topicSuffix;
  int pin;
  int seuilObscurite; // valeur ADC (0-4095) au-delà de laquelle on considère qu'il fait sombre
  unsigned long intervalMs;
  unsigned long lastRead = 0;
  int lastValue = 0;

public:
  SensorLDR(const String& piece, const String& appareil, int analogGpio,
            int seuilObscuriteAdc = 2500, unsigned long intervalMillis = 2000)
    : topicSuffix(piece + "/" + appareil), pin(analogGpio),
      seuilObscurite(seuilObscuriteAdc), intervalMs(intervalMillis) {}

  String getTopicSuffix() const override { return topicSuffix; }

  void begin() override { pinMode(pin, INPUT); }

  bool isSensor() const override { return true; }

  bool hasNewReading() override {
    unsigned long now = millis();
    if(now - lastRead < intervalMs) return false;
    lastRead = now;
    lastValue = analogRead(pin);
    return true;
  }

  /** Valeur ADC brute publiée sur MQTT (utile pour calibrer seuilObscurite à distance) */
  String readStatus() override {
    return String(lastValue);
  }

  /**
   * Lecture immédiate, sans passer par le cycle de publication MQTT —
   * pour un contrôleur d'automatisation (ex: PresenceLightController)
   * qui a besoin de savoir "fait-il sombre MAINTENANT ?" à chaque tour
   * de boucle, indépendamment de `intervalMs`.
   */
  bool isDark(){
    lastValue = analogRead(pin);
    return lastValue <= seuilObscurite;
  }
};

#endif
