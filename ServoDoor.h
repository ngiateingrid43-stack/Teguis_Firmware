#ifndef SERVO_DOOR_H   // garde d'inclusion
#define SERVO_DOOR_H

#include "Device.h"
#include <ESP32Servo.h>

/**
 * ServoDoor.h
 * ---------------------------------------------------------
 * Porte/garage motorisé(e) par SERVO — volontairement une classe
 * À PART de Relay.h (et non un membre de Relay) : sinon CHAQUE
 * relais (même une simple lumière) traînerait deux objets Servo
 * inutiles, et chaque instanciation réattacherait les mêmes pins
 * PWM en conflit. Un ServoDoor = un servo = une pin, point.
 *
 * IMPORTANT — bibliothèque requise :
 *   Installe "ESP32Servo" (par Kevin Harrington) via le
 *   Gestionnaire de bibliothèques de l'IDE Arduino. La lib
 *   "Servo" standard d'Arduino ne fonctionne pas sur ESP32.
 * ---------------------------------------------------------
 */

class ServoDoor : public Device {
private:
  String topicSuffix;
  uint8_t pin;
  Servo servo;
  int vitesse = 500;
  int angleFerme;
  int angleOuvert;
  unsigned long delaiParDegreMs;
  bool ouvert = false;
  long int tempsAction1 = 0;
  long int tempsAction2 = 0;

public:
  ServoDoor(const String& piece, const String& appareil, uint8_t pinNumber,
            int angleFermeDeg = 0, int angleOuvertDeg = 90, unsigned long speedDelayMs = 120)
    : topicSuffix(piece + "/" + appareil), pin(pinNumber),
      angleFerme(angleFermeDeg), angleOuvert(angleOuvertDeg),
      delaiParDegreMs(speedDelayMs) {}
      // Pas de .attach() ici : le hardware n'est pas encore prêt dans le
      // constructeur (objets globaux construits avant l'init Arduino).

  String getTopicSuffix() const override { return topicSuffix; }

  // Attache le servo et le met en position "fermée" au démarrage — dans
  // begin(), qui est appelé APRÈS l'initialisation matérielle par le .ino.
  void begin() override {
    ESP32PWM::allocateTimer(0);
    servo.setPeriodHertz(50);
    servo.attach(pin, 500, 2400);
    servo.write(angleFerme);
    ouvert = false;
  }

  bool isActuator() const override { return true; }

  // Le mouvement se déclenche ICI, au moment même où la commande MQTT
  // arrive — pas dans readStatus() qui ne sert qu'à republier l'état.
  void handleCommand(const String& payload) override {
    ouvert = (payload == "ON" || payload == "1" || payload == "OUVRIR");
    tempsAction1 = millis();
    int ouverture = angleFerme;
    int fermeture = angleOuvert;
    //boucle de fermeture et d ouverture en fonction de l'action avec un boucle non bloquante sans delay
    while(tempsAction1 != (millis() + vitesse * (ouvert ? angleOuvert : angleFerme))) {
      servo.write(ouvert ? ouverture++  : fermeture--);
    }
  }

  String readStatus() override {
    return ouvert ? "ON" : "OFF";
  }
};

#endif   // fin de la garde d'inclusion
