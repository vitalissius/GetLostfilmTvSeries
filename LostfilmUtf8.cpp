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



std::string cp1251ToUtf8(std::stringstream ss)
{
    std::string str = ss.str();
    std::wstring wstr(str.length(), 0);
    std::use_facet<std::ctype<wchar_t>>(std::locale(locR)).widen(&str[0], &str[0] + str.length(), &wstr[0]);

    std::string ustr = std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t>().to_bytes(wstr);
    return ustr;
};



template<typename Str1, typename Str2>
std::stringstream download(Str1 host, Str2 pathWithQuery)
{
    boost::asio::ip::tcp::iostream ios;
    ios.imbue(locR);
    ios.expires_from_now(boost::posix_time::seconds(60));
    ios.connect(host, "http");
    if (!ios)
    {
        throw std::exception("Connection error!");
    }

    ios << "GET ";
    ios << pathWithQuery;
    ios << " HTTP/1.0\r\n";
    ios << "Host: ";
    ios << host;
    ios << "\r\n";
    ios << "Accept: */*\r\n";
    ios << "Connection: close\r\n\r\n";

    std::string header;
    while (std::getline(ios, header) && header != "\r")
    {}

    std::stringstream sspage;
    sspage << ios.rdbuf();

    sspage << cp1251ToUtf8(std::move(sspage));
    return sspage;
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

    template<typename Str>
    Row::Row(Str path, Str locName, Str engName, Str country, Str releaseYear, Str genre, Str seasonsAmount, Str status)
        : _path(path)
        , _locName(locName)
        , _engName(engName)
        , _country(country)
        , _releaseYear(releaseYear)
        , _genre(genre)
        , _seasonsAmount(seasonsAmount)
        , _status(status)
    {}

    friend std::wostream& operator<<(std::wostream& out, const Row& r)
    {
        out << "path: " << std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(r._path)
            << "\nloc: " << std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(r._locName)
            << "\neng: " << std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(r._engName)
            << "\ncountry: " << std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(r._country)
            << "\nyear: " << std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(r._releaseYear)
            << "\ngenre: " << std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(r._genre)
            << "\namount: " << std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(r._seasonsAmount)
            << "\nstatus: " << std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(r._status)
            << "\n";
        return out;
    }
};



std::string& trim(std::string& str)
{
    while (std::isspace(str.front(), locR)) { str.erase(str.begin()); }
    while (std::isspace(str.back(), locR)) { str.erase(--str.end()); }
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



std::vector<std::string> tokenizeString(const std::string& str, bool toUpperFirstLetter = false)
{
    std::vector<std::string> vs = std::vector<std::string>();

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
                std::use_facet<std::ctype<wchar_t>>(locR).toupper(&wstrTmp[0], &wstrTmp[0] + 1);
                vs.push_back(std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(wstrTmp));
            }
            else
            {
                vs.push_back(trim(strToken));
            }
            token = strtok_s(nullptr, seps, &next_token);
        }
    }
    delete[] cstr;

    return vs;
}



struct GenresAndCountries {
    std::set<std::string> _genres;
    std::set<std::string> _countries;
};



void makeXmlFullData(const std::list<Row>& listRows, GenresAndCountries& gacOut)
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
            std::vector<std::string> vs = tokenizeString(lr._genre, true);
            for (auto s : vs)
            {
                gacOut._genres.insert(s);
                out << "      <genre>" << s << "</genre>\n";
            }
            out << "    </genres>\n";

            out << "    <countries>\n";
            vs = tokenizeString(lr._country);
            for (auto s : vs)
            {
                gacOut._countries.insert(s);
                out << "      <country>" << s << "</country>\n";
            }
            out << "    </countries>\n";

            out << "  </tvs>\n";
        }
        out << "</tvseries>\n";
    }
}



void makeXmlGenresAndCountries(const GenresAndCountries& gac)
{
    std::ofstream out("genres.xml");
    if (out.is_open())
    {
        out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n\n";
        out << "<genres>";
        out << "\n";

        for (auto s : gac._genres)
        {
            out << "  <genre>" << s << "</genre>\n";
        }

        out << "</genres>";
        out << "\n";
    }

    out.open("countries.xml");
    if (out.is_open())
    {
        out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n\n";
        out << "<countries>";
        out << "\n";

        for (auto s : gac._countries)
        {
            out << "  <country>" << s << "</country>\n";
        }

        out << "</countries>";
        out << "\n";
    }
}



int main()
{
    _setmode(_fileno(stdout), _O_U8TEXT);
    try
    {
        std::stringstream streamFullList = download("www.lostfilm.tv", "/serials.php");
        streamFullList.imbue(locR);

        std::list<std::tuple<std::string, std::string, std::string>> listTupleBegin;
        std::list<Row> listRowAll;

        while (!streamFullList.eof())
        {

            std::string line;
            std::getline(streamFullList, line);

            static bool found = false;
            if (!found && (line.find("<!-- ### Полный список сериалов -->") != std::string::npos))
            {
                found = true;
            }

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
        std::wcout << "Received " << listTupleBegin.size() << " URLs" << std::endl;

        for (auto l : listTupleBegin)
        {
            std::wcout << "url: " << std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(std::get<0>(l))
                << ", loc: " << std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(std::get<1>(l))
                << ", eng: " << std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(std::get<2>(l))
                << std::endl;
        }

        for (auto tuple : listTupleBegin)
        {
            std::stringstream stream = download("www.lostfilm.tv", std::get<0>(tuple));

            while (!stream.eof())
            {
                std::string line;
                std::getline(stream, line);
        
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
          
                    std::getline(stream, line);
                    std::regex_search(line, sm, std::regex("Год выхода: <span>(.*)</span><br />"));
                    year = sm[1];
          
                    std::getline(stream, line);
                    std::regex_search(line, sm, std::regex("Жанр: <span>(.*)</span><br />"));
                    genre = sm[1];

                    std::getline(stream, line);
                    std::regex_search(line, sm, std::regex("Количество сезонов: <span>(.*)</span><br />"));
                    amount = sm[1];

                    std::getline(stream, line);
                    std::regex_search(line, sm, std::regex("Статус: (.*)<br />"));
                    status = sm[1];

                    listRowAll.emplace_back(std::get<0>(tuple), std::get<1>(tuple), std::get<2>(tuple), country, year, genre, amount, status);
                }
                continue;
            }
            static int counter = 0;
            std::wstring strTmp = std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(std::get<1>(tuple));
            std::wcout << "Received the information about <" << ++counter << "> " << strTmp << std::endl;
        }

        for (auto r : listRowAll)
        {
            std::wcout << r << std::endl;
        }

        GenresAndCountries gac;
        makeXmlFullData(listRowAll, gac);
        makeXmlGenresAndCountries(gac);
    }
    catch (const std::exception e)
    {
        std::cout << e.what() << std::endl;
    }

    std::wcout << "\n\nPush 'Enter' to exit...";
    std::wcin.get();
    return 0;
}
