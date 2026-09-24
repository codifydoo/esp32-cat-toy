// Cat geometry ported from design/claude-design/Cat.dc.html (200 x 210 design canvas).
// Parts are drawn in design coordinates; the exporter renders the viewBox CANVAS.
export const CANVAS = { x: -20, y: -20, w: 240, h: 250 };

export const VARIANTS = {
  orange: { fur: '#FF9F43', stripe: '#D9661A', muzzle: '#FFE6C7', line: '#6B2E0A', iris: '#2A1633', nose: '#FF6F91', ear: '#FF9DB5', stripes: true, blaze: false },
  grey: { fur: '#A9B6C9', stripe: '#7483A0', muzzle: '#EEF2F8', line: '#2C3547', iris: '#1E2233', nose: '#FF7FA0', ear: '#FFB0C4', stripes: true, blaze: false },
  tux: { fur: '#3B3B52', stripe: '#3B3B52', muzzle: '#F6F6FA', line: '#C9CDEB', iris: '#FFD84A', nose: '#FF7FA0', ear: '#FF9DB5', stripes: false, blaze: true },
};

// Meow piano: one cat per note (Spec.dc.html "Piano pitches"), recolored the way Cat.dc.html recolors a
// cat given `fur` and `line` (on the orange base). Exported at scale S only (spec §3.11).
export const PIANO_CATS = {
  piano_c: { fur: '#FF5A5A', line: '#6E1616' },
  piano_d: { fur: '#FF9F3A', line: '#6B3A06' },
  piano_e: { fur: '#FFD84A', line: '#6B5400' },
  piano_g: { fur: '#5CD67A', line: '#1D5A2B' },
  piano_a: { fur: '#5AA8FF', line: '#173F74' },
};
export const pianoPalette = ({ fur, line }) => ({
  ...VARIANTS.orange,
  fur,
  line,
  stripes: false,
  blaze: false,
  muzzle: '#FFF3E0',
  iris: '#2A1633',
});

const circ = (x, y, r) => `M${x - r} ${y} a${r} ${r} 0 1 0 ${2 * r} 0 a${r} ${r} 0 1 0 ${-2 * r} 0 Z `;
const star4 = (x, y, r) => `M${x} ${y - r} Q${x} ${y} ${x + r} ${y} Q${x} ${y} ${x} ${y + r} Q${x} ${y} ${x - r} ${y} Q${x} ${y} ${x} ${y - r} Z `;

function spiral(cx, cy) {
  let s = '';
  for (let i = 0; i <= 44; i++) {
    const t = (i / 44) * Math.PI * 5;
    const r = 1 + i * 0.3;
    s += `${i ? 'L' : 'M'}${(cx + r * Math.cos(t)).toFixed(1)} ${(cy + r * Math.sin(t)).toFixed(1)} `;
  }
  return s;
}

function mane(rIn, rOut) {
  let s = '';
  for (let i = 0; i < 36; i++) {
    const a = (i / 36) * Math.PI * 2;
    const r = i % 2 ? rIn : rOut;
    s += `${i ? 'L' : 'M'}${(100 + r * Math.cos(a)).toFixed(1)} ${(110 + r * Math.sin(a) * 0.94).toFixed(1)} `;
  }
  return `${s}Z`;
}

const EARS = {
  normal: { l: 'M40 80 L48 22 L92 52 Z', r: 'M160 80 L152 22 L108 52 Z', li: 'M53 66 L56 36 L80 52 Z', ri: 'M147 66 L144 36 L120 52 Z' },
  perk: { l: 'M44 76 L50 6 L94 50 Z', r: 'M156 76 L150 6 L106 50 Z', li: 'M56 62 L56 24 L82 50 Z', ri: 'M144 62 L144 24 L118 50 Z' },
  droop: { l: 'M34 96 L20 46 L78 58 Z', r: 'M166 96 L180 46 L122 58 Z', li: 'M40 84 L32 58 L66 62 Z', ri: 'M160 84 L168 58 L134 62 Z' },
};
const TAIL = 'M146 184 Q194 186 186 146 Q182 128 166 134';
const STRIPES = 'M100 50 L100 64 M85 53 L88 65 M115 53 L112 65 M29 100 L44 104 M31 114 L44 114 M171 100 L156 104 M169 114 L156 114';

const stroke = (d, color, w) => `<path d="${d}" fill="none" stroke="${color}" stroke-width="${w}" stroke-linecap="round" stroke-linejoin="round"/>`;

export function catParts(c) {
  const line = (d, w) => stroke(d, c.line, w);
  const filled = (d, tongue) => `<path d="${d}" fill="#3A0F1E" stroke="${c.line}" stroke-width="4" stroke-linejoin="round"/>` + (tongue ? `<path d="${tongue}" fill="#FF7F9E"/>` : '');
  const ears = (e) =>
    [e.l, e.r].map((d) => `<path d="${d}" fill="${c.fur}" stroke="${c.line}" stroke-width="6" stroke-linejoin="round"/>`).join('') +
    [e.li, e.ri].map((d) => `<path d="${d}" fill="${c.ear}"/>`).join('');
  const irises = (r) => [72, 128].map((x) => `<circle cx="${x}" cy="104" r="${r}" fill="${c.iris}" stroke="${c.line}" stroke-width="4"/>`).join('');
  const tail = (deg) => `<g transform="rotate(${deg} 146 184)">${stroke(TAIL, c.line, 22)}${stroke(TAIL, c.fur, 12)}</g>`;
  const spiralEyes = (deg) => [72, 128].map((x) => `<g transform="rotate(${deg} ${x} 104)">${line(spiral(x, 104), 5)}</g>`).join('');
  const zz = (dy) => `<g transform="translate(0 ${dy})">${stroke('M150 34 L168 34 L150 52 L168 52 M174 10 L186 10 L174 22 L186 22', '#A9B8FF', 5)}</g>`;
  const purr = (dx) => `<g transform="translate(${dx} 0)">${stroke('M-2 150 Q4 142 10 150 Q16 158 22 150 M178 150 Q184 142 190 150 Q196 158 202 150 M4 170 Q10 162 16 170 M184 170 Q190 162 196 170', '#FFB0C8', 5)}</g>`;
  const robot = (ball) =>
    stroke('M100 12 L100 34', '#3A4A66', 6) +
    `<circle cx="100" cy="12" r="9" fill="${ball}" stroke="#3A4A66" stroke-width="4"/>` +
    '<path d="M30 90 Q30 32 100 32 Q170 32 170 90 Z" fill="#C3CFE0" stroke="#3A4A66" stroke-width="6" stroke-linejoin="round"/>' +
    stroke('M34 78 L166 78', '#7F93B3', 8) +
    '<circle cx="64" cy="56" r="5" fill="#3A4A66"/><circle cx="136" cy="56" r="5" fill="#3A4A66"/>' +
    '<circle cx="24" cy="112" r="12" fill="#C3CFE0" stroke="#3A4A66" stroke-width="5"/><circle cx="176" cy="112" r="12" fill="#C3CFE0" stroke="#3A4A66" stroke-width="5"/>';
  const miner = (lamp) =>
    '<path d="M40 70 Q42 28 100 28 Q158 28 160 70 Z" fill="#FFC23A" stroke="#6B4A00" stroke-width="6" stroke-linejoin="round"/>' +
    stroke('M28 72 L172 72', '#E09A10', 12) +
    `<circle cx="100" cy="46" r="12" fill="${lamp}" stroke="#6B4A00" stroke-width="5"/>`;

  return {
    body: `<ellipse cx="100" cy="178" rx="54" ry="28" fill="${c.fur}" stroke="${c.line}" stroke-width="6"/><ellipse cx="100" cy="184" rx="${c.blaze ? 34 : 26}" ry="16" fill="${c.muzzle}"/>`,
    tail_f0: tail(0),
    tail_f1: tail(6),
    tail_f2: tail(12),
    head:
      `<ellipse cx="100" cy="108" rx="74" ry="62" fill="${c.fur}" stroke="${c.line}" stroke-width="6"/>` +
      (c.stripes ? stroke(STRIPES, c.stripe, 6) : '') +
      (c.blaze ? `<path d="M100 64 Q90 92 82 114 Q100 106 118 114 Q110 92 100 64 Z" fill="${c.muzzle}"/>` : '') +
      `<ellipse cx="100" cy="130" rx="34" ry="21" fill="${c.muzzle}"/>` +
      `<path d="M93 118 L107 118 L100 126 Z" fill="${c.nose}" stroke="${c.line}" stroke-width="3" stroke-linejoin="round"/>` +
      line('M38 120 L62 125 M38 134 L62 132 M162 120 L138 125 M162 134 L138 132', 4),
    ears_normal: ears(EARS.normal),
    ears_perk: ears(EARS.perk),
    ears_droop: ears(EARS.droop),
    eyes_open: irises(13),
    eyes_half:
      `<path d="M58 104 L86 104 Q86 119 72 119 Q58 119 58 104 Z M114 104 L142 104 Q142 119 128 119 Q114 119 114 104 Z" fill="${c.iris}" stroke="${c.line}" stroke-width="4" stroke-linejoin="round"/>` +
      line('M54 103 L90 103 M110 103 L146 103', 7),
    eyes_closed: line('M60 102 Q72 114 84 102 M116 102 Q128 114 140 102', 7),
    eyes_happy: line('M60 108 Q72 94 84 108 M116 108 Q128 94 140 108', 7),
    eyes_big: irises(17), // the pupils are separate so they can look around (design: big eyes move too)
    eyes_spiral_f0: spiralEyes(0),
    eyes_spiral_f1: spiralEyes(90),
    eyes_spiral_f2: spiralEyes(180),
    eyes_spiral_f3: spiralEyes(270),
    pupil: '<circle cx="72" cy="104" r="9" fill="#140C1C"/>',
    highlight: '<circle cx="76" cy="99" r="4" fill="#FFFFFF"/>',
    pupil_big: '<circle cx="72" cy="104" r="12" fill="#140C1C"/>',
    highlight_big: '<circle cx="76" cy="98" r="5" fill="#FFFFFF"/>',
    mouth_w: line('M84 128 Q92 138 100 128 Q108 138 116 128', 5),
    mouth_smallw: line('M90 130 Q95 135 100 130 Q105 135 110 130', 5),
    mouth_small: line('M92 132 Q100 137 108 132', 5),
    mouth_sleepy: line('M94 133 Q100 129 106 133', 5),
    mouth_wavy: line('M82 134 Q88 127 94 134 Q100 141 106 134 Q112 127 118 134', 5),
    mouth_talk_b: filled('M88 128 Q100 146 112 128 Q100 132 88 128 Z'),
    mouth_talk_c: filled('M80 126 Q100 162 120 126 Q100 131 80 126 Z', 'M90 138 Q100 133 110 138 Q100 144 90 138 Z'),
    mouth_open: filled('M78 126 Q100 166 122 126 Q100 132 78 126 Z', 'M88 140 Q100 134 112 140 Q100 148 88 140 Z'),
    mouth_o: filled(circ(100, 138, 9)),
    mouth_osmall: filled(circ(100, 136, 5)),
    mouth_munch: filled('M88 128 Q100 144 112 128 Q100 132 88 128 Z'),
    mouth_yawn: filled('M86 128 Q86 170 100 170 Q114 170 114 128 Q100 122 86 128 Z', 'M90 158 Q100 150 110 158 Q100 166 90 158 Z'),
    paw: `<ellipse cx="100" cy="172" rx="17" ry="13" fill="${c.fur}" stroke="${c.line}" stroke-width="5"/>`,
    blush: '<ellipse cx="52" cy="126" rx="11" ry="7" fill="#FF7FA0" opacity="0.75"/><ellipse cx="148" cy="126" rx="11" ry="7" fill="#FF7FA0" opacity="0.75"/>',
    mouse_ears: [42, 158].map((x) => `<circle cx="${x}" cy="46" r="36" fill="#C9C9D8" stroke="${c.line}" stroke-width="6"/><circle cx="${x}" cy="46" r="21" fill="#FFA6C9"/>`).join(''),
    lion_mane_f0: `<path d="${mane(80, 100)}" fill="#D9822B" stroke="#6B3A0C" stroke-width="6" stroke-linejoin="round"/>`,
    lion_mane_f1: `<path d="${mane(84, 105)}" fill="#D9822B" stroke="#6B3A0C" stroke-width="6" stroke-linejoin="round"/>`,
    lion_ears: [54, 146].map((x) => `<circle cx="${x}" cy="60" r="16" fill="${c.fur}" stroke="${c.line}" stroke-width="6"/>`).join(''),
    robot_helmet_f0: robot('#FF5A5A'),
    robot_helmet_f1: robot('#FFD84A'),
    miner_hat_f0: miner('#FFF6B0'),
    miner_hat_f1: miner('#FFE070'),
    fx_waves: stroke('M180 70 Q189 84 180 98 M192 60 Q206 84 192 108 M20 70 Q11 84 20 98 M8 60 Q-6 84 8 108', '#4FD8FF', 6),
    fx_purr_f0: purr(0),
    fx_purr_f1: purr(3),
    fx_zz_f0: stroke('M156 40 L170 40 L156 54 L170 54', '#A9B8FF', 5),
    fx_zz_f1: zz(0),
    fx_zz_f2: zz(-6),
    fx_stars: `<path d="${star4(58, 36, 11) + star4(100, 22, 13) + star4(142, 36, 11)}" fill="#FFD84A"/>`,
    fx_sparkles: `<path d="${star4(22, 28, 12) + star4(178, 22, 14) + star4(186, 120, 9) + star4(14, 124, 9)}" fill="#FFE58A"/>`,
    fx_note: stroke('M172 8 L172 42', '#FFFFFF', 6) + '<ellipse cx="163" cy="44" rx="11" ry="8" fill="#FFFFFF"/><path d="M172 8 Q188 12 190 28 Q182 20 172 20 Z" fill="#FFFFFF"/>',
    fx_shh: stroke('M130 132 Q137 137 130 143 M138 126 Q150 137 138 149', '#A9B8FF', 4),
    prop_fish:
      '<path d="M62 166 Q90 144 118 166 Q90 188 62 166 Z" fill="#6FD3FF" stroke="#0F5A85" stroke-width="5" stroke-linejoin="round"/>' +
      '<path d="M116 166 L138 150 L138 182 Z" fill="#6FD3FF" stroke="#0F5A85" stroke-width="5" stroke-linejoin="round"/>' +
      '<circle cx="76" cy="162" r="4" fill="#0F5A85"/>',
    prop_broom:
      stroke('M170 38 L120 180', '#A8692E', 9) +
      '<path d="M100 172 L142 184 L134 207 L88 195 Z" fill="#FFD27A" stroke="#7A4A10" stroke-width="5" stroke-linejoin="round"/>' +
      stroke('M104 190 L100 200 M117 194 L113 204 M129 197 L125 206', '#7A4A10', 4),
  };
}
