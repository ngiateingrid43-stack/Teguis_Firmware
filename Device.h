#ifndef DEVICE_H   // garde d'inclusion
#define DEVICE_H

#include <Arduino.h>   // pour le type String et les fonctions de base Arduino

/**
 * Device.h
 * ---------------------------------------------------------
 * Interface abstraite commune à TOUT ce qu'on peut brancher :
 * relais, capteur de température, capteur de porte, etc.
 *
 * Pourquoi une interface commune plutôt qu'une classe par
 * type gérée à part : le reste du programme (DeviceRegistry,
 * MqttManager) manipule des `Device*` génériques. Ajouter un
 * nouveau type d'appareil (ex: un variateur, un capteur de
 * mouvement) ne demande JAMAIS de modifier DeviceRegistry ou
 * MqttManager — juste d'écrire une nouvelle classe qui hérite
 * de Device et de la déclarer dans config.h.
 * ---------------------------------------------------------
 */

class Device {
public:
  // Destructeur virtuel obligatoire dès qu'une classe a des méthodes virtuelles et
  // qu'on la détruit potentiellement via un pointeur de base (Device*).
  virtual ~Device() {}

  /** Ex: "salon/lumiere" — sert à construire les topics MQTT set/status */
  virtual String getTopicSuffix() const = 0;   // = 0 : méthode PURE virtuelle, chaque sous-classe DOIT l'implémenter

  /** Initialisation matérielle (pinMode, etc.), appelée une fois dans setup() */
  virtual void begin() = 0;   // pure virtuelle aussi : chaque device a sa propre init

  /** true si cet appareil réagit à des commandes (relais) */
  virtual bool isActuator() const { return false; }   // implémentation par défaut : "non", sauf si redéfini (Relay le fait)

  /** true si cet appareil publie périodiquement une valeur (capteur) */
  virtual bool isSensor() const { return false; }   // implémentation par défaut : "non", sauf si redéfini (SensorDHT le fait)

  /** Appelé quand une commande arrive sur son topic /set (relais uniquement) */
  virtual void handleCommand(const String& payload) { /* no-op par défaut */ }

  /** Appelé en boucle : retourne true si une nouvelle valeur est prête à publier */
  virtual bool hasNewReading() { return false; }   // les relais n'ont rien à publier périodiquement, donc false par défaut

  /** Valeur courante à publier sur le topic /status */
  virtual String readStatus() { return ""; }   // redéfini par chaque type concret (ON/OFF pour un relais, un nombre pour un capteur)
};

#endif   // fin de la garde d'inclusion
