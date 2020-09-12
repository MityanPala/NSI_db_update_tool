#include <iostream>
#include <fstream>
#include <string>
#include "curl.h"
#include <Windows.h>
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

void deleteLawsTag(string* contentString, string tagToFind, char separatorBetweenTags); // kostyl

int main() // по порядку возвращаем значения сверху вниз с увеличением на 1 - если дошли до самого конца - там получим 0
{
	CURL* curl;
	CURLcode res;

	//1.2.643.5.1.13.13.99.2.444 - тут тип справочника(классификатор и т.д.) равен null в списке справочников, однако я не знаю, чему равен при получении passport

	int counterOfAllDocs = 0;

	int modifiedFiles = 0; // число всех измененных
	int suc_modifiedFiles = 0; // число успешно измененных

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

	string archiveDownload = "&showArchive=";

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

	/*extract archive download information*/
	fstream archive;
	archive.open("all_data\\configuration_and_logs\\dl_settings.txt", ios::in);
	if (!archive)
	{
		writeLogs << "Cannot extract download settings from \"all_data\\configuration_and_logs\\dl_settings.txt\"" << endl;
		writeLogs.close();
		return 99;
	}
	string myArch = "";
	getline(archive, myArch);
	getline(archive, myArch);
	archive.close();
	archiveDownload += myArch;
	if ((archiveDownload.find("false") == -1) && (archiveDownload.find("true") == -1))
	{
		archiveDownload = "";
	}
	/*extract archive download information*/

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

		urlString = urlBase + tokenString + "&page=" + pageNumber + "&size=200" + archiveDownload + "&sorting=fullName&sortingDirection=DESC"; // размер выдачи больше 400 иногда не работает и сервер не хочет его обрабатывать

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
			curl_easy_setopt(curl, CURLOPT_TIMEOUT, 25L);
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
		stack <string> fullNamesList;
		stack <string> versionsListCopy;
		stack <string> oidsListCopy;

		deleteLawsTag(&content, "\"laws\":[", ']');

		extractTags(content, "\"oid\":\"", '\"', &oidsList);
		extractTags(content, "\"version\":\"", '\"', &versionsList);
		extractTags(content, "\"fullName\":\"", '\"', &fullNamesList);

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

		unsigned int maxPages = convertedStringToInt / 200;
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
				writeLogs << "Something went wrong during the page number conversion. Page number: \"" << j << "\"" << fileName << endl;
				writeLogs.close();
				return 200;
			}
			pageNumber = pageNumberToStr;

			urlString = urlBase + tokenString + "&page=" + pageNumber + "&size=200" + archiveDownload + "&sorting=fullName&sortingDirection=DESC";
			content = "";

			res = basicRequest(urlString, curl, &content);
			if (res != CURLE_OK)
			{
				writeLogs << "Something went wrong during the dictionary list request. Page number: \"" << j << "\"" << fileName << endl;
				writeLogs.close();
				return 201;
			}

			deleteLawsTag(&content, "\"laws\":[", ']');

			extractTags(content, "\"oid\":\"", '\"', &oidsList);
			extractTags(content, "\"version\":\"", '\"', &versionsList);
			extractTags(content, "\"fullName\":\"", '\"', &fullNamesList);
		}
		// ended receiving dictionaries lists

		if (oidsList.size() != versionsList.size())
		{
			writeLogs << "Sizes of oids list and versions list are not equal..." << endl;
			writeLogs.close();
			return 204;
		}

		// lets start to make a file with all lists of oids
		oidsListCopy = oidsList;
		versionsListCopy = versionsList;

		if (oidsListCopy.size() != fullNamesList.size())
		{
			writeLogs << "Sizes of oids list copy and fullNamesList are not equal..." << endl;
			writeLogs.close();
			return 20004;
		}

		string pathToList = "all_data\\updated_data\\documents_lists\\list_of_all_dictionaries.txt";

		fstream listofalldictionaries;
		listofalldictionaries.open(pathToList.c_str(), ios::out);
		pathToList = "all_data\\updated_data\\documents_lists\\justchecking.txt";

		if (!listofalldictionaries)
		{
			writeLogs << "Something went wrong while trying to save list of all dictionaries into file. Wanted destination: " << pathToList << endl;
			writeLogs.close();
			return 20003;
		}
		listofalldictionaries << "{\"total\":" << counterOfAllDocs << ",\"list\":[";

		while (oidsListCopy.size() > 0)
		{
			listofalldictionaries << "{\"oid\":\"" << oidsListCopy.top() << "\",";
			listofalldictionaries << "\"version\":\"" << versionsListCopy.top() << "\",";
			listofalldictionaries << "\"fullName\":\"" << fullNamesList.top() << "\"}";

			oidsListCopy.pop();
			versionsListCopy.pop();
			fullNamesList.pop();

			if (oidsListCopy.size() != 0)
			{
				listofalldictionaries << ",";
			}
		}
		listofalldictionaries << "]}";
		listofalldictionaries.close();
		// lets finish making a file with all lists of oids

		system("cls");

		string urlBasePass = "http://nsi.rosminzdrav.ru/port/rest/passport?userKey=" + tokenString + "&identifier=";
		string urlBaseData = "http://nsi.rosminzdrav.ru/port/rest/data?userKey=" + tokenString + "&identifier=";
		string urlPassString = "";
		string urlDataString = "";
		string topOID = "";
		string topVer = "";

		fstream docData;
		string savingBuffer;
		int dirFlag = 0;
		string getVersion = "";
		int counterOfUpToDate = 0;

		string getVersionFromDataFile; // kostily
		stack <string> bufferStack;

		// начнем получать информацию о самих справочниках...
		while (oidsList.size() > 0)
		{
			system("cls");
			cout << "Number of all files -------------------- " << counterOfAllDocs << endl;
			cout << "Up to date files ----------------------- " << counterOfUpToDate << endl;
			cout << "Successfully modified / all modified --- " << suc_modifiedFiles << "/" << modifiedFiles << endl << endl;

			content = "";
			topOID = oidsList.top();
			oidsList.pop();
			topVer = versionsList.top();
			versionsList.pop();

			// check if version is the same as we have in stack, if file exists
			savingBuffer = "all_data\\updated_data\\documents\\" + topOID + ".json"; // загружаем данные в файл с расширением json, txt - это паспорт
			docData.open(savingBuffer.c_str(), ios::in);
			if (docData)
			{
				docData >> getVersionFromDataFile;
				docData.close();
				extractTags(getVersionFromDataFile, "\"version\":\"", '\"', &bufferStack);
				if (bufferStack.size() != 0)
				{
					if (topVer == bufferStack.top()) // if the stack is not empty - we found version without mistakes
					{
						counterOfUpToDate++;
						cout << counterOfUpToDate << " out of " << counterOfAllDocs << " is up to date" << endl;
						continue;
					}
				}
			}
			// check if version is the same as we have in stack, if file exists ^

			modifiedFiles++; // если у нас не совпадают версии или ее нет - значит, мы затрагиваем директорию и следовательно будем проводить изменения ниже			

			// сохранили структуру
			urlPassString = urlBasePass + topOID + "&version=" + topVer; // pass url
			res = basicRequest(urlPassString, curl, &content);

			if (res != CURLE_OK)
			{
				writeLogs << "Something went wrong during the \"/passport\" request. Identifier: \"" << topOID << "\"" << endl;
				continue;
			}

			fileName = "all_data\\updated_data\\documents\\" + topOID + "_temp.txt";
			docData.open(fileName.c_str(), ios::out | ios::trunc);
			if (!docData)
			{
				writeLogs << "Something went wrong during the attempt to save \"/passport\" into file. Wanted destination: " << fileName << endl;
				continue;
			}
			docData << content;
			docData.close();

			//Sleep(2500);
			fileName = "del all_data\\updated_data\\documents\\" + topOID + ".txt";
			system(fileName.c_str());
			//Sleep(2500);
			fileName = "ren all_data\\updated_data\\documents\\" + topOID + "_temp.txt " + topOID + ".txt";
			system(fileName.c_str());
			// сохранили структуру ^

			//-----------------------------------------------------------------------------------------------
			// загрузка данных справочника
			/*STARTED TRYING TO RECIEVE TOTAL NUMBER OF DICTIONARIES*/
			int numPosN = content.find("\"rowsCount\":"); // "total": "rowsCount":
			int numLenN = content.length();

			if (!(numPosN >= 0 && numPosN < numLenN))
			{
				writeLogs << "Something went wrong while trying to get total number of rows: " << topOID << "|" << topVer << endl;
				continue;
			}

			int separatorIndexN = numPosN + 12; // длина подстроки "total":
			while ((separatorIndexN < numLenN) && (content[separatorIndexN] != ','))
			{
				separatorIndexN++;
			}

			int convertedStringToIntN = atoi(content.substr(numPosN + 12, separatorIndexN - (numPosN + 12)).c_str());
			if (!convertedStringToIntN)
			{
				writeLogs << "Something went wrong while trying to perform conversion of total number of rows to int: " << topOID << endl;
				continue;
			}

			unsigned int maxPagesN = convertedStringToIntN / 200;
			if ((((double)convertedStringToIntN) / 200.0) > ((double)maxPagesN))
			{
				maxPagesN++; // если к примеру в выдаче 1201 справочник, то мы получим 6 maxPages, но если разделить не нацело, то мы увидим, что надо прибавить 1 к кол-ву страниц
			}
			/*STOPPED TRYING TO RECIEVE TOTAL NUMBER OF DICTIONARIES*/

			char pageNumberToStrN[100];
			// открываем на запись общий файл, раньше открывался 1 файл на каждый запрос
			//fileName = "all_data\\updated_data\\documents\\" + topOID + "\\data\\" + topOID + "_page_" + pageNumber + ".txt";
			fileName = "all_data\\updated_data\\documents\\" + topOID + "_temp.json";

			docData.open(fileName.c_str(), ios::out | ios::trunc);
			if (!docData)
			{
				writeLogs << "Something went wrong while trying to open the whole file to save data into it - OID:" << topOID << endl;
				continue;
			}
			int successfullyDownloadedPages = 0;
			// открываем на запись общий файл, раньше открывался 1 файл на каждый запрос
			for (unsigned int j = 1; j <= maxPagesN; j++)
			{
				successfullyDownloadedPages++;
				static int timestart = 0;
				static int timeleft = 0;

				if (j == 1)
				{
					timestart = clock();
				}
				/*if (j == 25)
				{
					timeleft = clock();
					timeleft = ((timeleft - timestart) / CLOCKS_PER_SEC);
				}*/
				if (j % 25 == 0)
				{
					timeleft = clock();
					timeleft = ((timeleft - timestart) / CLOCKS_PER_SEC);

					system("cls");
					cout << "Number of all files -------------------- " << counterOfAllDocs << endl;
					cout << "Up to date files ----------------------- " << counterOfUpToDate << endl;
					cout << "Successfully modified / all modified --- " << suc_modifiedFiles << "/" << modifiedFiles << endl << endl;

					cout << topOID << ": " << j << " pages already loaded out of " << maxPagesN << ";" << endl;
					timestart = (int)((timeleft * (maxPagesN - j)) / 25);
					cout << "Estimated time left to finish downloading this dictionary: " << timestart / 60 << " minutes " << timestart % 60 << " seconds." << endl;
					timestart = clock();
				}

				if (_itoa_s(j, pageNumberToStrN, _countof(pageNumberToStrN), 10))
				{
					writeLogs << "Something went wrong during the page number conversion. Page number: \"" << j << "\" " << topOID << endl;
					successfullyDownloadedPages--;
					continue;
				}
				pageNumber = pageNumberToStrN;

				urlDataString = urlBaseData + topOID + "&version=" + topVer + "&page=" + pageNumber + "&size=200";
				content = "";

				int tries = 0;
				while (tries < 5)
				{
					res = basicRequest(urlDataString, curl, &content);

					if (res == CURLE_OK)
					{
						tries = -1;
						break;
					}

				}
				if (tries != -1)
				{
					writeLogs << "Something went wrong during the dictionary data request. Page number: \"" << j << "\" " << topOID << endl;
					successfullyDownloadedPages--;
					continue;
				}
				// раньше открытие файла для каждого запроса было тут
				tries = 0;
				if (j == 1)
				{
					string bufferToDeleteVer = topVer;
					unsigned int iterator = 0;
					while (iterator < bufferToDeleteVer.length())
					{
						bufferToDeleteVer[iterator++] = '*';
					}
					docData << "{\"version\":\"" << bufferToDeleteVer << "\"";
					/**/
					//docData << "{\"version\":\"" << topVer << "\"";
					static int somethingInteresting = 0;
					somethingInteresting = content.find_first_of('{');
					if (somethingInteresting != -1) // string::npos
					{
						content[somethingInteresting] = ',';
					}
					if (j != maxPagesN)
					{
						content.replace(content.find_last_of(']'), 2, "");
					}
					docData << content;
					continue;
				}

				//cout << "hello" << endl;

				content.replace(0, content.find_first_of('[') + 1, ",");
				if (j != maxPagesN)
				{
					content.replace(content.find_last_of(']'), 2, "");
					docData << content;
					continue;
				}
				docData << content;
				// подсчитать количество ошибок при загрузке страниц - если есть хоть одна - удалить файл полностью, так как с таким хардкодингом файл json будет испорчен и не прочитается нормально

				// блок обработки крайнего значения диапазона
			}

			if (successfullyDownloadedPages == maxPagesN)
			{
				docData.seekg(ios::beg); // если удалось записать файл полностью правильно - убираем затирание звездочками в начале записи файла
				docData << "{\"version\":\"" << topVer << "\"";
			}

			docData.close();

			//Sleep(2500);
			fileName = "del all_data\\updated_data\\documents\\" + topOID + ".json";
			system(fileName.c_str());
			//Sleep(2500);
			fileName = "ren all_data\\updated_data\\documents\\" + topOID + "_temp.json " + topOID + ".json";
			system(fileName.c_str());


			// загрузка данных справочника
			//-----------------------------------------------------------------------------------------------

			suc_modifiedFiles++; // если успешно заново загрузили все данные - доходим до этой строки и увеличиваем счетчик
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

	writeLogs << "Program has updated database. \nNumber of modified dictionaries: " << modifiedFiles << " out of " << counterOfAllDocs << endl;
	writeLogs << "Successfully modified dictionaries: " << suc_modifiedFiles << " out of all modified dictionaries: " << modifiedFiles << endl;
	writeLogs << "|  end of the process: " << currentDate << "|" << endl << separator << endl << endl;
	writeLogs.close();

	Sleep(10000);
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

void extractTags(string contentString, string tagToFind, char separatorBetweenTags, stack <string>* stackOfStrings)
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

		do {
			while ((separatorIndex < numLen) && (contentString[separatorIndex] != separatorBetweenTags))
			{
				separatorIndex++;
			}
			if ((contentString[separatorIndex] == separatorBetweenTags) && (contentString[separatorIndex - 1] == '\\'))
			{
				separatorIndex++;
			}
			else
			{
				break;
			}
		} while (true);
		

		stackOfStrings->push(contentString.substr(numPos + tagToFindLength, separatorIndex - (numPos + tagToFindLength)).c_str());
	}

	return;
}

void deleteLawsTag(string* contentString, string tagToFind, char separatorBetweenTags)
{
	int numLen = contentString->length();
	int numPos;
	int tagToFindLength = tagToFind.length();
	int separatorIndex;
	char eraserBase = '*';

	while (true)
	{
		numPos = contentString->find(tagToFind);

		if (!(numPos >= 0 && numPos < numLen))
		{
			return;
		}
		

		//contentString[numPos] = replaceTagWithChar; // если заменить oid на -id, то искаться подстрока oid не будет - этого достаточно, а время работы будет меньше(немного)

		separatorIndex = numPos + tagToFindLength; // длина подстроки tagToFind

		do {
			while ((separatorIndex < numLen) && ((*contentString)[separatorIndex] != separatorBetweenTags))
			{
				separatorIndex++;
			}
			if (((*contentString)[separatorIndex] == separatorBetweenTags) && ((*contentString)[separatorIndex] == '\\'))
			{
				separatorIndex++;
			}
			else
			{
				for (int deleting = numPos; deleting <= separatorIndex; deleting++)
				{
					(*contentString)[deleting] = eraserBase;
				}
				break;
			}
		} while (true);

		
	}

	return;
}