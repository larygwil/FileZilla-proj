#define MULT 100
#define TIMEOUT (2 * MULT)
#define MAXCONNECT (5 * MULT)
#define TOTALTIME (20 * MULT)

struct instance_data
{
	int sock;
	sockaddr_in addr;
	char ip[100];
	int id;
};

void* instance_main(void *p);

class CInstance
{
public:
	CInstance(int id, int fd, sockaddr_in addr);
	virtual ~CInstance();

	bool Run();

protected:
	static void* ThreadProc(void* data);

	void Main();
	void Close();
	bool Send(const char* str);
	void printf(const char* fmt, ...);
	void DataSocketEvent();

	int sock, datasock;
	sockaddr_in addr;
	int id;

	char clientip[16];	// Filled by IP
	int dataport;		// Filled by PREP
	int data;			// Filled by PREP
};

