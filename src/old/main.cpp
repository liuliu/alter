#include "cluscom.h"
#include <iostream>

class ClusComTest : public ClusCom
{
	private:
		virtual int handle( char* to_retrieve, char* to_reply )
		{
			unsigned int size = *((unsigned int*)resource);
			unsigned int number = *((unsigned int*)to_retrieve);
			unsigned int *hand = (unsigned int*)(resource+sizeof(unsigned int));
			unsigned int *mr = (unsigned int*)malloc(size*2*sizeof(unsigned int));
			unsigned int *r = (unsigned int*)to_reply;
			unsigned int *oir = mr;
			for ( int i = 0; i < size; i++ )
			{
				*mr = *hand;
				mr++;
				hand++;
				*mr = abs(*hand-number);
				mr++;
				hand++;
			}
			mr = oir; unsigned int c;
			for ( int i = 0; i < 100; i++, mr+=2, r+=2 )
			{
				unsigned int *j_r = mr+2;
				unsigned int *s = mr;
				for ( int j = i+1; j < size; j++, j_r+=2 )
					if ( *(j_r+1) < *(s+1) ) s = j_r;
				if ( s != mr )
				{
					c = *s; *s = *mr; *mr = c;
					c = *(s+1); *(s+1) = *(mr+1); *(mr+1) = c;
				}
				*r = *mr;
				*(r+1) = *(mr+1);
			}
			free(oir);
			return 0;
		}

		virtual int synthesize( apr_uint32_t &node, char** to_synthe, char* to_reply )
		{
			unsigned int **n = (unsigned int**)malloc( sizeof(unsigned int*)*node );
			unsigned int **on = n;
			for ( int i = 0; i < node; i++, n++ )
				*n = (unsigned int*)(*(to_synthe+i));
			unsigned int *r = (unsigned int*)to_reply;
			unsigned int *oir = r;
			for ( int i = 0; i < 100; i++, r+=2 )
			{
				n = on;
				unsigned int **s = n;
				for ( int j = 0; j < node; j++, n++ )
				{
					if ( *(*n+1) < *(*s+1) )
						s = n;
				}
				*r = **s;
				*(r+1) = *(*s+1);
				printf("%d %d\n", *r, *(r+1));
				(*s)+=2;
			}
			return 0;
		}

		virtual int manipulate( char* to_mani, apr_uint32_t& return_size, char* to_reply )
		{
			*(apr_uint32_t*)to_reply = *(apr_uint32_t*)to_mani+1;
			printf("for god sick, tell me iam here\n");
			return_size = 4;
			return 0;
		}
	public:
		ClusComTest( apr_int32_t _id, apr_int32_t _node, apr_uint32_t _layer, apr_uint32_t maximum, char* parent_addr, char** child_addrs, apr_pool_t* _mempool )
		: ClusCom( _id, _node, _layer, maximum, parent_addr, child_addrs, _mempool )
		{ /* ... */ }
		int load( const char* filename )
		{
			free( resource );
			apr_file_t* newfile;
			apr_file_open( &newfile, filename, APR_READ, APR_OS_DEFAULT, mempool );
			apr_finfo_t finfo;
			apr_file_info_get( &finfo, APR_FINFO_SIZE, newfile );
			apr_mmap_t* mmap;
			apr_mmap_create( &mmap, newfile, 0, finfo.size, APR_MMAP_READ, mempool );
			resource = (char*)malloc( finfo.size );
			memcpy( resource, mmap->mm, finfo.size );
			apr_mmap_delete( mmap );
		}
		virtual ~ClusComTest()
		{
			free( resource );
		}
		char* resource;
};

int main()
{
	apr_initialize();
	apr_pool_t* mempool;
	apr_pool_create( &mempool, NULL );
	ClusComTest *cluscomtest;
	CREATE_CLUSCOM_WITH_FILE( cluscomtest, ClusComTest, "/home/liu/cluscom.conf", mempool );
	cluscomtest->load( "/home/liu/testfile" );
	cluscomtest->spawn(5);
	cluscomtest->hold();
	apr_terminate();
}
