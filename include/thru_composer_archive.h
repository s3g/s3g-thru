#pragma once
#include "thru_composer_format.h"
#include "thru_composer_hash.h"
#include "thru_project_zip.h"
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>
namespace s3g_thru_pack {
inline bool archivePath(const std::string &n) {
  if (n == "manifest.json" || n == "composition.json")
    return true;
  if (n.rfind("assets/", 0) == 0 && n.size() == 75 && n.substr(71) == ".wav")
    return hashId(n.substr(7, 64));
  const std::string prefix = "extensions/s3g-thru/";
  return n.rfind(prefix, 0) == 0 && n.size() > prefix.size() + 4 &&
         n.substr(n.size() - 4) == ".bin" &&
         safeId(n.substr(prefix.size(), n.size() - prefix.size() - 4));
}
inline std::string readFile(const std::string &path, std::size_t limit) {
  struct stat st {};
  need(::lstat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode) &&
           st.st_size >= 0 && std::uint64_t(st.st_size) <= limit,
       path, "file missing or exceeds size limit");
  std::ifstream in(path, std::ios::binary);
  need(bool(in), path, "cannot open");
  std::string out(std::size_t(st.st_size), '\0');
  in.read(out.data(), std::streamsize(out.size()));
  need(bool(in) && in.peek() == EOF, path, "incomplete file read");
  return out;
}
inline void atomicFile(const std::string &path, const std::string &bytes) {
  const auto tmp = path + ".tmp";
  auto *f = std::fopen(tmp.c_str(), "wb");
  need(f, path, "cannot create");
  const bool ok = std::fwrite(bytes.data(), 1, bytes.size(), f) == bytes.size();
  const bool closed = std::fclose(f) == 0;
  if (!ok || !closed || std::rename(tmp.c_str(), path.c_str()) != 0) {
    std::remove(tmp.c_str());
    throw std::runtime_error(path + ": incomplete file write");
  }
}
struct Archive {
  mz_zip_archive zip{};
  s3g_thru_project::ZipBudget budget;
  std::map<std::string, int> entries;
  J document, manifest;
  explicit Archive(const std::string &path) {
    try {
      s3g_thru_project::setupZip(zip, budget);
      need(mz_zip_reader_init_file(&zip, path.c_str(), 0), "pack",
           "cannot read ZIP");
      need(zip.m_archive_size <= 2ull * 1024 * 1024 * 1024, "pack",
           "exceeds 2 GiB");
      const auto count = mz_zip_reader_get_num_files(&zip);
      need(count <= 640, "pack", "too many entries");
      std::uint64_t total = 0;
      for (mz_uint i = 0; i < count; ++i) {
        mz_zip_archive_file_stat s{};
        need(mz_zip_reader_file_stat(&zip, i, &s), "pack", "bad ZIP directory");
        std::string name = s.m_filename;
        need(archivePath(name) && !s.m_is_directory && !s.m_is_encrypted &&
                 (s.m_method == 0 || s.m_method == 8) &&
                 ((s.m_external_attr >> 16) & 0170000) != 0120000 &&
                 entries.emplace(name, int(i)).second,
             "pack", "unsafe or duplicate entry");
        const auto max =
            name.rfind("assets/", 0) == 0
                ? 512ull * 1024 * 1024
                : name.size() > 4 && name.substr(name.size() - 4) == ".bin"
                      ? 64ull * 1024 * 1024
                      : 16ull * 1024 * 1024;
        total += s.m_uncomp_size;
        need(s.m_uncomp_size <= max && total <= 2ull * 1024 * 1024 * 1024,
             "pack", "expanded size exceeds budget");
      }
      manifest = s3g_thru_json::parse(extract("manifest.json"));
      keys(manifest, "manifest",
           {"format", "version", "composition", "sha256", "portable", "files"});
      need(manifest.at("format").string() == "s3g-thrupack" &&
               manifest.at("version").number() == 1 &&
               manifest.at("composition").string() == "composition.json",
           "manifest", "unsupported version");
      const auto text = extract("composition.json");
      need(sha256(text) == manifest.at("sha256").string(), "composition",
           "hash mismatch");
      document = s3g_thru_json::parse(text);
      validate(document);
      std::set<std::string> declared{"manifest.json", "composition.json"};
      for (const auto &f : array(manifest.at("files"), "manifest.files", 638)) {
        keys(f, "manifest.file", {"path", "size", "sha256"});
        const auto &n = string(f.at("path"), "manifest.file.path");
        need(archivePath(n) && declared.insert(n).second && entries.count(n),
             "manifest", "missing or duplicate file");
        mz_zip_archive_file_stat s{};
        mz_zip_reader_file_stat(&zip, entries.at(n), &s);
        need(double(s.m_uncomp_size) == number(f.at("size"),
                                               "manifest.file.size", 1,
                                               512.0 * 1024 * 1024, true) &&
                 hashId(string(f.at("sha256"), "hash")),
             "manifest", "file size or hash invalid");
      }
      need(declared.size() == entries.size(), "manifest", "undeclared files");
    } catch (...) {
      mz_zip_reader_end(&zip);
      throw;
    }
  }
  ~Archive() { mz_zip_reader_end(&zip); }
  Archive(const Archive &) = delete;
  Archive &operator=(const Archive &) = delete;
  std::string extract(const std::string &n) {
    const auto i = entries.find(n);
    need(i != entries.end(), n, "missing pack entry");
    mz_zip_archive_file_stat s{};
    need(mz_zip_reader_file_stat(&zip, i->second, &s), n, "invalid entry");
    std::string out(std::size_t(s.m_uncomp_size), '\0');
    need(mz_zip_reader_extract_to_mem(&zip, i->second, out.data(), out.size(),
                                      0),
         n, "ZIP checksum mismatch");
    return out;
  }
  void extractFile(const std::string &n, const std::string &path) {
    need(entries.count(n), n, "missing entry");
    const auto tmp = path + ".tmp";
    if (!mz_zip_reader_extract_to_file(&zip, entries.at(n), tmp.c_str(), 0)) {
      std::remove(tmp.c_str());
      throw std::runtime_error(n + ": ZIP checksum mismatch");
    }
    const J *record = nullptr;
    for (const auto &f : manifest.at("files").arrayValue())
      if (f.at("path").string() == n)
        record = &f;
    need(record, n, "undeclared file");
    std::ifstream in(tmp, std::ios::binary);
    Sha256 hash;
    std::array<char, 65536> data{};
    while (in) {
      in.read(data.data(), data.size());
      hash.update(data.data(), std::size_t(in.gcount()));
    }
    if (!in.eof() || hash.finish() != record->at("sha256").string()) {
      std::remove(tmp.c_str());
      throw std::runtime_error(n + ": file hash mismatch");
    }
    need(std::rename(tmp.c_str(), path.c_str()) == 0, path,
         "cannot publish file");
  }
};
} // namespace s3g_thru_pack
