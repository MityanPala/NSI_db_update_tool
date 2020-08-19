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
void extractTags(string contentString, string tagToFind, char separatorBetweenTags, stack <string>* stackOfStrings);

int main() // по порядку возвращаем значения сверху вниз с увеличением на 1 - если дошли до самого конца - там получим 0
{
	CURL* curl;
	CURLcode res;
	int counter = 0; // number of successfully updated data files
	int counterOfAllDocs = 0;
	int versionMismatchCounter = 0;
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
	writeLogs << separator << endl << "|start of the process: " << currentDate << "|" << endl;

	string urlBase = "http://nsi.rosminzdrav.ru/port/rest/searchDictionary?userKey=";
	
	string pageNumber = "1";

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
	if ((proxyString == "" || proxyUsrPwd == "") && proxyString != "noproxy") // нам вообще может быть не нужен прокси
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

		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L); // 5 sec timeout
		
		urlString = urlBase + tokenString + "&page=" + pageNumber + "&size=200"; // размер выдачи больше 400 иногда не работает и сервер не хочет его обрабатывать

		res = basicRequest(urlString, curl, &content);
		if (res != CURLE_OK)
		{
			writeLogs << "Request \"" << urlString << "\" failed. Cannot get response without proxy. \nCURLcode error: \"" << curl_easy_strerror(res) << "\"" << endl;

			if (proxyString == "noproxy")
			{
				writeLogs << "Noproxy mode has been chosen - exiting the program..." << endl;
				writeLogs.close();
				return 1035;
			}

			curl_easy_setopt(curl, CURLOPT_HTTPPROXYTUNNEL, 1L);
			curl_easy_setopt(curl, CURLOPT_PROXYAUTH, CURLAUTH_BASIC);
			curl_easy_setopt(curl, CURLOPT_PROXY, proxyString.c_str());
			curl_easy_setopt(curl, CURLOPT_PROXYUSERPWD, proxyUsrPwd.c_str());
			curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
			curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

			res = basicRequest(urlString, curl, &content);
			if (res != CURLE_OK)
			{
				writeLogs << "Request \"" << urlString << "\" failed. Cannot get response using proxy. \nCURLcode error: \"" << curl_easy_strerror(res) << "\"" << endl;
				writeLogs.close();
				return 104;
			}
		}

		stack <string> oidsList; // помещаем в этот стек все oid
		stack <string> versionsList; // в этот же момент помещаем в стек все версии - до начала загрузки данных надо проверить размеры стеков, чтобы были равны
		fstream documentsLists;
		fileName = "all_data\\updated_data\\documents_lists\\" + pageNumber + "_page.txt";

		//documentsListsNames.push(fileName);
		
		documentsLists.open(fileName.c_str(), ios::out);
		if (!documentsLists)
		{
			writeLogs << "Something went wrong while trying to save 1st list into file. Wanted destination: " << fileName << endl;
			writeLogs.close();
			return 105;
		}
		documentsLists << content;
		documentsLists.close();

		extractTags(content, "\"oid\":\"", '\"', &oidsList);
		extractTags(content, "\"version\":\"", '\"', &versionsList);
		
		/*STARTED TRYING TO RECIEVE TOTAL NUMBER OF DICTIONARIES*/
		int numPos = content.find("\"total\":");
		int numLen = content.length();
		if (!(numPos >= 0 && numPos < numLen))
		{
			writeLogs << "Something went wrong while trying to get total number of documents." << fileName << endl;
			writeLogs.close();
			return 106;
		}

		int separatorIndex = numPos + 8; // длина подстроки "total":
		while ((separatorIndex < numLen) && (content[separatorIndex] != ','))
		{
			separatorIndex++;
		}

		int convertedStringToInt = atoi(content.substr(numPos + 8, separatorIndex - (numPos + 8)).c_str());
		if (!convertedStringToInt)
		{
			writeLogs << "Something went wrong during the conversion of total number of documents." << fileName << endl;
			writeLogs.close();
			return 107;
		}

		counterOfAllDocs = convertedStringToInt; // подкостыливаем по-тихоньку

		int maxPages = convertedStringToInt / 200;
		if ((((double)convertedStringToInt) / 200.0) > ((double)maxPages))
		{
			maxPages++; // если к примеру в выдаче 1201 справочник, то мы получим 6 maxPages, но если разделить не нацело, то мы увидим, что надо прибавить 1 к кол-ву страниц
		}
		/*STOPPED TRYING TO RECIEVE TOTAL NUMBER OF DICTIONARIES*/

		char pageNumberToStr[100];
		
		for (unsigned int j = 2; j <= maxPages; j++)
		{
			if (_itoa_s(j, pageNumberToStr, _countof(pageNumberToStr), 10))
			{
				writeLogs << "Something went wrong during the page number conversion. Page number: \"" << j << "\"" <<fileName << endl;
				writeLogs.close();
				return 200;
			}
			pageNumber = pageNumberToStr;

			urlString = urlBase + tokenString + "&page=" + pageNumber + "&size=200";
			content = "";
			res = basicRequest(urlString, curl, &content);

			if (res != CURLE_OK)
			{
				writeLogs << "Something went wrong during the dictionary list request. Page number: \"" << j << "\"" << fileName << endl;
				writeLogs.close();
				return 201;
			}

			fileName = "all_data\\updated_data\\documents_lists\\" + pageNumber + "_page.txt";

			documentsLists.open(fileName.c_str(), ios::out);
			if (!documentsLists)
			{
				writeLogs << "Something went wrong while trying to save " << j << " list into file. Wanted destination: " << fileName << endl;
				writeLogs.close();
				return 203;
			}
			documentsLists << content;
			documentsLists.close();

			extractTags(content, "\"oid\":\"", '\"', &oidsList);
			extractTags(content, "\"version\":\"", '\"', &versionsList);
		}
		// ended receiving dictionaries lists
		
		if (oidsList.size() != versionsList.size())
		{
			writeLogs << "Sizes of oids list and versions list are not equal..." << endl;
			writeLogs.close();
			return 204;
		}

		string urlBasePass = "http://nsi.rosminzdrav.ru/port/rest/passport?userKey=" + tokenString + "&identifier=";
		string urlBaseData = "http://nsi.rosminzdrav.ru/port/rest/data?userKey=" + tokenString + "&identifier=";
		string urlPassString = "";
		string urlDataString = "";
		string topOID = "";
		string topVer = "";

		fstream docData;
		string savingBuffer;
		int dirFlag = 0;
		pageNumber = "1";
		string getVersion = "";

		// начнем получать информацию о самих справочниках...
		while (oidsList.size() > 0)
		{
			content = "";
			topOID = oidsList.top();
			oidsList.pop();
			topVer = versionsList.top();
			versionsList.pop();

			savingBuffer = "mkdir all_data\\updated_data\\documents\\" + topOID;
			dirFlag = system(savingBuffer.c_str()); // возвращает 0 при успешном создании -> если не 0, значит уже есть
			if (dirFlag)
			{
				// сюда мы попали, так как директория уже существует
				fileName = "all_data\\updated_data\\documents\\" + topOID + "\\current_version.txt";
				docData.open(fileName.c_str(), ios::in);
				if (!docData)
				{
					// если нет такого файла - значит нам надо удалить директории пасс и дата, а затем по-обычному загрузить все
					savingBuffer = "rmdir /S /Q all_data\\updated_data\\documents\\" + topOID;
					dirFlag = system(savingBuffer.c_str()); 
					if (dirFlag)
					{
						writeLogs << "Something went wrong during the attempt to modify directory: " << savingBuffer << endl;
						continue;
					}	
				}
				else
				{
					getline(docData, getVersion); // считали версию
					docData.close();
					if (getVersion == topVer)
					{
						continue;
					}
					else
					{
						savingBuffer = "rmdir /S /Q all_data\\updated_data\\documents\\" + topOID;
						dirFlag = system(savingBuffer.c_str());
						if (dirFlag)
						{
							writeLogs << "Something went wrong during the attempt to modify directory: " << savingBuffer << endl;
							continue;
						}
					}
				}
			}
			
			//sssssssssssssssssssssssssssss
			urlPassString = urlBasePass + topOID; // pass url
			res = basicRequest(urlPassString, curl, &content);

			if (res != CURLE_OK)
			{
				writeLogs << "Something went wrong during the \"/passport\" request. Identifier: \"" << topOID << "\"" << endl;
				continue;
			}

			savingBuffer = "mkdir all_data\\updated_data\\documents\\" + topOID + "\\passport";
			system(savingBuffer.c_str());
			savingBuffer = "mkdir all_data\\updated_data\\documents\\" + topOID + "\\data";
			system(savingBuffer.c_str());

			fileName = "all_data\\updated_data\\documents\\" + topOID + "\\current_version.txt";
			docData.open(fileName.c_str(), ios::out);
			if (!docData)
			{
				writeLogs << "Something went wrong during the attempt to save \"version\" into file. Wanted destination: " << fileName << endl;
				continue;
			}
			docData << topVer;
			docData.close();
			// сохранили версию

			fileName = "all_data\\updated_data\\documents\\" + topOID + "\\passport\\" + topOID + ".txt";
			docData.open(fileName.c_str(), ios::out);
			if (!docData)
			{
				writeLogs << "Something went wrong during the attempt to save \"/passport\" into file. Wanted destination: " << fileName << endl;
				continue;
			}
			docData << content;
			docData.close();
			// сохранили структуру

			// как сделать загрузку данных?

			counter++;
		}
		// конец получения информации о справочниках

		curl_easy_cleanup(curl);
	}
	else
	{
		writeLogs << "Cannot initialize CURL object - please restart the program - this may help." << endl;
		writeLogs.close();
		return 500;
	}

	curl_global_cleanup();

	// начало расчета времени
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
	// конец расчета времени

	writeLogs << "Program has updated database. \nNumber of successful updates: " << counter << " out of " << counterOfAllDocs << endl;
	writeLogs << "|  end of the process: " << currentDate << "|" << endl << separator << endl << endl;
	writeLogs.close();
	
	Sleep(20000);
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

string replaceBadCharacters(string strToFix, char charToReplace, char charToReplaceWith)
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

void extractTags(string contentString, string tagToFind, char separatorBetweenTags, stack <string> *stackOfStrings)
{
	int numLen = contentString.length();
	int numPos;
	int tagToFindLength = tagToFind.length();
	int separatorIndex;
	char replaceTagWithChar = '-';

	while (true)
	{
		numPos = contentString.find(tagToFind);
		
		if (!(numPos >= 0 && numPos < numLen))
		{
			return;
		}

		contentString[numPos] = replaceTagWithChar; // если заменить oid на -id, то искаться подстрока oid не будет - этого достаточно, а время работы будет меньше(немного)

		separatorIndex = numPos + tagToFindLength; // длина подстроки tagToFind

		while ((separatorIndex < numLen) && (contentString[separatorIndex] != separatorBetweenTags))
		{
			separatorIndex++;
		}

		stackOfStrings->push(contentString.substr(numPos + tagToFindLength, separatorIndex - (numPos + tagToFindLength)).c_str());
	}

	return;
}