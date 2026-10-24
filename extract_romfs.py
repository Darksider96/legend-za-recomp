import os
import struct
import subprocess
import sys
import shutil

base_dir = r"C:\Users\johna\OneDrive\Documentos\legend za"
nsp_path = os.path.join(base_dir, "Pokémon Legends Z-A Switch NSP Base Game.nsp")
nca_path = os.path.join(base_dir, "program.nca")
romfs_dir = os.path.join(base_dir, "romfs")
keys_path = os.path.join(base_dir, "prod.keys")
titlekeys_path = os.path.join(base_dir, "title.keys")
hactool_path = r"C:\Users\johna\OneDrive\Desktop\SAK_64bit\bin\hactoolnet.exe"

print(f"Lendo cabeçalho do NSP: {nsp_path}")
with open(nsp_path, 'rb') as f:
    magic, num_files, str_size, res = struct.unpack('<4sIII', f.read(16))
    if magic != b'PFS0':
        print(f"Erro: Formato inválido {magic}")
        sys.exit(1)
    
    entries = []
    for _ in range(num_files):
        offset, size, str_offset, _ = struct.unpack('<QQII', f.read(24))
        entries.append((offset, size, str_offset))
    
    str_table = f.read(str_size)
    header_end = 16 + 24 * num_files + str_size
    
    target_offset = None
    target_size = None
    target_name = None
    for offset, size, str_offset in entries:
        name = str_table[str_offset:].split(b'\x00')[0].decode('utf-8')
        if name.endswith('.nca') and size > 1024 * 1024 * 1024:
            target_offset = header_end + offset
            target_size = size
            target_name = name
            break

if not target_name:
    print("Erro: Program NCA não encontrado.")
    sys.exit(1)

print(f"Encontrado Program NCA: {target_name} ({target_size / (1024*1024):.2f} MB)")
print("Extraindo Program NCA temporário para decodificação da RomFS...")

with open(nsp_path, 'rb') as src, open(nca_path, 'wb') as dst:
    src.seek(target_offset)
    bytes_left = target_size
    chunk_size = 64 * 1024 * 1024 # 64MB chunks
    last_reported = 0
    while bytes_left > 0:
        to_read = min(bytes_left, chunk_size)
        data = src.read(to_read)
        if not data:
            break
        dst.write(data)
        bytes_left -= len(data)
        written = target_size - bytes_left
        mb_written = int(written / (1024 * 1024))
        if mb_written - last_reported >= 512:
            print(f"Progresso extração NCA: {mb_written} MB / {int(target_size / (1024*1024))} MB")
            last_reported = mb_written

print("Extração do Program NCA concluída com sucesso.")
print(f"Executando hactoolnet para extrair a partição RomFS para: {romfs_dir}...")

cmd = [
    hactool_path,
    "-k", keys_path,
    "--titlekeys", titlekeys_path,
    "--disablekeywarns",
    "--romfsdir", romfs_dir,
    nca_path
]

res = subprocess.run(cmd, capture_output=True, text=True)
print("Saída do hactoolnet:")
print(res.stdout[:500] if len(res.stdout) > 500 else res.stdout)
if res.stderr:
    print("Stderr do hactoolnet:")
    print(res.stderr[:500] if len(res.stderr) > 500 else res.stderr)

print("\nRemovendo arquivo temporário program.nca para liberar espaço...")
if os.path.exists(nca_path):
    os.remove(nca_path)
    print("program.nca removido com sucesso!")

print("\n--- Verificação final da pasta RomFS ---")
if os.path.exists(romfs_dir):
    for root, dirs, files in os.walk(romfs_dir):
        for f in files[:20]:
            rel = os.path.relpath(os.path.join(root, f), romfs_dir)
            sz = os.path.getsize(os.path.join(root, f)) / (1024*1024)
            print(f"  [+] {rel}: {sz:.2f} MB")
        if len(files) > 20:
            print(f"  ... e mais {len(files) - 20} arquivos.")
        break
else:
    print("Pasta RomFS não foi criada.")

print("\nConcluído!")
