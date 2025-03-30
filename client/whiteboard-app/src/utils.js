export function convertColourToRGBA(colour) {
  let colourNames = {
    red: "#ff0000",
    blue: "#0000ff",
    green: "#008000",
    white: "#ffffff",
    black: "#000000",
    yellow: "#ffff00",
  };
  let RGBA = {
    r: parseInt(colourNames[colour].slice(1, 3), 16),
    g: parseInt(colourNames[colour].slice(3, 5), 16),
    b: parseInt(colourNames[colour].slice(5, 7), 16),
    a: 255,
  };
  return RGBA;
}
