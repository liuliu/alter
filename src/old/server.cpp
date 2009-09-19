#include "cluscom-daemon.h"

int main()
{
	apr_initialize();
	apr_pool_t* mempool;
	apr_pool_create( &mempool, NULL );
	ClusCom_Daemon* daemon;
	daemon = new ClusCom_Daemon ("localhost", 100, mempool);
	daemon->spawn(5);
	daemon->hold();
	apr_terminate();
}
