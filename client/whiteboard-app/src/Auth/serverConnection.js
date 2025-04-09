import axios from "axios";

export const openConnection = axios.create({
  timeout: 30000,
});

// pending requests object to keep track of requests
let pendingRequests = {};

// Function to cancel pending requests
const cancelPending = (url) => {
  if (pendingRequests[url]) {
    pendingRequests[url](
      "Request cancelled by interceptor due to a new request with the same ID."
    );
    delete pendingRequests[url];
  }
};

// Request Interceptor
openConnection.interceptors.request.use(
  (config) => {
    const { url } = config;

    // Check if the request URL includes "/getBoard" and if so, cancel any pending requests with the same ID
    if (url.includes("/getBoard")) {
      cancelPending(url); // Cancel any pending request with the same ID
      const CancelToken = axios.CancelToken;
      const source = CancelToken.source();
      config.cancelToken = source.token;
      pendingRequests[url] = source.cancel; // Store the cancel function in the config
    }

    return config;
  },
  (error) => {
    return Promise.reject(error);
  }
);

// Response Interceptor (for cleanup)
openConnection.interceptors.response.use(
  (response) => {
    const url = response.config.url;

    // If the response is successful and the URL matches, remove it from pending requests
    if (url.includes("/getBoard") && pendingRequests[url]) {
      delete pendingRequests[url];
    }
    return response;
  },
  (error) => {
    // Handle errors and cleanup pending requests
    const url = error?.config?.url;
    if (url && pendingRequests[url]) {
      delete pendingRequests[url];
      if (axios.isCancel(error)) {
        console.log(
          `Request with ID ${url} cancelled by interceptor:`,
          error.message
        );
        // You might want to handle cancellation errors here or let them propagate
      }
    }
    return Promise.reject(error);
  }
);
