# Generates a to-scale physical layout of the CanSat vehicle board.
# 100 x 100 mm single-sided perfboard, 2.54 mm pitch, drawn at 8 px/mm.
import io

S = 8.0            # px per mm
BX, BY = 60.0, 118.0   # board top-left in canvas px
BW = 100.0 * S     # board is 100 x 100 mm
M = 6.0            # mm from board edge to hole column 0
P = 2.54           # pitch, mm
NC = NR = 35       # 35 x 35 holes drawn

def hx(c): return BX + (M + c * P) * S
def hy(r): return BY + (M + r * P) * S
def mm(v): return v * S

o = []
def add(s): o.append(s)

W, H = 1360, 1030
add('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 %d %d" width="%d" height="%d" '
    'font-family="ui-sans-serif, system-ui, \'Segoe UI\', Roboto, Helvetica, Arial, sans-serif">' % (W, H, W, H))
add('<rect width="%d" height="%d" fill="#ffffff"/>' % (W, H))

add('<text x="40" y="46" font-size="26" font-weight="700" fill="#0f172a">Vehicle board — physical layout, to scale</text>')
add('<text x="40" y="72" font-size="14" fill="#64748b">100 × 100 mm single-sided perfboard · 2.54 mm pitch · drawn at 8 px/mm · component side, USB to the left</text>')
add('<text x="40" y="94" font-size="12.5" fill="#b45309">Positions of the Pico, RA-02, microSD and the four header strips follow your own dry fit. The passives are indicative — confirm free holes before soldering.</text>')

# ---- board -----------------------------------------------------------------
add('<rect x="%.1f" y="%.1f" width="%.1f" height="%.1f" rx="6" fill="#e8f2ea" stroke="#2f6f43" stroke-width="2.5"/>'
    % (BX, BY, BW, BW))

# ---- hole grid --------------------------------------------------------------
add('<g fill="#fdfffe" stroke="#c2d4c8" stroke-width="0.9">')
for r in range(NR):
    for c in range(NC):
        add('<circle cx="%.1f" cy="%.1f" r="4.6"/>' % (hx(c), hy(r)))
add('</g>')
add('<g fill="#e8f2ea">')
for r in range(NR):
    for c in range(NC):
        add('<circle cx="%.1f" cy="%.1f" r="1.9"/>' % (hx(c), hy(r)))
add('</g>')

# ---- corner mounting holes + keep-out --------------------------------------
for cx, cy in [(3.5, 3.5), (96.5, 3.5), (3.5, 96.5), (96.5, 96.5)]:
    px, py = BX + mm(cx), BY + mm(cy)
    add('<circle cx="%.1f" cy="%.1f" r="%.1f" fill="none" stroke="#94a3b8" stroke-width="1.4" stroke-dasharray="4 4"/>' % (px, py, mm(4.0)))
    add('<circle cx="%.1f" cy="%.1f" r="%.1f" fill="#ffffff" stroke="#475569" stroke-width="2"/>' % (px, py, mm(1.5)))

# ---- bus rings --------------------------------------------------------------
def ring(idx, colour, width):
    a, b = hx(idx), hx(NC - 1 - idx)
    t, d = hy(idx), hy(NR - 1 - idx)
    add('<rect x="%.1f" y="%.1f" width="%.1f" height="%.1f" fill="none" stroke="%s" stroke-width="%.1f" stroke-linejoin="round"/>'
        % (a, t, b - a, d - t, colour, width))

ring(0, '#0f172a', 6.0)     # GND ring, outermost
ring(1, '#c2650a', 5.0)     # 3V3 ring, just inside

# ---- footprint helper -------------------------------------------------------
def foot(c0, r0, c1, r1, label, sub, fill, stroke, pad=0.55, lx=None, ly=None, anchor='middle', fs=12.5):
    x0, y0 = hx(c0) - mm(pad * P), hy(r0) - mm(pad * P)
    x1, y1 = hx(c1) + mm(pad * P), hy(r1) + mm(pad * P)
    add('<rect x="%.1f" y="%.1f" width="%.1f" height="%.1f" rx="5" fill="%s" fill-opacity="0.92" stroke="%s" stroke-width="2.2"/>'
        % (x0, y0, x1 - x0, y1 - y0, fill, stroke))
    tx = lx if lx is not None else (x0 + x1) / 2
    ty = ly if ly is not None else (y0 + y1) / 2
    add('<text x="%.1f" y="%.1f" font-size="%.1f" font-weight="700" fill="#0f172a" text-anchor="%s">%s</text>' % (tx, ty, fs, anchor, label))
    if sub:
        add('<text x="%.1f" y="%.1f" font-size="10" fill="#475569" text-anchor="%s">%s</text>' % (tx, ty + 15, anchor, sub))

def pins(cells, colour, r=5.4):
    add('<g fill="%s">' % colour)
    for c, rr in cells:
        add('<circle cx="%.1f" cy="%.1f" r="%.1f"/>' % (hx(c), hy(rr), r))
    add('</g>')

AMB, INK, BLU, GRN = '#c2650a', '#0f172a', '#1d4ed8', '#15803d'

# ---- PICO: top row r14 = pins 40..21 (c8..c27); bottom row r21 = pins 1..20 --
foot(8, 13.3, 27, 21.8, '', '', '#f8fafc', '#334155', pad=0.35)
add('<text x="%.1f" y="%.1f" font-size="15" font-weight="700" fill="#0f172a" text-anchor="middle">Raspberry Pi Pico</text>'
    % ((hx(8) + hx(27)) / 2, hy(17.2)))
add('<text x="%.1f" y="%.1f" font-size="10.5" fill="#475569" text-anchor="middle">51 × 21 mm · 20 cols × 8 rows of holes</text>'
    % ((hx(8) + hx(27)) / 2, hy(18.3)))
# USB shell
add('<rect x="%.1f" y="%.1f" width="%.1f" height="%.1f" rx="2" fill="#cbd5e1" stroke="#475569" stroke-width="1.6"/>'
    % (hx(8) - mm(6.0), hy(16.6), mm(5.6), mm(7.6)))
add('<text x="%.1f" y="%.1f" font-size="10" font-weight="700" fill="#334155" text-anchor="end">USB</text>' % (hx(8) - mm(7.2), hy(17.9)))
pins([(c, 14) for c in range(8, 28)], INK, 5.0)
pins([(c, 21) for c in range(8, 28)], INK, 5.0)

# key pin callouts, top row: pin n sits at column 8 + (40 - n)
def topcol(pin): return 8 + (40 - pin)
def botcol(pin): return 8 + (pin - 1)

for pin, note in [(39, 'VSYS'), (38, 'GND'), (36, '3V3'), (33, 'AGND'), (32, 'AO'), (31, 'GP26'), (21, 'MISO')]:
    c = topcol(pin)
    pins([(c, 14)], AMB if pin in (36,) else ('#c02626' if pin == 39 else BLU), 6.2)
    add('<text x="%.1f" y="%.1f" font-size="9.5" font-weight="700" fill="#0f172a" text-anchor="middle">%d</text>' % (hx(c), hy(14) - 15, pin))
    add('<text x="%.1f" y="%.1f" font-size="8.5" fill="#475569" text-anchor="middle">%s</text>' % (hx(c), hy(14) - 26, note))

for pin, note in [(6, 'SDA'), (7, 'SCL'), (9, 'SD CS'), (16, 'TX'), (17, 'RX'), (19, 'LED'), (20, 'DO')]:
    c = botcol(pin)
    pins([(c, 21)], BLU, 6.2)
    add('<text x="%.1f" y="%.1f" font-size="9.5" font-weight="700" fill="#0f172a" text-anchor="middle">%d</text>' % (hx(c), hy(21) + 16, pin))
    add('<text x="%.1f" y="%.1f" font-size="8.5" fill="#475569" text-anchor="middle">%s</text>' % (hx(c), hy(21) + 27, note))

# ---- the 104 across pins 31 and 33 -----------------------------------------
c31, c33, c32 = topcol(31), topcol(33), topcol(32)
add('<rect x="%.1f" y="%.1f" width="%.1f" height="%.1f" rx="7" fill="none" stroke="#7c3aed" stroke-width="2.4" stroke-dasharray="5 4"/>'
    % (hx(c33) - 11, hy(14) - 11, (hx(c31) - hx(c33)) + 22, 22))
add('<text x="%.1f" y="%.1f" font-size="10.5" font-weight="700" fill="#6d28d9" text-anchor="middle">104 at pins 31–33 is DEFERRED — D-7</text>'
    % ((hx(c31) + hx(c33)) / 2, hy(14) + 20))
add('<text x="%.1f" y="%.1f" font-size="9.5" font-weight="700" fill="#b91c1c" text-anchor="middle">not fitted; step 14 decides</text>'
    % ((hx(c31) + hx(c33)) / 2, hy(14) + 33))

# ---- RA-02: pin columns c19 and c25, rows r3..r10 --------------------------
foot(19, 3, 25, 10, 'SX1278 RA-02', 'soldered down', '#dbeafe', '#1d4ed8', pad=0.8,
     ly=hy(6.0))
pins([(19, r) for r in range(3, 11)] + [(25, r) for r in range(3, 11)], BLU)
add('<circle cx="%.1f" cy="%.1f" r="7" fill="#fbbf24" stroke="#92400e" stroke-width="1.6"/>' % (hx(22), hy(2.4)))
add('<text x="%.1f" y="%.1f" font-size="9.5" font-weight="700" fill="#92400e" text-anchor="middle">u.FL → top edge</text>' % (hx(22), hy(2.0)))
add('<text x="%.1f" y="%.1f" font-size="9" fill="#1e3a8a" text-anchor="middle">NSS MOSI</text>' % (hx(25) + 34, hy(4.6)))
add('<text x="%.1f" y="%.1f" font-size="9" fill="#1e3a8a" text-anchor="middle">MISO SCK</text>' % (hx(25) + 34, hy(6.6)))
add('<text x="%.1f" y="%.1f" font-size="9" fill="#1e3a8a" text-anchor="middle">3.3V RST</text>' % (hx(19) - 34, hy(5.0)))
add('<text x="%.1f" y="%.1f" font-size="9" fill="#1e3a8a" text-anchor="middle">DIO0</text>' % (hx(19) - 34, hy(6.4)))

# ---- microSD: header column c28, body to c33, rows r3.5..r10 ---------------
foot(28, 4, 33, 9, 'microSD', 'soldered down', '#dbeafe', '#1d4ed8', pad=0.8, ly=hy(6.0))
pins([(28, r) for r in range(4, 10)], BLU)
add('<text x="%.1f" y="%.1f" font-size="9.5" font-weight="700" fill="#1e3a8a" text-anchor="middle">card slot → right edge</text>' % (hx(30.5), hy(10.9)))
add('<text x="%.1f" y="%.1f" font-size="9" fill="#1e3a8a" text-anchor="middle">100 µF ∥ 100 µF ∥ 104 at these pins</text>' % (hx(30.5), hy(12.0)))

# ---- LM393 strip: row r11, cols c12..c15 (AO nearest pin 32) ---------------
foot(8, 11, 11, 11, 'LM393 strip', '', '#dcfce7', '#15803d', pad=0.7, ly=hy(10.5), fs=11)
pins([(c, 11) for c in range(8, 12)], GRN)
add('<text x="%.1f" y="%.1f" font-size="9" fill="#14532d" text-anchor="middle">VCC GND DO AO → pin 32</text>' % (hx(9.5), hy(11) + 22))

# ---- battery divider: R1 row12 c17..c20, R2 row13 c17..c20 ----------------
def resistor(c0, c1, r, label, below=False):
    x0, x1, y = hx(c0), hx(c1), hy(r)
    add('<line x1="%.1f" y1="%.1f" x2="%.1f" y2="%.1f" stroke="#0f172a" stroke-width="2"/>' % (x0, y, x1, y))
    bw, bh = mm(6.5), mm(2.6)
    add('<rect x="%.1f" y="%.1f" width="%.1f" height="%.1f" rx="2" fill="#fff7ed" stroke="#0f172a" stroke-width="1.8"/>'
        % ((x0 + x1) / 2 - bw / 2, y - bh / 2, bw, bh))
    add('<circle cx="%.1f" cy="%.1f" r="3.4" fill="#0f172a"/>' % (x0, y))
    add('<circle cx="%.1f" cy="%.1f" r="3.4" fill="#0f172a"/>' % (x1, y))
    add('<text x="%.1f" y="%.1f" font-size="9" fill="#0f172a" text-anchor="middle">%s</text>' % ((x0 + x1) / 2, y + (19 if below else -14), label))

resistor(13, 16, 10, 'R1 33 kΩ — upper')
resistor(13, 16, 11, 'R2 33 kΩ — lower', below=True)
add('<polyline points="%.1f,%.1f %.1f,%.1f %.1f,%.1f" fill="none" stroke="#0f172a" stroke-width="2.4"/>'
    % (hx(16), hy(11), hx(17), hy(12.4), hx(17), hy(14)))
add('<text x="%.1f" y="%.1f" font-size="9" font-weight="700" fill="#0f172a" text-anchor="start">tap → pin 31</text>' % (hx(17) + 8, hy(12.35)))
add('<text x="%.1f" y="%.1f" font-size="8.5" fill="#475569" text-anchor="middle">battery divider</text>' % (hx(14.5), hy(9.35)))

# ---- power zone: left margin c2..c7 -----------------------------------------
foot(2, 12, 6, 20, 'POWER', '', '#fef3c7', '#b45309', pad=0.8, ly=hy(12.6), fs=12)
add('<text x="%.1f" y="%.1f" font-size="9.5" fill="#78350f" text-anchor="middle">3V3 node</text>' % (hx(4), hy(14.2)))
add('<text x="%.1f" y="%.1f" font-size="9.5" fill="#78350f" text-anchor="middle">test link</text>' % (hx(4), hy(15.6)))
add('<text x="%.1f" y="%.1f" font-size="9.5" fill="#78350f" text-anchor="middle">GND node</text>' % (hx(4), hy(17.0)))
add('<text x="%.1f" y="%.1f" font-size="9.5" fill="#78350f" text-anchor="middle">10 µF → RA-02</text>' % (hx(4), hy(18.4)))
add('<text x="%.1f" y="%.1f" font-size="9.5" fill="#78350f" text-anchor="middle">beside pins 36/38/39</text>' % (hx(4), hy(19.8)))
pins([(2, 13), (3, 13), (4, 13), (5, 13), (6, 13)], AMB, 5.0)

foot(2, 24, 5, 30, 'SWITCH +', 'battery entry', '#fee2e2', '#b91c1c', pad=0.8, ly=hy(26.2), fs=11)
add('<text x="%.1f" y="%.1f" font-size="9.5" fill="#7f1d1d" text-anchor="middle">silicone wire out</text>' % (hx(3.5), hy(28.6)))

# ---- bottom band strips -----------------------------------------------------
foot(7, 27, 12, 27, 'BMP280 strip', '6 pins', '#dcfce7', '#15803d', pad=0.7, ly=hy(28.6), fs=11)
pins([(c, 27) for c in range(7, 13)], GRN)
foot(15, 27, 24, 27, 'MPU-6500 strip', '10 pins · under pins 6 and 7', '#dcfce7', '#15803d', pad=0.7, ly=hy(28.6), fs=11)
pins([(c, 27) for c in range(15, 25)], GRN)
foot(27, 27, 30, 27, 'NEO-6M', '4 pins', '#dcfce7', '#15803d', pad=0.7, ly=hy(28.6), fs=11)
pins([(c, 27) for c in range(27, 31)], GRN)

# ---- LEDs -------------------------------------------------------------------
foot(31, 30, 33, 32, 'LEDs', '', '#ffe4e6', '#be123c', pad=0.7, ly=hy(30.6), fs=11)
add('<text x="%.1f" y="%.1f" font-size="9" fill="#881337" text-anchor="middle">status + power</text>' % (hx(32), hy(31.7)))
add('<text x="%.1f" y="%.1f" font-size="9" fill="#881337" text-anchor="middle">1 kΩ each</text>' % (hx(32), hy(32.5)))

# ---- scale bar --------------------------------------------------------------
sbx, sby = BX, BY + BW + 34
add('<line x1="%.1f" y1="%.1f" x2="%.1f" y2="%.1f" stroke="#0f172a" stroke-width="3"/>' % (sbx, sby, sbx + mm(25.4), sby))
add('<line x1="%.1f" y1="%.1f" x2="%.1f" y2="%.1f" stroke="#0f172a" stroke-width="3"/>' % (sbx, sby - 6, sbx, sby + 6))
add('<line x1="%.1f" y1="%.1f" x2="%.1f" y2="%.1f" stroke="#0f172a" stroke-width="3"/>' % (sbx + mm(25.4), sby - 6, sbx + mm(25.4), sby + 6))
add('<text x="%.1f" y="%.1f" font-size="11.5" fill="#0f172a">25.4 mm = 10 holes</text>' % (sbx + mm(25.4) + 12, sby + 4))

# ---- legend -----------------------------------------------------------------
LX = 900
add('<text x="%d" y="%.1f" font-size="15" font-weight="700" fill="#0f172a">What sits where</text>' % (LX, BY + 6))
rows = [
    ('#0f172a', 'GND ring', 'outermost ring of pads, all the way round'),
    ('#c2650a', '3V3 ring', 'the ring just inside it'),
    ('#1d4ed8', 'Soldered down', 'Pico, RA-02, microSD — into the grid'),
    ('#15803d', 'Header strips', 'IMU, BMP280, GPS, LM393 — 24 pins, jumpered'),
    ('#b45309', 'Power zone', 'nodes, test link, RA-02 bulk cap'),
    ('#b91c1c', 'Switch / battery', 'silicone wire leaves the board here'),
    ('#7c3aed', '104 at pins 31–33', 'deferred — not fitted, see D-7'),
]
y = BY + 34
for col, name, note in rows:
    add('<rect x="%d" y="%.1f" width="26" height="13" rx="3" fill="%s"/>' % (LX, y - 10, col))
    add('<text x="%d" y="%.1f" font-size="12.5" font-weight="700" fill="#0f172a">%s</text>' % (LX + 36, y, name))
    add('<text x="%d" y="%.1f" font-size="10.5" fill="#64748b">%s</text>' % (LX + 36, y + 15, note))
    y += 42

y += 6
add('<line x1="%d" y1="%.1f" x2="%d" y2="%.1f" stroke="#e2e8f0" stroke-width="2"/>' % (LX, y, 1320, y))
y += 26
add('<text x="%d" y="%.1f" font-size="14" font-weight="700" fill="#0f172a">Why each thing is there</text>' % (LX, y))
notes = [
    'RA-02 sits above pins 21–25, so every SPI',
    'run is 20–30 mm and near vertical.',
    '',
    'microSD next to it, on the same pins, with',
    'its card slot facing the right-hand edge.',
    '',
    'LM393 strip sits left of the divider; AO is',
    'its right-hand pin, nearest pin 32.',
    '',
    'Divider sits just above pins 32-35, so the',
    'high-impedance tap reaches pin 31 in a few',
    'millimetres. The long wire is the battery',
    'side, which is low impedance and does not',
    'care how far it travels.',
    '',
    'MPU strip sits under pins 6 and 7 — the',
    'shortest I2C run available on this board.',
    '',
    'Power zone hugs pins 36, 38 and 39, which',
    'is why the Pico must not slide left to make',
    'room for the USB. Cut the airframe instead.',
]
y += 22
for n in notes:
    if n:
        add('<text x="%d" y="%.1f" font-size="11.2" fill="#334155">%s</text>' % (LX, y, n))
    y += 15.5

add('</svg>')

io.open('documentation/hardware/diagrams/board-layout-to-scale.svg', 'w', encoding='utf-8', newline='\n').write('\n'.join(o))
print('written')
