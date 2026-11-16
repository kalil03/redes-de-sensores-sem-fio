import tempfile
from pathlib import Path
import unittest
from scripts.valida_handshake import crc8, valida


def pacote(direction, origem, destino, tipo, seq, valor):
    raw = bytes([0xA7, origem << 4 | destino, tipo << 4 | seq, valor])
    return direction + ': ' + (raw + bytes([crc8(raw)])).hex(' ').upper()


class HandshakeLogTest(unittest.TestCase):
    def check_log(self, lines):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'no1.log'
            path.write_text('\n'.join(lines))
            return valida(path, 1)

    def test_troca_completa(self):
        r = self.check_log([pacote('TX',1,0,7,4,90), pacote('RX',0,1,8,4,90),
                            pacote('TX',1,0,3,4,20), pacote('RX',0,1,2,4,0)])
        self.assertEqual(r['trocas_com_ack'], 1)
        self.assertEqual(r['erros'], [])

    def test_dado_sem_cts_e_cts_com_seq_errado(self):
        r = self.check_log([pacote('TX',1,0,7,4,90), pacote('RX',0,1,8,5,90),
                            pacote('TX',1,0,3,4,20)])
        self.assertEqual(len(r['erros']), 1)

    def test_repeticao_rts_sem_enviar_dados(self):
        r = self.check_log([pacote('TX',1,0,7,4,90), pacote('TX',1,0,7,4,90),
                            pacote('RX',0,1,8,4,90), pacote('TX',1,0,3,4,20),
                            pacote('RX',0,1,2,4,0)])
        self.assertEqual(r['rts_enviados'], 2)
        self.assertEqual(r['dados_enviados'], 1)
        self.assertEqual(r['erros'], [])

    def test_cts_com_crc_invalido_nao_autoriza(self):
        corrupto = pacote('RX',0,1,8,4,90)[:-2] + 'FF'
        r = self.check_log([pacote('TX',1,0,7,4,90), corrupto,
                            pacote('TX',1,0,3,4,20)])
        self.assertEqual(len(r['erros']), 1)

    def test_cts_de_outro_no_ou_reserva_invalida_nao_autoriza(self):
        for cts in [pacote('RX',2,1,8,4,90), pacote('RX',0,1,8,4,1)]:
            r = self.check_log([pacote('TX',1,0,7,4,90), cts,
                                pacote('TX',1,0,3,4,20)])
            self.assertEqual(len(r['erros']), 1)


if __name__ == '__main__':
    unittest.main()
