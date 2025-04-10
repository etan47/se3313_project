// src/Whiteboard.js
import { useRef, useState, useEffect, useCallback } from "react";
import { useAuth } from "./Auth/AuthProvider";
import { colourHexInt, convertIntToRGBA, convertHextoRGBA } from "./utils";
import { openConnection } from "./Auth/serverConnection";

// Material UI imports
import { 
  Box, 
  Button, 
  Typography, 
  TextField, 
  Slider, 
  List, 
  ListItem, 
  CircularProgress, 
  AppBar, 
  Toolbar, 
  Container, 
  Paper, 
  Grid,
  IconButton
} from '@mui/material';
import { styled } from '@mui/material/styles';

// Optional: Import MUI icons if you install @mui/icons-material
import ArrowBackIcon from '@mui/icons-material/ArrowBack';
import PaletteIcon from '@mui/icons-material/Palette';
import DeleteIcon from '@mui/icons-material/Delete';

// Styled components
const ColorButton = styled(Button)(({ theme, bgcolor }) => ({
  backgroundColor: bgcolor,
  minWidth: 40,
  height: 40,
  margin: theme.spacing(0.5),
  border: `2px solid ${theme.palette.grey[300]}`,
  '&:hover': {
    backgroundColor: bgcolor,
    border: `2px solid ${theme.palette.grey[800]}`,
  },
}));

const SessionButton = styled(Button)(({ theme }) => ({
  margin: theme.spacing(1),
}));

const CanvasContainer = styled(Paper)(({ theme }) => ({
  position: 'relative',
  margin: theme.spacing(2, 0),
  border: `1px solid ${theme.palette.divider}`,
  borderRadius: theme.shape.borderRadius,
  overflow: 'hidden',
}));

const LoadingOverlay = styled(Box)(({ theme }) => ({
  position: 'absolute',
  top: 0,
  left: 0,
  right: 0,
  bottom: 0,
  display: 'flex',
  alignItems: 'center',
  justifyContent: 'center',
  backgroundColor: 'rgba(255, 255, 255, 0.7)',
  zIndex: 1,
}));

const WhiteboardCanvas = styled('canvas')({
  display: 'block',
});

const Whiteboard = () => {
  const { email } = useAuth(); // Get email from AuthProvider
  const cWidth = 900;
  const cHeight = 600;
  const canvasRef = useRef(null);
  const contextRef = useRef(null);
  const [isDrawing, setIsDrawing] = useState(false);
  const finalPixels = useRef(new Set());
  const lastPos = useRef(null);
  const [thickness, setThickness] = useState(5);
  const previousPixels = useRef(new Set());
  const previousPixelsTimeout = useRef(null);
  const [sessionId, setSessionId] = useState(null); // State to store session ID
  const [loading, setLoading] = useState(false); // State to manage loading state
  const [whiteboards, setWhiteboards] = useState([]); // State to manage whiteboards
  const [canvasLoading, setCanvasLoading] = useState(true); // State to manage canvas loaded state
  const [fetchErrors, setFetchErrors] = useState(0); // State to manage fetch errors
  const [sessionIdInput, setSessionIdInput] = useState(''); // State to manage session ID input

  // Fetch whiteboards when the component mounts or when sessionId changes
  useEffect(() => {
    setCanvasLoading(true); // Set canvas loaded state to true when component mounts
    openConnection
      .get("/getWhiteboards?email=" + email) // Fetch whiteboards for the logged-in user
      .then((response) => {
        setWhiteboards(response.data.whiteboards.reverse()); // Set the whiteboards state with the response data
      })
      .catch((error) => {
        console.error("Error fetching whiteboards:", error);
      });
  }, [sessionId, email]); // Effect to run when sessionId changes

  // Function to get the updated canvas data from the server and draw it on the canvas
  const fetchUpdatedCanvas = useCallback(() => {
    if (!sessionId) return; // Ensure sessionId is available before making the request

    // get the updated canvas data from the server
    openConnection
      .get("/getBoard?session_id=" + sessionId) // Use the session ID in the URL
      .then((response) => {
        console.log("Canvas data fetched");

        // convert the response data to RGBA format
        let pixels = [];
        for (let i = 0; i < response.data.length; i++) {
          for (let j = 0; j < response.data[i].length; j++) {
            pixels.push(convertIntToRGBA(response.data[i][j]));
          }
        }

        // draw the pixels on the canvas
        drawPixels(pixels);
        setCanvasLoading(false); // Set canvas loaded state to true after fetching data
        setFetchErrors(0); // Reset fetch errors count
      })
      .catch((error) => {
        console.error("Error fetching canvas data:", error);
        setFetchErrors((prev) => prev + 1); // Increment fetch errors count
      });
  }, [sessionId]); // Effect to run when sessionId or fetchErrors changes

  // Effect to handle session expiration
  useEffect(() => {
    if (fetchErrors >= 20) {
      setFetchErrors(0); // Reset fetch errors count
      setSessionId(null); // Reset session ID if fetch errors exceed limit
      alert("Session expired. Please reopen from saved whiteboards.");
    }
  }, [fetchErrors]); // Effect to run when fetchUpdatedCanvas changes

  // Setup canvas when component mounts
  useEffect(() => {
    if (!sessionId) return; // Ensure sessionId is available before setting up the canvas
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

    // set up an interval to fetch updated canvas data every 3 seconds
    const intervalId = setInterval(() => {
      fetchUpdatedCanvas();
    }, 4000);

    return () => clearInterval(intervalId); // Cleanup interval on unmount
  }, [fetchUpdatedCanvas, sessionId]);

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
          if (i < 0 || i > cWidth - 1 || j < 0 || j > cHeight - 1) {
            continue;
          }
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

      openConnection
        .post("/drawLine?session_id=" + sessionId, toSend)
        .then((response) => {
          console.log("Canvas updated:", response.data);
        })
        .catch((error) => {
          console.error("Error updating canvas:", error);
        });
    }

    // store a copy of combination of previousPixels and finalPixels
    previousPixels.current = new Set([
      ...finalPixels.current,
      ...previousPixels.current,
    ]);

    // clear the previousPixelTimeout
    if (previousPixelsTimeout.current) {
      clearTimeout(previousPixelsTimeout.current);
    }

    // set a timeout to clear the previousPixels after 500ms
    previousPixelsTimeout.current = setTimeout(() => {
      previousPixels.current.clear();
      previousPixelsTimeout.current = null;
    }, 500);

    finalPixels.current = new Set(); // Clear the finalPixels set
    contextRef.current.closePath(); // Close the path in the context
    setIsDrawing(false); // Set isDrawing to false
  };

  // Clear the canvas
  const clearCanvas = () => {
    const canvas = canvasRef.current;
    contextRef.current.clearRect(0, 0, canvas.width, canvas.height);
    openConnection.put("/clear?session_id=" + sessionId).then((res) => {
      console.log("Canvas cleared:", res.data);
    });
  };

  const changeColour = (color) => {
    contextRef.current.fillStyle = color;
  };

  // Function to draw pixels on the canvas
  function drawPixels(pixels) {
    const ctx = canvasRef.current.getContext("2d"); // Get the canvas context
    const imageData = ctx.getImageData(0, 0, cWidth, cHeight); // Read current data
    const data = imageData.data; // Get the pixel data array

    // Check if the pixel array length matches the canvas dimensions
    if (pixels.length !== cWidth * cHeight) {
      console.error("Pixel array length does not match canvas dimensions.");
      return;
    }

    // Update pixels with the fetched canvas data
    for (let i = 0; i < pixels.length; i++) {
      const pixel = pixels[i];
      const dataIndex = i * 4;
      data[dataIndex] = pixel.r;
      data[dataIndex + 1] = pixel.g;
      data[dataIndex + 2] = pixel.b;
      data[dataIndex + 3] = pixel.a;
    }

    // Create a set of local pixels drawn by the user
    const pixelsToDraw = new Set([
      ...finalPixels.current,
      ...previousPixels.current,
    ]);

    // Draw local pixels on the canvas over the fetched pixels for smoothness
    pixelsToDraw.forEach((pixelKey) => {
      const [x, y] = pixelKey.split(",").map(Number);
      if (x >= 0 && x < cWidth && y >= 0 && y < cHeight) {
        const colour = convertHextoRGBA(contextRef.current.fillStyle);
        const dataIndex = (y * cWidth + x) * 4;
        data[dataIndex] = colour.r; // Set to user's color
        data[dataIndex + 1] = colour.g;
        data[dataIndex + 2] = colour.b;
        data[dataIndex + 3] = colour.a;
      }
    });

    ctx.putImageData(imageData, 0, 0); // Update the canvas with the new pixel data
  }

  // Function to create a new session
  function createSession() {
    setLoading(true); // Set loading state to true

    let postBody = {
      email: email,
    };

    openConnection
      .post("/startWhiteboard", postBody) // Send request to create a new session
      .then((response) => {
        setSessionId(response.data.session_id); // Set the session ID from the response
        setLoading(false); // Reset loading state
      })
      .catch((error) => {
        console.error("Error creating session:", error);
        setSessionId(null); // Reset session ID on error
        setLoading(false); // Reset loading state
      });
  }

  // Function to join an existing session
  function joinSession() {
    setLoading(true); // Set loading state to true

    // send request to join the session
    openConnection
      .post("/joinWhiteboard", {
        session_id: sessionIdInput,
        email: email,
      })
      .then((response) => {
        console.log(response);

        // Check if the response status is 200 (OK)
        if (response.status === 200) {
          console.log("Session joined successfully:", response.data);
          setSessionId(sessionIdInput); // Set the session ID state
          setLoading(false); // Reset loading state
        } else {
          console.error("Error joining session:", response.data.message);
          setSessionId(null); // Reset session ID on error
          setLoading(false); // Reset loading state
        }
      })
      .catch((error) => {
        console.error("Error joining session:", error);
        setSessionId(null); // Reset session ID on error
        setLoading(false); // Reset loading state
      });
  }

  // Function to load a saved whiteboard
  function loadWhiteboard(whiteboard) {
    setLoading(true); // Set loading state to true

    // send request to load the whiteboard
    openConnection
      .post("/loadWhiteboard?db_id=" + whiteboard)
      .then((response) => {
        console.log(response);

        // Check if the response status is 200 (OK)
        if (response.status === 200) {
          console.log("Whiteboard loaded successfully:", response.data);
          const sessionId = response.data.session_id; // Get the session ID from the response
          setSessionId(sessionId); // Set the session ID state
          setLoading(false); // Reset loading state
        } else {
          console.error("Error loading whiteboard:", response.data.message);
          setSessionId(null); // Reset session ID on error
          setLoading(false); // Reset loading state
        }
      })
      .catch((error) => {
        console.error("Error loading whiteboard:", error);
        setSessionId(null); // Reset session ID on error
        setLoading(false); // Reset loading state
      });
  }

  const colorButtons = [
    { color: "Black", hex: "#000000" },
    { color: "Red", hex: "#FF0000" },
    { color: "Orange", hex: "#FFA500" },
    { color: "Yellow", hex: "#FFFF00" },
    { color: "Green", hex: "#008000" },
    { color: "Blue", hex: "#0000FF" },
    { color: "Purple", hex: "#800080" },
    { color: "White", hex: "#FFFFFF" },
  ];

  // If loading, show a loading message with MUI
  if (loading) {
    return (
      <Container maxWidth="md" sx={{ textAlign: 'center', py: 4 }}>
        <CircularProgress size={60} sx={{ mb: 2 }} />
        <Typography variant="h5">Loading...</Typography>
        <Typography variant="body1" color="text.secondary">
          Please wait while we set up your whiteboard.
        </Typography>
      </Container>
    );
  }

  // If no session ID, show the create/join session options with MUI
  if (!sessionId) {
    return (
      <Container maxWidth="md" sx={{ py: 4 }}>
        <Paper sx={{ p: 3, mb: 3 }}>
          <Typography variant="h4" gutterBottom align="center">
            Whiteboard Application
          </Typography>
          <Typography variant="h6" gutterBottom align="center">
            Please create or join a session to start drawing.
          </Typography>
          
          <Box sx={{ textAlign: 'center', mb: 3 }}>
            <Button 
              variant="contained" 
              color="primary" 
              size="large"
              onClick={createSession}
              sx={{ minWidth: 200 }}
            >
              Create Session
            </Button>
          </Box>
          
          <Box sx={{ mb: 3, textAlign: 'center' }}>
            <Typography variant="subtitle1" gutterBottom>
              Join Session:
            </Typography>
            <Box sx={{ display: 'flex', justifyContent: 'center', alignItems: 'center', gap: 2 }}>
              <TextField
                placeholder="Enter session ID"
                value={sessionIdInput}
                onChange={(e) => setSessionIdInput(e.target.value)}
                variant="outlined"
                sx={{ width: '60%' }}
              />
              <Button 
                variant="contained" 
                onClick={joinSession}
                disabled={!sessionIdInput}
              >
                Join
              </Button>
            </Box>
          </Box>
          
          {whiteboards.length > 0 && (
            <Box>
              <Typography variant="subtitle1" gutterBottom>
                Saved Whiteboards:
              </Typography>
              <Paper variant="outlined" sx={{ maxHeight: 300, overflow: 'auto', p: 1 }}>
                <List dense>
                  {whiteboards.map((whiteboard) => (
                    <ListItem key={whiteboard} disablePadding>
                      <Button
                        fullWidth
                        variant="outlined"
                        onClick={() => loadWhiteboard(whiteboard)}
                        sx={{ justifyContent: 'flex-start', textTransform: 'none', py: 1 }}
                      >
                        {whiteboard}
                      </Button>
                    </ListItem>
                  ))}
                </List>
              </Paper>
            </Box>
          )}
        </Paper>
      </Container>
    );
  }

  // Render the whiteboard canvas and controls with MUI
  return (
    <Container maxWidth="lg">
      <AppBar position="static" color="default" elevation={0} sx={{ mb: 2 }}>
        <Toolbar>
          <Button 
            color="inherit"
            onClick={() => setSessionId(null)}
            sx={{ mr: 2 }}
          >
            Back
          </Button>
          <Typography variant="h6" component="div" sx={{ flexGrow: 1 }}>
            Whiteboard Canvas
          </Typography>
          <Typography variant="body2">
            Session ID: {sessionId}
          </Typography>
        </Toolbar>
      </AppBar>

      <CanvasContainer>
        <WhiteboardCanvas
          onMouseDown={startDrawing}
          onMouseMove={draw}
          onMouseUp={finishDrawing}
          onMouseOut={finishDrawing}
          ref={canvasRef}
        />
        {canvasLoading && (
          <LoadingOverlay>
            <CircularProgress />
          </LoadingOverlay>
        )}
      </CanvasContainer>
      
      <Grid container spacing={3}>
        <Grid item xs={12} md={6}>
          <Paper sx={{ p: 2 }}>
            <Typography variant="subtitle1" gutterBottom>
              Set Colour:
            </Typography>
            <Box sx={{ display: 'flex', flexWrap: 'wrap' }}>
              {colorButtons.map((btn) => (
                <ColorButton
                  key={btn.color}
                  onClick={() => changeColour(btn.hex)}
                  bgcolor={btn.hex}
                  aria-label={`Color ${btn.color}`}
                />
              ))}
            </Box>
          </Paper>
        </Grid>
        
        <Grid item xs={12} md={6}>
          <Paper sx={{ p: 2 }}>
            <Typography variant="subtitle1" gutterBottom>
              Set Thickness: {thickness}px
            </Typography>
            <Slider
              min={1}
              max={81}
              step={5}
              value={thickness}
              onChange={(e, newValue) => setThickness(newValue)}
              valueLabelDisplay="auto"
              aria-labelledby="thickness-slider"
            />
            
            <Box sx={{ mt: 2, textAlign: 'center' }}>
              <Button 
                variant="contained" 
                color="primary" 
                onClick={clearCanvas}
                sx={{ mt: 1 }}
              >
                Clear Canvas
              </Button>
            </Box>
          </Paper>
        </Grid>
      </Grid>
    </Container>
  );
};

export default Whiteboard;