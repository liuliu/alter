#include "cluscom-interface.h"
#include <iostream>
int main()
{
	apr_initialize();
	apr_pool_t* mempool;
	apr_pool_create( &mempool, NULL );
	int n;
	std::cin>>n;
	int *replies = (int*)malloc( REPLY_SIZE );
	if ( cluscom_query("localhost", DAEMON_PORT, (char*)&n, 4, (char*)replies, REPLY_SIZE, mempool ) == 0 )
	{
		int *iter_replies = replies;
		for ( int i = 0; i < 100; i++, iter_replies+=2 )
		{
			std::cout<<*iter_replies<<" "<<*(iter_replies+1)<<std::endl;
		}
		std::cout<<"The End."<<std::endl;
	} else {
		printf("Error!\n");
	}
	apr_terminate();
}
