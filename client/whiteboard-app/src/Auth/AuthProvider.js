import { createContext, useContext, useState, useEffect } from "react";

export const AuthContext = createContext(null);

// AuthProvider component to manage authentication state
export const AuthProvider = ({ children }) => {
  const [email, setEmail] = useState(""); // Initialize email state
  const [isLoggedIn, setIsLoggedIn] = useState(false); // Initialize login state

  // Effect to check local storage for email on component mount
  useEffect(() => {
    const storedEmail = localStorage.getItem("email");
    if (storedEmail) {
      setEmail(storedEmail);
      setIsLoggedIn(true); // Set to true if email exists in local storage
    } else {
      setIsLoggedIn(false); // Set to false if no email is found
    }
  }, []);

  return (
    <AuthContext.Provider
      value={{ email, setEmail, isLoggedIn, setIsLoggedIn }}
    >
      {children}
    </AuthContext.Provider>
  );
};

// Custom hook to use the AuthContext
export function useAuth() {
  const context = useContext(AuthContext);
  if (!context) {
    throw new Error("useAuth must be used within an AuthProvider");
  }
  return context;
}
