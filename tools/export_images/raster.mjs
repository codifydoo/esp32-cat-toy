import { Resvg } from '@resvg/resvg-js';
import { PNG } from 'pngjs';

// Render an SVG string to straight-alpha RGBA at the given pixel width.
export function render(svg, width) {
  const resvg = new Resvg(svg, { fitTo: { mode: 'width', value: width }, background: 'rgba(0, 0, 0, 0)' });
  const png = PNG.sync.read(resvg.render().asPng());
  return { w: png.width, h: png.height, rgba: png.data };
}

export function alphaBBox(img) {
  let x0 = img.w, y0 = img.h, x1 = -1, y1 = -1;
  for (let y = 0; y < img.h; y++) {
    for (let x = 0; x < img.w; x++) {
      if (img.rgba[(y * img.w + x) * 4 + 3] > 0) {
        if (x < x0) x0 = x;
        if (x > x1) x1 = x;
        if (y < y0) y0 = y;
        if (y > y1) y1 = y;
      }
    }
  }
  return x1 < 0 ? null : { x0, y0, x1, y1 };
}

export function unionBBox(a, b) {
  if (!a) return b;
  if (!b) return a;
  return { x0: Math.min(a.x0, b.x0), y0: Math.min(a.y0, b.y0), x1: Math.max(a.x1, b.x1), y1: Math.max(a.y1, b.y1) };
}

// Grow a box by one pixel where needed so width and height are even, without leaving the canvas.
export function evenBBox(bb, w, h) {
  const out = { ...bb };
  if ((out.x1 - out.x0 + 1) % 2) {
    if (out.x1 + 1 < w) out.x1 += 1;
    else if (out.x0 > 0) out.x0 -= 1;
  }
  if ((out.y1 - out.y0 + 1) % 2) {
    if (out.y1 + 1 < h) out.y1 += 1;
    else if (out.y0 > 0) out.y0 -= 1;
  }
  return out;
}

export function crop(img, bb) {
  const w = bb.x1 - bb.x0 + 1;
  const h = bb.y1 - bb.y0 + 1;
  const rgba = Buffer.alloc(w * h * 4);
  for (let y = 0; y < h; y++) {
    const start = ((bb.y0 + y) * img.w + bb.x0) * 4;
    img.rgba.copy(rgba, y * w * 4, start, start + w * 4);
  }
  return { w, h, rgba };
}

// LVGL v9 image file: 12-byte header (magic 0x19, cf RGB565A8 = 0x14), RGB565 plane, then A8 plane.
export function toLvglBin(img) {
  const { w, h, rgba } = img;
  const stride = w * 2;
  const buf = Buffer.alloc(12 + stride * h + w * h);
  buf[0] = 0x19;
  buf[1] = 0x14;
  buf.writeUInt16LE(0, 2);
  buf.writeUInt16LE(w, 4);
  buf.writeUInt16LE(h, 6);
  buf.writeUInt16LE(stride, 8);
  buf.writeUInt16LE(0, 10);
  const alpha = 12 + stride * h;
  for (let i = 0; i < w * h; i++) {
    const r = rgba[i * 4], g = rgba[i * 4 + 1], b = rgba[i * 4 + 2], a = rgba[i * 4 + 3];
    buf.writeUInt16LE(a === 0 ? 0 : ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3), 12 + i * 2);
    buf[alpha + i] = a;
  }
  return buf;
}

export function toPng(img) {
  const png = new PNG({ width: img.w, height: img.h });
  img.rgba.copy(png.data);
  return PNG.sync.write(png);
}
