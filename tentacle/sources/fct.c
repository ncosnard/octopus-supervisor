#include "../headers/fct.h"
#include <openssl/err.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <dirent.h>
#include <syslog.h>
#include <time.h>

void InitializeSSL() {
	SSL_load_error_strings();
	SSL_library_init();
	OpenSSL_add_all_algorithms();
}
void DestroySSL() {
	ERR_free_strings();
	EVP_cleanup();
}

void ShutdownSSL(SSL *ssl) {
	SSL_shutdown(ssl);
	SSL_free(ssl);
}

void getSSLerr(int err){
	switch(err){
		case SSL_ERROR_NONE:
			logError(LOG_LEVEL_ERROR,"sslerror : zero");
			break;
		case SSL_ERROR_ZERO_RETURN:
			logError(LOG_LEVEL_ERROR,"sslerror : zero return");
			break;
		case SSL_ERROR_WANT_READ:
			logError(LOG_LEVEL_ERROR,"sslerror : want read");
			break;
		case SSL_ERROR_WANT_WRITE:
			logError(LOG_LEVEL_ERROR,"sslerror : want write");
			break;
		case SSL_ERROR_WANT_CONNECT:
			logError(LOG_LEVEL_ERROR,"sslerror : want connect");
			break;
		case SSL_ERROR_WANT_ACCEPT:
			logError(LOG_LEVEL_ERROR,"sslerror : want accept");
			break;
		case SSL_ERROR_WANT_X509_LOOKUP:
			logError(LOG_LEVEL_ERROR,"sslerror : want X509 lookup");
			break;
		case SSL_ERROR_SYSCALL:
			logError(LOG_LEVEL_ERROR,"sslerror : syscall");
			break;
		case SSL_ERROR_SSL:
			logError(LOG_LEVEL_ERROR,"sslerror : ssl");
	}
}

int create_listenSock(int port){
	struct sockaddr_in listenExtremity;
	int sock, iSetOption = 1;

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if(sock < 0){
		return -1;
	}
	if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char*) &iSetOption, sizeof (iSetOption)) != 0){
		return -1;
	}

	listenExtremity.sin_family = AF_INET;
	listenExtremity.sin_addr.s_addr = INADDR_ANY;
	listenExtremity.sin_port = htons(port);
	if (bind(sock, (struct sockaddr *) &listenExtremity, sizeof (listenExtremity)) != 0) {
		close(sock);
		return -1;
	}
	if(listen(sock, 10) != 0){
		close(sock);
		return -1;
	}
	return sock;
}

int sendMsg(SSL *cSSL,char *msg){
	int ret;
	ret = SSL_write(cSSL,msg,strlen(msg));
	if(ret <= 0){
		getSSLerr(SSL_get_error(cSSL,ret));
		ShutdownSSL(cSSL);
		return -1;
	}
	return 0;
}
int getMsg(SSL *cSSL,char *msg,char* compareTo){
	int ret,i;
	char bufIn[BUFSIZ];
	for(i=0;i<BUFSIZ;i++) bufIn[i] = 0;
	ret = SSL_read(cSSL,bufIn,BUFSIZ);
	if(ret <= 0){
		getSSLerr(SSL_get_error(cSSL,ret));
		ShutdownSSL(cSSL);
		return -1;
	}
	for(i=0;i<BUFSIZ && bufIn[i] != '\n';i++);
	bufIn[i] = '\0';
	strcpy(msg,bufIn);
	if(compareTo != NULL){
		if(strcmp(msg,compareTo) != 0){
			ShutdownSSL(cSSL);
			return 1;
		}
	}
	return 0;
}
char *list(){
	char *ret;
	int size=1;
	DIR *dir;
	struct dirent *entry;
	char *name;
	int i;

	ret = (char *)malloc(sizeof(char));
	ret[0] = '\0';
	dir = opendir("/usr/share/octopus/tentacle/scripts");
	entry = readdir(dir);
	while(entry != NULL){
		if(entry->d_name[0] != '.'){
			name = entry->d_name;
			for(i=0;name[i]!='.'&&name[i]!='\0';i++); 
			name[i] = '\0';
			size += strlen(name)+1;
			ret = (char *)realloc(ret,size);
			sprintf(ret,"%s%s:",ret,name);
 		}
		entry = readdir(dir);
 	}
	ret[strlen(ret)-1] = '\0';
	return ret;
}

int logType(int param){
	static int type=3;
	if(param > 0 && param < LOG_TYPE_LOGFILE){
		type = param;
	}		
	return type;
}
int logLevel(int param){
	static int level=5;
	if(param > 0 && param < LOG_LEVEL_CRITICAL){
		level = param;
	}
	return level;
}
 int logError(int logValue, char *logMessage){
	char *msg;
	int sysLevel;
	time_t tps;
	struct tm instant;
	tps=time(NULL);
	instant=*localtime(&tps);
	if(logValue < logLevel(0)) return 0;
	msg = (char *)malloc(strlen(logMessage) + 512);
	switch(logValue){
		case LOG_LEVEL_DEBUG:
			sysLevel = LOG_DEBUG;
			sprintf(msg, "\033[36;01m%d%d%d%d%d%d : debug  > %s\033[00m\n",instant.tm_year+1900, instant.tm_mon+1, instant.tm_mday, instant.tm_hour, instant.tm_min, instant.tm_sec,logMessage);
			break;
		case LOG_LEVEL_INFO:
			sysLevel = LOG_INFO;
			sprintf(msg, "%d%d%d%d%d%d : info   > %s\n",instant.tm_year+1900, instant.tm_mon+1, instant.tm_mday, instant.tm_hour, instant.tm_min, instant.tm_sec,logMessage);
			break;
		case LOG_LEVEL_WARNING:
			sysLevel = LOG_WARNING;
			sprintf(msg, "\033[33m%d%d%d%d%d%d : warn   > %s\033[00m\n",instant.tm_year+1900, instant.tm_mon+1, instant.tm_mday, instant.tm_hour, instant.tm_min, instant.tm_sec,logMessage);
			break;
		case LOG_LEVEL_ERROR:
			sysLevel = LOG_ERR;
			sprintf(msg, "\033[31;01m%d%d%d%d%d%d : error  > %s\033[00m\n",instant.tm_year+1900, instant.tm_mon+1, instant.tm_mday, instant.tm_hour, instant.tm_min, instant.tm_sec,logMessage);
			break;
		default :
		case LOG_LEVEL_CRITICAL:
			sysLevel = LOG_CRIT;
			sprintf(msg, "\033[37;41;01m%d%d%d%d%d%d : crit   > %s\033[00m\n",instant.tm_year+1900, instant.tm_mon+1, instant.tm_mday, instant.tm_hour, instant.tm_min, instant.tm_sec,logMessage);
			break;
 	}
 	switch(logType(0)){
		case LOG_TYPE_STDOUT:
			write(STDOUT_FILENO,msg,strlen(msg));
			break;
		case LOG_TYPE_STDERR:
			write(STDERR_FILENO,msg,strlen(msg));
			break;
		case LOG_TYPE_LOGFILE:
			syslog(sysLevel,msg);
			break;
	}
	return 0;
}


