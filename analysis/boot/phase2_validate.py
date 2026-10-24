from image_helpers import *
import hashlib,json

checks={}
checks['main_sha256']=hashlib.sha256((ROOT.parent.parent/'exefs/main').read_bytes()).hexdigest()
assert checks['main_sha256']=='056eef7956b09414bfa0489b8de3ed0674393513f09a78d5312354cbd9557720'
checks['text_sha256']=hashlib.sha256(TEXT).hexdigest()
assert checks['text_sha256']=='996ff2d7ffa3954837b62475cdcb6605306beff39f3b2eed2bd3624ee2ab649b'
checks['graphics_callback_record']=struct.unpack('<QQ',mem(0x3a38ec8,16))
assert checks['graphics_callback_record']==(0x40,1)
assert struct.unpack('<Q',mem(0x3a043e8,8))[0]==BASE+0x9e0f20
checks['boot_compute_control_override']=struct.unpack('<II',mem(0x315b4e8,8))
assert checks['boot_compute_control_override']==(0,0x8000)
checks['effective_control_memory_size']=checks['boot_compute_control_override'][1]<<4
assert checks['effective_control_memory_size']==0x80000
slots=json.loads((ROOT/'phase2-nvn-slots.json').read_text())
slot_names={int(x['slot'],16):x['name'] for x in slots}
checks['nvn_slot_bindings']={}
for got,name in [(0x3cdd4c8,'nvnDeviceInitialize'),(0x3cdd038,'nvnQueueInitialize'),
                 (0x3cdcca0,'nvnDeviceGetInteger'),(0x3cdd028,'nvnQueueBuilderGetQueueMemorySize')]:
    slot=struct.unpack('<Q',mem(got,8))[0]
    assert slot_names[slot]==name
    checks['nvn_slot_bindings'][hex(BASE+got)]={'slot':hex(slot),'name':name}
for a in [0x17798b8,0x176bab8,0x176b9b0,0x176b9d4,0x176b958,0x176b970,0x176b988]:
    assert struct.unpack_from('<I',TEXT,a)[0]&0xfffffc1f==0xd63f0000
checks['critical_blr_instructions']='PASS'
fs_parts=[]
for addr in [0x3cef3a4,0x314c8f2,0x30f98d6,0x30333c5]:
    fs_parts.append(mem(addr,100).split(b'\0')[0].decode())
checks['first_asset_path']=''.join(fs_parts)
assert checks['first_asset_path']=='data:/system/config/data/trinity_config.trowc'
direct_edges=[
 (0x19e8bc4,0x19e31b0),(0x5aeca0,0x19dd110),(0x19dd2e8,0x193a498),
 (0x193a6a4,0x2fc5610),(0x19554c0,0x2fc4580),
 (0x9e1120,0xd26b90),(0xd26bb4,0x16b4df4),(0x16b4df4,0x176b750),
 (0x176b7b4,0x17755f0),(0x1775938,0x2fc4fb0),
 (0xd26bdc,0xd277f0),(0xd27880,0x16b4ff4),(0x16b5218,0x175cd38),
 (0x175cd68,0x175b780),(0x175ba84,0x1779834),
 (0x1779858,0x2fc4fe0),(0x1779870,0x2fc4fe0),(0x1779880,0x1775db0),
 (0xd26c30,0x16b5440),(0x16b5464,0x175bd84),(0x175be0c,0x177989c),
 (0x175be20,0x17798cc),(0x16b5538,0x16b58d0),(0x16b598c,0x175eb78),
 (0x175eb80,0x176b85c),(0x16b59d8,0x175eb84),(0x175ebb0,0x176b8e0)]
for site,target in direct_edges:
    w=struct.unpack_from('<I',TEXT,site)[0]
    assert w&0xfc000000 in (0x94000000,0x14000000)
    disp=w&0x3ffffff
    if disp&(1<<25):disp-=1<<26
    assert site+disp*4==target,(hex(BASE+site),hex(BASE+target))
checks['report_direct_edges']=f'PASS: {len(direct_edges)} BL/B edges'
(ROOT/'phase2-validation.json').write_text(json.dumps(checks,indent=2),encoding='utf-8')
print(json.dumps(checks,indent=2))
