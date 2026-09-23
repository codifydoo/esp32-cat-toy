export const APP_ICONS = [
  { key: 'voices', bg: '#FF5FA2', ring: '#9E1F5C' },
  { key: 'pet', bg: '#FFB547', ring: '#9A5A00' },
  { key: 'draw', bg: '#8A7BFF', ring: '#3F2FA8' },
  { key: 'yarn', bg: '#2FC4A0', ring: '#0E6B55' },
  { key: 'clock', bg: '#3D7BFF', ring: '#16378F' },
  { key: 'piano', bg: '#FF7A45', ring: '#9A3510' },
];

const GLYPH = {
  voices: (a) =>
    '<path d="M50 84 Q36 112 50 140 M190 84 Q204 112 190 140" fill="none" stroke="#FFF6E6" stroke-width="10" stroke-linecap="round"/>' +
    '<path d="M72 112 Q72 166 120 166 Q168 166 168 112" fill="none" stroke="#FFF6E6" stroke-width="12" stroke-linecap="round"/>' +
    '<path d="M120 166 L120 190 M90 194 L150 194" fill="none" stroke="#FFF6E6" stroke-width="12" stroke-linecap="round"/>' +
    `<rect x="94" y="44" width="52" height="96" rx="26" fill="#FFF6E6" stroke="${a.ring}" stroke-width="8"/>` +
    `<path d="M107 76 L133 76 M107 96 L133 96 M107 116 L133 116" fill="none" stroke="${a.ring}" stroke-width="6" stroke-linecap="round"/>`,
  pet: (a) =>
    `<path d="M120 196 C62 158 42 120 58 90 C72 64 106 62 120 92 C134 62 168 64 182 90 C198 120 178 158 120 196 Z" fill="#FFF6E6" stroke="${a.ring}" stroke-width="8" stroke-linejoin="round"/>` +
    `<ellipse cx="120" cy="140" rx="24" ry="19" fill="${a.bg}"/>` +
    [[94, 114], [110, 100], [130, 100], [146, 114]].map(([x, y]) => `<circle cx="${x}" cy="${y}" r="9" fill="${a.bg}"/>`).join(''),
  draw: () =>
    '<path d="M46 170 A74 74 0 0 1 194 170" fill="none" stroke="#FF5A5A" stroke-width="16" stroke-linecap="round"/>' +
    '<path d="M70 170 A50 50 0 0 1 170 170" fill="none" stroke="#FFD84A" stroke-width="16" stroke-linecap="round"/>' +
    '<path d="M94 170 A26 26 0 0 1 146 170" fill="none" stroke="#5CE0FF" stroke-width="16" stroke-linecap="round"/>' +
    '<path d="M66 50 Q66 70 86 70 Q66 70 66 90 Q66 70 46 70 Q66 70 66 50 Z M178 58 Q178 72 192 72 Q178 72 178 86 Q178 72 164 72 Q178 72 178 58 Z" fill="#FFF6E6"/>',
  yarn: (a) =>
    '<path d="M158 158 Q196 170 186 198 Q180 212 164 204" fill="none" stroke="#FF7FB0" stroke-width="8" stroke-linecap="round"/>' +
    `<circle cx="112" cy="118" r="58" fill="#FF7FB0" stroke="${a.ring}" stroke-width="8"/>` +
    '<path d="M66 94 Q112 78 160 102 M58 122 Q112 106 168 130 M68 150 Q112 136 160 158 M92 68 Q140 108 124 174" fill="none" stroke="#B8356F" stroke-width="6" stroke-linecap="round"/>',
  clock: (a) =>
    `<circle cx="112" cy="124" r="62" fill="#FFE58A" stroke="${a.ring}" stroke-width="8"/>` +
    `<circle cx="142" cy="102" r="52" fill="${a.bg}"/>` +
    '<path d="M170 138 Q170 152 184 152 Q170 152 170 166 Q170 152 156 152 Q170 152 170 138 Z M168 52 Q168 62 178 62 Q168 62 168 72 Q168 62 158 62 Q168 62 168 52 Z" fill="#FFF6E6"/>',
  piano: (a) =>
    `<path d="M58 100 L70 60 L100 96 Z M182 100 L170 60 L140 96 Z" fill="#FFF6E6" stroke="${a.ring}" stroke-width="8" stroke-linejoin="round"/>` +
    `<rect x="46" y="94" width="148" height="88" rx="14" fill="#FFF6E6" stroke="${a.ring}" stroke-width="8"/>` +
    `<path d="M83 98 L83 178 M120 98 L120 178 M157 98 L157 178" fill="none" stroke="${a.ring}" stroke-width="5"/>` +
    `<rect x="72" y="98" width="22" height="46" rx="5" fill="${a.ring}"/><rect x="146" y="98" width="22" height="46" rx="5" fill="${a.ring}"/>`,
};

export function appIconSvg(app) {
  return (
    '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 240 240" width="240" height="240">' +
    `<circle cx="120" cy="120" r="113" fill="${app.bg}" stroke="${app.ring}" stroke-width="12"/>` +
    GLYPH[app.key](app) +
    '</svg>'
  );
}
