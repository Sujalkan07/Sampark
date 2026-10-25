import React from 'react';
import { Navigate, useParams } from 'react-router-dom';

/**
 * ConversationPage has been refactored into an in-place split view
 * within DashboardPage (/dashboard/:conversationId) following the
 * WhatsApp Web-style dashboard layout requirement.
 * This component cleanly redirects any direct links to the new route.
 */
export function ConversationPage() {
  const { conversationId } = useParams();
  return <Navigate to={conversationId ? `/dashboard/${conversationId}` : '/dashboard'} replace />;
}

export default ConversationPage;
