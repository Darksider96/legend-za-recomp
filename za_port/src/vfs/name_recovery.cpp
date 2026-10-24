#include "vfs/name_recovery.hpp"

#include "za/parallel_for.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>

namespace za::vfs {

namespace {

using Clock = std::chrono::steady_clock;

constexpr unsigned kFilterBits = 25;  // 32 Mi bits = 4 MB; ~0,5% de falso positivo para 178 mil hashes

constexpr uint64_t Step(uint64_t hash, char c) {
    return (hash ^ static_cast<uint8_t>(c)) * kFnvPrime;
}

uint64_t Extend(uint64_t hash, std::string_view text) {
    for (char c : text) hash = Step(hash, c);
    return hash;
}

size_t FilterIndex(uint64_t hash) {
    return static_cast<size_t>((hash * 0x9E3779B97F4A7C15ULL) >> (64 - kFilterBits));
}

bool IsPathChar(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
           c == '_' || c == '-' || c == '.' || c == '/';
}

std::string_view DirOf(std::string_view path) {
    const size_t slash = path.rfind('/');
    return slash == std::string_view::npos ? std::string_view() : path.substr(0, slash);
}

std::string_view BaseOf(std::string_view path) {
    const size_t slash = path.rfind('/');
    return slash == std::string_view::npos ? path : path.substr(slash + 1);
}

// Texto apos o primeiro '.' do ultimo componente: "a/b/c.trmdl" -> "trmdl"
std::string_view ExtensionOf(std::string_view path) {
    const std::string_view base = BaseOf(path);
    const size_t dot = base.find('.');
    return dot == std::string_view::npos ? std::string_view() : base.substr(dot + 1);
}

std::string_view StemOf(std::string_view path) {
    const std::string_view base = BaseOf(path);
    return base.substr(0, base.find('.'));
}

// "arc/xxx.trmdl.trpak" -> "xxx.trmdl"
std::string_view PackStem(std::string_view pack_name) {
    if (pack_name.starts_with("arc/")) pack_name.remove_prefix(4);
    if (pack_name.ends_with(".trpak")) pack_name.remove_suffix(6);
    return pack_name;
}

std::string Join(std::string_view dir, std::string_view name) {
    std::string path;
    path.reserve(dir.size() + 1 + name.size());
    if (!dir.empty()) path.append(dir).push_back('/');
    path.append(name);
    return path;
}

// Chama on_run(texto) para cada sequencia maxima de caracteres de caminho com ao menos min_length bytes
template <typename Fn>
void ScanPathRuns(std::span<const uint8_t> data, size_t min_length, Fn&& on_run) {
    size_t start = 0;
    for (size_t i = 0; i <= data.size(); ++i) {
        if (i < data.size() && IsPathChar(static_cast<char>(data[i]))) continue;
        if (i - start >= min_length) on_run(std::string_view(reinterpret_cast<const char*>(data.data() + start), i - start));
        start = i + 1;
    }
}

long long ElapsedMs(Clock::time_point start) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start).count();
}

} // namespace

NameRecovery::NameRecovery(TrpfsArchive& archive, const PathDictionary& seed)
    : archive_(archive),
      filter_((size_t{1} << kFilterBits) / 64, 0),
      named_per_pack_(archive.GetPackCount(), 0),
      pack_resolved_(archive.GetPackCount(), 0) {
    for (const auto& entry : archive.GetEntries()) {
        const size_t index = FilterIndex(entry.hash);
        filter_[index >> 6] |= uint64_t{1} << (index & 63);
    }

    const auto& packs = archive.GetPacks();
    pack_by_stem_.reserve(packs.size());
    for (uint32_t i = 0; i < packs.size(); ++i) {
        const std::string_view stem = PackStem(packs[i].name);
        pack_by_stem_.emplace(std::string(stem), i);
        if (const std::string_view extension = ExtensionOf(stem); !extension.empty()) extensions_.emplace(extension);
    }

    for (const auto& [hash, path] : seed.Entries()) Learn(path, stats_.seed);
}

bool NameRecovery::Contains(uint64_t hash) const {
    const size_t index = FilterIndex(hash);
    return ((filter_[index >> 6] >> (index & 63)) & 1) != 0 && archive_.HasFile(hash);
}

// Extensao com cara de tipo de arquivo ("trmsh", "bntx"): nem todo tipo aparece em nome de pack
bool NameRecovery::HasPlausibleExtension(std::string_view text) {
    const std::string_view extension = ExtensionOf(text);
    if (extension.size() < 2 || extension.size() > 10) return false;
    bool letter = false;
    for (char c : extension) {
        if (c >= 'a' && c <= 'z') letter = true;
        else if (c < '0' || c > '9') return false;
    }
    return letter;
}

bool NameRecovery::Learn(std::string_view path, size_t& counter) {
    const uint64_t hash = Fnv1a64(path);
    if (result_.Find(hash) || !Contains(hash)) return false;

    result_.Add(path);
    learned_order_.emplace_back(path);
    ++counter;
    if (uint32_t index = 0; archive_.FindPackIndex(hash, index)) ++named_per_pack_[index];
    AddDirectory(DirOf(path));
    if (const std::string_view extension = ExtensionOf(path); !extension.empty()) extensions_.emplace(extension);

    std::string flat;
    flat.reserve(path.size());
    for (char c : path) {
        if (c != '/') flat.push_back(c);
    }
    if (const auto pack = pack_by_stem_.find(flat); pack != pack_by_stem_.end()) pack_resolved_[pack->second] = 1;
    return true;
}

void NameRecovery::AddDirectory(std::string_view dir) {
    while (!dir.empty()) {
        if (!directory_set_.emplace(dir).second) return;  // prefixos ja foram adicionados junto
        directories_.emplace_back(dir);
        directory_states_.push_back(Fnv1a64(dir));
        vocabulary_.insert(Fnv1a64(BaseOf(dir)));
        dir = DirOf(dir);
    }
}

void NameRecovery::AddSeedString(std::string_view text) {
    while (text.starts_with("./")) text.remove_prefix(2);
    while (text.starts_with('/')) text.remove_prefix(1);
    if (text.empty()) return;

    // Raiz usada em strings de formato ("ik_chara/%s/%s.trmdl" vira "ik_chara/")
    if (text.ends_with('/')) {
        while (text.ends_with('/')) text.remove_suffix(1);
        if (text.find('.') == std::string_view::npos) AddDirectory(text);
        return;
    }

    const std::string_view extension = ExtensionOf(text);
    if (extension.empty() || extension.size() > 12 || BaseOf(text).starts_with('.')) return;

    // Componentes de caminhos relativos orientam a busca nos nomes de pack
    for (std::string_view dir = DirOf(text); !dir.empty(); dir = DirOf(dir)) vocabulary_.insert(Fnv1a64(BaseOf(dir)));
    if (string_set_.emplace(text).second) strings_.emplace_back(text);
}

void NameRecovery::AddSeedBinary(std::span<const uint8_t> data) {
    const size_t strings_before = strings_.size();
    const size_t directories_before = directories_.size();
    ScanPathRuns(data, 4, [this](std::string_view run) { AddSeedString(run); });
    std::cout << "[TRPFS] Sementes do binario: +" << (strings_.size() - strings_before) << " strings, +"
              << (directories_.size() - directories_before) << " diretorios" << std::endl;
}

std::optional<std::string> NameRecovery::ResolvePackStem(std::string_view stem, int max_free_slashes) const {
    // Diretorios nao tem '.', entao todas as barras ficam antes do primeiro ponto do nome
    const size_t dot = stem.find('.');
    if (dot == std::string_view::npos || dot == 0) return std::nullopt;

    std::vector<size_t> cuts;
    std::optional<std::string> found;

    // state: FNV de stem[0, start) com as barras ja inseridas
    const auto search = [&](const auto& self, size_t start, uint64_t state, int budget) -> bool {
        if (Contains(Extend(state, stem.substr(start)))) {
            std::string path;
            size_t previous = 0;
            for (size_t cut : cuts) {
                path.append(stem.substr(previous, cut - previous)).push_back('/');
                previous = cut;
            }
            path.append(stem.substr(previous));
            found = std::move(path);
            return true;
        }

        uint64_t running = state;
        uint64_t component = kFnvOffsetBasis;
        for (size_t cut = start + 1; cut < dot; ++cut) {
            running = Step(running, stem[cut - 1]);
            component = Step(component, stem[cut - 1]);
            const bool known = vocabulary_.contains(component);
            if (!known && budget == 0) continue;
            cuts.push_back(cut);
            if (self(self, cut, Step(running, '/'), known ? budget : budget - 1)) return true;
            cuts.pop_back();
        }
        return false;
    };

    search(search, 0, kFnvOffsetBasis, max_free_slashes);
    return found;
}

size_t NameRecovery::RecoverFromStrings(unsigned threads) {
    const size_t string_count = strings_.size();
    const size_t directory_count = directories_.size();
    if (string_count == tested_strings_ && directory_count == tested_directories_) return 0;

    const auto start = Clock::now();
    std::vector<std::vector<std::string>> found(threads);
    ParallelFor(string_count, threads, [&](size_t i, unsigned thread) {
        const std::string& text = strings_[i];
        const bool fresh = i >= tested_strings_;
        if (fresh) {
            const uint64_t hash = Fnv1a64(text);
            if (Contains(hash) && !result_.Find(hash)) found[thread].push_back(text);
        }
        // strings novas contra todos os diretorios; antigas apenas contra os diretorios novos
        for (size_t d = fresh ? 0 : tested_directories_; d < directory_count; ++d) {
            const uint64_t hash = Extend(Step(directory_states_[d], '/'), text);
            if (Contains(hash) && !result_.Find(hash)) found[thread].push_back(Join(directories_[d], text));
        }
    });
    tested_strings_ = string_count;
    tested_directories_ = directory_count;

    size_t learned = 0;
    for (const auto& list : found) {
        for (const std::string& path : list) learned += Learn(path, stats_.from_strings) ? 1 : 0;
    }
    std::cout << "[TRPFS]   strings x diretorios: +" << learned << " (" << string_count << " strings, "
              << directory_count << " diretorios) em " << ElapsedMs(start) << " ms" << std::endl;
    return learned;
}

size_t NameRecovery::RecoverFromPackNames(int max_free_slashes, unsigned threads) {
    if (vocabulary_.size() == vocabulary_at_last_pack_pass_) return 0;

    const auto& packs = archive_.GetPacks();
    size_t learned_total = 0;
    for (int budget = 0; budget <= max_free_slashes; ++budget) {
        for (;;) {
            std::vector<uint32_t> pending;
            for (uint32_t i = 0; i < packs.size(); ++i) {
                if (!pack_resolved_[i]) pending.push_back(i);
            }
            if (pending.empty()) break;

            const auto start = Clock::now();
            std::vector<std::optional<std::string>> found(pending.size());
            ParallelFor(pending.size(), threads, [&](size_t k, unsigned) {
                found[k] = ResolvePackStem(PackStem(packs[pending[k]].name), budget);
            });

            size_t resolved = 0, learned = 0;
            for (size_t k = 0; k < pending.size(); ++k) {
                if (!found[k]) continue;
                pack_resolved_[pending[k]] = 1;
                ++resolved;
                learned += Learn(*found[k], stats_.from_packs) ? 1 : 0;
            }
            std::cout << "[TRPFS]   nomes de pack (ate " << budget << " barras livres): " << resolved << " de "
                      << pending.size() << " resolvidos, +" << learned << " nomes em " << ElapsedMs(start) << " ms"
                      << std::endl;
            learned_total += learned;
            if (learned == 0) break;  // vocabulario nao mudou: repetir o passe nao acha nada novo
        }
    }
    vocabulary_at_last_pack_pass_ = vocabulary_.size();
    return learned_total;
}

size_t NameRecovery::RecoverSiblings(unsigned threads) {
    if (result_.Size() == names_at_last_sibling_pass_) return 0;

    const auto start = Clock::now();
    const auto& packs = archive_.GetPacks();
    std::vector<std::vector<std::string_view>> names_by_pack(packs.size());
    for (const auto& [hash, path] : result_.Entries()) {
        uint32_t index = 0;
        if (archive_.FindPackIndex(hash, index)) names_by_pack[index].push_back(path);
    }

    std::vector<uint32_t> work;
    for (uint32_t i = 0; i < packs.size(); ++i) {
        if (!names_by_pack[i].empty() && names_by_pack[i].size() < packs[i].file_count) work.push_back(i);
    }

    const std::vector<std::string> extensions(extensions_.begin(), extensions_.end());
    std::vector<std::vector<std::string>> found(threads);
    ParallelFor(work.size(), threads, [&](size_t k, unsigned thread) {
        std::vector<std::string_view> dirs, stems;
        const auto add = [](std::vector<std::string_view>& list, std::string_view value) {
            if (!value.empty() && std::find(list.begin(), list.end(), value) == list.end()) list.push_back(value);
        };
        for (std::string_view path : names_by_pack[work[k]]) {
            const std::string_view dir = DirOf(path);
            const std::string_view stem = StemOf(path);
            add(dirs, dir);
            add(stems, stem);
            add(stems, BaseOf(dir));
            for (size_t cut = stem.find('_'); cut != std::string_view::npos; cut = stem.find('_', cut + 1)) {
                add(stems, stem.substr(0, cut));
            }
        }

        for (std::string_view dir : dirs) {
            const uint64_t dir_state = Step(Extend(kFnvOffsetBasis, dir), '/');
            for (std::string_view stem : stems) {
                const uint64_t stem_state = Step(Extend(dir_state, stem), '.');
                for (const std::string& extension : extensions) {
                    const uint64_t hash = Extend(stem_state, extension);
                    if (Contains(hash) && !result_.Find(hash)) {
                        found[thread].push_back(Join(dir, std::string(stem) + "." + extension));
                    }
                }
            }
        }
    });

    size_t learned = 0;
    for (const auto& list : found) {
        for (const std::string& path : list) learned += Learn(path, stats_.from_siblings) ? 1 : 0;
    }
    names_at_last_sibling_pass_ = result_.Size();
    std::cout << "[TRPFS]   irmaos no mesmo pack: +" << learned << " (" << work.size() << " packs) em "
              << ElapsedMs(start) << " ms" << std::endl;
    return learned;
}

size_t NameRecovery::RecoverNumericVariants(unsigned threads) {
    const size_t end = learned_order_.size();
    if (numeric_cursor_ >= end) return 0;

    // Apenas nomes novos, e so de packs que ainda tem arquivos sem nome
    const auto start = Clock::now();
    const auto& packs = archive_.GetPacks();
    std::vector<size_t> work;
    for (size_t i = numeric_cursor_; i < end; ++i) {
        uint32_t index = 0;
        if (archive_.FindPackIndex(Fnv1a64(learned_order_[i]), index) && named_per_pack_[index] < packs[index].file_count) {
            work.push_back(i);
        }
    }
    numeric_cursor_ = end;

    std::vector<std::vector<std::string>> found(threads);
    ParallelFor(work.size(), threads, [&](size_t k, unsigned thread) {
        const std::string_view path = learned_order_[work[k]];
        const size_t slash = path.rfind('/');
        for (size_t run = slash == std::string_view::npos ? 0 : slash + 1; run < path.size();) {
            if (path[run] < '0' || path[run] > '9') {
                ++run;
                continue;
            }
            size_t run_end = run;
            while (run_end < path.size() && path[run_end] >= '0' && path[run_end] <= '9') ++run_end;
            const size_t width = run_end - run;

            if (width <= 4) {
                const uint64_t prefix = Extend(kFnvOffsetBasis, path.substr(0, run));
                const std::string_view suffix = path.substr(run_end);
                char digits[4] = {'0', '0', '0', '0'};
                uint32_t limit = 1;
                for (size_t i = 0; i < width; ++i) limit *= 10;

                for (uint32_t value = 0; value < limit; ++value) {
                    if (path.compare(run, width, std::string_view(digits, width)) != 0) {
                        uint64_t hash = prefix;
                        for (size_t i = 0; i < width; ++i) hash = Step(hash, digits[i]);
                        hash = Extend(hash, suffix);
                        if (Contains(hash) && !result_.Find(hash)) {
                            std::string variant(path);
                            variant.replace(run, width, digits, width);
                            found[thread].push_back(std::move(variant));
                        }
                    }
                    for (size_t i = width; i-- > 0;) {  // odometro decimal
                        if (++digits[i] <= '9') break;
                        digits[i] = '0';
                    }
                }
            }
            run = run_end;
        }
    });

    size_t learned = 0;
    for (const auto& list : found) {
        for (const std::string& path : list) learned += Learn(path, stats_.from_numeric) ? 1 : 0;
    }
    std::cout << "[TRPFS]   variacoes numericas: +" << learned << " (" << work.size() << " nomes base) em "
              << ElapsedMs(start) << " ms" << std::endl;
    return learned;
}

size_t NameRecovery::RecoverFromContent(unsigned threads) {
    const auto start = Clock::now();
    const auto& packs = archive_.GetPacks();

    std::vector<std::vector<uint64_t>> files_by_pack(packs.size());
    for (const auto& entry : archive_.GetEntries()) files_by_pack[entry.pack_index].push_back(entry.hash);

    // Diretorios candidatos por pack: dos nomes conhecidos e seus pais
    std::vector<std::vector<std::string_view>> dirs_by_pack(packs.size());
    for (const auto& [hash, path] : result_.Entries()) {
        uint32_t index = 0;
        if (!archive_.FindPackIndex(hash, index)) continue;
        auto& dirs = dirs_by_pack[index];
        for (std::string_view dir = DirOf(path); !dir.empty(); dir = DirOf(dir)) {
            if (std::find(dirs.begin(), dirs.end(), dir) == dirs.end()) dirs.push_back(dir);
        }
    }

    const size_t cache_budget = archive_.GetPackCacheBudget();
    archive_.SetPackCacheBudget(std::max<size_t>(cache_budget, size_t{512} << 20));

    std::atomic<size_t> packs_done{0};
    std::atomic<uint64_t> bytes_scanned{0};
    std::mutex log_mutex;
    std::vector<std::vector<std::string>> found(threads);
    std::vector<std::vector<std::string>> leftovers(threads);  // sem match local: vao para o cruzamento global
    ParallelFor(packs.size(), threads, [&](size_t i, unsigned thread) {
        std::vector<uint8_t> data;
        std::unordered_set<std::string> strings;
        for (uint64_t hash : files_by_pack[i]) {
            if (!archive_.ReadFile(hash, data)) continue;
            bytes_scanned += data.size();
            ScanPathRuns(data, 5, [&](std::string_view run) {
                while (run.starts_with("./")) run.remove_prefix(2);
                while (run.starts_with('/')) run.remove_prefix(1);
                if (HasPlausibleExtension(run)) strings.emplace(run);
            });
        }

        const auto& dirs = dirs_by_pack[i];
        for (const std::string& text : strings) {
            const uint64_t absolute = Fnv1a64(text);
            bool matched = Contains(absolute);
            if (matched && !result_.Find(absolute)) found[thread].push_back(text);
            for (std::string_view dir : dirs) {
                const uint64_t hash = Extend(Step(Extend(kFnvOffsetBasis, dir), '/'), text);
                if (!Contains(hash)) continue;
                matched = true;
                if (!result_.Find(hash)) found[thread].push_back(Join(dir, text));
            }
            if (!matched) leftovers[thread].push_back(text);
        }

        const size_t done = ++packs_done;
        if (done % 1000 == 0 || done == packs.size()) {
            std::lock_guard lock(log_mutex);
            std::cout << "[TRPFS]   minerando conteudo: " << done << "/" << packs.size() << " packs, "
                      << (bytes_scanned.load() >> 20) << " MB descomprimidos" << std::endl;
        }
    });
    archive_.SetPackCacheBudget(cache_budget);

    size_t learned = 0;
    for (const auto& list : found) {
        for (const std::string& path : list) learned += Learn(path, stats_.from_content) ? 1 : 0;
    }
    const size_t strings_before = strings_.size();
    for (const auto& list : leftovers) {
        for (const std::string& text : list) {
            if (string_set_.insert(text).second) strings_.push_back(text);
        }
    }
    std::cout << "[TRPFS]   conteudo: +" << learned << " nomes em " << ElapsedMs(start) << " ms; "
              << (strings_.size() - strings_before) << " strings sem match local seguem para o cruzamento global"
              << std::endl;
    return learned + (strings_.size() - strings_before);
}

NameRecoveryStats NameRecovery::Run(const NameRecoveryOptions& options) {
    const unsigned threads = options.threads ? options.threads : DefaultThreadCount();
    const auto start = Clock::now();
    std::cout << "[TRPFS] Reconstruindo nomes: " << result_.Size() << " caminhos semente, " << directories_.size()
              << " diretorios, " << extensions_.size() << " extensoes, " << threads << " threads" << std::endl;

    for (int round = 1;; ++round) {
        size_t learned = RecoverFromStrings(threads);
        learned += RecoverFromPackNames(options.max_free_slashes, threads);
        if (options.siblings) learned += RecoverSiblings(threads);
        if (options.numeric) learned += RecoverNumericVariants(threads);
        std::cout << "[TRPFS] Rodada " << round << ": +" << learned << " nomes (total " << result_.Size() << ")" << std::endl;
        if (learned > 0) continue;

        if (options.mine_content && !content_mined_) {
            content_mined_ = true;
            if (RecoverFromContent(threads) > 0) continue;
        }
        break;
    }

    stats_.packs_resolved = static_cast<size_t>(std::count(pack_resolved_.begin(), pack_resolved_.end(), uint8_t{1}));
    stats_.total = result_.Size();
    stats_.seconds = static_cast<double>(ElapsedMs(start)) / 1000.0;
    std::cout << "[TRPFS] Dicionario: " << stats_.total << " caminhos de " << archive_.GetEntryCount() << " arquivos ("
              << stats_.packs_resolved << "/" << archive_.GetPackCount() << " packs nomeados) em " << stats_.seconds
              << " s" << std::endl;
    return stats_;
}

} // namespace za::vfs
