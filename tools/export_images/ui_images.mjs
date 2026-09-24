const circ = (x, y, r) => `M${x - r} ${y} a${r} ${r} 0 1 0 ${2 * r} 0 a${r} ${r} 0 1 0 ${-2 * r} 0 Z `;
const PAW = 'M6.5 16.5 Q6.5 11 12 11 Q17.5 11 17.5 16.5 Q17.5 20 12 20 Q6.5 20 6.5 16.5 Z ' +
  circ(5, 9.5, 2.4) + circ(9.3, 5.6, 2.4) + circ(14.7, 5.6, 2.4) + circ(19, 9.5, 2.4);
const ARCH = 'M80 250 L80 160 Q80 36 233 36 Q386 36 386 160 L386 250';
// Magic paws brush button: black disc with a white ring at 45 % (Screen.dc.html draw view).
const BRUSH_DISC = '<circle cx="75" cy="75" r="70" fill="#000000" stroke="#FFFFFF" stroke-opacity="0.45" stroke-width="6"/>';
// The yarn with its strands turned by `deg`: four frames make one full roll (Assets.dc.html yarn_ball_f0-f3).
const yarn = (deg) =>
  '<circle cx="32" cy="32" r="27" fill="#FF7FB0" stroke="#A8326A" stroke-width="5"/>' +
  `<g transform="rotate(${deg} 32 32)"><path d="M10 26 Q32 16 54 30 M8 38 Q32 28 56 42 M22 9 Q42 30 34 58" fill="none" stroke="#B8356F" stroke-width="4" stroke-linecap="round"/></g>`;
const BASKET_WEAVE = (y0) =>
  `<path d="M36 ${y0 + 2} L42 ${y0 + 58} M60 ${y0 + 2} L60 ${y0 + 58} M84 ${y0 + 2} L78 ${y0 + 58} M14 ${y0 + 24} L106 ${y0 + 24} M18 ${y0 + 42} L102 ${y0 + 42}" fill="none" stroke="#6B3A0C" stroke-width="4"/>`;
const basket = (y0) =>
  `<path d="M8 ${y0} L112 ${y0} L98 ${y0 + 60} L22 ${y0 + 60} Z" fill="#C98B4A" stroke="#6B3A0C" stroke-width="6" stroke-linejoin="round"/>` +
  BASKET_WEAVE(y0) +
  `<path d="M6 ${y0} L114 ${y0}" fill="none" stroke="#E6A865" stroke-width="10" stroke-linecap="round"/>`;

export const UI_IMAGES = [
  { name: 'ui/paw_hint', viewBox: [0, 0, 24, 24], crop: false, svg: `<path d="${PAW}" fill="#FFFFFF"/>` },
  {
    name: 'ui/mic_glyph',
    viewBox: [0, 0, 220, 220],
    crop: true,
    svg:
      '<path d="M68 100 Q68 148 110 148 Q152 148 152 100" fill="none" stroke="#FFF6E6" stroke-width="11" stroke-linecap="round"/>' +
      '<path d="M110 148 L110 168 M84 172 L136 172" fill="none" stroke="#FFF6E6" stroke-width="11" stroke-linecap="round"/>' +
      '<rect x="88" y="42" width="44" height="80" rx="22" fill="#FFF6E6" stroke="#9E1F5C" stroke-width="7"/>',
  },
  {
    name: 'ui/cave_arch',
    viewBox: [0, 0, 466, 466],
    crop: true,
    svg:
      `<path d="${ARCH}" fill="none" stroke="#3B2F5C" stroke-width="34" stroke-linecap="round" stroke-linejoin="round"/>` +
      `<path d="${ARCH}" fill="none" stroke="#5A4A8A" stroke-width="10" stroke-linecap="round" stroke-dasharray="16 30"/>`,
  },
  {
    name: 'ui/fish',
    viewBox: [0, 0, 150, 150],
    crop: true,
    svg:
      '<path d="M30 75 Q64 42 100 75 Q64 108 30 75 Z" fill="#FFF6E6" stroke="#0F5A85" stroke-width="6" stroke-linejoin="round"/>' +
      '<path d="M98 75 L122 54 L122 96 Z" fill="#FFF6E6" stroke="#0F5A85" stroke-width="6" stroke-linejoin="round"/>' +
      '<circle cx="50" cy="70" r="6" fill="#0F5A85"/>',
  },
  {
    name: 'ui/ball',
    viewBox: [0, 0, 48, 48],
    crop: false,
    svg:
      '<circle cx="24" cy="24" r="20" fill="#FFD84A" stroke="#9A7A00" stroke-width="5"/>' +
      '<path d="M14 18 Q24 8 34 18" fill="none" stroke="#FFF6B0" stroke-width="4" stroke-linecap="round"/>',
  },
  {
    name: 'ui/heart',
    viewBox: [0, 0, 30, 30],
    size: 34,
    crop: false,
    svg: '<path d="M15 27 C4 19 1 12 4 7 C7 2 13 3 15 8 C17 3 23 2 26 7 C29 12 26 19 15 27 Z" fill="#FF7FB0"/>',
  },
  {
    name: 'ui/brush_rainbow',
    viewBox: [0, 0, 150, 150],
    crop: false,
    svg:
      BRUSH_DISC +
      '<path d="M35 95 A40 40 0 0 1 115 95" fill="none" stroke="#FF5A5A" stroke-width="12" stroke-linecap="round"/>' +
      '<path d="M53 95 A22 22 0 0 1 97 95" fill="none" stroke="#FFD84A" stroke-width="12" stroke-linecap="round"/>' +
      '<circle cx="75" cy="95" r="6" fill="#5CE0FF"/>',
  },
  {
    name: 'ui/brush_paw',
    viewBox: [0, 0, 150, 150],
    crop: false,
    svg:
      BRUSH_DISC +
      '<ellipse cx="75" cy="88" rx="22" ry="18" fill="#FFB0C8"/>' +
      '<circle cx="48" cy="62" r="9" fill="#FFB0C8"/><circle cx="65" cy="48" r="9" fill="#FFB0C8"/>' +
      '<circle cx="85" cy="48" r="9" fill="#FFB0C8"/><circle cx="102" cy="62" r="9" fill="#FFB0C8"/>',
  },
  {
    name: 'ui/draw_hint',
    viewBox: [0, 0, 160, 120],
    crop: false,
    svg:
      '<circle cx="20" cy="90" r="8" fill="#FF5A5A"/><circle cx="46" cy="66" r="8" fill="#FF9F3A"/>' +
      '<circle cx="76" cy="54" r="8" fill="#FFD84A"/><circle cx="106" cy="60" r="8" fill="#5CD67A"/>' +
      '<circle cx="132" cy="80" r="8" fill="#4FD8FF"/>',
  },
  {
    name: 'ui/dust',
    viewBox: [0, 0, 266, 60],
    crop: false,
    svg:
      '<circle cx="20" cy="40" r="14" fill="#8A8AA0"/><circle cx="44" cy="28" r="10" fill="#8A8AA0"/>' +
      '<circle cx="230" cy="36" r="14" fill="#8A8AA0"/><circle cx="250" cy="20" r="9" fill="#8A8AA0"/>',
  },
  ...[0, 90, 180, 270].map((deg, i) => ({ name: `ui/yarn_f${i}`, viewBox: [0, 0, 64, 64], crop: false, svg: yarn(deg) })),
  { name: 'ui/yarn_basket', viewBox: [0, 0, 120, 96], crop: false, svg: basket(30) },
  {
    name: 'ui/yarn_basket_full',
    viewBox: [0, 0, 120, 110],
    crop: false,
    svg: '<circle cx="60" cy="38" r="26" fill="#FF7FB0" stroke="#A8326A" stroke-width="5"/>' + basket(44),
  },
  {
    name: 'ui/tilt_hint',
    viewBox: [0, 0, 60, 44],
    crop: false,
    svg:
      '<rect x="6" y="10" width="48" height="24" rx="12" fill="none" stroke="#FFFFFF" stroke-opacity="0.5" stroke-width="5"/>' +
      '<circle cx="30" cy="22" r="5" fill="#FF7FB0"/>',
  },
  {
    name: 'ui/piano_note',
    viewBox: [0, 0, 100, 100],
    crop: false,
    svg:
      '<circle cx="50" cy="50" r="44" fill="none" stroke="#FFFFFF" stroke-opacity="0.6" stroke-width="6" stroke-dasharray="0.1 14" stroke-linecap="round"/>' +
      '<path d="M58 26 L58 62" fill="none" stroke="#FFFFFF" stroke-width="7" stroke-linecap="round"/>' +
      '<ellipse cx="48" cy="64" rx="12" ry="9" fill="#FFFFFF"/>' +
      '<path d="M58 26 Q74 30 76 46 Q68 38 58 38 Z" fill="#FFFFFF"/>',
  },
];
