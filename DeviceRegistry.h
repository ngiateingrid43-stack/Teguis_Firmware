#ifndef DEVICE_REGISTRY_H   // garde d'inclusion
#define DEVICE_REGISTRY_H

#include <functional>    // pour std::function (utilisé pour les callbacks de publication)
#include "Device.h"

/**
 * DeviceRegistry.h
 * ---------------------------------------------------------
 * Contient TOUS les appareils déclarés (relais + capteurs),
 * sans jamais connaître leur type concret — uniquement
 * l'interface Device.
 *
 * Deux responsabilités :
 *  1. Router une commande MQTT entrante vers le bon Device
 *     (en comparant le topic à getTopicSuffix())
 *  2. Parcourir les capteurs/relais à chaque tour de boucle
 *     pour savoir s'il faut publier une nouvelle valeur
 *
 * C'est la SEULE classe qui doit changer si tu modifies la
 * façon dont les topics sont construits — tout le reste
 * (Relay, SensorDHT, main.ino) n'a pas à le savoir.
 * ---------------------------------------------------------
 */

#define MAX_DEVICES 32   // taille max du tableau statique de devices (facile à augmenter si besoin)

// Type de fonction utilisé pour "callback de publication" : reçoit (topic, valeur).
using PublishFn = std::function<void(const String& topic, const String& valeur)>;

class DeviceRegistry {
private:
  Device* devices[MAX_DEVICES];   // tableau de pointeurs génériques vers CHAQUE appareil déclaré
  int count = 0;                  // nombre d'appareils actuellement enregistrés
  String topicPrefix; // ex: "villa-douala"

  String normalizeCategory(const String& suffix) const {
    int slash = suffix.indexOf('/');
    String device = (slash < 0) ? suffix : suffix.substring(slash + 1);
    device.toLowerCase();

    if(device == "led" || device == "lumiere" || device == "lampe" || device == "light" || device == "relais" || device == "relay") return "lighting";
    if(device == "temp" || device == "temperature" || device == "hum" || device == "humidite" || device == "humidity" || device == "ventilateur" || device == "fan") return "climate";
    if(device == "pir" || device == "mouvement" || device == "porte" || device == "fenetre" || device == "sirene" || device == "contact") return "security";
    if(device == "mq2" || device == "gaz" || device == "smoke" || device == "safety") return "safety";
    if(device == "pompe" || device == "water" || device == "arrosage") return "garden";
    if(device == "power" || device == "appareils" || device == "courant" || device == "energie") return "energy";
    if(device == "camera" || device == "cam") return "camera";
    return "security";
  }

  String normalizeDeviceName(const String& value) const {
    String device = value;
    device.trim();
    device.toLowerCase();

    if(device == "lumiere" || device == "lampe" || device == "led" || device == "light") return "led";
    if(device == "temperature" || device == "temp") return "temp";
    if(device == "humidite" || device == "humidity" || device == "hum") return "hum";
    if(device == "mouvement" || device == "pir") return "pir";
    if(device == "gaz" || device == "mq2") return "mq2";
    if(device == "appareils" || device == "courant" || device == "power" || device == "energie") return "power";
    if(device == "ventilateur" || device == "fan") return "ventilateur";
    if(device == "relais" || device == "relay") return "relais";
    if(device == "camera" || device == "cam") return "camera";
    if(device == "pompe" || device == "water" || device == "arrosage") return "pompe";
    if(device == "sirene" || device == "siren") return "sirene";
    return device;
  }

  String buildPlatformTopic(const String& suffix, const String& suffixKind = "") const {
    if(suffix.length() == 0) return "home/" + topicPrefix;

    int slash = suffix.indexOf('/');
    if(slash < 0) {
      String device = normalizeDeviceName(suffix);
      String category = normalizeCategory(suffix);
      String base = "home/" + topicPrefix + "/" + category + "/" + device;
      if(category == "energy" && device == "power") return "home/" + topicPrefix + "/energy/power";
      if(suffixKind.length() > 0) return base + "/" + suffixKind;
      return base;
    }

    String room = suffix.substring(0, slash);
    String device = suffix.substring(slash + 1);
    String category = normalizeCategory(suffix);

    if(category == "energy" && (device.equalsIgnoreCase("power") || device.equalsIgnoreCase("appareils") || device.equalsIgnoreCase("energie"))) {
      String base = "home/" + topicPrefix + "/energy/power";
      if(suffixKind.length() > 0) return base + "/" + suffixKind;
      return base;
    }

    String base = "home/" + topicPrefix + "/" + category + "/" + room + "/" + normalizeDeviceName(device);
    if(suffixKind.length() > 0) return base + "/" + suffixKind;
    return base;
  }

public:
  // Constructeur : mémorise le préfixe utilisé pour construire tous les topics.
  explicit DeviceRegistry(const String& prefix) : topicPrefix(prefix) {}

  // Ajoute un nouvel appareil au registre (appelé depuis registerAllDevices() dans config.h).
  void add(Device* device){
    if(count < MAX_DEVICES) devices[count++] = device;   // ignore silencieusement si MAX_DEVICES est dépassé
  }

  // Initialise matériellement chaque appareil (pinMode, etc.) — appelé une fois dans setup().
  void beginAll(){
    for(int i = 0; i < count; i++) devices[i]->begin();
  }

  /** Topic générique à souscrire côté MqttManager : accepte home/villa-douala/... et home/villa-douala/.../set */
  String subscriptionFilter() const {
    return "home/" + topicPrefix + "/+/+/+/#";   // accepte le topic direct et le topic /set
  }

  /**
   * Appelé par MqttManager quand un message arrive. Retrouve
   * l'appareil correspondant au topic et lui délègue la commande.
   */
  void routeCommand(const String& topic, const String& payload){
    for(int i = 0; i < count; i++){
      if(!devices[i]->isActuator()) continue;   // seuls les relais reçoivent des commandes, pas les capteurs
      String expected = buildPlatformTopic(devices[i]->getTopicSuffix());
      String expectedWithSet = expected + "/set";
      if(topic == expected || topic == expectedWithSet){
        devices[i]->handleCommand(payload);   // délègue le traitement réel au Relay concerné
        return;                                // on a trouvé le bon appareil, inutile de continuer la boucle
      }
    }
    // Aucun appareil ne correspond : topic inconnu, ignoré silencieusement.
    // (Utile de logger en développement, voir main.ino)
  }

  /**
   * A appeler à chaque tour de loop(). `publish` reçoit (topic, valeur)
   * pour chaque capteur ayant une nouvelle valeur — DeviceRegistry ne
   * connaît pas MqttManager directement, pour rester testable seul.
   */
  void pollAndPublish(const PublishFn& publish){
    for(int i = 0; i < count; i++){
      if(devices[i]->isSensor() && devices[i]->hasNewReading()){   // seulement les capteurs, et seulement s'il y a du nouveau
        String topic = buildPlatformTopic(devices[i]->getTopicSuffix());
        publish(topic, devices[i]->readStatus());   // délègue la publication réelle au callback fourni
      }
    }
  }

  /** Republie l'état de tous les relais (utile juste après une reconnexion MQTT) */
  void publishAllActuatorStates(const PublishFn& publish){
    for(int i = 0; i < count; i++){
      if(devices[i]->isActuator()){   // seulement les relais (pas les capteurs, qui se republient tout seuls)
        String topic = buildPlatformTopic(devices[i]->getTopicSuffix());
        publish(topic, devices[i]->readStatus());
      }
    }
  }
};

#endif   // fin de la garde d'inclusion
