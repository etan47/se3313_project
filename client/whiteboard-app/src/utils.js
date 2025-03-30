export const colourNameHex = {
  black: "#000000",
  red: "#ff0000",
  orange: "#ffa500",
  yellow: "#ffff00",
  green: "#008000",
  blue: "#0000ff",
  purple: "#800080",
  white: "#ffffff",
};

export const colourHexName = reverseMap(colourNameHex);

export const colourIntHex = {
  0: "#000000",
  1: "#ff0000",
  2: "#ffa500",
  3: "#ffff00",
  4: "#008000",
  5: "#0000ff",
  6: "#800080",
  7: "#ffffff",
};

export const colourHexInt = reverseMap(colourIntHex);

export function convertColourToRGBA(colour) {
  let RGBA = {
    r: parseInt(colourNameHex[colour].slice(1, 3), 16),
    g: parseInt(colourNameHex[colour].slice(3, 5), 16),
    b: parseInt(colourNameHex[colour].slice(5, 7), 16),
    a: 255,
  };
  return RGBA;
}

export function convertIntToRGBA(colour) {
  let RGBA = {
    r: parseInt(colourIntHex[colour].slice(1, 3), 16),
    g: parseInt(colourIntHex[colour].slice(3, 5), 16),
    b: parseInt(colourIntHex[colour].slice(5, 7), 16),
    a: 255,
  };
  return RGBA;
}

export function convertHextoRGBA(hex) {
  let RGBA = {
    r: parseInt(hex.slice(1, 3), 16),
    g: parseInt(hex.slice(3, 5), 16),
    b: parseInt(hex.slice(5, 7), 16),
    a: 255,
  };
  return RGBA;
}

function reverseMap(map) {
  const reversed = {};
  for (const key in map) {
    const value = map[key];
    reversed[value] = key;
  }
  return reversed;
}
