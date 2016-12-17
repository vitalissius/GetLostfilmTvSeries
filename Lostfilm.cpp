#include <chrono>
#include <codecvt>
#include <fstream>
#include <functional>
#include <list>
#include <iostream>
#include <memory>
#include <vector>
#include <regex>
#include <set>
#include <string>
#include <tuple>

#include <boost/asio.hpp>

#ifdef _MSC_VER
  std::locale locr("rus_rus.1251");       // ukr_ukr.1251
#else
  std::locale locr("ru_RU");              // uk_UA
#endif  // !_MSC_VER



struct Information;

struct Serial;

template<typename Str1, typename Str2>
std::vector<Information> downloadInformation(Str1 host, Str2 path);

template<typename Str>
std::vector<Serial> downloadSerials(Str host, std::vector<Information> data);

template<typename Str>
void makeXmlGenres(Str filename, const std::set<std::string>& genres, bool is_to_utf8=false);

template<typename Str>
void makeXmlCountries(Str filename, const std::set<std::string>& countries, bool is_to_utf8=false);

template<typename Str>
void makeXmlFullData(Str filename, const std::vector<Serial>& serials, bool is_to_utf8=false);

std::set<std::string> reorganizeGenres(const std::vector<Serial>& disorganized);

std::set<std::string> reorganizeCountries(std::vector<Serial>& disorganized);


int main()
{
    std::locale::global(locr);

    std::string host = "www.lostfilm.tv";
    std::string path = "/serials.php";
    

    auto data = downloadInformation(host, path);
    std::cout << data.size() << " elements\n";
    
    auto serials = downloadSerials(host, std::move(data));
    
    auto genres = reorganizeGenres(serials);
    makeXmlGenres("genres.xml", genres);

    auto countries = reorganizeCountries(serials);
    makeXmlCountries("countries.xml", countries);

    makeXmlFullData("tvseries.xml", serials);

    std::copy(serials.begin(), serials.end(), std::ostream_iterator<Serial>(std::cout, "\n"));

    return 0;
}



struct Information {
    std::string _path;
    std::string _loc_name;
    std::string _orig_name;

    Information(std::string url, std::string loc_name, std::string orig_name)
        : _path(std::move(url))
        , _loc_name(std::move(loc_name))
        , _orig_name(std::move(orig_name))
    {}
};

struct Serial {
    std::string _path;
    std::string _loc_name;
    std::string _orig_name;
    std::string _country;
    std::string _release_year;
    std::string _genre;
    std::string _seasons_amount;
    std::string _status;

public:
    Serial(const std::string& path,
        const std::string& loc_name,
        const std::string& orig_name,
        const std::string& country,
        const std::string& release_year,
        const std::string& genre,
        const std::string& seasons_amount,
        const std::string& status)
        : _path(std::move(path))
        , _loc_name(std::move(loc_name))
        , _orig_name(std::move(orig_name))
        , _country(std::move(country))
        , _release_year(std::move(release_year))
        , _genre(std::move(genre))
        , _seasons_amount(std::move(seasons_amount))
        , _status(std::move(status))
    {}

    friend std::ostream& operator<<(std::ostream& out, const Serial& serial)
    {
        out << "\nPath:           " << serial._path
            << "\nLocale name:    " << serial._loc_name
            << "\nOriginal name:  " << serial._orig_name
            << "\nCountry:        " << serial._country
            << "\nRelease year:   " << serial._release_year
            << "\nGenre:          " << serial._genre
            << "\nSeasons amount: " << serial._seasons_amount
            << "\nStatus:         " << serial._status;
        return out;
    }
};



std::string& trim(std::string& str)
{
    if (!str.empty())
    {
        std::size_t pos = 0;
        while (std::isspace(*(str.begin() + pos), locr)) { ++pos; }
        if (pos != 0) { str.erase(0, pos); }

        std::size_t rpos = 0;
        while (std::isspace(*(str.rbegin() + rpos), locr)) { ++rpos; }
        if (rpos != 0) { str.erase(str.size() - rpos, str.size()); }
    }
    return str;
}

std::vector<std::string> tokenize(std::string str, const char* seps, const bool is_to_upeer = false)
{
    std::vector<std::string> vs;

    for (const auto& e : std::string(seps))
    {
        std::replace_if(str.begin(), str.end(), [e](const char c) { return c == e; }, '\n');
    }
    std::stringstream ss;
    ss << str;
    std::string tmp;
    while (std::getline(ss, tmp))
    {
        tmp = trim(tmp);
        if (is_to_upeer)    // is to uppercase the first letter
        {
            std::use_facet<std::ctype<char>>(locr).toupper(&tmp[0], &tmp[0] + 1);
        }
        vs.emplace_back(std::move(tmp));
    }
    return vs;
}

std::string cp1251ToUtf8(const std::string& str)
{
    std::wstring wstr(str.length(), 0);
    std::use_facet<std::ctype<wchar_t>>(std::locale(locr)).widen(&str[0], &str[0] + str.length(), &wstr[0]);
    const std::string ustr = std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t>().to_bytes(wstr);
    return ustr;
};

std::string converter(const std::string& data, bool is_to_utf8 = false)
{
    if (is_to_utf8) { return cp1251ToUtf8(data); }
    else { return data; }
}



template<typename Str1, typename Str2>
std::stringstream downloadPage(Str1 host, Str2 path)
{
    boost::asio::ip::tcp::iostream ios;
    ios.expires_from_now(boost::posix_time::seconds(5));
    ios.connect(host, "http");
    if (!ios)
    {
        throw std::exception(ios.error().message().data());
    }

    ios << "GET " << path << " HTTP/1.0\r\n";
    ios << "Host: " << host << "\r\n";
    ios << "Accept: */*\r\n";
    ios << "Connection: close\r\n\r\n";

    std::string header;
    while (std::getline(ios, header) && header != "\r") {}

    std::stringstream page;
    page << ios.rdbuf();
    return page;
}

template<typename Str1, typename Str2>
std::vector<Information> downloadInformation(Str1 host, Str2 path)
{
    std::stringstream ss = downloadPage(host, path);

    std::string buf;
    std::string start_marker = "<!-- ### Полный список сериалов -->";
    while (std::getline(ss, buf) && (buf.find(start_marker) == std::string::npos))
    {
    }

    std::vector<Information> data;

    std::string end_marker = "<br />";
    while (std::getline(ss, buf) && (buf.find(end_marker) == std::string::npos))
    {
        std::smatch match;
        std::regex expr(R"_(<a href="(.*)" class="bb_a">(.*)<br><span>\((.*)\)</span></a>)_");
        if (std::regex_search(buf, match, expr))
        {
            data.emplace_back(std::move(match[1]), std::move(match[2]), std::move(match[3]));
        }
    }
    return data;
}

template<typename Str>
std::vector<Serial> downloadSerials(Str host, std::vector<Information> data)
{
    std::vector<Serial> serials;
    std::size_t index = 0;

    std::regex expr_country(R"_(Страна: (.+)<br />)_");
    std::regex expr_releaseyear(R"_(Год выхода: <span>(.+)</span><br />)_");
    std::regex expr_genre(R"_(Жанр: <span>(.+)</span><br />)_");
    std::regex expr_seasons_amount(R"_(Количество сезонов: <span>(.+)</span><br />)_");
    std::regex expr_status(R"_(Статус: (.+)<br />)_");

    for (const auto& e : data)
    {
        std::cout << "<" << index++ << "> Receiving information about " << e._loc_name << ".\n";

        const std::stringstream ss = downloadPage(host, e._path);

        const std::string page = ss.str();

        const std::string start_marker = "<h1>" + e._loc_name + " " + "(" + e._orig_name + ")" + "</h1><br />";
        static const std::string end_marker = R"_(<div class="content">)_";

        const auto start_pos = page.find(start_marker);
        const auto end_pos = page.find(end_marker, start_pos);

        const std::string block(page.begin() + start_pos, page.begin() + end_pos);

        // lambda
        static const auto _search = [](const std::string& s, const std::regex& r) -> std::string {
            std::smatch m;
            if (std::regex_search(s, m, r))
            {
                return m[1];
            }
            return{ "" };
        };

        serials.emplace_back(
            std::move(e._path),
            std::move(e._loc_name),
            std::move(e._orig_name),
            _search(block, expr_country),
            _search(block, expr_releaseyear),
            _search(block, expr_genre),
            _search(block, expr_seasons_amount),
            _search(block, expr_status)
        );
    }
    return serials;
}



std::string xmlDeclaration(bool is_to_utf8=false)
{
    static const std::string charset_cp1251 = "windows-1251";
    static const std::string charset_utf8 = "utf-8";

    std::string head;
    head += "<?xml version=\"1.0\" encoding=\"";
    if (is_to_utf8) { head += charset_utf8; }
    else { head += charset_cp1251; }
    head += "\"?>\n\n";

    return head;
}

template<typename Str>
void makeXmlGenres(Str filename, const std::set<std::string>& genres, bool is_to_utf8)
{
    std::ofstream fout(filename);
    if (fout.is_open())
    {
        fout << xmlDeclaration(is_to_utf8);
        fout << "<genres>";
        fout << "\n";

        for (const auto& e : genres) 
        {
            fout << "  <genre>";
            if (is_to_utf8) { fout << cp1251ToUtf8(e); }
            else { fout << e; }
            fout << "</genre>\n";
        }

        fout << "</genres>";
        fout << "\n";
    }
}

template<typename Str>
void makeXmlCountries(Str filename, const std::set<std::string>& countries, bool is_to_utf8)
{
    std::ofstream fout(filename);
    if (fout.is_open())
    {
        fout << xmlDeclaration(is_to_utf8);
        fout << "<countries>";
        fout << "\n";

        for (const auto& e : countries)
        {
            fout << "  <country>";
            if (is_to_utf8) { fout << cp1251ToUtf8(e); }
            else { fout << e; }
            fout << "</country>\n";
        }

        fout << "</countries>";
        fout << "\n";
    }
}

template<typename Str>
void makeXmlFullData(Str filename, const std::vector<Serial>& serials, bool is_to_utf8)
{
    std::ofstream fout(filename);
    if (fout.is_open())
    {
        fout << xmlDeclaration(is_to_utf8);
        fout << "<tvseries>\n";
        for (const auto& e : serials)
        {
            fout << "  <tvs name=\"";
            fout << converter(e._orig_name, is_to_utf8);
            fout << "\" locname=\"";
            fout << converter(e._loc_name, is_to_utf8);
            fout << "\" year=\"";
            fout << e._release_year;
            fout << "\">\n";
            fout << "    <info amount=\"";
            fout << e._seasons_amount;
            fout << "\" status=\"";
            fout << converter(e._status, is_to_utf8);
            fout << "\" path=\"" << e._path;
            fout << "\"/>\n";

            fout << "    <genres>\n";
            for (const auto genres : tokenize(e._genre, ",./", true))
            {
                fout << "      <genre>";
                fout << converter(genres, is_to_utf8);
                fout << "</genre>\n";
            }
            fout << "    </genres>\n";

            fout << "    <countries>\n";
            for (const auto countries : tokenize(e._country, ",./", true))
            {
                fout << "      <country>";
                fout << converter(countries, is_to_utf8);
                fout << "</country>\n";
            }
            fout << "    </countries>\n";
            fout << "  </tvs>\n";
        }
        fout << "</tvseries>\n";
    }
}



enum class Member {
    Genre,
    Country
};

std::set<std::string> reorganize(const std::vector<Serial>& disorganized, Member mem)
{
    std::set<std::string> organized;
    for (const auto& e : disorganized)
    {
        const auto v = tokenize(mem == Member::Genre ? e._genre : e._country, ".,/", true);
        organized.insert(v.begin(), v.end());
    }
    return organized;
}

std::set<std::string> reorganizeGenres(const std::vector<Serial>& disorganized)
{
    return reorganize(disorganized, Member::Genre);
}

std::set<std::string> reorganizeCountries(std::vector<Serial>& disorganized)
{
    return reorganize(disorganized, Member::Country);
}
