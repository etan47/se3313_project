"use client";

import { useState } from "react";
import "./App.css";
import { useAuth } from "./Auth/AuthProvider";
import Whiteboard from "./Whiteboard";
import Login from "./Login";
import SignUp from "./SignUp";
import NavBar from "./NavBar";

function App() {
  const { email, setEmail, isLoggedIn, setIsLoggedIn } = useAuth();
  // const [isLoggedIn, setIsLoggedIn] = useState(false); //! Set to true for testing purposes
  // const [email, setEmail] = useState("");
  const [showSignUp, setShowSignUp] = useState(false);

  const handleLoginSuccess = (loggedInEmail) => {
    setEmail(loggedInEmail);
    localStorage.setItem("email", loggedInEmail); // Store email in local storage
    setIsLoggedIn(true);
  };

  const handleLogout = () => {
    localStorage.removeItem("email"); // Remove email from local storage
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
