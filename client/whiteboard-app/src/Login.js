"use client"

import { useState, useEffect } from "react"
import "./Login.css"

const Login = ({ onLoginSuccess, onNavigateToSignUp }) => {
  const [username, setUsername] = useState("")
  const [password, setPassword] = useState("")
  const [error, setError] = useState("")
  const [accounts, setAccounts] = useState([
    { username: "abcdef", password: "123456" },
    { username: "abcdefg", password: "1234567" },
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

    // Find if username exists
    const userMatch = accounts.find((cred) => cred.username === username)

    if (!userMatch) {
      setError("Incorrect username!")
      return
    }

    // Check if password matches
    if (userMatch.password !== password) {
      setError("Incorrect password!")
      return
    }

    // Login successful
    setError("")
    onLoginSuccess(username) // Pass the username to App.js
  }

  return (
    <div className="login-container">
      <h2>Login to Whiteboard</h2>
      <form onSubmit={handleSubmit} className="login-form">
        <div className="form-group">
          <label htmlFor="username">Username:</label>
          <input type="text" id="username" value={username} onChange={(e) => setUsername(e.target.value)} required />
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

