EXPORT bool SweetSocket_serverStartAccepting(struct SweetSocket_global_context *context, enum SweetSocket_apply_on serverID, void *functionSend, void *functionRecv, void *parmsRecv, void *parmsSend, enum SweetSocket_peer_pool_behaviour pool);

EXPORT bool SweetSocket_serverStartListening(struct SweetSocket_global_context *context, enum SweetSocket_apply_on serverID);