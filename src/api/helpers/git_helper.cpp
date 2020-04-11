/*  LOOT

    A load order optimisation tool for Oblivion, Skyrim, Fallout 3 and
    Fallout: New Vegas.

    Copyright (C) 2012-2016    WrinklyNinja

    This file is part of LOOT.

    LOOT is free software: you can redistribute
    it and/or modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation, either version 3 of
    the License, or (at your option) any later version.

    LOOT is distributed in the hope that it will
    be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with LOOT.  If not, see
    <https://www.gnu.org/licenses/>.
    */

#include "api/helpers/git_helper.h"

#include <iomanip>
#include <sstream>

#include <boost/lexical_cast.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include "api/helpers/logging.h"
#include "loot/exception/error_categories.h"
#include "loot/exception/git_state_error.h"

using std::string;

namespace fs = std::filesystem;

namespace loot {
GitHelper::GitData::GitData() :
    repo(nullptr),
    remote(nullptr),
    config(nullptr),
    object(nullptr),
    commit(nullptr),
    reference(nullptr),
    reference2(nullptr),
    blob(nullptr),
    annotated_commit(nullptr),
    tree(nullptr),
    diff(nullptr),
    buffer({0}) {
  // Init threading system and OpenSSL (for Linux builds).
  git_libgit2_init();

  checkout_options = GIT_CHECKOUT_OPTIONS_INIT;
  clone_options = GIT_CLONE_OPTIONS_INIT;
}

GitHelper::GitData::~GitData() {
  git_commit_free(commit);
  git_object_free(object);
  git_config_free(config);
  git_remote_free(remote);
  git_repository_free(repo);
  git_reference_free(reference);
  git_reference_free(reference2);
  git_blob_free(blob);
  git_annotated_commit_free(annotated_commit);
  git_tree_free(tree);
  git_diff_free(diff);
  git_buf_free(&buffer);
  git_strarray_free(&checkout_options.paths);

  git_libgit2_shutdown();
}

void GitHelper::InitialiseOptions(const std::string& branch,
                                  const std::string& filenameToCheckout) {
  auto logger = getLogger();
  if (logger) {
    logger->debug(
        "Setting up checkout options using branch {} and filename {}.",
        branch,
        filenameToCheckout);
  }

  data_.checkout_options.checkout_strategy =
      GIT_CHECKOUT_FORCE | GIT_CHECKOUT_DONT_REMOVE_EXISTING;

  char** paths = new char*[1];
  paths[0] = new char[filenameToCheckout.length() + 1];
  strcpy(paths[0], filenameToCheckout.c_str());

  data_.checkout_options.paths.strings = paths;
  data_.checkout_options.paths.count = 1;

  // Initialise clone options.
  data_.clone_options.checkout_opts = data_.checkout_options;
  data_.clone_options.bare = 0;
  data_.clone_options.checkout_branch = branch.c_str();
}

void GitHelper::Open(const std::filesystem::path& repoRoot) {
  auto logger = getLogger();
  if (logger) {
    logger->info("Attempting to open Git repository at: {}",
                  repoRoot.u8string());
  }
  Call(git_repository_open(&data_.repo, repoRoot.u8string().c_str()));
}

void GitHelper::SetRemoteUrl(const std::string& remote,
                             const std::string& url) {
  if (data_.repo == nullptr) {
    throw GitStateError(
        "Cannot set remote URL for repository that has not been opened.");
  }

  auto logger = getLogger();
  if (logger) {
    logger->info("Setting URL for remote {} to {}", remote, url);
  }

  Call(git_remote_set_url(data_.repo, remote.c_str(), url.c_str()));
}

void GitHelper::Call(int error_code) {
  if (!error_code)
    return;

  const git_error* last_error = giterr_last();
  std::string gitError;
  if (last_error == nullptr)
    gitError = std::to_string(error_code) + ".";
  else
    gitError = std::to_string(error_code) + "; " + last_error->message;
  giterr_clear();

  auto message = "Git operation failed. Details: " + gitError;

  throw std::system_error(error_code, libgit2_category(), message);
}

bool GitHelper::IsRepository(const std::filesystem::path& path) {
  return git_repository_open_ext(NULL,
                                 path.u8string().c_str(),
                                 GIT_REPOSITORY_OPEN_NO_SEARCH,
                                 NULL) == 0;
}

int GitHelper::DiffFileCallback(const git_diff_delta* delta,
                                float progress,
                                void* payload) {
  auto logger = getLogger();
  if (logger) {
    logger->trace("Checking diff for: {}", delta->old_file.path);
  }

  DiffPayload* gdp = (DiffPayload*)payload;
  if (strcmp(delta->old_file.path, gdp->fileToFind) == 0) {
    if (logger) {
      logger->warn("Edited masterlist found.");
    }
    gdp->fileFound = true;
  }

  return 0;
}

// Clones a repository and opens it.
void GitHelper::Clone(const std::filesystem::path& path,
                      const std::string& url) {
  if (data_.repo != nullptr)
    throw GitStateError(
        "Cannot clone repository that has already been opened.");

  // Clone the remote repository.
  auto logger = getLogger();
  if (logger) {
    logger->info("Repository doesn't exist, cloning the remote repository at \"{}\".", url);
  }

  fs::create_directories(path.parent_path());

  // If the target path is not an empty directory, clone into a temporary
  // directory.
  fs::path repoPath;
  if (fs::exists(path) && !fs::is_empty(path)) {
    if (logger) {
      logger->trace("Target repo path not empty, cloning into temporary directory.");
    }
    auto directory = "LOOT-" + path.filename().u8string() + "-" +
                     boost::lexical_cast<std::string>((boost::uuids::random_generator())());
    repoPath = fs::temp_directory_path() / directory;

    // Remove path in case it already exists.
    fs::remove_all(repoPath);
  } else {
    repoPath = path;
  }

  // Perform the clone.
  Call(git_clone(&data_.repo,
                 url.c_str(),
                 repoPath.u8string().c_str(),
                 &data_.clone_options));

  // If repo was cloned into a temporary directory, move it into the target
  // path.
  if (repoPath != path) {
    // Free the repository to ensure all file handles are closed.
    git_repository_free(data_.repo);
    data_.repo = nullptr;

    if (logger) {
      logger->trace(
          "Target repo path not empty, moving cloned files in.");
    }

    std::vector<std::filesystem::path> filenamesToMove;
    for (fs::directory_iterator it(repoPath);
         it != fs::directory_iterator();
         ++it) {
      // libgit2 v0.28.1 creates a _git2_<random> symlink on clone.
      if (!it->is_regular_file() && !it->is_directory()) {
        continue;
      }

      auto targetPath = path / it->path().filename();
      if (fs::exists(targetPath)) {
        fs::remove_all(targetPath);
      }
      filenamesToMove.push_back(it->path().filename());
    }

    for (const auto& filename : filenamesToMove) {
      fs::copy(repoPath / filename, path / filename, std::filesystem::copy_options::recursive);
    }

    try {
      fs::remove_all(repoPath);
    } catch (std::exception& e) {
      if (logger) {
        logger->error(
          "Could not delete temporary repository path \"{}\": {}",
          repoPath.u8string(), e.what());
      }
    }

    // Open the repo again.
    Open(path);
  }
}

void GitHelper::Fetch(const std::string& remote) {
  if (data_.repo == nullptr)
    throw GitStateError(
        "Cannot fetch updates for repository that has not been opened.");

  auto logger = getLogger();
  if (logger) {
    logger->trace("Fetching updates from remote.");
  }

  // Get the origin remote.
  Call(git_remote_lookup(&data_.remote, data_.repo, remote.c_str()));

  // Now fetch any updates.
  git_fetch_options fetch_options = GIT_FETCH_OPTIONS_INIT;
  Call(git_remote_fetch(data_.remote, nullptr, &fetch_options, nullptr));

  // Log some stats on what was fetched either during update or clone.
  const git_transfer_progress* stats = git_remote_stats(data_.remote);
  if (logger) {
    logger->trace("Received {} of {} objects in {} bytes.",
                   stats->indexed_objects,
                   stats->total_objects,
                   stats->received_bytes);
  }

  git_remote_free(data_.remote);
  data_.remote = nullptr;
}

void GitHelper::CheckoutNewBranch(const std::string& remote,
                                  const std::string& branch) {
  if (data_.repo == nullptr)
    throw GitStateError(
        "Cannot fetch updates for repository that has not been opened.");
  else if (data_.commit != nullptr)
    throw GitStateError(
        "Cannot fetch repository updates, commit memory already allocated.");
  else if (data_.object != nullptr)
    throw GitStateError(
        "Cannot fetch repository updates, object memory already allocated.");
  else if (data_.reference != nullptr)
    throw GitStateError(
        "Cannot fetch repository updates, reference memory already allocated.");

  auto logger = getLogger();
  if (logger) {
    logger->trace("Looking up commit referred to by the remote branch \"{}\".",
                   branch);
  }
  Call(git_revparse_single(
      &data_.object, data_.repo, (remote + "/" + branch).c_str()));
  const git_oid* commit_id = git_object_id(data_.object);

  if (logger) {
    logger->trace("Creating the new branch.");
  }
  Call(git_commit_lookup(&data_.commit, data_.repo, commit_id));
  Call(git_branch_create(
      &data_.reference, data_.repo, branch.c_str(), data_.commit, 1));

  if (logger) {
    logger->trace("Setting the upstream for the new branch.");
  }
  Call(git_branch_set_upstream(data_.reference,
                               (remote + "/" + branch).c_str()));

  // Check if HEAD points to the desired branch and set it to if not.
  if (!git_branch_is_head(data_.reference)) {
    if (logger) {
      logger->trace("Setting HEAD to follow branch: {}", branch);
    }
    Call(git_repository_set_head(data_.repo,
                                 (string("refs/heads/") + branch).c_str()));
  }

  if (logger) {
    logger->trace("Performing a Git checkout of HEAD.");
  }
  Call(git_checkout_head(data_.repo, &data_.checkout_options));

  // Free tree and commit pointers. Reference pointer is still used below.
  git_object_free(data_.object);
  git_commit_free(data_.commit);
  git_reference_free(data_.reference);
  data_.commit = nullptr;
  data_.object = nullptr;
  data_.reference = nullptr;
}

void GitHelper::CheckoutRevision(const std::string& revision) {
  if (data_.repo == nullptr)
    throw GitStateError(
        "Cannot checkout revision for repository that has not been opened.");
  else if (data_.object != nullptr)
    throw GitStateError(
        "Cannot fetch repository updates, object memory already allocated.");

  // Get an object ID for 'HEAD^'.
  Call(git_revparse_single(&data_.object, data_.repo, revision.c_str()));
  const git_oid* oid = git_object_id(data_.object);

  // Detach HEAD to HEAD~1. This will roll back HEAD by one commit each time it
  // is called.
  Call(git_repository_set_head_detached(data_.repo, oid));

  // Checkout the new HEAD.
  auto logger = getLogger();
  if (logger) {
    logger->trace("Performing a Git checkout of HEAD.");
  }
  Call(git_checkout_head(data_.repo, &data_.checkout_options));

  git_object_free(data_.object);
  data_.object = nullptr;
}

void GitHelper::DeleteBranch(const std::string& branch) {
  if (data_.repo == nullptr) {
    throw GitStateError(
        "Cannot delete branch for repository that has not been opened.");
  } else if (data_.reference != nullptr) {
    throw GitStateError(
        "Cannot delete branch, reference memory already allocated.");
  }

  Call(git_branch_lookup(
      &data_.reference, data_.repo, branch.c_str(), GIT_BRANCH_LOCAL));

  auto logger = getLogger();
  int ret = git_branch_is_head(data_.reference);
  if (ret == 1) {
    if (logger) {
      logger->debug("Detaching HEAD before deleting branch.");
    }
    git_repository_detach_head(data_.repo);
  } else {
    Call(ret);
  }

  if (logger) {
    logger->debug("Deleting branch.");
  }

  Call(git_branch_delete(data_.reference));

  git_reference_free(data_.reference);
  data_.reference = nullptr;
}

bool GitHelper::BranchExists(const std::string& branch) {
  if (data_.repo == nullptr) {
    throw GitStateError(
        "Cannot check branch existence for repository that has not been "
        "opened.");
  } else if (data_.reference != nullptr) {
    throw GitStateError(
        "Cannot check branch existence, reference memory already allocated.");
  }

  int ret = git_branch_lookup(
      &data_.reference, data_.repo, branch.c_str(), GIT_BRANCH_LOCAL);
  if (ret != GIT_ENOTFOUND) {
    // Handle other errors from preceding branch lookup.
    Call(ret);

    git_reference_free(data_.reference);
    data_.reference = nullptr;

    return true;
  }

  return false;
}

bool GitHelper::IsBranchUpToDate(const std::string& branch) {
  if (data_.repo == nullptr) {
    throw GitStateError(
        "Cannot check branch existence for repository that has not been "
        "opened.");
  } else if (data_.reference != nullptr) {
    throw GitStateError(
        "Cannot check branch existence, reference memory already allocated.");
  } else if (data_.reference2 != nullptr) {
    throw GitStateError(
        "Cannot check branch existence, reference2 memory already allocated.");
  }

  Call(git_branch_lookup(
      &data_.reference, data_.repo, branch.c_str(), GIT_BRANCH_LOCAL));

  // Get remote branch reference.
  Call(git_branch_lookup(&data_.reference2,
                         data_.repo,
                         ("origin/" + branch).c_str(),
                         GIT_BRANCH_REMOTE));

  // Get the branch tips' commit IDs.
  auto local_commit_id = GetCommitId(data_.reference);
  auto remote_commit_id = GetCommitId(data_.reference2);

  bool upToDate = memcmp(local_commit_id->id, remote_commit_id->id, 20) == 0;

  // Free the remote branch reference.
  git_reference_free(data_.reference2);
  data_.reference2 = nullptr;

  git_reference_free(data_.reference);
  data_.reference = nullptr;

  return upToDate;
}

bool GitHelper::IsBranchCheckedOut(const std::string& branch) {
  if (data_.repo == nullptr) {
    throw GitStateError(
        "Cannot check branch existence for repository that has not been "
        "opened.");
  } else if (data_.reference != nullptr) {
    throw GitStateError(
        "Cannot check branch existence, reference memory already allocated.");
  }

  Call(git_branch_lookup(
      &data_.reference, data_.repo, branch.c_str(), GIT_BRANCH_LOCAL));

  bool isCheckedOut = git_branch_is_checked_out(data_.reference);

  git_reference_free(data_.reference);
  data_.reference = nullptr;

  return isCheckedOut;
}

const git_oid* GitHelper::GetCommitId(git_reference* reference) {
  if (reference == nullptr)
    throw GitStateError(
        "Cannot get the commit ID of a null git_reference pointer.");
  else if (data_.object != nullptr)
    throw GitStateError(
        "Cannot fetch repository updates, object memory already allocated.");

  Call(git_reference_peel(&data_.object, reference, GIT_OBJ_COMMIT));
  const git_oid* remote_commit_id = git_object_id(data_.object);

  git_object_free(data_.object);
  data_.object = nullptr;

  return remote_commit_id;
}

std::string GitHelper::GetHeadCommitId(bool shortId) {
  if (data_.repo == nullptr)
    throw GitStateError(
        "Cannot checkout revision for repository that has not been opened.");
  else if (data_.object != nullptr)
    throw GitStateError(
        "Cannot fetch repository updates, object memory already allocated.");
  else if (data_.reference != nullptr)
    throw GitStateError(
        "Cannot fetch repository updates, reference memory already allocated.");
  else if (data_.buffer.ptr != nullptr)
    throw GitStateError(
        "Cannot fetch repository updates, buffer memory already allocated.");

  auto logger = getLogger();
  if (logger) {
    logger->trace("Getting the Git object for HEAD.");
  }
  Call(git_repository_head(&data_.reference, data_.repo));

  std::string id;
  if (shortId) {
    Call(git_reference_peel(&data_.object, data_.reference, GIT_OBJ_COMMIT));

    if (logger) {
      logger->trace("Generating hex string for Git object ID.");
    }
    Call(git_object_short_id(&data_.buffer, data_.object));
    id = data_.buffer.ptr;

    git_object_free(data_.object);
    git_buf_free(&data_.buffer);
    data_.object = nullptr;
    data_.buffer = {0};
  } else {
    auto commit_id = GetCommitId(data_.reference);
    char c_rev[GIT_OID_HEXSZ + 1];

    id = git_oid_tostr(c_rev, GIT_OID_HEXSZ + 1, commit_id);
  }

  git_reference_free(data_.reference);
  data_.reference = nullptr;

  return id;
}

std::string GitHelper::GetHeadCommitDate() {
  if (data_.repo == nullptr) {
    throw GitStateError(
        "Cannot get HEAD commit date for repository that has not been opened.");
  } else if (data_.commit != nullptr) {
    throw GitStateError(
        "Cannot get HEAD commit date, commit memory already allocated.");
  } else if (data_.reference != nullptr) {
    throw GitStateError(
        "Cannot get HEAD commit date, reference memory already allocated.");
  }

  auto logger = getLogger();
  if (logger) {
    logger->trace("Getting the Git reference for HEAD.");
  }
  Call(git_repository_head(&data_.reference, data_.repo));

  auto commit_id = GetCommitId(data_.reference);

  git_reference_free(data_.reference);
  data_.reference = nullptr;

  if (logger) {
    logger->trace("Getting commit for ID.");
  }

  Call(git_commit_lookup(&data_.commit, data_.repo, commit_id));
  git_time_t time = git_commit_time(data_.commit);

  git_commit_free(data_.commit);
  data_.commit = nullptr;

  std::ostringstream out;
  out << std::put_time(std::gmtime(&time), "%Y-%m-%d");
  return out.str();
}

bool GitHelper::IsFileDifferent(const std::filesystem::path& repoRoot,
                                const std::string& filename) {
  auto logger = getLogger();

  if (!IsRepository(repoRoot)) {
    if (logger) {
      logger->info("Unknown masterlist revision: Git repository missing.");
    }
    throw GitStateError("Cannot check if the \"" + filename +
                        "\" working copy is edited, Git repository missing.");
  }

  if (logger) {
    logger->trace("Existing repository found, attempting to open it.");
  }
  GitHelper git;
  git.Call(git_repository_open(&git.data_.repo, repoRoot.u8string().c_str()));

  // Perform a git diff, then iterate the deltas to see if one exists for the
  // masterlist.
  if (logger) {
    logger->trace("Getting the tree for the HEAD revision.");
  }
  git.Call(
      git_revparse_single(&git.data_.object, git.data_.repo, "HEAD^{tree}"));
  git.Call(git_tree_lookup(
      &git.data_.tree, git.data_.repo, git_object_id(git.data_.object)));

  if (logger) {
    logger->trace("Performing git diff.");
  }
  git.Call(git_diff_tree_to_workdir_with_index(
      &git.data_.diff, git.data_.repo, git.data_.tree, NULL));

  if (logger) {
    logger->trace("Iterating over git diff deltas.");
  }
  GitHelper::DiffPayload payload;
  payload.fileFound = false;
  payload.fileToFind = filename.c_str();
  git.Call(git_diff_foreach(
      git.data_.diff, &git.DiffFileCallback, NULL, NULL, NULL, &payload));

  return payload.fileFound;
}
}
