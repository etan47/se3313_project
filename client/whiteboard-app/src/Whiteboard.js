// src/Whiteboard.js
import React, { useRef, useState, useEffect } from 'react';

const Whiteboard = () => {
  const canvasRef = useRef(null);
  const contextRef = useRef(null);
  const [isDrawing, setIsDrawing] = useState(false);
  const finalPixels = useRef(new Set());
  const lastPos = useRef(null);
  const [thickness, setThickness] = useState(5);

  // Setup canvas when component mounts
  useEffect(() => {
    const canvas = canvasRef.current;
    // Set canvas dimensions and scale for high resolution
    canvas.width = window.innerWidth * 2;
    canvas.height = window.innerHeight * 2;
    canvas.style.width = `${window.innerWidth}px`;
    canvas.style.height = `${window.innerHeight}px`;

    const context = canvas.getContext("2d");
    context.scale(2, 2);
    context.fillStyle = "Black";
    contextRef.current = context;
  }, []);

  // Start drawing on mouse down
  const startDrawing = ({ nativeEvent }) => {
    const { offsetX, offsetY } = nativeEvent;
    lastPos.current = { x: offsetX, y: offsetY };
    setIsDrawing(true);
  };

  const getLinePixels = ((x0, y0, x1, y1) => {
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
  });

  // Draw as the mouse moves
  const draw = ({ nativeEvent }) => {
    if (!isDrawing || !lastPos.current) return;
    const { offsetX, offsetY } = nativeEvent;
    const newPixels = getLinePixels(lastPos.current.x, lastPos.current.y, offsetX, offsetY);
    lastPos.current = { x: offsetX, y: offsetY };

    const half = Math.floor(thickness / 2);
    newPixels.forEach((p) => {
      let x = p[0];
      let y = p[1];
      contextRef.current.fillRect(x - half, y - half, thickness, thickness);
      for (let i = x - half; i <= x + half; i++){
        for (let j = y - half; j <= y + half; j++){
          const key = `${i},${j}`;
          if (!finalPixels.current.has(key)){
            finalPixels.current.add(key);
          }
        }
      }
    })
  };

  // Stop drawing on mouse up or when leaving the canvas
  const finishDrawing = () => {
    if (finalPixels.current.size !== 0){
      const toSend = {colour: contextRef.current.strokeStyle, pixels: [...finalPixels.current].map((p) => {
        let coords = p.split(",");
        return {x: parseInt(coords[0]), y: parseInt(coords[1])}
      })};
      console.log(toSend);
    }
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
  }

  const changeThickness = (e) => {
    setThickness(e.target.textContent);
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
