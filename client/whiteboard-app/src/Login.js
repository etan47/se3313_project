"use client";

import { useState } from "react";
import { openConnection } from "./Auth/serverConnection";
import "./Login.css";

const Login = ({ onLoginSuccess, onNavigateToSignUp }) => {
  const [email, setEmail] = useState("");
  const [password, setPassword] = useState("");
  const [error, setError] = useState("");

  const handleSubmit = async (e) => {
    e.preventDefault();

    // Find if email exists
    try {
      let result = await openConnection.post("/login", {
        email: email,
        password: password,
      });

      setError("");

      onLoginSuccess(result.data.email, result.data.username); // Pass email to App.js
    } catch (error) {
      console.error("Error logging in:", error);

      if (error.response && error.response.status === 401) {
        setError("Invalid credentials. Please try again.");
        return;
      }

      setError("Login failed. Please try again.");
      return;
    }
  };

  return (
    <div className="login-container">
      <h2>Login to Whiteboard</h2>
      <form onSubmit={handleSubmit} className="login-form">
        <div className="form-group">
          <label htmlFor="email">Email:</label>
          <input
            type="email"
            id="email"
            value={email}
            onChange={(e) => setEmail(e.target.value)}
            required
          />
        </div>
        <div className="form-group">
          <label htmlFor="password">Password:</label>
          <input
            type="password"
            id="password"
            value={password}
            onChange={(e) => setPassword(e.target.value)}
            required
          />
        </div>
        {error && <div className="error-message">{error}</div>}
        <button type="submit" className="login-button">
          Login
        </button>
        <div className="signup-link" onClick={onNavigateToSignUp}>
          Create an account if you are new!
        </div>
      </form>
    </div>
  );
};

export default Login;
