#ifndef MESSAGE_IDENTIFIER
#define MESSAGE_IDENTIFIER(n,p) const extern EXPORT_COREMODULE MessageIdentifier n
#endif

// NOTE: Whether messages are sent to scripts at all or sent to disabled scripts is determined on a per callback basis here,
//       make sure you set MessageIdentifier::Options appropriately for your message, leaving out kDontSendToDisabled will
//       mean that script code in the callback function will be executed regardless of whether the script is enabled or not.

MESSAGE_IDENTIFIER(kEnterTrigger, ("OnTriggerEnter", MessageIdentifier::kSendToScripts, ClassID (Collider)));
MESSAGE_IDENTIFIER(kExitTrigger, ("OnTriggerExit", MessageIdentifier::kSendToScripts, ClassID (Collider)));
MESSAGE_IDENTIFIER(kStayTrigger, ("OnTriggerStay", MessageIdentifier::kSendToScripts, ClassID (Collider)));
MESSAGE_IDENTIFIER(kEnterContact, ("OnCollisionEnter", MessageIdentifier::kSendToScripts, ClassID (Collision)));
MESSAGE_IDENTIFIER(kExitContact, ("OnCollisionExit", MessageIdentifier::kSendToScripts, ClassID (Collision)));
MESSAGE_IDENTIFIER(kStayContact, ("OnCollisionStay", MessageIdentifier::kSendToScripts, ClassID (Collision)));
MESSAGE_IDENTIFIER(kCollisionEnter2D, ("OnCollisionEnter2D", MessageIdentifier::kSendToScripts, ClassID (Collision2D)));
MESSAGE_IDENTIFIER(kCollisionExit2D, ("OnCollisionExit2D", MessageIdentifier::kSendToScripts, ClassID (Collision2D)));
MESSAGE_IDENTIFIER(kCollisionStay2D, ("OnCollisionStay2D", MessageIdentifier::kSendToScripts, ClassID (Collision2D)));
MESSAGE_IDENTIFIER(kTriggerEnter2D, ("OnTriggerEnter2D", MessageIdentifier::kSendToScripts, ClassID (Collider2D)));
MESSAGE_IDENTIFIER(kTriggerExit2D, ("OnTriggerExit2D", MessageIdentifier::kSendToScripts, ClassID (Collider2D)));
MESSAGE_IDENTIFIER(kTriggerStay2D, ("OnTriggerStay2D", MessageIdentifier::kSendToScripts, ClassID (Collider2D)));
MESSAGE_IDENTIFIER(kJointBreak, ("OnJointBreak", MessageIdentifier::kSendToScripts, ClassID (float)));
MESSAGE_IDENTIFIER(kTransformChanged, ("OnTransformChanged", MessageIdentifier::kDontSendToScripts, ClassID (int)));
MESSAGE_IDENTIFIER(kParticleCollisionEvent, ("OnParticleCollision", MessageIdentifier::kSendToScripts, ClassID (GameObject)));
MESSAGE_IDENTIFIER(kDidModifyValidity, ("OnDidModifyMeshValidity", MessageIdentifier::kDontSendToScripts));
MESSAGE_IDENTIFIER(kDidDeleteMesh, ("OnDidModifyMeshDelete", MessageIdentifier::kDontSendToScripts, ClassID (Mesh)));
MESSAGE_IDENTIFIER(kDidModifyBounds, ("OnDidModifyMeshBounds", MessageIdentifier::kDontSendToScripts));
MESSAGE_IDENTIFIER(kDidModifyMesh, ("OnDidModifyMesh", MessageIdentifier::kDontSendToScripts));
MESSAGE_IDENTIFIER(kLayerChanged, ("OnLayersChanged", MessageIdentifier::kDontSendToScripts));
MESSAGE_IDENTIFIER(kDestroyedComponentNotification, ("OnDestroyedComponent", MessageIdentifier::kUseNotificationManager));
MESSAGE_IDENTIFIER(kDidAddComponent , ("OnDidAddComponent", MessageIdentifier::kDontSendToScripts, ClassID (Component)));
MESSAGE_IDENTIFIER(kBecameVisible, ("OnBecameVisible", MessageIdentifier::kSendToScripts));
MESSAGE_IDENTIFIER(kBecameInvisible, ("OnBecameInvisible", MessageIdentifier::kSendToScripts));
MESSAGE_IDENTIFIER(kOnWillRenderObject, ("OnWillRenderObject", (MessageIdentifier::Options)(MessageIdentifier::kSendToScripts|MessageIdentifier::kDontSendToDisabled)));
MESSAGE_IDENTIFIER(kPreCull, ("OnPreCull", (MessageIdentifier::Options)(MessageIdentifier::kSendToScripts|MessageIdentifier::kDontSendToDisabled)));
MESSAGE_IDENTIFIER(kPostRender, ("OnPostRender", (MessageIdentifier::Options)(MessageIdentifier::kSendToScripts|MessageIdentifier::kDontSendToDisabled)));
MESSAGE_IDENTIFIER(kPreRender, ("OnPreRender", (MessageIdentifier::Options)(MessageIdentifier::kSendToScripts|MessageIdentifier::kDontSendToDisabled)));
MESSAGE_IDENTIFIER(kControllerColliderHit, ("OnControllerColliderHit", MessageIdentifier::kSendToScripts, ClassID (MonoObject)));
MESSAGE_IDENTIFIER(kAnimatorMove, ("OnAnimatorMove", (MessageIdentifier::Options)(MessageIdentifier::kSendToScripts|MessageIdentifier::kDontSendToDisabled)));
MESSAGE_IDENTIFIER(kAnimatorMoveBuiltin, ("OnAnimatorMoveBuiltin", MessageIdentifier::kDontSendToScripts, ClassID(RootMotionData)));
MESSAGE_IDENTIFIER(kAnimatorIK, ("OnAnimatorIK", (MessageIdentifier::Options)(MessageIdentifier::kSendToScripts|MessageIdentifier::kDontSendToDisabled), ClassID (int)));
MESSAGE_IDENTIFIER(kForceRecreateCollider, ("ForceRecreateCollider", MessageIdentifier::kDontSendToScripts));
MESSAGE_IDENTIFIER(kRecreateRigidBodyDependencies2D, ("RecreateRigidBodyDependencies2D", MessageIdentifier::kDontSendToScripts, ClassID (Rigidbody2D)));
MESSAGE_IDENTIFIER(kServerInitialized, ("OnServerInitialized", MessageIdentifier::kSendToScripts, ClassID (int), "NetworkPlayer"));
MESSAGE_IDENTIFIER(kPlayerConnected, ("OnPlayerConnected", MessageIdentifier::kSendToScripts, ClassID (int), "NetworkPlayer"));
MESSAGE_IDENTIFIER(kConnectedToServer, ("OnConnectedToServer", MessageIdentifier::kSendToScripts));
MESSAGE_IDENTIFIER(kPlayerDisconnected, ("OnPlayerDisconnected", MessageIdentifier::kSendToScripts, ClassID (int), "NetworkPlayer"));
MESSAGE_IDENTIFIER(kDisconnectedFromServer, ("OnDisconnectedFromServer", MessageIdentifier::kSendToScripts, ClassID (int), "NetworkDisconnection"));
MESSAGE_IDENTIFIER(kConnectionAttemptFailed, ("OnFailedToConnect", MessageIdentifier::kSendToScripts, ClassID (int), "NetworkConnectionError"));
MESSAGE_IDENTIFIER(kMasterServerConnectionAttemptFailed, ("OnFailedToConnectToMasterServer", MessageIdentifier::kSendToScripts, ClassID (int), "NetworkConnectionError"));
MESSAGE_IDENTIFIER(kDisconnectedFromMasterServer, ("OnDisconnectedFromMasterServer", MessageIdentifier::kSendToScripts, ClassID (int), "NetworkDisconnection"));
MESSAGE_IDENTIFIER(kMasterServerEvent, ("OnMasterServerEvent", MessageIdentifier::kSendToScripts, ClassID (int), "MasterServerEvent"));
MESSAGE_IDENTIFIER(kLevelWasLoaded, ("OnLevelWasLoaded", MessageIdentifier::kSendToScripts, ClassID (int)));
MESSAGE_IDENTIFIER(kPlayerPause, ("OnApplicationPause", MessageIdentifier::kSendToScripts, ClassID (bool)));
MESSAGE_IDENTIFIER(kPlayerFocus, ("OnApplicationFocus", MessageIdentifier::kSendToScripts, ClassID (bool)));
MESSAGE_IDENTIFIER(kPlayerQuit, ("OnApplicationQuit", MessageIdentifier::kSendToScripts));
MESSAGE_IDENTIFIER(kTerrainChanged, ("OnTerrainChanged", MessageIdentifier::kSendToScripts, ClassID (int), "TerrainChangedFlags"));  // Investigate getting rid of these
MESSAGE_IDENTIFIER(kSetLightmapIndex, ("SetLightmapIndex", MessageIdentifier::kSendToScripts, ClassID (int)));                       //
MESSAGE_IDENTIFIER(kShiftLightmapIndex, ("ShiftLightmapIndex", MessageIdentifier::kSendToScripts, ClassID (int)));                   //
MESSAGE_IDENTIFIER(kDidModifyAnimatorController, ("OnDidModifyAnimatorController", MessageIdentifier::kDontSendToScripts));
MESSAGE_IDENTIFIER(kDidModifyMotion, ("OnDidModifyMotion", MessageIdentifier::kDontSendToScripts));
MESSAGE_IDENTIFIER(kDidVelocityChange, ("OnDidVelocityChange", MessageIdentifier::kDontSendToDisabled, ClassID (Vector3f)));
MESSAGE_IDENTIFIER(kDidModifyAvatar, ("OnDidModifyAvatar", MessageIdentifier::kDontSendToScripts));

#if ENABLE_NEW_EVENT_SYSTEM

#ifndef SEND_TO_SCRIPTS
#define SEND_TO_SCRIPTS  (MessageIdentifier::Options)(MessageIdentifier::kSendToScripts | MessageIdentifier::kDontSendToDisabled)
#endif

MESSAGE_IDENTIFIER(kOnMouseEnterEvent,	("OnMouseEnterEvent",	SEND_TO_SCRIPTS));
MESSAGE_IDENTIFIER(kOnMouseExitEvent,	("OnMouseExitEvent",	SEND_TO_SCRIPTS));
MESSAGE_IDENTIFIER(kOnMouseMoveEvent,	("OnMouseMoveEvent",	SEND_TO_SCRIPTS));
MESSAGE_IDENTIFIER(kOnPressEvent,		("OnPressEvent",		SEND_TO_SCRIPTS));
MESSAGE_IDENTIFIER(kOnReleaseEvent,		("OnReleaseEvent",		SEND_TO_SCRIPTS));
MESSAGE_IDENTIFIER(kOnDragEvent,		("OnDragEvent",			SEND_TO_SCRIPTS));
MESSAGE_IDENTIFIER(kOnDropEvent,		("OnDropEvent",			SEND_TO_SCRIPTS));
MESSAGE_IDENTIFIER(kOnDragEnterEvent,	("OnDragEnterEvent",	SEND_TO_SCRIPTS));
MESSAGE_IDENTIFIER(kOnDragExitEvent,	("OnDragExitEvent",		SEND_TO_SCRIPTS));
MESSAGE_IDENTIFIER(kOnClickEvent,		("OnClickEvent",		SEND_TO_SCRIPTS));
MESSAGE_IDENTIFIER(kOnKeyEvent,			("OnKeyEvent",			SEND_TO_SCRIPTS));
MESSAGE_IDENTIFIER(kOnSelectEvent,		("OnSelectEvent",		SEND_TO_SCRIPTS));
MESSAGE_IDENTIFIER(kOnDeselectEvent,	("OnDeselectEvent",		SEND_TO_SCRIPTS));
MESSAGE_IDENTIFIER(kOnScrollEvent,		("OnScrollEvent",		SEND_TO_SCRIPTS));
//MESSAGE_IDENTIFIER(kOnShowTooltip,		("OnShowTooltip",	SEND_TO_SCRIPTS));
//MESSAGE_IDENTIFIER(kOnHideTooltip,		("OnHideTooltip",	SEND_TO_SCRIPTS));
#endif
