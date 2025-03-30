import axios from "axios";

export const openConnection = axios.create({
  //   baseURL: "http://127.0.0.1:8080",
  timeout: 5000,
});
