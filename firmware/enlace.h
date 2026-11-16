#pragma once
#include <RF24.h>
#include <RF24_config.h>
#include <nRF24L01.h>
#include <printf.h>

RF24 radio(CE_PIN, CSN_PIN);

uint64_t address[2] = { 0x3030303030LL, 0x3030303030LL };

// Byte VALOR do RTS/CTS contém a duração da reserva em ms.

#define ID_GW 0
#define ID_N1 1
#define ID_N2 2

#define T_ACK          2
#define T_ENVIA        3
#define T_REPASSE      4
#define T_ENTREGUE     5
#define T_NAO_ENTREGUE 6
#define T_RTS          7
#define T_CTS          8

// Quadro: rede, origem/destino, tipo/seq, valor, CRC (5 bytes).
byte meuSeq[3]    = { 0, 0, 0 };
byte ultimoSeq[3] = { 0xFF, 0xFF, 0xFF };

byte pendente[5];
bool temPendente = false;

struct {
  unsigned long env;
  unsigned long ack;
  unsigned long falha;
  unsigned long retx;
  unsigned long entr;
  unsigned long naoEntr;
  unsigned long semConf;
  unsigned long ocup;
  unsigned long dup;
  unsigned long crc;
  unsigned long alheio;
  unsigned long txErro;
  unsigned long rtsTx, ctsTx, ctsRx, ctsTimeout;
  unsigned long nav, csAbort, semReserva;
} st;

// CRC-8: polinômio 0x07, inicial 0xA7, bytes 0..3.
byte crc8(byte *d, byte n) {
  byte crc = NET_ID;
  for (byte i = 0; i < n; i++) {
    crc ^= d[i];
    for (byte b = 0; b < 8; b++)
      crc = (crc & 0x80) ? (crc << 1) ^ 0x07 : (crc << 1);
  }
  return crc;
}

void montaPacote(byte *p, byte destino, byte tipo, byte seq, byte valor) {
  p[0] = NET_ID;
  p[1] = (MEU_ID << 4) | (destino & 0x0F);
  p[2] = (tipo << 4) | (seq & 0x0F);
  p[3] = valor;
  p[4] = crc8(p, 4);
}

byte origemDe(byte *p) { return p[1] >> 4; }
byte tipoDe(byte *p)   { return p[2] >> 4; }
byte seqDe(byte *p)    { return p[2] & 0x0F; }
byte valorDe(byte *p)  { return p[3]; }

void mostraPacote(const __FlashStringHelper *rotulo, byte *p) {
#if DEBUG_RADIO
  Serial.print(rotulo);
  for (byte i = 0; i < 5; i++) {
    Serial.print(' ');
    if (p[i] < 0x10) Serial.print('0');
    Serial.print(p[i], HEX);
  }
  if (p[0] != NET_ID)              Serial.println(F("  (outra equipe)"));
  else if (crc8(p, 4) != p[4])     Serial.println(F("  (CRC errado)"));
  else {
    Serial.print(F("  de ")); Serial.print(p[1] >> 4);
    Serial.print(F(" p/ "));  Serial.print(p[1] & 0x0F);
    Serial.print(F(" tipo ")); Serial.print(p[2] >> 4);
    Serial.print(F(" seq "));  Serial.println(p[2] & 0x0F);
  }
#endif
}

void campo(const __FlashStringHelper *nome, unsigned long v) {
  Serial.print(' '); Serial.print(nome); Serial.print('='); Serial.print(v);
}

void mostraStats() {
  static unsigned long tUltimo = 0;
  if (millis() - tUltimo < PERIODO_STATS) return;
  tUltimo = millis();
  Serial.print(F("STATS no=")); Serial.print(MEU_ID);
  campo(F("t"), millis() / 1000);
  campo(F("env"), st.env);         campo(F("ack"), st.ack);
  campo(F("falha"), st.falha);     campo(F("retx"), st.retx);
  campo(F("entr"), st.entr);       campo(F("naoentr"), st.naoEntr);
  campo(F("semconf"), st.semConf); campo(F("ocup"), st.ocup);
  campo(F("dup"), st.dup);         campo(F("crc"), st.crc);
  campo(F("alheio"), st.alheio);   campo(F("txerr"), st.txErro);
  campo(F("rts"), st.rtsTx); campo(F("cts_tx"), st.ctsTx);
  campo(F("cts_rx"), st.ctsRx); campo(F("cts_timeout"), st.ctsTimeout);
  campo(F("nav"), st.nav); campo(F("cs_abort"), st.csAbort);
  campo(F("sem_reserva"), st.semReserva);
  Serial.println();

  if (!radio.isChipConnected()) Serial.println(F("ERRO: radio nao responde no SPI (fio? alimentacao?)"));
}

byte reservaOrigem = 0xFF, reservaDestino = 0xFF, reservaSeq = 0;
unsigned long reservaAte = 0;
bool txEmCurso = false;

bool reservaAtiva() {
  if (reservaOrigem != 0xFF && (long)(millis() - reservaAte) >= 0) reservaOrigem = 0xFF;
  return reservaOrigem != 0xFF;
}
bool mesmaReserva(byte origem, byte destino, byte seq) {
  return reservaAtiva() && reservaOrigem == origem && reservaDestino == destino && reservaSeq == seq;
}
void reservaMeio(byte origem, byte destino, byte seq, byte ms) {
  reservaOrigem = origem; reservaDestino = destino; reservaSeq = seq;
  reservaAte = millis() + ms;
}
void liberaMinhaReserva() {
  if (reservaAtiva() && reservaOrigem == MEU_ID) reservaOrigem = 0xFF;
}
bool tipoDados(byte tipo) { return tipo >= T_ENVIA && tipo <= T_NAO_ENTREGUE; }

void observaReserva(byte *p) {
  byte orig = origemDe(p), dest = p[1] & 0x0F, seq = seqDe(p), tipo = tipoDe(p);
  if (tipo == T_ACK && mesmaReserva(dest, orig, seq)) {
    reservaOrigem = 0xFF;
  } else if (dest != MEU_ID && orig != MEU_ID) {
    if (tipo == T_RTS && p[3] == RESERVA_MS &&
        (!reservaAtiva() || mesmaReserva(orig, dest, seq))) {
      reservaMeio(orig, dest, seq, TIMEOUT_CTS);
    } else if (tipo == T_CTS && p[3] == RESERVA_MS &&
               (!reservaAtiva() || mesmaReserva(dest, orig, seq))) {
      reservaMeio(dest, orig, seq, p[3]);
    }
  }
}

bool recebePacote(byte *buf, unsigned long timeout) {
  unsigned long t0 = millis();
  do {
    if (radio.available()) {
      radio.read(buf, 5);
      mostraPacote(F("RX:"), buf);
      if (buf[0] != NET_ID) { st.alheio++; continue; }
      if (crc8(buf, 4) != buf[4]) { st.crc++; continue; }
      byte orig = origemDe(buf), dest = buf[1] & 0x0F, tipo = tipoDe(buf);
      if (orig > ID_N2 || dest > ID_N2 || orig == dest ||
          !(tipoDados(tipo) || tipo == T_ACK || tipo == T_RTS || tipo == T_CTS)) continue;
      observaReserva(buf);
      if (dest == MEU_ID && orig != MEU_ID) return true;
    }
  } while (millis() - t0 < timeout);
  return false;
}

bool canalOcupado() {
  radio.stopListening();
  radio.startListening();
  delayMicroseconds(200);
  return radio.testRPD();
}

bool aguarda(unsigned long ms, byte destino, byte seq, byte tipoEsperado);

bool enviaPacote(byte *p, bool usarCS) {
  if (usarCS) {
    bool livre = false;
    for (byte i = 0; i < MAX_ESPERA_CANAL; i++) {
      bool nav = reservaAtiva();
      if (!nav && !canalOcupado()) { livre = true; break; }
      st.ocup++;
      if (nav) st.nav++;
      Serial.println(nav ? F("  meio reservado -> aguardando") : F("  canal ocupado -> aguardando"));
      aguarda(random(CS_ESPERA_MIN, CS_ESPERA_MAX), 0xFF, 0, 0);
    }
    if (!livre) { st.csAbort++; return false; }
  }
  radio.stopListening();
  bool saiu = radio.write(p, 5);
  radio.startListening();
  mostraPacote(F("TX:"), p);
  if (!saiu) {
    st.txErro++;
    Serial.println(F("  ERRO: o radio nao transmitiu"));
  }
  return saiu;
}

bool enviaControle(byte destino, byte tipo, byte seq, byte valor) {
  byte p[5]; montaPacote(p, destino, tipo, seq, valor);
  bool saiu = enviaPacote(p, false);
  if (saiu && tipo == T_CTS) st.ctsTx++;
  return saiu;
}
void enviaAck(byte destino, byte seq) { enviaControle(destino, T_ACK, seq, 0); }

void confirmaPacote(byte *buf) {
  byte orig = origemDe(buf), seq = seqDe(buf);
  if (!mesmaReserva(orig, MEU_ID, seq)) { st.semReserva++; return; }
  if (seq == ultimoSeq[orig]) {
    enviaAck(orig, seq);
    reservaOrigem = 0xFF;
    st.dup++;
    Serial.println(F("  repetido: ACK reenviado, pacote ignorado"));
    return;
  }
  if (temPendente) return;
  memcpy(pendente, buf, 5);
  ultimoSeq[orig] = seq;
  temPendente = true;
  enviaAck(orig, seq);
  reservaOrigem = 0xFF;
}

bool atendePacote(byte *buf, byte destino, byte seq, byte tipoEsperado) {
  byte tipo = tipoDe(buf), orig = origemDe(buf), recebidoSeq = seqDe(buf);
  if (tipo == T_RTS) {

    if (buf[3] != RESERVA_MS || txEmCurso ||
        (temPendente && recebidoSeq != ultimoSeq[orig]) ||
        (reservaAtiva() && !mesmaReserva(orig, MEU_ID, recebidoSeq))) return false;
    reservaMeio(orig, MEU_ID, recebidoSeq, RESERVA_MS);
    enviaControle(orig, T_CTS, recebidoSeq, RESERVA_MS);
    return false;
  }
  if (tipo == tipoEsperado && orig == destino && recebidoSeq == seq) {
    if (tipo == T_CTS && txEmCurso && buf[3] == RESERVA_MS &&
        (!reservaAtiva() || mesmaReserva(MEU_ID, destino, seq))) {
      reservaMeio(MEU_ID, destino, seq, RESERVA_MS);
      st.ctsRx++;
      Serial.println(F("  CTS recebido -> autorizado"));
      return true;
    }
    if (tipo == T_ACK && txEmCurso && buf[3] == 0) return true;
  }
  if (tipoDados(tipo)) confirmaPacote(buf);
  return false;
}

bool aguarda(unsigned long ms, byte destino, byte seq, byte tipoEsperado) {
  byte buf[5]; unsigned long t0 = millis();
  while (millis() - t0 < ms) {
    if (recebePacote(buf, 0) && atendePacote(buf, destino, seq, tipoEsperado)) return true;
  }
  return false;
}

// Cada tentativa exige novo CTS; DATA e seq são preservados no retry.
bool enviaComAck(byte destino, byte tipo, byte valor) {
  byte seq = meuSeq[destino];
  meuSeq[destino] = (seq + 1) & 0x0F;
  byte p[5], rts[5];
  montaPacote(p, destino, tipo, seq, valor);
  montaPacote(rts, destino, T_RTS, seq, RESERVA_MS);
  st.env++;
  txEmCurso = true;
  for (byte tent = 1; tent <= MAX_TENTATIVAS; tent++) {
    if (tent > 1) st.retx++;
    liberaMinhaReserva();
    bool ok = false;
    if (enviaPacote(rts, true)) {
      st.rtsTx++;
      if (aguarda(TIMEOUT_CTS, destino, seq, T_CTS)) {
        if (mesmaReserva(MEU_ID, destino, seq) && enviaPacote(p, false))
          ok = aguarda(TIMEOUT_ACK, destino, seq, T_ACK);
      } else {
        st.ctsTimeout++;
        Serial.println(F("  sem CTS -> nova tentativa sem enviar DATA"));
      }
    }
    liberaMinhaReserva();
    if (ok) {
      st.ack++;
      txEmCurso = false;
      Serial.print(F("  ACK do no ")); Serial.print(destino);
      Serial.print(F(" (seq ")); Serial.print(seq);
      Serial.print(F(", tentativa ")); Serial.print(tent); Serial.println(F(")"));
      return true;
    }
    if (tent < MAX_TENTATIVAS)
      aguarda(random(BACKOFF_MIN, BACKOFF_MAX), 0xFF, 0, 0);
  }
  txEmCurso = false;
  st.falha++;
  Serial.print(F("  FALHA: no ")); Serial.print(destino); Serial.println(F(" nao respondeu"));
  return false;
}

void processaRadio() {
  byte buf[5];
  if (recebePacote(buf, 0)) atendePacote(buf, 0xFF, 0, 0);
  if (temPendente) {
    memcpy(buf, pendente, 5);
    temPendente = false;
    trataPacote(buf);
  }
  mostraStats();
}

void configuraRadio() {
  Serial.begin(115200);
  while (!Serial) {}

  if (!radio.begin()) {
    Serial.println(F("radio hardware is not responding!!"));
    while (1) {}
  }

  radio.setPALevel(PA_NIVEL);
  radio.setChannel(CANAL);
  radio.setPayloadSize(5);
  radio.setAutoAck(false);
  radio.setCRCLength(RF24_CRC_DISABLED);
  radio.setDataRate(RF24_250KBPS);
  radio.openWritingPipe(address[0]);
  radio.openReadingPipe(0, address[1]);
  radio.startListening();

  printf_begin();
  radio.printPrettyDetails();

  randomSeed(analogRead(A7) ^ micros());
  for (byte i = 0; i < 3; i++) meuSeq[i] = random(16);

  Serial.print(F("Pronto. MEU_ID = ")); Serial.println(MEU_ID);
}
