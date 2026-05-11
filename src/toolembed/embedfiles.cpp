//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright 2026 ANSSI. All Rights Reserved.
//
// Author(s): fabienfl (ANSSI)
//

#include "embedfiles.h"

#include <atomic>
#include <vector>

#include <windows.h>
#include <winhttp.h>

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QList>
#include <QSet>
#include <QSettings>

#include "7z/extract.h"
#include "resource/resource.h"
#include "utils/log.h"
#include "version.h"
#include "xmldom.h"

namespace {

// Downloads the resource at 'url' to 'destPath', following up to 'maxRedirects' redirects.
// Returns an error string on failure.
std::expected<void, QString> DownloadUrl(
    const std::wstring &url, const QString &destPath, int redirectsLeft = 10)
{
    if (redirectsLeft < 0)
        return std::unexpected(QString("Too many redirects for URL"));

    URL_COMPONENTS urlComp = {};
    urlComp.dwStructSize = sizeof(urlComp);
    wchar_t hostName[256] = {};
    wchar_t urlPath[4096] = {};
    urlComp.lpszHostName = hostName;
    urlComp.dwHostNameLength = static_cast<DWORD>(std::size(hostName));
    urlComp.lpszUrlPath = urlPath;
    urlComp.dwUrlPathLength = static_cast<DWORD>(std::size(urlPath));

    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &urlComp))
        return std::unexpected(QString("Invalid URL: %1").arg(QString::fromStdWString(url)));

    const bool isHttps = (urlComp.nScheme == INTERNET_SCHEME_HTTPS);

    // Use a realistic browser-like User-Agent to avoid 403s from CDNs
    const std::wstring userAgent = std::wstring(
                                       L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) dfir-orc-forge/")
                                   + kDofVersionStringW;

    HINTERNET hSession = WinHttpOpen(
        userAgent.c_str(),
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0);
    if (!hSession)
        return std::unexpected(QString("WinHttpOpen failed (0x%1)").arg(GetLastError(), 0, 16));

    HINTERNET hConnect = WinHttpConnect(hSession, hostName, urlComp.nPort, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return std::unexpected(QString("WinHttpConnect failed (0x%1)").arg(GetLastError(), 0, 16));
    }

    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect,
        L"GET",
        urlPath,
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        isHttps ? WINHTTP_FLAG_SECURE : 0);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return std::unexpected(
            QString("WinHttpOpenRequest failed (0x%1)").arg(GetLastError(), 0, 16));
    }

    auto closeHandles = [&] {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
    };

    // Disable auto-redirect so we can inspect Location headers ourselves
    DWORD disableRedirect = WINHTTP_DISABLE_REDIRECTS;
    WinHttpSetOption(
        hRequest, WINHTTP_OPTION_DISABLE_FEATURE, &disableRedirect, sizeof(disableRedirect));

    if (!WinHttpSendRequest(
            hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)
        || !WinHttpReceiveResponse(hRequest, nullptr)) {
        const DWORD err = GetLastError();
        closeHandles();
        return std::unexpected(QString("HTTP request failed (0x%1)").arg(err, 0, 16));
    }

    // Read HTTP status code
    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    if (!WinHttpQueryHeaders(
            hRequest,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &statusCode,
            &statusSize,
            WINHTTP_NO_HEADER_INDEX)) {
        const DWORD err = GetLastError();
        closeHandles();
        return std::unexpected(QString("Failed to read HTTP status (0x%1)").arg(err, 0, 16));
    }

    // Handle redirects (301, 302, 303, 307, 308)
    if (statusCode == 301 || statusCode == 302 || statusCode == 303 || statusCode == 307
        || statusCode == 308) {
        wchar_t location[4096] = {};
        DWORD locationSize = sizeof(location);
        WinHttpQueryHeaders(
            hRequest,
            WINHTTP_QUERY_LOCATION,
            WINHTTP_HEADER_NAME_BY_INDEX,
            location,
            &locationSize,
            WINHTTP_NO_HEADER_INDEX);
        closeHandles();

        if (location[0] == L'\0')
            return std::unexpected(QString("Redirect with no Location header"));

        return DownloadUrl(std::wstring(location), destPath, redirectsLeft - 1);
    }

    if (statusCode != 200) {
        closeHandles();
        return std::unexpected(QString("Unexpected HTTP status %1").arg(statusCode));
    }

    // Ensure destination directory exists
    const QString absDir = QFileInfo(destPath).absolutePath();
    if (!QDir().mkpath(absDir)) {
        closeHandles();
        return std::unexpected(QString("Failed to create directory: %1").arg(absDir));
    }

    HANDLE hFile = CreateFileW(
        destPath.toStdWString().c_str(),
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        closeHandles();
        return std::unexpected(QString("Cannot create file: %1").arg(destPath));
    }

    std::vector<char> buffer(64 * 1024);
    DWORD bytesRead = 0;
    bool writeOk = true;
    while (WinHttpReadData(hRequest, buffer.data(), static_cast<DWORD>(buffer.size()), &bytesRead)
           && bytesRead > 0) {
        DWORD written = 0;
        if (!WriteFile(hFile, buffer.data(), bytesRead, &written, nullptr) || written != bytesRead) {
            writeOk = false;
            break;
        }
    }

    CloseHandle(hFile);
    closeHandles();

    if (!writeOk) {
        DeleteFileW(destPath.toStdWString().c_str());
        return std::unexpected(QString("Write error while downloading to: %1").arg(destPath));
    }

    return {};
}

// Detects archive type from the first bytes of a file (magic number check).
// Returns "zip", "7z", or "" (plain / unknown).
// ZIP: PK\x03\x04 (50 4B 03 04)
// 7z:  7z\xBC\xAF\x27\x1C (37 7A BC AF 27 1C)
static QString DetectArchiveType(const QString &filePath)
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    const QByteArray h = f.read(6);
    f.close();
    if (h.size() >= 4 && static_cast<uint8_t>(h[0]) == 0x50 && static_cast<uint8_t>(h[1]) == 0x4B
        && static_cast<uint8_t>(h[2]) == 0x03 && static_cast<uint8_t>(h[3]) == 0x04)
        return QStringLiteral("zip");
    if (h.size() >= 6 && static_cast<uint8_t>(h[0]) == 0x37 && static_cast<uint8_t>(h[1]) == 0x7A
        && static_cast<uint8_t>(h[2]) == 0xBC && static_cast<uint8_t>(h[3]) == 0xAF
        && static_cast<uint8_t>(h[4]) == 0x27 && static_cast<uint8_t>(h[5]) == 0x1C)
        return QStringLiteral("7z");
    return {};
}

// Parses the url= attribute value.
// Format: <url>[,type=auto|zip|7z][,item=path/inside/archive.exe][,eula=eula.txt]
//
// type=auto (default): detect archive type from magic bytes after download.
// type=zip / type=7z:  force a specific codec, skip magic detection.
// item=:               file to pick from archive; defaults to dest basename.
// eula=:               EULA file inside the archive; shown once per content hash.
struct DownloadSpec
{
    QString url;
    QString type; // "zip", "7z", "auto", or "" -- "" treated as "auto"
    QString item;
    QString eula; // filename of EULA inside the archive, or ""
};

static DownloadSpec ParseUrlAttr(const QString &value)
{
    DownloadSpec spec;
    const QStringList parts = value.split(QLatin1Char(','));
    if (parts.isEmpty())
        return spec;

    spec.url = parts[0].trimmed();
    spec.type = QStringLiteral("auto"); // default

    for (int i = 1; i < parts.size(); ++i) {
        const QString kv = parts[i].trimmed();
        if (kv.startsWith(QLatin1String("type="))) {
            const QString t = kv.mid(5).trimmed().toLower();
            if (t == QLatin1String("zip") || t == QLatin1String("7z") || t == QLatin1String("auto"))
                spec.type = t;
        } else if (kv.startsWith(QLatin1String("item="))) {
            spec.item = kv.mid(5).trimmed();
        } else if (kv.startsWith(QLatin1String("eula="))) {
            spec.eula = kv.mid(5).trimmed();
        } else if (kv.startsWith(QLatin1String("license="))) {
            spec.eula = kv.mid(8).trimmed(); // alias
        }
    }

    return spec;
}

// Downloads 'url' and installs the result at 'dest'.
//
// archiveType:
//   "zip" / "7z" -- forced extraction with that codec
//   "auto" / "" -- download to temp, detect magic bytes;
//                  extract if archive, copy directly if plain file.
// item: file to pick from archive; empty -> dest basename.
// eula: filename of EULA inside the archive; empty -> auto-detect.
// eulaPrompt: called with EULA text if the hash has not yet been accepted.
// acceptedHashes: in-memory set of EULA content hashes already accepted this
//   session; updated on acceptance; checked before QSettings to avoid any
//   flush-timing dependency when multiple entries share the same EULA.
static std::expected<void, QString> InstallDownload(
    const QString &url,
    const QString &dest,
    const QString &executableName,
    const QString &archiveType,
    const QString &item,
    const QString &eula,
    const EulaPromptFn &eulaPrompt,
    QSet<QString> &acceptedHashes)
{
    const bool autoDetect = archiveType.isEmpty() || archiveType == QLatin1String("auto");

    // 1. Download to a temp file.
    const QString tmpBase = QDir::tempPath()
                            + QString("/dfir-orc-forge/%1").arg(QCoreApplication::applicationPid());
    if (!QDir().mkpath(tmpBase))
        return std::unexpected(QString("Failed to create temp directory: %1").arg(tmpBase));

    // Include a per-call unique suffix so that two entries with the same
    // basename do not share the same temp file path.
    static std::atomic<int> sDownloadSeq{0};
    const QString tmpFile
        = tmpBase + QString("/dl-%1-%2").arg(++sDownloadSeq).arg(QFileInfo(dest).fileName());

    if (auto rv = DownloadUrl(url.toStdWString(), tmpFile); !rv)
        return rv;

    // 2. Resolve effective archive type.
    QString effectiveType = archiveType;
    if (autoDetect) {
        effectiveType = DetectArchiveType(tmpFile);
        LOG_DEBUG(
            "Auto-detected archive type '{}' for '{}'",
            effectiveType.toStdString(),
            url.toStdString());
    }

    // Plain file: move directly to destination.
    if (effectiveType.isEmpty()) {
        const QString destDir = QFileInfo(dest).absolutePath();
        if (!QDir().mkpath(destDir)) {
            QFile::remove(tmpFile);
            return std::unexpected(QString("Failed to create directory: %1").arg(destDir));
        }
        QFile::remove(dest);
        if (!QFile::rename(tmpFile, dest)) {
            QFile::remove(tmpFile);
            return std::unexpected(QString("Failed to move '%1' to '%2'").arg(tmpFile, dest));
        }
        return {};
    }

    // 3. Extract archive to a temp directory.
    const QString extractDir = tmpFile + "-extracted";
    if (!QDir().mkpath(extractDir)) {
        QFile::remove(tmpFile);
        return std::unexpected(QString("Failed to create extract directory: %1").arg(extractDir));
    }

    auto extractRv
        = (effectiveType == QLatin1String("zip"))
              ? SevenZip::ExtractZip(tmpFile.toStdWString(), extractDir.toStdWString())
              : SevenZip::ExtractArchive(tmpFile.toStdWString(), extractDir.toStdWString());

    QFile::remove(tmpFile);
    if (!extractRv) {
        QDir(extractDir).removeRecursively();
        return std::unexpected(QString("Failed to extract archive for '%1': %2")
                                   .arg(dest, QString::fromStdString(extractRv.error().message())));
    }

    // 4. EULA check: locate a EULA/license file inside the extracted archive and,
    //    if its content has not been accepted before, prompt the user.
    //    eula= in the URL attribute is an explicit override; when absent the
    //    archive is scanned for any well-known license filename automatically.
    if (eulaPrompt) {
        // Well-known EULA/license basenames (case-insensitive).
        static const QStringList kLicenseNames = {
            "eula.txt",
            "eula.rtf",
            "eula.html",
            "license.txt",
            "license.rtf",
            "license.html",
            "license.md",
            "licence.txt",
            "licence.rtf",
            "copying",
            "copying.txt",
        };

        // Locate the EULA file: explicit eula= path first, then auto-detect.
        QString eulaPath;
        if (!eula.isEmpty()) {
            // Explicit: exact relative match preferred, basename fallback.
            const QString eulaBasename = QFileInfo(eula).fileName().toLower();
            const QString eulaRelative = QString(eula).replace(QLatin1Char('\\'), QLatin1Char('/'));
            QDirIterator eulaIt(
                extractDir,
                QDir::Files | QDir::Readable | QDir::NoDotAndDotDot,
                QDirIterator::Subdirectories);
            while (eulaIt.hasNext()) {
                const QString candidate = eulaIt.next();
                const QString rel = QDir(extractDir).relativeFilePath(candidate);
                if (rel.compare(eulaRelative, Qt::CaseInsensitive) == 0) {
                    eulaPath = candidate;
                    break;
                }
                if (eulaPath.isEmpty() && QFileInfo(candidate).fileName().toLower() == eulaBasename)
                    eulaPath = candidate;
            }
        } else {
            // Auto-detect: scan for any well-known license filename.
            QDirIterator eulaIt(
                extractDir,
                QDir::Files | QDir::Readable | QDir::NoDotAndDotDot,
                QDirIterator::Subdirectories);
            while (eulaIt.hasNext() && eulaPath.isEmpty()) {
                const QString candidate = eulaIt.next();
                if (kLicenseNames
                        .contains(QFileInfo(candidate).fileName().toLower(), Qt::CaseInsensitive))
                    eulaPath = candidate;
            }
        }

        if (!eulaPath.isEmpty()) {
            QFile ef(eulaPath);
            if (ef.open(QIODevice::ReadOnly)) {
                const QByteArray eulaBytes = ef.readAll();
                ef.close();
                const QString hash = QString::fromLatin1(
                    QCryptographicHash::hash(eulaBytes, QCryptographicHash::Sha256).toHex());

                // Key combines content hash and executable name so that two
                // different executables sharing the same EULA text each require
                // their own explicit acceptance.
                const QString acceptKey = hash + QLatin1Char('/') + executableName;

                // Check in-memory set first (same session, multiple entries
                // sharing the same EULA content), then fall back to QSettings
                // (cross-session persistence).
                const bool alreadyAccepted
                    = acceptedHashes.contains(acceptKey)
                      || QSettings()
                             .value(QStringLiteral("eula/accepted/") + acceptKey, false)
                             .toBool();

                if (!alreadyAccepted) {
                    const QString eulaText = QString::fromUtf8(eulaBytes);
                    if (!eulaPrompt(eulaText, executableName)) {
                        QDir(extractDir).removeRecursively();
                        return std::unexpected(
                            QString("License for '%1' was not accepted; download skipped.")
                                .arg(QFileInfo(dest).fileName()));
                    }
                    // Persist acceptance both in memory and in QSettings.
                    acceptedHashes.insert(acceptKey);
                    QSettings().setValue(QStringLiteral("eula/accepted/") + acceptKey, true);
                }
            }
        }
    }

    // 5. Search recursively for the target file inside the extracted tree.
    // item= may be a relative path; try exact relative match first,
    // then fall back to basename-only match.
    const QString wantedBasename
        = (item.isEmpty() ? QFileInfo(dest).fileName() : QFileInfo(item).fileName()).toLower();
    const QString wantedRelative = item.isEmpty()
                                       ? QString()
                                       : QString(item).replace(QLatin1Char('\\'), QLatin1Char('/'));
    QString found;
    QDirIterator
        it(extractDir,
           QDir::Files | QDir::Readable | QDir::NoDotAndDotDot,
           QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString candidate = it.next();
        if (!wantedRelative.isEmpty()) {
            const QString rel = QDir(extractDir).relativeFilePath(candidate);
            if (rel.compare(wantedRelative, Qt::CaseInsensitive) == 0) {
                found = candidate;
                break;
            }
        }
        if (found.isEmpty() && QFileInfo(candidate).fileName().toLower() == wantedBasename)
            found = candidate;
    }

    if (found.isEmpty()) {
        QDir(extractDir).removeRecursively();
        return std::unexpected(QString("'%1' not found inside downloaded archive from %2")
                                   .arg(item.isEmpty() ? wantedBasename : item, url));
    }

    // 6. Copy to destination.
    const QString destDir = QFileInfo(dest).absolutePath();
    if (!QDir().mkpath(destDir)) {
        QDir(extractDir).removeRecursively();
        return std::unexpected(QString("Failed to create directory: %1").arg(destDir));
    }

    QFile::remove(dest);
    const bool copied = QFile::copy(found, dest);
    QDir(extractDir).removeRecursively();

    if (!copied)
        return std::unexpected(QString("Failed to copy '%1' to '%2'").arg(found, dest));

    return {};
}

} // namespace

// Walks all executable <file path="..."> entries in the toolembed config
// and calls visitor(f) for each one. Does not perform any network I/O.
// Entries without a url= attribute are included with empty url fields.
template<typename Visitor>
static std::expected<void, QString> WalkEmbedFiles(
    const QString &embedConfigPath, const QString &configFolder, Visitor &&visitor)
{
    XmlDom dom;
    if (!dom.loadFromFile(embedConfigPath))
        return std::unexpected(QString("Failed to parse embed config: %1").arg(embedConfigPath));

    auto nodes = dom.selectNodes("//file[@path]");
    if (!nodes)
        return {};

    long length = 0;
    (*nodes)->get_length(&length);

    for (long i = 0; i < length; ++i) {
        MSXML::IXMLDOMNodePtr node;
        if (FAILED((*nodes)->get_item(i, &node)) || !node)
            continue;

        MSXML::IXMLDOMNamedNodeMapPtr attrs;
        if (FAILED(node->get_attributes(&attrs)) || !attrs)
            continue;

        auto getAttr = [&](const wchar_t *attrName) -> QString {
            try {
                MSXML::IXMLDOMNodePtr attr = attrs->getNamedItem(_bstr_t(attrName));
                if (!attr)
                    return {};
                _variant_t val;
                attr->get_nodeValue(&val);
                return QString::fromWCharArray(static_cast<const wchar_t *>(_bstr_t(val)));
            } catch (const _com_error &) {
                return {};
            }
        };

        const QString name = getAttr(L"name");
        const QString path = getAttr(L"path");
        const QString urlAttr = getAttr(L"url");
        if (path.isEmpty())
            continue;

        // Only list executable files; config/schema/data files are not relevant
        // to the third-party software list shown to the user.
        if (!path.endsWith(QLatin1String(".exe"), Qt::CaseInsensitive))
            continue;

        const QString dest = QDir(configFolder)
                                 .filePath(
                                     QString(path).replace(QLatin1Char('\\'), QLatin1Char('/')));

        const bool present = QFileInfo::exists(dest);

        EmbedFile f;
        f.name = name;
        f.dest = dest;
        f.presentOnDisk = present;
        if (!urlAttr.isEmpty()) {
            const DownloadSpec spec = ParseUrlAttr(urlAttr);
            f.url = urlAttr;
            f.urlParsed = spec.url;
            f.archiveType = spec.type;
            f.item = spec.item;
            f.eula = spec.eula;
        }
        visitor(f);
    }

    return {};
}

std::expected<QList<EmbedFile>, QString> ListEmbedFiles(
    const QString &embedConfigPath, const QString &configFolder)
{
    QList<EmbedFile> result;
    auto rv = WalkEmbedFiles(embedConfigPath, configFolder, [&](const EmbedFile &f) {
        result.append(f);
    });
    if (!rv)
        return std::unexpected(rv.error());
    return result;
}

std::expected<void, QString> ResolveEmbedDownloads(
    const QString &embedConfigPath,
    const QString &configFolder,
    EulaPromptFn eulaPrompt,
    QSet<QString> forceDests)
{
    auto all = ListEmbedFiles(embedConfigPath, configFolder);
    if (!all)
        return std::unexpected(all.error());

    QSet<QString> acceptedHashes;

    for (const auto &f : *all) {
        // Skip entries with no download URL.
        if (f.urlParsed.isEmpty())
            continue;
        // Skip already-present files unless the caller explicitly requested overwrite.
        if (f.presentOnDisk && !forceDests.contains(f.dest))
            continue;

        LOG_DEBUG(
            "Downloading {} -> {} (type={}, item={}, eula={})",
            f.urlParsed.toStdString(),
            f.dest.toStdString(),
            f.archiveType.toStdString(),
            f.item.toStdString(),
            f.eula.toStdString());
        const QString displayName = f.name.isEmpty() ? QFileInfo(f.dest).fileName() : f.name;
        if (auto result = InstallDownload(
                f.urlParsed,
                f.dest,
                displayName,
                f.archiveType,
                f.item,
                f.eula,
                eulaPrompt,
                acceptedHashes);
            !result)
            return std::unexpected(
                QString("Failed to download '%1':\n%2").arg(f.url, result.error()));
    }

    return {};
}

std::expected<QString, QString> ExtractEmbeddedConfig(const QString &tempBaseDir)
{
    auto buffer = Resource::GetResourceData(L"RT_RCDATA", L"DFIRORC_CONFIG", 0);
    if (!buffer)
        return std::unexpected(QString("No embedded DFIRORC_CONFIG resource found"));

    const QString destDir = QDir(tempBaseDir).filePath("dfir-orc-config");
    if (!QDir().mkpath(destDir))
        return std::unexpected(QString("Failed to create temp config directory: %1").arg(destDir));

    auto rv = SevenZip::ExtractZip(
        std::string_view(reinterpret_cast<const char *>(buffer->data()), buffer->size()),
        destDir.toStdWString());
    if (!rv)
        return std::unexpected(QString("Failed to extract embedded config: %1")
                                   .arg(QString::fromStdString(rv.error().message())));

    // The archive is expected to contain a "config" subdirectory at its root.
    // Try that well-known name first, then fall back to the first non-hidden
    // subdirectory (ignoring .git and similar dot-directories).
    const QString configSubDir = QDir(destDir).filePath("config");
    if (QDir(configSubDir).exists())
        return configSubDir;

    const QFileInfoList entries = QDir(destDir).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &entry : entries) {
        if (!entry.fileName().startsWith(u'.'))
            return entry.absoluteFilePath();
    }

    return destDir;
}
