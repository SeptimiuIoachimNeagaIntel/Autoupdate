// Downloads the newest file matching "JLP*.zip" from an Artifactory
// repository folder using libcurl, with the file listing/metadata parsed
// via the Artifactory REST "storage" API.

#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

using json = nlohmann::json;

namespace {

constexpr const char* kDefaultFolderUrl =
    "https://af01p-ir.devtools.intel.com/artifactory/mvt-releases-local/JLPTool";
constexpr const char* kFileNamePattern = "JLP*.zip";

// RAII wrapper around curl_global_init/curl_global_cleanup.
class CurlGlobalGuard {
public:
    CurlGlobalGuard() { curl_global_init(CURL_GLOBAL_DEFAULT); }
    ~CurlGlobalGuard() { curl_global_cleanup(); }
    CurlGlobalGuard(const CurlGlobalGuard&) = delete;
    CurlGlobalGuard& operator=(const CurlGlobalGuard&) = delete;
};

size_t WriteToString(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* out = static_cast<std::string*>(userdata);
    out->append(ptr, size * nmemb);
    return size * nmemb;
}

size_t WriteToFile(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* out = static_cast<std::ofstream*>(userdata);
    out->write(ptr, static_cast<std::streamsize>(size * nmemb));
    return size * nmemb;
}

// Performs an HTTP GET and returns {httpStatusCode, body}. On transport
// failure, httpStatusCode is 0 and an error message is printed to stderr.
std::pair<long, std::string> HttpGet(const std::string& url) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "Failed to initialize curl handle\n";
        return {0, {}};
    }

    std::string body;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteToString);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "jlp_downloader/1.0");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 0L);

    CURLcode res = curl_easy_perform(curl);
    long httpCode = 0;
    if (res == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    } else {
        std::cerr << "HTTP GET failed for " << url << ": " << curl_easy_strerror(res) << "\n";
    }

    curl_easy_cleanup(curl);
    return {httpCode, body};
}

// Downloads url to destinationPath, streaming the response directly to
// disk. Returns true on success (HTTP 200).
bool DownloadFile(const std::string& url, const std::string& destinationPath) {
    std::ofstream out(destinationPath, std::ios::binary);
    if (!out) {
        std::cerr << "Failed to open output file: " << destinationPath << "\n";
        return false;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "Failed to initialize curl handle\n";
        return false;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteToFile);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &out);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "jlp_downloader/1.0");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 600L);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 0L);

    CURLcode res = curl_easy_perform(curl);
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_easy_cleanup(curl);
    out.close();

    if (res != CURLE_OK || httpCode != 200) {
        std::cerr << "Download failed (HTTP " << httpCode << ")\n";
        return false;
    }
    return true;
}

// Minimal glob matcher supporting '*' and '?', case-sensitive.
bool MatchesWildcard(const std::string& name, const std::string& pattern) {
    size_t n = 0, p = 0, star = std::string::npos, matchPos = 0;
    while (n < name.size()) {
        if (p < pattern.size() && (pattern[p] == '?' || pattern[p] == name[n])) {
            ++n;
            ++p;
        } else if (p < pattern.size() && pattern[p] == '*') {
            star = p++;
            matchPos = n;
        } else if (star != std::string::npos) {
            p = star + 1;
            n = ++matchPos;
        } else {
            return false;
        }
    }
    while (p < pattern.size() && pattern[p] == '*') {
        ++p;
    }
    return p == pattern.size();
}

// Rewrites a repository folder URL such as
// "https://host/artifactory/repo/path" into the corresponding Artifactory
// REST storage API URL "https://host/artifactory/api/storage/repo/path".
std::optional<std::string> ToStorageApiUrl(const std::string& folderUrl) {
    const std::string marker = "/artifactory/";
    size_t pos = folderUrl.find(marker);
    if (pos == std::string::npos) {
        return std::nullopt;
    }
    std::string prefix = folderUrl.substr(0, pos + marker.size());
    std::string suffix = folderUrl.substr(pos + marker.size());
    return prefix + "api/storage/" + suffix;
}

std::string JoinUrl(const std::string& base, const std::string& child) {
    if (!base.empty() && base.back() == '/') {
        return base + child;
    }
    return base + "/" + child;
}

struct Candidate {
    std::string name;
    std::string lastModified;
    long long epochMillis = 0;
};

// Days since 1970-01-01 for a given civil (proleptic Gregorian) date.
// Algorithm by Howard Hinnant (public domain); avoids depending on the
// current timezone the way mktime()/localtime() would.
long long DaysFromCivil(long long y, unsigned m, unsigned d) {
    y -= m <= 2;
    const long long era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + static_cast<long long>(doe) - 719468;
}

// Parses an Artifactory-style ISO8601 timestamp (e.g.
// "2026-09-10T18:03:03.635+01:00" or "...Z") into milliseconds since the
// Unix epoch (UTC), so timestamps using different timezone offsets can be
// compared correctly. Returns std::nullopt on failure.
std::optional<long long> ParseIso8601ToEpochMillis(const std::string& s) {
    int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0, millis = 0;
    int consumed = 0;
    int matched = std::sscanf(s.c_str(), "%4d-%2d-%2dT%2d:%2d:%2d.%3d%n", &year, &month, &day,
                               &hour, &minute, &second, &millis, &consumed);
    if (matched != 7) {
        return std::nullopt;
    }

    char offsetSign = 'Z';
    int offsetHour = 0, offsetMinute = 0;
    std::string rest = s.substr(consumed);
    if (!rest.empty() && (rest[0] == '+' || rest[0] == '-')) {
        offsetSign = rest[0];
        if (std::sscanf(rest.c_str() + 1, "%2d:%2d", &offsetHour, &offsetMinute) != 2) {
            return std::nullopt;
        }
    }

    long long days = DaysFromCivil(year, static_cast<unsigned>(month), static_cast<unsigned>(day));
    long long utcSeconds = days * 86400LL + hour * 3600LL + minute * 60LL + second;
    if (offsetSign == '+') {
        utcSeconds -= (offsetHour * 3600LL + offsetMinute * 60LL);
    } else if (offsetSign == '-') {
        utcSeconds += (offsetHour * 3600LL + offsetMinute * 60LL);
    }
    return utcSeconds * 1000LL + millis;
}

std::optional<Candidate> QueryLatestJlpArchive(const std::string& folderUrl) {
    auto apiUrlOpt = ToStorageApiUrl(folderUrl);
    if (!apiUrlOpt) {
        std::cerr << "Could not derive Artifactory API URL from: " << folderUrl << "\n";
        return std::nullopt;
    }
    const std::string apiUrl = *apiUrlOpt;

    std::cout << "Listing folder: " << apiUrl << "\n";
    auto [listCode, listBody] = HttpGet(apiUrl);
    if (listCode != 200) {
        std::cerr << "Failed to list folder contents (HTTP " << listCode << ")\n";
        return std::nullopt;
    }

    json listJson;
    try {
        listJson = json::parse(listBody);
    } catch (const std::exception& e) {
        std::cerr << "Failed to parse folder listing JSON: " << e.what() << "\n";
        return std::nullopt;
    }

    std::vector<std::string> candidateNames;
    for (const auto& child : listJson.value("children", json::array())) {
        if (child.value("folder", false)) {
            continue;
        }
        std::string uri = child.value("uri", "");
        if (!uri.empty() && uri.front() == '/') {
            uri.erase(0, 1);
        }
        if (MatchesWildcard(uri, kFileNamePattern)) {
            candidateNames.push_back(uri);
        }
    }

    if (candidateNames.empty()) {
        std::cerr << "No files matching pattern '" << kFileNamePattern << "' were found\n";
        return std::nullopt;
    }

    std::cout << "Found " << candidateNames.size() << " candidate(s), checking timestamps...\n";

    std::optional<Candidate> best;
    for (const auto& name : candidateNames) {
        auto [code, body] = HttpGet(JoinUrl(apiUrl, name));
        if (code != 200) {
            std::cerr << "  Skipping " << name << ": failed to fetch metadata (HTTP " << code << ")\n";
            continue;
        }

        try {
            json fileJson = json::parse(body);
            std::string lastModified = fileJson.value("lastModified", "");
            auto epochMillis = ParseIso8601ToEpochMillis(lastModified);
            std::cout << "  " << name << " lastModified=" << lastModified << "\n";
            if (!epochMillis) {
                std::cerr << "    Skipping: could not parse timestamp\n";
                continue;
            }
            if (!best || *epochMillis > best->epochMillis) {
                best = Candidate{name, lastModified, *epochMillis};
            }
        } catch (const std::exception& e) {
            std::cerr << "  Skipping " << name << ": failed to parse metadata JSON: " << e.what() << "\n";
        }
    }

    return best;
}

} // namespace

int main(int argc, char** argv) {
    std::string folderUrl = kDefaultFolderUrl;
    std::string outputDir = ".";
    if (argc > 1) {
        folderUrl = argv[1];
    }
    if (argc > 2) {
        outputDir = argv[2];
    }

    CurlGlobalGuard curlGuard;

    std::optional<Candidate> best = QueryLatestJlpArchive(folderUrl);

    if (!best) {
        std::cerr << "Could not determine the latest matching file\n";
        return 1;
    }

    std::cout << "Latest matching file: " << best->name << " (lastModified=" << best->lastModified << ")\n";

    const std::string downloadUrl = JoinUrl(folderUrl, best->name);
    const std::string destinationPath = JoinUrl(outputDir, best->name);

    std::cout << "Downloading " << downloadUrl << " -> " << destinationPath << "\n";
    if (!DownloadFile(downloadUrl, destinationPath)) {
        std::cerr << "Download failed\n";
        return 1;
    }

    std::cout << "Download complete: " << destinationPath << "\n";
    return 0;
}
