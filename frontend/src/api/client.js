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

  async sendMessage(conversationId, messageData) {
    const payload = typeof messageData === 'string'
      ? { body: messageData }
      : {
          body: messageData?.body || '',
          ...(messageData?.replyToId ? { replyToId: messageData.replyToId } : {}),
          ...(messageData?.attachmentId ? { attachmentId: messageData.attachmentId } : {}),
        };

    return request(`/api/conversations/${conversationId}/messages`, {
      method: 'POST',
      body: JSON.stringify(payload),
    });
  },

  async markConversationAsRead(conversationId) {
    return request(`/api/conversations/${conversationId}/read`, {
      method: 'POST',
    });
  },

  // Phase 3 Profile, Message Editing, and Search APIs
  async updateProfile({ fullName, bio }) {
    return request('/api/users/profile', {
      method: 'PUT',
      body: JSON.stringify({ fullName, bio }),
    });
  },

  async changePassword({ currentPassword, newPassword }) {
    return request('/api/auth/password', {
      method: 'PUT',
      body: JSON.stringify({ currentPassword, newPassword }),
    });
  },

  async getUserProfile(userId) {
    return request(`/api/users/${userId}`, {
      method: 'GET',
    });
  },

  async editMessage(messageId, body) {
    return request(`/api/messages/${messageId}`, {
      method: 'PUT',
      body: JSON.stringify({ body }),
    });
  },

  async deleteMessage(messageId) {
    return request(`/api/messages/${messageId}`, {
      method: 'DELETE',
    });
  },

  async searchConversationMessages(conversationId, query) {
    const params = new URLSearchParams();
    if (query) params.append('q', query);
    return request(`/api/conversations/${conversationId}/messages/search?${params.toString()}`, {
      method: 'GET',
    });
  },

  async searchGlobalMessages(query) {
    const params = new URLSearchParams();
    if (query) params.append('q', query);
    return request(`/api/messages/search?${params.toString()}`, {
      method: 'GET',
    });
  },

  // Phase 4: Group Chat APIs
  async createGroup({ name, description = '', avatarUrl = '', memberIds = [] }) {
    return request('/api/groups', {
      method: 'POST',
      body: JSON.stringify({ name, description, avatarUrl, memberIds }),
    });
  },

  async getGroupMembers(conversationId) {
    return request(`/api/conversations/${conversationId}/members`, {
      method: 'GET',
    });
  },

  async addGroupMember(conversationId, userId) {
    return request(`/api/conversations/${conversationId}/members`, {
      method: 'POST',
      body: JSON.stringify({ userId }),
    });
  },

  async removeGroupMember(conversationId, userId) {
    return request(`/api/conversations/${conversationId}/members/${userId}`, {
      method: 'DELETE',
    });
  },

  async updateGroup(conversationId, { name, description = '', avatarUrl = '' }) {
    return request(`/api/conversations/${conversationId}`, {
      method: 'PUT',
      body: JSON.stringify({ name, description, avatarUrl }),
    });
  },

  async deleteGroup(conversationId) {
    return request(`/api/conversations/${conversationId}`, {
      method: 'DELETE',
    });
  },

  async leaveGroup(conversationId) {
    return request(`/api/conversations/${conversationId}/leave`, {
      method: 'POST',
    });
  },

  // Phase 5: Conversation Removal & Contacts APIs
  async removeConversation(conversationId) {
    return request(`/api/conversations/${conversationId}`, {
      method: 'DELETE',
    });
  },

  async getContacts() {
    return request('/api/contacts', {
      method: 'GET',
    });
  },

  async addContact(contactUserId) {
    return request('/api/contacts', {
      method: 'POST',
      body: JSON.stringify({ contactUserId }),
    });
  },

  async removeContact(contactUserId) {
    return request(`/api/contacts/${contactUserId}`, {
      method: 'DELETE',
    });
  },

  // Phase 4: Reaction APIs
  async toggleReaction(messageId, emoji) {
    return request(`/api/messages/${messageId}/reactions`, {
      method: 'POST',
      body: JSON.stringify({ emoji }),
    });
  },

  // Phase 4: Attachment APIs
  async uploadAttachment(conversationId, file) {
    const formData = new FormData();
    formData.append('file', file);
    const url = `${API_BASE_URL}/api/conversations/${conversationId}/attachments`;
    const response = await fetch(url, {
      method: 'POST',
      credentials: 'include',
      body: formData,
    });
    if (!response.ok) {
      const data = await response.json().catch(() => ({}));
      throw new ApiError(data?.error || 'Failed to upload attachment.', response.status);
    }
    return await response.json();
  },

  async deleteAttachment(attachmentId) {
    return request(`/api/attachments/${attachmentId}`, {
      method: 'DELETE',
    });
  },

  // Phase 4: User Preferences APIs
  async getPreferences() {
    return request('/api/users/preferences', {
      method: 'GET',
    });
  },

  async updatePreferences(preferences) {
    return request('/api/users/preferences', {
      method: 'PUT',
      body: JSON.stringify(preferences),
    });
  },
};
