#pragma once

#define NET_ID 0xA7
#define CANAL 103
#define PA_NIVEL RF24_PA_LOW
#define CE_PIN 7
#define CSN_PIN 8
#define TIMEOUT_CTS 30
#define RESERVA_MS 90
#define TIMEOUT_ACK 30
#define MAX_TENTATIVAS 5
#define BACKOFF_MIN 5
#define BACKOFF_MAX 20
#define MAX_ESPERA_CANAL 10
#define CS_ESPERA_MIN 2
#define CS_ESPERA_MAX 10
#define PERIODO_STATS 10000
#define DEBUG_RADIO 1

#ifndef NO2_TEM_SENSOR
#define NO2_TEM_SENSOR 0 // Ativar só depois de instalar o segundo HC-SR04.
#endif
#define SENSOR_HABILITADO (MEU_ID == 1 || (MEU_ID == 2 && NO2_TEM_SENSOR))
