#ifndef GUARD_cluscom_config_h
#define GUARD_cluscom_config_h

//for ClusCom data properties
#define REQUEST_SIZE (4)
#define REPLY_SIZE (800)
#define MANIPULATE_REQUEST_SIZE (4)
#define MANIPULATE_PROCESSED_SIZE (4)

//for ClusCom network properties
#define REQUEST_PORT (2898)
#define REPLY_PORT (9853)
#define MANIPULATE_PORT (4759)
#define DAEMON_PORT (8823)

//for ClusCom socket properties
#define DEF_SOCKET_TIMEOUT (200000)
#define DEF_SOCKET_BACKLOG (SOMAXCONN)
#define DEF_ERROR_RETRY_TIME (1000000)
#define DEF_PROCESS_TIMEOUT (100000)

//for debugging
#define FRL_DEBUGGING

#endif
