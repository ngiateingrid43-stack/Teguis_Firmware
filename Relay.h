#ifndef RELAY_H   // garde d'inclusion
#define RELAY_H

#include "Device.h"

/**
 * Relay.h
 * ---------------------------------------------------------
 * Un relais générique : lumière, prise... Tout ce qui se résume
 * à "ON/OFF sur une broche" utilise cette même classe — pas
 * besoin d'une classe par pièce.
 *
 * Le contrôle de porte par SERVO (garage, salon) vit désormais
 * dans ServoDoor.h — une classe séparée. Le mélanger ici forçait
 * CHAQUE relais (même une simple lumière) à traîner deux objets
 * Servo inutiles et provoquait des conflits d'attach() sur les
 * mêmes pins à chaque nouvelle instance.
 * ---------------------------------------------------------
 */

class Relay : public Device {   // hérite de l'interface Device
private:
  String topicSuffix;   // ex: "salon/lumiere"
  uint8_t pin;           // numéro de broche GPIO physique reliée au relais
  bool activeHigh;       // certains modules relais s'activent sur LOW, voir README
  bool state = false;    // état logique courant (indépendant du niveau électrique réel, voir writeState)

public:
  // Constructeur : construit le topicSuffix à partir de piece+appareil, et mémorise pin/polarité.
  Relay(const String& piece, const String& appareil, uint8_t pinNumber, bool isActiveHigh = true)
    : topicSuffix(piece + "/" + appareil), pin(pinNumber), activeHigh(isActiveHigh) {}

  // Renvoie le suffixe de topic pour cet appareil (utilisé par DeviceRegistry pour router/construire les topics).
  String getTopicSuffix() const override { return topicSuffix; }

  // Configure la broche en sortie et force un état de repos sûr au démarrage.
  void begin() override {
    pinMode(pin, OUTPUT);
    writeState(false); // état de repos sûr au démarrage
  }

  // Un relais est bien un actionneur (réagit aux commandes) — utilisé par DeviceRegistry pour le routage.
  bool isActuator() const override { return true; }

  // Traite une commande reçue sur le topic .../set (payload = "ON"/"1" ou autre chose = OFF).
  void handleCommand(const String& payload) override {
    state = (payload == "ON" || payload == "1");   // accepte deux formats de payload courants
    writeState(state);                              // applique réellement le changement sur la broche
  }

  // Renvoie l'état courant sous forme de texte, pour republier sur le topic .../status.
  String readStatus() override {
    return state ? "ON" : "OFF";
  }

private:
  // Traduit un état logique (allumé/éteint) en niveau électrique réel (HIGH/LOW),
  // en tenant compte de la polarité du module relais (activeHigh).
  void writeState(bool on){
    digitalWrite(pin, (on == activeHigh) ? HIGH : LOW);
  }
};

#endif   // fin de la garde d'inclusion
