# Architecture du firmware Teguis (ESP32)

```
Cette architechture a été Conçue par NGIATE KAMANANG INGRID dans de rester flexible pendant que on ajoutes des pièces/appareils, sans jamais devoir réécrire la logique centrale.
c'est un moteur solide pour la domotique et des projets de beaucoup plus grand comme des hotels connectees des motels connectes et bien d'autre Habitation
```

---

## 1. Structure des fichiers

```
teguis_firmware/
├── teguis_firmware.ino   → point d'entrée, orchestration minimale (setup/loop)
├── config.h              → TON installation : Wi-Fi, broker, liste des appareils par pièce
├── Device.h              → interface commune à tout appareil (le cœur de l'extensibilité)
├── Relay.h               → un type d'appareil concret : actionneur ON/OFF
├── SensorDHT.h           → un autre type concret : capteur température/humidité
├── SensorPIR             → un type concret : capteur de mouvement ou de presence
├── SensorMQ2             → un type concret : capteur de gaz
├── SensorContact         → un type concret : capteur de contact 
├── SensorCurrent.h       → un type concret : capteur de courant (estimation de puissance)
├── Keypad1x5.h           → lecture du clavier analogique 1x5 (contrôle d'accès)
├── RFIDReader.h          → lecture d'un lecteur RFID MFRC522 (contrôle d'accès)
├── AccessControl.h       → double authentification code + badge, 100% locale (voir section 8)
├── DeviceRegistry.h      → registre central, route les commandes et déclenche les publications
├── WifiManager.h         → encapsule la connexion Wi-Fi
└── MqttManager.h         → encapsule PubSubClient (connexion, reconnexion, callback)
```

---

## 2. Le principe de conception central : `Device` comme interface commune

Tout ce que tu branches (relais, capteur, et demain un variateur ou un détecteur de mouvement) implémente la même interface `Device` (voir `Device.h`). `DeviceRegistry`, `MqttManager` et le `.ino` principal ne manipulent **que** des `Device*` génériques — ils ne savent jamais s'ils ont affaire à un relais ou un capteur.

**Pourquoi c'est important pour toi concrètement :** ajouter un 7ème type d'appareil (ex: un servo-moteur pour un volet) ne demande de créer QU'UNE nouvelle classe `Volet.h` qui hérite de `Device` — zéro modification dans `DeviceRegistry.h`, `MqttManager.h` ou le `.ino`. C'est le pattern **Strategy/Polymorphisme** appliqué à ton problème.

## 3. Comment les commandes circulent (routage sans if/else géant)

Sans cette architecture, le réflexe naturel serait un immense `if(topic == "maison/salon/lumiere/set") ... else if(topic == "maison/chambre/lumiere/set") ...` qui grossit indéfiniment et devient impossible à maintenir avec 6 pièces et plusieurs appareils chacune.

Ici, `DeviceRegistry::routeCommand()` (dans `DeviceRegistry.h`) fait ça une seule fois, génériquement :
```cpp
void routeCommand(const String& topic, const String& payload){
  for(int i = 0; i < count; i++){
    if(!devices[i]->isActuator()) continue;
    String expected = topicPrefix + "/" + devices[i]->getTopicSuffix() + "/set";
    if(topic == expected){
      devices[i]->handleCommand(payload);
      return;
    }
  }
}
```
Chaque `Device` connaît son propre suffixe de topic (`getTopicSuffix()`) — le registre n'a besoin de rien savoir de plus.

## 4. Comment les publications sortent (capteurs ET relais)

`DeviceRegistry::pollAndPublish()` est appelée à chaque tour de `loop()`. Elle demande à chaque appareil `hasNewReading()` — pour un `Relay`, ça retourne toujours `false` (les relais ne publient que sur commande, pas périodiquement) ; pour un `SensorDHT`, ça retourne `true` toutes les 5 secondes (configurable).

**Point d'extension :** si demain tu veux qu'un relais publie AUSSI périodiquement son état (pas juste après une commande), tu changes uniquement `Relay::hasNewReading()` — rien ailleurs.

## 5. Pourquoi un seul `config.h` centralise toute ta maison

`registerAllDevices()` dans `config.h` est le SEUL endroit où tu listes tes 6 pièces et leurs appareils :
```cpp
registry.add(new Relay("salon", "lumiere", 26));
registry.add(new Relay("chambre", "lumiere", 27));
// ...
```
Ajouter une pièce ou un appareil = ajouter une ligne ici. Le reste du firmware (`DeviceRegistry`, `MqttManager`, le `.ino`) n'a jamais besoin d'être touché — exactement le même principe que `CONFIG.rooms` dans ton appli web, pour que les deux bases de code restent cohérentes dans leur philosophie.

---

## 6. Point de vigilance spécifique à la pièce "douche"

Une salle de douche/bain a des règles électriques particulières (zones/volumes de sécurité autour du point d'eau, indices de protection IP minimum requis pour tout matériel électrique à proximité). Le commentaire dans `config.h` te rappelle ce point, mais concrètement :
- **Place le relais lui-même en dehors de la douche** (dans un couloir, un placard technique voisin) — seul le câblage vers la charge (lumière, ventilateur d'extraction) doit entrer dans la pièce humide
- Si tu places un capteur (température/humidité) dans la douche, vérifie qu'il a un indice de protection adapté (IP44 minimum, IP65 si proche des projections d'eau directes)
- Renseigne-toi sur la norme électrique en vigueur pour les pièces d'eau avant de câbler quoi que ce soit de réel dans cette pièce — c'est un point que ton superviseur ([[dr-bilon]]) voudra probablement voir documenté dans ton rapport

## 7. Sécurité MQTT côté firmware

`config.h` a des champs `MQTT_USER`/`MQTT_PASSWORD` et un `MQTT_PORT` par défaut à 1883 (non chiffré) pour simplifier les premiers tests sur Wokwi. **Avant tout déploiement réel :**
- Passe à `MQTT_PORT 8883` avec un broker qui supporte TLS (nécessite `WiFiClientSecure` à la place de `WiFiClient` dans `MqttManager.h` — pas encore fait dans ce squelette, à ajouter quand tu passes en production)
- Renseigne un vrai couple utilisateur/mot de passe (jamais de broker public en usage réel, comme déjà vu pour ton appli web)

---

## 8. Double authentification locale (code clavier + badge RFID)

Trois nouveaux fichiers : `Keypad1x5.h`, `RFIDReader.h`, `AccessControl.h`.

**Pourquoi ce module n'est PAS un `Device` comme les autres :** un `Device` publie une valeur ou réagit à UNE commande simple. Le contrôle d'accès a un vrai état interne (code en cours de saisie, menu admin, fenêtre de validation à deux facteurs) qui ne se résume pas à "lire une broche" ou "écrire ON/OFF" — il mérite sa propre classe, appelée directement depuis `teguis_firmware.ino`, pas via `DeviceRegistry`.

### Le principe : vraie double authentification, pas juste deux méthodes au choix

Code ET badge sont tous les deux OBLIGATOIRES (dans n'importe quel ordre, avec 15 secondes pour présenter le second facteur) — ni l'un ni l'autre seul ne suffit. C'est la différence entre "deux façons d'entrer" et une vraie 2FA.

### Fonctionne SANS réseau, par conception

`AccessControl::loop()` tourne en premier dans `loop()`, avant même la gestion Wi-Fi/MQTT — la serrure doit s'ouvrir même si le broker est injoignable. Le callback `setEventCallback()` reporte les événements (`code_ok`, `badge_ok`, `acces_accorde`, etc.) sur MQTT **si** connecté, mais rien ne bloque si ce n'est pas le cas — même philosophie que l'alarme locale de `SensorMQ2`.

### Menu admin (atteint en tapant le code admin puis OK)

```
1 = changer le code admin       2 = changer le code utilisateur
3 = enrôler un nouveau badge RFID   4 = quitter
```

### Persistance EEPROM (codes et badge survivent aux redémarrages)

Les codes utilisateur/admin et l'UID du badge enrôlé sont sauvegardés dans l'EEPROM de l'ESP32 (en réalité une zone de flash émulée par la bibliothèque `EEPROM.h`) — plus besoin de tout ré-enrôler après une coupure de courant ou un reset.

- Un octet "magique" en adresse 0 détecte le tout premier démarrage (flash vierge) et initialise l'EEPROM avec les valeurs par défaut du constructeur
- Chaque changement de code (menu admin) et chaque enrôlement de badge déclenche immédiatement `EEPROM.commit()` — sans cet appel, l'écriture resterait seulement en RAM et se perdrait au redémarrage
- Plan mémoire (voir les `#define EEPROM_ADDR_*` en tête de `AccessControl.h`) : magique=octet 0, code utilisateur=octets 1-7, code admin=octets 8-14, badge=octets 16-47

**Pour repartir de zéro** (codes/badge oubliés ou EEPROM corrompue) : le plus simple est de flasher une fois un croquis minimal qui appelle `EEPROM.write(0, 0); EEPROM.commit();` avant de reflasher le firmware normal — ça invalide l'octet magique et force une réinitialisation aux valeurs par défaut au prochain démarrage.

### Bibliothèques à installer

- `MFRC522` (GithubCommunity / miguelbalboa) — via Library Manager
- `LiquidCrystal_I2C` — probablement déjà installée si tu as testé `clavier1x5ESP32.ino`

### Câblage (voir aussi le plan complet dans `config.h`)

| Composant | Broches |
|---|---|
| Clavier 1x5 | Analogique → GPIO 35 |
| RFID MFRC522 | SCK=18 · MOSI=4 · MISO=34 · SS=13 · RST=25 |
| Serrure/gâche | GPIO 15 (broche de strapping — teste bien qu'aucun état de repos ne perturbe le démarrage) |

**Point technique important :** sur ESP32, le bus SPI se reroute sur n'importe quelles broches via la matrice GPIO (contrairement à un AVR classique) — c'est pourquoi ces broches SPI ont pu être choisies pour éviter tout conflit avec le reste de ton câblage, plutôt que d'imposer les broches VSPI par défaut (18/19/23) qui, elles, entraient en conflit avec des relais déjà utilisés.

### Limite connue à garder en tête

Les écrans LCD de confirmation utilisent des `delay()` courts (800 ms-1,2 s) pendant un événement d'authentification, comme dans ton croquis d'origine — pendant ce court instant, la boucle MQTT est mise en pause. Sans impact pratique pour quelques authentifications par jour ; à revoir seulement si tu observes des déconnexions MQTT liées à un usage très fréquent.

---

## 9. Prochaine étape suggérée

Une fois cette architecture claire, tu peux commencer à coder dans cet ordre (cohérent avec le guide de test sans ESP32 déjà vu) :
1. Vérifie que `teguis_firmware.ino` compile dans l'IDE Arduino ou Wokwi (même sans matériel, juste pour valider la syntaxe C++)
2. Teste `WifiManager` seul (juste la connexion Wi-Fi, regarde le moniteur série)
3. Teste `MqttManager` seul (connexion + un seul relais déclar dans `config.h`)
4. Ajoute progressivement le reste de tes appareils, un par un, en testant chaque ajout avec le client HiveMQ WebSocket avant de passer au suivant

## Devellopé par
``` 
 NGIATE KAMANNG INGRID
 Membre de Rihen
```
