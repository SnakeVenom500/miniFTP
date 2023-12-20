int createServerDataConnection(int connectfd);
int changeServerDirectory(char* path, int connectfd);
int listServerContents(int connectfd, int dataConnectFD);
int getFile_Server(int connectfd, int dataConnectFD);
int putFile_Server(int connectfd, int dataConnectFD);

void startCommandLine(int socketfd, char* address);
int createSocketConnection(char* portNumber, char* address);
int createClientDataConnection(int socketfd, char* address);
int listClientContents();
int changeClientDirectory(char* path);
int readServerResponse(int socketfd);
int rls(int socketfd, int dataConnectFD);
int getFile_Client(int socketfd, char* path, char* address);
int show(int socketfd, char* path, char* address);
int put(int socketfd, char* path, char* address);
