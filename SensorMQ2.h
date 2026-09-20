// SensorMQ2.h
#ifndef SENSOR_MQ2_H
#define SENSOR_MQ2_H

#include "Device.h"

/**
 * SensorMQ2.h
 * ---------------------------------------------------------
 * Capteur de gaz/fumée MQ2, sortie analogique.
 *
 * Respecte le principe Teguis "alarme locale avant tout réseau" :
 * le seuil est comparé et la sortie d'alarme locale pilotée dans
 * hasNewReading(), qui s'exécute à chaque tour de loop() via
 * DeviceRegistry::pollAndPublish — indépendamment du fait que le
 * Wi-Fi ou le broker MQTT soient joignables. La publication réseau
 * qui suit est un bonus d'information, jamais une condition au
 * déclenchement de l'alarme physique.
 * ---------------------------------------------------------
 */

class SensorMQ2 : public Device {
private:
  String topicSuffix;
  int analogPin;
  int alarmPin1;                 // sortie locale (buzzer/relais) ; -1 = désactivée
  int alarmPin2;                 // sortie locale (buzzer/relais) ;
  int alarmThreshold;           // seuil brut ADC (0-4095 sur ESP32) déclenchant l'alarme
  unsigned long intervalMs;
  unsigned long lastRead = 0;
  unsigned long warmupUntil = 0;
  bool warmedUp = false;
  int lastValue = 0;
  bool alarmActive = false;

public:
  SensorMQ2(const String& piece, const String& appareil, int analogGpio,
            int alarmGpio1 = -1, int alarmGpio2 = -1, int threshold = 1800,
            unsigned long intervalMillis = 2000, unsigned long warmupMillis = 30000)
    : topicSuffix(piece + "/" + appareil), analogPin(analogGpio),
      alarmPin1(alarmGpio1), alarmPin2(alarmGpio2), alarmThreshold(threshold), intervalMs(intervalMillis),
      warmupUntil(warmupMillis) {}   // délai relatif, converti en horodatage absolu dans begin()

  String getTopicSuffix() const override { return topicSuffix; }

  /**
   * Sécurité importante : `alarmPin2` est pensé ici pour piloter un
   * dispositif de coupure/extinction automatique. Une fausse alarme MQ2
   * (fumée de cuisson, aérosol, humidité) reste possible avec un capteur
   * bas de gamme — réfléchis à une confirmation (ex: seuil soutenu sur
   * plusieurs lectures, ou double capteur) avant de piloter quoi que ce
   * soit d'irréversible (extincteur, coupure gaz) uniquement sur ce
   * seuil. Le buzzer/l'alerte locale (alarmPin1) est sans risque à
   * déclencher directement ; un actionneur physique mérite plus de prudence.
   */
  void begin() override {
    pinMode(analogPin, INPUT);
    if(alarmPin1 >= 0){
      pinMode(alarmPin1, OUTPUT);
      digitalWrite(alarmPin1, LOW);
    }
    if(alarmPin2 >= 0){                 // était absent : alarmPin2 n'était jamais configuré en sortie
      pinMode(alarmPin2, OUTPUT);
      digitalWrite(alarmPin2, LOW);
    }
    warmupUntil = millis() + warmupUntil;
  }

  bool isSensor() const override { return true; }

  bool hasNewReading() override {
    // Chauffe minimale du MQ2 : lectures avant ~20-30s non fiables.
    // C'est un garde-fou de fonctionnement, PAS un étalonnage (voir note ci-dessous).
    if(!warmedUp){
      if(millis() < warmupUntil) return false;
      warmedUp = true;
    }

    unsigned long now = millis();
    if(now - lastRead < intervalMs) return false;
    lastRead = now;

    lastValue = analogRead(analogPin);
    Serial.println(analogRead(analogPin));

    // --- Alarme locale, avant toute considération réseau ---
    bool shouldAlarm = lastValue >= alarmThreshold;
    if(shouldAlarm != alarmActive){
      alarmActive = shouldAlarm;
      // Chaque sortie est pilotée indépendamment de l'autre : avant, alarmPin2
      // dépendait par erreur de la présence d'alarmPin1 (et plantait si
      // alarmPin1 était désactivé à -1 pendant qu'alarmPin2 était actif).
      if(alarmPin1 >= 0) {
        if(alarmActive == HIGH){
          tone(23, 1000);  // change state of the LED by setting the pin to the HIGH voltage level                     
        } else {
          digitalWrite(alarmPin1, LOW);
        }
      }
       
      if(alarmPin2 >= 0) digitalWrite(alarmPin2, alarmActive ? HIGH : LOW);
    }

    return true;
  }

  String readStatus() override {
    return String(lastValue);
  }

  bool isAlarmActive() const { return alarmActive; }
};

#endif