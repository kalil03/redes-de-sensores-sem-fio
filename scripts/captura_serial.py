#!/usr/bin/env python3
"""Captura simultânea dos Nanos em Linux, sem dependências externas."""
import argparse
import fcntl
import json
import os
from pathlib import Path
import re
import select
import struct
import termios
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--segundos', type=int, default=120)
    parser.add_argument('--saida', type=Path, required=True)
    parser.add_argument('--gateway', required=True)
    parser.add_argument('--no1', required=True)
    parser.add_argument('--no2', help='Omitir no ensaio com o nó 2 desligado')
    parser.add_argument('--sem-reset', action='store_true',
                        help='Não pulsar DTR; conferir logs pois abrir a porta pode reiniciar alguns Nanos')
    args = parser.parse_args()
    if args.segundos <= 0:
        parser.error('--segundos deve ser positivo')
    args.saida.mkdir(parents=True, exist_ok=False)
    ports = {role: getattr(args, role) for role in ('gateway', 'no1', 'no2')
             if getattr(args, role) is not None}
    (args.saida / 'portas.json').write_text(json.dumps(ports, indent=2) + '\n')
    opened = {}
    files = {}
    try:
        for role, port in ports.items():
            fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
            opened[fd] = role
            files[fd] = (args.saida / f'{role}.log').open('wb')
            attr = termios.tcgetattr(fd)
            attr[0] = attr[1] = attr[3] = 0
            attr[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
            attr[4] = attr[5] = termios.B115200
            attr[6][termios.VMIN] = attr[6][termios.VTIME] = 0
            termios.tcsetattr(fd, termios.TCSANOW, attr)
            if not args.sem_reset:
                fcntl.ioctl(fd, termios.TIOCMBIC, struct.pack('I', termios.TIOCM_DTR))
        if not args.sem_reset:
            time.sleep(.15)
            for fd in opened:
                fcntl.ioctl(fd, termios.TIOCMBIS, struct.pack('I', termios.TIOCM_DTR))
        start = time.monotonic()
        while time.monotonic() - start < args.segundos:
            ready, _, _ = select.select(list(opened), [], [], .25)
            for fd in ready:
                data = os.read(fd, 8192)
                if not data:
                    raise RuntimeError(f'Porta desconectada: {opened[fd]}')
                files[fd].write(data)
                files[fd].flush()
    finally:
        for fd in opened:
            files[fd].close()
            os.close(fd)
    summary = {}
    for role in ports:
        lines = (args.saida / f'{role}.log').read_text(errors='replace').splitlines()
        stats = [line for line in lines if line.startswith('STATS ')]
        summary[role] = {
            'ultimo_stats': dict((key, int(value)) for key, value in
                                 re.findall(r'(\w+)=(\d+)', stats[-1])) if stats else None,
            'confirmacoes_fim_a_fim': sum(line.startswith('CONFIRMADO:') for line in lines),
            'boots': sum('Pronto. MEU_ID' in line for line in lines),
            'erros': [line for line in lines if 'ERRO:' in line or 'not responding' in line],
        }
    text = json.dumps(summary, indent=2, ensure_ascii=False) + '\n'
    (args.saida / 'resumo.json').write_text(text)
    print(text)


if __name__ == '__main__':
    main()
