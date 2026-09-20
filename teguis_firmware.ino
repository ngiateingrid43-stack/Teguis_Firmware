/**
 * teguis_firmware.ino
 * ---------------------------------------------------------
 * Point d'entrée. Volontairement très court : toute la logique
 * vit dans les classes (WifiManager, MqttManager, DeviceRegistry,
 * Relay, SensorDHT, AccessControl...). Ce fichier ne fait qu'assembler.
 * ---------------------------------------------------------
 **/

// Un seul #include par responsabilité — chaque .h explique son propre rôle en en-tête.
#include "config.h"
#include "WifiManager.h"
#include "MqttManager.h"
#include "DeviceRegistry.h"
#include "AccessControl.h"

// Objets globaux créés une seule fois au démarrage du programme (avant setup()).
WifiManager wifiManager(WIFI_SSID, WIFI_PASSWORD);                 // gère la connexion Wi-Fi
DeviceRegistry registry(TOPIC_PREFIX);                             // contient tous les relais/capteurs déclarés
MqttManager mqttManager(MQTT_SERVER, MQTT_PORT, MQTT_USER, MQTT_PASSWORD, "esp32-teguis", &registry); // gère MQTT

// Double authentification (code clavier 1x5 + badge RFID). Fonctionne
// entièrement SANS Wi-Fi ni MQTT — voir AccessControl.h pour le détail.
AccessControl accessControl(
  KEYPAD_PIN,
  RFID_SS_PIN, RFID_RST_PIN,
  RFID_SCK_PIN, RFID_MISO_PIN, RFID_MOSI_PIN,
  LOCK_RELAY_PIN
);

// Exécuté une seule fois au démarrage/reset de l'ESP32.
void setup(){
  Serial.begin(115200);   // ouvre le port série pour les logs de debug (moniteur série Arduino IDE)

  registerAllDevices(registry);  // défini dans config.h — un seul endroit à modifier
  registry.beginAll();           // initialise matériellement chaque relais/capteur (pinMode, etc.)

  accessControl.begin();  // initialise clavier + RFID + LCD + serrure — indépendant du réseau

  // Best-effort : reporte les événements d'accès sur MQTT si le broker est
  // joignable, mais AccessControl continue de fonctionner normalement sinon.
  accessControl.setEventCallback([](const String& evenement){
    if(mqttManager.isConnected()){
      mqttManager.publish(String("home/") + String(TOPIC_PREFIX) + "/security/access", evenement);
    }
  });

  wifiManager.begin();    // connexion Wi-Fi bloquante jusqu'à succès (voir WifiManager.h)
  mqttManager.begin();    // prépare le client MQTT (TLS, callback) — pas encore connecté au broker ici

  // Republie l'état du relais salon/lumiere sur MQTT dès que l'automatisation
  // présence+obscurité change son état, pour garder le dashboard synchronisé.
  if(salonLightAuto){
    salonLightAuto->setPublishCallback([](const String& topic, const String& valeur){
      mqttManager.publish(topic, valeur);
    });
  }

}

// Exécuté en boucle infinie après setup().
void loop(){
  // Le contrôle d'accès tourne EN PREMIER et INCONDITIONNELLEMENT : la
  // sécurité physique (code + badge) ne doit jamais attendre le Wi-Fi/MQTT.
  accessControl.loop();
  if(salonLightAuto) salonLightAuto->loop();
  wifiManager.ensureConnected();   // reconnecte automatiquement le Wi-Fi si la liaison est tombée

  mqttManager.loop();              // gère la reconnexion MQTT + traite les messages entrants/keep-alive

  // Publie les nouvelles lectures de capteurs dès qu'elles sont prêtes
  registry.pollAndPublish([](const String& topic, const String& valeur){
    mqttManager.publish(topic, valeur);   // callback exécuté pour chaque capteur ayant une nouvelle valeur
  });
}
