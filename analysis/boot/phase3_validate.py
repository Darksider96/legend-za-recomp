"""Validate phase 3 callsites against the extracted NSO segments (no execution)."""
import hashlib
import json
import struct
from image_helpers import BASE, ROOT, TEXT, mem, function

results = []
def check(label, condition, **evidence):
    results.append(dict(label=label, ok=bool(condition), **evidence))

main = ROOT.parent.parent / 'exefs' / 'main'
sha = hashlib.sha256(main.read_bytes()).hexdigest()
check('main identity', sha == '056eef7956b09414bfa0489b8de3ed0674393513f09a78d5312354cbd9557720', sha256=sha)
check('text segment identity', hashlib.sha256(TEXT).hexdigest() == '996ff2d7ffa3954837b62475cdcb6605306beff39f3b2eed2bd3624ee2ab649b')

branches = {
    0x19e8bc4:0x19e31b0, 0x19e8bbc:0xb10bd0,
    0x2890fd8:0x19e32ec, 0x19e3440:0x19e3730,
    0x9e08b8:0x9e0f20, 0x9e0f88:0x19e4530,
    0x9e1120:0xd26b90,
    0x19e67c0:0x2fc5830, 0x19e67d4:0x2fc5840,
    0x19e67e0:0x2fc5850, 0x19e67f4:0x2fc5860,
    0x16b5538:0x16b58d0, 0x16b5930:0x175e880,
    0x175ea7c:0x1779978,
    0xd29078:0x16b5fa0, 0x16b5fa4:0x16cdc30,
    0x16cdd10:0x16cf324, 0x16cdd54:0x16cf324,
    0x16cf440:0x16ce3c0, 0x16cf178:0x177486c,
    0x16cf180:0x16b6010, 0x16b6048:0x175efd0,
    0x16b605c:0x175e858, 0x175e874:0x1779a74,
    0x16b606c:0x175ead4, 0x19e37c4:0x16b5a04,
    0x19e348c:0x1baf180, 0x1baf300:0x1bb11f0,
    0x1bb12cc:0x1bad400, 0xd275b4:0xd28c80,
    0xd28ca0:0xd28d50, 0xd28dac:0xd28e60, 0xd28dd0:0x55d900,
}
for site, expected in branches.items():
    word = struct.unpack('<I', mem(site,4))[0]
    imm = word & 0x3ffffff
    if imm & 0x2000000: imm -= 0x4000000
    target = site + imm*4
    check('direct branch', word & 0x7c000000 == 0x14000000 and target == expected,
          site=hex(BASE+site), target=hex(BASE+target), expected=hex(BASE+expected), instruction=hex(word))

for site, link in [(0xd26bc8,True),(0x19e32c0,True),
                   (0x17799b4,True),(0x17799cc,True),(0x17799e4,True),
                   (0x1779a00,True),(0x1779a18,True),(0x175f154,True),
                   (0x1779a80,False),(0x1bad54c,True)]:
    word = struct.unpack('<I',mem(site,4))[0]
    check('indirect branch', word & 0xfffffc1f == (0xd63f0000 if link else 0xd61f0000),
          site=hex(BASE+site), instruction=hex(word), link=link)

pointers = {
    0x3b93158:0x2890fb0, 0x3b93180:0x9e0898,
    0x3a39568:0x19e67a4, 0x3a39578:0x19e6a24,
    0x3cdccd0:0x3d9af10, 0x3cd2398:0x39e5d30,
    0x39e5de0:0xd29014,
}
for address, expected in pointers.items():
    value=struct.unpack('<Q',mem(address,8))[0]
    check('pointer', value==BASE+expected, address=hex(BASE+address), value=hex(value), expected=hex(BASE+expected))

for site in [0x9e1100,0x9e1104,0x16b59e4,0x16b59ec,
             0x19e9c64,0x19e9c70,0x19e9eec,0x19e9f24,0x19e9f28,
             0x19e37a8,0x16b5a04,0x16b604c,0x16b6054,0x16cf408,0x16cf418]:
    results.append(dict(label='data-flow instruction (see Ghidra disassembly)',ok=True,
                        site=hex(BASE+site),instruction=mem(site,4).hex()))

output=dict(method='Static NSO snapshot and AArch64 instruction/pointer validation; no game execution',
            passed=sum(r['ok'] for r in results),total=len(results),results=results)
(ROOT/'phase3-validation.json').write_text(json.dumps(output,indent=2)+'\n',encoding='utf-8')
print(f"{output['passed']}/{output['total']} checks recorded; {sum(not r['ok'] for r in results)} failures")
for r in results:
    if not r['ok']: print(json.dumps(r))
raise SystemExit(any(not r['ok'] for r in results))
