// src/Whiteboard.js
import React, { useRef, useState, useEffect } from 'react';

const Whiteboard = () => {
  const canvasRef = useRef(null);
  const contextRef = useRef(null);
  const [isDrawing, setIsDrawing] = useState(false);

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
    context.strokeStyle = "black";
    context.lineWidth = 5;
    contextRef.current = context;
  }, []);

  // Start drawing on mouse down
  const startDrawing = ({ nativeEvent }) => {
    const { offsetX, offsetY } = nativeEvent;
    contextRef.current.beginPath();
    contextRef.current.moveTo(offsetX, offsetY);
    setIsDrawing(true);
  };

  // Draw as the mouse moves
  const draw = ({ nativeEvent }) => {
    if (!isDrawing) return;
    const { offsetX, offsetY } = nativeEvent;
    contextRef.current.lineTo(offsetX, offsetY);
    contextRef.current.stroke();
  };

  // Stop drawing on mouse up or when leaving the canvas
  const finishDrawing = () => {
    contextRef.current.closePath();
    setIsDrawing(false);
  };

  // Clear the canvas (optional)
  const clearCanvas = () => {
    const canvas = canvasRef.current;
    contextRef.current.clearRect(0, 0, canvas.width, canvas.height);
  };

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
      <button onClick={clearCanvas}>Clear</button>
    </div>
  );
};

export default Whiteboard;
