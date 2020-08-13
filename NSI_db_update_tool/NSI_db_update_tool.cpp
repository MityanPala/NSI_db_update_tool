#include <iostream>
#include <fstream>
#include <string>
#include "curl.h"

#include <stack>

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
string replaceBadCharacters(string strToFix, char charToReplace, char charToReplaceWith);

int main() // по порядку возвращаем значения сверху вниз с увеличением на 1 - если дошли до самого конца - там получим 0
{
	CURL* curl;
	CURLcode res;
	curl_global_init(CURL_GLOBAL_DEFAULT); // this needs to be initialized once - will allow code below to use libcurl

	//ShowWindow(GetConsoleWindow(), SW_HIDE); // пока не отладил программу до конца - можно выводить прогресс загрузки файлов с помощью cout
	system("mkdir all_data\n");
	system("mkdir all_data\\configuration_and_logs\n");
	system("mkdir all_data\\configuration_and_logs\\logs\n");
	system("mkdir all_data\\updated_data\n");
	system("mkdir all_data\\updated_data\\documents_lists\n");
	system("mkdir all_data\\updated_data\\documents\n");

	string separator = "//--------------------------------------------------------------------------";
	string logsName = "all_data\\configuration_and_logs\\logs\\overall_app_logs.txt";
	string currentDate = "cannot_recieve_date";

	struct tm newtime;
	__time32_t aclock;
	char buffer[32];
	errno_t errNum;
	_time32(&aclock);
	_localtime32_s(&newtime, &aclock);
	errNum = asctime_s(buffer, 32, &newtime);
	if (!errNum)
	{
		currentDate = buffer;
		int enterPos = currentDate.find('\n');
		int enterLen = currentDate.length();
		if (enterPos >= 0 && enterPos < enterLen)
		{
			currentDate.erase(enterPos, enterLen);
		}
		else
		{
			currentDate = "cannot_recieve_date";
		}
	}

	fstream writeLogs;
	writeLogs.open(logsName.c_str(), ios::app);
	if (!writeLogs)
	{
		logsName = "all_data\\configuration_and_logs\\logs\\";
		currentDate = replaceBadCharacters(currentDate, ':', '-');
		currentDate = replaceBadCharacters(currentDate, ' ', '_');
		logsName += currentDate; // local time
		logsName += "_app_logs.txt";
		writeLogs.open(logsName.c_str(), ios::out); // чтобы проверить, существует файл или нет - надо сначала ios::in, потом открыть его на запись - но надо ли?
		if (!writeLogs)
		{
			return 100; // если не можем создать файл, то либо не хватает прав, либо такой файл уже существует
		}
	}
	writeLogs << endl << separator << endl << "|start of process: " << currentDate << "|" << endl;

	string urlBase = "http://nsi.rosminzdrav.ru/port/rest/searchDictionary?userKey=";
	
	string pages[] = { "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15" }; // костыль, убрать
	string pageNumber = pages[1];

	string tokenString = "";
	string proxyString = "";
	string proxyUsrPwd = "";
	string fileName = "";

	/***EXTRACT USAGE INFO FROM FILE***/
	fstream extractToken, extractProxyInfo;
	extractToken.open("all_data\\configuration_and_logs\\user_token.txt", ios::in);
	if (!extractToken)
	{
		writeLogs << "Cannot extract user token from \"all_data\\configuration_and_logs\\user_token.txt\"" << endl;
		writeLogs.close();
		return 101;
	}
	getline(extractToken, tokenString);
	extractToken.close();

	extractProxyInfo.open("all_data\\configuration_and_logs\\proxy_info.txt", ios::in);
	if (!extractProxyInfo)
	{
		writeLogs << "Cannot extract proxy info from \"all_data\\configuration_and_logs\\proxy_info.txt\"" << endl;
		writeLogs.close();
		return 102;
	}
	getline(extractProxyInfo, proxyString);
	getline(extractProxyInfo, proxyUsrPwd);
	extractProxyInfo.close();
	if (proxyString == "" || proxyUsrPwd == "") // нам вообще может быть не нужен прокси - тогда надо будет сделать этот return ниже, на этапе, когда не получится сделать запрос без прокси
	{
		writeLogs << "Cannot properly extract proxy or password from \"all_data\\configuration_and_logs\\proxy_info.txt\"" << endl;
		writeLogs << "Please, check file structure(1st line: proxy url - \"http://xxx.xxx.xxx.xxx:xxxx\"; 2nd line - \"usr:pwd\")" << endl;
		writeLogs.close();
		return 103;
	}
	/***EXTRACT USAGE INFO FROM FILE***/

	curl = curl_easy_init();
	if (curl)
	{
		string content = "";
		string urlString = "";

		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L); // SKIP_PEER_VERIFICATION
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L); // SKIP_HOSTNAME_VERIFICATION
		
		urlString = urlBase + tokenString + "&page=" + pageNumber + "&size=200"; // размер выдачи больше 400 иногда не работает и сервер не хочет его обрабатывать

		res = basicRequest(urlString, curl, &content);
		if (res != CURLE_OK)
		{
			writeLogs << "Request \"" << urlString << "\" failed. Cannot get response without proxy. CURLcode error: \"" << curl_easy_strerror(res) << "\"" << endl;

			curl_easy_setopt(curl, CURLOPT_HTTPPROXYTUNNEL, 1L);
			curl_easy_setopt(curl, CURLOPT_PROXYAUTH, CURLAUTH_BASIC);
			curl_easy_setopt(curl, CURLOPT_PROXY, proxyString.c_str());
			curl_easy_setopt(curl, CURLOPT_PROXYUSERPWD, proxyUsrPwd.c_str());
			curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
			curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

			res = basicRequest(urlString, curl, &content);
			if (res != CURLE_OK)
			{
				writeLogs << "Request \"" << urlString << "\" failed. Cannot get response using proxy. CURLcode error: \"" << curl_easy_strerror(res) << "\"" << endl;
				writeLogs.close();
				return 104;
			}
		}

		stack <string> documentsListsNames; // запушим в стек имена файлов, в которые мы помещали списки, чтобы когда получим все списки - уже извлекать их и открывать заново

		fstream documentsLists;
		fileName = "all_data\\updated_data\\documents_lists\\" + pageNumber + "_page.txt";

		documentsListsNames.push(fileName);
		
		documentsLists.open(fileName.c_str(), ios::out);
		if (!documentsLists)
		{
			writeLogs << "Something went wrong while trying to save 1st list into file. Wanted destination: " << fileName << endl;
			writeLogs.close();
			return 105;
		}
		documentsLists << content;
		documentsLists.close();

		int numPos = content.find("\"total\":");
		int numLen = content.length();
		if (!(numPos >= 0 && numPos < numLen))
		{
			writeLogs << "Something went wrong while trying to get total number of documents." << fileName << endl;
			writeLogs.close();
			return 106;
		}

		fstream F;
		string substrInContent = content.substr(numPos + 8, 15); // переделать, чтобы получать кол-во страниц выделяя тэг в json полностью по кавычкам и/или запятым-разделителям
		//cout << endl << substrInContent << endl;
		numPos = substrInContent.find(",");
		substrInContent = substrInContent.substr(0, numPos); // 8 - length of "total:"
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

		// оставшийся цикл для получения списков - дальше буду уже извлекать из этого списка файлов oid и загружать сами документы
		for (int j = 2; j <= maxPages; j++)
		{
			pageNumber = pages[j];
			//pageNumber = 1;
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
		return 500;
	}

	curl_global_cleanup();
	writeLogs << "Program has updated database. \nNumber of successful updates: " << "uuu/xxx" << endl;
	writeLogs << "Number of failed updates: " << "yyy/xxx" << endl;
	writeLogs << "|end of process: " << currentDate << "|" << endl << separator << endl;
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

string replaceBadCharacters(string strToFix, const char charToReplace, const char charToReplaceWith)
{
	for (unsigned int i = 0; i < strToFix.length(); i++)
	{
		if (strToFix[i] != charToReplace)
		{
			continue;
		}
		else
		{
			strToFix[i] = charToReplaceWith;
		}
	}

	return strToFix;
}