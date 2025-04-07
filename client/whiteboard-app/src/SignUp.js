"use client";

import { useState } from "react";
import { openConnection } from "./Auth/serverConnection";
import "./SignUp.css";
import "./Auth/serverConnection.js";

const SignUp = ({ onNavigateToLogin }) => {
  const [email, setEmail] = useState("");
  const [username, setUsername] = useState("");
  const [password, setPassword] = useState("");
  const [message, setMessage] = useState("");
  const [isSuccess, setIsSuccess] = useState(false);

  const handleCreateAccount = async (e) => {
    e.preventDefault();

    try {
      setMessage("");

      let result = await openConnection.post("/register", {
        email: email,
        username: username,
        password: password,
      });

      // Show success message
      setMessage("Your account was created successfully!");
      setIsSuccess(true);

      // Clear form
      setEmail("");
      setUsername("");
      setPassword("");
    } catch (error) {
      console.error("Error creating account:", error);

      if (error.response && error.response.status === 409) {
        setMessage("Email already exists!");
        setIsSuccess(false);
        return;
      }

      setMessage("Account creation failed. Please try again.");
      setIsSuccess(false);
      return;
    }
  };

  return (
    <div className="signup-container">
      <h2>Create a New Account</h2>
      <form onSubmit={handleCreateAccount} className="signup-form">
        <div className="form-group">
          <label htmlFor="new-email">Email:</label>
          <input
            type="email"
            id="new-email"
            value={email}
            onChange={(e) => setEmail(e.target.value)}
            required
          />
        </div>
        <div className="form-group">
          <label htmlFor="new-username">Username:</label>
          <input
            type="text"
            id="new-username"
            value={username}
            onChange={(e) => setUsername(e.target.value)}
            required
          />
        </div>
        <div className="form-group">
          <label htmlFor="new-password">Password:</label>
          <input
            type="password"
            id="new-password"
            value={password}
            onChange={(e) => setPassword(e.target.value)}
            required
          />
        </div>
        {message && (
          <div className={isSuccess ? "success-message" : "error-message"}>
            {message}
          </div>
        )}
        <div className="button-group">
          <button
            type="button"
            className="next-button"
            onClick={onNavigateToLogin}
          >
            Back
          </button>
          <button type="submit" className="signup-button">
            Create
          </button>
        </div>
      </form>
    </div>
  );
};

export default SignUp;
