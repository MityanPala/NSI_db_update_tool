#include <iostream>
#include <fstream>
#include <string>
#include "curl.h"

using namespace std;

static size_t write_data(char* ptr, size_t size, size_t nmemb, string* data)
{
	if (data)
	{
		data->append(ptr, size * nmemb);
		return size * nmemb;
	}
	else return 0;  // будет ошибка
}

static size_t write_head(char* ptr, size_t size, size_t nmemb, std::ostream* stream)
{
	(*stream) << string(ptr, size * nmemb);
	return size * nmemb;
}

CURLcode basicRequest(string urlF, CURL* curlHandle, string* contentString);

int main() // по порядку возвращаем значения сверху вниз с увеличением на 1 - если дошли до самого конца - там получим 0
{
	/*****************************************************************/
	CURL* curl;
	CURLcode res;
	curl_global_init(CURL_GLOBAL_DEFAULT); // this needs to be initialized once - will allow code below to use libcurl
	/*****************************************************************/
	//cout << system("mkdir all_data"); // 1 если уже существует
	system("mkdir all_data");
	system("mkdir all_data\\configuration_and_logs");
	system("mkdir all_data\\configuration_and_logs\\logs");
	system("mkdir all_data\\updated_data");
	system("mkdir all_data\\updated_data\\documents_lists");
	system("mkdir all_data\\updated_data\\documents");
	system("cls");
	//ShowWindow(GetConsoleWindow(), SW_HIDE); top!

	string separator = "//--------------------------------------------------------------------------";
	string logsName = "all_data\\configuration_and_logs\\logs\\overall_app_logs.txt";
	time_t seconds = time(NULL);

	fstream writeLogs;
	writeLogs.open(logsName, ios::app);
	if (!writeLogs)
	{
		logsName = "all_data\\configuration_and_logs\\logs\\";
		logsName += asctime(localtime(&seconds)); // local time
		logsName += "_app_logs.txt";
		writeLogs.open(logsName, ios::out);
		if (!writeLogs)
		{
			return 100;
		}
	}
	writeLogs << endl << separator << endl << "|start of process: " << asctime_s(localtime(&seconds)) << "|" << endl;

	string urlBase = "http://nsi.rosminzdrav.ru/port/rest/searchDictionary?userKey=";
	
	string pages[] = { "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15" }; // костыль, убрать
	string pageNumber = pages[1];

	string tokenString = "";
	string fileName = "";

	/***EXTRACT USAGE INFO FROM FILE***/
	fstream extractToken, extractProxyInfo;
	extractToken.open("all_data\\configuration_and_logs\\user_token.txt", ios::in);
	if (!extractToken)
	{

		return 1;
	}
	getline(extractToken, tokenString);
	extractToken.close();
	/***EXTRACT USAGE INFO FROM FILE***/

	curl = curl_easy_init();
	if (curl)
	{
		string content = "";
		string urlString = "";

		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L); // SKIP_PEER_VERIFICATION
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L); // SKIP_HOSTNAME_VERIFICATION
		
		urlString = urlBase + tokenString + "&page=" + pageNumber + "&size=200"; // размер выдачи больше 400 иногда не работает и сервер не хочет его обрабатывать

		//попробовать сделать запрос без прокси - если проходит - все ок, не проходит -> задаем параметры прокси(которые слетят только после cleanup'а)
		res = basicRequest(urlString, curl, &content);
		if (res != CURLE_OK)
		{

		}
		cout << endl << "|" << res << endl;
		system("pause");
		return 0;

		/*curl_easy_setopt(curl, CURLOPT_HTTPPROXYTUNNEL, 1L);
		curl_easy_setopt(curl, CURLOPT_PROXYAUTH, CURLAUTH_BASIC);
		curl_easy_setopt(curl, CURLOPT_PROXY, "10.14.10.147:8080");
		curl_easy_setopt(curl, CURLOPT_PROXYTYPE, CURLPROXY_HTTP); //traffic inspector http ftp 2.0.1.721 
		
		curl_easy_setopt(curl, CURLOPT_PROXYUSERNAME, "dima");
		curl_easy_setopt(curl, CURLOPT_PROXYPASSWORD, "123");
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
		curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);*/

		/*****************************************************************/
		

		curl_easy_setopt(curl, CURLOPT_URL, urlString.c_str()); // не работает со string, надо преобразовывать в const
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &content);

		res = curl_easy_perform(curl); // выполнение запроса

		cout << "---" << res << "---" << endl;
		
		if ("" == content) // при несрабатывании типа аутентификации в прокси
		{
			cout << endl << content << " is empty..." << endl;
		}
		system("pause");

		fstream F;
		fileName = "all_data\\updated_data\\" + pageNumber + "_page.txt";
		F.open(fileName.c_str(), ios::out);
		F << content;
		F.close();
		if (res != CURLE_OK)
		{
			fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
		}
		int position = content.find("\"total\":");
		string substrInContent = content.substr(position + 8, 15);
		//cout << endl << substrInContent << endl;
		position = substrInContent.find(",");
		substrInContent = substrInContent.substr(0, position); // 8 - length of "total:"
		//cout << endl << substrInContent << endl;
		//system("pause");
		int convertedStringToInt = atoi(substrInContent.c_str());
		if (convertedStringToInt)
		{
			cout << endl << convertedStringToInt << "|" << "SUCCESS!" << endl;
		}
		else
		{
			return 0; // не знаю, как обработать, кол-во страниц при ошибке
		}
		// TODO - почистить код сверху, чтобы он был более читаемый(привести в порядок и переделать)
		int maxPages = convertedStringToInt / 200;
		if ((((double)convertedStringToInt) / 200.0) > ((double)maxPages))
		{
			maxPages++; // если к примеру в выдаче 1201 справочник, то мы получим 6 maxPages, но если разделить не нацело, то мы увидим, что надо прибавить 1 к кол-ву страниц
		}

		for (int j = 2; j <= maxPages; j++)
		{
			pageNumber = pages[j];
			urlString = urlBase + tokenString + "&page=" + pageNumber + "&size=200";
			curl_easy_setopt(curl, CURLOPT_URL, urlString.c_str());
			//string content2;
			content = "";
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &content);
			res = curl_easy_perform(curl); // выполнение запроса
			fileName = "all_data\\updated_data\\" + pageNumber + "_page.txt";
			F.open(fileName.c_str(), ios::out);
			F << content;
			F.close();
			if (res != CURLE_OK)
			{
				fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
			}
		}
		// TODO - почистить код сверху, чтобы он был более читаемый(привести в порядок и переделать)
		curl_easy_cleanup(curl);
	}
	else
	{
		cout << " An error occured, while initializing libcurl, try to launch the program again...";
		Sleep(10000);			// ошибка при инициализации объекта curl, необходимо либо заново инициализировать,
		cout << endl << endl;	// либо заново запустить программу - иначе искать ошибки в совместимости
		system("pause");
		return 0;
	}

	curl_global_cleanup(); // when program doesn't use libcurl anymore in the following code
	writeLogs << "|end of process: " << asctime(localtime(&seconds)) << "|" << endl << separator << endl;
	writeLogs.close();
	return 0;
}

CURLcode basicRequest(string urlF, CURL* curlHandle, string* contentString)
{
	CURLcode resF;

	curl_easy_setopt(curlHandle, CURLOPT_URL, urlF.c_str());
	curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, write_data);
	curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, contentString);
	resF = curl_easy_perform(curlHandle);

	return resF;
}