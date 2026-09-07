# Generates the complete vehicle wiring schedule: every Pico pin, every module pin.
# Run from the repository root:  python tools/gen_wiring_schedule.py
import io

W, H = 1500, 1268
o = []
def add(s): o.append(s)

SPI   = '#1d4ed8'
I2C   = '#15803d'
UART  = '#7c3aed'
RADIO = '#0891b2'
P33   = '#c2650a'
PBAT  = '#c02626'
GND   = '#334155'
ADC   = '#be185d'
NONE  = '#94a3b8'

def esc(t):
    return t.replace('&', '&amp;').replace('<', '&lt;').replace('>', '&gt;')

add('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 %d %d" width="%d" height="%d" '
    'font-family="ui-sans-serif, system-ui, \'Segoe UI\', Roboto, Helvetica, Arial, sans-serif">'
    % (W, H, W, H))
add('<rect width="%d" height="%d" fill="#ffffff"/>' % (W, H))

add('<text x="40" y="46" font-size="26" font-weight="700" fill="#0f172a">CanSat 2026 — complete wiring schedule</text>')
add('<text x="40" y="72" font-size="13.5" fill="#64748b">Every Pico pin and every module pin. Pico physical numbering, USB to the left, component side up. Module pin order is silkscreen, as printed.</text>')

legend = [(PBAT, 'battery'), (P33, '3.3 V'), (GND, 'ground'), (SPI, 'SPI0'),
          (I2C, 'I2C0'), (UART, 'UART0'), (RADIO, 'radio ctrl'), (ADC, 'analogue'), (NONE, 'unused')]
x = 40
for col, name in legend:
    add('<rect x="%d" y="88" width="22" height="11" rx="2.5" fill="%s"/>' % (x, col))
    add('<text x="%d" y="98" font-size="11.5" fill="#334155">%s</text>' % (x + 28, name))
    x += 34 + len(name) * 7 + 26
add('<line x1="40" y1="112" x2="%d" y2="112" stroke="#e2e8f0" stroke-width="2"/>' % (W - 40))

RH = 21.0

def block(x0, y0, width, title, subtitle, rows, numcol=True):
    """rows: (pin_label, name, destination, colour)"""
    add('<text x="%.1f" y="%.1f" font-size="14.5" font-weight="700" fill="#0f172a">%s</text>' % (x0, y0, esc(title)))
    if subtitle:
        add('<text x="%.1f" y="%.1f" font-size="10.8" fill="#64748b">%s</text>' % (x0 + len(title) * 8.6 + 14, y0, esc(subtitle)))
    y = y0 + 12
    add('<line x1="%.1f" y1="%.1f" x2="%.1f" y2="%.1f" stroke="#cbd5e1" stroke-width="1.6"/>' % (x0, y, x0 + width, y))
    y += 5
    for i, (pin, name, dest, col) in enumerate(rows):
        yy = y + i * RH
        if i % 2 == 0:
            add('<rect x="%.1f" y="%.1f" width="%.1f" height="%.1f" fill="#f8fafc"/>' % (x0, yy, width, RH))
        add('<rect x="%.1f" y="%.1f" width="3.5" height="%.1f" fill="%s"/>' % (x0, yy, RH, col))
        tb = yy + 14.5
        if numcol:
            add('<text x="%.1f" y="%.1f" font-size="11.5" font-weight="700" fill="#0f172a" text-anchor="end" '
                'font-family="ui-monospace, Consolas, monospace">%s</text>' % (x0 + 34, tb, esc(pin)))
        add('<text x="%.1f" y="%.1f" font-size="11.5" font-weight="600" fill="%s" '
            'font-family="ui-monospace, Consolas, monospace">%s</text>' % (x0 + 44, tb, col if col != NONE else '#64748b', esc(name)))
        add('<text x="%.1f" y="%.1f" font-size="11.5" fill="%s">%s</text>'
            % (x0 + 152, tb, '#94a3b8' if col == NONE else '#1e293b', esc(dest)))
    return y + len(rows) * RH

# ---------------- Pico, top row: 40 .. 21 -----------------------------------
top = [
    ('40', 'VBUS',   'NEVER WIRE — live 5 V', NONE),
    ('39', 'VSYS',   'Schottky cathode  (anode ← switch ← batt +)', PBAT),
    ('38', 'GND',    'GND node → GND ring', GND),
    ('37', '3V3_EN', 'not connected', NONE),
    ('36', '3V3 OUT','test link → 3.3 V distribution node', P33),
    ('35', 'ADC_VREF','not connected', NONE),
    ('34', 'GP28',   'not connected  (zero-ref deferred, D-7)', NONE),
    ('33', 'AGND',   'GND node, ONE tie beside pin 38', ADC),
    ('32', 'GP27',   'LM393  AO', ADC),
    ('31', 'GP26',   'battery divider midpoint', ADC),
    ('30', 'RUN',    'not connected', NONE),
    ('29', 'GP22',   'RA-02  DIO1   (optional, never read)', RADIO),
    ('28', 'GND',    'spare ground — may join GND ring', GND),
    ('27', 'GP21',   'RA-02  DIO0', RADIO),
    ('26', 'GP20',   'RA-02  RST', RADIO),
    ('25', 'GP19',   'RA-02 MOSI  +  microSD MOSI', SPI),
    ('24', 'GP18',   'RA-02 SCK   +  microSD CLK', SPI),
    ('23', 'GND',    'spare ground', GND),
    ('22', 'GP17',   'RA-02  NSS', SPI),
    ('21', 'GP16',   'RA-02 MISO  +  microSD MISO', SPI),
]

bot = [
    ('1',  'GP0',  'not connected', NONE),
    ('2',  'GP1',  'not connected', NONE),
    ('3',  'GND',  'spare ground', GND),
    ('4',  'GP2',  'not connected', NONE),
    ('5',  'GP3',  'not connected', NONE),
    ('6',  'GP4',  'MPU-6500 SDA  +  BMP280 SDA', I2C),
    ('7',  'GP5',  'MPU-6500 SCL  +  BMP280 SCL', I2C),
    ('8',  'GND',  'spare ground', GND),
    ('9',  'GP6',  'microSD  CS', SPI),
    ('10', 'GP7',  'MPU-6500 INT  (optional, never read)', I2C),
    ('11', 'GP8',  'not connected', NONE),
    ('12', 'GP9',  'not connected', NONE),
    ('13', 'GND',  'spare ground', GND),
    ('14', 'GP10', 'not connected', NONE),
    ('15', 'GP11', 'not connected', NONE),
    ('16', 'GP12', 'NEO-6M  RX      (Pico transmits)', UART),
    ('17', 'GP13', 'NEO-6M  TX      (Pico receives)', UART),
    ('18', 'GND',  'spare ground', GND),
    ('19', 'GP14', '1 kΩ → status LED anode', P33),
    ('20', 'GP15', 'LM393  DO', ADC),
]

block(40, 140, 700, 'Pico — TOP row', 'left → right, USB end first', top)
block(760, 140, 700, 'Pico — BOTTOM row', 'left → right, USB end first', bot)

# ---------------- modules ---------------------------------------------------
MY = 640
ra = [
    ('J2·1', 'GND',  'GND ring', GND),
    ('J2·2', 'GND',  'star GND  ← capacitors here', GND),
    ('J2·3', '3.3V', 'star 3.3 V  ← capacitors here', P33),
    ('J2·4', 'RST',  'pin 26  GP20', RADIO),
    ('J2·5', 'DIO0', 'pin 27  GP21', RADIO),
    ('J2·6', 'DIO1', 'pin 29  GP22   (optional)', RADIO),
    ('J2·7', 'DIO2', 'not connected', NONE),
    ('J2·8', 'DIO3', 'not connected', NONE),
    ('J1·1', 'GND',  'not connected', NONE),
    ('J1·2', 'NSS',  'pin 22  GP17', SPI),
    ('J1·3', 'MOSI', 'pin 25  GP19', SPI),
    ('J1·4', 'MISO', 'pin 21  GP16', SPI),
    ('J1·5', 'SCK',  'pin 24  GP18', SPI),
    ('J1·6', 'DIO5', 'not connected', NONE),
    ('J1·7', 'DIO4', 'not connected', NONE),
    ('J1·8', 'GND',  'not connected', NONE),
]
block(40, MY, 440, 'SX1278 RA-02', 'J2 read from the u.FL end', ra)

sd = [
    ('1', 'GND',  'star GND  ← caps here', GND),
    ('2', 'MISO', 'pin 21  GP16', SPI),
    ('3', 'CLK',  'pin 24  GP18   (CLK, not SCK)', SPI),
    ('4', 'MOSI', 'pin 25  GP19', SPI),
    ('5', 'CS',   'pin 9   GP6', SPI),
    ('6', '3V3',  'star 3.3 V  ← caps here', P33),
]
y2 = block(520, MY, 440, 'microSD reader', 'GND and 3V3 at opposite ends', sd)

mpu = [
    ('1',  'VCC',   '3V3 ring', P33),
    ('2',  'GND',   'GND ring', GND),
    ('3',  'SCL',   'pin 7   GP5', I2C),
    ('4',  'SDA',   'pin 6   GP4', I2C),
    ('5',  'EDA',   'not connected', NONE),
    ('6',  'ECL',   'not connected', NONE),
    ('7',  'AD0',   'not connected — strapped low, 0x68', NONE),
    ('8',  'INT',   'pin 10  GP7   (optional)', I2C),
    ('9',  'NCS',   'not connected', NONE),
    ('10', 'FSYNC', 'not connected', NONE),
]
block(520, y2 + 34, 440, 'MPU-6500 IMU', 'sold as MPU-9250', mpu)

bmp = [
    ('1', 'VCC', '3V3 ring', P33),
    ('2', 'GND', 'GND ring', GND),
    ('3', 'SCL', 'pin 7   GP5', I2C),
    ('4', 'SDA', 'pin 6   GP4', I2C),
    ('5', 'CSB', 'not connected — strapped, I2C', NONE),
    ('6', 'SDO', 'not connected — strapped low, 0x76', NONE),
]
y3 = block(1000, MY, 440, 'GY-BMP280', '6-pin variant', bmp)

neo = [
    ('1', 'VCC', '3V3 ring', P33),
    ('2', 'RX',  'pin 16  GP12   (Pico TX)', UART),
    ('3', 'TX',  'pin 17  GP13   (Pico RX)', UART),
    ('4', 'GND', 'GND ring', GND),
]
y4 = block(1000, y3 + 34, 440, 'NEO-6M GPS', 'patch antenna faces up', neo)

lm = [
    ('1', 'AO',  'pin 32  GP27   short, own return', ADC),
    ('2', 'DO',  'pin 20  GP15', ADC),
    ('3', 'GND', 'GND ring, near the AGND tie', GND),
    ('4', 'VCC', '3V3 ring', P33),
]
block(1000, y4 + 34, 440, 'LM393 sound', '4-pin variant', lm)

# ---------------- discretes -------------------------------------------------
DY = 1055
disc = [
    ('', 'battery +', 'JST-RCY → switch → Schottky anode', PBAT),
    ('', 'Schottky', 'cathode → pin 39  VSYS   (band toward Pico)', PBAT),
    ('', 'battery −', 'GND ring', GND),
    ('', 'divider', 'switched node → 33 kΩ → tap → 33 kΩ → GND ring', ADC),
    ('', 'divider tap', 'pin 31  GP26      (no capacitor — D-7)', ADC),
    ('', 'status LED', 'pin 19 → 1 kΩ → anode; cathode → GND ring', P33),
    ('', 'power LED', '3V3 ring → 1 kΩ → anode; cathode → GND ring', P33),
    ('', 'test link', 'pin 36 → 2-pin header → 3.3 V node', P33),
]
block(40, DY, 700, 'Discretes', 'power path, divider, LEDs', disc, numcol=False)

caps = [
    ('', 'microSD', '100 µF 50 V ∥ 100 µF 25 V ∥ 104', P33),
    ('', 'RA-02', '10 µF 50 V ∥ 104', P33),
    ('', 'LM393', '104', P33),
    ('', 'MPU-6500', 'none — omitted, D-8', NONE),
    ('', 'BMP280', 'none — omitted, D-8', NONE),
    ('', 'NEO-6M', 'none — omitted, D-8', NONE),
    ('', 'GP26 divider', 'none — deferred, D-7', NONE),
    ('', '', 'six fitted · stripe → GND on every electrolytic', NONE),
]
block(760, DY, 700, 'Capacitors', 'all at each module’s own pins', caps, numcol=False)

add('<text x="40" y="%d" font-size="11.5" font-weight="700" fill="#991b1b">'
    'Pin 40 VBUS is live 5 V and nothing here tolerates 5 V. AGND (33) is a separate plane — tie it once, beside pin 38. '
    'Battery switch OFF whenever USB is connected until the Schottky is fitted.</text>' % (H - 22))

add('</svg>')
io.open('documentation/hardware/diagrams/wiring-schedule.svg', 'w', encoding='utf-8', newline='\n').write('\n'.join(o))
print('written')
