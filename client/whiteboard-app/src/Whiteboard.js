// src/Whiteboard.js
import React, { useRef, useState, useEffect } from 'react';

const Whiteboard = () => {
  const canvasRef = useRef(null);
  const contextRef = useRef(null);
  const [isDrawing, setIsDrawing] = useState(false);
  const [pixels, setPixels] = useState([]);
  const [lastPos, setLastPos] = useState(null);

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
    context.lineCap = "round";
    context.strokeStyle = "Black";
    context.lineWidth = 1;
    contextRef.current = context;
  }, []);

  // Start drawing on mouse down
  const startDrawing = ({ nativeEvent }) => {
    const { offsetX, offsetY } = nativeEvent;
    setLastPos({ x: offsetX, y: offsetY });
    contextRef.current.beginPath();
    contextRef.current.moveTo(offsetX, offsetY);
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
        pixels.push({ x: x0, y: y0 });
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
    if (!isDrawing || !lastPos) return;
    const { offsetX, offsetY } = nativeEvent;
    const newPixels = getLinePixels(lastPos.x, lastPos.y, offsetX, offsetY);
    setPixels((prevPixels) => [...prevPixels, ...newPixels]);
    contextRef.current.lineTo(offsetX, offsetY);
    contextRef.current.stroke();
    setLastPos({ x: offsetX, y: offsetY });
  };

  // Stop drawing on mouse up or when leaving the canvas
  const finishDrawing = () => {
    if (pixels.length !== 0){
      const toSend = {colour: contextRef.current.strokeStyle, pixels: pixels};
      console.log(toSend);
    }
    setPixels([]);
    contextRef.current.closePath();
    setIsDrawing(false);
  };

  // Clear the canvas (optional)
  const clearCanvas = () => {
    const canvas = canvasRef.current;
    contextRef.current.clearRect(0, 0, canvas.width, canvas.height);
  };

  const changeColour = (e) => {
    contextRef.current.strokeStyle = e.target.textContent;
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
      <button onClick={changeColour}>Black</button>
      <button onClick={changeColour}>Red</button>
      <button onClick={changeColour}>Green</button>
      <button onClick={changeColour}>Blue</button>
      <button onClick={changeColour}>Yellow</button>
      <button onClick={changeColour}>White</button>
      <button onClick={clearCanvas}>Clear</button>
    </div>
  );
};

export default Whiteboard;
