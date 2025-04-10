// src/NavBar.js
import React from "react";
import { AppBar, Toolbar, Typography, Button, Box } from "@mui/material";

const NavBar = ({ email, onLogout }) => {
  return (
    <AppBar position="static" color="primary" elevation={2}>
      <Toolbar>
        <Typography variant="h6" component="div" sx={{ flexGrow: 1 }}>
          Hello, {email}
        </Typography>
        <Button 
          color="inherit" 
          variant="outlined" 
          onClick={onLogout}
          sx={{ 
            bgcolor: 'rgba(255, 255, 255, 0.1)', 
            '&:hover': { bgcolor: 'rgba(255, 255, 255, 0.2)' } 
          }}
        >
          Logout
        </Button>
      </Toolbar>
    </AppBar>
  );
};

export default NavBar;