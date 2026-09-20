#ifndef PRESENCE_LIGHT_CONTROLLER_H
#define PRESENCE_LIGHT_CONTROLLER_H

#include <Arduino.h>
#include <functional>
#include "SensorPIR.h"
#include "SensorLDR.h"
#include "Relay.h"

/**
 * PresenceLightController.h
 * ---------------------------------------------------------
 * Automatisation : allume une ampoule (Relay) UNIQUEMENT si un
 * mouvement est détecté (SensorPIR) ET que la luminosité ambiante
 * est insuffisante (SensorLDR) — évite d'allumer en plein jour même
 * si quelqu'un passe devant le capteur.
 *
 * Comportement complet :
 *  - S'allume dès que (mouvement détecté) ET (sombre)
 *  - Reste allumée tant qu'il fait sombre, pendant `extinctionDelaiMs`
 *    après le DERNIER mouvement détecté (évite que la lumière clignote
 *    entre deux détections rapprochées)
 *  - S'éteint automatiquement si la pièce redevient claire, même si
 *    quelqu'un est encore présent (économie d'énergie), ou après le
 *    délai d'extinction si plus personne n'est détecté
 *
 * Fonctionne comme un TROISIÈME "utilisateur" du relais, au même titre
 * que le dashboard (toggle manuel) et la commande vocale : elle appelle
 * directement `relay->handleCommand()`, puis republie l'état via le
 * callback MQTT pour que le dashboard reste synchronisé en temps réel —
 * sinon le changement resterait invisible jusqu'à la prochaine reconnexion
 * MQTT (le seul moment où DeviceRegistry republie sinon l'état d'un relais).
 *
 * NE DÉPEND D'AUCUNE CONNEXION RÉSEAU pour fonctionner — comme
 * AccessControl et l'alarme locale de SensorMQ2, l'automatisation
 * physique reste opérationnelle même hors ligne. Le callback MQTT
 * est un bonus d'information, jamais une condition.
 * ---------------------------------------------------------
 */

class PresenceLightController {
private:
  SensorPIR* pir;
  SensorLDR* ldr;
  Relay* relay;
  String topicPrefix;

  unsigned long extinctionDelaiMs;
  unsigned long lastMotionAt = 0;
  bool lightOnByAutomation = false;

  using PublishFn = std::function<void(const String& topic, const String& valeur)>;
  PublishFn onPublish;

public:
  PresenceLightController(SensorPIR* pirSensor, SensorLDR* ldrSensor, Relay* targetRelay,
                           const String& mqttTopicPrefix,
                           unsigned long delaiExtinctionMs = 30000)
    : pir(pirSensor), ldr(ldrSensor), relay(targetRelay),
      topicPrefix(mqttTopicPrefix), extinctionDelaiMs(delaiExtinctionMs) {}

  void setPublishCallback(PublishFn cb){ onPublish = cb; }

  /** A appeler à chaque tour de loop(), indépendamment de l'état Wi-Fi/MQTT. */
  void loop(){
    bool presence = pir->currentState();
    bool sombre = ldr->isDark();
    unsigned long now = millis();

    if(presence) lastMotionAt = now;

    bool shouldBeOn = sombre && (now - lastMotionAt < extinctionDelaiMs);

    if(shouldBeOn != lightOnByAutomation){
      lightOnByAutomation = shouldBeOn;
      relay->handleCommand(shouldBeOn ? "ON" : "OFF");
      report();
    }
  }

private:
  void report(){
    if(!onPublish) return;
    String topic = "home/" + topicPrefix + "/" + relay->getTopicSuffix();
    onPublish(topic, relay->readStatus());
  }
};

#endif
