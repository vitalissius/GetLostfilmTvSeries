#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <list>
#include <memory>
#include <regex>
#include <tuple>
#include <set>
#include <functional>
#include <codecvt>

#include <io.h>                           // _O_U8TEXT
#include <fcntl.h>                        // _setmode

#include <boost/asio.hpp>

#ifdef _MSC_VER
  std::locale locR("rus_rus.1251");       // ukr_ukr.1251
#else
  std::locale locR("ru_RU");              // uk_UA
#endif // !_MSC_VER

std::locale locRUtf8 = std::locale(std::locale::empty(), new std::codecvt_utf8<wchar_t>());

std::shared_ptr<std::stringstream> downloadFunction(const std::string& host, const std::string& pathWithQuery)
{
  try
  {
    boost::asio::io_service inputOutputService;

    // Get a list of endpoints corresponding to the server name:
    boost::asio::ip::tcp::resolver ipResolver(inputOutputService);
    boost::asio::ip::tcp::resolver::query queryTo(host, "http");
    boost::asio::ip::tcp::resolver::iterator endpointIter = ipResolver.resolve(queryTo);
    boost::asio::ip::tcp::resolver::iterator endIter;

    // Try each endpoint until we successfully establish a connection.
    boost::asio::ip::tcp::socket socketToInputOutput(inputOutputService);
    boost::system::error_code errorCode = boost::asio::error::host_not_found;
    while (errorCode && endpointIter != endIter)
    {
      socketToInputOutput.close();
      socketToInputOutput.connect(*endpointIter++, errorCode);
    }
    if (errorCode)
    {
      throw boost::system::system_error(errorCode);
    }

    // From the request. We specify the "Connection: close" header so that the server will close the socket after transmitting the response.
    // This will allow us to treat all data up until the EOF as the content.
    boost::asio::streambuf requestLine;
    std::ostream requestOutputStream(&requestLine);
    requestOutputStream << "GET " << pathWithQuery << " HTTP/1.0\r\n";
    requestOutputStream << "Host: " << host << "\r\n";
    requestOutputStream << "Accept: */*\r\n";
    requestOutputStream << "Connection: close\r\n\r\n";

    // Send a request:
    boost::asio::write(socketToInputOutput, requestLine);

    // Read the response status line:
    boost::asio::streambuf responseLine;
    boost::asio::read_until(socketToInputOutput, responseLine, "\r\n");

    // Check that response is OK:
    std::istream responseInputStream(&responseLine);
    std::string httpVersion;
    responseInputStream >> httpVersion;
    unsigned int statusCode;
    responseInputStream >> statusCode;
    std::string statusMessage;
    std::getline(responseInputStream, statusMessage);
    if (!responseInputStream || httpVersion.substr(0, 5) != "HTTP/")
    {
      std::cout << "Invalid response!\n";
      return nullptr;
    }
    if (statusCode != 200)
    {
      std::cout << "Response returned with status code: " << statusCode << "\n";
      return nullptr;
    }

    // Read the response headers, which are terminated by a blank line^
    boost::asio::read_until(socketToInputOutput, responseLine, "\r\n\r\n");

    // Process the response headers:
    std::string headerString;
    while (std::getline(responseInputStream, headerString) && headerString != "\r");  // <-- Ignore header


    std::stringstream xmlDoc;
    // Write whatever content we already have to output:
    if (responseLine.size() > 0)
    {
      xmlDoc << &responseLine;
    }
    // Read until EOF, writing data to output as we go:
    while (boost::asio::read(socketToInputOutput, responseLine, boost::asio::transfer_at_least(1), errorCode))
    {
      xmlDoc << &responseLine;
    }
    if (errorCode != boost::asio::error::eof)
    {
      throw boost::system::system_error(errorCode);
    }

    std::shared_ptr<std::stringstream> returnedStringStream = std::make_shared<std::stringstream>();
    returnedStringStream->swap(xmlDoc);

    returnedStringStream->imbue(locR);
    std::function<std::shared_ptr<std::stringstream>(std::shared_ptr<std::stringstream>)> cp1251ToUtf8 = [](std::shared_ptr<std::stringstream> ss)
    {
      std::string narrowStr = ss->str();
      std::wstring wideStr(narrowStr.length(), 0);
      std::use_facet<std::ctype<wchar_t>>(std::locale(locR)).widen(&narrowStr[0], &narrowStr[0] + narrowStr.length(), &wideStr[0]);
      std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> cv;
      std::string strUtf8 = cv.to_bytes(wideStr);
      std::stringstream ssUtf8;
      ssUtf8.imbue(locRUtf8);
      ssUtf8 << strUtf8;
      std::shared_ptr<std::stringstream> pSsUtf8 = std::make_shared<std::stringstream>();
      pSsUtf8->swap(ssUtf8);
      return pSsUtf8;
    };
    return cp1251ToUtf8(returnedStringStream);
  }
  catch (std::exception& e)
  {
    std::cout << e.what() << std::endl;
  }
  return std::make_shared<std::stringstream>();
}



struct Row
{
  std::string _path;
  std::string _engName;
  std::string _locName;
  std::string _country;
  std::string _releaseYear;
  std::string _genre;
  std::string _seasonsAmount;
  std::string _status;

  Row::Row(const std::string& path,
           const std::string& locName,
           const std::string& engName,
           const std::string& country,
           const std::string& releaseYear,
           const std::string& genre,
           const std::string& seasonsAmount,
           const std::string& status) 
           :
           _path(path),
           _locName(locName),
           _engName(engName),           
           _country(country),
           _releaseYear(releaseYear),
           _genre(genre),
           _seasonsAmount(seasonsAmount),
           _status(status)
  {}

  friend std::ostream& operator<<(std::ostream& o, const Row& r)
  {
    o << "path: " << r._path
      << "\nloc: " << r._locName
      << "\neng: " << r._engName
      << "\ncountry: " << r._country
      << "\nyear: " << r._releaseYear
      << "\ngenre: " << r._genre
      << "\namount: " << r._seasonsAmount
      << "\nstatus: " << r._status
      << "\n";
    return o;
  }
};

std::string& trim(std::string& str)
{
  while (std::isspace(str.front(), locRUtf8)) str.erase(str.begin());
  while (std::isspace(str.back(), locRUtf8)) str.erase(--str.end());
  return str;
}

std::string& changeAmpersand(std::string& str)
{
  std::string::size_type pos = str.find("&");
  while (pos != std::string::npos)
  {
    str.insert(pos + 1, "amp;");
    pos = str.find("&", pos + 4);
  }
  return str;
}

std::unique_ptr<std::vector<std::string>> tokenizeString(const std::string& str, bool toUpperFirstLetter = false)
{
  std::unique_ptr<std::vector<std::string>> pvs = std::make_unique<std::vector<std::string>>();

  char* cstr = new char[str.size() + 1];
  strcpy_s(cstr, str.size() + 1, str.data());
  char* token = nullptr;
  char* next_token = nullptr;
  char seps[] = ",/.";
  token = strtok_s(cstr, seps, &next_token);
  while (token != nullptr)
  {
    if (token != nullptr)
    {
      std::string strToken = token;
      if (toUpperFirstLetter)
      {
        std::wstring wstrTmp = std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(trim(strToken));
        std::use_facet<std::ctype<wchar_t>>(locRUtf8).toupper(&wstrTmp[0], &wstrTmp[0] + 1);
        pvs->push_back(std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(wstrTmp));
      }
      else
      {
        pvs->push_back(trim(strToken));
      }
      token = strtok_s(nullptr, seps, &next_token);
    }
  }
  delete[] cstr;

  return pvs;
}

struct GenresAndCountries {
  std::set<std::string> _genres;
  std::set<std::string> _countries;
};

void makeXmlFullData(const std::list<Row>& listRows, const std::shared_ptr<GenresAndCountries> gacIfNecessary = nullptr)
{
  std::ofstream out("tvseries.xml", std::ios::out | std::ios::trunc);
  if (out.is_open())
  {
    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n\n";
    out << "<tvseries>\n";
    for (auto lr : listRows)
    {
      out << "  <tvs name=\"" << trim(changeAmpersand(lr._engName)) 
          << "\" locname=\"" << lr._locName << "\" year=\"" << lr._releaseYear << "\">\n";
      out << "    <info amount=\"" << lr._seasonsAmount << "\" status=\"" << lr._status 
          << "\" path=\"" << lr._path << "\"/>\n";
      
      out << "    <genres>\n";
      std::unique_ptr<std::vector<std::string>> pvs = tokenizeString(lr._genre, true);
      for (auto s : *pvs)
      {
        if (gacIfNecessary) gacIfNecessary->_genres.insert(s);
        out << "      <genre>" << s << "</genre>\n";
      }
      out << "    </genres>\n";

      out << "    <countries>\n";
      pvs = tokenizeString(lr._country);
      for (auto s : *pvs)
      {
        if (gacIfNecessary) gacIfNecessary->_countries.insert(s);
        out << "      <country>" << s << "</country>\n";
      }
      out << "    </countries>\n";

      out << "  </tvs>\n";
    }
    out << "</tvseries>\n";
    out.close();
  }
}

void makeXmlGenresAndCountries(const std::shared_ptr<GenresAndCountries> gac)
{
  std::ofstream out("genres.xml");
  if (out.is_open())
  {
    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n\n";
    out << "<genres>";
    out << "\n";

    for (auto s : gac->_genres)
    {
      out << "  <genre>" << s << "</genre>\n";
    }

    out << "</genres>";
    out << "\n";
    out.close();
  }

  out.open("countries.xml");
  if (out.is_open())
  {
    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n\n";
    out << "<countries>";
    out << "\n";

    for (auto s : gac->_countries)
    {
      out << "  <country>" << s << "</country>\n";
    }

    out << "</countries>";
    out << "\n";
    out.close();
  }
}

int main()
{
//  setlocale(0, ".1251");
//   std::cin.get();
//   std::cin.clear();
  //std::cout.imbue(locRUtf8);
  //_setmode(_fileno(stdout), _O_TEXT);


  try
  {
    std::shared_ptr<std::stringstream> streamFullList = downloadFunction("www.lostfilm.tv", "/serials.php");    
    streamFullList->imbue(locRUtf8);

    std::list<std::tuple<std::string, std::string, std::string>> listTupleBegin;
    std::list<Row> listRowAll;
    
    while (!streamFullList->eof())
    {

      std::string line;
      std::getline(*streamFullList, line);

      static bool found = false;
      if (!found && (line.find("<!-- ### Полный список сериалов -->") != std::string::npos)) found = true;
      if (found)
      {
        if (line.find("<!-- ### Текстовая информация -->") != std::string::npos) break;
        std::smatch sm;

        std::string path;
        std::string rus;
        std::string eng;

        if (std::regex_search(line, sm, std::regex("(/browse.php\\?cat=\\d\\d\?\\d)")))
        {
          path = sm[1];
          line = sm.suffix();

          if (std::regex_search(line, sm, std::regex(">(.*)<br>")))
          {
            rus = sm[1];
            line = sm.suffix();

            if (std::regex_search(line, sm, std::regex(">\\((.*)\\)</span>")))
            {
              eng = sm[1];
            }
          }
          listTupleBegin.push_back(std::make_tuple(path, rus, eng));
        }
      }
    }
    std::cout << "Given a " << listTupleBegin.size() << " URL's" << std::endl;

//     for (auto l : listTupleBegin)
//     {
//       std::cout << "url: " << std::get<0>(l) << ", loc: " << std::get<1>(l) << ", eng: " << std::get<2>(l) << std::endl;
//     }

    _setmode(_fileno(stdout), _O_U8TEXT);
    for (auto tuple : listTupleBegin)
    {
      std::shared_ptr<std::stringstream> stream = downloadFunction("www.lostfilm.tv", std::get<0>(tuple));

      while (!stream->eof())
      {
        std::string line;
        std::getline(*stream, line);
        
        std::string country;
        std::string year;
        std::string genre;
        std::string amount;
        std::string status;
        
        if (line.find("Страна:") != std::string::npos)
        {
          std::smatch sm;
          std::regex_search(line, sm, std::regex("Страна: (.*)<br />"));
          country = sm[1];
          
          std::getline(*stream, line);
          std::regex_search(line, sm, std::regex("Год выхода: <span>(.*)</span><br />"));
          year = sm[1];
          
          std::getline(*stream, line);
          std::regex_search(line, sm, std::regex("Жанр: <span>(.*)</span><br />"));
          genre = sm[1];

          std::getline(*stream, line);
          std::regex_search(line, sm, std::regex("Количество сезонов: <span>(.*)</span><br />"));
          amount = sm[1];

          std::getline(*stream, line);
          std::regex_search(line, sm, std::regex("Статус: (.*)<br />"));
          status = sm[1];

          listRowAll.emplace_back(std::get<0>(tuple), std::get<1>(tuple), std::get<2>(tuple), country, year, genre, amount, status);
        }
        continue;
      }
      static int counter = 0;
      //std::cout << "Given an info about <" << ++counter << "> " << std::get<1>(tuple) << std::endl;
      std::wstring strTmp = std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(std::get<1>(tuple));
      std::wcout << "Given an info about <" << ++counter << "> " << strTmp << std::endl;
      //MessageBox(nullptr, strTmp.data(), nullptr, MB_OK);
    }
    _setmode(_fileno(stdout), _O_TEXT);


    for (auto r : listRowAll)
    {
      std::cout << r << std::endl;
    }

    std::shared_ptr<GenresAndCountries> gac = std::make_shared<GenresAndCountries>();
    makeXmlFullData(listRowAll, gac);
    makeXmlGenresAndCountries(gac);

  }
  catch (const std::exception e)
  {
    std::cout << e.what() << std::endl;
  }
  
  std::cout << "\n\nPush 'Enter' to exit...";
  std::cin.get();
  return 0;
}
