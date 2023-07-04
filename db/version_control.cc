#include <util/random.h>
#include <random>
#include "version_control.h"
#include "filename.h"
#include "log_reader.h"
#include "table/merger.h"
#include "index/btree_index.h"
#include "index/ff_btree_iterator.h"
#include "index/ff_btree.h"
#include "db_impl.h"


namespace leveldb {

// Builder class

class VersionControl::Builder {
  std::vector<std::shared_ptr<FileMetaData>> added_files_;
  std::set<uint64_t> deleted_files_;
  std::unordered_map<uint64_t, uint64_t> dead_key_counter_;
  VersionControl* vcontrol_;
  Version* base_;
 public:

  Builder(VersionControl* vcontrol, Version* base)
      : vcontrol_(vcontrol), base_(base) {
    base_->Ref();
  }

  ~Builder() {
    base_->Unref();
  }

  void Apply(VersionEdit* edit) {
    for (const auto& iter : edit->deleted_files_) {
      deleted_files_.insert(iter);
    }
    for (const auto& iter : edit->dead_key_counter_) {
      dead_key_counter_.insert({iter.first, iter.second});
    }
    for (const auto& iter : edit->new_files_) {
      std::shared_ptr<FileMetaData> f = std::make_shared<FileMetaData>();
      f->number = iter.number;
      f->file_size = iter.file_size;
      f->total = iter.total;
      f->alive = iter.alive;
      f->smallest = iter.smallest;
      f->largest = iter.largest;
      deleted_files_.erase(f->number);
      f->allowed_seeks = (f->file_size / 2048);
      if (f->allowed_seeks < 100) f->allowed_seeks = 100;
      added_files_.push_back(f);
    }
  }

  void SaveTo(Version* v, int threshold, bool is_major) {
    for (const auto& iter : base_->files_) {
      assert(iter.first == iter.second->number);
      auto f = iter.second;
      if (deleted_files_.count(iter.first) <= 0) { // do not add if got deleted
        uint64_t dead = 0;
        try {
          dead = dead_key_counter_.at(f->number);
        } catch (std::exception& e) { }
        if (f->alive > dead) {
          f->alive -= dead;
          if (100 * f->alive / f->total <= threshold) { // move to compaction list
            v->AddCompactionFile(f);
            vcontrol_->state_change_ = true;
            v->SetFileCheckCount(f, -1);
          } else {
            v->AddFile(f);
//            int cnt = is_major ? 0 : base_->file_check_count_[f->number];
            int cnt = base_->file_check_count_[f->number];
            // clear the check count after major compaction
            v->SetFileCheckCount(f, cnt);
          }
        }
      }
    }

    for (const auto& iter : base_->merge_candidates_) {
      assert(iter.first == iter.second->number);
      auto f = iter.second;
      if (deleted_files_.count(iter.first) <= 0) { // do not add if got deleted
//        uint64_t dead = 0;
        v->AddCompactionFile(f);
        v->SetFileCheckCount(f, -1);
//        try {
//          dead = dead_key_counter_.at(f->number);
//        } catch (std::exception& e) { }
//        if (f->alive > dead) {
//            // not understand ??
//          f->alive -= dead;
//          v->AddCompactionFile(f);
//          v->SetFileCheckCount(f, -1);
//        }
      }
    }
    for (const auto& f : added_files_) {
      assert(dead_key_counter_.count(f->number) >= 0);
      v->AddFile(f);
      v->SetFileCheckCount(f, 0);
    }

    for (const auto& f : base_->files_to_compact_) {
        assert(deleted_files_.count(f->number) >= 0);
    }
  }

};

// Compaction class

Compaction::~Compaction() {
  if (input_version_ != nullptr) {
    input_version_->Unref();
  }
}

void Compaction::AddInputDeletions(VersionEdit* edit) {
  for (const auto& f : inputs_) {
    edit->DeleteFile(f->number);
  }
}


void Compaction::ReleaseInputs() {
  if (input_version_ != nullptr) {
    input_version_->Unref();
    input_version_ = nullptr;
  }
}

void Compaction::ReleaseFiles() {
  inputs_.clear();
}

bool Compaction::IsInput(uint64_t num) {
  for (const auto& f : inputs_) {
    if (f->number == num) {
      return true;
    }
  }
  return false;
}

// Version Control class

VersionControl::VersionControl(DBImpl* db,
                               const std::string& dbname,
                               const Options* options,
                               TableCache* table_cache,
                               const InternalKeyComparator* cmp)
    : env_(options->env),
      db_(db),
      dbname_(dbname),
      options_(options),
      table_cache_(table_cache),
      icmp_(*cmp),
      next_file_number_(2),
      manifest_file_number_(0),
      last_sequence_(0),
      log_number_(0),
      prev_log_number_(0),
      gen(rd()),
      distribution(0, INT_MAX),
      locality_check_key(0),
      state_change_(false),
      descriptor_file_(nullptr),
      descriptor_log_(nullptr),
      current_(nullptr) {
  AppendVersion(new Version(this));
}

VersionControl::~VersionControl() {
  delete descriptor_log_;
  delete descriptor_file_;
  current_->Unref();
}

void VersionControl::AppendVersion(Version* v) {
  assert(v->refs_ == 0);
  assert(v != current_);
  if (current_ != nullptr) {
		db_->pm_root_->current = v;
		clflush((char*)(db_->pm_root_->current), sizeof(void*));
    current_->Unref();
  }
  current_ = v;
  v->Ref();
}

void VersionControl::Finalize(Version *v) {
    for (int i = 0; i < config::MaxMergeCandidateCheckRound; ++i) {
        PickCandidates(v);
        CalculateScore(v);
        if (v->files_to_compact_.size() >= 2){
//            std::cout<< v->files_to_compact_.size() << "is pick to compactee\n";
            break;
        }
        v->files_to_compact_.clear();
        EvictCandidates(v);
    }
    EvictAll(v);
}

Status VersionControl::Recover(bool* save_manifest) {
  struct LogReporter : public log::Reader::Reporter {
    Status* status;
    virtual void Corruption(size_t bytes, const Status& s) {
      if (this->status->ok()) *this->status = s;
    }
  };

  std::string current;
  Status s = ReadFileToString(env_, CurrentFileName(dbname_), &current);
  if (!s.ok()) {
    return s;
  }
  if (current.empty() || current[current.size()-1] != '\n') {
    return Status::Corruption("CURRENT file does not end with newline");
  }
  current.resize(current.size() - 1);

  std::string dscname = dbname_ + "/" + current;
  SequentialFile* file;
  s = env_->NewSequentialFile(dscname, &file);
  if (!s.ok()) {
    if (s.IsNotFound()) {
      return Status::Corruption(
          "CURRENT points to a non-existent file", s.ToString());
    }
    return s;
  }

  bool have_log_number = false;
  bool have_prev_log_number = false;
  bool have_next_file = false;
  bool have_last_sequence = false;
  uint64_t next_file = 0;
  uint64_t last_sequence = 0;
  uint64_t log_number = 0;
  uint64_t prev_log_number = 0;
  Builder builder(this, current_);

  {
    LogReporter reporter;
    reporter.status = &s;
    log::Reader reader(file, &reporter, true, 0);
    Slice record;
    std::string scratch;
    while (reader.ReadRecord(&record, &scratch) && s.ok()) {
      VersionEdit edit;
      s = edit.DecodeFrom(record);
      if (s.ok()) {
        if (edit.HasComparatorName() &&
            edit.GetComparatorName() != icmp_.user_comparator()->Name()) {
          s = Status::InvalidArgument(
              {edit.GetComparatorName() + " does not match existing comparator "},
              {icmp_.user_comparator()->Name()});
        }
      }

      if (s.ok()) {
        builder.Apply(&edit);
      }

      if (edit.HasLogNumber()) {
        log_number = edit.GetLogNumber();
        have_log_number = true;
      }

      if (edit.HasPrevLogNumber()) {
        prev_log_number = edit.GetPrevLogNumber();
        have_prev_log_number = true;
      }

      if (edit.HasNextFileNumber()) {
        next_file = edit.GetNextFile();
        have_next_file = true;
      }

      if (edit.HasLastSequence()) {
        last_sequence = edit.GetLastSequence();
        have_last_sequence = true;
      }
    }
  }

  delete file;
  file = nullptr;

  if (s.ok()) {
    if (!have_next_file) {
      s = Status::Corruption("no meta-nextfile entry in descriptor");
    } else if (!have_log_number) {
      s = Status::Corruption("no meta-lognumber entry in descriptor");
    } else if (!have_last_sequence) {
      s = Status::Corruption("no last-sequence-number entry in descriptor");
    }

    if (!have_prev_log_number) {
      prev_log_number = 0;
    }

    MarkFileNumberUsed(prev_log_number);
    MarkFileNumberUsed(log_number);
  }

  if (s.ok()) {
    Version* v = new Version(this);
    builder.SaveTo(v, options_->merge_threshold, false);
    Finalize(v);
    AppendVersion(v);
    manifest_file_number_ = next_file;
    next_file_number_ = next_file + 1;
    last_sequence_ = last_sequence;
    log_number_ = log_number;
    prev_log_number_ = prev_log_number;

    if (ReuseManifest(dscname, current)) {
    } else {
      *save_manifest = true;
    }
  }
  return s;
}

bool VersionControl::ReuseManifest(const std::string& dscname,
                                   const std::string& dscbase) {
  if (!options_->reuse_logs) {
    return false;
  }
  // Skip it, don't need to reuse manifest
}

Status VersionControl::LogAndApply(VersionEdit* edit, port::Mutex* mu, bool major) {
  if (edit->HasLogNumber()) {
    assert(edit->GetLogNumber() >= log_number_);
    assert(edit->GetLogNumber() < next_file_number_);
  } else {
    edit->SetLogNumber(log_number_);
  }
  if (!edit->HasPrevLogNumber()) {
    edit->SetPrevLogNumber(prev_log_number_);
  }
  edit->SetNextFile(next_file_number_);
  edit->SetLastSequence(last_sequence_);

  Version* v = new Version(this);
  {
    Builder builder(this, current_);
    edit->Wait();
    builder.Apply(edit);
    builder.SaveTo(v, options_->merge_threshold, major);
  }
  Finalize(v);



  std::string new_manifest_file;
  Status s;
  if (descriptor_log_ == nullptr) {
    assert(descriptor_file_ == nullptr);
    new_manifest_file = DescriptorFileName(dbname_, manifest_file_number_);
    edit->SetNextFile(next_file_number_);
    s = env_->NewWritableFile(new_manifest_file, &descriptor_file_);
    if (s.ok()) {
      descriptor_log_ = new log::Writer(descriptor_file_);
      s = WriteSnapshot(descriptor_log_);
    }
  }

  {
    mu->Unlock();
    if (s.ok()) {
      std::string record;
      edit->EncodeTo(&record);
      s = descriptor_log_->AddRecord(record);
      if (s.ok()) {
        s = descriptor_file_->Sync();
      }
      if (!s.ok()) {
        Log(options_->info_log, "MANIFEST write: %s\n", s.ToString().c_str());
      }
    }
    if (s.ok() && !new_manifest_file.empty()) {
      s = SetCurrentFile(env_, dbname_, manifest_file_number_);
    }
    mu->Lock();
  }

  if (s.ok()) {
    // append version
    AppendVersion(v);
    log_number_ = edit->GetLogNumber();
    prev_log_number_ = edit->GetPrevLogNumber();
  } else {
    v->Unref();
    if (!new_manifest_file.empty()) {
      delete descriptor_log_;
      delete descriptor_file_;
      descriptor_log_ = nullptr;
      descriptor_file_ = nullptr;
      env_->DeleteFile(new_manifest_file);
    }
  }
  std::string msg;
  for (const auto& f : current()->merge_candidates_) {
    msg.append(" ").append(std::to_string(f.first));
  }
  Log(options_->info_log, "Merge candidates left [%s]", msg.c_str());
  return s;
}

void VersionControl::RegisterFileAccess(const uint16_t& file_number) {
  if (file_number == 0) return;
  std::shared_ptr<FileMetaData> file_metadata;
  try {
    file_metadata = current_->files_.at(file_number);
    file_metadata->allowed_seeks--;
  } catch (std::exception& e) {
//    try {
//      file_metadata = current_->merge_candidates_.at(file_number);
//      file_metadata->allowed_seeks = 0;
//    } catch (std::exception& e) {
//      // File is generated and keys are added to index, but version is not committed to state
//      return;
//    }
    return;
    }
  if (file_metadata->allowed_seeks <= 0) {
    state_change_ = true;
    current_->MoveToMerge({file_number}, false);
    Log(options_->info_log, "File %d got exceeded access number", file_number);
    db_->MaybeScheduleCompaction();
  }
}

void VersionControl::EvictCandidates(Version* v) {
//    std::cout<<"****************** start evict *********************************\n";
//    std::cout << "before evict: " << v->merge_candidates_.size() << "items in mergecandidate.\n";
    Log(options_->info_log, "Evict compaction candidates");
    default_random_engine e(time(0));
    std::unordered_map<uint64_t, std::shared_ptr<FileMetaData>> tmp;
    bool stay;
    for (auto & merge_candidate : v->merge_candidates_) {
        uint64_t file_number = merge_candidate.first;
        int cnt = v->file_check_count_[file_number];
        if (cnt == - 1 || e() % (cnt + 1) == 0){
            stay = true;
        }else{
            stay = false;
        }
        if (stay) {
            tmp.insert(std::pair<uint64_t, std::shared_ptr<FileMetaData>>(merge_candidate.first,merge_candidate.second));
        }
    }

    v->merge_candidates_.swap(tmp);

//    std::cout << "after evict: " << v->merge_candidates_.size() << "items in mergecandidate.\n";
//    std::cout<<"****************** evict done *********************************\n";
}

void VersionControl::EvictAll(Version *v) {
    Log(options_->info_log, "Evict compaction candidates");
    std::unordered_map<uint64_t, std::shared_ptr<FileMetaData>> tmp;
    bool stay;
    for (auto & merge_candidate : v->merge_candidates_) {
        uint64_t file_number = merge_candidate.first;
        int cnt = v->file_check_count_[file_number];
        if (cnt == - 1){
            stay = true;
        }else{
            stay = false;
        }
        if (stay) {
            tmp.insert(std::pair<uint64_t, std::shared_ptr<FileMetaData>>(merge_candidate.first,merge_candidate.second));
        }else{
            if (v->files_.count(file_number)!=0){
                printf("Dup ERROR!");
                exit(-1);
            }
            v->files_.insert(std::pair<uint64_t, std::shared_ptr<FileMetaData>>(merge_candidate.first,merge_candidate.second));
        }
    }

    v->merge_candidates_.swap(tmp);
}

void VersionControl::PickCandidates(Version * v) {
    Log(options_->info_log, "Move to compaction candidate");
    if (v->files_.size() <  config::StartCompactionCheckTrigger) return;
    if (v->merge_candidates_.size() >= config::StopWritesTrigger) {
        Log(options_->info_log, "Too many files... skip moving files to compaction candidate");
        return;
    }

    std::set<uint32_t> files_;
    int count = config::FilesToPickAtOneRound - v->merge_candidates_.size();
    if (count > v->files_.size()){
        count = (int )(config::PickRatio * (double )v->files_.size());
    }

    // start pick process
    default_random_engine e(time(0));
    int i = 0;
//    std::cout<<"start to pick " << count << "files in " << config::MaxPickTimes << " rounds\n";
    for (auto p = v->files_.begin(); files_.size() < count && p != v->files_.end() && i < config::MaxPickTimes; p++, i++) {
        uint64_t file_number = p->first;
        // there should be no shared files in `files_` and `merge_candidates_`
        assert(v->merge_candidates_.find(file_number) == v->merge_candidates_.end());
//        if (v->merge_candidates_.find(file_number) != v->merge_candidates_.end()){
//            continue;
//        }
        assert(v->file_check_count_.find(file_number) != v->file_check_count_.end());
        int cnt = v->file_check_count_[file_number];
        if (e() % (cnt + 1) == 0){
            //picked, insert into tmp files
            if (v->file_check_count_[file_number] == config::MaxRoundNumberForPerFile){
                // reset the number to 1 if reaches max
                v->file_check_count_[file_number] = 1;
            }else{
                //
                v->file_check_count_[file_number]++;
            }
            files_.insert(file_number);
        }
        if (p == v->files_.end()){
            p = v->files_.begin();
        }
    }

//    std::cout << files_.size()<< " files picked!" <<std::endl;
    if (!files_.empty()){
        v->MoveToMerge(files_, false);
    }
}

Compaction* VersionControl::PickCompaction() {
  // get compaction and return
  if (current_->merge_candidates_.size() <= 1) return nullptr;
  state_change_ = true;
  Compaction* c = new Compaction(options_);
//  PickCandidates();
    if (current_->files_to_compact_.size() >  1){
        for (auto & f: current_->files_to_compact_) {
//            std::cout<< f->number << "sent to compaction\n";
            c->AddInput(f);
        }
    }
//  TryToPick(&c);
  if (c->num_input_files() <= 1) {
    c->ReleaseFiles();
    if (current_->merge_candidates_.size() >= config::StopWritesTrigger) {
        // so here we can see that if mc's size is ge stop writer trigger, they will force to pick
        // we should not force pick!
      ForcedPick(&c);
    }
  }
  if (c->num_input_files() <= 1) {
    state_change_ = false;
    Log(options_->info_log, "No compaction candidates were picked");
    // evict files if failed try to pick
//    current_->Evict();
    delete c;
    return nullptr;
  }
  Log(options_->info_log, "Compact %zu candidates for merge", c->num_input_files());
  std::string msg;
  for (int i = 0; i < c->num_input_files(); i++) {
    msg.append(std::to_string(c->input(i)->number));
    msg.append(" ");
  }
  Log(options_->info_log, "Picking files for merge %s", msg.c_str());
  return c;
}

void VersionControl::ForcedPick(Compaction** c) {
  Log(options_->info_log, "Forced compaction");
  for (auto iter = current_->merge_candidates_.begin(); iter != current_->merge_candidates_.end() &&
      (*c)->num_input_files() <= options_->forced_compaction_size;
    iter++) {
      (*c)->AddInput(iter->second);
  }
}

void VersionControl::CalculateScore(Version* v) {
//    std::cout << "start to calc score in" << v->merge_candidates_.size() << "files\n";
    std::vector<std::pair<double, std::shared_ptr<FileMetaData>>> best_pick_list;
    best_pick_list.reserve(v->merge_candidates_.size());
    double best_pick_score = 0.0;
    for (const auto& main_candidate : v->merge_candidates_) {
        std::vector<std::pair<double, std::shared_ptr<FileMetaData>>> current_pick_list;
        current_pick_list.reserve(v->merge_candidates_.size());
        double current_pick_score = 0.0;
        double smallest1 = fast_atoi(main_candidate.second->smallest.user_key());
        double largest1 = fast_atoi(main_candidate.second->largest.user_key());
        for (const auto& next_candidate : v->merge_candidates_) {
            double smallest2 = fast_atoi(next_candidate.second->smallest.user_key());
            double largest2 = fast_atoi(next_candidate.second->largest.user_key());
            if (largest2 < smallest1 || smallest2 > largest1) {
                continue; // skip
            } else {
                double score = (min(largest1, largest2) - max(smallest1, smallest2))/(max(largest1, largest2) - min(smallest1, smallest2));
                current_pick_score += score;
                current_pick_list.emplace_back(score, next_candidate.second);
            }
        }
        if (current_pick_score > best_pick_score) {
            best_pick_list.swap(current_pick_list);
            best_pick_score = current_pick_score;
        }
    }
    std::sort(best_pick_list.begin(), best_pick_list.end(), [](const auto& a, const auto& b) {
        return a.first > b.first;
    });
    double threshold = state_change_ ? config::OverlapRatioThreshold : 0.0;
//    std::cout << "end calc score in" << v->merge_candidates_.size() << "files\n";
    for (const auto& iter : best_pick_list) {
        // question: do we need to move compactee files from merge candidates?
        v->files_to_compact_.push_back(iter.second);
        if (iter.first <= threshold || v->files_to_compact_.size() >= config::CompactCandidateStopWriteTrigger) {
            break;
        }
    }
}

void VersionControl::TryToPick(Compaction** c) {
  std::vector<std::pair<double, std::shared_ptr<FileMetaData>>> best_pick_list;
  best_pick_list.reserve(current()->merge_candidates_.size());
  double best_pick_score = 0.0;
  for (const auto& main_candidate : current()->merge_candidates_) {
    std::vector<std::pair<double, std::shared_ptr<FileMetaData>>> current_pick_list;
    current_pick_list.reserve(current()->merge_candidates_.size());
    double current_pick_score = 0.0;
    double smallest1 = fast_atoi(main_candidate.second->smallest.user_key());
    double largest1 = fast_atoi(main_candidate.second->largest.user_key());
    for (const auto& next_candidate : current()->merge_candidates_) {
      double smallest2 = fast_atoi(next_candidate.second->smallest.user_key());
      double largest2 = fast_atoi(next_candidate.second->largest.user_key());
      if (largest2 < smallest1 || smallest2 > largest1) {
        continue; // skip
      } else {
        double score = (min(largest1, largest2) - max(smallest1, smallest2))/(max(largest1, largest2) - min(smallest1, smallest2));
        current_pick_score += score;
        current_pick_list.emplace_back(score, next_candidate.second);
      }
    }
    if (current_pick_score > best_pick_score) {
      best_pick_list.swap(current_pick_list);
      best_pick_score = current_pick_score;
    }
  }

  std::sort(best_pick_list.begin(), best_pick_list.end(), [](const auto& a, const auto& b) {
    return a.first > b.first;
  });

  double threshold = state_change_ ? config::OverlapRatioThreshold : 0.0;
  for (const auto& iter : best_pick_list) {
    (*c)->AddInput(iter.second);
    if (iter.first <= threshold || (*c)->num_input_files() >= config::CompactionTrigger) {
      break;
    }
  }
//  printf("finish\n");
}

bool VersionControl::NeedsCompaction() const {
  // decide whether it needed or not looking for current version
  return current_->files_to_compact_.size() > config::CompactCandidateStopWriteTrigger || (current_->merge_candidates_.size() > config::StopWritesTrigger && state_change_);
}

Iterator* VersionControl::MakeInputIterator(Compaction* c) {
  ReadOptions options;
  options.verify_checksums = options_->paranoid_checks;
  options.fill_cache = false;

  Iterator** list = new Iterator*[c->num_input_files()];
  for (size_t i = 0; i < c->num_input_files(); i++) {
    list[i] = table_cache_->NewIterator(options, c->input(i)->number, c->input(i)->file_size);
  }
  Iterator* result = NewMergingIterator(&icmp_, list, c->num_input_files());
  delete[] list;
  return result;
}

void VersionControl::MarkFileNumberUsed(uint64_t number) {
  if (next_file_number_ <= number) {
    next_file_number_ = number + 1;
  }
}

void VersionControl::ReuseFileNumber(uint64_t file_number) {
  if (next_file_number_ == file_number + 1) {
    next_file_number_ = file_number;
  }
}

void VersionControl::SetLastSequence(uint64_t s) {
  assert(s >= last_sequence_);
  last_sequence_ = s;
}

Status VersionControl::WriteSnapshot(log::Writer* log) {
  VersionEdit edit;
  edit.SetComparatorName(icmp_.user_comparator()->Name());
  for (const auto& iter : current_->files_) {
    auto f = iter.second;
    edit.AddFile(f->number, f->file_size, f->total, f->alive, f->smallest, f->largest);
  }
  for (const auto& iter : current_->merge_candidates_) {
    auto f = iter.second;
    edit.AddMergeCandidates(f->number, f->file_size, f->total, f->alive, f->smallest, f->largest);
  }
  std::string record;
  edit.EncodeTo(&record);
  return log->AddRecord(record);
}

const char* VersionControl::Summary(SummaryStorage* scratch) const {
  snprintf(scratch->buffer, sizeof(scratch->buffer), " Regular files number %lu, Merge files number %lu", current_->NumFiles(), current_->MergeNumFiles());
  return scratch->buffer;
}

void VersionControl::StateChange() {
  state_change_ = true;
  db_->MaybeScheduleCompaction();
}

} // namespace leveldb
