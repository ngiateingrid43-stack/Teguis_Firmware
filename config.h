#ifndef CONFIG_H   // garde d'inclusion : évite que ce fichier soit inclus deux fois dans la même compilation
#define CONFIG_H

/**
 * config.h
 * 
 * TOUT ce qui est propre à TON installation vit ici :
 * identifiants Wi-Fi/broker, et surtout la liste des appareils
 * par pièce. Pour ajouter un appareil, une seule ligne à
 * ajouter dans une des fonctions ci-dessous — aucune autre
 * partie du firmware ne change.
 * 
 */

//  Wi-Fi 
#define WIFI_SSID     "Mets ta connexion"   // nom de ton réseau Wi-Fi
#define WIFI_PASSWORD "ttrkpma2006"             // mot de passe de ce réseau

//  Broker MQTT 
#define MQTT_SERVER   "f0f51473e19f4dc89f2813a0a491dcbb.s1.eu.hivemq.cloud"   // cluster HiveMQ Cloud
#define MQTT_PORT     8883            // 8883 en TLS (voir README, section sécurité)
#define MQTT_USER     "viciaHome"              // identifiant MQTT de la plateforme
#define MQTT_PASSWORD "viciaSecure"            // mot de passe associé à MQTT_USER
#define TOPIC_PREFIX  "villa-douala"           // slug exact de la maison côté plateforme

#define KEYPAD_PIN      35
#define RFID_SCK_PIN    18
#define RFID_MISO_PIN   34
#define RFID_MOSI_PIN   4
#define RFID_SS_PIN     13
#define RFID_RST_PIN    25
#define LOCK_RELAY_PIN  15   // broche de strapping, voir note de sécurité plus bas

// Inclusions des classes utilisées plus bas dans ce fichier (registre, relais, capteur, lib DHT officielle).
#include "DeviceRegistry.h"
#include "Relay.h"
#include "SensorDHT.h"
#include "SensorPIR.h"
#include "SensorMQ2.h"
#include "SensorContact.h"
#include "SensorCurrent.h"
#include "AccessControl.h"
#include "SensorLDR.h"
#include "PresenceLightController.h"
#include "ServoDoor.h"
#include <DHT.h>

/**
 * Un seul capteur DHT11 physique peut être partagé par plusieurs
 * "devices" logiques (température ET humidité) — déclaré ici pour
 * qu'il ne soit initialisé (dht.begin()) qu'une seule fois.
 */
// GPIO 17 (déplacé depuis 15, qui entrait en conflit avec le relais du couloir).
static DHT dhtSalon(17, DHT11);   // capteur d'humidité/température dans le salon
static Relay* salonLumiereRelay = nullptr;
static SensorPIR* salonPirSensor = nullptr;
static SensorLDR* salonLdrSensor = nullptr;
static PresenceLightController* salonLightAuto = nullptr;
/**
 * Déclare tous les appareils de la maison. Modifie CETTE fonction
 * pour ajouter/retirer un appareil — c'est le seul endroit à toucher.
 *c'est le point cle pour la scalabilite
 */
// `inline` : évite une erreur de "symbole dupliqué" si ce header est inclus plusieurs fois dans le projet.
inline void registerAllDevices(DeviceRegistry& registry){

  dhtSalon.begin(); // initialisation unique du capteur physique partagé

  //  Salon  
  // Les 3 lignes suivantes gardent le pointeur concret (Relay*/SensorPIR*/SensorLDR*)
  // en plus de l'ajouter au registre — nécessaire pour que PresenceLightController
  // puisse appeler leurs méthodes spécifiques (handleCommand, currentState, isDark),
  // qui n'existent pas sur l'interface générique Device.
  salonLumiereRelay = new Relay("salon", "lumiere", 26);        // relais lumière salon sur GPIO 26
  registry.add(salonLumiereRelay);

  salonPirSensor = new SensorPIR("salon", "mouvement", 16);    // capteur PIR de mouvement sur GPIO 16
  registry.add(salonPirSensor);

  // LDR sur GPIO 22 (libérée ci-dessus). Seuil à calibrer chez toi — voir le
  // commentaire de calibration en tête de SensorLDR.h avant de faire confiance
  // à la valeur par défaut (2500).
  salonLdrSensor = new SensorLDR("salon", "luminosite", 32, 300);
  registry.add(salonLdrSensor);

  // Automatisation présence + obscurité -> lumière du salon.
  // Sans ce bloc, salonLightAuto restait à nullptr et la logique de
  // PresenceLightController.h n'était jamais exécutée.
  salonLightAuto = new PresenceLightController(salonPirSensor, salonLdrSensor, salonLumiereRelay, TOPIC_PREFIX);
  //registry.add(new Relay("salon", "lumiere", 26));   // relais lumière du salon sur la broche GPIO 26
  registry.add(new SensorDHT("salon", "humidite", &dhtSalon, HUMIDITY));       // capteur d'humidité (partage dhtSalon)
  registry.add(new SensorDHT("salon", "temperature", &dhtSalon, TEMPERATURE)); // capteur de température (même capteur physique)
  //  Salon (porte) 
  // Porte du salon motorisée par SERVO — GPIO 12, volontairement laissée
  // libre pour ça (broche de strapping MTDI, mais servo.attach() n'a lieu
  // qu'après le démarrage dans begin(), donc sans impact sur le boot).
  registry.add(new ServoDoor("salon", "porte", 12));

  //  Chambre 
  registry.add(new Relay("chambre", "lumiere", 27)); // relais lumière de la chambre sur GPIO 27

  //  Cuisine 
  registry.add(new Relay("cuisine", "lumiere", 14));      // relais lumière cuisine sur GPIO 14
  // MQ2 : analogique sur GPIO 36 (broche ENTRÉE SEULE, idéale pour un signal piloté
  // activement comme celui du MQ2 — GPIO 31 n'existe pas physiquement sur l'ESP32).
  // alarmPin1=23 (buzzer/alerte locale) ; alarmPin2=22 (coupure/extincteur —
  // voir note de sécurité dans SensorMQ2.h avant de brancher un actionneur réel).
  registry.add(new SensorMQ2("cuisine", "gaz", 5, 23, 23, 1800));

  //  Garage 
  // Porte de garage motorisée par SERVO — GPIO 33 (libre, ne rentre en
  registry.add(new ServoDoor("garage", "porte", 33));
  //registry.add(new Relay("garage", "lumiere", 32));        // relais lumière garage sur GPIO 32

  

  //  Douche 
  registry.add(new Relay("douche", "lumiere", 19));     // relais lumière douche sur GPIO 19

  //  Pompe à eau 
  // GPIO 21 remplacé par GPIO 0 : le 21 est déjà utilisé par portail/porte
  // juste au-dessus — deux relais ne peuvent jamais partager la même broche
  // (le second écraserait le comportement du premier au moment de la lecture
  // de son état, et digitalWrite serait contradictoire entre les deux).
  // GPIO0 est une broche de strapping (bouton BOOT sur la plupart des DevKit) :
  // utilisable après démarrage, mais évite qu'un état de la pompe au repos
  // tire la ligne d'une façon qui interférerait avec un futur reset/reflash.
  //registry.add(new Relay("pompe", "pompe", 0)); // relay pompe a eau

  //  Jardin 
  /* arrosage automatique */

  //  Énergie 
  // Un seul capteur de courant déclaré ici pour l'instant (ex: ACS712) sur le
  // circuit "autres appareils". GPIO 39 est la broche encore libre après
  // affectation du module de contrôle d'accès (GPIO 34 sert maintenant au
  // MISO du lecteur RFID, donc plus disponible pour un second capteur ici).
  // Ajuste `mVperAmp` selon TON modèle de capteur (185/100/66 mV/A pour les
  // versions 5A/20A/30A) et `mainsVoltage` selon ta tension secteur réelle.
  registry.add(new SensorCurrent("energie", "appareils", 2, 100.0, 220.0));

}

#endif   // fin de la garde d'inclusion CONFIG_H