const fs = require('fs');
const {rezOfBytes} = require('./rez-utils');

console.log(`/*****************
* This file is generated by scripts/make-logo-pict.js, edits may be lost
*****************/`);

const boundingRect = {top: 0, left: 0, bottom: 0x6c, right: 0xa8};

function rectBytes(rect) {
  return [
	 ...wordBytes(rect.top),
	 ...wordBytes(rect.left),
	 ...wordBytes(rect.bottom),
	 ...wordBytes(rect.right),
  ];
}

function rectRegionBytes(rect) {
  return [
	 ...wordBytes(10),
	 ...wordBytes(rect.top),
	 ...wordBytes(rect.left),
	 ...wordBytes(rect.bottom),
	 ...wordBytes(rect.right),
  ];
}

function wordBytes(word) {
  return [0xff & (word >> 8), 0xff & word];
}

function longBytes(word) {
  return [0xff & (word >> 24), 0xff & (word >> 16), 0xff & (word >> 8), 0xff & word];
}

const header = [
  0x00, 0x11, 0x02, 0xff, 0x0c, 0x00, 0xff, 0xfe, // required specific header bytes
  0x00, 0x00, // reserved
  0x00, 0x48, 0x00, 0x00, // horizontal resolution 72dpi
  0x00, 0x48, 0x00, 0x00, // vertical resolution 72dpi
  ...rectBytes(boundingRect),
  0x00, 0x00, // reserved
];

function words(string) {
  return string.split(/\s+/).filter(x => x.length).flatMap(x => wordBytes(parseInt(x, 16)));
}

function fgColor(r, g, b) {
  return [...wordBytes(0x001a), r, r, g, g, b, b];
}

function clipRect(rect) {
  return [...wordBytes(0x0001), ...rectRegionBytes(rect)];
}

function fillRect(rect) {
  return [...wordBytes(0x0034), ...rectBytes(rect)];
}

function polyBytes(poly) {
  const bdrect = {
	 top: Math.min(...poly.map(p => p.y)),
	 left: Math.min(...poly.map(p => p.x)),
	 bottom: Math.max(...poly.map(p => p.y)),
	 right: Math.max(...poly.map(p => p.x)),
  };
  return [...wordBytes(2 + 8 + 4 * poly.length),
			 ...rectBytes(bdrect),
			 ...poly.flatMap(pt => {return [...wordBytes(pt.y), ...wordBytes(pt.x)]; })];
}

function paintPoly(poly) {
  return [...wordBytes(0x0071), ...polyBytes(poly)];
}

function endPict() {
 return wordBytes(0x00FF);
}

function defHilite() {
  return wordBytes(0x001e);
}

const image = [
  ...defHilite(),
  ...clipRect({top: 2, left: 2, bottom: 0x6e, right: 0xaa}),
  ...fgColor(0x66, 0x99, 0x66),
  ...fillRect({top: 2, left: 2, bottom: 0x6e, right: 0xaa}),
  ...fgColor(0xff, 0x00, 0x00),
  ...words(`005C 0008 0008`),
  ...fgColor(0x00, 0xff, 0x00),
  ...paintPoly([{x:2,y:0x6e}, {x:0x54,y:2}, {x:0xaa,y:0x6e}, {x:2,y:0x6e}]),
  ...endPict(),
]

const imageWithHeader = [
  ...header,
  ...image,
];

const imageRezBytes = [
  ...wordBytes(
	 2 + /* length of this length field */
	 8 + /* length of bounding Rect */
	 imageWithHeader.length),
  ...rectBytes(boundingRect),
  ...imageWithHeader
];

console.log(`
data 'PICT' (rAboutPict, purgeable) {
  ${rezOfBytes(imageRezBytes).replace(/ \$/g, '\n $')}
};`);
