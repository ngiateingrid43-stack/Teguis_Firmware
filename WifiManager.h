#ifndef WIFI_MANAGER_H   // garde d'inclusion
#define WIFI_MANAGER_H

#include <WiFi.h>   // bibliothèque Wi-Fi officielle de l'ESP32 (fournie par le core Arduino-ESP32)

/**
 * WifiManager.h
 * ---------------------------------------------------------
 * Isole tout ce qui concerne le Wi-Fi. Si demain tu changes
 * de méthode (WiFiManager avec portail captif pour configurer
 * le Wi-Fi sans recompiler, par exemple), seul ce fichier change.
 * ---------------------------------------------------------
 */

class WifiManager {
private:
  const char* ssid;       // nom du réseau Wi-Fi à rejoindre
  const char* password;   // mot de passe de ce réseau

public:
  // Constructeur : mémorise juste les identifiants, ne se connecte pas encore.
  WifiManager(const char* wifiSsid, const char* wifiPassword)
    : ssid(wifiSsid), password(wifiPassword) {}

  // Lance la connexion Wi-Fi et BLOQUE tant qu'elle n'a pas réussi.
  void begin(){
    Serial.print("Connexion Wi-Fi");
    WiFi.mode(WIFI_STA);           // mode "station" : l'ESP32 rejoint un réseau existant (pas un point d'accès)
    WiFi.begin(ssid, password);    // démarre la tentative de connexion
    while(WiFi.status() != WL_CONNECTED){   // tant que non connecté...
      delay(300);                            // ...on attend un peu...
      Serial.print(".");                     // ...et on affiche un point de progression sur le port série
    }
    Serial.println(" connecté ! IP: " + WiFi.localIP().toString());   // adresse IP locale obtenue, utile pour le debug
  }

  // Vrai si le Wi-Fi est actuellement connecté.
  bool isConnected(){
    return WiFi.status() == WL_CONNECTED;
  }

  /** A appeler dans loop() si tu veux une reconnexion Wi-Fi automatique */
  void ensureConnected(){
    if(!isConnected()) begin();   // si la connexion est tombée, on relance une connexion complète
  }
};

#endif   // fin de la garde d'inclusion
