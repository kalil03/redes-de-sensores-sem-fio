#pragma once
#include <Arduino.h>
#if MEU_ID < 0 || MEU_ID > 2
#error "MEU_ID deve ser 0, 1 ou 2"
#endif
#include "config.h"
void trataPacote(byte *buf);
#include "enlace.h"
#if SENSOR_HABILITADO
#include "hcsr04.h"
#endif

#if MEU_ID == 0
void registraAplicacao(byte origem, byte seq, byte valor, bool entregue) {
  bool sensor = origem == ID_N1 || (origem == ID_N2 && NO2_TEM_SENSOR);
  Serial.print(F("APP {\"evento\":\"leitura\",\"no\":")); Serial.print(origem);
  Serial.print(F(",\"seq\":")); Serial.print(seq);
  Serial.print(F(",\"tipo\":\"")); Serial.print(sensor ? F("distancia") : F("teste"));
  Serial.print(F("\",\"valor\":")); Serial.print(valor);
  Serial.print(F(",\"entregue\":")); Serial.print(entregue ? F("true") : F("false"));
  Serial.print(F(",\"t_ms\":")); Serial.print(millis()); Serial.println('}');
}

void trataPacote(byte *buf) {
  byte orig  = origemDe(buf);
  byte valor = valorDe(buf);

  if (tipoDe(buf) == T_ENVIA && orig != ID_GW) {
    byte outro = (orig == ID_N1) ? ID_N2 : ID_N1;
    Serial.print(F("No ")); Serial.print(orig);
    Serial.print(F(" -> no ")); Serial.print(outro);
    Serial.print(F(": valor ")); Serial.println(valor);

    bool entregue = enviaComAck(outro, T_REPASSE, valor);
    if (entregue) {
      st.entr++;
      Serial.println(F("  entregue! avisando quem mandou"));
      enviaComAck(orig, T_ENTREGUE, valor);
    } else {
      st.naoEntr++;
      Serial.println(F("  NAO entregue! avisando quem mandou"));
      enviaComAck(orig, T_NAO_ENTREGUE, valor);
    }
    registraAplicacao(orig, seqDe(buf), valor, entregue);
  }
}

#else
#define OUTRO_NO        ((MEU_ID == ID_N1) ? ID_N2 : ID_N1)
#define TIMEOUT_ENTREGA 1500

bool aguardandoEntrega = false;
byte valorAguardando   = 0;
unsigned long tEnvio   = 0;

void trataPacote(byte *buf) {
  if (origemDe(buf) != ID_GW) return;
  byte valor = valorDe(buf);

  switch (tipoDe(buf)) {
    case T_REPASSE:
      Serial.print(F("Recebi do no ")); Serial.print(OUTRO_NO);
      Serial.print(F(" (via gateway): valor ")); Serial.println(valor);
      break;
    case T_ENTREGUE:
      st.entr++;
      Serial.print(F("CONFIRMADO: no ")); Serial.print(OUTRO_NO);
      Serial.print(F(" recebeu o valor ")); Serial.println(valor);
      if (valor == valorAguardando) aguardandoEntrega = false;
      break;
    case T_NAO_ENTREGUE:
      st.naoEntr++;
      Serial.print(F("NAO ENTREGUE: no ")); Serial.print(OUTRO_NO);
      Serial.print(F(" nao recebeu o valor ")); Serial.println(valor);
      if (valor == valorAguardando) aguardandoEntrega = false;
      break;
  }
}

#endif

void setup() {
  configuraRadio();
#if SENSOR_HABILITADO
  configuraSensor();
#endif
}

void loop() {
#if MEU_ID == 0
  processaRadio();
#else
#if SENSOR_HABILITADO
  processaSensor();
#endif
  processaRadio();

  if (aguardandoEntrega && millis() - tEnvio > TIMEOUT_ENTREGA) {
    st.semConf++;
    Serial.print(F("SEM CONFIRMACAO de entrega do valor ")); Serial.println(valorAguardando);
    aguardandoEntrega = false;
  }

  static unsigned long tUltimo = 0;
  static byte contador = 0;
  if (millis() - tUltimo > 3000 + MEU_ID * 700) {
    tUltimo = millis();
    byte valor;
#if SENSOR_HABILITADO
    valor = valorSensorParaRadio();
    Serial.print(F("Enviando distancia_2cm=")); Serial.print(valor);
#else
    valor = contador;
    Serial.print(F("Enviando teste=")); Serial.print(valor);
#endif
    Serial.print(F(" ao no ")); Serial.print(OUTRO_NO); Serial.println(F(" via gateway"));
    if (enviaComAck(ID_GW, T_ENVIA, valor)) {
      aguardandoEntrega = true;
      valorAguardando   = valor;
      tEnvio            = millis();
    }
    contador++;
  }
#endif
}
