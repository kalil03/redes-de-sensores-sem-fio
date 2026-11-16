# Monitoramento de distância com nRF24

Três Arduino Nano: gateway USB e dois nós com HC-SR04. Interface Flask,
histórico SQLite e comunicação RTS/CTS/DATA/ACK com CRC próprio.

## Montagem

| Placa | Função | Sensor atual |
|---|---|---|
| 0x2a | gateway | nenhum |
| 0x2d | nó 1 | instalado e testado |
| 0x32 | nó 2 | ainda não instalado |

HC-SR04: **VCC → 5V, GND → GND, TRIG → D3, ECHO → D2**.
Montar com USB desligado e contatos firmes.

Rádio: CE=D7, CSN=D8, SCK=D13, MOSI=D11, MISO=D12, alimentação de 3,3 V
pelo shield. Não ligar VCC do rádio em 5 V. Instalar a antena antes de transmitir.

## 1. Preparar outro computador

Clonar traz o código, mas não instala as dependências, não grava os Arduinos
nem transfere o histórico. O servidor foi validado em Linux com Python 3.10.
Os comandos para Windows estão abaixo; não houve teste físico nesse sistema.

É necessário:

- Git e Python 3.10 ou superior, com `pip` e `venv`.
- Três placas alimentadas, com os rádios e sensores montados conforme acima.
- Cabo USB de dados para o gateway. Os outros nós precisam de alimentação;
  podem usar USB, sem necessidade de abrir os seus seriais no servidor.
- Computador e celular na mesma rede local.
- Arduino IDE, Arduino AVR Boards e RF24 somente se for gravar as placas.

No Ubuntu/Debian, instalar as ferramentas no terminal:

```bash
sudo apt update
sudo apt install git python3 python3-venv python3-pip
```

No Windows, instalar [Git](https://git-scm.com/downloads/win) e
[Python](https://www.python.org/downloads/windows/). Abrir um novo PowerShell
após a instalação e conferir `git --version` e `py --version`.

Clonar e entrar na pasta, tanto no Linux quanto no PowerShell:

```bash
git clone --branch develop https://github.com/kalil03/redes-de-sensores-sem-fio.git
cd redes-de-sensores-sem-fio
```

Executar os próximos comandos a partir dessa pasta, onde está este README.

## 2. Instalar o servidor

### Linux

```bash
python3 -m venv .venv
.venv/bin/python -m pip install -r servidor/requirements.txt
```

### Windows — PowerShell

```powershell
py -m venv .venv
.\.venv\Scripts\python.exe -m pip install -r servidor/requirements.txt
```

Não é necessário ativar o ambiente: os comandos usam diretamente seu Python.
Flask e pyserial são instalados pelo arquivo de dependências. SQLite já faz
parte do Python; não é preciso instalar um servidor de banco de dados.

## 3. Gravar as placas, se necessário

Se as mesmas placas já estiverem com o firmware final, pular para o passo 4.
Mudar de computador não apaga o programa dos Arduinos.

Para placas novas ou após alterar o firmware:

1. Instalar a [Arduino IDE](https://docs.arduino.cc/software/ide-v2/tutorials/getting-started/ide-v2-downloading-and-installing).
2. No Gerenciador de Placas, instalar **Arduino AVR Boards**, da Arduino.
3. No Gerenciador de Bibliotecas, instalar **RF24**, de TMRh20.
4. Preparar os sketches com um dos comandos abaixo.

Linux:

```bash
.venv/bin/python scripts/prepara_ide.py
```

Windows — PowerShell:

```powershell
.\.venv\Scripts\python.exe scripts/prepara_ide.py
```

O comando copia as fontes para `build/arduino/`, sem depender dos links
simbólicos do repositório. Usar esses arquivos na IDE em qualquer sistema:

| Arquivo a abrir | Placa desta equipe |
|---|---|
| `build/arduino/gateway/gateway.ino` | 0x2a — gateway |
| `build/arduino/no1/no1.ino` | 0x2d — nó 1 |
| `build/arduino/no2/no2.ino` | 0x32 — nó 2 |

Para cada placa, selecionar **Arduino Nano**, processador **ATmega328P (Old
Bootloader)** e a porta correspondente. Clicar em **Verificar** e depois
**Carregar**. Se a gravação falhar, conferir cabo/porta e tentar **ATmega328P**
sem Old Bootloader, conforme o bootloader da placa. Ver
[orientação oficial sobre o processador do Nano](https://support.arduino.cc/hc/en-us/articles/4401874304274-Select-the-right-processor-for-Arduino-Nano).

As identificações já estão definidas; não editar `MEU_ID`. O canal é **103
nas três placas**, atribuído pelo professor. Canal escolhe a frequência;
identificação distingue quem envia e quem recebe.

A configuração atual é `NO2_TEM_SENSOR=0`, em `firmware/config.h`: nó 1 com
sensor, nó 2 com contador de teste. Após instalar o segundo HC-SR04, mudar
para `1`, executar `prepara_ide.py` novamente e gravar as três placas.

Editar as fontes em `firmware/`; `build/arduino/` é uma cópia gerada e será
sobrescrita pelo preparador. Manter os quatro cabeçalhos junto de cada `.ino`.
Não mudar fiação, canal ou tempos para transferir o projeto de computador.

## 4. Descobrir a porta USB do gateway

Conectar primeiro somente o gateway e verificar a porta que aparece. Depois
conectar/alimentar os outros nós. A porta do computador anterior pode mudar.
As etiquetas 0x2a/0x2d/0x32 identificam nossas placas, não os nomes das portas.

Linux:

```bash
.venv/bin/python -m serial.tools.list_ports -v
ls -l /dev/serial/by-path/
```

O gateway pode aparecer como `/dev/ttyUSB0`, `/dev/ttyUSB1` ou `/dev/ttyACM0`.
Preferir o caminho completo de `/dev/serial/by-path/` para a mesma entrada USB.

Se houver erro de permissão no Ubuntu/Debian:

```bash
sudo usermod -aG dialout "$USER"
```

Fazer logout completo e login novamente, reconectar o USB e conferir `groups`.

Windows — PowerShell:

```powershell
.\.venv\Scripts\python.exe -m serial.tools.list_ports -v
```

Anotar a porta, por exemplo `COM3`. Também aparece em **Gerenciador de
Dispositivos → Portas (COM e LPT)** e em **Ferramentas → Porta** na IDE.
Se não aparecer, conferir cabo de dados e o driver USB do conversor da placa.

Fechar o Monitor Serial da IDE antes de iniciar o servidor. A porta do gateway
não pode ser usada pelo monitor, por uma captura e pelo servidor ao mesmo tempo.

## 5. Iniciar o servidor

Substituir a porta dos exemplos pela porta real do gateway.

Linux:

```bash
.venv/bin/python -m servidor.app --serial /dev/ttyUSB0 --host 0.0.0.0
```

Windows — PowerShell:

```powershell
.\.venv\Scripts\python.exe -m servidor.app --serial COM3 --host 0.0.0.0
```

Deixar esse terminal aberto. No computador, acessar
[http://127.0.0.1:5000](http://127.0.0.1:5000). Aguardar alguns segundos pelo
reinício do gateway e pelas primeiras mensagens dos nós.

O painel deve indicar **Gateway conectado** e exibir as leituras recebidas.
Na montagem atual, o nó 2 aparece como **Modo de teste**. Gateway conectado
indica que a porta foi aberta; conferir também se chegam registros novos.

## 6. Acessar pelo celular

1. Conectar o celular à mesma rede do computador.
2. Descobrir o IPv4 local do computador: `hostname -I` no Linux ou `ipconfig`
   no Windows. Escolher o endereço da conexão ativa, como `192.168.1.20`.
3. Abrir no navegador do celular `http://192.168.1.20:5000`, substituindo o IP.

No celular, não usar `localhost`, `127.0.0.1` ou `0.0.0.0`: é necessário o IP
do computador. Não é preciso instalar aplicativo nem ter internet após instalar
as dependências; a comunicação do painel usa a rede local.

Se abrir no computador e não abrir no celular, conferir o IP, a mesma rede,
a liberação da porta TCP 5000 no firewall e se o roteador isola dispositivos
(por exemplo, uma rede de convidados). No Windows, permitir o Python na rede
privada quando o firewall solicitar. O computador precisa continuar ligado,
com o servidor aberto e o gateway conectado.

## 7. Encerrar, reiniciar e preservar o histórico

Encerrar com **Ctrl+C** no terminal. Para voltar, entrar na pasta do projeto e
repetir somente o comando do passo 5; não recriar o ambiente a cada execução.

- Banco criado automaticamente: `servidor/instance/leituras.sqlite3`.
- Log do gateway: `logs/aplicacao/gateway.log`.
- O banco e os logs não estão no GitHub. Um clone novo começa sem histórico.
- Para transferir o histórico, encerrar o servidor nos dois computadores,
  copiar a pasta `servidor/instance/` inteira para o destino e iniciar novamente.
- Não copiar `.venv` entre computadores: instalar as dependências no destino.

O horário salvo é o recebimento no computador, em UTC. O navegador apresenta
o horário local. Leituras inválidas e contadores de teste não viram distância.
O serial tenta reconectar automaticamente; eventos perdidos durante uma
desconexão ou erro de banco não são recuperados.

API: `/api/status` e `/api/leituras?no=1&limite=20` (limite de 1 a 500).
A aplicação oferece consulta e histórico na rede local, sem atuadores remotos.

## Verificações e problemas comuns

Testes locais, sem placas, na raiz do projeto:

```bash
.venv/bin/python -m unittest servidor.test_app scripts.test_valida_handshake -v
```

No PowerShell, substituir `.venv/bin/python` por `.\.venv\Scripts\python.exe`.
Esses testes não verificam a fiação, o rádio ou os sensores reais.

| Sintoma | Conferir |
|---|---|
| `No module named flask` ou `serial` | Usar o Python da `.venv` e instalar o `requirements.txt` |
| Porta inexistente ou acesso negado | Porta real, cabo, permissões e Monitor Serial fechado |
| `Address already in use` | Encerrar o outro servidor ou usar `--port 5001` e acessar essa porta |
| Painel conectado, sem novos dados | Alimentação, antenas, firmware dos três nós e configuração do rádio |
| Distância inválida | VCC=5V, GND, TRIG=D3, ECHO=D2 e posição do objeto |
| Erro nos cabeçalhos ao compilar | Executar `scripts/prepara_ide.py` e abrir o sketch em `build/arduino/` |

Para diagnóstico dos três seriais, com o servidor parado, somente no Linux:

```bash
python3 scripts/captura_serial.py --saida logs/teste --segundos 60 \
  --gateway /dev/ttyUSB1 --no1 /dev/ttyUSB0 --no2 /dev/ttyUSB2
python3 scripts/valida_handshake.py logs/teste
```

Ajustar as três portas e usar uma pasta de saída nova a cada captura. A captura
reinicia as placas por DTR. `--sem-reset` omite esse pulso; abrir a porta ainda
pode reiniciar alguns Nanos. Os monitores seriais, quando usados, são 115200 baud.

## Estado da entrega

Validados em bancada: enlace dos três nós, perda de CTS/ACK, duplicatas, CRC,
leitura de um sensor, servidor e banco. A limpeza mantém os algoritmos e os
parâmetros existentes; a validação local cobre compilação dos três IDs nos
modos com um e dois sensores e testes do servidor e do analisador de logs.
As placas estão desconectadas; não houve gravação nem novo ensaio físico.

Pendentes: instalar/testar o segundo sensor, verificar interferência acústica,
contenção/NAV sob carga e as quatro condições de avaliação abaixo.

## Requisitos da matéria

| Requisito | Implementação / verificação pendente |
|---|---|
| Gateway e dois nós trocando dados | Repasse nos dois sentidos, com aviso de entrega/falha |
| Aplicação IoT acessível no smartphone | Distância e histórico no painel Flask/SQLite, na mesma rede |
| Protocolo próprio | Carrier sense, RTS/CTS/DATA/ACK, seq, CRC e retransmissão |
| Canal limpo | Repetir ensaio final com os dois sensores |
| Outras equipes transmitindo | Ensaio real pendente |
| Ruído adicionado | Ensaio real pendente |
| Canal de uma rede Wi-Fi | Ajustar o mesmo canal nas três placas conforme o professor e testar |

Os testes de bancada anteriores não substituem as quatro etapas da avaliação.

Detalhes do enlace em [PROTOCOLO.md](PROTOCOLO.md).
