#include <task_curl/task_curl.h>
#include <curl/curl.h>

int task_curl()
{
	CURL* curl = curl_easy_init();
	curl_easy_cleanup(curl);
	return 1;
}
