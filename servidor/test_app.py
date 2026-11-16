import json
from pathlib import Path
import tempfile
import unittest

from servidor.app import create_app, grava, interpreta


def evento(**changes):
    data = dict(evento='leitura', no=1, seq=3, tipo='distancia', valor=5,
                entregue=True, t_ms=10000)
    data.update(changes)
    return 'APP ' + json.dumps(data)


class AplicacaoTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.db = Path(self.temp.name) / 'dados.sqlite3'
        self.app = create_app(self.db)
        self.client = self.app.test_client()

    def tearDown(self):
        self.temp.cleanup()

    def test_distancia_escala_invalida_e_teste_separados(self):
        self.assertEqual(interpreta(evento())['distancia_mm'], 100)
        invalida = interpreta(evento(valor=0))
        self.assertIsNone(invalida['distancia_mm'])
        self.assertEqual(invalida['estado'], 'invalida')
        teste = interpreta(evento(no=2, tipo='teste', valor=245))
        self.assertIsNone(teste['distancia_mm'])
        self.assertEqual(teste['estado'], 'teste')

    def test_rejeita_lixo_e_campos_invalidos(self):
        self.assertIsNone(interpreta('RX: A7 20 31 01 FF'))
        for linha in ['APP {', 'APP []', evento(no=3), evento(no=True),
                      evento(seq=16), evento(valor=201), evento(t_ms=-1),
                      evento(entregue=1), evento(tipo='outro')]:
            with self.subTest(linha=linha), self.assertRaises((ValueError, TypeError)):
                interpreta(linha)

    def test_persistencia_filtros_e_falha_de_repasse(self):
        grava(self.db, interpreta(evento(entregue=False)))
        grava(self.db, interpreta(evento(no=2, tipo='teste', valor=4)))
        outra = create_app(self.db).test_client()
        rows = outra.get('/api/leituras?no=1&limite=1').get_json()
        self.assertEqual(len(rows), 1)
        self.assertEqual(rows[0]['distancia_mm'], 100)
        self.assertEqual(rows[0]['entregue'], 0)
        status = outra.get('/api/status').get_json()
        self.assertTrue(status['nos'][0]['recente'])
        self.assertEqual(status['nos'][1]['ultima']['tipo'], 'teste')
        self.assertFalse(status['serial']['conectado'])
        for url in ['/api/leituras?no=3', '/api/leituras?limite=0',
                    '/api/leituras?limite=501', '/api/leituras?no=abc']:
            self.assertEqual(outra.get(url).status_code, 400)

    def test_pagina_e_estado_inicial(self):
        self.assertEqual(self.client.get('/').status_code, 200)
        self.assertEqual(self.client.get('/api/leituras').get_json(), [])
        self.assertTrue(all(not n['recente'] for n in self.client.get('/api/status').get_json()['nos']))


if __name__ == '__main__':
    unittest.main()
