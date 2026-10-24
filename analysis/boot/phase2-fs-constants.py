from image_helpers import *

def cstr(a):
 return mem(a,128).split(b'\0')[0].decode('ascii')
for a in [0x3166a0e,0x3cef3a4,0x314c8f2,0x30f98d6,0x30333c5,0x30c501a,0x3142805,0x3090211,0x309b31d]:
 print(f'STRING {BASE+a:010x} {cstr(a)!r}')
vtable=struct.unpack('<Q',mem(0x3cc99e0,8))[0]
print(f'PTR 7103cc99e0 -> {vtable:010x} +0x10 => vptr={vtable+0x10:010x}')
for offset in [0x40,0x50]:
 slot=vtable-BASE+0x10+offset
 target=struct.unpack('<Q',mem(slot,8))[0]
 print(f'VPTR_SLOT {BASE+slot:010x} vptr+{offset:#x} -> {target:010x}')
prefix=struct.unpack('<Q',mem(0x3cc5108,8))[0]-BASE
print(f'PTR 7103cc5108 -> {BASE+prefix:010x} {cstr(prefix)!r}')
print('STATIC_PATH_ASSEMBLY',cstr(prefix)+cstr(0x314c8f2)+cstr(0x30f98d6)+cstr(0x30333c5))
