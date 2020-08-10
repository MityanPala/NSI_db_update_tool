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

int main()
{
	/*****************************************************************/
	CURL* curl;
	CURLcode res;
	curl_global_init(CURL_GLOBAL_DEFAULT); // this needs to be initialized once- will allow code below to use libcurl
	/*****************************************************************/
	string urlBase = "http://nsi.rosminzdrav.ru/port/rest/searchDictionary?userKey=";
	string urlString = "";
	string tokenString = "";
	string pages[] = { "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15" }; // костыль, убрать
	string pageNumber = pages[1];
	string fileName = "";

	/***EXTRACT USER TOKEN FROM FILE***/
	fstream extractToken;
	extractToken.open("all_data\\configuration_and_logs\\user_token.txt", ios::in);
	if (!extractToken)
	{
		cout << " Cannot open token file...";
		Sleep(10000);
		cout << endl << endl;
		system("pause");
		return 0;
	}
	getline(extractToken, tokenString);
	extractToken.close();
	/***EXTRACT USER TOKEN FROM FILE***/
	string kok;
	curl = curl_easy_init();
	if (curl)
	{
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L); // SKIP_PEER_VERIFICATION
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L); // SKIP_HOSTNAME_VERIFICATION
		curl_easy_setopt(curl, CURLOPT_HTTPPROXYTUNNEL, 1L);
		curl_easy_setopt(curl, CURLOPT_PROXY, "10.14.10.147:8080");
		//curl_easy_setopt(curl, CURLOPT_PROXYTYPE, CURLPROXY_HTTP); //traffic inspector http ftp 2.0.1.721 
		curl_easy_setopt(curl, CURLOPT_PROXYAUTH, CURLAUTH_ANY);
		//curl_easy_setopt(curl, CURLOPT_PROXYUSERPWD, "dima:123");

		/*****************************************************************/
		bool firstResponse = true; // в первом полученном от сервера сообщении найдем количество строк в списке справочников - около 1200 на июль 2020
		// сделать полностью в цикле
		string content;
		urlString = urlBase + tokenString + "&page=" + pageNumber + "&size=200"; // размер выдачи больше 400 иногда не работает и сервер не хочет его обрабатывать

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
		kok = content.c_str();
		//cout << "1-------------------------------------\n" << content.c_str() << "1-------------------------------------" << endl;
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

	system("pause");
	curl_global_cleanup(); // when program doesn't use libcurl anymore in the following code
	return 0;
}