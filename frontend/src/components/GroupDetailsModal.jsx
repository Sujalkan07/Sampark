import React, { useState, useEffect } from 'react';
import { api } from '../api/client';
import { useAuth } from '../context/AuthContext';
import { AlertBanner } from './AlertBanner';
import { getPresenceStatus } from '../utils/date';

export function GroupDetailsModal({ isOpen, onClose, conversation, onGroupUpdated, onGroupLeft }) {
  const { user } = useAuth();
  const [members, setMembers] = useState([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);
  const [success, setSuccess] = useState(null);

  // Edit Group state (admin only)
  const [isEditing, setIsEditing] = useState(false);
  const [editName, setEditName] = useState(conversation?.name || '');
  const [editDesc, setEditDesc] = useState(conversation?.description || '');
  const [savingEdit, setSavingEdit] = useState(false);

  // Add Member state
  const [isAddingMember, setIsAddingMember] = useState(false);
  const [availableUsers, setAvailableUsers] = useState([]);
  const [selectedUserToAdd, setSelectedUserToAdd] = useState('');
  const [addingMember, setAddingMember] = useState(false);

  // Determine if current user is admin
  const currentMemberRecord = members.find((m) => m.userId === user?.id);
  const isAdmin = currentMemberRecord?.role === 'admin' || conversation?.createdBy === user?.id;

  const fetchMembers = async () => {
    if (!conversation?.id) return;
    try {
      setLoading(true);
      const res = await api.getGroupMembers(conversation.id);
      setMembers(res?.members || []);
    } catch (err) {
      setError(err.message || 'Failed to fetch group members.');
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    if (!isOpen || !conversation?.id) return;
    setEditName(conversation.name || '');
    setEditDesc(conversation.description || '');
    setIsEditing(false);
    setIsAddingMember(false);
    setError(null);
    setSuccess(null);
    fetchMembers();
  }, [isOpen, conversation]);

  const handleOpenAddMember = async () => {
    setIsAddingMember(true);
    try {
      const data = await api.searchUsers('', 100);
      const existingIds = new Set(members.map((m) => m.userId));
      const notInGroup = (data?.users || []).filter((u) => !existingIds.has(u.id));
      setAvailableUsers(notInGroup);
      if (notInGroup.length > 0) {
        setSelectedUserToAdd(notInGroup[0].id.toString());
      }
    } catch (err) {
      setError('Failed to load user list.');
    }
  };

  const handleAddMemberSubmit = async (e) => {
    e.preventDefault();
    if (!selectedUserToAdd) return;
    try {
      setAddingMember(true);
      setError(null);
      await api.addGroupMember(conversation.id, parseInt(selectedUserToAdd, 10));
      setSuccess('Member added successfully.');
      setIsAddingMember(false);
      fetchMembers();
      onGroupUpdated?.();
    } catch (err) {
      setError(err.message || 'Failed to add member.');
    } finally {
      setAddingMember(false);
    }
  };

  const handleRemoveMember = async (targetUserId, memberName) => {
    if (!window.confirm(`Are you sure you want to remove ${memberName} from this group?`)) return;
    try {
      setError(null);
      await api.removeGroupMember(conversation.id, targetUserId);
      setSuccess(`Removed ${memberName} from group.`);
      fetchMembers();
      onGroupUpdated?.();
    } catch (err) {
      setError(err.message || 'Failed to remove member.');
    }
  };

  const handleSaveDetails = async (e) => {
    e.preventDefault();
    if (!editName.trim()) {
      setError('Group name cannot be empty.');
      return;
    }
    try {
      setSavingEdit(true);
      setError(null);
      const res = await api.updateGroup(conversation.id, {
        name: editName.trim(),
        description: editDesc.trim(),
      });
      setIsEditing(false);
      setSuccess('Group details updated.');
      if (res?.conversation) {
        onGroupUpdated?.(res.conversation);
      }
    } catch (err) {
      setError(err.message || 'Failed to update group details.');
    } finally {
      setSavingEdit(false);
    }
  };

  const handleLeaveGroup = async () => {
    if (!window.confirm('Are you sure you want to leave this group?')) return;
    try {
      setError(null);
      await api.leaveGroup(conversation.id);
      onGroupLeft?.();
      onClose?.();
    } catch (err) {
      setError(err.message || 'Failed to leave group.');
    }
  };

  const handleDeleteGroup = async () => {
    if (!window.confirm('CAUTION: Are you sure you want to disband and permanently delete this group? All messages and history will be deleted.')) return;
    try {
      setError(null);
      await api.deleteGroup(conversation.id);
      onGroupLeft?.();
      onClose?.();
    } catch (err) {
      setError(err.message || 'Failed to delete group.');
    }
  };

  if (!isOpen) return null;

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center p-4 bg-black/40 backdrop-blur-xs animate-fade-in">
      <div
        className="w-full max-w-lg bg-surface-container-lowest border border-border-hairline rounded-2xl shadow-2xl flex flex-col max-h-[90vh] overflow-hidden text-on-surface"
        role="dialog"
        aria-modal="true"
      >
        {/* Header Banner */}
        <div className="p-6 bg-gradient-to-r from-primary to-primary-container text-white shrink-0 relative">
          <button
            type="button"
            onClick={onClose}
            className="absolute top-4 right-4 w-8 h-8 rounded-full bg-white/10 hover:bg-white/20 text-white flex items-center justify-center transition-colors cursor-pointer"
            aria-label="Close"
          >
            ✕
          </button>

          <div className="flex items-center gap-4">
            <div className="w-14 h-14 rounded-2xl bg-white/10 border border-white/20 flex items-center justify-center text-white text-xl font-bold shadow-md select-none shrink-0">
              <span className="material-symbols-outlined text-[28px]">groups</span>
            </div>

            <div className="min-w-0 flex-1">
              <h2 className="text-lg font-bold truncate leading-tight">
                {conversation?.name || 'Group Chat'}
              </h2>
              <div className="flex items-center gap-2 mt-1 text-xs text-white/80 font-mono">
                <span>{members.length} member{members.length !== 1 ? 's' : ''}</span>
                <span>•</span>
                <span>Created {new Date(conversation?.createdAt).toLocaleDateString()}</span>
              </div>
            </div>
          </div>
        </div>

        {/* Modal Body */}
        <div className="p-6 overflow-y-auto space-y-5 flex-1 custom-scrollbar">
          {error && <AlertBanner type="error" message={error} onClose={() => setError(null)} />}
          {success && <AlertBanner type="success" message={success} onClose={() => setSuccess(null)} />}

          {/* Group Details (View / Edit) */}
          <div className="bg-surface-container-low/40 rounded-xl p-4 border border-border-hairline">
            {isEditing ? (
              <form onSubmit={handleSaveDetails} className="space-y-3">
                <div>
                  <label className="block text-xs font-semibold text-primary mb-1">Group Name</label>
                  <input
                    type="text"
                    value={editName}
                    onChange={(e) => setEditName(e.target.value)}
                    className="w-full px-3 py-1.5 text-xs rounded-lg bg-surface-container-lowest border border-border-hairline focus:border-secondary outline-none"
                    required
                  />
                </div>
                <div>
                  <label className="block text-xs font-semibold text-primary mb-1">Description</label>
                  <textarea
                    value={editDesc}
                    onChange={(e) => setEditDesc(e.target.value)}
                    rows={2}
                    className="w-full px-3 py-1.5 text-xs rounded-lg bg-surface-container-lowest border border-border-hairline focus:border-secondary outline-none resize-none"
                  />
                </div>
                <div className="flex items-center justify-end gap-2 pt-1">
                  <button
                    type="button"
                    onClick={() => setIsEditing(false)}
                    className="px-3 py-1 text-xs text-outline hover:text-on-surface cursor-pointer"
                  >
                    Cancel
                  </button>
                  <button
                    type="submit"
                    disabled={savingEdit}
                    className="px-3 py-1 text-xs font-semibold rounded-lg bg-primary text-white cursor-pointer"
                  >
                    {savingEdit ? 'Saving...' : 'Save'}
                  </button>
                </div>
              </form>
            ) : (
              <div>
                <div className="flex items-start justify-between">
                  <div>
                    <h3 className="text-xs font-bold text-primary uppercase tracking-wider font-mono">Description</h3>
                    <p className="text-xs text-on-surface mt-1 leading-relaxed">
                      {conversation?.description || <span className="text-outline italic">No description provided.</span>}
                    </p>
                  </div>
                  {isAdmin && (
                    <button
                      type="button"
                      onClick={() => setIsEditing(true)}
                      className="text-xs font-semibold text-secondary hover:underline cursor-pointer flex items-center gap-1"
                    >
                      <span className="material-symbols-outlined text-[14px]">edit</span>
                      <span>Edit</span>
                    </button>
                  )}
                </div>
              </div>
            )}
          </div>

          {/* Members Section */}
          <div>
            <div className="flex items-center justify-between mb-2">
              <h3 className="text-xs font-bold text-primary uppercase tracking-wider font-mono">
                Group Members ({members.length})
              </h3>
              {isAdmin && !isAddingMember && (
                <button
                  type="button"
                  onClick={handleOpenAddMember}
                  className="px-2.5 py-1 text-xs font-semibold rounded-lg bg-surface-container hover:bg-surface-container-high text-primary flex items-center gap-1 cursor-pointer transition-colors shadow-xs"
                >
                  <span className="material-symbols-outlined text-[16px]">person_add</span>
                  <span>Add Member</span>
                </button>
              )}
            </div>

            {/* Add Member inline form */}
            {isAddingMember && (
              <form onSubmit={handleAddMemberSubmit} className="mb-3 p-3 bg-surface-container-low rounded-xl border border-border-hairline flex items-center gap-2">
                <select
                  value={selectedUserToAdd}
                  onChange={(e) => setSelectedUserToAdd(e.target.value)}
                  className="flex-1 px-3 py-1.5 text-xs rounded-lg bg-surface-container-lowest border border-border-hairline outline-none text-on-surface"
                >
                  {availableUsers.length === 0 ? (
                    <option value="">No other users available</option>
                  ) : (
                    availableUsers.map((u) => (
                      <option key={u.id} value={u.id}>
                        {u.fullName} (@{u.username})
                      </option>
                    ))
                  )}
                </select>
                <button
                  type="submit"
                  disabled={addingMember || availableUsers.length === 0}
                  className="px-3 py-1.5 text-xs font-semibold rounded-lg bg-primary text-white cursor-pointer disabled:opacity-50"
                >
                  {addingMember ? 'Adding...' : 'Add'}
                </button>
                <button
                  type="button"
                  onClick={() => setIsAddingMember(false)}
                  className="px-2 py-1.5 text-xs text-outline hover:text-on-surface cursor-pointer"
                >
                  ✕
                </button>
              </form>
            )}

            {/* Members List */}
            <div className="border border-border-hairline rounded-xl divide-y divide-border-hairline/60 overflow-hidden">
              {loading ? (
                <div className="py-6 text-center text-xs text-outline flex items-center justify-center gap-2">
                  <span className="w-4 h-4 border-2 border-primary border-t-transparent rounded-full animate-spin"></span>
                  <span>Loading members...</span>
                </div>
              ) : (
                members.map((m) => {
                  const mUser = m.user;
                  const isSelf = m.userId === user?.id;
                  const presence = getPresenceStatus(mUser?.lastActiveAt);

                  return (
                    <div key={m.id} className="p-3 flex items-center justify-between hover:bg-surface-container/40 transition-colors">
                      <div className="flex items-center gap-3 min-w-0">
                        <div className="relative shrink-0">
                          <div className="w-8 h-8 rounded-full bg-primary-container text-white flex items-center justify-center text-xs font-bold shadow-xs">
                            {(mUser?.fullName || mUser?.username || 'U')[0].toUpperCase()}
                          </div>
                          {presence.isOnline && (
                            <span className="absolute bottom-0 right-0 w-2 h-2 rounded-full bg-india-green ring-2 ring-white"></span>
                          )}
                        </div>

                        <div className="truncate">
                          <div className="flex items-center gap-2">
                            <span className="text-xs font-semibold text-on-surface truncate">
                              {mUser?.fullName || 'User'}
                            </span>
                            {isSelf && (
                              <span className="text-[10px] px-1.5 py-0.2 rounded-full bg-surface-container-high text-outline font-mono">
                                You
                              </span>
                            )}
                            {m.role === 'admin' && (
                              <span className="text-[10px] px-1.5 py-0.2 rounded-full bg-saffron/15 text-saffron-hover font-semibold font-mono">
                                Admin
                              </span>
                            )}
                          </div>
                          <div className="text-[11px] text-outline font-mono">
                            @{mUser?.username} • <span className={presence.isOnline ? 'text-india-green' : ''}>{presence.label}</span>
                          </div>
                        </div>
                      </div>

                      {/* Admin remove button */}
                      {isAdmin && !isSelf && (
                        <button
                          type="button"
                          onClick={() => handleRemoveMember(m.userId, mUser?.fullName || mUser?.username)}
                          className="text-[11px] font-semibold text-error/80 hover:text-error hover:bg-red-50 px-2 py-1 rounded-md transition-colors cursor-pointer"
                          title="Remove from group"
                        >
                          Remove
                        </button>
                      )}
                    </div>
                  );
                })
              )}
            </div>
          </div>
        </div>

        {/* Footer Actions */}
        <div className="px-6 py-3.5 bg-surface-container-low border-t border-border-hairline flex items-center justify-between shrink-0">
          <button
            type="button"
            onClick={handleLeaveGroup}
            className="text-xs font-semibold text-error hover:underline cursor-pointer flex items-center gap-1"
          >
            <span className="material-symbols-outlined text-[16px]">logout</span>
            <span>Leave Group</span>
          </button>

          <div className="flex items-center gap-2">
            {isAdmin && (
              <button
                type="button"
                onClick={handleDeleteGroup}
                className="px-3 py-1.5 text-xs font-semibold rounded-xl bg-error/10 hover:bg-error/20 text-error transition-colors cursor-pointer"
              >
                Delete Group
              </button>
            )}
            <button
              type="button"
              onClick={onClose}
              className="px-4 py-1.5 text-xs font-semibold rounded-xl bg-surface-container hover:bg-surface-container-high text-on-surface transition-colors cursor-pointer shadow-xs"
            >
              Done
            </button>
          </div>
        </div>
      </div>
    </div>
  );
}

export default GroupDetailsModal;
