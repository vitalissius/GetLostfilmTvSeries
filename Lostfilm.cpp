#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <list>
#include <memory>
#include <regex>
#include <tuple>
#include <set>

#include <boost/asio.hpp>

std::shared_ptr<std::stringstream> downloadFunction(const std::string& host, const std::string& pathWithQuery)
{
  std::shared_ptr<std::stringstream> returnedStringStream = std::make_shared<std::stringstream>();
  std::string hostString = host;
  std::string pathString = pathWithQuery;

  try
  {
    boost::asio::io_service inputOutputService;

    // Get a list of endpoints corresponding to the server name:
    boost::asio::ip::tcp::resolver ipResolver(inputOutputService);
    boost::asio::ip::tcp::resolver::query queryTo(hostString, "http");
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
    requestOutputStream << "GET " << pathString << " HTTP/1.0\r\n";
    requestOutputStream << "Host: " << hostString << "\r\n";
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
    returnedStringStream->swap(xmlDoc);
  }
  catch (std::exception& e)
  {
    std::cout << e.what() << std::endl;
  }
  return returnedStringStream;
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

std::unique_ptr<std::vector<std::string>> tokenizeString(const std::string& str, bool toUpperFirstLetter = false)
{
  std::unique_ptr<std::vector<std::string>> pvs = std::make_unique<std::vector<std::string>>();

  char* cstr = new char[str.size() + 1];
  strcpy_s(cstr, str.size() + 1, str.data());
  char* token = nullptr;
  char* next_token = nullptr;
  char seps[] = " ,/.";
  token = strtok_s(cstr, seps, &next_token);
  while (token != nullptr)
  {
    if (token != nullptr)
    {
      if (toUpperFirstLetter)
      {
        static std::locale loc("");
        std::use_facet<std::ctype<char>>(loc).toupper(token, token + 1);
        pvs->push_back(token);
      }
      else
      {
        pvs->push_back(token);
      }
      token = strtok_s(nullptr, seps, &next_token);
    }
  }
  delete[] cstr;

  return pvs;
}

std::string& replAndTrim(std::string& str) // Trim string in front (if necessary), and adding 'amp;' after '&' (if necessary);
{
  static std::locale loc("");
  while (std::isspace(str.front(), loc)) str.erase(str.begin());

  std::string::size_type pos = str.find("&");
  while (pos != std::string::npos)
  {
    str.insert(pos + 1, "amp;");
    pos = str.find("&", pos + 4);
  }
  return str;
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
    out << "<?xml version=\"1.0\" encoding=\"windows-1251\"?>\n\n";
    out << "<tvseries>\n";
    for (auto lr : listRows)
    {
      out << "\t<tvs name=\"" << replAndTrim(lr._engName) << "\" locname=\"" << lr._locName << "\" year=\"" << lr._releaseYear << "\">\n";
      out << "\t\t<info amount=\"" << lr._seasonsAmount << "\" status=\"" << lr._status << "\" path=\"" << lr._path << "\" />\n";
      
      out << "\t\t<genres>\n";
      std::unique_ptr<std::vector<std::string>> pvs = tokenizeString(lr._genre, true);
      for (auto s : *pvs)
      {
        if (gacIfNecessary) gacIfNecessary->_genres.insert(s);
        out << "\t\t\t<genre>" << s << "</genre>\n";
      }
      out << "\t\t</genres>\n";

      out << "\t\t<countries>\n";
      pvs = tokenizeString(lr._country);
      for (auto s : *pvs)
      {
        if (gacIfNecessary) gacIfNecessary->_countries.insert(s);
        out << "\t\t\t<country>" << s << "</country>\n";
      }
      out << "\t\t</countries>\n";

      out << "\t</tvs>\n";
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
    out << "<?xml version=\"1.0\" encoding=\"windows-1251\"?>\n\n";
    out << "<genres>";
    out << "\n";

    for (auto s : gac->_genres)
    {
      out << "\t<genre>" << s << "</genre>\n";
    }

    out << "</genres>";
    out << "\n";
    out.close();
  }

  out.open("countries.xml");
  if (out.is_open())
  {
    out << "<?xml version=\"1.0\" encoding=\"windows-1251\"?>\n\n";
    out << "<countries>";
    out << "\n";

    for (auto s : gac->_countries)
    {
      out << "\t<country>" << s << "</country>\n";
    }

    out << "</countries>";
    out << "\n";
    out.close();
  }
}

int main()
{
  setlocale(0, ".1251");
  std::cin.get();
  std::cin.clear();

  try
  {
    std::shared_ptr<std::stringstream> streamFullList = downloadFunction("www.lostfilm.tv", "/serials.php");    
    
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
      std::cout << "Given an info about " << std::get<2>(tuple) << std::endl;
    }


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
