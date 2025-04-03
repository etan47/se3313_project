"use client"

import { useState, useEffect } from "react"
import "./Login.css"

const Login = ({ onLoginSuccess, onNavigateToSignUp }) => {
  const [email, setEmail] = useState("")
  const [password, setPassword] = useState("")
  const [error, setError] = useState("")
  const [accounts, setAccounts] = useState([
    { email: "admin@test.com", username: "Admin", password: "admin" },
    { email: "bob@test.com", username: "Bob", password: "bob" },
  ])

  // Load accounts from localStorage on component mount
  useEffect(() => {
    const storedAccounts = localStorage.getItem("accounts")
    if (storedAccounts) {
      setAccounts(JSON.parse(storedAccounts))
    }
  }, [])

  const handleSubmit = (e) => {
    e.preventDefault()

    // Find if email exists
    const userMatch = accounts.find((cred) => cred.email === email)

    if (!userMatch) {
      setError("Incorrect email!")
      return
    }

    // Check if password matches
    if (userMatch.password !== password) {
      setError("Incorrect password!")
      return
    }

    // Login successful
    setError("")
    onLoginSuccess(email, userMatch.username) // Pass both email and username to App.js
  }

  return (
    <div className="login-container">
      <h2>Login to Whiteboard</h2>
      <form onSubmit={handleSubmit} className="login-form">
        <div className="form-group">
          <label htmlFor="email">Email:</label>
          <input type="text" id="email" value={email} onChange={(e) => setEmail(e.target.value)} required />
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
  )
}

export default Login

