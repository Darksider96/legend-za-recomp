"""Inspect the bounded Avalon tables extracted for phase 5; never runs the game."""
from pathlib import Path
import collections
import hashlib
import json
import struct

HERE = Path(__file__).resolve().parent
WORKSPACE = HERE.parents[1]
ASSETS = HERE / 'phase5-stats-assets' / 'avalon' / 'data'

class FB:
    def __init__(self, data): self.data = data
    def read(self, fmt, p): return struct.unpack_from('<' + fmt, self.data, p)[0]
    def fields(self, table):
        vt = table - self.read('i', table)
        size = self.read('H', vt)
        assert size >= 4 and size % 2 == 0 and vt + size <= len(self.data)
        return [self.read('H', p) for p in range(vt + 4, vt + size, 2)]
    def field(self, table, index):
        fs = self.fields(table)
        return table + fs[index] if index < len(fs) and fs[index] else None
    def vector(self, table, index):
        p = self.field(table, index)
        assert p is not None
        p += self.read('I', p)
        return p, self.read('I', p)
    def table_element(self, vector, index):
        v, n = vector
        assert index < n
        p = v + 4 + index * 4
        return p + self.read('I', p)

def sha(data): return hashlib.sha256(data).hexdigest()

def main():
    tables = {}
    for name in ['personal_array.bin', 'waza_array.bin', 'tokusei_array.bin', 'growTable.bin']:
        data = (ASSETS / name).read_bytes()
        fb = FB(data)
        root = fb.read('I', 0)
        entry = dict(size=len(data), sha256=sha(data), root=hex(root), root_fields=fb.fields(root))
        if name == 'growTable.bin':
            # This field is inline storage, not an offset to a vector.
            field = fb.field(root, 0)
            count = (len(data) - field) // 4
            assert count == 808 and field + count * 4 == len(data)
            entry.update(field0_inline_offset=hex(field), inline_uint32_count=count,
                         groups_of_101=[list(struct.unpack_from('<101I', data, field+i*101*4)) for i in range(8)],
                         grouping_limit='Observed 808 inline u32 partitioned as 8x101; growth-class names not recovered.')
        else:
            vector = fb.vector(root, 0)
            entry.update(field0_vector_offset=hex(vector[0]), field0_vector_count=vector[1])
        tables[name] = entry

    data = (ASSETS / 'personal_array.bin').read_bytes()
    fb = FB(data)
    vector = fb.vector(fb.read('I', 0), 0)
    records = []
    for index in range(vector[1]):
        table = fb.table_element(vector, index)
        key = fb.field(table, 0)
        assert key is not None
        species, form = struct.unpack_from('<HH', data, key)
        stats = fb.field(table, 20)
        assert stats is not None
        records.append(dict(index=index, table_offset=hex(table), species=species, form=form,
                            runtime_map_key=species * 10000 + form,
                            key_field_offset=key-table, stats_field_offset=stats-table,
                            stats_bytes=list(data[stats:stats+6]), vtable_fields=fb.fields(table)))
    keys = [(r['species'], r['form']) for r in records]
    assert len(keys) == len(set(keys))
    selected = []
    for species in [150, 25, 1, 8]:
        matches = [r for r in records if r['species'] == species and r['form'] == 0]
        assert len(matches) == 1
        selected.append(matches[0])

    dictionary = (WORKSPACE / 'analysis' / 'recovered_dictionary.txt').read_text(encoding='utf-8').splitlines()
    known_paths = [p for p in dictionary if p and not p.startswith('#')]
    tbl = [p for p in known_paths if p.endswith('.tbl')]
    result = dict(
        method='Static extracted assets + main accessor assembly; no target execution',
        tables=tables,
        personal=dict(records=len(records), unique_species_form_keys=len(set(keys)),
                      key_wire_format='field0 inline uint16 species, uint16 form (little endian)',
                      key_runtime_formula='species * 10000 + form',
                      key_reader='0x7100CF3D30', key_lookup='0x7100CF4270',
                      stats_wire_format='field20 inline six uint8, vtable entry 0x2C',
                      stats_accessor_functions=['0x7100CF9428','0x7100CF9474','0x7100CF94C0',
                                                '0x7100CF950C','0x7100CF9558','0x7100CF95A4'],
                      stats_order_interpretation=['HP','Attack','Defense','SpAttack','SpDefense','Speed'],
                      semantic_limit='Storage and byte indices are code-proven; stat names are descriptive interpretations.',
                      selected=selected),
        recovered_dictionary_tbl=dict(known_paths=len(known_paths), tbl_paths=len(tbl),
                                      outside_ik_message=sum(not p.startswith('ik_message/') for p in tbl),
                                      boundary='Only the recovered dictionary; unnamed archive members are not classified.'))
    (HERE / 'phase5-stats-assets.json').write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
    print(json.dumps(dict(table_counts={n:r.get('field0_vector_count',r.get('inline_uint32_count')) for n,r in tables.items()},
                          samples=[{k:r[k] for k in ['species','form','index','stats_bytes']} for r in selected],
                          tbl=result['recovered_dictionary_tbl']), indent=2))

if __name__ == '__main__': main()
