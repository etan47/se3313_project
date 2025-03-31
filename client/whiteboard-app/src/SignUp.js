"use client"

import { useState, useEffect } from "react"
import "./SignUp.css"
import "./Auth/serverConnection.js"

const SignUp = ({ onNavigateToLogin }) => {
  const [username, setUsername] = useState("")
  const [password, setPassword] = useState("")
  const [message, setMessage] = useState("")
  const [isSuccess, setIsSuccess] = useState(false)
  const [accounts, setAccounts] = useState([])

  // Load existing accounts from localStorage on component mount
  useEffect(() => {
    const storedAccounts = localStorage.getItem("accounts")
    if (storedAccounts) {
      setAccounts(JSON.parse(storedAccounts))
    } else {
      // Initialize with default accounts if none exist
      const initialAccounts = [
        { username: "abcdef", password: "123456" },
        { username: "abcdefg", password: "1234567" },
      ]
      localStorage.setItem("accounts", JSON.stringify(initialAccounts))
      setAccounts(initialAccounts)
    }
  }, [])

  const handleCreateAccount = (e) => {
    e.preventDefault()

    // Check if username already exists
    const userExists = accounts.some((account) => account.username === username)

    if (userExists) {
      setMessage("Username already exists!")
      setIsSuccess(false)
      return
    }

    // Create new account
    const newAccount = { username, password }
    
    const updatedAccounts = [...accounts, newAccount]

    // Save to localStorage (simulating file storage)
    localStorage.setItem("accounts", JSON.stringify(updatedAccounts))
    setAccounts(updatedAccounts)

    // Show success message
    setMessage("Your account was created successfully!")
    setIsSuccess(true)

    // Clear form
    setUsername("")
    setPassword("")
  }

  return (
    <div className="signup-container">
      <h2>Create a New Account</h2>
      <form onSubmit={handleCreateAccount} className="signup-form">
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
        {message && <div className={isSuccess ? "success-message" : "error-message"}>{message}</div>}
        <div className="button-group">
          <button type="submit" className="signup-button">
            Create
          </button>
          <button type="button" className="next-button" onClick={onNavigateToLogin}>
            Back
          </button>
        </div>
      </form>
    </div>
  )
}

export default SignUp

