#include "../include/frl_logging.h"

int main()
{
	F_INFO_IF_RUN(10 > 9, printf("xxx\n"), "i am here %d %d\n", 11, 12);
	return 0;
}
