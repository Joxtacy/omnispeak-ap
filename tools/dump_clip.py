#!/usr/bin/env python3
"""Dump foreground tile clip attributes for CK5 Security Center (lvl 2)
around the Bag O' Sugar 9-14 cluster. Replicates omnispeak's
CAL_CarmackExpand / CAL_RLEWExpand / TI_Fore* exactly."""
import struct, sys, os

BIN = os.path.dirname(os.path.abspath(__file__))

def rd(name):
    with open(os.path.join(BIN, name), "rb") as f:
        return f.read()

# --- GFXINFOE: first 6 uint16 are numTiles8,8m,16,16m,32,32m ---
gfx = rd("GFXINFOE.CK5")
numTiles8, numTiles8m, numTiles16, numTiles16m = struct.unpack_from("<4H", gfx, 0)

# --- MAPHEAD: rleTag (u16) + 100 * int32 header offsets ---
mh = rd("MAPHEAD.CK5")
rleTag = struct.unpack_from("<H", mh, 0)[0]
headerOffsets = struct.unpack_from("<100I", mh, 2)

gm = rd("GAMEMAPS.CK5")
ti = rd("TILEINFO.CK5")

def carmack_expand(src, exp_len):
    # exp_len in bytes; work in words
    out = []
    n = exp_len // 2
    i = 0
    while len(out) < n:
        ch = struct.unpack_from("<H", src, i)[0]; i += 2
        hi = ch & 0xff00
        if hi == 0xA700:
            count = ch & 0xff
            if count == 0:
                b = src[i]; i += 1
                out.append((ch & 0xff00) | b)
            else:
                offset = src[i]; i += 1
                start = len(out) - offset
                for k in range(count):
                    out.append(out[start + k])
        elif hi == 0xA800:
            count = ch & 0xff
            if count == 0:
                b = src[i]; i += 1
                out.append((ch & 0xff00) | b)
            else:
                offset = struct.unpack_from("<H", src, i)[0]; i += 2
                for k in range(count):
                    out.append(out[offset + k])
        else:
            out.append(ch)
    return out  # list of words

def rlew_expand(words, exp_len_bytes, rletag):
    out = []
    n = exp_len_bytes // 2
    i = 0
    while len(out) < n and i < len(words):
        v = words[i]; i += 1
        if v != rletag:
            out.append(v)
        else:
            count = words[i]; i += 1
            value = words[i]; i += 1
            out.extend([value] * count)
    return out

def load_plane(map_index, plane):
    off = headerOffsets[map_index]
    # CA_MapHeader: 3*u32 planeOffsets, 3*u16 planeLengths, u16 w, u16 h, 16 name, 4 sig
    planeOffsets = struct.unpack_from("<3I", gm, off)
    planeLengths = struct.unpack_from("<3H", gm, off + 12)
    width, height = struct.unpack_from("<2H", gm, off + 18)
    name = gm[off+22:off+38].split(b"\0")[0].decode("latin1")
    plane_size = width * height * 2
    po, pl = planeOffsets[plane], planeLengths[plane]
    comp = gm[po:po+pl]
    carmack_exp_len = struct.unpack_from("<H", comp, 0)[0]
    words = carmack_expand(comp[2:], carmack_exp_len)
    # first word of carmack output is the RLEW-expanded length
    rlew = rlew_expand(words[1:], plane_size, rleTag)
    return width, height, name, rlew

# --- TI_Fore* (byte layout per id_ti.c) ---
base = numTiles16 * 2
def TI_ForeTop(t):    return ti[t + base]
def TI_ForeRight(t):  return ti[t + base + numTiles16m]
def TI_ForeBottom(t): return ti[t + base + numTiles16m*2]
def TI_ForeLeft(t):   return ti[t + base + numTiles16m*3]
def TI_ForeMisc(t):   return ti[t + base + numTiles16m*5]

LVL = 2  # Security Center
w, h, name, fg = load_plane(LVL, 1)
_, _, _, info = load_plane(LVL, 2)

ITEMNAMES = {4:"pts100",5:"pts200",6:"pts500",7:"pts1000",8:"pts2000",
             9:"BAGOSUGAR(5k)",10:"1-UP",11:"stunner"}
def decode_info(v):
    if 57 <= v <= 68:
        it = v - 57
        return f"item{it}={ITEMNAMES.get(it, '?')}"
    if v == 69: return "stunner-if-low"
    if v == 70: return "keycard"
    if v == 0:  return "."
    return f"#{v}"

print("\n=== INFO PLANE (plane 2) raw values + decoded item ===")
print("Sugar spawn = info value 66 (item9). Sugars: 9=(63,14) 10=(64,14) | 11-14=(62-65,15)\n")
for y in range(12, 18):
    row = []
    for x in range(58, 69):
        v = info[y*w + x]
        d = decode_info(v)
        row.append(f"({x},{y})={v}:{d}")
    print("  " + "  ".join(r for r in row if not r.endswith("=0:.")) or "  (all empty)")
print()
print(f"GFXINFOE: numTiles16={numTiles16} numTiles16m={numTiles16m}")
print(f"Level {LVL} '{name}'  width={w} height={h}  rleTag={rleTag:#06x}\n")

print("Wide solid/open map.  #=fully solid  ^=top-only(stand-on)  .=open")
print("Item markers: 9 0 = sugar9/10 (row14), 1 2 3 4 = sugar11-14 (row15),")
print("              c=2000pt candy, L=1-up.\n")
X0, X1 = 40, 70
Y0, Y1 = 4, 24
items = {(63,14):"9",(64,14):"0",(62,15):"1",(63,15):"2",(64,15):"3",(65,15):"4",
         (60,14):"c",(61,14):"c",(62,14):"c",(65,14):"L"}
hdr = "      " + "".join(str(x%10) for x in range(X0,X1+1))
print(hdr)
for y in range(Y0, Y1+1):
    line = []
    for x in range(X0, X1+1):
        if (x,y) in items:
            line.append(items[(x,y)]); continue
        t = fg[y*w + x]
        T,B,L,R = TI_ForeTop(t),TI_ForeBottom(t),TI_ForeLeft(t),TI_ForeRight(t)
        if T and B and L and R: line.append("#")
        elif T and not (B or L or R): line.append("^")
        elif T or B or L or R: line.append("+")
        else: line.append(".")
    print(f"y={y:2d}  " + "".join(line))
