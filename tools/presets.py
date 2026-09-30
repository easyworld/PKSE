"""Extract the appearance set + gender fields from all 8 PLA character presets."""
import sys, os

src = open("scblocks.py").read().split('raw = bytearray(')[0]
m = type(sys)("sc")
exec(compile(src, "scblocks.py", "exec"), m.__dict__)

ROOT = r"C:/Users/Kiasta/Desktop/PLA_Saves"
KFASH = 0x6B35BADB
KSTAT = 0xF25C070E
NAMES = ["Hair", "Contacts", "Eyebrows", "Glasses", "Hat", "Top",
         "Bottoms", "Outfit", "Shoes", "Accessory", "Unk50", "Skin"]


def load(path):
    raw = bytearray(open(path, "rb").read())
    p = m.static_xorpad(bytearray(raw[:-m.SIZE_HASH]))
    return {k: bytes(d) for k, t, s, d in m.parse(p)}


sets = {}
for g in ("Male", "Female"):
    for i in (1, 2, 3, 4):
        name = f"{g}{i}"
        b = load(os.path.join(ROOT, name, "main"))
        st = b[KSTAT]
        sets[name] = dict(
            fash=b[KFASH],
            g15=st[0x15],
            g3c=int.from_bytes(st[0x3C:0x40], "little"),
        )

print("=== gender fields ===")
for n, v in sets.items():
    print(f"  {n:8s} 0x15={v['g15']}  0x3C={v['g3c']}")

print("\n=== appearance per preset ===")
hdr = "slot".ljust(10) + "".join(n.ljust(18) for n in sets)
print(hdr)
for i, slot in enumerate(NAMES):
    row = slot.ljust(10)
    for n in sets:
        row += sets[n]["fash"][i * 8:i * 8 + 8].hex().ljust(18)
    print(row)

print("\n=== which slots vary ACROSS PRESETS within a gender? ===")
for g in ("Male", "Female"):
    print(f"  {g}:")
    for i, slot in enumerate(NAMES):
        vals = {sets[f'{g}{j}']["fash"][i * 8:i * 8 + 8] for j in (1, 2, 3, 4)}
        print(f"    {slot:10s} {len(vals)} distinct value(s)" + ("" if len(vals) > 1 else "  (same for all 4)"))

print("\n=== does preset N male share any slot value with preset N female? ===")
for j in (1, 2, 3, 4):
    same = [NAMES[i] for i in range(12)
            if sets[f'Male{j}']["fash"][i*8:i*8+8] == sets[f'Female{j}']["fash"][i*8:i*8+8]]
    print(f"  preset {j}: identical slots = {same}")
