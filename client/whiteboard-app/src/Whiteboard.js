// src/Whiteboard.js
import React, { useRef, useState, useEffect, useCallback } from "react";
import { colourHexInt, convertIntToRGBA, convertHextoRGBA } from "./utils";
import { openConnection } from "./Auth/serverConnection";

const Whiteboard = () => {
  const cWidth = 300;
  const cHeight = 300;
  const canvasRef = useRef(null);
  const contextRef = useRef(null);
  const [isDrawing, setIsDrawing] = useState(false);
  const finalPixels = useRef(new Set());
  const lastPos = useRef(null);
  const [thickness, setThickness] = useState(5);
  const previousPixels = useRef(new Set());
  const previousPixelsTimeout = useRef(null);

  const fetchUpdatedCanvas = useCallback(() => {
    openConnection
      .get("/getBoard")
      .then((response) => {
        console.log("Canvas data fetched");
        let pixels = [];
        for (let i = 0; i < response.data.length; i++) {
          for (let j = 0; j < response.data[i].length; j++) {
            pixels.push(convertIntToRGBA(response.data[i][j]));
          }
        }
        drawPixels(pixels);
      })
      .catch((error) => {
        console.error("Error fetching canvas data:", error);
      });
  }, []);

  // Setup canvas when component mounts
  useEffect(() => {
    const canvas = canvasRef.current;
    // Set canvas dimensions and scale for high resolution
    canvas.width = cWidth;
    canvas.height = cHeight;
    canvas.style.width = `${cWidth}px`;
    canvas.style.height = `${cHeight}px`;

    const context = canvas.getContext("2d");
    context.scale(1, 1);
    context.fillStyle = "Black";
    contextRef.current = context;

    const intervalId = setInterval(() => {
      fetchUpdatedCanvas();
    }, 250); // This feels like a lot, but it is necessary to get the pixels from the server.

    return () => clearInterval(intervalId);
  }, [fetchUpdatedCanvas]);

  // Start drawing on mouse down
  const startDrawing = ({ nativeEvent }) => {
    const { offsetX, offsetY } = nativeEvent;
    lastPos.current = { x: offsetX, y: offsetY };
    setIsDrawing(true);
  };

  const getLinePixels = (x0, y0, x1, y1) => {
    let pixels = [];
    let dx = Math.abs(x1 - x0);
    let dy = Math.abs(y1 - y0);
    let sx = x0 < x1 ? 1 : -1;
    let sy = y0 < y1 ? 1 : -1;
    let err = dx - dy;

    while (true) {
      pixels.push([x0, y0]);
      if (x0 === x1 && y0 === y1) break;
      let e2 = err * 2;
      if (e2 > -dy) {
        err -= dy;
        x0 += sx;
      }
      if (e2 < dx) {
        err += dx;
        y0 += sy;
      }
    }

    return pixels;
  };

  const drawPixel = (x, y) => {
    const half = Math.floor(thickness / 2);
    contextRef.current.fillRect(x - half, y - half, thickness, thickness);
  };

  // Draw as the mouse moves
  const draw = ({ nativeEvent }) => {
    if (!isDrawing || !lastPos.current) return;
    const { offsetX, offsetY } = nativeEvent;
    const newPixels = getLinePixels(
      lastPos.current.x,
      lastPos.current.y,
      offsetX,
      offsetY
    );
    lastPos.current = { x: offsetX, y: offsetY };

    const half = Math.floor(thickness / 2);
    newPixels.forEach((p) => {
      let x = p[0];
      let y = p[1];
      drawPixel(x, y);
      for (let i = x - half; i <= x + half; i++) {
        for (let j = y - half; j <= y + half; j++) {
          const key = `${i},${j}`;
          if (!finalPixels.current.has(key)) {
            finalPixels.current.add(key);
          }
        }
      }
    });
  };

  // Stop drawing on mouse up or when leaving the canvas
  const finishDrawing = () => {
    if (finalPixels.current.size !== 0) {
      const toSend = {
        colour: Number(colourHexInt[contextRef.current.fillStyle]),
        pixels: [...finalPixels.current].map((p) => {
          let coords = p.split(",");
          return { x: parseInt(coords[0]), y: parseInt(coords[1]) };
        }),
      };
      console.log(toSend);

      openConnection.post("/drawLine", toSend).then((response) => {
        console.log("Canvas updated:", response.data);
      });
    }
    // Store a copy of finalPixels before clearing
    previousPixels.current = new Set(finalPixels.current);

    // Set a timeout to clear the previousPixels after 500ms
    if (previousPixelsTimeout.current) {
      clearTimeout(previousPixelsTimeout.current);
    }
    previousPixelsTimeout.current = setTimeout(() => {
      previousPixels.current.clear();
      previousPixelsTimeout.current = null;
    }, 500);

    finalPixels.current = new Set();
    contextRef.current.closePath();
    setIsDrawing(false);
  };

  // Clear the canvas (optional)
  const clearCanvas = () => {
    const canvas = canvasRef.current;
    contextRef.current.clearRect(0, 0, canvas.width, canvas.height);
  };

  const changeColour = (e) => {
    contextRef.current.fillStyle = e.target.textContent;
  };

  const changeThickness = (e) => {
    setThickness(e.target.textContent);
  };

  function drawPixels(pixels) {
    const ctx = canvasRef.current.getContext("2d");
    const imageData = ctx.getImageData(0, 0, cWidth, cHeight); // Read current data
    const data = imageData.data;

    if (pixels.length !== cWidth * cHeight) {
      console.error("Pixel array length does not match canvas dimensions.");
      return;
    }

    // Update pixels received from server
    for (let i = 0; i < pixels.length; i++) {
      const pixel = pixels[i];
      const dataIndex = i * 4;
      data[dataIndex] = pixel.r;
      data[dataIndex + 1] = pixel.g;
      data[dataIndex + 2] = pixel.b;
      data[dataIndex + 3] = pixel.a;
    }

    // Update pixels drawn by the user
    const pixelsToDraw = new Set([
      ...finalPixels.current,
      ...previousPixels.current,
    ]);

    pixelsToDraw.forEach((pixelKey) => {
      const [x, y] = pixelKey.split(",").map(Number);
      if (x >= 0 && x < cWidth && y >= 0 && y < cHeight) {
        const colour = convertHextoRGBA(contextRef.current.fillStyle);
        const dataIndex = (y * cWidth + x) * 4;
        data[dataIndex] = colour.r; // Set to user's color. for example black.
        data[dataIndex + 1] = colour.g;
        data[dataIndex + 2] = colour.b;
        data[dataIndex + 3] = colour.a;
      }
    });

    ctx.putImageData(imageData, 0, 0);
  }

  return (
    <div>
      <canvas
        onMouseDown={startDrawing}
        onMouseMove={draw}
        onMouseUp={finishDrawing}
        onMouseOut={finishDrawing}
        ref={canvasRef}
        style={{ border: "1px solid #000" }}
      />
      <br />
      <p>Set Colour:</p>
      <button onClick={changeColour}>Black</button>
      <button onClick={changeColour}>Red</button>
      <button onClick={changeColour}>Orange</button>
      <button onClick={changeColour}>Yellow</button>
      <button onClick={changeColour}>Green</button>
      <button onClick={changeColour}>Blue</button>
      <button onClick={changeColour}>Purple</button>
      <button onClick={changeColour}>White</button>
      <button onClick={clearCanvas}>Clear</button>
      <br />
      <p>Set Thickness:</p>
      <button onClick={changeThickness}>1</button>
      <button onClick={changeThickness}>4</button>
      <button onClick={changeThickness}>10</button>
      <button onClick={changeThickness}>20</button>
      <button onClick={changeThickness}>60</button>
    </div>
  );
};

export default Whiteboard;
