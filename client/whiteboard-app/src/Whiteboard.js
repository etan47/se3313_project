// src/Whiteboard.js
import React, { useRef, useState, useEffect } from "react";
import { convertColourToRGBA } from "./utils";

const Whiteboard = () => {
  const cWidth = 300;
  const cHeight = 300;
  const canvasRef = useRef(null);
  const contextRef = useRef(null);
  const [isDrawing, setIsDrawing] = useState(false);

  // Setup canvas when component mounts
  useEffect(() => {
    const canvas = canvasRef.current;

    canvas.width = cWidth * 2;
    canvas.height = cHeight * 2;
    canvas.style.width = `${cWidth}px`;
    canvas.style.height = `${cHeight}px`;

    const context = canvas.getContext("2d");
    context.scale(3, 3);
    context.lineCap = "round";
    context.strokeStyle = "black";
    context.lineWidth = 1;
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
    console.log(offsetX, offsetY);
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

  function fetchUpdatedCanvas() {
    // This function would fetch the updated canvas data from the server

    // Simulate fetching
    const pixels = [];
    const colours = ["white", "black", "red", "green", "blue", "yellow"];
    for (let y = 0; y < cHeight * 2; y++) {
      for (let x = 0; x < cWidth * 2; x++) {
        let colour = colours[Math.floor(Math.random() * colours.length)];
        pixels.push(convertColourToRGBA(colour));
      }
    }
    drawPixels(pixels);
  }

  function drawPixels(pixels) {
    const ctx = canvasRef.current.getContext("2d");
    const imageData = ctx.createImageData(cWidth * 2, cHeight * 2);
    const data = imageData.data;

    if (pixels.length !== cWidth * 4 * cHeight) {
      console.error("Pixel array length does not match canvas dimensions.");
      return;
    }

    for (let i = 0; i < pixels.length; i++) {
      const pixel = pixels[i];

      // Calculate the index in the imageData.data array
      const dataIndex = i * 4;

      // Set RGBA values
      data[dataIndex] = pixel.r; // Red
      data[dataIndex + 1] = pixel.g; // Green
      data[dataIndex + 2] = pixel.b; // Blue
      data[dataIndex + 3] = pixel.a; // Alpha
    }
    ctx.putImageData(imageData, 0, 0);
  }

  setTimeout(() => {
    fetchUpdatedCanvas();
  }, 2000); // Fetch updated canvas every second

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
