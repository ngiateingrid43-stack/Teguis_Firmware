#ifndef SENSOR_DHT_H   // garde d'inclusion
#define SENSOR_DHT_H

#include "Device.h"
#include <DHT.h>   // bibliothèque officielle Adafruit pour les capteurs DHT11/DHT22

/**
 * SensorDHT.h
 * ---------------------------------------------------------
 * Capteur de température OU humidité (DHT11). Deux instances
 * différentes (une par grandeur mesurée) partagent le même
 * capteur physique mais publient sur deux topics distincts —
 * plus simple à router côté DeviceRegistry qu'un topic combiné.
 * ---------------------------------------------------------
 */

// Enumération des deux grandeurs qu'un DHT22 peut mesurer.
enum DhtReading { TEMPERATURE, HUMIDITY };

class SensorDHT : public Device {   // hérite de l'interface Device
private:
  String topicSuffix;               // ex: "douche/temperature"
  DHT* dht;                          // pointeur partagé entre temp/humidité du même capteur physique
  DhtReading reading;                 // quelle grandeur CETTE instance mesure (température ou humidité)
  unsigned long intervalMs;           // intervalle minimum entre deux lectures (évite de saturer le capteur)
  unsigned long lastRead = 0;         // horodatage (millis()) de la dernière lecture effectuée
  String lastValue;                   // dernière valeur mesurée valide, en texte
  bool pendingPublish = false;        // vrai si une nouvelle valeur attend d'être publiée (non utilisé activement ici, gardé pour extension)

public:
  // Constructeur : construit le topicSuffix, mémorise le capteur partagé, la grandeur et l'intervalle (5s par défaut).
  SensorDHT(const String& piece, const String& appareil, DHT* dhtInstance,
            DhtReading readingType, unsigned long intervalMillis = 5000)
    : topicSuffix(piece + "/" + appareil), dht(dhtInstance),
      reading(readingType), intervalMs(intervalMillis) {}

  // Renvoie le suffixe de topic pour ce capteur (utilisé par DeviceRegistry).
  String getTopicSuffix() const override { return topicSuffix; }

  // Pas d'initialisation ici : voir le commentaire ci-dessous.
  void begin() override {
    // dht->begin() est appelé une seule fois côté config.h (capteur partagé),
    // pas ici, pour éviter un double begin() si temp+humidité partagent le DHT.
  }

  // Ce Device est bien un capteur (publie périodiquement) — utilisé par DeviceRegistry pour le poll.
  bool isSensor() const override { return true; }

  // Appelé à chaque tour de loop() (via pollAndPublish) : effectue une nouvelle lecture SI l'intervalle
  // minimum s'est écoulé, et renvoie true si une valeur valide vient d'être obtenue.
  bool hasNewReading() override {
    unsigned long now = millis();
    if(now - lastRead < intervalMs) return false;   // pas encore temps de relire, on attend le prochain tour
    lastRead = now;                                  // on marque cette tentative de lecture (même si elle échoue)

    // Lit la température ou l'humidité selon le type configuré pour cette instance.
    float value = (reading == TEMPERATURE) ? dht->readTemperature() : dht->readHumidity();
    if(isnan(value)) return false; // lecture ratée, on réessaiera au prochain intervalle

    lastValue = String(value, 1); // une décimale
    pendingPublish = true;
    return true;   // une nouvelle valeur valide est disponible
  }

  // Renvoie la dernière valeur mesurée (appelé juste après hasNewReading() a retourné true).
  String readStatus() override {
    pendingPublish = false;
    return lastValue;
  }
};

#endif   // fin de la garde d'inclusion
