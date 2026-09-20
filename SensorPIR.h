// SensorPIR.h
#ifndef SENSOR_PIR_H
#define SENSOR_PIR_H

#include "Device.h"

/**
 * SensorPIR.h
 * ---------------------------------------------------------
 * 
 * ---------------------------------------------------------
 */

class SensorPIR : public Device {
private:
  String topicSuffix;
  int pin;
  bool lastState = false;
  bool stateChanged = false;

public:
  SensorPIR(const String& piece, const String& appareil, int gpioPin)
    : topicSuffix(piece + "/" + appareil), pin(gpioPin) {}

  String getTopicSuffix() const override { return topicSuffix; }

  void begin() override { pinMode(pin, INPUT); }

  bool isSensor() const override { return true; }

  bool hasNewReading() override {
    bool current = digitalRead(pin) == HIGH;
    if(current != lastState){
      lastState = current;
      stateChanged = true;
      return true;
    }
    return false;
  }

  String readStatus() override {
    stateChanged = false;
    return lastState ? "1" : "0";
  }

  /** Lecture immédiate sans effet de bord — pour un contrôleur d'automatisation
   *  (ex: PresenceLightController) qui a besoin de l'état courant à chaque tour
   *  de boucle, indépendamment du cycle de publication MQTT de hasNewReading(). */
  bool currentState() const { return lastState; }
};

#endif