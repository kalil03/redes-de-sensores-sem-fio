"""Gateway serial -> SQLite -> API Flask. Execute com python -m servidor.app."""
import argparse
from datetime import datetime, timezone
import json
import logging
from pathlib import Path
import sqlite3
import threading

from flask import Flask, jsonify, render_template, request

LOG = logging.getLogger(__name__)


def agora():
    return datetime.now(timezone.utc).isoformat(timespec='milliseconds')


def conecta(db):
    conn = sqlite3.connect(str(db), timeout=5)
    conn.row_factory = sqlite3.Row
    return conn


def prepara_db(db):
    Path(db).parent.mkdir(parents=True, exist_ok=True)
    with conecta(db) as conn:
        conn.execute('PRAGMA journal_mode=WAL')
        conn.execute('''CREATE TABLE IF NOT EXISTS leituras (
            id INTEGER PRIMARY KEY, recebido_em TEXT NOT NULL,
            no INTEGER NOT NULL CHECK(no IN (1,2)), seq INTEGER NOT NULL,
            tipo TEXT NOT NULL, valor INTEGER NOT NULL,
            distancia_mm INTEGER, estado TEXT NOT NULL,
            entregue INTEGER NOT NULL, gateway_ms INTEGER NOT NULL
        )''')
        conn.execute('CREATE INDEX IF NOT EXISTS idx_leituras_no_id ON leituras(no,id)')


def interpreta(linha):
    """Só aceita eventos APP completos; nunca interpreta TX/RX como medição."""
    if not linha.startswith('APP '):
        return None
    e = json.loads(linha[4:])
    if not isinstance(e, dict) or e.get('evento') != 'leitura':
        raise ValueError('evento desconhecido')
    for name, low, high in [('no', 1, 2), ('seq', 0, 15), ('valor', 0, 255),
                            ('t_ms', 0, 4294967295)]:
        if type(e.get(name)) is not int or not low <= e[name] <= high:
            raise ValueError('campo inválido: ' + name)
    if e.get('tipo') not in ('distancia', 'teste') or type(e.get('entregue')) is not bool:
        raise ValueError('tipo ou confirmação inválidos')
    sensor = e['tipo'] == 'distancia'
    if sensor and e['valor'] > 200:
        raise ValueError('distância fora da codificação')
    valido = sensor and e['valor'] != 0
    return dict(recebido_em=agora(), no=e['no'], seq=e['seq'], tipo=e['tipo'],
                valor=e['valor'], distancia_mm=e['valor'] * 20 if valido else None,
                estado=('ok' if valido else 'invalida') if sensor else 'teste',
                entregue=int(e['entregue']), gateway_ms=e['t_ms'])


def grava(db, leitura):
    with conecta(db) as conn:
        conn.execute('''INSERT INTO leituras
            (recebido_em,no,seq,tipo,valor,distancia_mm,estado,entregue,gateway_ms)
            VALUES (:recebido_em,:no,:seq,:tipo,:valor,:distancia_mm,:estado,:entregue,:gateway_ms)''', leitura)


class LeitorSerial(threading.Thread):
    def __init__(self, porta, db, arquivo_log):
        super().__init__(daemon=True, name='gateway-serial')
        self.porta, self.db, self.arquivo_log = porta, db, Path(arquivo_log)
        self.parar = threading.Event()
        self.lock = threading.Lock()
        self.status = dict(porta=porta, conectado=False, eventos=0, rejeitados=0,
                           ultimo_evento=None, erro=None)

    def snapshot(self):
        with self.lock:
            return dict(self.status)

    def atualiza(self, **values):
        with self.lock:
            self.status.update(values)

    def run(self):
        import serial
        self.arquivo_log.parent.mkdir(parents=True, exist_ok=True)
        while not self.parar.is_set():
            try:
                with serial.Serial(self.porta, 115200, timeout=.5, exclusive=True) as porta, \
                        self.arquivo_log.open('a', encoding='utf-8', buffering=1) as log:
                    self.atualiza(conectado=True, erro=None)
                    # Acumula fragmentos entre timeouts, com limite de memória.
                    pending = bytearray()
                    descartar = False
                    while not self.parar.is_set():
                        data = porta.read(porta.in_waiting or 1)
                        for b in data:
                            if b == 10:
                                if descartar:
                                    descartar = False
                                    pending.clear()
                                    continue
                                linha = pending.decode('utf-8', errors='replace').rstrip('\r')
                                pending.clear()
                                log.write(agora() + ' ' + linha + '\n')
                                try:
                                    leitura = interpreta(linha)
                                    if leitura:
                                        grava(self.db, leitura)
                                        with self.lock:
                                            self.status['eventos'] += 1
                                            self.status['ultimo_evento'] = leitura['recebido_em']
                                except (ValueError, TypeError):
                                    with self.lock:
                                        self.status['rejeitados'] += 1
                            elif not descartar:
                                pending.append(b)
                                if len(pending) > 1024:
                                    pending.clear()
                                    descartar = True
                                    with self.lock:
                                        self.status['rejeitados'] += 1
            except (serial.SerialException, OSError, sqlite3.Error) as exc:
                LOG.warning('Gateway: %s', exc)
                self.atualiza(conectado=False, erro=str(exc))
                self.parar.wait(2)
        self.atualiza(conectado=False)


def create_app(db=None, leitor=None):
    app = Flask(__name__)
    db = Path(db or Path(__file__).parent / 'instance' / 'leituras.sqlite3')
    prepara_db(db)
    app.config['DB'] = db

    @app.get('/')
    def index():
        return render_template('index.html')

    @app.get('/api/leituras')
    def leituras():
        try:
            limite = int(request.args.get('limite', '50'))
            no = int(request.args['no']) if 'no' in request.args else None
            if not 1 <= limite <= 500 or no not in (None, 1, 2):
                raise ValueError
        except ValueError:
            return jsonify(erro='Use limite de 1 a 500 e no igual a 1 ou 2.'), 400
        query = 'SELECT * FROM leituras'
        params = []
        if no is not None:
            query += ' WHERE no=?'
            params.append(no)
        with conecta(db) as conn:
            rows = conn.execute(query + ' ORDER BY id DESC LIMIT ?', params + [limite]).fetchall()
        return jsonify([dict(row) for row in rows])

    @app.get('/api/status')
    def status():
        nodes = []
        with conecta(db) as conn:
            for no in (1, 2):
                row = conn.execute('SELECT * FROM leituras WHERE no=? ORDER BY id DESC LIMIT 1', (no,)).fetchone()
                item = dict(row) if row else None
                idade = (datetime.now(timezone.utc) - datetime.fromisoformat(row['recebido_em'])).total_seconds() if row else None
                nodes.append(dict(no=no, recente=idade is not None and idade < 15,
                                  idade_s=round(idade, 1) if idade is not None else None,
                                  ultima=item))
        serial_status = leitor.snapshot() if leitor else dict(conectado=False, porta=None, erro='Leitor serial não iniciado')
        return jsonify(serial=serial_status, nos=nodes)

    return app


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--serial', required=True, help='Porta do gateway (preferir /dev/serial/by-path/...)')
    parser.add_argument('--db', default='servidor/instance/leituras.sqlite3')
    parser.add_argument('--log', default='logs/aplicacao/gateway.log')
    parser.add_argument('--host', default='127.0.0.1')
    parser.add_argument('--port', type=int, default=5000)
    args = parser.parse_args()
    logging.basicConfig(level=logging.INFO)
    leitor = LeitorSerial(args.serial, args.db, args.log)
    app = create_app(args.db, leitor)
    leitor.start()
    try:
        app.run(host=args.host, port=args.port, debug=False, use_reloader=False)
    finally:
        leitor.parar.set()
        leitor.join(timeout=3)


if __name__ == '__main__':
    main()
