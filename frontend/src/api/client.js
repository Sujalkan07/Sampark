const API_BASE_URL = import.meta.env.VITE_API_BASE_URL || '';

class ApiError extends Error {
  constructor(message, status = 500, details = null) {
    super(message);
    this.name = 'ApiError';
    this.status = status;
    this.details = details;
  }
}

async function request(endpoint, options = {}) {
  const url = `${API_BASE_URL}${endpoint}`;
  const config = {
    ...options,
    credentials: 'include',
    headers: {
      'Content-Type': 'application/json',
      Accept: 'application/json',
      ...options.headers,
    },
  };

  try {
    const response = await fetch(url, config);
    let data = null;
    const contentType = response.headers.get('content-type');
    if (contentType && contentType.includes('application/json')) {
      data = await response.json();
    } else {
      const text = await response.text();
      data = text ? { message: text } : {};
    }

    if (!response.ok) {
      const errorMessage = data?.error || data?.message || `Request failed with status ${response.status}`;
      throw new ApiError(errorMessage, response.status, data);
    }

    return data;
  } catch (error) {
    if (error instanceof ApiError) {
      throw error;
    }
    // Network / server connection failure
    throw new ApiError(
      'Unable to connect to the ConnectX server. Please check your network connection or try again.',
      0,
      { originalError: error.message }
    );
  }
}

export const api = {
  async getHealth() {
    return request('/api/health', { method: 'GET' });
  },

  async register({ username, email, fullName, password }) {
    return request('/api/auth/register', {
      method: 'POST',
      body: JSON.stringify({ username, email, fullName, password }),
    });
  },

  async login({ identifier, password }) {
    return request('/api/auth/login', {
      method: 'POST',
      body: JSON.stringify({ identifier, password }),
    });
  },

  async logout() {
    return request('/api/auth/logout', {
      method: 'POST',
    });
  },

  async getMe() {
    return request('/api/auth/me', {
      method: 'GET',
    });
  },

  async searchUsers(query = '', limit = 50) {
    const params = new URLSearchParams();
    if (query) params.append('q', query);
    if (limit) params.append('limit', limit.toString());
    const queryString = params.toString();
    const endpoint = queryString ? `/api/users?${queryString}` : '/api/users';
    return request(endpoint, {
      method: 'GET',
    });
  },
};
