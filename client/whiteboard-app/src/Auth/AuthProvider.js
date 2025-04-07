import { createContext, useContext, useState, useEffect } from "react";

export const AuthContext = createContext(null);
export const AuthProvider = ({ children }) => {
  const [email, setEmail] = useState("");
  const [isLoggedIn, setIsLoggedIn] = useState(false); //! Set to true for testing purposes

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

export function useAuth() {
  const context = useContext(AuthContext);
  if (!context) {
    throw new Error("useAuth must be used within an AuthProvider");
  }
  return context;
}
