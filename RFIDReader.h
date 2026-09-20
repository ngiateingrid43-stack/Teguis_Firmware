#ifndef RFID_READER_H
#define RFID_READER_H

#include <SPI.h>
#include <MFRC522.h>

/**
 * RFIDReader.h
 * ---------------------------------------------------------
 * Encapsule un lecteur RFID MFRC522 (module SPI 13.56MHz, très
 * répandu et bon marché — badges/tags type MIFARE Classic).
 *
 * Bibliothèque nécessaire : "MFRC522" par GithubCommunity /
 * miguelbalboa, installable via le Library Manager de l'IDE Arduino.
 *
 * Sur ESP32, le bus SPI peut être routé sur N'IMPORTE QUELLES
 * broches via la matrice GPIO (contrairement à l'AVR classique) —
 * c'est pourquoi `begin()` prend explicitement SCK/MISO/MOSI en
 * paramètre : ça permet de choisir des broches qui n'entrent pas
 * en conflit avec le reste de ton installation (voir config.h).
 * ---------------------------------------------------------
 */

class RFIDReader {
private:
  MFRC522 mfrc522;
  uint8_t ssPin;
  String lastUid;
  unsigned long lastSeenAt = 0;
  static const unsigned long DEBOUNCE_MS = 2000; // évite de relire le même badge en boucle s'il reste posé sur le lecteur

public:
  RFIDReader(uint8_t ss, uint8_t rst) : mfrc522(ss, rst), ssPin(ss) {}

  void begin(uint8_t sckPin, uint8_t misoPin, uint8_t mosiPin){
    SPI.begin(sckPin, misoPin, mosiPin, ssPin);
    mfrc522.PCD_Init();
  }

  /**
   * Retourne l'UID (format hex, ex: "3F A2 9B 01") d'un badge
   * NOUVELLEMENT présenté, ou une chaîne vide si rien de neuf.
   * Non bloquant : renvoie immédiatement si aucun badge n'est là.
   */
  String checkForCard(){
    if(!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()){
      return "";
    }

    String uid = "";
    for(byte i = 0; i < mfrc522.uid.size; i++){
      if(mfrc522.uid.uidByte[i] < 0x10) uid += "0";
      uid += String(mfrc522.uid.uidByte[i], HEX);
      if(i < mfrc522.uid.size - 1) uid += " ";
    }
    uid.toUpperCase();

    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();

    unsigned long now = millis();
    if(uid == lastUid && (now - lastSeenAt) < DEBOUNCE_MS){
      return ""; // même badge relu trop vite (encore posé sur le lecteur), ignoré
    }
    lastUid = uid;
    lastSeenAt = now;
    return uid;
  }
};

#endif
