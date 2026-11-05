#!/usr/bin/env python3
"""Inspeciona a estrutura de bibliotecas de piano Nord (.npno).

Uso: python npno_inspector.py "Meu piano.npno" --output pasta

Não decodifica áudio, não gera WAV e não modifica o arquivo original.
Os deslocamentos CNSP baseiam-se em documentação comunitária do nord-format.
"""
from __future__ import annotations
import argparse
import csv
import json
import mmap
from collections import Counter, defaultdict
from pathlib import Path
from struct import unpack_from

HEADER_BYTES = 44
DIRECTORY_AT = 0x732
STROKE_BYTES = 118
NAMES = {0: 'ataque', 1: 'ressonancia', 2: 'soltura'}

def midi_name(number: int) -> str:
    notes = ('C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B')
    return f'{notes[number % 12]}{number // 12 - 1}'

def parse(path: Path):
    with path.open('rb') as f, mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ) as mm:
        if len(mm) < HEADER_BYTES + DIRECTORY_AT or mm[:4] != b'CBIN' or mm[8:12] != b'npno':
            raise ValueError('Arquivo sem cabecalho CBIN/npno valido')
        d = memoryview(mm)[HEADER_BYTES:]
        try:
            if bytes(d[:4]) != b'CNSP':
                raise ValueError('Corpo CNSP nao encontrado no offset esperado')
            be16 = lambda k: unpack_from('>H', d, k)[0]
            be32 = lambda k: unpack_from('>I', d, k)[0]
            cleaned = lambda k, length: bytes(d[k:k+length]).split(b'\0')[0].decode('latin-1').strip()
            version = be16(0x04)
            channels = be16(0x61e)
            if version not in (0x450, 0x464):
                raise ValueError(f'Layout de versao 0x{version:X} nao suportado')
            if channels not in (1, 2):
                raise ValueError('Numero de canais fora de 1/2')
            declared = be16(0x620)
            root_counts = [be16(0x622 + i*2) for i in range(128)]
            if sum(root_counts) != declared:
                raise ValueError('Inconsistencia entre soma de notas e registros')
            roots = [i for i, n in enumerate(root_counts) for _ in range(n)]
            directory_end = DIRECTORY_AT + declared * STROKE_BYTES
            if directory_end > len(d):
                raise ValueError('Diretorio de gravacoes truncado')
            block_bytes = 1022 * channels
            records = []
            position = None
            for index, root in enumerate(roots):
                off = DIRECTORY_AT + index * STROKE_BYTES
                start = be32(off)
                bank = d[off + 4]
                layer = d[off + 5]
                frames = be32(off + 6)
                blocks = be16(off + 10)
                end = start + blocks * block_bytes
                if index == 0:
                    expected = ((directory_end + 192 + block_bytes - 1) // block_bytes) * block_bytes - 192
                    if start != expected:
                        raise ValueError(f'Inicio do audio diferente da grade esperada: {start} != {expected}')
                    if any(d[directory_end:start]):
                        raise ValueError('Padding anterior ao audio nao e zero')
                elif start != position:
                    raise ValueError(f'Intervalo de audio inconsistente no registro {index}')
                if end > len(d):
                    raise ValueError(f'Gravacao {index} ultrapassa o final do arquivo')
                position = end
                records.append({
                    'indice': index, 'nota_raiz_midi': root,
                    'nota_raiz_nome': midi_name(root),
                    'banco': NAMES.get(bank, f'desconhecido_{bank}'),
                    'camada': int(layer), 'quadros_audio': frames,
                    'blocos': blocks, 'deslocamento_no_corpo': start,
                    'bytes_audio_codificado': blocks * block_bytes,
                })
            if position != len(d):
                raise ValueError('Dados nao esperados apos o ultimo registro')
            key_map = list(d[0x8c:0x10c])
            covered = [k for k, root in enumerate(key_map) if root != 255]
            distinct_roots = sorted(set(roots))
            if any(root not in distinct_roots for root in key_map if root != 255):
                raise ValueError('Mapeamento de teclas aponta a gravacoes ausentes')
            counts = Counter(rec['banco'] for rec in records)
            byroot = defaultdict(lambda: {'ataque': 0, 'ressonancia': 0, 'soltura': 0})
            for rec in records:
                if rec['banco'] in NAMES.values():
                    byroot[rec['nota_raiz_midi']][rec['banco']] += 1
            summary = {
                'arquivo': path.name, 'tamanho_bytes': len(mm), 'tamanho_mb_decimal': round(len(mm)/1e6, 2),
                'cabecalho': 'CBIN / npno', 'corpo': 'CNSP',
                'versao_cnsp_hex': f'0x{version:03X}',
                'nome': cleaned(0x3c, 32), 'identificacao': cleaned(0x1c, 32),
                'fabricante_variacao': cleaned(0x5c, 32),
                'canais': channels, 'gravacoes_total': declared,
                'notas_raiz_gravadas': len(distinct_roots),
                'notas_raiz_midi': distinct_roots,
                'notas_raiz_legiveis': [midi_name(n) for n in distinct_roots],
                'bancos': dict(counts),
                'teclas_mapeadas': len(covered),
                'alcance_teclas_midi': [min(covered), max(covered)] if covered else [],
                'mapa_teclas_midi_para_raiz': {str(k): int(key_map[k]) for k in covered},
                'gravacoes_por_raiz': {str(k): v for k, v in byroot.items()},
                'integridade_estrutural': 'APROVADA: diretório, blocos, spans, mapa e tamanho conferidos',
                'checksum': 'NAO VERIFICADO',
                'audio_pcm_decodificado': False,
                'aviso': 'Análise estrutural somente. Nao valida decodificacao, qualidade sonora ou licencas.'
            }
            return summary, records
        finally:
            d.release()

def main():
    parser = argparse.ArgumentParser(description='Inspetor de arquivos NPno Nord')
    parser.add_argument('file', type=Path)
    parser.add_argument('--output', type=Path, default=Path('resultado_npno'))
    args = parser.parse_args()
    summary, records = parse(args.file)
    args.output.mkdir(exist_ok=True, parents=True)
    (args.output/'resumo.json').write_text(json.dumps(summary, ensure_ascii=False, indent=2), encoding='utf-8')
    with (args.output/'catalogo_gravacoes.csv').open('w', encoding='utf-8-sig', newline='') as output:
        writer = csv.DictWriter(output, fieldnames=list(records[0]))
        writer.writeheader()
        writer.writerows(records)
    print(f"OK: {summary['nome']} / {summary['fabricante_variacao']} / {summary['gravacoes_total']} gravacoes")
    print(f"Estrutura: {summary['integridade_estrutural']}")
    print(f"Arquivos gerados em {args.output}")

if __name__ == '__main__':
    main()
