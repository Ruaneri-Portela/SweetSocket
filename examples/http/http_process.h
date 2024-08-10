#include <stdint.h>

struct HTTPsendParms {
	const wchar_t* path;
	const wchar_t* virtualPath;
	int64_t requestedStartFile;
	int64_t requestedEndFile;
	struct HTTP_server_envolvirment* envolvirment;
};

