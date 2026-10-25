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
      let errorMessage = data?.error || data?.message;
      if (!errorMessage) {
        if (response.status === 500 || response.status === 502 || response.status === 503 || response.status === 504) {
          errorMessage = 'Sampark backend server is unreachable. Please ensure the C++ backend server is running on port 8080.';
        } else {
          errorMessage = `Request failed with status ${response.status}`;
        }
      }
      throw new ApiError(errorMessage, response.status, data);
    }

    return data;
  } catch (error) {
    if (error instanceof ApiError) {
      throw error;
    }
    // Network / server connection failure
    throw new ApiError(
      'Unable to connect to the Sampark server. Please check your network connection or try again.',
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

  // Phase 2 Messaging APIs
  async createOrGetConversation(participantId) {
    return request('/api/conversations', {
      method: 'POST',
      body: JSON.stringify({ participantId }),
    });
  },

  async getConversations() {
    return request('/api/conversations', {
      method: 'GET',
    });
  },

  async getConversation(conversationId) {
    return request(`/api/conversations/${conversationId}`, {
      method: 'GET',
    });
  },

  async getMessages(conversationId, { limit = 50, before = 0, after = 0 } = {}) {
    const params = new URLSearchParams();
    if (limit) params.append('limit', limit.toString());
    if (before) params.append('before', before.toString());
    if (after) params.append('after', after.toString());
    const queryString = params.toString();
    const endpoint = queryString
      ? `/api/conversations/${conversationId}/messages?${queryString}`
      : `/api/conversations/${conversationId}/messages`;
    return request(endpoint, {
      method: 'GET',
    });
  },

  async sendMessage(conversationId, body) {
    return request(`/api/conversations/${conversationId}/messages`, {
      method: 'POST',
      body: JSON.stringify({ body }),
    });
  },

  async markConversationAsRead(conversationId) {
    return request(`/api/conversations/${conversationId}/read`, {
      method: 'POST',
    });
  },
};
