"""Prepara sketches independentes para a Arduino IDE, sem links simbólicos."""
from pathlib import Path
import shutil


def main():
    raiz = Path(__file__).resolve().parents[1]
    saida = raiz / 'build' / 'arduino'
    for nome in ('gateway', 'no1', 'no2'):
        destino = saida / nome
        destino.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(raiz / nome / f'{nome}.ino', destino / f'{nome}.ino')
        for arquivo in (raiz / 'firmware').glob('*.h'):
            shutil.copyfile(arquivo, destino / arquivo.name)
        print(destino / f'{nome}.ino')


if __name__ == '__main__':
    main()
