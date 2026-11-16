# Protocolo de comunicação

## Quadro de 5 bytes

| Byte | Conteúdo |
|---|---|
| 0 | rede: 0xA7 |
| 1 | origem no nibble alto; destino no baixo |
| 2 | tipo no nibble alto; sequência no baixo |
| 3 | dado; duração da reserva no RTS/CTS; zero no ACK |
| 4 | CRC-8 dos bytes 0..3 |

CRC: polinômio 0x07, inicial 0xA7, sem reflexão e sem XOR final.
Origens/destinos: 0=gateway, 1=nó 1, 2=nó 2. Seq varia de 0 a 15 por destino.

| Tipo | Nome | Uso |
|---|---|---|
| 2 | ACK | confirma o dado do salto |
| 3 | ENVIA | nó envia ao gateway para repasse |
| 4 | REPASSE | gateway encaminha ao outro nó |
| 5 | ENTREGUE | informa ao remetente que o destino confirmou |
| 6 | NAO_ENTREGUE | informa falha no repasse |
| 7 | RTS | pede autorização; VALOR=90 ms |
| 8 | CTS | concede autorização; VALOR=90 ms |

## Acesso ao meio

Cada envio confiável dos tipos 3..6 usa:

```text
carrier sense → RTS → CTS → DATA → ACK
```

Só RTS disputa o meio. CTS, DATA autorizado e ACK usam a reserva.
CTS precisa corresponder à origem, destino e seq esperados. Sem CTS, não há
DATA. Dado com CRC inválido ou sem reserva válida não recebe ACK.

Terceiros que ouvem RTS aguardam até 30 ms; ao ouvir CTS, reservam até 90 ms.
ACK correspondente libera a reserva; sem ACK, ela expira por `millis()`.
A reserva local não obriga outras equipes a esperar e não elimina colisões.

Cada retry refaz RTS/CTS e mantém os bytes e seq do DATA. Duplicata recebe
novo ACK, mas não é processada novamente. Um receptor ocupado ou sem espaço
não concede um novo RTS. Esperas continuam atendendo o rádio.

## Parâmetros

| Parâmetro | Valor |
|---|---|
| Canal | 103 |
| Taxa / potência | 250 kbps / PA_LOW |
| Endereço TX/RX | 0x3030303030 |
| AutoAck / CRC do rádio | desligados; feitos em software |
| Timeout CTS / ACK | 30 ms / 30 ms |
| Reserva CTS | 90 ms |
| Tentativas | 5 no máximo |
| Backoff | sorteio de 5 a 19 ms |
| Carrier sense | RPD, espera de 200 µs antes da leitura |
| Canal ocupado | até 10 esperas de 2 a 9 ms; depois nova tentativa |
| Confirmação fim a fim | até 1500 ms |
| Envio dos nós | 3,7 s e 4,4 s |
| Estatísticas | a cada 10 s |

PA_LOW foi mantido após os testes de bancada. O canal 103 foi atribuído pelo
professor; o código-base usa 37. A etapa no canal do Wi-Fi exige rever esse valor com o
professor. ACK/CTS precisam chegar rapidamente; por isso não disputam o meio
novamente. A reserva de 90 ms permite concluir a troca e limita bloqueios
quando um controle se perde. Esses tempos não são os do padrão Wi-Fi.

## Encaminhamento

```text
nó 1 → gateway: ENVIA (RTS/CTS/DATA/ACK)
gateway → nó 2: REPASSE (RTS/CTS/DATA/ACK)
gateway → nó 1: ENTREGUE ou NAO_ENTREGUE (RTS/CTS/DATA/ACK)
```

ACK confirma somente um salto. ENTREGUE depende do ACK do destino final.
A confirmação de repasse registrada no banco não garante que o aviso
ENTREGUE voltou ao remetente.

## Aplicação

HC-SR04 em D3/TRIG e D2/ECHO, alimentado em 5 V. Captura do eco por interrupção,
timeout de 30 ms e intervalo nominal de 200 ms, sem esperar bloqueado pelo eco.

No byte VALOR, 1..200 representa distância em passos de 20 mm, arredondada.
Zero indica leitura inválida, fora de 20..4000 mm ou com mais de 2 s.
Resolução de transporte não equivale à precisão física do sensor.

Por enquanto só o nó 1 tem sensor. O nó 2 envia teste, identificado como tal.
Os sketches `gateway/gateway.ino`, `no1/no1.ino` e `no2/no2.ino` já definem
a identificação de cada placa (0..2), sem ajuste manual ao gravar. Usam o
mesmo código em `firmware/principal.h`, com links para os cabeçalhos no Linux.
`NO2_TEM_SENSOR`, em `config.h`, habilita a segunda leitura e sua identificação
no gateway; manter o mesmo valor nas três placas. O enlace fica em `enlace.h`
e a captura do sensor em `hcsr04.h`, sem cópias por placa. Canal e identificação
são independentes: o canal 103 é comum às três; a identificação distingue quem
envia e quem recebe os quadros e confirmações.
O gateway imprime eventos `APP` após tratar o repasse. Flask valida esses
registros e guarda horário de chegada, nó, seq, valor, distância e confirmação
no SQLite. Dados com mais de 15 s aparecem como antigos no painel.

## Estatísticas e validação

`env/ack/falha`: transações iniciadas, confirmadas e esgotadas.
`retx`: novas tentativas, mesmo quando não houve DATA por falta de CTS.
`entr/naoentr/semconf`: entrega fim a fim, falha e ausência de aviso.
`ocup/nav/cs_abort`: esperas por energia, reserva e limite de carrier sense.
`dup/crc/alheio/txerr`: duplicatas, CRC inválido, outra rede e erro de envio.
`rts/cts_tx/cts_rx/cts_timeout/sem_reserva`: controles e dados sem concessão.

Testes físicos realizados com um sensor e nó 2 em teste:

- 60 s com RTS/CTS: 28 entregas fim a fim; sem falha definitiva.
- CTS omitido: repetiu RTS, sem DATA antes de CTS.
- ACK omitido: repetiu RTS/CTS/DATA; processamento único no receptor.
- CRC alterado: descarte e recuperação por retransmissão.
- Destino desligado: aviso de falha validado na versão anterior sem RTS/CTS.
- Canal ocupado simulado: espera validada na versão anterior.
- Sensor, Flask, banco e painel validados com dados reais.

Contenção/NAV sob carga e interferência real ainda precisam de teste.
Repetir o ensaio de destino desligado com RTS/CTS e validar os dois sensores.
Sequência de 4 bits e inicial aleatório não garantem tratamento de pacotes
antigos após resets ou atraso maior que uma volta da sequência.

Nesta limpeza, os algoritmos e parâmetros foram preservados. Sem placas
conectadas, a verificação ficou restrita à compilação e aos testes locais.
As quatro etapas reais (canal limpo, outras equipes, ruído e Wi-Fi) continuam
pendentes para a montagem final com os dois sensores.
