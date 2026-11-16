#!/usr/bin/env python3
"""Valida nos logs que cada DATA transmitido teve RTS e CTS correspondentes."""
import argparse
import json
from pathlib import Path
import re


def crc8(data):
    crc = 0xA7
    for value in data:
        crc ^= value
        for _ in range(8):
            crc = ((crc << 1) ^ (7 if crc & 128 else 0)) & 255
    return crc


def valida(path, node):
    lines = path.read_text(errors='replace').splitlines()
    # Excluir bytes antigos de antes do reset quando o boot estiver capturado.
    boots = [i for i, line in enumerate(lines) if 'Pronto. MEU_ID' in line]
    if boots:
        lines = lines[boots[-1] + 1:]
    state = {}
    data = 0
    complete = 0
    rts = 0
    cts = 0
    errors = []
    tipos = set()
    for i, line in enumerate(lines):
        m = re.match(r'^(TX|RX): ((?:[0-9A-F]{2} ){4}[0-9A-F]{2})', line)
        if not m:
            continue
        raw = bytes.fromhex(m[2])
        if raw[0] != 0xA7 or crc8(raw[:4]) != raw[4]:
            continue
        orig, dest, tipo, seq = raw[1] >> 4, raw[1] & 15, raw[2] >> 4, raw[2] & 15
        if m[1] == 'TX' and orig == node:
            key = (dest, seq)
            if tipo == 7 and raw[3] == 90:
                state[key] = 'rts'
                rts += 1
            elif 3 <= tipo <= 6:
                if state.get(key) != 'cts':
                    errors.append(f'linha {i+1}: DATA sem CTS: {line}')
                state[key] = 'data'
                tipos.add(tipo)
                data += 1
        elif m[1] == 'RX' and dest == node:
            key = (orig, seq)
            if tipo == 8 and raw[3] == 90 and state.get(key) == 'rts':
                state[key] = 'cts'
                cts += 1
            elif tipo == 2 and raw[3] == 0 and state.get(key) == 'data':
                state[key] = 'ack'
                complete += 1
    return dict(no=node, rts_enviados=rts, cts_correspondentes=cts,
                dados_enviados=data, trocas_com_ack=complete,
                tipos_dados=sorted(tipos), erros=errors)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('pasta', type=Path)
    args = parser.parse_args()
    result = {role: valida(args.pasta / f'{role}.log', node)
              for role, node in [('gateway', 0), ('no1', 1), ('no2', 2)]}
    (args.pasta / 'handshake.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))
    if any(r['erros'] or r['trocas_com_ack'] == 0 for r in result.values()):
        raise SystemExit(1)


if __name__ == '__main__':
    main()
