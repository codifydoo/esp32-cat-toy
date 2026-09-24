const circ = (x, y, r) => `M${x - r} ${y} a${r} ${r} 0 1 0 ${2 * r} 0 a${r} ${r} 0 1 0 ${-2 * r} 0 Z `;
const PAW = 'M6.5 16.5 Q6.5 11 12 11 Q17.5 11 17.5 16.5 Q17.5 20 12 20 Q6.5 20 6.5 16.5 Z ' +
  circ(5, 9.5, 2.4) + circ(9.3, 5.6, 2.4) + circ(14.7, 5.6, 2.4) + circ(19, 9.5, 2.4);
const ARCH = 'M80 250 L80 160 Q80 36 233 36 Q386 36 386 160 L386 250';

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
];
