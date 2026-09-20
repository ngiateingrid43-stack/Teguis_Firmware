#ifndef ACCESS_CONTROL_H
#define ACCESS_CONTROL_H

#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>
#include <functional>
#include "Keypad1x5.h"
#include "RFIDReader.h"

/**
 * AccessControl.h
 * ---------------------------------------------------------
 * DOUBLE AUTHENTIFICATION LOCALE : code (clavier 1x5) + badge RFID.
 * Accès accordé UNIQUEMENT si les DEUX facteurs sont validés dans
 * une même fenêtre de temps (VALIDATION_WINDOW_MS), dans n'importe
 * quel ordre — c'est la définition d'une double authentification :
 * ni le code seul, ni le badge seul, ne suffisent.
 *
 * FONCTIONNE SANS WI-FI NI MQTT. Même principe que "sécurité locale
 * avant tout réseau" déjà appliqué à SensorMQ2 (alarme gaz) : ce
 * module ne dépend d'AUCUNE connexion pour fonctionner. Le callback
 * `setEventCallback` est un BONUS optionnel pour reporter les
 * événements sur MQTT quand le réseau est disponible — jamais une
 * condition au fonctionnement local.
 *
 * PERSISTANCE EEPROM : codes et badge survivent aux redémarrages/coupures
 * de courant. L'EEPROM de l'ESP32 est en réalité une zone de la mémoire
 * flash émulée par la bibliothèque `EEPROM.h` — contrairement à un AVR,
 * il faut appeler `EEPROM.begin(taille)` une fois, et `EEPROM.commit()`
 * après chaque écriture pour que le changement soit réellement persisté
 * (sans ça, l'écriture reste seulement en RAM et se perd au redémarrage).
 * Un octet "magique" en adresse 0 distingue une EEPROM déjà initialisée
 * par ce firmware d'une flash vierge (qui contient des 0xFF aléatoires) —
 * sans lui, on lirait des codes corrompus au tout premier démarrage.
 *
 * Menu admin (atteint en tapant le code admin) :
 *   1 = changer le code admin   2 = changer le code utilisateur
 *   3 = enrôler un nouveau badge RFID   4 = quitter
 * ---------------------------------------------------------
 */

// Plan mémoire EEPROM (adresses en octets) :
#define EEPROM_SIZE        64
#define EEPROM_ADDR_MAGIC  0    // 1 octet : sentinelle d'initialisation
#define EEPROM_ADDR_USER   1    // 7 octets : code utilisateur (6 chiffres + '\0')
#define EEPROM_ADDR_ADMIN  8    // 7 octets : code admin (6 chiffres + '\0')
#define EEPROM_ADDR_BADGE  16   // 32 octets : UID du badge (chaîne hex + '\0')
#define EEPROM_MAGIC_VALUE 0xA5 // valeur arbitraire : si absente, l'EEPROM n'a jamais été écrite par ce firmware

enum AccessMode {
  MODE_NORMAL,
  MODE_ADMIN_MENU,
  MODE_SET_ADMIN_CODE,
  MODE_SET_USER_CODE,
  MODE_ENROLL_RFID
};

class AccessControl {
private:
  Keypad1x5 keypad;
  RFIDReader rfid;
  LiquidCrystal_I2C lcd;

  uint8_t rfidSckPin, rfidMisoPin, rfidMosiPin;
  int lockRelayPin;
  unsigned long unlockDurationMs;

  String codeUtilisateur;
  String codeAdmin;
  String codeSaisi = "";
  String badgeAutorise; // UID du badge utilisateur autorisé (un seul pour l'instant)

  AccessMode mode = MODE_NORMAL;

  bool codeValide = false;
  bool badgeValide = false;
  unsigned long codeValideAt = 0;
  unsigned long badgeValideAt = 0;

  unsigned long lockUntil = 0; // fin de la période de déverrouillage en cours

  static const unsigned long VALIDATION_WINDOW_MS = 15000; // 15s pour présenter le second facteur

  using EventFn = std::function<void(const String& evenement)>;
  EventFn onEvent;

public:
  AccessControl(int keypadAnalogPin, uint8_t rfidSs, uint8_t rfidRst,
                uint8_t rfidSck, uint8_t rfidMiso, uint8_t rfidMosi,
                int lockPin, const String& codeUserDefaut = "123412",
                const String& codeAdminDefaut = "444444",
                unsigned long dureeDeverouillageMs = 3000)
    : keypad(keypadAnalogPin), rfid(rfidSs, rfidRst), lcd(0x27, 16, 2),
      rfidSckPin(rfidSck), rfidMisoPin(rfidMiso), rfidMosiPin(rfidMosi),
      lockRelayPin(lockPin), unlockDurationMs(dureeDeverouillageMs),
      codeUtilisateur(codeUserDefaut), codeAdmin(codeAdminDefaut) {}

  void setEventCallback(EventFn cb){ onEvent = cb; }

  void begin(){
    keypad.begin();
    rfid.begin(rfidSckPin, rfidMisoPin, rfidMosiPin);
    lcd.init();
    lcd.backlight();
    pinMode(lockRelayPin, OUTPUT);
    digitalWrite(lockRelayPin, LOW);

    EEPROM.begin(EEPROM_SIZE);
    loadFromEeprom(); // écrase codeUtilisateur/codeAdmin/badgeAutorise si une sauvegarde existe déjà

    afficherEcranNormal();
  }

  /** A appeler à CHAQUE tour de loop(), indépendamment de l'état Wi-Fi/MQTT. */
  void loop(){
    unsigned long now = millis();

    if(lockUntil > 0 && now > lockUntil){
      digitalWrite(lockRelayPin, LOW);
      lockUntil = 0;
    }

    // Un facteur validé expire si le second n'arrive pas dans la fenêtre.
    if(codeValide && (now - codeValideAt > VALIDATION_WINDOW_MS)) codeValide = false;
    if(badgeValide && (now - badgeValideAt > VALIDATION_WINDOW_MS)) badgeValide = false;

    handleKeypad();
    handleRfid();
  }

private:
  void loadFromEeprom(){
    uint8_t magic = EEPROM.read(EEPROM_ADDR_MAGIC);
    if(magic != EEPROM_MAGIC_VALUE){
      // Première utilisation : flash vierge. On écrit les valeurs par défaut
      // (celles passées au constructeur) pour initialiser proprement l'EEPROM.
      saveCodesToEeprom();
      saveBadgeToEeprom();
      EEPROM.write(EEPROM_ADDR_MAGIC, EEPROM_MAGIC_VALUE);
      EEPROM.commit();
      return;
    }

    String savedUser = EEPROM.readString(EEPROM_ADDR_USER);
    String savedAdmin = EEPROM.readString(EEPROM_ADDR_ADMIN);
    String savedBadge = EEPROM.readString(EEPROM_ADDR_BADGE);

    if(savedUser.length() > 0) codeUtilisateur = savedUser;
    if(savedAdmin.length() > 0) codeAdmin = savedAdmin;
    badgeAutorise = savedBadge; // peut être vide si aucun badge encore enrôlé, c'est normal
  }

  void saveCodesToEeprom(){
    EEPROM.writeString(EEPROM_ADDR_USER, codeUtilisateur.c_str());
    EEPROM.writeString(EEPROM_ADDR_ADMIN, codeAdmin.c_str());
    EEPROM.commit(); // sans commit(), l'écriture reste en RAM et se perd au redémarrage
  }

  void saveBadgeToEeprom(){
    EEPROM.writeString(EEPROM_ADDR_BADGE, badgeAutorise.c_str());
    EEPROM.commit();
  }

  void handleRfid(){
    String uid = rfid.checkForCard();
    if(uid == "") return;

    if(mode == MODE_ENROLL_RFID){
      badgeAutorise = uid;
      saveBadgeToEeprom(); // persiste immédiatement, survit à un redémarrage/coupure de courant
      lcd.clear(); lcd.setCursor(0,0); lcd.print("BADGE ENREGISTRE");
      delay(1000);
      mode = MODE_NORMAL;
      afficherEcranNormal();
      return;
    }

    if(mode != MODE_NORMAL) return; // ignore les badges pendant les sous-menus

    if(badgeAutorise != "" && uid == badgeAutorise){
      badgeValide = true;
      badgeValideAt = millis();
      report("badge_ok");
      evaluerAcces();
    } else {
      lcd.clear(); lcd.setCursor(0,0); lcd.print("BADGE INCONNU");
      delay(800);
      report("badge_refuse");
      afficherEcranNormal();
    }
  }

  void handleKeypad(){
    int touche = keypad.readKey();
    if(touche == 0) return;

    switch(mode){
      case MODE_NORMAL:         handleNormalKey(touche); break;
      case MODE_ADMIN_MENU:     handleAdminMenuKey(touche); break;
      case MODE_SET_ADMIN_CODE: handleSetCodeKey(touche, true); break;
      case MODE_SET_USER_CODE:  handleSetCodeKey(touche, false); break;
      case MODE_ENROLL_RFID:
        if(touche == 4){ mode = MODE_NORMAL; afficherEcranNormal(); }
        break;
    }
  }

  void handleNormalKey(int touche){
    if(touche >= 1 && touche <= 4){
      if(codeSaisi.length() < 6){
        codeSaisi += String(touche);
        rafraichirEtoiles();
      }
      return;
    }

    // touche == 5 : valider le code saisi
    if(codeSaisi == codeUtilisateur){
      codeValide = true;
      codeValideAt = millis();
      report("code_ok");
      evaluerAcces();
    } else if(codeSaisi == codeAdmin){
      mode = MODE_ADMIN_MENU;
      afficherMenuAdmin();
    } else {
      lcd.clear(); lcd.setCursor(0,0); lcd.print("CODE INCORRECT");
      delay(800);
      report("code_refuse");
      afficherEcranNormal();
    }
    codeSaisi = "";
  }

  void evaluerAcces(){
    if(codeValide && badgeValide){
      codeValide = false;
      badgeValide = false;
      digitalWrite(lockRelayPin, HIGH);
      tone(23, 1000,500);  // change state of the LED by setting the pin to the HIGH voltage level                     
      lockUntil = millis() + unlockDurationMs;
      lcd.clear(); lcd.setCursor(0,0); lcd.print("ACCES ACCORDE");
      report("acces_accorde");
      delay(1000);
      afficherEcranNormal();
    } else {
      // Un seul facteur validé : on attend l'autre, sans redonner d'info
      // sur LEQUEL manque à un badge/code frauduleux serait une fuite mineure,
      // mais utile en usage normal pour guider l'utilisateur légitime.
      lcd.clear(); lcd.setCursor(0,0);
      lcd.print(codeValide ? "Code OK. Badge ?" : "Badge OK. Code ?");
      delay(800);
      afficherEcranNormal();
    }
  }

  void handleAdminMenuKey(int touche){
    codeSaisi = "";
    if(touche == 1){ mode = MODE_SET_ADMIN_CODE; afficherSaisie("Nouv. Code Admin"); }
    else if(touche == 2){ mode = MODE_SET_USER_CODE; afficherSaisie("Nouv. Code User"); }
    else if(touche == 3){
      mode = MODE_ENROLL_RFID;
      lcd.clear(); lcd.setCursor(0,0); lcd.print("Presenter badge");
      lcd.setCursor(0,1); lcd.print("4:Annuler");
    }
    else if(touche == 4){ mode = MODE_NORMAL; afficherEcranNormal(); }
  }

  void handleSetCodeKey(int touche, bool estAdmin){
    if(touche >= 1 && touche <= 4){
      if(codeSaisi.length() < 6){
        codeSaisi += String(touche);
        lcd.setCursor(1 + codeSaisi.length() - 1, 1);
        lcd.print(touche);
      }
      return;
    }
    // touche == 5 : enregistrer
    if(codeSaisi.length() == 6){
      if(estAdmin) codeAdmin = codeSaisi; else codeUtilisateur = codeSaisi;
      saveCodesToEeprom(); // persiste immédiatement, survit à un redémarrage/coupure de courant
      lcd.clear(); lcd.setCursor(0,0); lcd.print("CODE ENREGISTRE!");
    } else {
      lcd.clear(); lcd.setCursor(0,0); lcd.print("ERREUR: 6 Chifr.");
    }
    delay(1000);
    codeSaisi = "";
    mode = MODE_NORMAL;
    afficherEcranNormal();
  }

  void report(const String& evenement){
    if(onEvent) onEvent(evenement); // best-effort, jamais bloquant si le réseau est down
  }

  void afficherEcranNormal(){
    lcd.clear();
    lcd.setCursor(0,0); lcd.print("Code + Badge");
    lcd.setCursor(0,1); lcd.print(">");
  }

  void afficherSaisie(const char* titre){
    lcd.clear();
    lcd.setCursor(0,0); lcd.print(titre);
    lcd.setCursor(0,1); lcd.print(">");
  }

  void afficherMenuAdmin(){
    lcd.clear();
    lcd.setCursor(0,0); lcd.print("1:CodeAd 2:CodeUs");
    lcd.setCursor(0,1); lcd.print("3:Badge 4:Quitter");
  }

  void rafraichirEtoiles(){
    lcd.setCursor(1,1);
    for(unsigned int i = 0; i < codeSaisi.length(); i++) lcd.print("*");
  }
};

#endif
