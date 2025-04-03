"use client"
import "./NavBar.css"

const NavBar = ({ email, username, onLogout }) => {
  // Use username if available, otherwise fall back to email
  const displayName = username || email

  return (
    <div className="navbar">
      <div className="navbar-greeting">Hello, {displayName}</div>
      <div className="navbar-actions">
        <button className="logout-button" onClick={onLogout}>
          Logout
        </button>
      </div>
    </div>
  )
}

export default NavBar

