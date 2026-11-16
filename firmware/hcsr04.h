#pragma once
#include <Arduino.h>
#include <util/atomic.h>

static const byte SENSOR_TRIG = 3;
static const byte SENSOR_ECHO = 2;
static const unsigned long SENSOR_INTERVALO_MS = 200;
static const unsigned long SENSOR_TIMEOUT_US = 30000;
static volatile bool sensorArmado = false;
static volatile bool sensorSubiu = false;
static volatile bool sensorPronto = false;
static volatile unsigned long sensorInicioEcho = 0;
static volatile unsigned long sensorDuracao = 0;
static bool sensorMedindo = false;
static unsigned long sensorDisparoUs = 0;
static unsigned long sensorUltimoMs = 0;
static byte sensorValorRadio = 0;
static unsigned long sensorLeituraMs = 0;

// 1..200 = passos de 20 mm; 0 = leitura inválida ou antiga.
static byte valorSensorParaRadio() {
  if (millis() - sensorLeituraMs > 2000) return 0;
  return sensorValorRadio;
}

static void sensorBordaEcho() {
  if (!sensorArmado) return;
  unsigned long agora = micros();
  if (digitalRead(SENSOR_ECHO)) {
    if (!sensorSubiu) {
      sensorInicioEcho = agora;
      sensorSubiu = true;
    }
  } else if (sensorSubiu) {
    sensorDuracao = agora - sensorInicioEcho;
    sensorPronto = true;
    sensorArmado = false;
  }
}

static void configuraSensor() {
  pinMode(SENSOR_TRIG, OUTPUT);
  digitalWrite(SENSOR_TRIG, LOW);
  pinMode(SENSOR_ECHO, INPUT);
  attachInterrupt(digitalPinToInterrupt(SENSOR_ECHO), sensorBordaEcho, CHANGE);
  Serial.print(F("SENSOR_CONFIG no=")); Serial.print(MEU_ID);
  Serial.println(F(" trig=D3 echo=D2 alimentacao=5V"));
}

static void processaSensor() {
  if (sensorMedindo) {
    bool pronto;
    unsigned long duracao;
    bool timeout = micros() - sensorDisparoUs >= SENSOR_TIMEOUT_US;

    // AVR: copiar o tempo de 32 bits sem interferência da ISR.
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
      pronto = sensorPronto;
      duracao = sensorDuracao;
      if (pronto || timeout) sensorArmado = false;
    }
    if (!pronto && !timeout) return;
    sensorMedindo = false;
    sensorLeituraMs = millis();
    sensorValorRadio = 0;
    Serial.print(F("SENSOR no=")); Serial.print(MEU_ID);
    Serial.print(F(" estado="));
    if (!pronto) {
      Serial.println(F("sem_echo"));
    } else if (duracao == 0 || duracao > SENSOR_TIMEOUT_US) {
      Serial.println(F("pulso_invalido"));
    } else {
      unsigned long mm = duracao * 343UL / 2000UL;
      if (mm >= 20 && mm <= 4000) sensorValorRadio = (mm + 10) / 20;
      Serial.print(F("ok distancia_mm="));
      Serial.print(mm);
      Serial.print(F(" tempo_us=")); Serial.println(duracao);
    }
    return;
  }
  unsigned long agora = millis();
  if (agora - sensorUltimoMs < SENSOR_INTERVALO_MS) return;
  sensorUltimoMs = agora;
  sensorDisparoUs = micros();
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    sensorSubiu = false;
    sensorPronto = false;
    sensorArmado = true;
  }
  sensorMedindo = true;

  digitalWrite(SENSOR_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(SENSOR_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(SENSOR_TRIG, LOW);
}
