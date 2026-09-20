// SensorCurrent.h
#ifndef SENSOR_CURRENT_H
#define SENSOR_CURRENT_H

#include "Device.h"

/**
 * SensorCurrent.h
 * ---------------------------------------------------------
 * Capteur de courant analogique à effet Hall type ACS712 (ou
 * équivalent). Un ACS712 sort une tension proportionnelle au
 * courant AC/DC traversant son fil, centrée sur Vcc/2 au repos.
 *
 * Comme un ACS712 bas de gamme n'est PAS un vrai wattmètre RMS,
 * ce capteur estime la puissance ainsi :
 *   1. échantillonne la tension de sortie pendant ~40ms (2
 *      périodes secteur à 50Hz) pour capturer un cycle complet
 *   2. calcule l'amplitude crête-à-crête autour du point milieu
 *   3. convertit en ampères RMS via la sensibilité du capteur
 *      (mV/A, dépend du modèle : 185/100/66 mV/A pour les
 *      versions 5A/20A/30A)
 *   4. multiplie par la tension secteur supposée pour obtenir
 *      des Watts
 *
 * C'est une ESTIMATION, pas une mesure de précision (pas de
 * vrai calcul RMS, pas de mesure de facteur de puissance) —
 * largement suffisant pour un bilan de consommation domestique
 * indicatif, pas pour de la facturation.
 * ---------------------------------------------------------
 */

class SensorCurrent : public Device {
private:
  String topicSuffix;
  int pin;                    // broche analogique (ADC1 recommandé : 32-39)
  float mVperAmp;              // sensibilité du capteur (voir datasheet ACS712)
  float mainsVoltage;          // tension secteur supposée pour le calcul de puissance (ex: 220V)
  float zeroOffsetV;            // tension de repos mesurée (calibrée au démarrage, proche de Vcc/2)
  unsigned long intervalMs;
  unsigned long lastRead = 0;
  float lastWatts = 0;

  static const int ADC_MAX = 4095;     // résolution ADC 12 bits de l'ESP32
  static const int ADC_VREF_MV = 3300; // tension de référence ADC, en mV (approximatif sans étalonnage matériel)

public:
  SensorCurrent(const String& piece, const String& appareil, int analogGpio,
                float sensitivityMvPerAmp = 100.0, float assumedMainsVoltage = 220.0,
                unsigned long intervalMillis = 3000)
    : topicSuffix(piece + "/" + appareil), pin(analogGpio),
      mVperAmp(sensitivityMvPerAmp), mainsVoltage(assumedMainsVoltage),
      intervalMs(intervalMillis) {}

  String getTopicSuffix() const override { return topicSuffix; }

  void begin() override {
    pinMode(pin, INPUT);
    // Calibration du zéro : suppose qu'AUCUN courant ne circule au démarrage.
    // Si ce n'est pas garanti chez toi (appareil déjà branché et actif au
    // boot), remplace cette ligne par une valeur fixe mesurée au multimètre.
    long total = 0;
    const int samples = 200;
    for(int i = 0; i < samples; i++){ total += analogRead(pin); delay(1); }
    zeroOffsetV = (total / (float)samples) * (ADC_VREF_MV / (float)ADC_MAX) / 1000.0;
  }

  bool isSensor() const override { return true; }

  bool hasNewReading() override {
    unsigned long now = millis();
    if(now - lastRead < intervalMs) return false;
    lastRead = now;

    // Echantillonnage sur ~40ms pour capturer au moins un cycle secteur complet (50Hz = 20ms/cycle).
    int minRaw = ADC_MAX, maxRaw = 0;
    unsigned long start = millis();
    while(millis() - start < 40){
      int raw = analogRead(pin);
      if(raw < minRaw) minRaw = raw;
      if(raw > maxRaw) maxRaw = raw;
    }

    // Amplitude crête-à-crête convertie en volts, puis en ampères RMS approximatifs.
    float peakToPeakV = (maxRaw - minRaw) * (ADC_VREF_MV / (float)ADC_MAX) / 1000.0;
    float amplitudeV = peakToPeakV / 2.0;                 // demi-amplitude = valeur crête
    float amps = (amplitudeV * 1000.0 / mVperAmp) / 1.41421356; // crête → RMS (÷√2)
    if(amps < 0.05) amps = 0;                              // bruit de fond en dessous de 50mA, on arrondit à zéro

    lastWatts = amps * mainsVoltage;
    return true;
  }

  // Publie directement la puissance en Watts (une décimale) — c'est ce que
  // l'app affiche et intègre dans le temps pour estimer les kWh consommés.
  String readStatus() override {
    return String(lastWatts, 1);
  }
};

#endif
