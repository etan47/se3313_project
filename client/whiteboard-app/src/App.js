"use client";

import { useState } from "react";
import "./App.css";
import Whiteboard from "./Whiteboard";
import Login from "./Login";
import SignUp from "./SignUp";
import NavBar from "./NavBar";

function App() {
  const [isLoggedIn, setIsLoggedIn] = useState(false);
  const [email, setEmail] = useState("");
  const [showSignUp, setShowSignUp] = useState(false);

  const handleLoginSuccess = (loggedInEmail) => {
    setEmail(loggedInEmail);
    setIsLoggedIn(true);
  };

  const handleLogout = () => {
    setIsLoggedIn(false);
    setEmail("");
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
          <NavBar email={email} onLogout={handleLogout} />
          <div className="whiteboard-content">
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
