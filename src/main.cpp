// Downloads the highest-versioned JLP_<version>.zip file from an Artifactory
// repository folder using libcurl and the REST "storage" API.

#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include <cctype>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

using json = nlohmann::json;

namespace {

constexpr const char* kDefaultFolderUrl =
    "https://af01p-ir.devtools.intel.com/artifactory/mvt-releases-local/JLPTool";
constexpr const char* kArchivePrefix = "JLP_";
constexpr const char* kArchiveSuffix = ".zip";

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
    std::string versionText;
    std::vector<std::string> versionParts;
};

std::optional<std::vector<std::string>> ParseArchiveVersion(const std::string& name) {
    const std::string prefix = kArchivePrefix;
    const std::string suffix = kArchiveSuffix;
    if (name.size() <= prefix.size() + suffix.size() || name.compare(0, prefix.size(), prefix) != 0 ||
        name.compare(name.size() - suffix.size(), suffix.size(), suffix) != 0) {
        return std::nullopt;
    }

    const std::string version = name.substr(prefix.size(), name.size() - prefix.size() - suffix.size());
    std::vector<std::string> parts;
    size_t start = 0;
    while (start <= version.size()) {
        const size_t end = version.find('.', start);
        std::string part = version.substr(start, end == std::string::npos ? end : end - start);
        if (part.empty()) {
            return std::nullopt;
        }
        for (char character : part) {
            if (!std::isdigit(static_cast<unsigned char>(character))) {
                return std::nullopt;
            }
        }

        const size_t firstNonZero = part.find_first_not_of('0');
        parts.push_back(firstNonZero == std::string::npos ? "0" : part.substr(firstNonZero));
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    return parts;
}

bool IsHigherVersion(const std::vector<std::string>& candidate, const std::vector<std::string>& current) {
    const size_t partCount = candidate.size() > current.size() ? candidate.size() : current.size();
    for (size_t index = 0; index < partCount; ++index) {
        const std::string candidatePart = index < candidate.size() ? candidate[index] : "0";
        const std::string currentPart = index < current.size() ? current[index] : "0";
        if (candidatePart.size() != currentPart.size()) {
            return candidatePart.size() > currentPart.size();
        }
        if (candidatePart != currentPart) {
            return candidatePart > currentPart;
        }
    }
    return false;
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

    std::optional<Candidate> best;
    for (const auto& child : listJson.value("children", json::array())) {
        if (child.value("folder", false)) {
            continue;
        }
        std::string uri = child.value("uri", "");
        if (!uri.empty() && uri.front() == '/') {
            uri.erase(0, 1);
        }
        auto versionParts = ParseArchiveVersion(uri);
        if (!versionParts) {
            continue;
        }
        if (!best || IsHigherVersion(*versionParts, best->versionParts)) {
            const size_t versionLength = uri.size() - std::string(kArchivePrefix).size() -
                                         std::string(kArchiveSuffix).size();
            best = Candidate{uri, uri.substr(std::string(kArchivePrefix).size(), versionLength),
                             std::move(*versionParts)};
        }
    }

    if (!best) {
        std::cerr << "No archives matching JLP_<version>.zip were found\n";
        return std::nullopt;
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

    std::cout << "Latest matching file: " << best->name << " (version=" << best->versionText << ")\n";

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
