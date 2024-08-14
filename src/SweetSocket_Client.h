EXPORT bool SweetSocket_clientStartConnection(struct SweetSocket_global_context *context, enum SweetSocket_apply_on serverID);

EXPORT bool SweetSocket_clientEnablePools(struct SweetSocket_global_context *context, enum SweetSocket_apply_on clientID, bool enableRecivePool, bool enableSendPool);
