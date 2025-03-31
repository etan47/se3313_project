"use client";

import { useState } from "react";
import "./App.css";
import Whiteboard from "./Whiteboard";
import Login from "./Login";
import SignUp from "./SignUp";
import NavBar from "./NavBar";

function App() {
  const [isLoggedIn, setIsLoggedIn] = useState(false);
  const [username, setUsername] = useState("");
  const [showSignUp, setShowSignUp] = useState(false);

  const handleLoginSuccess = (loggedInUsername) => {
    setUsername(loggedInUsername);
    setIsLoggedIn(true);
  };

  const handleLogout = () => {
    setIsLoggedIn(false);
    setUsername("");
  };

  const navigateToSignUp = () => {
    setShowSignUp(true);
  };

  const navigateToLogin = () => {
    setShowSignUp(false);
  };

  return (
    <div className="App">
      {isLoggedIn ? (
        <div className="whiteboard-container">
          <NavBar username={username} onLogout={handleLogout} />
          <div className="whiteboard-content">
            <h1>Whiteboard Canvas</h1>
            <Whiteboard />
          </div>
        </div>
      ) : showSignUp ? (
        <SignUp onNavigateToLogin={navigateToLogin} />
      ) : (
        <Login
          onLoginSuccess={handleLoginSuccess}
          onNavigateToSignUp={navigateToSignUp}
        />
      )}
    </div>
  );
}

export default App;
