import test from 'node:test';
import assert from 'node:assert/strict';
import { alphaBBox, unionBBox, evenBBox, crop, toLvglBin, render } from './raster.mjs';

const img = (w, h, pixels) => {
  const rgba = Buffer.alloc(w * h * 4);
  for (const [x, y, r, g, b, a] of pixels) rgba.set([r, g, b, a], (y * w + x) * 4);
  return { w, h, rgba };
};

test('alphaBBox finds every non-transparent pixel', () => {
  assert.deepEqual(alphaBBox(img(4, 3, [[1, 1, 255, 0, 0, 255], [2, 2, 0, 0, 255, 10]])), { x0: 1, y0: 1, x1: 2, y1: 2 });
  assert.equal(alphaBBox(img(2, 2, [])), null);
});

test('unionBBox merges boxes and ignores null', () => {
  assert.deepEqual(unionBBox(null, { x0: 1, y0: 2, x1: 3, y1: 4 }), { x0: 1, y0: 2, x1: 3, y1: 4 });
  assert.deepEqual(unionBBox({ x0: 1, y0: 2, x1: 3, y1: 4 }, { x0: 0, y0: 3, x1: 5, y1: 3 }), { x0: 0, y0: 2, x1: 5, y1: 4 });
});

test('evenBBox pads odd sizes and stays inside the canvas', () => {
  assert.deepEqual(evenBBox({ x0: 1, y0: 0, x1: 1, y1: 2 }, 4, 4), { x0: 1, y0: 0, x1: 2, y1: 3 });
  assert.deepEqual(evenBBox({ x0: 2, y0: 0, x1: 4, y1: 1 }, 5, 4), { x0: 1, y0: 0, x1: 4, y1: 1 });
  assert.deepEqual(evenBBox({ x0: 0, y0: 0, x1: 2, y1: 0 }, 3, 1), { x0: 0, y0: 0, x1: 2, y1: 0 });
});

test('crop copies the box', () => {
  const c = crop(img(3, 2, [[2, 1, 9, 8, 7, 6]]), { x0: 1, y0: 1, x1: 2, y1: 1 });
  assert.equal(c.w, 2);
  assert.equal(c.h, 1);
  assert.deepEqual([...c.rgba.subarray(4, 8)], [9, 8, 7, 6]);
});

test('toLvglBin writes an LVGL v9 RGB565A8 image', () => {
  const bin = toLvglBin(img(2, 1, [[0, 0, 255, 0, 0, 255], [1, 0, 0, 255, 0, 128]]));
  assert.equal(bin.length, 12 + 2 * 2 + 2);
  assert.deepEqual([...bin.subarray(0, 12)], [0x19, 0x14, 0, 0, 2, 0, 1, 0, 4, 0, 0, 0]);
  assert.equal(bin.readUInt16LE(12), 0xf800);
  assert.equal(bin.readUInt16LE(14), 0x07e0);
  assert.deepEqual([...bin.subarray(16)], [255, 128]);
});

test('toLvglBin stores black under fully transparent pixels', () => {
  const bin = toLvglBin(img(1, 1, [[0, 0, 255, 255, 255, 0]]));
  assert.equal(bin.readUInt16LE(12), 0);
  assert.equal(bin[14], 0);
});

test('render draws an SVG at the requested width', () => {
  const r = render('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 10 10"><rect width="5" height="10" fill="#fff"/></svg>', 20);
  assert.equal(r.w, 20);
  assert.equal(r.h, 20);
  assert.equal(r.rgba[3], 255);
  assert.equal(r.rgba[19 * 4 + 3], 0);
});
