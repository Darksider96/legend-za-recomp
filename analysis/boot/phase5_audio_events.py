"""Reproduce the four cry Event IDs from the supplied executable string and banks.

Static only. Event IDs are not media IDs; SwitchContainer state can select
different descendants. No audio decoder or target execution is involved.
"""
from pathlib import Path
import hashlib
import struct
import json

HERE=Path(__file__).resolve().parent
ROOT=HERE.parent.parent

def wwise_hash(name):
    value=2166136261
    for byte in name.lower().encode('ascii'):
        value=((value*16777619)^byte)&0xffffffff
    return value

def parse_bank(path):
    b=path.read_bytes(); pos=0; chunks={}; objects={}
    while pos<len(b):
        assert pos+8<=len(b)
        tag,size=struct.unpack_from('<4sI',b,pos)
        assert pos+8+size<=len(b)
        chunks[tag.decode('ascii')]={'chunk_offset':pos,'data_offset':pos+8,'size':size}
        if tag==b'HIRC':
            count=struct.unpack_from('<I',b,pos+8)[0]; p=pos+12
            for _ in range(count):
                kind,length=struct.unpack_from('<BI',b,p)
                identifier=struct.unpack_from('<I',b,p+5)[0]
                assert p+5+length<=pos+8+size and identifier not in objects
                objects[identifier]={'type':kind,'object_offset':p,'length':length,
                                     'payload':b[p+9:p+5+length]}
                p+=5+length
            assert p==pos+8+size
        pos+=8+size
    return b,chunks,objects

bank_root=ROOT/'romfs/audio/wwise/bin/Switch'
bank_path=bank_root/'PM_VOICE.bnk'
data,chunks,objects=parse_bank(bank_path)
pm_data,pm_chunks,pm_objects=parse_bank(bank_root/'PM.bnk')
version=struct.unpack_from('<I',data,chunks['BKHD']['data_offset'])[0]
main_ro=(HERE/'rodata.bin').read_bytes()
fmt=main_ro[0x306cf3e-0x2fc8000:].split(b'\0',1)[0]
assert fmt==b'PLAY_PV_%04hu'
main_text=(HERE/'text.bin').read_bytes()
main_data=(HERE/'data.bin').read_bytes()
code_checks=[]
for site,target in {
    0x3cadf8:0x2fc3ac0,0x84f75c:0x2fc3ac0,0x9df094:0x2fc3ac0,
    0x84f8ac:0x84f90c,0x9df248:0x84f90c,0x84f938:0x3ad92c,
    0x3ad940:0x1c3fa54,0x1c41634:0x45f4a0,
    0x45f50c:0x45f590,0x45f568:0x45f590,
    0x84fc38:0x1c3fc80,0x1c415dc:0xcd30d0,0x1c415bc:0x2f0de80,
}.items():
    w=struct.unpack_from('<I',main_text,site)[0]
    imm=w&0x3ffffff
    if imm&0x2000000:imm-=0x4000000
    assert w&0x7c000000==0x14000000 and site+imm*4==target
    code_checks.append(dict(site=f'0x{0x7100000000+site:X}',target=f'0x{0x7100000000+target:X}',kind='B_or_BL'))
for slot,target in [(0xd8,0x1c41610),(0xb0,0x1c415d0),(0xa0,0x1c415a8)]:
    value=struct.unpack_from('<Q',main_data,0x3a42788+slot-0x39e1000)[0]
    assert value==0x7100000000+target
    code_checks.append(dict(vptr='0x7103A42788',slot=hex(slot),target=f'0x{value:X}',kind='vtable'))
for site in [0x1c3fa7c,0x1c3fc9c,0x1c3fc6c]:
    w=struct.unpack_from('<I',main_text,site)[0]
    assert w&0xfffffc1f==0xd63f0000
    code_checks.append(dict(site=f'0x{0x7100000000+site:X}',kind='BLR'))
records=[]
for species,label in [(150,'Mewtwo'),(25,'Pikachu'),(1,'Bulbasaur'),(8,'Wartortle')]:
    name=f'PLAY_PV_{species:04d}'
    identifier=wwise_hash(name)
    event=objects[identifier]
    assert event['type']==4
    # All four records have exactly one action: byte count + one little-endian ID.
    assert len(event['payload'])==5 and event['payload'][0]==1
    action_id=struct.unpack_from('<I',event['payload'],1)[0]
    action=objects[action_id]
    assert action['type']==3 and action['payload'][:2]==b'\x03\x04'
    target_id=struct.unpack_from('<I',action['payload'],2)[0]
    target=objects[target_id]
    assert target['type']==6
    records.append(dict(species_id=species,asset_species_key=f'pm{species:04d}',name=label,
        event_name=name,event_id=identifier,event_id_hex=f'0x{identifier:08X}',
        event_bank='PM_VOICE.bnk',hirc_event_type=4,
        event_object_file_offset=f"0x{event['object_offset']:X}",
        event_id_file_offset=f"0x{event['object_offset']+5:X}",
        action_id=action_id,action_object_file_offset=f"0x{action['object_offset']:X}",
        target_switch_container_id=target_id,
        target_object_file_offset=f"0x{target['object_offset']:X}",
        target_hirc_type=6,unique_media_id=None,
        media_note='One Event selects a SwitchContainer; no single unconditional Media ID is established.',
        evidence_status='verified_name_format_and_HIRC_event_action_target_chain'))

result=dict(schema_version=1,analysis_mode='static; no target execution',
    executable=dict(path='exefs/main',sha256='056eef7956b09414bfa0489b8de3ed0674393513f09a78d5312354cbd9557720',
        image_base='0x7100000000',format_va='0x710306CF3E',format_string=fmt.decode()),
    bank=dict(path='romfs/audio/wwise/bin/Switch/PM_VOICE.bnk',
        sha256=hashlib.sha256(data).hexdigest(),bkhd_version=version,
        hirc_objects=len(objects),hirc_events=sum(o['type']==4 for o in objects.values())),
    correction_to_initial_hypothesis=dict(PM_bnk_media_count=pm_chunks['DIDX']['size']//12,
        PM_bnk_event_count=sum(o['type']==4 for o in pm_objects.values()),
        four_cry_events_found_in='PM_VOICE.bnk'),
    hash_algorithm=dict(name='Wwise FNV-1 32, ASCII lowercase',offset_basis='0x811C9DC5',
        prime='0x01000193',recurrence='h = ((h * 16777619) XOR lowercase_byte) mod 2^32',
        confidence='Verified in main 0x710045F4A0 (multiply then XOR, ASCII A-Z to lowercase) and four exact HIRC Event objects.'),
    main_callsites=dict(
        format_construction=['0x71003CADF8','0x710084F75C','0x71009DF094'],
        post_wrappers=['0x710084F6F4','0x71009DF02C'],
        shared_dispatch='0x710084F90C',dispatch_callsite='0x710084F938',
        engine_post_event_wrapper='0x71003AD92C',backend_dispatch='0x7101C3FA54',
        backend_indirect_callsite='0x7101C3FA7C',backend_virtual_slot='0xD8',
        backend_vptr='0x7103A42788',backend_post_thunk='0x7101C41610',
        ak_post_event_string='0x710045F4A0',ak_post_event_numeric='0x710045F590',
        string_to_numeric_tail_callsite='0x710045F50C',
        form_switch_formatter='0x710084FB8C',form_switch_format='FORM%02hu',
        engine_set_switch='0x7101C3FC80',engine_set_switch_callsite='0x7101C3FC9C',
        backend_set_switch_thunk='0x7101C415D0',ak_set_switch_string='0x7100CD30D0',
        engine_set_rtpc='0x7101C3FC4C',engine_set_rtpc_callsite='0x7101C3FC6C',
        backend_set_rtpc_thunk='0x7101C415A8',ak_set_rtpc_string='0x7102F0DE80',
        pokemon_data_species_getter_vslot='0x1A8',pokemon_data_form_getter_vslot='0x1B0'),
    main_evidence_checks=code_checks,pokemon=records,
    limitations=['Event IDs are verified; selected media depends on form/context switch state.',
                 'No measured runtime execution or audio playback was performed.'])
(HERE/'phase5_wwise_events.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print(f'PASS: {len(records)} event/action/SwitchContainer chains; BKHD version {version}; PM DIDX count {pm_chunks["DIDX"]["size"]//12}')
