#ifndef MQTT_MANAGER_H   // garde d'inclusion
#define MQTT_MANAGER_H

#include <PubSubClient.h>        // bibliothèque MQTT légère, la plus utilisée sur ESP32/Arduino
#include <WiFiClientSecure.h>    // couche TLS par-dessus la connexion Wi-Fi, pour le chiffrement wss/MQTTS
#include "DeviceRegistry.h"

/**
 * MqttManager.h
 * ---------------------------------------------------------
 * Encapsule PubSubClient : connexion, reconnexion automatique,
 * abonnement, et routage des messages entrants vers le
 * DeviceRegistry. Le reste du programme ne touche jamais
 * PubSubClient directement.
 *
 * Note technique : PubSubClient exige une fonction de callback
 * "libre" (pas une méthode de classe liée à `this`). On utilise
 * donc un pointeur statique vers le manager courant, réglé une
 * seule fois dans begin() — un compromis imposé par la bibliothèque,
 * pas un choix de conception qu'on aurait fait autrement.
 * ---------------------------------------------------------
 */

class MqttManager; // forward declaration pour le callback statique
static MqttManager* g_activeManager = nullptr;   // pointeur global vers l'unique instance active

class MqttManager {
private:
  WiFiClientSecure wifiClient;   // connexion TCP chiffrée TLS, utilisée en dessous par PubSubClient
  PubSubClient client;           // client MQTT proprement dit, construit sur wifiClient
  const char* server;            // adresse du broker (hostname)
  int port;                      // port du broker (8883 en TLS)
  const char* username;          // identifiant MQTT
  const char* password;          // mot de passe MQTT
  String clientId;                // identifiant unique de CET appareil sur le broker
  DeviceRegistry* registry;      // pointeur vers le registre, pour router les commandes reçues

public:
  // Constructeur : initialise `client` avec `wifiClient` (liste d'initialisation), puis mémorise le reste.
  MqttManager(const char* brokerHost, int brokerPort, const char* mqttUser,
              const char* mqttPass, const String& id, DeviceRegistry* deviceRegistry)
    : client(wifiClient), server(brokerHost), port(brokerPort),
      username(mqttUser), password(mqttPass), clientId(id), registry(deviceRegistry) {}

  // Prépare le client MQTT (TLS + callback), mais NE se connecte pas encore au broker
  // (la vraie connexion a lieu dans loop() → reconnect(), appelé tant que non connecté).
  void begin(){
    g_activeManager = this;               // on s'enregistre comme instance active pour le callback statique
    wifiClient.setInsecure();             // n'exige pas de vérifier le certificat du serveur (voir README, compromis TLS)
    client.setServer(server, port);       // configure l'adresse/port du broker
    client.setCallback(MqttManager::staticCallback);   // branche la fonction appelée à chaque message reçu
  }

  // Vrai si actuellement connecté au broker MQTT.
  bool isConnected(){
    return client.connected();
  }

  /** A appeler dans loop() — gère la reconnexion et redonne la main à PubSubClient */
  void loop(){
    if(!client.connected()) reconnect();   // tente une reconnexion si la liaison est tombée
    client.loop(); // indispensable : sans ça, ni messages entrants ni keep-alive
  }

  // Publie une valeur sur un topic MQTT donné.
  void publish(const String& topic, const String& value){
    client.publish(topic.c_str(), value.c_str());   // PubSubClient attend des char* (C-strings), pas des String
  }

private:
  // Fonction "libre" exigée par PubSubClient (pas une méthode liée à un `this` précis) :
  // elle retrouve l'instance active via g_activeManager pour lui déléguer le vrai traitement.
  static void staticCallback(char* topic, byte* payload, unsigned int length){
    String message;
    for(unsigned int i = 0; i < length; i++) message += (char)payload[i];   // reconstruit la chaîne octet par octet
    if(g_activeManager && g_activeManager->registry){
      g_activeManager->registry->routeCommand(String(topic), message);      // délègue au registre le routage réel
    }
  }

  // Boucle bloquante de reconnexion : tant que non connecté, on retente toutes les 2 secondes.
  void reconnect(){
    while(!client.connected()){
      Serial.print("Connexion MQTT...");
      // Connexion avec ou sans identifiants, selon que MQTT_USER est vide ou non.
      bool ok = (strlen(username) > 0)
        ? client.connect(clientId.c_str(), username, password)
        : client.connect(clientId.c_str());

      if(ok){
        Serial.println("connecté !");
        client.subscribe(registry->subscriptionFilter().c_str());   // ex: teguis_dashboard_01/+/+/set

        // Republie l'état de tous les relais pour resynchroniser le dashboard
        registry->publishAllActuatorStates([this](const String& t, const String& v){
          this->publish(t, v);   // callback appelé pour chaque relais connu
        });
      } else {
        Serial.print("échec (code ");
        Serial.print(client.state());        // code d'erreur PubSubClient (ex: -2 = échec de connexion réseau)
        Serial.println("), nouvelle tentative dans 2s");
        delay(2000);   // attente avant nouvelle tentative, pour ne pas spammer le broker
      }
    }
  }
};

#endif   // fin de la garde d'inclusion
