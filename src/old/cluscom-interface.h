#ifndef GUARD_cluscom_interface_h
#define GUARD_cluscom_interface_h

#include "cluscom.h"

int cluscom_query( char* host, int port, char* input, int len_in, char* output, int len_out, apr_pool_t* mempool );

#endif
