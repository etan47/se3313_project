"use client"
import "./NavBar.css"

const NavBar = ({ username, onLogout }) => {
  return (
    <div className="navbar">
      <div className="navbar-greeting">Hello, {username}</div>
      <div className="navbar-actions">
        <button className="logout-button" onClick={onLogout}>
          Logout
        </button>
      </div>
    </div>
  )
}

export default NavBar

